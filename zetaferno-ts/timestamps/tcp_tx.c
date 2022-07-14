/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Timestamps tests
 */

/**
 * @page timestamps-tcp_tx Obtaining TX timestamps for sent TCP packets
 *
 * @objective Check that when TX timestamps are configured, they can
 *            be obtained for sent TCP packets.
 *
 * @param pco_iut           PCO on IUT.
 * @param pco_tst           PCO on Tester.
 * @param iut_addr          Network address on IUT.
 * @param tst_addr          Network address on Tester.
 * @param open_method       How to open connection:
 *                          - @c active
 *                          - @c passive
 *                          - @c passive_close (close listener after
 *                            passively establishing connection)
 * @param close_tst_first   If @c TRUE, close TCP connection from
 *                          Tester firstly, otherwise - from IUT.
 * @param send_func         Function to use for sending:
 *                          - @b zft_send_single()
 *                          - @b zft_send()
 * @param msg_more          If @c TRUE, use @c MSG_MORE flag for all
 *                          sending calls except the last one.
 * @param dropped           If @c TRUE, send as many packets as
 *                          necessary to make some of the packet
 *                          reports being dropped before they
 *                          are retrieved, and check
 *                          @c ZF_PKT_REPORT_DROPPED flag usage.
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "timestamps/tcp_tx"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_tad.h"
#include "tapi_tcp.h"
#include "tapi_cfg_if.h"
#include "te_vector.h"
#include "timestamps.h"

/** Allowed time deviation in milliseconds */
#define TST_PRECISION 500

/**
 * Maximum number of packet reports retrieved by a single
 * zft_get_tx_timestamps() call.
 */
#define MAX_REPORTS 10

/**
 * Number of packet bunches to send from IUT when no
 * dropped reports should appear.
 */
#define BUNCHES_NUM_NORMAL 5

/**
 * Maximum number of packet bunches to send when checking
 * dropped packet reports.
 * This value should be big enough to account for
 * the bond/team case.
 */
#define BUNCHES_NUM_DROPPED 2000

/**
 * Maximum time to wait after sending a bunch of packets when
 * dropped reports are not checked, ms.
 */
#define MAX_WAIT_AFTER_BUNCH_NORMAL 500

/**
 * Maximum time to wait after sending a bunch of packets when
 * dropped reports are checked, in ms.
 */
#define MAX_WAIT_AFTER_BUNCH_DROPPED 10

/** Maximum number of sending calls per packet bunch */
#define MAX_SENDS_PER_BUNCH 5

