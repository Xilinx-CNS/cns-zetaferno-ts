/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP TX tests
 *
 * $Id:$
 */

/**
 * @page udp_tx-send_dgram Datagrams transmission using Zetaferno Direct API
 *
 * @objective  Send a datagram using Zetaferno Direct API send()-like
 *             functions.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param func          Transmitting function.
 * @param large_buffer  Use large data buffer to send.
 * @param few_iov       Use several iov vectors.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_tx/send_dgram"

#include "zf_test.h"
#include "rpc_zf.h"

/**
 * Expected total size of all packet headers for UDP packet.
 */
#define EXP_UDP_HEADER_SIZE (14 /* Ethernet */ + 20 /* IP */ + 8 /* UDP */)

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

    const struct if_nameindex *iut_if = NULL;
    unsigned int               vlan_header_add = 0;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zfut_p     iut_utx = RPC_NULL;
    int tst_s;

    unsigned int header_size = 0;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(iut_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate and bind Zetaferno UDP TX zocket. */
    rpc_zfut_alloc(pco_iut, &iut_utx, stack, iut_addr, tst_addr, 0, attr);

    /*- Create UDP socket on tester. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);
    rpc_connect(pco_tst, tst_s, iut_addr);

    if (tapi_interface_is_vlan(pco_iut, iut_if))
        vlan_header_add = ZFTS_VLAN_TAG_LEN;

    header_size = rpc_zfut_get_header_size(pco_iut, iut_utx);
    if (header_size != EXP_UDP_HEADER_SIZE + vlan_header_add)
        ERROR_VERDICT("zfut_get_header_size() returned unexpected value");

    /*- Send a datagram from IUT using the ZF zocket. */
    /*- Receive the datagram on tester and check its length and data. */
    zfts_zfut_check_send_func(pco_iut, stack, iut_utx, pco_tst, tst_s,
                              func, large_buffer, few_iov);

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_utx);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
