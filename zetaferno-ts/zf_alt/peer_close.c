/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-peer_close Using alternatives API when peer socket is closed or shutdown.
 *
 * @objective Use alternative when peer socket is closed or write shutdown.
 *
 * @param alts_queue    Queue data at the beginning if @c TRUE.
 * @param sock_close    Close tester socket if @c TRUE, else - shutdown(WR).
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

#define TE_TEST_NAME "zf_alt/peer_close"

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

    rpc_zft_p iut_zftl = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    int       tst_s = -1;

    rpc_zf_althandle  alt1;
    rpc_zf_althandle  alt2;

    rpc_iovec iov1;
    rpc_iovec iov2;

    te_dbuf           iut_sent = TE_DBUF_INIT(0);
    te_dbuf           tst_received = TE_DBUF_INIT(0);

    te_bool     alts_queue;
    te_bool     sock_close;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(alts_queue);
    TEST_GET_BOOL_PARAM(sock_close);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    rpc_make_iov(&iov1, 1, 1, ZFTS_TCP_DATA_MAX);
    rpc_make_iov(&iov2, 1, 1, ZFTS_TCP_DATA_MAX);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection to get ZFT zocket. */
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- Allocate two alternatives. */
    rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alt1);
    rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alt2);

    /*- Queue data to both alternatives if @p alts_queue is @c TRUE. */
    if (alts_queue)
    {
        rpc_zft_alternatives_queue(pco_iut, iut_zft, alt1,
                                   &iov1, 1, 0);
        rpc_zft_alternatives_queue(pco_iut, iut_zft, alt2,
                                   &iov2, 1, 0);
    }

    /*- Close or shutdown Tester socket according to @p sock_close. */
    if (sock_close)
        RPC_CLOSE(pco_tst, tst_s);
    else
        rpc_shutdown(pco_tst, tst_s, RPC_SHUT_WR);
    TAPI_WAIT_NETWORK;

    /*- Queue (if required) and send some data using the first
     * alternative. */

    if (!alts_queue)
    {
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alt1,
                                        &iov1, 1, 0);
        if (rc < 0)
            TEST_VERDICT("zft_alternatives_queue() for the first "
                         "alternative failed with errno %r",
                         RPC_ERRNO(pco_iut));
    }

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_alternatives_send(pco_iut, stack, alt1);
    if (rc < 0)
        TEST_VERDICT("zf_alternatives_send() for the first alternative "
                     "failed with errno %r", RPC_ERRNO(pco_iut));

    ZFTS_WAIT_NETWORK(pco_iut, stack);
    te_dbuf_append(&iut_sent, iov1.iov_base, iov1.iov_len);

    if (sock_close)
    {
        /*- If @p sock_close is @c TRUE:
         *  - if @p alts_queue is @c TRUE, cancel data on
         *    the second alternative;
         *  - Try to queue data on the second alternative,
         *    check that it fails. */

        if (alts_queue)
        {
            rpc_zf_alternatives_cancel(pco_iut, stack, alt2);
            rpc_zf_process_events(pco_iut, stack);
        }

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alt2,
                                        &iov2, 1, 0);
        if (rc >= 0)
            TEST_VERDICT("zft_alternatives_queue() succeeded "
                         "after closing socket on peer and sending "
                         "some data to it");
        else if (RPC_ERRNO(pco_iut) != RPC_ENOTCONN)
            TEST_VERDICT("zft_alternatives_queue() failed with "
                         "unexpected errno %r after closing socket on "
                         "peer and sending some data to it",
                         RPC_ERRNO(pco_iut));
    }
    else
    {
        /*- If @p sock_close is @c FALSE, queue (if required) and send
         * data using both alternatives. */

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alt1,
                                        &iov1, 1, 0);
        if (rc < 0)
            TEST_VERDICT("Final zft_alternatives_queue() for the first "
                         "alternative failed with errno %r",
                         RPC_ERRNO(pco_iut));

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_alternatives_send(pco_iut, stack, alt1);
        if (rc < 0)
            TEST_VERDICT("Final zf_alternatives_send() failed for "
                         "the first alternative with errno %r",
                         RPC_ERRNO(pco_iut));
        rpc_zf_process_events(pco_iut, stack);
        te_dbuf_append(&iut_sent, iov1.iov_base, iov1.iov_len);

        if (alts_queue)
        {
            rpc_zf_alternatives_cancel(pco_iut, stack, alt2);
            rpc_zf_process_events(pco_iut, stack);
        }

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alt2,
                                        &iov2, 1, 0);
        if (rc < 0)
            TEST_VERDICT("Final zft_alternatives_queue() for the second "
                         "alternative failed with errno %r",
                         RPC_ERRNO(pco_iut));

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_alternatives_send(pco_iut, stack, alt2);
        if (rc < 0)
            TEST_VERDICT("Final zf_alternatives_send() failed for "
                         "the second alternative with errno %r",
                         RPC_ERRNO(pco_iut));
        rpc_zf_process_events(pco_iut, stack);
        te_dbuf_append(&iut_sent, iov2.iov_base, iov2.iov_len);

        zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s,
                               &tst_received);
        ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr, iut_sent.ptr,
                                 tst_received.len, iut_sent.len,
                                 " from IUT");
    }

    TEST_SUCCESS;

cleanup:

    rpc_release_iov(&iov1, 1);
    rpc_release_iov(&iov2, 1);
    te_dbuf_free(&iut_sent);
    te_dbuf_free(&tst_received);

    CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alt1);
    CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alt2);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
