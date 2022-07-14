/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-connect_after_fault Connect after failed connection attempt
 *
 * @objective Establish TCP connection after failed connection attempt.
 *
 * @param rst       If @c TRUE send @b RST in answer to @b SYN, else just
 *                  do not reply to @b SYN.
 * @param no_route  If @c TRUE, call zft_connect() before routes via
 *                  gateway are configured (makes sense only when
 *                  @p rst is @c FALSE).
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/connect_after_fault"

#include "zf_test.h"
#include "rpc_zf.h"

#include "te_dbuf.h"

/**
 * How long to wait until TCP zocket connect attempt is
 * considered unsuccessful.
 */
#define CONNECT_TIMEOUT 8000

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    rcf_rpc_server *pco_gw = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr;
    const struct sockaddr *gw_iut_addr = NULL;
    const struct sockaddr *gw_tst_addr = NULL;

    rpc_zft_handle_p  iut_zft_handle = RPC_NULL;
    rpc_zft_p         iut_zft1 = RPC_NULL;
    rpc_zft_p         iut_zft2 = RPC_NULL;
    int               tst_s_listening = -1;
    int               tst_s = -1;
    int               addr_family;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    te_bool                route_dst_added = FALSE;
    te_bool                route_src_added = FALSE;

    int           ipv4_fw;
    int           tcp_state;
    rpc_errno     zft_err;

    te_bool rst;
    te_bool no_route;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_gw);
    TEST_GET_PCO(pco_tst);

    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ADDR_NO_PORT(gw_iut_addr);
    TEST_GET_ADDR_NO_PORT(gw_tst_addr);
    TEST_GET_BOOL_PARAM(rst);
    TEST_GET_BOOL_PARAM(no_route);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Set attribute @b tcp_syn_retries to @c 0 to reduce time to wait
     * until connect attempt is failed, unless @p rst is @c TRUE. */
    if (!rst)
        rpc_zf_attr_set_int(pco_iut, attr, "tcp_syn_retries", 0);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- if @p no_route is @c TRUE, allocate TCP zocket handle,
     * call @b zft_connect() and check that it fails with
     * @c EHOSTUNREACH error. Finish testing after that. */
    if (no_route)
    {
        rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);
        rpc_zft_addr_bind(pco_iut, iut_zft_handle, iut_addr, 0);

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_connect(pco_iut, iut_zft_handle, tst_addr, &iut_zft1);
        if (rc >= 0)
            TEST_VERDICT("zft_connect() succeeded unexpectedly");
        else if (RPC_ERRNO(pco_iut) != RPC_EHOSTUNREACH)
            ERROR_VERDICT("zft_connect() returned unexpected errno %r",
                          RPC_ERRNO(pco_iut));

        TEST_SUCCESS;
    }

    /*- Configure gateway between IUT and Tester. */

    addr_family = iut_addr->sa_family;

    /* Add route on 'pco_iut': 'tst_addr' via gateway 'gw_iut_addr' */
    if (tapi_cfg_add_route_via_gw(
            pco_iut->ta,
            addr_family,
            te_sockaddr_get_netaddr(tst_addr),
            te_netaddr_get_size(addr_family) * 8,
            te_sockaddr_get_netaddr(gw_iut_addr)) != 0)
    {
        TEST_FAIL("Cannot add route to the Tester");
    }
    route_dst_added = TRUE;

    /* Add route on 'pco_tst': 'iut_addr' via gateway 'gw_tst_addr' */
    if (tapi_cfg_add_route_via_gw(
            pco_tst->ta,
            addr_family,
            te_sockaddr_get_netaddr(iut_addr),
            te_netaddr_get_size(addr_family) * 8,
            te_sockaddr_get_netaddr(gw_tst_addr)) != 0)
    {
        TEST_FAIL("Cannot add route to the IUT");
    }
    route_src_added = TRUE;

    /*- If @p rst is @c FALSE, break connection between IUT
     * and Tester by disabling forwarding on gateway. */

    if (rst)
        ipv4_fw = 1;
    else
        ipv4_fw = 0;

    CHECK_RC(tapi_cfg_sys_set_int(pco_gw->ta, ipv4_fw, NULL,
                                  "net/ipv4/ip_forward"));

    CFG_WAIT_CHANGES;

    /*- Allocate and bind TCP zocket handle. */
    rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);
    rpc_zft_addr_bind(pco_iut, iut_zft_handle, iut_addr, 0);

    /*- Call @b zft_connect() and check that it succeeds,
     * and after that zocket is firstly in @c TCP_SYN_SENT
     * state and later moves to @c TCP_CLOSE state. */
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zft_connect(pco_iut, iut_zft_handle, tst_addr, &iut_zft1);
    if (rc < 0)
        TEST_VERDICT("zft_connect() unexpectedly failed with errno %r",
                     RPC_ERRNO(pco_iut));
    iut_zft_handle = RPC_NULL;
    tcp_state = rpc_zft_state(pco_iut, iut_zft1);
    if (tcp_state != RPC_TCP_SYN_SENT)
        TEST_VERDICT("TCP zocket has unexpected state %s "
                     "just after zft_connect()",
                     tcp_state_rpc2str(tcp_state));

    rpc_zf_process_events_long(pco_iut, stack, CONNECT_TIMEOUT);

    tcp_state = rpc_zft_state(pco_iut, iut_zft1);
    if (tcp_state != RPC_TCP_CLOSE)
        TEST_VERDICT("TCP zocket has unexpected state %s "
                     "after processing connect attempt",
                     tcp_state_rpc2str(tcp_state));

    zft_err = rpc_zft_error(pco_iut, iut_zft1);
    if ((rst && zft_err != RPC_ECONNREFUSED) ||
        (!rst && zft_err != RPC_ETIMEDOUT))
        TEST_VERDICT("zft_error() returned unexpected errno %s (%d)",
                     errno_rpc2str(zft_err), zft_err);

    /*- If @p rst is @c FALSE, repair network connection between
     * IUT and Tester by enabling forwarding on gateway. */
    if (!rst)
    {
        /* Turn on forwarding on router host */
        CHECK_RC(tapi_cfg_sys_set_int(pco_gw->ta, 1, NULL,
                                      "net/ipv4/ip_forward"));

        CFG_WAIT_CHANGES;
    }

    /*- Create listener socket on tester. */
    tst_s_listening =
      rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                 RPC_PROTO_DEF, FALSE, FALSE, tst_addr);
    rpc_listen(pco_tst, tst_s_listening, 1);

    /*- Establish TCP connection (using the same zft_handle if
     * zft_connect() failed previously). */
    if (iut_zft_handle == RPC_NULL)
        rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);
    rpc_zft_connect(pco_iut, iut_zft_handle, tst_addr, &iut_zft2);
    iut_zft_handle = RPC_NULL;
    ZFTS_WAIT_NETWORK(pco_iut, stack);
    tst_s = rpc_accept(pco_tst, tst_s_listening, NULL, NULL);

    /*- Check data transmission. */
    zfts_zft_check_connection(pco_iut, stack, iut_zft2, pco_tst, tst_s);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s_listening);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft2);

    CLEANUP_RPC_ZFT_HANDLE_FREE(pco_iut, iut_zft_handle);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    if (route_dst_added &&
        tapi_cfg_del_route_via_gw(
            pco_iut->ta,
            addr_family,
            te_sockaddr_get_netaddr(tst_addr),
            te_netaddr_get_size(addr_family) * 8,
            te_sockaddr_get_netaddr(gw_iut_addr)) != 0)
    {
        ERROR("Cannot delete route to the Tester");
        result = EXIT_FAILURE;
    }

    if (route_src_added &&
        tapi_cfg_del_route_via_gw(
            pco_tst->ta,
            addr_family,
            te_sockaddr_get_netaddr(iut_addr),
            te_netaddr_get_size(addr_family) * 8,
            te_sockaddr_get_netaddr(gw_tst_addr)) != 0)
    {
        ERROR("Cannot delete route to the IUT");
        result = EXIT_FAILURE;
    }

    TEST_END;
}
