/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP TX tests
 *
 * $Id$
 */

/**
 * @page udp_tx-address_reusing A few zockets bound to the same addresses couple
 *
 * @objective  Create a few ZF UDP TX zockets with exactly the same
 *             addresses couple @b laddr and @b raddr. Check that datagrams
 *             can be accordingly sent from all zockets.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param iut_addr      IUT address.
 * @param tst_addr      Tester address.
 * @param func          Transmitting function.
 * @param large_buffer  Use large data buffer to send.
 * @param few_iov       Use several iov vectors.
 * @param utx_num       TX zockets number.
 * @param same_stack    Use the same stack for all zockets if @c TRUE.
 * @param addr_type     Local address type:
 *      - @c IPv4:\<non-zero port>
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_tx/address_reusing"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_mem.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    zfts_send_function     func;
    te_bool                large_buffer;
    te_bool                few_iov;
    tapi_address_type      addr_type;
    int                    utx_num;
    te_bool                same_stack;

    rpc_zf_attr_p    attr = RPC_NULL;
    rpc_zf_stack_p  *stack = NULL;
    rpc_zfut_p      *iut_utx = NULL;
    struct sockaddr *laddr;
    int              tst_s = -1;
    int              i;
    int              j;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);
    TEST_GET_ENUM_PARAM(addr_type, TAPI_ADDRESS_TYPE);
    TEST_GET_BOOL_PARAM(same_stack);
    TEST_GET_INT_PARAM(utx_num);

    iut_utx = tapi_calloc(utx_num, sizeof(*iut_utx));
    stack = tapi_calloc(utx_num, sizeof(*stack));

    laddr = tapi_sockaddr_clone_typed(iut_addr, addr_type);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Allocate ZF stack single or for each zocket in dependence on the
     * test argument @p same_stack. */
    if (same_stack)
    {
        rpc_zf_stack_alloc(pco_iut, attr, stack);
        for (i = 1; i < utx_num; i++)
            stack[i] = stack[0];
    }
    else
    {
        for (i = 0; i < utx_num; i++)
            rpc_zf_stack_alloc(pco_iut, attr, stack + i);
    }

    /*- Allocate and bind @p utx_num Zetaferno UDP TX zockets, @c laddr
     * value is dependent on the test argument @p addr_type. */
    for (i = 0; i < utx_num; i++)
        rpc_zfut_alloc(pco_iut, iut_utx + i, stack[i], laddr, tst_addr, 0,
                       attr);

    /*- Create UDP socket on tester and bind to the local IPv4 address. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    /*- Connect the tester socket if @p addr_type is @c specific. */
    if (addr_type == TAPI_ADDRESS_SPECIFIC)
        rpc_connect(pco_tst, tst_s, iut_addr);

    /*- For each zocket: send few datagrams from the zocket, receive them on
     * tester and check data. */
    for (i = 0; i < utx_num; i++)
        zfts_zfut_check_send_func(pco_iut, stack[i], iut_utx[i], pco_tst,
                                  tst_s, func, large_buffer, few_iov);

    /*- In the loop for each zocket:
     *      - free one TX zocket;
     *      - check that the rest zockets can transmit datagrams. */
    for (i = 0; i < utx_num; i++)
    {
        rpc_zfut_free(pco_iut, iut_utx[i]);

        for (j = i + 1; j < utx_num; j++)
            zfts_zfut_check_send_func(pco_iut, stack[j], iut_utx[j],
                                      pco_tst, tst_s, func, large_buffer,
                                      few_iov);
    }

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    if (stack != NULL)
    {
        if (same_stack)
            CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack[0]);
        else
        {
            for (i = 0; i < utx_num; i++)
                CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack[i]);
        }
    }

    CLEANUP_RPC_ZF_ATTR_FREE(pco_iut, attr);
    CLEANUP_RPC_ZF_DEINIT(pco_iut);

    TEST_END;
}
