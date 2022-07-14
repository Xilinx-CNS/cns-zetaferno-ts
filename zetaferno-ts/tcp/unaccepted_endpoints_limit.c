/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-unaccepted_endpoints_limit  Unaccepted endpoints limit
 *
 * @objective  Initiate a lot of connection requests from tester - more then
 *             the endpoints limit, check that the first (64 by default)
 *             connections are accepted.
 *
 * @param pco_iut   PCO on IUT.
 * @param pco_tst   PCO on TST.
 * @param iut_addr  IUT address.
 * @param tst_addr  Tester address.
 * @param max_tcp_endpoints ZF attribute @b max_tcp_endpoints value:
 *      - do not set (default endpoints number is @c 64);
 *      - @c 1;
 *      - @c 10;
 *      - @c 64;
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "tcp/unaccepted_endpoints_limit"

#include "zf_test.h"
#include "rpc_zf.h"

/** Maximum number of connections we try to establish. */
#define MAX_CONNECTIONS_NUM 1000

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr tst_addr_zero_port;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zftl_p     iut_tl = RPC_NULL;

    int         tst_socks[MAX_CONNECTIONS_NUM];
    rpc_zft_p   iut_zfts[MAX_CONNECTIONS_NUM];
    int         i;

    const char *max_tcp_endpoints = NULL;
    int         max_tcp_endpoints_int = 0;
    int         successful_connects = 0;
    int         successful_accepts = 0;
    int         successful_conn_checks = 0;
    te_bool     test_failed = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_STRING_PARAM(max_tcp_endpoints);

    /*- Specify ZF attribute @b max_tcp_endpoints value and create stack. */
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    if (strcmp(max_tcp_endpoints, "do_not_set") != 0)
    {
        max_tcp_endpoints_int = atoi(max_tcp_endpoints);
        rpc_zf_attr_set_int(pco_iut, attr, "max_tcp_endpoints",
                            max_tcp_endpoints_int);
    }
    else
    {
        /* Default attribute value */
        max_tcp_endpoints_int = ZFTS_MAX_TCP_ENDPOINTS;
    }

    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Create and bind listening zocket. */
    rpc_zftl_listen(pco_iut, stack, iut_addr, attr, &iut_tl);

    memcpy(&tst_addr_zero_port, tst_addr, sizeof(struct sockaddr));
    SIN(&tst_addr_zero_port)->sin_port = 0;

    for (i = 0; i < MAX_CONNECTIONS_NUM; i++)
    {
        tst_socks[i] = -1;
        iut_zfts[i] = RPC_NULL;
    }

    /*- Initiate a number (exceeding endpoints limit) of connection requests
     * from tester. */

    for (i = 0; i < MAX_CONNECTIONS_NUM; i++)
    {
        tst_socks[i] = rpc_socket(pco_tst,
                                  rpc_socket_domain_by_addr(tst_addr),
                                  RPC_SOCK_STREAM, RPC_PROTO_DEF);
        rpc_bind(pco_tst, tst_socks[i], &tst_addr_zero_port);
        rpc_fcntl(pco_tst, tst_socks[i], RPC_F_SETFL, RPC_O_NONBLOCK);

        RPC_AWAIT_IUT_ERROR(pco_tst);
        rpc_connect(pco_tst, tst_socks[i], iut_addr);

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_process_events_long(pco_iut, stack,
                                        ZFTS_WAIT_EVENTS_TIMEOUT);
        if (rc < 0)
            TEST_VERDICT("Failed to process incoming connection, errno %r",
                         RPC_ERRNO(pco_iut));

        RPC_AWAIT_IUT_ERROR(pco_tst);
        rc = rpc_connect(pco_tst, tst_socks[i], iut_addr);
        if (rc < 0)
            break;

        successful_connects = i + 1;
    }

    /*- Check that first @p max_tcp_endpoints connections can be accepted
     * and the number of connections does not exceed set endpoints limit. */
    if (successful_connects != max_tcp_endpoints_int)
    {
        ERROR("Unexpected number of successful connect() "
              "calls: %d instead of %d",
              successful_connects, max_tcp_endpoints_int);

        if (successful_connects > max_tcp_endpoints_int)
            ERROR_VERDICT("More than expected connect() "
                          "calls were successful");
        else
            ERROR_VERDICT("Less than expected connect() "
                          "calls were successful");

        test_failed = TRUE;
    }

    for (i = 0; i < successful_connects; i++)
    {
        RPC_AWAIT_IUT_ERROR(pco_iut);
        rc = rpc_zftl_accept(pco_iut, iut_tl, &iut_zfts[i]);
        if (rc < 0)
            break;

        successful_accepts = i + 1;
    }

    if (successful_accepts < successful_connects)
    {
        ERROR_VERDICT("Less connections were accepted "
                      " than the number of successful connect() calls");

        test_failed = TRUE;
    }

    /*- Send/receive some data by each established connection. */
    for (i = 0; i < successful_accepts; i++)
    {
        zfts_zft_check_connection(pco_iut, stack, iut_zfts[i],
                                  pco_tst,
                                  /*
                                   * zftl_accept() returns connections
                                   * in reverse order.
                                   */
                                  tst_socks[successful_accepts - i - 1]);
        successful_conn_checks = i + 1;
    }

    if (test_failed)
        TEST_STOP;
    TEST_SUCCESS;

cleanup:

    if (successful_conn_checks < successful_accepts)
    {
        ERROR_VERDICT("Checking data transmission "
                      "via accepted connections failed");
    }

    for (i = 0; i < successful_connects; i++)
    {
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zfts[i]);
        CLEANUP_RPC_CLOSE(pco_tst, tst_socks[i]);
    }

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_tl);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
