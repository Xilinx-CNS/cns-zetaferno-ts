/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-listeners_limit Examine listener TCP zockets number limit
 *
 * @objective Specify maximum number of listener endpoints using ZF
 *            attribute @b max_tcp_listen_endpoints, allocate all
 *            possible listener zockets and check that all of them
 *            can accept connections.
 *
 * @param pco_iut             PCO on IUT.
 * @param pco_tst             PCO on TST.
 * @param iut_addr            IUT address.
 * @param tst_addr            Tester address.
 * @param max_endpoints       Listen endpoints number to specify with
 *                            attribute @b max_tcp_listen_endpoints:
 *                            - do not set (default endpoints number
 *                              is @c 16);
 *                            - @c 1;
 *                            - @c 10;
 *                            - @c 16;
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "tcp/listeners_limit"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;


    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    int   max_endpoints = 0;
    int   i = 0;
    int   recreated_i = 0;

    struct sockaddr_storage listen_addrs[ZFTS_MAX_TCP_LISTEN_ENDPOINTS];
    rpc_zftl_p              iut_tls[ZFTS_MAX_TCP_LISTEN_ENDPOINTS];
    rpc_zft_p               iut_zfts[ZFTS_MAX_TCP_LISTEN_ENDPOINTS];
    int                     tst_socks[ZFTS_MAX_TCP_LISTEN_ENDPOINTS];
    rpc_zftl_p              iut_tl_aux = RPC_NULL;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(max_endpoints);

    ZFTS_INIT_STATIC_ARRAY(iut_tls, RPC_NULL);
    ZFTS_INIT_STATIC_ARRAY(iut_zfts, RPC_NULL);
    ZFTS_INIT_STATIC_ARRAY(tst_socks, -1);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Set attribute @b max_tcp_listen_endpoints to @p max_endpoints. */
    if (max_endpoints < 0)
        max_endpoints = ZFTS_MAX_TCP_LISTEN_ENDPOINTS;
    else
        rpc_zf_attr_set_int(pco_iut, attr,
                            "max_tcp_listen_endpoints",
                            max_endpoints);

    /*- Allocate Zetaferno stack. */
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Allocate maximum number of listener zockets. */
    for (i = 0; i < max_endpoints; i++)
    {
        RING("Creating listener zocket %d", i + 1);
        tapi_sockaddr_clone(pco_iut, iut_addr, &listen_addrs[i]);
        rpc_zftl_listen(pco_iut, stack, SA(&listen_addrs[i]),
                        attr, &iut_tls[i]);
    }

    RING("Trying to create one extra listener zocket");

    /*- Allocate one more listener zocket, check that
     * the allocation fails. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zftl_listen(pco_iut, stack, iut_addr, attr, &iut_tl_aux);
    if (rc == 0)
        TEST_VERDICT("Listener zocket was successfully created when "
                     "max_tcp_listen_endpoints limit is already reached");
    else if (RPC_ERRNO(pco_iut) != RPC_ENOBUFS)
        ERROR_VERDICT("zftl_listen() failed with unexpected errno %r",
                      RPC_ERRNO(pco_iut));

    i = rand_range(0, max_endpoints - 1);

    RING("Freeing and recreating listener zocket %d", i + 1);

    /*- Close one (random) of the listener zockets. */
    rpc_zftl_free(pco_iut, iut_tls[i]);
    iut_tls[i] = RPC_NULL;
    tapi_sockaddr_clone(pco_iut, iut_addr, &listen_addrs[i]);

    /*- Allocate one more listener zocket, it should be successful now. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zftl_listen(pco_iut, stack, SA(&listen_addrs[i]),
                         attr, &iut_tls[i]);
    if (rc < 0)
        TEST_VERDICT("Failed to allocate listener zocket after destroying "
                     "another one");

    recreated_i = i;

    /*- Accept connection on each listener zocket. */
    for (i = 0; i < max_endpoints; i++)
    {
        RING("Establishing TCP connection with help of listener zocket %d",
             i + 1);

        tst_socks[i] = rpc_socket(
                           pco_tst,
                           rpc_socket_domain_by_addr(tst_addr),
                           RPC_SOCK_STREAM, RPC_PROTO_DEF);

        pco_tst->op = RCF_RPC_CALL;
        rpc_connect(pco_tst, tst_socks[i], SA(&listen_addrs[i]));

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_wait_for_event(pco_iut, stack);
        if (rc < 0)
        {
            if (i == recreated_i)
                TEST_VERDICT("rpc_zf_wait_for_event() failed "
                             "with errno %r while processing "
                             "incoming connection to recreated "
                             "listener zocket",
                             RPC_ERRNO(pco_iut));
            else
                TEST_VERDICT("rpc_zf_wait_for_event() failed "
                             "with errno %r while processing "
                             "incoming connection",
                             RPC_ERRNO(pco_iut));
        }

        rpc_zf_process_events(pco_iut, stack);

        pco_tst->op = RCF_RPC_WAIT;
        rpc_connect(pco_tst, tst_socks[i], SA(&listen_addrs[i]));

        rpc_zftl_accept(pco_iut, iut_tls[i], &iut_zfts[i]);
    }

    /*- Check data transmission on each established connection. */
    for (i = 0; i < max_endpoints; i++)
    {
        RING("Checking data transmission over connection %d", i + 1);

        zfts_zft_check_connection(pco_iut, stack, iut_zfts[i],
                                  pco_tst, tst_socks[i]);
    }

    TEST_SUCCESS;

cleanup:

    for (i = 0; i < max_endpoints; i++)
    {
        CLEANUP_RPC_CLOSE(pco_tst, tst_socks[i]);

        CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zfts[i]);
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_tls[i]);
    }

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_tl_aux);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
