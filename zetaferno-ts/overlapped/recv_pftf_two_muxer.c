/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Overlapped tests
 */

/** @page overlapped-recv_pftf_two_muxer Packets reception on multiple zockets using packets from the future API
 *
 * @objective Check that PFTF API properly receives packets when multiple zockets and muxer sets are used.
 *
 * @param env           Network environment configuration:
 *                      - @ref arg_types_env_peer2peer.two_addrs
 * @param total_len     Length of packet to send from TST
 * @param part_len      Length of data to check overlapped reception
 * @param raddr_bind    Whether to use remote address to bind zockets:
 *                      - @c FALSE: Traffic can be accepted from all
 *                                 remote addresses
 *                      - @c TRUE: Accept traffic from only one remote adress
 * @param zocket_type   Zocket type
 * @par Scenario:
 *
 * @author Denis Pryazhennikov <Denis.Pryazhennikov@oktetlabs.ru>
 */

#define TE_TEST_NAME "overlapped/recv_pftf_two_muxer"

#include "zf_test.h"
#include "rpc_zf.h"

#include "overlapped_lib.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr1 = NULL;
    const struct sockaddr *iut_addr2 = NULL;
    const struct sockaddr *tst_addr1 = NULL;
    const struct sockaddr *tst_addr2 = NULL;
    zfts_zocket_type       zocket_type;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_ptr        iut_s1 = RPC_NULL;
    rpc_ptr        iut_s2 = RPC_NULL;
    int            tst_s1;
    int            tst_s2;

    rpc_zf_muxer_set_p     muxer_set1 = RPC_NULL;
    rpc_zf_muxer_set_p     muxer_set2 = RPC_NULL;
    rpc_zf_waitable_p      iut_waitable1 = RPC_NULL;
    rpc_zf_waitable_p      iut_waitable2 = RPC_NULL;

    int     part_len;
    int     total_len;
    te_bool raddr_bind;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr1);
    TEST_GET_ADDR(pco_iut, iut_addr2);
    TEST_GET_ADDR(pco_tst, tst_addr1);
    TEST_GET_ADDR(pco_tst, tst_addr2);
    TEST_GET_INT_PARAM(part_len);
    TEST_GET_INT_PARAM(total_len);
    TEST_GET_BOOL_PARAM(raddr_bind);
    TEST_GET_ENUM_PARAM(zocket_type, ZFTS_ZOCKET_TYPES);

    TEST_STEP("Allocate Zetaferno attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Create %s zocket on IUT. Create socket on Tester."
              "Establish a connection between them.",
              zfts_zocket_type2str(zocket_type));
    switch(zocket_type)
    {
        case ZFTS_ZOCKET_URX:
            zfts_create_zocket(zocket_type, pco_iut, attr, stack, iut_addr1,
                               pco_tst, raddr_bind ? tst_addr1 : NULL, &iut_s1, &iut_waitable1,
                               NULL);
            zfts_create_zocket(zocket_type, pco_iut, attr, stack, iut_addr2,
                               pco_tst, raddr_bind ? tst_addr2 : NULL, &iut_s2, &iut_waitable2,
                               NULL);

            tst_s1 = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr1),
                                RPC_SOCK_DGRAM, RPC_PROTO_DEF);
            rpc_bind(pco_tst, tst_s1, tst_addr1);
            rpc_connect(pco_tst, tst_s1, iut_addr1);

            tst_s2 = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr2),
                                RPC_SOCK_DGRAM, RPC_PROTO_DEF);
            rpc_bind(pco_tst, tst_s2, tst_addr2);
            rpc_connect(pco_tst, tst_s2, iut_addr2);
            break;

        case ZFTS_ZOCKET_ZFT_ACT:
        case ZFTS_ZOCKET_ZFT_PAS:
            zfts_create_zocket(zocket_type, pco_iut, attr, stack, iut_addr1,
                               pco_tst, tst_addr1, &iut_s1,
                               &iut_waitable1, &tst_s1);
            zfts_create_zocket(zocket_type, pco_iut, attr, stack, iut_addr2,
                               pco_tst, tst_addr2 , &iut_s2,
                               &iut_waitable2, &tst_s2);
            break;

        default:
            TEST_FAIL("Incorrect zocket type");

    }

    TEST_STEP("Allocate two muxer sets.");
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set1);
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set2);

    TEST_STEP("Add the frist zocket to the first muxer set and the "
              "second zocket to the second muxer set with "
              "@c ZF_EPOLLIN_OVERLAPPED | @c EPOLLIN events for both.");
    rpc_zf_muxer_add_simple(pco_iut, muxer_set1, iut_waitable1,
                            iut_s1,
                            RPC_ZF_EPOLLIN_OVERLAPPED | RPC_EPOLLIN);

    rpc_zf_muxer_add_simple(pco_iut, muxer_set2, iut_waitable2,
                            iut_s2,
                            RPC_ZF_EPOLLIN_OVERLAPPED | RPC_EPOLLIN);

    TEST_STEP("Receive pftf on the first zocket.");
    prepare_send_recv_pftf(pco_iut, pco_tst, muxer_set1, iut_s1, tst_s1,
                           total_len, part_len, zocket_type);

    TEST_STEP("Check that there are no events on the "
              "second muxer set.");
    zfts_muxer_wait_check_no_evts(pco_iut, muxer_set2, 0,
                                  "The first send to the first zocket");

    TEST_STEP("Receive pftf on the second zocket.");
    prepare_send_recv_pftf(pco_iut, pco_tst, muxer_set2, iut_s2, tst_s2,
                           total_len, part_len, zocket_type);

    TEST_STEP("Check that there are no events on the "
              "first muxer set.");
    zfts_muxer_wait_check_no_evts(pco_iut, muxer_set1, 0,
                                  "The second send to the second zocket");

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_CLOSE(pco_tst, tst_s1);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s2);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set1);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set2);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_waitable1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_waitable2);

    if (zocket_type == ZFTS_ZOCKET_URX)
    {
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s1);
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s2);

    }
    else
    {
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_s1);
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_s2);

    }

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

