/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP TX tests
 *
 * $Id$
 */

/**
 * @page udp_tx-laddr_inaddr_any Correct source address is used if bind to @c INADDR_ANY.
 *
 * @objective Bind two zockets using INADDR_ANY in laddr and different
 *            destination addresses. Check that datagrams are sent from two
 *            different source addresses.
 *
 * @param func          Transmitting function.
 * @param large_buffer  Use large data buffer to send if @c TRUE.
 * @param few_iov       Use several iov vectors if @c TRUE.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "udp_tx/laddr_inaddr_any"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server              *pco_iut = NULL;
    rcf_rpc_server              *pco_tst = NULL;
    const struct sockaddr       *iut_addr1 = NULL;
    const struct sockaddr       *iut_addr2 = NULL;
    const struct sockaddr       *tst_addr1 = NULL;
    const struct sockaddr       *tst_addr2 = NULL;
    const struct if_nameindex   *iut_if = NULL;

    struct sockaddr *iut_bind_addr1 = NULL;
    struct sockaddr *iut_bind_addr2 = NULL;

    rpc_zf_attr_p   attr = RPC_NULL;
    rpc_zf_stack_p  stack = RPC_NULL;
    rpc_zfut_p      iut_utx1 = RPC_NULL;
    rpc_zfut_p      iut_utx2 = RPC_NULL;
    int             tst_s1 = -1;
    int             tst_s2 = -1;

    cfg_handle      rt_handle1 = CFG_HANDLE_INVALID;
    cfg_handle      rt_handle2 = CFG_HANDLE_INVALID;

    zfts_send_function     func;
    te_bool                large_buffer;
    te_bool                few_iov;

    TEST_START;
    TEST_GET_IF(iut_if);
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr1);
    TEST_GET_ADDR(pco_iut, iut_addr2);
    TEST_GET_ADDR(pco_tst, tst_addr1);
    TEST_GET_ADDR(pco_tst, tst_addr2);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);

    /*- Set up routing table to use different source addresses
     * for two destinations. */

    CHECK_RC(tapi_cfg_add_route(pco_iut->ta,
                                tst_addr1->sa_family,
                                te_sockaddr_get_netaddr(tst_addr1),
                                te_netaddr_get_size(
                                    tst_addr1->sa_family) * 8,
                                NULL,
                                iut_if->if_name,
                                te_sockaddr_get_netaddr(iut_addr1),
                                0, 0, 0, 0, 0, 0, &rt_handle1));

    CHECK_RC(tapi_cfg_add_route(pco_iut->ta,
                                tst_addr2->sa_family,
                                te_sockaddr_get_netaddr(tst_addr2),
                                te_netaddr_get_size(
                                    tst_addr2->sa_family) * 8,
                                NULL,
                                iut_if->if_name,
                                te_sockaddr_get_netaddr(iut_addr2),
                                0, 0, 0, 0, 0, 0, &rt_handle2));

    CFG_WAIT_CHANGES;

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate two UDP TX zockets: use INADDR_ANY in laddr
     *  and different destination addresses. */

    iut_bind_addr1 = tapi_sockaddr_clone_typed(iut_addr1,
                                               TAPI_ADDRESS_WILDCARD);
    iut_bind_addr2 = tapi_sockaddr_clone_typed(iut_addr2,
                                               TAPI_ADDRESS_WILDCARD);

    rpc_zfut_alloc(pco_iut, &iut_utx1, stack, iut_bind_addr1,
                   tst_addr1, 0, attr);
    rpc_zfut_alloc(pco_iut, &iut_utx2, stack, iut_bind_addr2,
                   tst_addr2, 0, attr);

    /*- Create two UDP sockets on Tester; bind and connect them to
     * specific addresses to receive datagrams only from the
     * appropriate zockets. */

    tst_s1 = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr1),
                        RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s1, tst_addr1);
    rpc_connect(pco_tst, tst_s1, iut_addr1);

    tst_s2 = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr2),
                        RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s2, tst_addr2);
    rpc_connect(pco_tst, tst_s2, iut_addr2);

    /*- Send datagrams according to parameters @p func, @p large_buffer and
     * @p few_iov. Read and check data on Tester. */
    zfts_zfut_check_send_func(pco_iut, stack, iut_utx1, pco_tst, tst_s1,
                              func, large_buffer, few_iov);
    zfts_zfut_check_send_func(pco_iut, stack, iut_utx2, pco_tst, tst_s2,
                              func, large_buffer, few_iov);

    TEST_SUCCESS;

cleanup:

    if (rt_handle1 != CFG_HANDLE_INVALID)
        cfg_del_instance(rt_handle1, FALSE);
    if (rt_handle2 != CFG_HANDLE_INVALID)
        cfg_del_instance(rt_handle2, FALSE);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s1);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s2);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_utx1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_utx2);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    free(iut_bind_addr1);
    free(iut_bind_addr2);

    TEST_END;
}
