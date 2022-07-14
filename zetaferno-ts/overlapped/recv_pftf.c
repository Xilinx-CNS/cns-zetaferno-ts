/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Overlapped tests
 */

/** @page overlapped-recv_pftf Packets reception using packets from the future API
 *
 * @objective  Read a packet using Zetaferno Direct API overlapped reception.
 *
 * @param env           Network environment configuration:
 *                      - @ref arg_types_env_peer2peer_mcast
 * @param total_len     Length of datagram to send from TST
 * @param part_len      Length of data to check overlapped reception
 * @param mcast_bind    Whether to use multicast address:
 *                      - @c FALSE: Use unicast address to bind
 *                      - @c TRUE: Use multicast address to bind
 * @param raddr_bind    Whether to use remote address to bind zocket:
 *                      - @c FALSE: Traffic can be accepted from all
 *                                  remote addresses
 *                      - @c TRUE: Accept traffic from only one remote adress
 * @param zocket_type   Zocket type
 * @par Scenario:
 *
 * @author Denis Pryazhennikov <Denis.Pryazhennikov@oktetlabs.ru>
 */

#define TE_TEST_NAME "overlapped/recv_pftf"

#include "zf_test.h"
#include "rpc_zf.h"

#include "overlapped_lib.h"


int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *mcast_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    zfts_zocket_type       zocket_type;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_ptr        iut_s = RPC_NULL;
    int            tst_s;

    rpc_zf_muxer_set_p     muxer_set = RPC_NULL;
    rpc_zf_waitable_p      iut_waitable = RPC_NULL;

    int     part_len;
    int     total_len;
    te_bool mcast_bind;
    te_bool raddr_bind;

    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(part_len);
    TEST_GET_INT_PARAM(total_len);
    TEST_GET_BOOL_PARAM(raddr_bind);
    TEST_GET_BOOL_PARAM(mcast_bind);
    if (mcast_bind)
        TEST_GET_ADDR(pco_iut, mcast_addr);
    TEST_GET_ENUM_PARAM(zocket_type, ZFTS_ZOCKET_TYPES);

    TEST_STEP("Allocate Zetaferno attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Allocate Zetaferno zocket and create socket on Tester"
              "according to @p zocket_type.");
    switch (zocket_type)
    {
        case ZFTS_ZOCKET_URX:
        {
            zfts_create_zocket(zocket_type, pco_iut, attr, stack,
                               mcast_bind ? SA(mcast_addr) : SA(iut_addr),
                               pco_tst, raddr_bind ? tst_addr : NULL, &iut_s,
                               &iut_waitable, NULL);

            tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                               RPC_SOCK_DGRAM, RPC_PROTO_DEF);
            rpc_bind(pco_tst, tst_s, tst_addr);
            rpc_connect(pco_tst, tst_s, mcast_bind ? mcast_addr : iut_addr);

            break;
        }

        case ZFTS_ZOCKET_ZFT_ACT:
        case ZFTS_ZOCKET_ZFT_PAS:
        {
            zfts_create_zocket(zocket_type, pco_iut, attr, stack, iut_addr,
                               pco_tst,  tst_addr, &iut_s, &iut_waitable,
                               &tst_s);

            break;
        }

        default:
            TEST_FAIL("Unsupported zocket type");
    }

    TEST_STEP("Allocate a muxer set and add the zocket to it with "
              "@c ZF_EPOLLIN_OVERLAPPED event.");
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_waitable,
                            iut_s, RPC_ZF_EPOLLIN_OVERLAPPED | RPC_EPOLLIN);

    TEST_STEP("Repeat in the loop @c ZFTS_IOVCNT times:"
              "Send packet with @p total len size from Tester"
              "Ensure arrival of @p part_len bytes of data");
    for (i = 0; i < ZFTS_IOVCNT; i++)
    {
       prepare_send_recv_pftf(pco_iut, pco_tst, muxer_set, iut_s, tst_s,
                              total_len, part_len, zocket_type);
    }

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_waitable);
    if (zocket_type == ZFTS_ZOCKET_URX)
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s);
    else
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_s);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);


    TEST_END;
}
