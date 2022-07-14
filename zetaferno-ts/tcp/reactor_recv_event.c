/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-reactor_recv_event Wait for data reception event blocking on ZF reactor.
 *
 * @objective Poll for ZF reactor status until there is an event with
 *            incoming data.
 *
 * @param open_method     How to open connection:
 *                        - @c active
 *                        - @c passive
 *                        - @c passive_close (close listener after
 *                          passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/reactor_recv_event"

#include "zf_test.h"
#include "rpc_zf.h"

#include "te_dbuf.h"

/** Maximum number of bytes to send at once. */
#define MAX_DATA_SIZE 4000

/** Number of iterations in the main loop. */
#define LOOP_ITERS 1000

int
main(int argc, char *argv[])
{
    rcf_rpc_server          *pco_iut = NULL;
    rcf_rpc_server          *pco_tst = NULL;
    const struct sockaddr   *iut_addr = NULL;
    const struct sockaddr   *tst_addr = NULL;

    rpc_zf_attr_p   attr = RPC_NULL;
    rpc_zf_stack_p  stack = RPC_NULL;

    rpc_zft_p       iut_zft = RPC_NULL;
    rpc_zftl_p      iut_zftl = RPC_NULL;
    int             tst_s = -1;

    char    send_buf[MAX_DATA_SIZE];
    size_t  pkt_len;
    int     i;

    te_dbuf recv_data = TE_DBUF_INIT(0);
    te_bool recv_failed;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_SET_DEF_RECV_FUNC(recv_func);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection according to @p open_method. */
    zfts_establish_tcp_conn_ext2(open_method,
                                 pco_iut, attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- Set @c TCP_NODELAY for Tester socket, so that it will
     * send all data at once. */
    rpc_setsockopt_int(pco_tst, tst_s, RPC_TCP_NODELAY, 1);

    /*- In a loop @c LOOP_ITERS times: */
    for (i = 0; i < LOOP_ITERS; i++)
    {
        RING("Iteration %d", i + 1);

        te_fill_buf(send_buf, MAX_DATA_SIZE);

        /*-- Block in @p rpc_zf_wait_for_event waiting for
         * the incoming event. */
        pco_iut->op = RCF_RPC_CALL;
        rpc_zf_wait_for_event(pco_iut, stack);

        /*-- Send data from Tester, use random
         * data length [1; @c MAX_DATA_SIZE]. */
        pkt_len = rand_range(1, MAX_DATA_SIZE);
        rpc_send(pco_tst, tst_s, send_buf, pkt_len, 0);

        /*-- Get an event on ZF reactor. */
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_wait_for_event(pco_iut, stack);
        if (rc < 0)
            TEST_VERDICT("rpc_zf_wait_for_event() failed with errno %r",
                         RPC_ERRNO(pco_iut));

        /*-- Read and check the data. */

        te_dbuf_free(&recv_data);

        recv_failed = FALSE;
        do {
            RPC_AWAIT_ERROR(pco_iut);
            rc = zfts_zft_recv_dbuf(pco_iut, iut_zft, &recv_data);
            if (rc <= 0)
            {
                if (rc < 0 && RPC_ERRNO(pco_iut) != RPC_EAGAIN)
                    TEST_VERDICT("Receive function failed with unexpected "
                                 "errno %r", RPC_ERRNO(pco_iut));

                if (recv_failed || recv_data.len >= pkt_len)
                {
                    break;
                }
                else
                {
                    recv_failed = TRUE;
                    ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);
                }
            }
            else
            {
                recv_failed = FALSE;
            }

            rpc_zf_process_events(pco_iut, stack);
        } while (TRUE);

        ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, send_buf,
                                 recv_data.len, pkt_len,
                                 " from IUT");
    }

    TEST_SUCCESS;

cleanup:

    te_dbuf_free(&recv_data);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
