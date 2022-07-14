/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-queue_mult Queue a few messages to one alternative.
 *
 * @objective Queue a few messages to one alternative and send it,
 *            optionally send data using zft_send() while data is
 *            put to the alternative queue.
 *
 * @param queue_length  Queue length:
 *                      - @c 10
 * @param zft_send      Send data using zft_send() while data is put
 *                      to queue.
 * @param little        Send one 1-byte data packet @c TRUE, else send
 *                      a lot of data, do not iterate if @p zft_send
 *                      is @c FALSE.
 * @param open_method   How to open connection:
 *                      - @c active
 *                      - @c passive
 *                      - @c passive_close (close listener after
 *                        passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "zf_alt/queue_mult"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zftl_p  iut_zftl = RPC_NULL;
    rpc_zft_p   iut_zft = RPC_NULL;
    int         tst_s = -1;

    rpc_zf_althandle   alt;
    rpc_iovec         *iovs;

    char        send_buf[ZFTS_TCP_DATA_MAX];
    char        recv_buf[ZFTS_TCP_DATA_MAX];
    rpc_iovec   send_iov = { .iov_base = send_buf,
                             .iov_len = ZFTS_TCP_DATA_MAX,
                             .iov_rlen = ZFTS_TCP_DATA_MAX };
    int         i;

    te_bool     failed = FALSE;

    te_dbuf           iut_sent = TE_DBUF_INIT(0);
    te_dbuf           tst_received = TE_DBUF_INIT(0);

    int       queue_length;
    te_bool   zft_send;
    te_bool   little;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(queue_length);
    TEST_GET_BOOL_PARAM(zft_send);
    TEST_GET_BOOL_PARAM(little);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    te_fill_buf(send_buf, ZFTS_TCP_DATA_MAX);
    rpc_alloc_iov(&iovs, queue_length, 1, ZFTS_TCP_DATA_MAX);

    if (little)
        send_iov.iov_len = 1;

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection to get zft zocket. */
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- Allocate one alternative. */
    rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alt);

    /*- In the loop @p queue_length times:
     *  - queue a data packet;
     *  - if @p zft_send is @c TRUE:
     *    - send data using @c zft_send();
     *    - choose data amount based on @p little;
     *    - read and check data on tester. */

    for (i = 0; i < queue_length; i++)
    {
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alt,
                                        &iovs[i], 1, 0);
        if (rc < 0)
            TEST_VERDICT("zft_alternatives_queue() failed with errno %r",
                         RPC_ERRNO(pco_iut));
        te_dbuf_append(&iut_sent, iovs[i].iov_base, iovs[i].iov_len);

        if (zft_send)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zft_send(pco_iut, iut_zft, &send_iov, 1, 0);
            if (rc < 0)
                TEST_VERDICT("zft_send() failed with errno %r",
                             RPC_ERRNO(pco_iut));
            else if (rc == 0)
                TEST_VERDICT("zft_send() unexpectedly returned zero");

            rpc_zf_process_events(pco_iut, stack);

            RPC_AWAIT_ERROR(pco_tst);
            rc = rpc_recv(pco_tst, tst_s, recv_buf, ZFTS_TCP_DATA_MAX,
                          RPC_MSG_DONTWAIT);
            if (rc < 0)
                TEST_VERDICT("recv() failed on Tester with errno %r "
                             "after sending data with zft_send()",
                             RPC_ERRNO(pco_tst));

            ZFTS_CHECK_RECEIVED_DATA(recv_buf, send_iov.iov_base,
                                     rc, send_iov.iov_len,
                                     " from IUT via zft_send()");
        }
    }

    /*- Send queued data; @b zf_alternatives_send() should fail if
     * @p zft_send is @c TRUE. */

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_alternatives_send(pco_iut, stack, alt);
    if (rc < 0)
    {
        if (!zft_send || RPC_ERRNO(pco_iut) != RPC_EINVAL)
            TEST_VERDICT("zf_alternatives_send() unexpectedly failed "
                         "with errno %r",
                         RPC_ERRNO(pco_iut));
        else
            TEST_SUCCESS;
    }
    else
    {
        if (zft_send)
        {
            ERROR_VERDICT("zf_alternatives_send() succeeded after "
                          "zft_send() usage");
            failed = TRUE;
        }
    }

    /*- Read and check data on tester. */

    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s,
                           &tst_received);

    ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr, iut_sent.ptr,
                             tst_received.len, iut_sent.len,
                             " from IUT via zf_alternatives_send()");

    if (failed)
        TEST_STOP;
    TEST_SUCCESS;

cleanup:

    te_dbuf_free(&iut_sent);
    te_dbuf_free(&tst_received);
    rpc_free_iov(iovs, queue_length);

    CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alt);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
