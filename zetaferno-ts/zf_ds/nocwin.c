/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Delegated Sends tests
 */

/**
 * @page zf_ds-nocwin Calling zf_delegated_send_prepare() with insufficient congestion window
 *
 * @objective Check that @b zf_delegated_send_prepare() fails when
 *            TCP congestion window is not large enough for requested
 *            data length (unless @b cong_wnd_override is used).
 *
 * @param env                 Testing environment:
 *                            - @ref arg_types_peer2peer_gw
 * @param ds_send             If @c TRUE, actually send data reserved
 *                            for delegated sends over a RAW socket;
 *                            otherwise let ZF transmit it after
 *                            completion.
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "zf_ds/nocwin"

#include "zf_test.h"
#include "rpc_zf.h"

#include "zf_ds_test.h"

#include "tapi_route_gw.h"

/** Minimum number of bytes to send via delegated sends */
#define MIN_DS_LEN 5000
/** Maximum number of bytes to send via delegated sends */
#define MAX_DS_LEN 10000
/** How long to wait until congestion window reduction, in ms */
#define WAIT_CWIN_REDUCED 2000
/**
 * How long to wait for transmit finalizing at the end, after
 * repairing network connectivity, in ms.
 */
#define WAIT_TRANSMIT_FINISH 2000

int
main(int argc, char *argv[])
{
    TAPI_DECLARE_ROUTE_GATEWAY_PARAMS;

    tapi_route_gateway gw;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    int tst_s = -1;
    int raw_socket = -1;

    struct zf_ds ds;
    char headers[ZFTS_TCP_HDRS_MAX];

    char send_buf[ZFTS_TCP_DATA_MAX];
    int send_len;
    char ds_buf[MAX_DS_LEN];
    int ds_len;

    te_dbuf sent_data = TE_DBUF_INIT(0);
    te_dbuf recv_data = TE_DBUF_INIT(0);

    te_bool ds_send;

    TEST_START;
    TAPI_GET_ROUTE_GATEWAY_PARAMS;
    TEST_GET_BOOL_PARAM(ds_send);

    TEST_STEP("Configure routing between IUT and Tester via a "
              "gateway host.");
    TAPI_INIT_ROUTE_GATEWAY(gw);
    CHECK_RC(tapi_route_gateway_configure(&gw));
    CFG_WAIT_CHANGES;

    TEST_STEP("Allocate ZF attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Establish TCP connection between a zocket on IUT and "
              "a socket on Tester.");
    zfts_establish_tcp_conn(TRUE, pco_iut, attr, stack, &iut_zft,
                            iut_addr, pco_tst, &tst_s, tst_addr);

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, create RAW socket on IUT to "
                  "send data from it.");
        raw_socket = rpc_socket(pco_iut, RPC_AF_PACKET, RPC_SOCK_RAW,
                                RPC_IPPROTO_RAW);
    }

    TEST_STEP("Break connection between the gateway and Tester.");
    CHECK_RC(tapi_route_gateway_break_gw_tst(&gw));
    CFG_WAIT_CHANGES;

    TEST_STEP("Call zft_send_single() on IUT to send some data and "
              "call @b zf_reactor_perform() in a loop for a while. "
              "It should result in reducing TCP congestion window on "
              "IUT.");

    send_len = rand_range(1, ZFTS_TCP_DATA_MAX);
    te_fill_buf(send_buf, send_len);
    rc = rpc_zft_send_single(pco_iut, iut_zft, send_buf,
                             send_len, 0);
    CHECK_RC(te_dbuf_append(&sent_data, send_buf, rc));

    rpc_zf_process_events_long(pco_iut, stack, WAIT_CWIN_REDUCED);

    TEST_STEP("Prepare some data for delegated sends.");
    ds_len = rand_range(MIN_DS_LEN, MAX_DS_LEN);
    te_fill_buf(ds_buf, ds_len);

    memset(&ds, 0, sizeof(ds));
    ds.headers = headers;
    ds.headers_size = sizeof(headers);

    TEST_STEP("Call @b zf_delegated_send_prepare() asking it for "
              "a big data size. Check that it fails returning "
              "@c ZF_DELEGATED_SEND_RC_NOCWIN.");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, ds_len,
                                       0, 0, &ds);
    zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_NOCWIN, 0,
                          rc, &ds, "The first call of ");

    TEST_STEP("Call @b zf_delegated_send_prepare() the second time, "
              "now using @b cong_wnd_override parameter to override "
              "TCP congestion window. Check that it succeeds now.");

    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, ds_len,
                                       ds_len, 0, &ds);
    zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_OK, ds_len,
                          rc, &ds, "The second call of ");

    TEST_STEP("Repair network connection between the gateway and Tester.");
    CHECK_RC(tapi_route_gateway_repair_gw_tst(&gw));
    CFG_WAIT_CHANGES;

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, send data prepared for "
                  "delegated sends via the RAW socket from IUT.");
    }
    TEST_STEP("Call @b zf_delegated_send_complete(), passing to it the "
              "data prepared for delegated sends before.");

    zfts_ds_raw_send_complete(pco_iut, iut_if->if_index, raw_socket,
                              iut_zft, &ds, ds_buf, ds_len,
                              ZFTS_TCP_DATA_MAX / 2,
                              ZFTS_TCP_DATA_MAX);

    CHECK_RC(te_dbuf_append(&sent_data, ds_buf, ds_len));

    TEST_STEP("Call @b zf_reactor_perform() in a loop for a while on IUT.");
    rpc_zf_process_events_long(pco_iut, stack, WAIT_TRANSMIT_FINISH);

    TEST_STEP("Read all the data on Tester, check that it matches data "
              "sent from IUT.");
    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s, &recv_data);
    ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, sent_data.ptr,
                             recv_data.len, sent_data.len);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_iut, raw_socket);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    te_dbuf_free(&sent_data);
    te_dbuf_free(&recv_data);

    TEST_END;
}
