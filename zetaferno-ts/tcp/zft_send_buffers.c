/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-zft_send_buffers A few calls of @a zft_send() in a row using various send buffers.
 *
 * @objective Perform a few calls of @a zft_send() in a row using
 *            various (length, iovcnt) send buffers.
 *
 * @param number  How many calls @a zft_send() should be performed:
 *                - @c 10
 * @param length  Buffers length:
 *                - @c 1
 *                - @c 1000
 *                - @c 5000
 *                - rand in range [1; 2000]
 * @param iovcnt  IOV vectors number to send packets:
 *                - @c 1 (only supported value for now)
 *                - @c 10
 *                - @c 100
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/zft_send_buffers"

#include "zf_test.h"
#include "rpc_zf.h"

#include "te_dbuf.h"

/** Minimum size of packet which size is chosen randomly. */
#define MIN_RAND_SIZE 1
/** Maximum size of packet which size is chosen randomly. */
#define MAX_RAND_SIZE 2000

int
main(int argc, char *argv[])
{
    rcf_rpc_server          *pco_iut = NULL;
    rcf_rpc_server          *pco_tst = NULL;
    const struct sockaddr   *iut_addr = NULL;
    const struct sockaddr   *tst_addr = NULL;

    rpc_zf_attr_p   attr = RPC_NULL;
    rpc_zf_stack_p  stack = RPC_NULL;

    rpc_zft_p iut_zft = RPC_NULL;
    int       tst_s = -1;

    rpc_iovec *sndiov = NULL;
    int        i = 0;
    int        j = 0;

    size_t    min_size;
    size_t    max_size;

    te_dbuf sent_data = TE_DBUF_INIT(0);
    te_dbuf received_data = TE_DBUF_INIT(0);

    char data[ZFTS_TCP_DATA_MAX];

    int number = 0;
    int length = 0;
    int iovcnt = 0;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(number);
    TEST_GET_INT_PARAM(length);
    TEST_GET_INT_PARAM(iovcnt);

    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection. */
    zfts_establish_tcp_conn(TRUE, pco_iut, attr, stack,
                            &iut_zft, iut_addr,
                            pco_tst, &tst_s, tst_addr);

    sndiov = tapi_calloc(iovcnt, sizeof(*sndiov));

    /*- Perform a few @a zft_send() calls
     *  - use test argumens @p length and @p iovcnt to generate
     *    sending iov vector. */
    for (i = 0; i < number; i++)
    {
        if (length >= 0)
        {
            min_size = length;
            max_size = length;
        }
        else
        {
            min_size = MIN_RAND_SIZE;
            max_size = MAX_RAND_SIZE;
        }

        rpc_make_iov(sndiov, iovcnt, min_size, max_size);

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_send(pco_iut, iut_zft, sndiov, iovcnt, 0);
        if (rc < 0)
        {
            if (RPC_ERRNO(pco_iut) == RPC_EAGAIN)
                break;
            TEST_VERDICT("zft_send() failed with errno %r",
                         RPC_ERRNO(pco_iut));
        }
        rpc_zf_process_events(pco_iut, stack);

        for (j = 0; j < iovcnt; j++)
        {
            te_dbuf_append(&sent_data,
                           sndiov[j].iov_base, rc > (int)sndiov[j].iov_len ?
                                               (int)sndiov[j].iov_len : rc);
            rc -= sndiov[j].iov_len;
            if (rc <= 0)
                break;
        }

        rpc_release_iov(sndiov, iovcnt);
    }

    /*- Process ZF events. */
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Read all the packets on tester, check the data. */
    do {
        RPC_AWAIT_IUT_ERROR(pco_tst);
        rc = rpc_recv(pco_tst, tst_s, data, ZFTS_TCP_DATA_MAX,
                      RPC_MSG_DONTWAIT);
        if (rc < 0)
        {
            if (RPC_ERRNO(pco_tst) == RPC_EAGAIN)
                break;
            else
                TEST_VERDICT("recv() failed with unexpected errno %r",
                             RPC_ERRNO(pco_tst));
        }
        rpc_zf_process_events(pco_iut, stack);
        te_dbuf_append(&received_data, data, rc);
    } while (TRUE);

    ZFTS_CHECK_RECEIVED_DATA(received_data.ptr, sent_data.ptr,
                             received_data.len, sent_data.len,
                             " from IUT");

    TEST_SUCCESS;

cleanup:

    free(sndiov);
    te_dbuf_free(&sent_data);
    te_dbuf_free(&received_data);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