/** Maximum number of IOVs passed to zft_send() */
#define MAX_IOVS 3

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    const struct if_nameindex *tst_if = NULL;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    rpc_zftl_p iut_zftl = RPC_NULL;
    int tst_s = -1;

    rpc_zf_muxer_set_p muxer_set = RPC_NULL;
    rpc_zf_waitable_p waitable = RPC_NULL;

    zfts_conn_open_method open_method;
    te_bool close_tst_first;
    zfts_tcp_send_func_t send_func;
    te_bool msg_more;
    te_bool dropped;

    char bufs[MAX_IOVS][ZFTS_TCP_DATA_MAX];
    rpc_iovec iovs[MAX_IOVS];
    int i;
    int j;
    int k;
    int sends_num;
    int send_size;
    int iovs_num;
    int bunches_num;

    int mss;
    int send_flags;

    te_dbuf sent_data = TE_DBUF_INIT(100);
    char recv_buf[MAX_SENDS_PER_BUNCH *
                  MAX_IOVS * ZFTS_TCP_DATA_MAX];
    int recv_len = 0;
    te_bool readable;
    unsigned int total_sent_len = 0;
    te_bool dropped_encountered = FALSE;

    te_vec all_reports = TE_VEC_INIT(tarpc_zf_pkt_report);
    tarpc_zf_pkt_report *report;
    int reports_num;

    csap_handle_t csap = CSAP_INVALID_HANDLE;
    ts_tcp_tx_csap_data user_data;
    tapi_tad_trrecv_cb_data cb_data;
    unsigned int pkts_num = 0;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(tst_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_BOOL_PARAM(close_tst_first);
    TEST_GET_ENUM_PARAM(send_func, ZFTS_TCP_SEND_FUNCS);
    TEST_GET_BOOL_PARAM(msg_more);
    TEST_GET_BOOL_PARAM(dropped);

    TEST_STEP("Disable GRO on Tester interface so that CSAP will "
              "capture packets as they are sent from IUT.");
    CHECK_RC(tapi_cfg_if_feature_set_all_parents(pco_tst->ta,
                                                 tst_if->if_name,
                                                 "rx-gro", 0));

    /* Do not print out packets captured by CSAP. */
    rcf_tr_op_log(FALSE);

    TEST_STEP("Configure a CSAP on Tester to capture TCP packets sent "
              "from IUT.");
    CHECK_RC(tapi_tcp_ip4_eth_csap_create(pco_tst->ta, 0,
                                          tst_if->if_name,
                                          TAD_ETH_RECV_DEF |
                                          TAD_ETH_RECV_NO_PROMISC,
                                          NULL, NULL,
                                          SIN(tst_addr)->sin_addr.s_addr,
                                          SIN(iut_addr)->sin_addr.s_addr,
                                          SIN(tst_addr)->sin_port,
                                          SIN(iut_addr)->sin_port,
                                          &csap));

    CHECK_RC(tapi_tad_trrecv_start(pco_tst->ta, 0, csap,
                                   NULL, TAD_TIMEOUT_INF, 0,
                                   RCF_TRRECV_PACKETS));

    SLEEP(2);

    TEST_STEP("Allocate ZF attributes and stack, enabling "
              "@b tx_timestamping.");
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_attr_set_int(pco_iut, attr, "tx_timestamping", 1);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    TEST_STEP("Create TCP Zocket on IUT and TCP socket on Tester, "
              "establish TCP connection according to @p open_method.");

    zfts_establish_tcp_conn_ext2(open_method,
                                 pco_iut, attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    rpc_zf_process_events_long(
                    pco_iut, stack,
                    rand_range(1, ZFTS_WAIT_EVENTS_TIMEOUT));

    TEST_STEP("Allocate ZF muxer to wait for @c EPOLLERR on the IUT "
              "zocket.");
    ts_tx_muxer_configure(pco_iut, stack, iut_zft, &muxer_set, &waitable);

    TEST_STEP("Check that @c EPOLLERR is reported now because "
              "TX timestamp is available for SYN packet.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft, 0, RPC_EPOLLERR,
                      "After connection establishment");

    mss = rpc_zft_get_mss(pco_iut, iut_zft);

    TEST_STEP("Send a few bunches of packets from IUT to Tester, "
              "wating for a while after each bunch to increase "
              "difference in timestamps. Use @p send_func to send "
              "data, pass @c MSG_MORE flag to it if required by "
              "@p msg_more.");
    if (dropped)
    {
        TEST_SUBSTEP("If @p dropped is @c TRUE, after each bunch "
                     "retrieve the single packet report with "
                     "@b zft_get_tx_timestamps() and check its "
                     "@b flags. Keep sending new packet bunches "
                     "until the retrieved report has @b DROPPED "
                     "flag set. Append every retrieved report to "
                     "an array of all the retrieved reports.");
    }
    TEST_SUBSTEP("Check that after sending new packets @c EPOLLERR is "
                 "reported by the muxer.");

    bunches_num = (dropped ? BUNCHES_NUM_DROPPED : BUNCHES_NUM_NORMAL);
    for (i = 0; i < bunches_num; i++)
    {
        sends_num = rand_range((dropped ? 2 : 1), MAX_SENDS_PER_BUNCH);
        for (j = 0; j < sends_num; j++)
        {
            /*
             * Do not set MSG_MORE in the last packet of the last
             * bunch to ensure that all data is sent eventually.
             */
            if (msg_more && (i < bunches_num - 1 || j < sends_num - 1))
                send_flags = RPC_MSG_MORE;
            else
                send_flags = 0;

            if (send_func == ZFTS_TCP_SEND_ZFT_SEND_SINGLE)
            {
                send_size = rand_range(1, sizeof(bufs[0]));
                te_fill_buf(bufs[0], send_size);
                rc = rpc_zft_send_single(pco_iut, iut_zft, bufs[0],
                                         send_size, send_flags);
                CHECK_RC(te_dbuf_append(&sent_data, bufs[0], send_size));
            }
            else
            {
                send_size = 0;
                iovs_num = rand_range(1, MAX_IOVS);
                for (k = 0; k < iovs_num; k++)
                {
                    iovs[k].iov_len = iovs[k].iov_rlen =
                        rand_range(1, ZFTS_TCP_DATA_MAX);
                    iovs[k].iov_base = bufs[k];
                    te_fill_buf(bufs[k], iovs[k].iov_len);

                    send_size += iovs[k].iov_len;
                    CHECK_RC(te_dbuf_append(&sent_data, bufs[k],
                                            iovs[k].iov_len));
                }

                rc = rpc_zft_send(pco_iut, iut_zft, iovs, iovs_num,
                                  send_flags);
            }

            if (rc != send_size)
            {
                TEST_VERDICT("ZF sending function returned unexpected "
                             "value");
            }
            total_sent_len += send_size;

            if (!msg_more)
            {
                /*
                 * When using MSG_MORE, it is possible that a new
                 * zft_send()/zft_send_single() call does not result
                 * in sending a new packet, and therefore EPOLLERR
                 * event may be not reported as ZF muxer is
                 * edge-triggered.
                 */
                ts_tx_muxer_check(pco_iut, muxer_set, iut_zft,
                                  ZFTS_WAIT_EVENTS_TIMEOUT, RPC_EPOLLERR,
                                  "After sending data");
            }
        }

        if (i == bunches_num - 1)
            ZFTS_WAIT_NETWORK(pco_iut, stack);

        /*
         * All data is not checked right here because when MSG_MORE is
         * used, some data may be not yet sent until the last sending
         * call is called without MSG_MORE.
         */
        RPC_GET_READABILITY(readable, pco_tst, tst_s, 0);
        if (readable)
        {
            recv_len = rpc_recv(pco_tst, tst_s, recv_buf,
                                sizeof(recv_buf), 0);
        }

        if ((unsigned int)recv_len > sent_data.len)
            TEST_VERDICT("More data was received than sent");

        if (memcmp(recv_buf, sent_data.ptr, recv_len) != 0)
        {
            TEST_VERDICT("Data received on Tester did not match data sent "
                         "from IUT");
        }

        te_dbuf_cut(&sent_data, 0, recv_len);
        recv_len = 0;

        rpc_zf_process_events_long(
              pco_iut, stack,
              rand_range(1, (dropped ?
                             MAX_WAIT_AFTER_BUNCH_DROPPED :
                             MAX_WAIT_AFTER_BUNCH_NORMAL)));

        if (dropped && !dropped_encountered)
        {
            ts_get_tx_reports(pco_iut, iut_zft, &all_reports, 1, 1,
                              TRUE);

            reports_num = te_vec_size(&all_reports);
            if (reports_num > 0)
            {
                report = (tarpc_zf_pkt_report *)te_vec_get(&all_reports,
                                                           reports_num - 1);
                if (report->flags & TARPC_ZF_PKT_REPORT_DROPPED)
                {
                    RING("Obtained a packet report with "
                         "ZF_PKT_REPORT_DROPPED flag set, finishing "
                         "sending");
                    dropped_encountered = TRUE;
                    if (msg_more)
                    {
                        /*
                         * Let the last bunch to be sent, where the last
                         * sending call will be without MSG_MORE. It
                         * will ensure that all the queued data is sent.
                         */
                        bunches_num = i + 2;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }

    if (sent_data.len > 0)
    {
        TEST_VERDICT("Not all the data sent from IUT was received "
                     "on Tester");
    }

    if (msg_more)
    {
        ts_tx_muxer_check(pco_iut, muxer_set, iut_zft,
                          0, RPC_EPOLLERR,
                          "After sending data");
    }

    TEST_STEP("Close TCP connection according to @p close_tst_first.");

    if (close_tst_first)
    {
        RPC_CLOSE(pco_tst, tst_s);
        rpc_zf_wait_for_event(pco_iut, stack);
    }

    rpc_zft_shutdown_tx(pco_iut, iut_zft);
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    if (!close_tst_first)
    {
        RPC_CLOSE(pco_tst, tst_s);
        rpc_zf_wait_for_event(pco_iut, stack);
    }

    TEST_STEP("Check that after connection termination @c EPOLLERR is "
              "reported by the muxer.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft, 0, RPC_EPOLLERR,
                      "After terminating connection");

    TEST_STEP("Call @b zft_get_tx_timestamps() in a loop until it returns "
              "zero packet reports. Append all retrieved reports to the "
              "same array.");

    ts_get_tx_reports(pco_iut, iut_zft, &all_reports, 1, MAX_REPORTS,
                      FALSE);

    if (te_vec_size(&all_reports) == 0)
        TEST_VERDICT("No packet reports were retrieved");

    TEST_STEP("Process packets captured by the CSAP on Tester. Check that "
              "there is a matching packet report for every packet sent "
              "from IUT except for ACKs with no data and for packets for "
              "which reports were dropped (if @p dropped is @c TRUE). "
              "Check that timestamps from packet reports match timestamps "
              "reported by the CSAP. If @p msg_more is @c TRUE, check that "
              "all the packets except the last one have payload of MSS "
              "bytes.");

    memset(&cb_data, 0, sizeof(cb_data));
    cb_data.callback = &ts_tcp_tx_csap_cb;
    cb_data.user_data = &user_data;

    memset(&user_data, 0, sizeof(user_data));
    user_data.reports = &all_reports;
    user_data.ms_adjust = ts_get_clock_adjustment(pco_iut, pco_tst);
    user_data.msg_more = msg_more;
    user_data.mss = mss;

    CHECK_RC(tapi_tad_trrecv_stop(pco_tst->ta, 0, csap, &cb_data,
                                  &pkts_num));
    if (pkts_num == 0)
        TEST_VERDICT("CSAP has not captured any packets");

    if (user_data.failed)
        TEST_STOP;

    if (!user_data.fin_encountered)
        TEST_VERDICT("FIN packet was not captured");

    if (msg_more && user_data.less_mss_detected &&
        user_data.first_less_mss < user_data.max_data_off)
    {
        TEST_VERDICT("MSG_MORE did not work properly");
    }

    if (user_data.max_offset != total_sent_len)
        TEST_VERDICT("Not all the expected packets were processed");

    if (user_data.cur_id < te_vec_size(user_data.reports))
        TEST_VERDICT("Not all the packet reports were processed");

    RING("%u reports with DROPPED flag were retrieved", user_data.drop_cnt);
    if (dropped)
    {
        if (user_data.drop_cnt == 0)
        {
            TEST_VERDICT("No report with DROPPED flag was retrieved");
        }
        else if (user_data.drop_cnt > (msg_more ? 2 : 1))
        {
            /*
             * In case of checking MSG_MORE we send more data after
             * encountering a report with DROPPED flag, which may
             * result in dropping additional reports, so we allow for
             * two reports with DROPPED flag in this case.
             */
            TEST_VERDICT("Too many reports with DROPPED flag were "
                         "retrieved");
        }
    }
    else
    {
        if (user_data.drop_cnt > 0)
        {
            TEST_VERDICT("Report(s) with DROPPED flag was retrieved "
                         "unexpectedly");
        }
    }

    TEST_STEP("Check that @c EPOLLERR is not reported by the muxer now.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft, 0, 0,
                      "After getting all the reports");

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, waitable);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_CHECK_RC(tapi_tad_csap_destroy(pco_tst->ta, 0, csap));

    te_dbuf_free(&sent_data);
    te_vec_free(&all_reports);

    TEST_END;
}
