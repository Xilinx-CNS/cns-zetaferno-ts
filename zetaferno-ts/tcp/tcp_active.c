/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-tcp_active Active TCP connection opening using Zetaferno Direct API
 *
 * @objective  Test active opening of TCP connection, sending and receiving
 *             some data over it.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
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

#define TE_TEST_NAME  "tcp/tcp_active"

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

    rpc_zft_handle_p    iut_zft_handle = RPC_NULL;
    rpc_zft_p           iut_zft = RPC_NULL;

    int            tst_s = -1;
    int            tst_s_listening = -1;

    zfts_tcp_recv_func_t recv_func;
    zfts_tcp_send_func_t send_func;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(iut_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(recv_func, ZFTS_TCP_RECV_FUNCS);
    TEST_GET_ENUM_PARAM(send_func, ZFTS_TCP_SEND_FUNCS);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate TCP zocket. */
    rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);

    /*- Bind the zocket to the specific address:port. */
    rpc_zft_addr_bind(pco_iut, iut_zft_handle, iut_addr, 0);

    /*- Create listening socket on tester. */
    tst_s_listening =
      rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                 RPC_PROTO_DEF, FALSE, FALSE, tst_addr);
    rpc_listen(pco_tst, tst_s_listening, 1);

    /*- Connect the zocket to the tester socket. */
    rpc_zft_connect(pco_iut, iut_zft_handle, tst_addr, &iut_zft);
    iut_zft_handle = RPC_NULL;
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    tst_s = rpc_accept(pco_tst, tst_s_listening, NULL, NULL);

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

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s_listening);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);

    CLEANUP_RPC_ZFT_HANDLE_FREE(pco_iut, iut_zft_handle);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
