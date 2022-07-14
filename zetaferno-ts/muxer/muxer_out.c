/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-muxer_out Process outgoing events using multiplexer
 *
 * @objective Add UDP TX and connected (actively and passively) TCP
 *            zockets to a muxer set, check that OUT event is returned
 *            for all zockets.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/muxer_out"

#include "zf_test.h"
#include "rpc_zf.h"

/* Length of packet used in the test. */
#define PKT_LEN 100

/* Timeout used for @b zf_muxer_wait() call. */
#define MUXER_TIMEOUT 500

int
main(int argc, char *argv[])
{

    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr1 = NULL;
    const struct sockaddr *iut_addr2 = NULL;
    const struct sockaddr *tst_addr1 = NULL;
    const struct sockaddr *tst_addr2 = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zfut_p     iut_zfut = RPC_NULL;
    rpc_zft_p      iut_zft_act = RPC_NULL;
    rpc_zft_p      iut_zft_pas = RPC_NULL;

    zfts_zock_descr zock_descrs[] = {
                        { &iut_zfut, "UDP TX zocket" },
                        { &iut_zft_act, "TCP actively connected zocket" },
                        { &iut_zft_pas, "TCP passively connected zocket" },
                        { NULL, "" },
                        };

    rpc_zf_waitable_p iut_zfut_waitable = RPC_NULL;
    rpc_zf_waitable_p iut_zft_act_waitable = RPC_NULL;
    rpc_zf_waitable_p iut_zft_pas_waitable = RPC_NULL;

    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;

    char send_data[PKT_LEN];
    char recv_data[PKT_LEN];

    int tst_udp_s = -1;
    int tst_tcp_s1 = -1;
    int tst_tcp_s2 = -1;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr1);
    TEST_GET_ADDR(pco_iut, iut_addr2);
    TEST_GET_ADDR(pco_tst, tst_addr1);
    TEST_GET_ADDR(pco_tst, tst_addr2);

    te_fill_buf(send_data, PKT_LEN);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate UDP TX zocket. */
    rpc_zfut_alloc(pco_iut, &iut_zfut, stack,
                   iut_addr1, tst_addr1, 0, attr);

    tst_udp_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr1),
                           RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_udp_s, tst_addr1);

    /*- Establish TCP connection to get actively connected ZFT zocket. */
    zfts_establish_tcp_conn(TRUE, pco_iut, attr, stack,
                            &iut_zft_act, iut_addr1,
                            pco_tst, &tst_tcp_s1, tst_addr1);

    /*- Establish TCP connection to get passively connected ZFT zocket. */
    zfts_establish_tcp_conn(FALSE, pco_iut, attr, stack,
                            &iut_zft_pas, iut_addr2,
                            pco_tst, &tst_tcp_s2, tst_addr2);

    /*- Allocate a multiplexer set. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);

    /*- Get pointers to @a zf_waitable handles and add them
     * to the muxer set. */
    iut_zfut_waitable = rpc_zfut_to_waitable(pco_iut, iut_zfut);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_zfut_waitable,
                            iut_zfut, RPC_EPOLLOUT);

    iut_zft_act_waitable = rpc_zft_to_waitable(pco_iut, iut_zft_act);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_zft_act_waitable,
                            iut_zft_act, RPC_EPOLLOUT);

    iut_zft_pas_waitable = rpc_zft_to_waitable(pco_iut, iut_zft_pas);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_zft_pas_waitable,
                            iut_zft_pas, RPC_EPOLLOUT);

    /*- Call @a zf_muxer_wait():
     *  - 3 events should be returned. */
    ZFTS_CHECK_MUXER_EVENTS(
                 pco_iut, muxer_set,
                 "Before sending data from zockets",
                 MUXER_TIMEOUT, zock_descrs,
                 { RPC_EPOLLOUT, { .u32 = iut_zfut } },
                 { RPC_EPOLLOUT, { .u32 = iut_zft_act } },
                 { RPC_EPOLLOUT, { .u32 = iut_zft_pas } });

    /*- Send some data using all zockets. */
    rc = rpc_zfut_send_single(pco_iut, iut_zfut, send_data, PKT_LEN);
    if (rc != PKT_LEN)
        TEST_FAIL("zfut_send_single() returned unexpected value %d",
                  rc);

    zfts_zft_send(pco_iut, iut_zft_act, send_data, PKT_LEN);
    zfts_zft_send(pco_iut, iut_zft_pas, send_data, PKT_LEN);

    rpc_zf_process_events(pco_iut, stack);

    /*- Read and check data on tester. */

    rc = rpc_recv(pco_tst, tst_udp_s, recv_data, PKT_LEN, 0);
    ZFTS_CHECK_RECEIVED_DATA(recv_data, send_data,
                             rc, PKT_LEN,
                             " from UDP socket");

    rc = rpc_recv(pco_tst, tst_tcp_s1, recv_data, PKT_LEN, 0);
    ZFTS_CHECK_RECEIVED_DATA(recv_data, send_data,
                             rc, PKT_LEN,
                             " from the first TCP socket");

    rc = rpc_recv(pco_tst, tst_tcp_s2, recv_data, PKT_LEN, 0);
    ZFTS_CHECK_RECEIVED_DATA(recv_data, send_data,
                             rc, PKT_LEN,
                             " from the second TCP socket");

    /*- Call @a zf_muxer_wait():
     *  - no events should be returned. */
    ZFTS_CHECK_MUXER_EVENTS(
                     pco_iut, muxer_set,
                     "After sending data from zockets",
                     MUXER_TIMEOUT, zock_descrs);

    TEST_SUCCESS;

cleanup:

    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);

    CLEANUP_RPC_CLOSE(pco_tst, tst_tcp_s1);
    CLEANUP_RPC_CLOSE(pco_tst, tst_tcp_s2);
    CLEANUP_RPC_CLOSE(pco_tst, tst_udp_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zfut_waitable);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zft_act_waitable);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zft_pas_waitable);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_zfut);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft_act);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft_pas);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

