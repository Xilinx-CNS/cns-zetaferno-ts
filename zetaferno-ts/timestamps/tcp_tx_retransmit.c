/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Timestamps tests
 */

/**
 * @page timestamps-tcp_tx_retransmit Obtaining TX timestamps for retransmitted TCP packets
 *
 * @objective Check that when TX timestamps are configured, they can
 *            be obtained for retransmitted TCP packets, and
 *            @c ZF_PKT_REPORT_TCP_RETRANS flag is set in corresponding
 *            packet reports.
 *
 * @param pco_iut                   PCO on IUT.
 * @param pco_tst                   PCO on Tester.
 * @param iut_addr                  Network address on IUT.
 * @param tst_addr                  Network address on Tester.
 * @param open_method               How to open connection:
 *                                  - @c active
 *                                  - @c passive
 *                                  - @c passive_close (close listener after
 *                                    passively establishing connection)
 * @param close_tst_first           If @c TRUE, close TCP connection from
 *                                  Tester firstly, otherwise - from IUT.
 * @param retransmit_type           Type of the packet which is
 *                                  retransmitted:
 *                                  - @c SYN
 *                                  - @c FIN
 *                                  - @c DATA
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "timestamps/tcp_tx_retransmit"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_tad.h"
#include "tapi_tcp.h"
#include "te_vector.h"
#include "timestamps.h"
#include "tapi_route_gw.h"

/**
 * How long to wait after allocating ZF stack, if requested (in ms).
 */
#define WAIT_AFTER_STACK_ALLOC 2000

/**
 * How long to wait for retransmits, in ms.
 */
#define RETRANSMIT_TIME 2000

/**
 * How long to wait after repairing broken connection
 * (to be sure that retransmitted packet is finally
 * acknowledged), in ms.
 */
#define WAIT_AFTER_CONN_REPAIR (RETRANSMIT_TIME * 1.5)

/**
 * Maximum number of packet reports retrieved by a single
 * zft_get_tx_timestamps() call.
 */
#define MAX_REPORTS 10

/** Kinds of packets which may be retransmitted */
enum {
    RETRANSMIT_SYN,   /**< SYN packet */
    RETRANSMIT_FIN,   /**< FIN packet */
    RETRANSMIT_DATA,  /**< Data packet */
};

/** List of values of "retransmit_type" parameter */
#define RETRANSMIT_TYPES \
    { "SYN", RETRANSMIT_SYN },    \
    { "FIN", RETRANSMIT_FIN },    \
    { "DATA", RETRANSMIT_DATA }

/**
 * Wait for a while for retransmits to be made, then repair
 * connection and wait again to be sure that the retransmitted
 * packet is acknowledged.
 *
 * @param rpcs        RPC server on IUT.
 * @param gateway     Structure describing gateway configuration.
 * @param stack       ZF stack RPC pointer.
 */
static void
wait_retransmit_repair_conn(rcf_rpc_server *rpcs,
                            tapi_route_gateway *gateway,
                            rpc_zf_stack_p stack)
{
    rpc_zf_process_events_long(rpcs, stack, RETRANSMIT_TIME);
    CHECK_RC(tapi_route_gateway_repair_gw_tst(gateway));
    rpcs->timeout = rpcs->def_timeout + WAIT_AFTER_CONN_REPAIR;
    rpc_zf_process_events_long(rpcs, stack, WAIT_AFTER_CONN_REPAIR);
}

