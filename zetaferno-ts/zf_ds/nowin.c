/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Delegated Sends tests
 */

/**
 * @page zf_ds-nowin Calling zf_delegated_send_prepare() with insufficient send window
 *
 * @objective Check behaviour of @b zf_delegated_send_prepare() when it is
 *            called on a zocket with insufficient TCP send window.
 *
 * @param env                 Testing environment:
 *                            - @ref arg_types_env_peer2peer
 * @param send_type           How to send data when overfilling
 *                            send window:
 *                            - @c normal (@b zft_send_single())
 *                            - @c delegated (via RAW socket before
 *                              @b zf_delegated_send_complete())
 *                            - @c retransmit (via retransmit after
 *                              @b zf_delegated_send_complete())
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "zf_ds/nowin"

#include "zf_test.h"
#include "rpc_zf.h"

#include "zf_ds_test.h"

/** Value to set for SO_RCVBUF on Tester */
#define TST_RCVBUF 2048
/** Number of bytes to ask from zf_delegated_send_prepare() */
#define DS_DATA_LEN 50000

enum {
    SEND_TYPE_NORMAL,
    SEND_TYPE_DELEGATED,
    SEND_TYPE_RETRANSMIT,
};

#define SEND_TYPES \
    { "normal", SEND_TYPE_NORMAL },         \
    { "delegated", SEND_TYPE_DELEGATED },   \
    { "retransmit", SEND_TYPE_RETRANSMIT }

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    const struct if_nameindex *iut_if = NULL;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    int tst_s = -1;
    int raw_socket = -1;

    struct zf_ds ds;
    char headers[ZFTS_TCP_HDRS_MAX];
    char send_buf[DS_DATA_LEN];
    int send_len;
    int send_type;

    te_dbuf recv_data = TE_DBUF_INIT(0);

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(iut_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(send_type, SEND_TYPES);

    TEST_STEP("Allocate ZF attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Establish TCP connection between a zocket on IUT and "
              "a socket on Tester. Set small receive buffer size on "
              "the Tester socket.");
    zfts_establish_tcp_conn_ext2(ZFTS_CONN_OPEN_ACT, pco_iut, attr, stack,
                                 NULL, &iut_zft, iut_addr, pco_tst, &tst_s,
                                 tst_addr, -1, TST_RCVBUF);

    if (send_type == SEND_TYPE_DELEGATED)
    {
        TEST_STEP("If @p send_type is @c delegated, create RAW socket on "
                  "IUT to send data from it.");
        raw_socket = rpc_socket(pco_iut, RPC_AF_PACKET, RPC_SOCK_RAW,
                                RPC_IPPROTO_RAW);
    }

    memset(&ds, 0, sizeof(ds));
    ds.headers = headers;
    ds.headers_size = sizeof(headers);

    TEST_STEP("Call @b zf_delegated_send_prepare() the first time on IUT, "
              "asking to reserve a lot of bytes for delegated sends. "
              "Check that it succeeds but retrieves a smaller value "
              "in @b delegated_wnd field equal to reported @b send_wnd.");
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, DS_DATA_LEN,
                                       0, 0, &ds);

    if (rc != ZF_DELEGATED_SEND_RC_OK)
    {
        TEST_VERDICT("The first call of zf_delegated_send_prepare() "
                     "returned %s instead of OK",
                     zf_delegated_send_rc_rpc2str(rc));
    }

    if (ds.send_wnd >= DS_DATA_LEN)
    {
        TEST_VERDICT("Send window is reported to be too big");
    }
    else if (ds.delegated_wnd != ds.send_wnd)
    {
        TEST_VERDICT("zf_delegated_send_prepare() reserved %s bytes than "
                     "send window allows",
                     (ds.delegated_wnd < ds.send_wnd ? "less" : "more"));
    }

    send_len = ds.delegated_wnd;
    te_fill_buf(send_buf, send_len);

    if (send_type == SEND_TYPE_NORMAL)
    {
        TEST_STEP("If @p send_type is @c normal, call on IUT "
                  "@b zf_delegated_send_cancel() and then send "
                  "previously reserved bytes via the usual "
                  "@b zft_send_single() call to fill the send window.");

        rpc_zf_delegated_send_cancel(pco_iut, iut_zft);

        rc = rpc_zft_send_single(pco_iut, iut_zft, send_buf, send_len, 0);
        if (rc != send_len)
            TEST_VERDICT("zft_send_single() returned unexpected value");
    }
    else
    {
        TEST_STEP("If @p send_type is @c delegated, send all "
                  "the reserved bytes via the RAW socket from IUT.");
        TEST_STEP("If @p send_type is @c delegated or @c retransmit, call "
                  "@b zf_delegated_send_complete() on IUT.");

        zfts_ds_raw_send_complete(pco_iut, iut_if->if_index, raw_socket,
                                  iut_zft, &ds, send_buf, send_len,
                                  ZFTS_TCP_DATA_MAX / 2,
                                  ZFTS_TCP_DATA_MAX);
    }

    TEST_STEP("Call @b zf_delegated_send_prepare() the second time. "
              "Check that now it fails returning "
              "@c ZF_DELEGATED_SEND_RC_NOWIN and reports @b send_wnd "
              "to be zero.");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, DS_DATA_LEN,
                                       0, 0, &ds);
    zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_NOWIN, 0,
                          rc, &ds, "The second call of ");
    if (ds.send_wnd != 0)
    {
        TEST_VERDICT("The second call of zf_delegated_send_prepare() "
                     "returned NOWIN but non-zero send_wnd");
    }

    TEST_STEP("Receive and check all sent data on Tester.");
    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s, &recv_data);
    ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, send_buf,
                             recv_data.len, send_len);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_iut, raw_socket);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    te_dbuf_free(&recv_data);

    TEST_END;
}
