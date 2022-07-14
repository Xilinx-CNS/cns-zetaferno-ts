/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-zc_recv_done_some Read a part of data with zero-copy receive.
 *
 * @objective Read a part of data using zero-copy receive, check that the
 *            remaining part can be read later.
 *
 * @param open_method     How to open connection:
 *                        - @c active
 *                        - @c passive
 *                        - @c passive_close (close listener after
 *                          passively establishing connection)
 * @param send_iut        Send some data from IUT if @c TRUE.
 * @param done_some_zero  If @c TRUE, call @b zft_zc_recv_done_some()
 *                        with zero len argument.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/zc_recv_done_some"

#include "zf_test.h"
#include "rpc_zf.h"
#include "te_dbuf.h"

/** Number of iterations in main test loop. */
#define LOOP_ITERS 100

/**
 * Minimum amount of data sent from Tester on each loop
 * iteration.
 */
#define MIN_TST_SEND 100

/**
 * Maximum amount of data sent from Tester on each loop
 * iteration.
 */
#define MAX_TST_SEND 5000

/**
 * Maximum amount of data read at once by IUT.
 */
#define MAX_CHUNK_LEN 1400

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p   attr = RPC_NULL;
    rpc_zf_stack_p  stack = RPC_NULL;

    rpc_zft_p       iut_zft = RPC_NULL;
    rpc_zftl_p      iut_zftl = RPC_NULL;
    int             tst_s = -1;

    char      tst_send_buf[MAX_TST_SEND];
    char      iut_send_buf[MAX_CHUNK_LEN];
    char      recv_buf[MAX_CHUNK_LEN];
    int       send_len;
    int       iut_chunk_len;
    te_dbuf   tst_sent = TE_DBUF_INIT(0);
    te_dbuf   iut_received = TE_DBUF_INIT(0);
    te_dbuf   aux_dbuf = TE_DBUF_INIT(0);
    int       i;

    rpc_iovec     iovs[ZFTS_IOVCNT];
    rpc_zft_msg   msg;

    zfts_conn_open_method   open_method;
    te_bool                 send_iut = FALSE;
    te_bool                 done_some_zero = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_BOOL_PARAM(send_iut);
    TEST_GET_BOOL_PARAM(done_some_zero);

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
        /*-- Call blocking @b rpc_zf_wait_for_event() on IUT. */
        pco_iut->op = RCF_RPC_CALL;
        rpc_zf_wait_for_event(pco_iut, stack);

       /*-- Send some data from Tester, random volume
        * [@c MIN_TST_SEND; @c MAX_TST_SEND] on each iteration. */
        send_len = rand_range(MIN_TST_SEND, MAX_TST_SEND);
        te_fill_buf(tst_send_buf, send_len);
        rpc_send(pco_tst, tst_s, tst_send_buf, send_len, 0);
        te_dbuf_append(&tst_sent, tst_send_buf, send_len);

        /*-- Wait for @b rpc_zf_wait_for_event() termination on IUT,
         * process events. */
        rpc_zf_wait_for_event(pco_iut, stack);
        rpc_zf_process_events(pco_iut, stack);

        /*-- Read data chunks in a loop: */
        while (TRUE)
        {
            int total_recv_len;

            memset(iovs, 0, sizeof(iovs));
            memset(&msg, 0, sizeof(msg));
            msg.iov = iovs;
            msg.iovcnt = ZFTS_IOVCNT;

            /*--- Receive data using @c zft_zc_recv(). */
            rpc_zft_zc_recv(pco_iut, iut_zft, &msg, 0);
            if (msg.iovcnt <= 0)
                break;

            total_recv_len = rpc_iov_data_len(iovs, msg.iovcnt);

            /*--- Choose data length from [1; @c MAX_CHUNK_LEN]. */
            iut_chunk_len = rand_range(1, MAX_CHUNK_LEN);

            /*
             * If @p done_some_zero is @c TRUE, choose length of
             * the last data chunk so that it will be greater
             * than amount of data returned by @b zft_zc_recv().
             */
            if (iut_chunk_len == total_recv_len && done_some_zero)
            {
                if (iut_chunk_len == MAX_CHUNK_LEN)
                    iut_chunk_len--;
                else
                    iut_chunk_len++;
            }

            /*--- If @b zft_zc_recv() returned less data than selected
             * data chunk length: if @p done_some_zero is @c TRUE,
             * call @b zft_zc_recv_done_some() with zero len argument;
             * otherwise copy received data and call @b zft_zc_recv_done().
             * Break from the loop.
             */

            if (total_recv_len < iut_chunk_len)
            {
                if (done_some_zero)
                {
                    RPC_AWAIT_ERROR(pco_iut);
                    rc = rpc_zft_zc_recv_done_some(pco_iut, iut_zft,
                                                   &msg, 0);
                    if (rc < 0)
                        TEST_VERDICT("zft_zc_recv_done_some() with zero "
                                     "len failed with errno %r",
                                     RPC_ERRNO(pco_iut));
                }
                else
                {
                    rpc_iov_append2dbuf(iovs, msg.iovcnt, &iut_received);
                    rpc_zft_zc_recv_done(pco_iut, iut_zft, &msg);
                    rpc_zf_process_events(pco_iut, stack);
                }

                rpc_release_iov(iovs, ZFTS_IOVCNT);
                break;
            }

            /*--- If @b zft_zc_recv() returned more than selected data
             * chunk length, copy chunk data and call
             * @b zft_zc_recv_done_some() with selected chunk length.
             */
            memcpy(recv_buf, rpc_iov2dbuf(iovs, msg.iovcnt, &aux_dbuf),
                   iut_chunk_len);
            te_dbuf_append(&iut_received, recv_buf, iut_chunk_len);
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zft_zc_recv_done_some(pco_iut, iut_zft,
                                           &msg, iut_chunk_len);
            if (rc < 0)
                TEST_VERDICT("zft_zc_recv_done_some() with nonzero len "
                             "failed with errno %r",
                             RPC_ERRNO(pco_iut));

            rpc_release_iov(iovs, ZFTS_IOVCNT);
            rpc_zf_process_events(pco_iut, stack);
        }

        /*-- If @p send_iut is @c TRUE, send some data from IUT
         * and read and check it on Tester. */
        if (send_iut)
        {
            send_len = rand_range(1, MAX_CHUNK_LEN);
            te_fill_buf(iut_send_buf, send_len);

            memset(iovs, 0, sizeof(iovs));
            iovs[0].iov_base = iut_send_buf;
            iovs[0].iov_len = iovs[0].iov_rlen = send_len;
            rpc_zft_send(pco_iut, iut_zft, iovs, 1, 0);
            rpc_zf_process_events(pco_iut, stack);

            rc = rpc_recv(pco_tst, tst_s, recv_buf, MAX_CHUNK_LEN, 0);

            ZFTS_CHECK_RECEIVED_DATA(recv_buf, iut_send_buf,
                                     rc, send_len,
                                     " from IUT");
        }
    }

    /*- Read all the remaining data on the IUT using zero-copy read,
     *  finilize read with @b zft_zc_recv_done(). */
    while (TRUE)
    {
        memset(iovs, 0, sizeof(iovs));
        memset(&msg, 0, sizeof(msg));
        msg.iov = iovs;
        msg.iovcnt = ZFTS_IOVCNT;

        rpc_zft_zc_recv(pco_iut, iut_zft, &msg, 0);
        if (msg.iovcnt <= 0)
            break;

        rpc_iov_append2dbuf(iovs, msg.iovcnt, &iut_received);
        rpc_zft_zc_recv_done(pco_iut, iut_zft, &msg);
        rpc_zf_process_events(pco_iut, stack);

        rpc_release_iov(iovs, ZFTS_IOVCNT);
    }

    /*- Check all read data. */
    ZFTS_CHECK_RECEIVED_DATA(iut_received.ptr, tst_sent.ptr,
                             iut_received.len, tst_sent.len,
                             " from Tester");

    TEST_SUCCESS;

cleanup:

    te_dbuf_free(&aux_dbuf);
    te_dbuf_free(&tst_sent);
    te_dbuf_free(&iut_received);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