int
main(int argc, char *argv[])
{
    TAPI_DECLARE_ROUTE_GATEWAY_PARAMS;
    tapi_route_gateway gateway;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zft_handle_p iut_zft_handle = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    rpc_zftl_p iut_zftl = RPC_NULL;
    int tst_s = -1;
    int tst_s_listener = -1;

    rpc_zf_muxer_set_p muxer_set = RPC_NULL;
    rpc_zf_waitable_p waitable = RPC_NULL;

    zfts_conn_open_method open_method;
    te_bool close_tst_first;
    int retransmit_type;

    char send_buf[ZFTS_TCP_DATA_MAX];
    char recv_buf[ZFTS_TCP_DATA_MAX];
    size_t send_len;
    size_t total_sent = 0;

    csap_handle_t csap = CSAP_INVALID_HANDLE;
    tapi_tad_trrecv_cb_data cb_data;
    ts_tcp_tx_csap_data user_data;
    unsigned int pkts_num = 0;
    unsigned int reports_num = 0;

    te_vec all_reports = TE_VEC_INIT(tarpc_zf_pkt_report);

    TEST_START;
    TAPI_GET_ROUTE_GATEWAY_PARAMS;
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_BOOL_PARAM(close_tst_first);
    TEST_GET_ENUM_PARAM(retransmit_type, RETRANSMIT_TYPES);

    TEST_STEP("Configure connection between IUT and Tester over "
              "gateway.");
    TAPI_INIT_ROUTE_GATEWAY(gateway);
    CHECK_RC(tapi_route_gateway_configure(&gateway));
    CFG_WAIT_CHANGES;

    /* Do not print out packets captured by CSAP. */
    rcf_tr_op_log(FALSE);

    TEST_STEP("Create a CSAP on gateway host to capture packets sent "
              "from IUT.");
    CHECK_RC(tapi_tcp_ip4_eth_csap_create(pco_gw->ta, 0,
                                          gw_iut_if->if_name,
                                          TAD_ETH_RECV_DEF |
                                          TAD_ETH_RECV_NO_PROMISC,
                                          NULL, NULL,
                                          SIN(tst_addr)->sin_addr.s_addr,
                                          SIN(iut_addr)->sin_addr.s_addr,
                                          SIN(tst_addr)->sin_port,
                                          SIN(iut_addr)->sin_port,
                                          &csap));

    CHECK_RC(tapi_tad_trrecv_start(pco_gw->ta, 0, csap,
                                   NULL, TAD_TIMEOUT_INF, 0,
                                   RCF_TRRECV_PACKETS));
    SLEEP(2);

    TEST_STEP("Allocate ZF attributes and stack, enabling "
              "@b tx_timestamping.");
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_attr_set_int(pco_iut, attr, "tx_timestamping", 1);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    if (retransmit_type == RETRANSMIT_SYN)
    {
        TEST_STEP("If @p retransmit_type is @c RETRANSMIT_SYN, break "
                  "connection between gateway and Tester.");
        CHECK_RC(tapi_route_gateway_break_gw_tst(&gateway));
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    TEST_STEP("Create a TCP socket on Tester and a TCP zocket on "
              "IUT; initiate TCP connection from IUT or Tester "
              "according to @p open_method.");

    tst_s = rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                       RPC_PROTO_DEF, FALSE, FALSE,
                                       tst_addr);

    if (open_method == ZFTS_CONN_OPEN_ACT)
    {
        tst_s_listener = tst_s;
        tst_s = -1;

        rpc_listen(pco_tst, tst_s_listener, -1);

        rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);
        rpc_zft_addr_bind(pco_iut, iut_zft_handle, iut_addr, 0);
        rpc_zft_connect(pco_iut, iut_zft_handle, tst_addr, &iut_zft);
        iut_zft_handle = RPC_NULL;
    }
    else
    {
        rpc_zftl_listen(pco_iut, stack, iut_addr,
                        attr, &iut_zftl);

        pco_tst->op = RCF_RPC_CALL;
        pco_tst->timeout = pco_tst->def_timeout + RETRANSMIT_TIME +
                           WAIT_AFTER_CONN_REPAIR;
        rpc_connect(pco_tst, tst_s, iut_addr);
    }

    if (retransmit_type == RETRANSMIT_SYN)
    {
        TEST_STEP("If @p retransmit_type is @c RETRANSMIT_SYN, wait for "
                  "a while, then repair broken connection and wait again "
                  "so that TCP connection establishment can finish "
                  "successfully. Call @b zf_reactor_preform() during "
                  "waiting.");
        wait_retransmit_repair_conn(pco_iut, &gateway, stack);
    }
    else
    {
        TEST_STEP("If @p retransmit_type is not @c RETRANSMIT_SYN, "
                  "just wait for a while calling "
                  "@b zf_reactor_perform().");
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    TEST_STEP("Obtain accepted socket on Tester or accepted "
              "zocket on IUT according to @p open_method.");
    if (open_method == ZFTS_CONN_OPEN_ACT)
    {
        tst_s = rpc_accept(pco_tst, tst_s_listener, NULL, NULL);
    }
    else
    {
        rpc_connect(pco_tst, tst_s, iut_addr);
        rpc_zftl_accept(pco_iut, iut_zftl, &iut_zft);
    }

    if (open_method == ZFTS_CONN_OPEN_PAS_CLOSE)
        ZFTS_FREE(pco_iut, zftl, iut_zftl);

    TEST_STEP("Allocate ZF muxer to wait for @c EPOLLERR on the IUT "
              "zocket.");
    ts_tx_muxer_configure(pco_iut, stack, iut_zft, &muxer_set, &waitable);

    TEST_STEP("Check that @c EPOLLERR is reported now because "
              "TX timestamp is available for SYN packet.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft, 0, RPC_EPOLLERR,
                      "After connection establishment");

    TEST_STEP("Send initial data packet from IUT, receive and check "
              "it on Tester.");
    total_sent = zfts_zft_check_sending(pco_iut, stack, iut_zft,
                                        pco_tst, tst_s);

    TEST_STEP("Check that after sending a packet @c EPOLLERR is "
              "reported by the muxer.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft,
                      0, RPC_EPOLLERR,
                      "After sending the first packet");

    if (retransmit_type == RETRANSMIT_DATA)
    {
        TEST_STEP("if @p retransmit_type is @c RETRANSMIT_DATA, break "
                  "connection between gateway and Tester.");
        CHECK_RC(tapi_route_gateway_break_gw_tst(&gateway));
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    TEST_STEP("Send some data from the IUT zocket.");

    send_len = rand_range(1, sizeof(send_buf));
    te_fill_buf(send_buf, send_len);
    rpc_zft_send_single(pco_iut, iut_zft, send_buf, send_len, 0);

    TEST_STEP("Check that after sending a packet @c EPOLLERR is "
              "reported by the muxer.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft,
                      0, RPC_EPOLLERR,
                      "After sending the second packet");

    if (retransmit_type == RETRANSMIT_DATA)
    {
        TEST_STEP("If @p retransmit_type is @c RETRANSMIT_DATA, wait for "
                  "a while, then repair broken connection and wait again "
                  "so that TCP data packet can reach Tester successfully. "
                  "Call @b zf_reactor_perform() during wating.");
        wait_retransmit_repair_conn(pco_iut, &gateway, stack);
    }
    else
    {
        TEST_STEP("If @p retransmit_type is not @c RETRANSMIT_DATA, "
                  "just wait for a while calling "
                  "@b zf_reactor_perform().");
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    TEST_STEP("Receive data on Tester, check it.");
    rc = rpc_recv(pco_tst, tst_s, recv_buf, sizeof(recv_buf), 0);
    if (rc != (int)send_len || memcmp(send_buf, recv_buf, send_len) != 0)
    {
        TEST_VERDICT("Data received on Tester does not match data "
                     "sent from IUT");
    }
    total_sent += send_len;

    TEST_STEP("Send final data packet from IUT, receive and check "
              "it on Tester.");
    total_sent += zfts_zft_check_sending(pco_iut, stack, iut_zft,
                                         pco_tst, tst_s);

    TEST_STEP("Check that after sending a packet @c EPOLLERR is "
              "reported by the muxer.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft,
                      0, RPC_EPOLLERR,
                      "After sending the third packet");

    if (close_tst_first)
    {
        TEST_STEP("If @p close_tst_first is @c TRUE, call @b close() on "
                  "the Tester socket and wait for an event on ZF stack "
                  "on IUT.");
        RPC_CLOSE(pco_tst, tst_s);
        rpc_zf_wait_for_event(pco_iut, stack);
    }

    if (retransmit_type == RETRANSMIT_FIN)
    {
        TEST_STEP("if @p retransmit_type is @c RETRANSMIT_FIN, break "
                  "connection between gateway and Tester.");
        CHECK_RC(tapi_route_gateway_break_gw_tst(&gateway));
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    TEST_STEP("Call @b zft_shutdown_tx() on the IUT zocket.");
    rpc_zft_shutdown_tx(pco_iut, iut_zft);

    if (retransmit_type == RETRANSMIT_FIN)
    {
        TEST_STEP("If @p retransmit_type is @c RETRANSMIT_FIN, wait for "
                  "a while, then repair broken connection and wait again "
                  "so that TCP FIN packet can reach Tester successfully. "
                  "Call @b zf_reactor_perform() during wating.");
        wait_retransmit_repair_conn(pco_iut, &gateway, stack);
    }
    else
    {
        TEST_STEP("If @p retransmit_type is not @c RETRANSMIT_FIN, "
                  "just wait for a while calling "
                  "@b zf_reactor_perform().");
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    if (!close_tst_first)
    {
        TEST_STEP("If @p close_tst_first is @c FALSE, call @b close() on "
                  "the Tester socket and wait for an event on ZF stack "
                  "on IUT.");
        RPC_CLOSE(pco_tst, tst_s);
        rpc_zf_wait_for_event(pco_iut, stack);
    }

    TEST_STEP("Check that after connection termination @c EPOLLERR is "
              "reported by the muxer.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft,
                      0, RPC_EPOLLERR,
                      "After terminating connection");

    TEST_STEP("Obtain all the reports about sent packets on IUT zocket "
              "with @b zft_get_tx_timestamps()");
    ts_get_tx_reports(pco_iut, iut_zft, &all_reports, 1, MAX_REPORTS,
                      FALSE);

    memset(&cb_data, 0, sizeof(cb_data));
    cb_data.callback = &ts_tcp_tx_csap_cb;
    cb_data.user_data = &user_data;

    memset(&user_data, 0, sizeof(user_data));
    user_data.reports = &all_reports;
    user_data.ms_adjust = ts_get_clock_adjustment(pco_iut, pco_gw);

    TEST_STEP("Process packets captured by the CSAP and check that "
              "every packet has a matching packet report. Check that "
              "reports for retransmitted packets have @c TCP_RETRANS "
              "flag set.");

    CHECK_RC(tapi_tad_trrecv_stop(pco_gw->ta, 0, csap, &cb_data,
                                  &pkts_num));
    if (pkts_num == 0)
        TEST_VERDICT("CSAP has not captured any packets");

    reports_num = te_vec_size(user_data.reports);
    RING("%u packets were captured (not counting empty ACKs), "
         "%u packet reports were obtained",
         user_data.pkts_cnt, reports_num);

    if (user_data.pkts_cnt > reports_num)
        TEST_VERDICT("Reports for some packets are missed");

    if (user_data.failed)
        TEST_STOP;

    if (user_data.cur_id < reports_num)
    {
        TEST_VERDICT("Some reports are left not matched after "
                     "processing all the captured packets");
    }

    if (user_data.max_offset < total_sent)
    {
        TEST_VERDICT("Less data than expected was found in captured "
                     "packets");
    }

    if (user_data.retrans_cnt == 0)
    {
        TEST_VERDICT("No packet reports with TCP_RETRANS flag were "
                     "detected");
    }

    if (user_data.syn_retrans > 0 && retransmit_type != RETRANSMIT_SYN)
        RING_VERDICT("Unexpected SYN retransmits were detected");

    if (user_data.fin_retrans > 0 && retransmit_type != RETRANSMIT_FIN)
        RING_VERDICT("Unexpected FIN retransmits were detected");

    if (user_data.data_retrans > 0 && retransmit_type != RETRANSMIT_DATA)
        RING_VERDICT("Unexpected data retransmits were detected");

    if (user_data.syn_retrans == 0 && retransmit_type == RETRANSMIT_SYN)
        TEST_VERDICT("Expected SYN retransmits were not detected");

    if (user_data.fin_retrans == 0 && retransmit_type == RETRANSMIT_FIN)
        TEST_VERDICT("Expected FIN retransmits were not detected");

    if (user_data.data_retrans == 0 && retransmit_type == RETRANSMIT_DATA)
        TEST_VERDICT("Expected data retransmits were not detected");

    TEST_STEP("Check that @c EPOLLERR is not reported by the muxer now.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft, 0, 0,
                      "After getting all the reports");

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s_listener);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFT_HANDLE_FREE(pco_iut, iut_zft_handle);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, waitable);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_CHECK_RC(tapi_tad_csap_destroy(pco_gw->ta, 0, csap));

    te_vec_free(&all_reports);

    TEST_END;
}
