/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id:$
 */

/**
 * @page udp_rx-endpoints_limit Examine UDP RX zockets number limits
 *
 * @objective Specify maximum number of endpoints using ZF attribute
 *            @b max_udp_rx_endpoints, allocate all possible zockets and
 *            check that they can accept datagrams.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param max_endpoints Endpoints number to specify with attribute
 *                      @b max_udp_rx_endpoints, @c -1 can be used to leave
 *                      it unspecified (default).
 * @param binds_num     How many times every zocket should be bound.
 * @param bind_later    Bind zockets just after allocation if @c FALSE.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/endpoints_limit"

#include "zf_test.h"
#include "rpc_zf.h"

/**
 * Bind UDP RX zocket @p binds_num times, use addresses from the array
 * @p laddr and new port for each bind.
 *
 * @param pco_iut   IUT RPC server handle.
 * @param urx       UDP RX zocket handle.
 * @param tst_addr  Tester address.
 * @param laddr     IP addresses array.
 * @param binds_num How many times bind the zocket.
 *
 * @return Pointer to the last bound address.
 */
static struct sockaddr *
zocket_bind_many(rcf_rpc_server *pco_iut, rpc_zfur_p urx,
                 const struct sockaddr *tst_addr, struct sockaddr **laddr,
                 int binds_num)
{
    struct sockaddr *iut_addr_bind =
        tapi_sockaddr_clone_typed(*laddr, TAPI_ADDRESS_SPECIFIC);
    int i;

    for (i = 0; i < binds_num; i++)
    {
        tapi_sockaddr_clone(pco_iut, laddr[i], SS(iut_addr_bind));
        RPC_AWAIT_IUT_ERROR(pco_iut);
        if (rpc_zfur_addr_bind(pco_iut, urx, iut_addr_bind, tst_addr, 0) < 0)
        {
            TEST_VERDICT("zfur_addr_bind() failed with errno: %r",
                         RPC_ERRNO(pco_iut));
        }
    }

    return iut_addr_bind;
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server            *pco_iut;
    rcf_rpc_server            *pco_tst;
    const struct sockaddr     *iut_addr = NULL;
    const struct sockaddr     *tst_addr = NULL;
    const struct if_nameindex *iut_if = NULL;
    tapi_env_net              *net = NULL;
    int max_endpoints;
    int binds_num;
    int bind_later;

    struct sockaddr **laddr_list = NULL;
    struct sockaddr *iut_addr_bind[ZFTS_MAX_UDP_ENDPOINTS] = {NULL};
    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zfur_p     iut_s[ZFTS_MAX_UDP_ENDPOINTS + 1] = {0};
    int tst_s = -1;
    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_IF(iut_if);
    TEST_GET_NET(net);
    TEST_GET_INT_PARAM(max_endpoints);
    TEST_GET_INT_PARAM(binds_num);
    TEST_GET_BOOL_PARAM(bind_later);

    /*- Allocate and add @p binds_number IP addresses on IUT. */
    laddr_list = tapi_env_add_addresses(pco_iut, net, AF_INET, iut_if,
                                        binds_num);
    CHECK_NOT_NULL(laddr_list);

    /* Resolve ARP for new added IPs. */
    for (i = 0; i < binds_num; i++)
        tapi_rpc_provoke_arp_resolution(pco_tst, laddr_list[i]);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Set attribute @b max_udp_rx_endpoints to @p max_endpoints. */
    if (max_endpoints != -1)
        rpc_zf_attr_set_int(pco_iut, attr, "max_udp_rx_endpoints",
                            max_endpoints);
    else
        max_endpoints = ZFTS_MAX_UDP_ENDPOINTS;

    /*- Allocate Zetaferno stack, the call should fail if an invalid value
     * of @b max_udp_rx_endpoints is specified. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zf_stack_alloc(pco_iut, attr, &stack);
    if (max_endpoints == 0 || max_endpoints > ZFTS_MAX_UDP_ENDPOINTS)
    {
        if (rc == 0)
            TEST_VERDICT("Stack allocation unexpectedly succeeded");
        if (RPC_ERRNO(pco_iut) != RPC_EINVAL)
            TEST_VERDICT("Stack allocation failed with unexpected errno %r",
                         RPC_ERRNO(pco_iut));
        TEST_SUCCESS;
    }
    else if (rc != 0)
        TEST_VERDICT("Stack allocation unexpectedly failed: %r",
                     RPC_ERRNO(pco_iut));

    /*- Allocate maximum number of UDP RX zockets, bind them just after
     * allocation if @p bind_later is @c FALSE:
     *  - Each zocket is bound @p bind_num number times. */
    for (i = 0; i < max_endpoints; i++)
    {
        rpc_zfur_alloc(pco_iut, iut_s + i, stack, attr);

        if (!bind_later)
        {
            iut_addr_bind[i] = zocket_bind_many(pco_iut, iut_s[i], tst_addr,
                                                laddr_list, binds_num);
        }
    }

    /*- Try to allocate one more zocket, the call should fail. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zfur_alloc(pco_iut, iut_s + i, stack, attr);
    if (rc == 0)
        TEST_VERDICT("UDP RX zocket allocation unexpectedly succeeded");

    if (RPC_ERRNO(pco_iut) != RPC_ENOBUFS)
        TEST_VERDICT("Zocket allocation failed with unexpected errno %r",
                     RPC_ERRNO(pco_iut));

    /*- Bind all allocated zockets now if @p bind_later is @c TRUE. */
    if (bind_later)
    {
        for (i = 0; i < max_endpoints; i++)
        {
            iut_addr_bind[i] = zocket_bind_many(pco_iut, iut_s[i], tst_addr,
                                                laddr_list, binds_num);
        }
    }

    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    /*- Check that all bound zockets can receive datagrams. */
    for (i = 0; i < max_endpoints; i++)
        zfts_zfur_check_reception(pco_iut, stack, iut_s[i], pco_tst, tst_s,
                                  iut_addr_bind[i]);

    TEST_SUCCESS;

cleanup:
    if (stack != RPC_NULL)
    {
        for (i = 0; i < max_endpoints; i++)
        {
            CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s[i]);
        }
    }
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    TEST_END;
}
