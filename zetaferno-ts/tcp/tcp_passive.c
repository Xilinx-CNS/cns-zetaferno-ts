/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-tcp_passive Passive TCP connection opening using Zetaferno Direct API
 *
 * @objective  Test passive opening of TCP connection, sending and receiving
 *             some data over it.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param shutdown_how  How to shutdown listener zocket:
 *                      @b zftl_free or do not shutdown.
 * @param recv_func     TCP receive function to test:
 *                      - zft_zc_recv
 *                      - zft_recv
 * @param send_func     TCP send function to test:
 *                      - zft_send
 *                      - zft_send_single
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

#define TE_TEST_NAME  "tcp/tcp_passive"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    const struct if_nameindex *iut_if = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zftl_p          iut_zftl = RPC_NULL;
    rpc_zft_p           iut_zft = RPC_NULL;
    rpc_zf_waitable_p   iut_zftl_waitable = RPC_NULL;
    int                 tst_s = -1;

    struct sockaddr_in laddr;

    zfts_tcp_shutdown_func  shutdown_how = ZFTS_TCP_SHUTDOWN_NONE;
    zfts_tcp_recv_func_t    recv_func;
    zfts_tcp_send_func_t    send_func;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(iut_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(shutdown_how, ZFTS_TCP_SHUTDOWN_FUNCS);
    TEST_GET_ENUM_PARAM(recv_func, ZFTS_TCP_RECV_FUNCS);
    TEST_GET_ENUM_PARAM(send_func, ZFTS_TCP_SEND_FUNCS);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Create listening zocket binding it to the specific address:port. */
    rpc_zftl_listen(pco_iut, stack, iut_addr, attr, &iut_zftl);

    /*- Call @b zftl_getname(), check that it returns correct
     * local address. */
    rpc_zftl_getname(pco_iut, iut_zftl, &laddr);
    if (tapi_sockaddr_cmp(iut_addr, SA(&laddr)) != 0)
        ERROR_VERDICT("zftl_getname() returned incorrect local address");

    /*- Call @b zftl_to_waitable() to obtain waitable handle for
     * listener zocket. */
    iut_zftl_waitable = rpc_zftl_to_waitable(pco_iut, iut_zftl);

    /*- Create TCP socket on tester and connect it to IUT zocket. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_STREAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    pco_tst->op = RCF_RPC_CALL;
    rpc_connect(pco_tst, tst_s, iut_addr);

    /*- Accept connection on the listening zocket. */
    ZFTS_WAIT_NETWORK(pco_iut, stack);
    rpc_zftl_accept(pco_iut, iut_zftl, &iut_zft);

    pco_tst->op = RCF_RPC_WAIT;
    rpc_connect(pco_tst, tst_s, iut_addr);

    /*- Shutdown the listening zocket in the way determined
     * by @p shutdown_how. */
    if (shutdown_how == ZFTS_TCP_SHUTDOWN_FREE)
        rpc_zftl_free(pco_iut, iut_zftl);

    /*- Check that data can be transmitted in both directions.
     * Call and check @b zft_state(), @b zft_error(),
     * @b zft_get_header_size(), @b zft_getname(), @b zft_to_waitable(),
     * @b zft_send_space() and @b zft_shutdown_tx().
     * Call and check @b zft_recv() and zft_error() after closing
     * Tester socket. */
    zfts_zft_sanity_checks(pco_iut, stack, iut_zft, iut_if, iut_addr,
                           pco_tst, &tst_s, tst_addr, recv_func, send_func);

    TEST_SUCCESS;

cleanup:

    if (shutdown_how != ZFTS_TCP_SHUTDOWN_FREE)
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zftl_waitable);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
