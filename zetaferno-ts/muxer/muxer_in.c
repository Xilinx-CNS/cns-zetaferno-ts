/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-muxer_in Process incoming events using multiplexer
 *
 * @objective Add UDP RX, TCP listener and connected (actively and
 *            passively) TCP zockets to a muxer set, invoke IN event
 *            for all zockets simultaneously or sequentially. Check
 *            that muxer returns all incoming events.
 *
 * @param sequential          If @c TRUE invoke events for all zockets
 *                            sequentially, else - simultaneously.
 * @param accepted_from_set   If @c TRUE, passively opened TCP zocket
 *                            should be accepted from listener which
 *                            is in muxer set.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/muxer_in"

#include "zf_test.h"
#include "rpc_zf.h"

/** Length of packet used in the test. */
#define PKT_LEN 100

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr_storage iut_addr1;
    struct sockaddr_storage iut_addr2;
    struct sockaddr_storage iut_addr3;
    struct sockaddr_storage tst_addr1;
    struct sockaddr_storage tst_addr2;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zfur_p     iut_zfur1 = RPC_NULL;
    rpc_zfur_p     iut_zfur2 = RPC_NULL;
    rpc_zftl_p     iut_zftl = RPC_NULL;
    rpc_zft_p      iut_zft_act = RPC_NULL;
    rpc_zft_p      iut_zft_pas = RPC_NULL;
    rpc_zft_p      iut_zft_accepted = RPC_NULL;

    zfts_zock_descr zock_descrs[] = {
                        { &iut_zfur1, "the first UDP RX zocket" },
                        { &iut_zfur2, "the second UDP RX zocket" },
                        { &iut_zftl, "listening TCP zocket" },
                        { &iut_zft_act, "active TCP zocket" },
                        { &iut_zft_pas, "passive TCP zocket" },
                        { NULL, "" }
                        };

    rpc_zf_waitable_p iut_zfur1_waitable = RPC_NULL;
    rpc_zf_waitable_p iut_zfur2_waitable = RPC_NULL;
    rpc_zf_waitable_p iut_zftl_waitable = RPC_NULL;
    rpc_zf_waitable_p iut_zft_act_waitable = RPC_NULL;
    rpc_zf_waitable_p iut_zft_pas_waitable = RPC_NULL;

    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;

    int tst_udp_s = -1;
    int tst_tcp_s1 = -1;
    int tst_tcp_s2 = -1;
    int tst_tcp_s_conn = -1;

    char send_data[PKT_LEN];
    char recv_data[PKT_LEN];

    te_bool sequential = FALSE;
    te_bool accepted_from_set = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(sequential);
    TEST_GET_BOOL_PARAM(accepted_from_set);

    te_fill_buf(send_data, PKT_LEN);

    tapi_sockaddr_clone_exact(iut_addr, &iut_addr1);
    tapi_sockaddr_clone_exact(tst_addr, &tst_addr1);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate the first UDP RX zocket and bind it
     * to both local and remote addresses. */
    rpc_zfur_alloc(pco_iut, &iut_zfur1, stack, attr);
    rpc_zfur_addr_bind(pco_iut, iut_zfur1, SA(&iut_addr1),
                       SA(&tst_addr1), 0);

    /*- Allocate the second UDP RX zocket and bind it
     * only to a local address. */
    CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr, &iut_addr2));
    rpc_zfur_alloc(pco_iut, &iut_zfur2, stack, attr);
    rpc_zfur_addr_bind(pco_iut, iut_zfur2, SA(&iut_addr2),
                       NULL, 0);

    /*- Obtain actively opened TCP zocket. */
    zfts_establish_tcp_conn(TRUE, pco_iut, attr, stack,
                            &iut_zft_act, SA(&iut_addr1),
                            pco_tst, &tst_tcp_s1, SA(&tst_addr1));

    /*- Allocate TCP listener zocket. */
    rpc_zftl_listen(pco_iut, stack, SA(&iut_addr2),
                    attr, &iut_zftl);

    /*- Obtain passively opened TCP zocket, using previously allocated
     * TCP listener zocket if @p accepted_from_set is @c TRUE. */

    CHECK_RC(tapi_sockaddr_clone(pco_tst, tst_addr, &tst_addr2));

    if (accepted_from_set)
    {
        zfts_establish_tcp_conn_ext(FALSE, pco_iut, attr, stack,
                                    iut_zftl, &iut_zft_pas, SA(&iut_addr2),
                                    pco_tst, &tst_tcp_s2, SA(&tst_addr2),
                                    -1, -1);
    }
    else
    {
        CHECK_RC(tapi_sockaddr_clone(pco_tst, iut_addr, &iut_addr3));
        zfts_establish_tcp_conn(FALSE, pco_iut, attr, stack,
                                &iut_zft_pas, SA(&iut_addr3),
                                pco_tst, &tst_tcp_s2, SA(&tst_addr2));
    }

    /*- Allocate a multiplexer set. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);

    /*- Get pointers to @a zf_waitable handles and add them to
     * the muxer set. */

    iut_zfur1_waitable = rpc_zfur_to_waitable(pco_iut, iut_zfur1);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_zfur1_waitable,
                            iut_zfur1, RPC_EPOLLIN);

    iut_zfur2_waitable = rpc_zfur_to_waitable(pco_iut, iut_zfur2);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_zfur2_waitable,
                            iut_zfur2, RPC_EPOLLIN);

    iut_zft_act_waitable = rpc_zft_to_waitable(pco_iut, iut_zft_act);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_zft_act_waitable,
                            iut_zft_act, RPC_EPOLLIN);

    iut_zft_pas_waitable = rpc_zft_to_waitable(pco_iut, iut_zft_pas);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_zft_pas_waitable,
                            iut_zft_pas, RPC_EPOLLIN);

    iut_zftl_waitable = rpc_zftl_to_waitable(pco_iut, iut_zftl);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_zftl_waitable,
                            iut_zftl, RPC_EPOLLIN);

    /*- If @p sequential is @c FALSE
     *  - invoke IN actions for all zockets
     *    - send a datagram to UDP RX zockets;
     *    - connect to the listener zocket;
     *    - send a data packet to the connected TCP zockets;
     *  - call @a zf_muxer_wait()
     *    - 5 events should be returned.
     * Else
     *  - send a datagram to the first UDP RX zocket;
     *  - call @a zf_muxer_wait()
     *    - 1 event should be returned;
     *  - send a datagram to the second UDP RX zocket;
     *  - call @a zf_muxer_wait()
     *    - 1 event should be returned;
     *  - connect to the listener zocket;
     *  - call @a zf_muxer_wait()
     *    - 1 event should be returned;
     *  - send a data packet to the actively opened TCP zocket;
     *  - call @a zf_muxer_wait()
     *    - 1 event should be returned.
     *  - send a data packet to the passively opened TCP zocket;
     *  - call @a zf_muxer_wait()
     *    - 1 event should be returned. */

    tst_udp_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                           RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_udp_s, SA(&tst_addr1));

    rc = rpc_sendto(pco_tst, tst_udp_s, send_data, PKT_LEN,
                    0, SA(&iut_addr1));
    if (rc != PKT_LEN)
        TEST_FAIL("sendto() returned unexpected value %d", rc);

    if (sequential)
        ZFTS_CHECK_MUXER_EVENTS(
                     pco_iut, muxer_set,
                     "After sending data to the first UDP RX zocket",
                     -1, zock_descrs,
                     { RPC_EPOLLIN, { .u32 = iut_zfur1 } });

    rc = rpc_sendto(pco_tst, tst_udp_s, send_data, PKT_LEN,
                    0, SA(&iut_addr2));
    if (rc != PKT_LEN)
        TEST_FAIL("sendto() returned unexpected value %d", rc);

    if (sequential)
        ZFTS_CHECK_MUXER_EVENTS(
                     pco_iut, muxer_set,
                     "After sending data to the second UDP RX zocket",
                     -1, zock_descrs,
                     { RPC_EPOLLIN, { .u32 = iut_zfur2 } });

    tst_tcp_s_conn = rpc_socket(pco_tst,
                                rpc_socket_domain_by_addr(tst_addr),
                                RPC_SOCK_STREAM, RPC_PROTO_DEF);
    pco_tst->op = RCF_RPC_CALL;
    rpc_connect(pco_tst, tst_tcp_s_conn, SA(&iut_addr2));
    ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);
    pco_tst->op = RCF_RPC_WAIT;
    rpc_connect(pco_tst, tst_tcp_s_conn, SA(&iut_addr2));

    if (sequential)
        ZFTS_CHECK_MUXER_EVENTS(
                     pco_iut, muxer_set,
                     "After connecting to listening zocket",
                     -1, zock_descrs,
                     { RPC_EPOLLIN, { .u32 = iut_zftl } });

    rc = rpc_send(pco_tst, tst_tcp_s1,
                  send_data, PKT_LEN, 0);
    if (rc != PKT_LEN)
        TEST_FAIL("send() returned unexpected value %d", rc);

    if (sequential)
        ZFTS_CHECK_MUXER_EVENTS(
                     pco_iut, muxer_set,
                     "After sending data to actively opened TCP zocket",
                     -1, zock_descrs,
                     { RPC_EPOLLIN, { .u32 = iut_zft_act } });

    rc = rpc_send(pco_tst, tst_tcp_s2,
                  send_data, PKT_LEN, 0);
    if (rc != PKT_LEN)
        TEST_FAIL("send() returned unexpected value %d", rc);

    if (sequential)
        ZFTS_CHECK_MUXER_EVENTS(
                     pco_iut, muxer_set,
                     "After sending data to passively opened TCP zocket",
                     -1, zock_descrs,
                     { RPC_EPOLLIN, { .u32 = iut_zft_pas } });
    else
        ZFTS_CHECK_MUXER_EVENTS(
                     pco_iut, muxer_set,
                     "After generating EPOLLIN events on all zockets",
                     -1, zock_descrs,
                     { RPC_EPOLLIN, { .u32 = iut_zfur1 } },
                     { RPC_EPOLLIN, { .u32 = iut_zfur2 } },
                     { RPC_EPOLLIN, { .u32 = iut_zft_act } },
                     { RPC_EPOLLIN, { .u32 = iut_zft_pas } },
                     { RPC_EPOLLIN, { .u32 = iut_zftl } });

    /*- Accept TCP connection on the listener zocket. */
    rpc_zftl_accept(pco_iut, iut_zftl, &iut_zft_accepted);

    /*- Read and check data on UDP RX and connected TCP zockets. */

    rc = zfts_zfur_recv(pco_iut, iut_zfur1, recv_data, PKT_LEN);
    ZFTS_CHECK_RECEIVED_DATA(recv_data, send_data,
                             rc, PKT_LEN,
                             " from the first UDP RX zocket");

    rc = zfts_zfur_recv(pco_iut, iut_zfur2, recv_data, PKT_LEN);
    ZFTS_CHECK_RECEIVED_DATA(recv_data, send_data,
                             rc, PKT_LEN,
                             " from the second UDP RX zocket");

    rc = zfts_zft_recv(pco_iut, iut_zft_act, recv_data, PKT_LEN);
    ZFTS_CHECK_RECEIVED_DATA(recv_data, send_data,
                             rc, PKT_LEN,
                             " from the actively opened TCP zocket");

    rc = zfts_zft_recv(pco_iut, iut_zft_pas, recv_data, PKT_LEN);
    ZFTS_CHECK_RECEIVED_DATA(recv_data, send_data,
                             rc, PKT_LEN,
                             " from the passively opened TCP zocket");

    TEST_SUCCESS;

cleanup:

    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);

    CLEANUP_RPC_CLOSE(pco_tst, tst_tcp_s1);
    CLEANUP_RPC_CLOSE(pco_tst, tst_tcp_s2);
    CLEANUP_RPC_CLOSE(pco_tst, tst_tcp_s_conn);
    CLEANUP_RPC_CLOSE(pco_tst, tst_udp_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zfur1_waitable);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zfur2_waitable);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zft_act_waitable);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zft_pas_waitable);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zftl_waitable);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_zfur1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_zfur2);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft_act);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft_pas);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft_accepted);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
