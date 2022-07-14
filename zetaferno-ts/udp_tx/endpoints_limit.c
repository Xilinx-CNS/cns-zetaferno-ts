/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP TX tests
 *
 * $Id$
 */

/**
 * @page udp_tx-endpoints_limit Examine UDP TX zockets number limits
 *
 * @objective Specify maximum number of endpoints using ZF attribute
 *            @b max_udp_tx_endpoints, allocate all possible zockets and
 *            check that they can accept datagrams.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param func          Transmitting function.
 * @param large_buffer  Use large data buffer to send.
 * @param few_iov       Use several iov vectors.
 * @param send_later    Send data just after zocket allocation if @c FALSE.
 * @param max_endpoints Endpoints number to specify with attribute
 *                      @b max_udp_tx_endpoints, @c -1 can be used to leave
 *                      it unspecified (default):
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
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_tx/endpoints_limit"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    int                    max_endpoints;
    zfts_send_function     func;
    te_bool                large_buffer;
    te_bool                few_iov;
    te_bool                send_later;

    rpc_zf_attr_p    attr = RPC_NULL;
    rpc_zf_stack_p   stack = RPC_NULL;
    rpc_zfut_p      *iut_utx = NULL;
    rpc_zfur_p       iut_urx = RPC_NULL;
    int              tst_s = -1;
    int              i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);
    TEST_GET_BOOL_PARAM(send_later);
    TEST_GET_INT_PARAM(max_endpoints);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Set attribute @b max_udp_tx_endpoints to @p max_endpoints. */
    if (max_endpoints != -1)
        rpc_zf_attr_set_int(pco_iut, attr, "max_udp_tx_endpoints",
                            max_endpoints);
    else
        max_endpoints = ZFTS_MAX_UDP_ENDPOINTS;

    iut_utx = tapi_calloc(max_endpoints + 1, sizeof(*iut_utx));

    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Create UDP socket on tester and bind it to the local IPv4 address. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    /*- Allocate maximum number of UDP TX zockets, if send_later
     * is @c FALSE: */
    for (i = 0; i < max_endpoints; i++)
    {
        rpc_zfut_alloc(pco_iut, iut_utx + i, stack, iut_addr, tst_addr, 0,
                       attr);
        /*-- Send few datagrams from created zockets, receive them on tester
         * and check data. */
        if (!send_later)
            zfts_zfut_check_send_func(pco_iut, stack, iut_utx[i], pco_tst,
                                      tst_s, func, large_buffer, few_iov);
    }

    /*- Try to allocate one more TX zocket, the call should fail. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zfut_alloc(pco_iut, iut_utx + i, stack, iut_addr, tst_addr, 0,
                        attr);
    if (rc == 0)
        TEST_VERDICT("UDP TX zocket allocation unexpectedly succeeded");

    if (RPC_ERRNO(pco_iut) != RPC_ENOBUFS)
        TEST_VERDICT("Zocket allocation failed with unexpected errno %r",
                     RPC_ERRNO(pco_iut));

    /*- Check datagrams transmission now if @p send_later is @c TRUE. */
    if (send_later)
    {
        for (i = 0; i < max_endpoints; i++)
            zfts_zfut_check_send_func(pco_iut, stack, iut_utx[i], pco_tst,
                                      tst_s, func, large_buffer, few_iov);
    }

    /*- Allocate and bind UDP RX zocket. */
    rpc_zfur_alloc(pco_iut, &iut_urx, stack, attr);
    rpc_zfur_addr_bind(pco_iut, iut_urx, SA(iut_addr), tst_addr, 0);

    /*- Check that UDP RX zocket can receive datagrams. */
    zfts_zfur_check_reception(pco_iut, stack, iut_urx, pco_tst, tst_s,
                              iut_addr);

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    if (iut_utx != NULL)
    {
        for (i = 0; i < max_endpoints; i++)
            CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_utx[i]);
    }

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_urx);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
