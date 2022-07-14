/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id:$
 */

/**
 * @page udp_rx-bind_address_types  Test possible kinds of remote and local address types to bind.
 *
 * @objective  Pass a specific address, @c INADDR_ANY or @c NULL using
 *             (non-)zero port as remote and local address arguments in
 *             @a zfur_addr_bind() call, check that data can be received in
 *             all cases.
 *
 * @param pco_iut           PCO on IUT.
 * @param pco_tst           PCO on TST.
 * @param iut_addr          IUT address.
 * @param tst_addr          Tester address.
 * @param local_addr_type   Local address type:
 *      - @c IPv4:\<non-zero port>
 *      - @c INADDR_ANY:\<non-zero port> (unsupported)
 *      - @c IPv4:\<zero port> (unsupported)
 *      - @c INADDR_ANY:\<zero port> (unsupported)
 *      - @c NULL (unsupported)
 * @param remote_addr_type  Remote address type:
 *      - @c IPv4:\<non-zero port>
 *      - @c INADDR_ANY:\<non-zero port> (unsupported)
 *      - @c IPv4:\<zero port> (unsupported)
 *      - @c INADDR_ANY:\<zero port>
 *      - @c NULL
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/bind_address_types"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    struct sockaddr       *iut_addr_r = NULL;
    const struct sockaddr *tst_addr = NULL;
    tapi_address_type      local_addr_type;
    tapi_address_type      remote_addr_type;

    rpc_zf_attr_p    attr = RPC_NULL;
    rpc_zf_stack_p   stack = RPC_NULL;
    rpc_zfur_p       iut_s = RPC_NULL;
    int tst_s;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(local_addr_type, TAPI_ADDRESS_TYPE);
    TEST_GET_ENUM_PARAM(remote_addr_type, TAPI_ADDRESS_TYPE);

    tapi_rpc_provoke_arp_resolution(pco_tst, iut_addr);

    /*- Allocate Zetaferno attributes and stack. */
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Allocate Zetaferno UDP RX zocket. */
    rpc_zfur_alloc(pco_iut, &iut_s, stack, attr);

    /*- Bind the zocket using both local and remote addresses. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = zfts_zfur_bind(pco_iut, iut_s, local_addr_type, iut_addr,
                        &iut_addr_r, remote_addr_type, tst_addr, 0);
    if (rc != 0)
        TEST_VERDICT("Function zfur_addr_bind() failed with errno: %r",
                     RPC_ERRNO(pco_iut));

    /*- Create UDP socket on tester. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);
    rpc_connect(pco_tst, tst_s, iut_addr_r);

    /*- Send a few datagrams from the tester, check that all datagrams are
     * received, validate received data. */
    zfts_zfur_check_reception(pco_iut, stack, iut_s, pco_tst, tst_s, NULL);

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
