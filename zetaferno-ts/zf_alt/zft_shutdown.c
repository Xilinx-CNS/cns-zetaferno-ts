/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-zft_shutdown Alternatives API behavior after zft_shutdown_tx().
 *
 * @objective Using alternatives after write shutdown on the connection.
 *
 * @param alts_send       Try to send queued data if @c TRUE.
 * @param alts_cancel     Cancel alternatives if @c TRUE.
 * @param alts_release    Release alternatives if @c TRUE.
 * @param zock_shutdown   Shutdown ZFT zocket if @c TRUE.
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

#define TE_TEST_NAME "zf_alt/zft_shutdown"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr_storage iut_addr2;
    struct sockaddr_storage tst_addr2;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p iut_zftl1 = RPC_NULL;
    rpc_zft_p iut_zft1 = RPC_NULL;
    int       tst_s1 = -1;
    rpc_zft_p iut_zftl2 = RPC_NULL;
    rpc_zft_p iut_zft2 = RPC_NULL;
    int       tst_s2 = -1;

    te_dbuf           iut_sent = TE_DBUF_INIT(0);
    te_dbuf           tst_received = TE_DBUF_INIT(0);

    rpc_zf_althandle *alts;
    int               n_alts;
    int               alt_count_max;
    rpc_iovec        *iovs;
    int               i;
    int               j;

    te_bool alts_send;
    te_bool alts_cancel;
    te_bool alts_release;
    te_bool zock_shutdown;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(alts_send);
    TEST_GET_BOOL_PARAM(alts_cancel);
    TEST_GET_BOOL_PARAM(alts_release);
    TEST_GET_BOOL_PARAM(zock_shutdown);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    CHECK_RC(tapi_sh_env_get_int(pco_iut, "ZFTS_ALT_COUNT_DEF",
                                 &alt_count_max));

    alts = (rpc_zf_althandle *)tapi_calloc(sizeof(*alts), alt_count_max);
    iovs = (rpc_iovec *)tapi_calloc(sizeof(*iovs), alt_count_max);

    n_alts = rand_range(1, alt_count_max);
    for (i = 0; i < n_alts; i++)
    {
        rpc_make_iov(&iovs[i], 1, 1, ZFTS_TCP_DATA_MAX);
    }

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection to get zft zocket. */
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl1, &iut_zft1, iut_addr,
                                 pco_tst, &tst_s1, tst_addr,
                                 -1, -1);

    /*- Allocate random number [1; max] of alternatives. */
    for (i = 0; i < n_alts; i++)
    {
        rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alts[i]);
    }

    /*- Queue data to all alternatives. */
    for (i = 0; i < n_alts; i++)
    {
        rpc_zft_alternatives_queue(pco_iut, iut_zft1, alts[i],
                                   &iovs[i], 1, 0);
    }

    /*- Call @c zft_shutdown_tx() if @p zock_shutdown is @c TRUE, else
     * release zft zocket. */

    if (zock_shutdown)
    {
        rpc_zft_shutdown_tx(pco_iut, iut_zft1);
    }
    else
    {
        rpc_zft_free(pco_iut, iut_zft1);
        iut_zft1 = RPC_NULL;
    }

    /*- If @p alts_send is @c TRUE:
     *  - try to send data with @c zf_alternatives_send();
     *  - it should fail on all alternatives. */

    if (alts_send)
    {
        for (i = 0; i < n_alts; i++)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zf_alternatives_send(pco_iut, stack, alts[i]);
            if (rc >= 0)
                TEST_VERDICT("zf_alternatives_send() "
                             "unexpectedly succeeded");
            else if (RPC_ERRNO(pco_iut) != RPC_EINVAL)
                TEST_VERDICT("zf_alternatives_send() failed with "
                             "unexpected errno %r", RPC_ERRNO(pco_iut));
        }
    }

    /*- Cancel alternatives if @p alts_cancel is @c TRUE. */

    if (alts_cancel)
    {
        for (i = 0; i < n_alts; i++)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zf_alternatives_cancel(pco_iut, stack, alts[i]);
            if (rc < 0)
                TEST_VERDICT("zf_alternatives_cancel() unexpectedly "
                             "failed with errno %r", RPC_ERRNO(pco_iut));
        }
    }

    /*- Release alternatives if @p alts_release is @c TRUE. */

    if (alts_release)
    {
        for (i = 0; i < n_alts; i++)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zf_alternatives_release(pco_iut, stack, alts[i]);
            if (rc < 0)
                TEST_VERDICT("zf_alternatives_release() unexpectedly "
                             "failed with errno %r", RPC_ERRNO(pco_iut));
        }
    }

    /*- If @p zock_shutdown is @c TRUE, send data from Tester, then
     * read and check data on IUT. */
    if (zock_shutdown)
        zfts_zft_check_receiving(pco_iut, stack, iut_zft1, pco_tst, tst_s1);

    /*- If @p alts_release is @c FALSE:
     *  - establish new TCP connection;
     *  - reuse the allocated alternatives:
     *    - queue data on all alternatives, send data from random one;
     *    - read and check data on tester. */

    if (!alts_release)
    {
        CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr, &iut_addr2));
        CHECK_RC(tapi_sockaddr_clone(pco_tst, tst_addr, &tst_addr2));

        zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                     attr, stack,
                                     &iut_zftl2, &iut_zft2, SA(&iut_addr2),
                                     pco_tst, &tst_s2, SA(&tst_addr2),
                                     -1, -1);

        for (i = 0; i < n_alts; i++)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zft_alternatives_queue(pco_iut, iut_zft2, alts[i],
                                            &iovs[i], 1, 0);
            if (rc < 0)
            {
                if (alts_cancel)
                    TEST_VERDICT("zft_alternatives_queue() unexpectedly "
                                 "failed with errno %r for new connection",
                                 RPC_ERRNO(pco_iut));
                else if (RPC_ERRNO(pco_iut) != RPC_EINVAL)
                    TEST_VERDICT("zft_alternatives_queue() failed with "
                                 "unexpected errno %r for new connection",
                                 RPC_ERRNO(pco_iut));
            }
        }

        if (alts_cancel)
        {
            i = rand_range(0, n_alts - 1);
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zf_alternatives_send(pco_iut, stack, alts[i]);
            if (rc < 0)
                TEST_VERDICT("zf_alternatives_send() unexpectedly "
                             "failed with errno %r for the second "
                             "connection", RPC_ERRNO(pco_iut));
            te_dbuf_append(&iut_sent, iovs[i].iov_base,
                           iovs[i].iov_len);

            for (j = 0; j < n_alts; j++)
            {
                if (j != i)
                    rpc_zf_alternatives_cancel(pco_iut, stack, alts[j]);
            }

            rpc_zf_process_events(pco_iut, stack);

            zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s2,
                                   &tst_received);

            ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr, iut_sent.ptr,
                                     tst_received.len, iut_sent.len,
                                     " from IUT via the second connection");
        }
    }

    /*- Release the first zocket if @p zock_shutdown is @c TRUE. */
    if (zock_shutdown)
    {
        rpc_zft_free(pco_iut, iut_zft1);
        iut_zft1 = RPC_NULL;
    }

    TEST_SUCCESS;

cleanup:

    te_dbuf_free(&iut_sent);
    te_dbuf_free(&tst_received);

    for (i = 0; i < n_alts; i++)
    {
        if (!alts_release)
            CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alts[i]);
        rpc_release_iov(&iovs[i], 1);
    }

    CLEANUP_RPC_CLOSE(pco_tst, tst_s1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl1);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s2);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft2);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl2);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

