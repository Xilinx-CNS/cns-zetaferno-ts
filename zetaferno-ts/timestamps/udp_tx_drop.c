/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Timestamps tests
 */

/**
 * @page timestamps-udp_tx_drop Dropped packet reports when obtaining UDP TX timestamps
 *
 * @objective Check that when too many UDP packets are sent, some packet
 *            reports with TX timestamps are dropped, and
 *            @c ZF_PKT_REPORT_DROPPED flag is set to notify about it.
 *
 * @param pco_iut           PCO on IUT.
 * @param pco_tst           PCO on Tester.
 * @param iut_addr          Network address on IUT.
 * @param tst_addr          Network address on Tester.
 * @param send_func         Function to use for sending:
 *                          - @b zfut_send_single()
 *                          - @b zfut_send()
 * @param large_buffer      If @c TRUE, send large UDP packets so
 *                          that they can be fragmented (should be
 *                          used only with @b zfut_send())
 * @param few_iov           If @c TRUE, pass multiple IOVs to
 *                          the sending function (applicable
 *                          only for @b zfut_send()).
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "timestamps/udp_tx_drop"

#include "zf_test.h"
#include "rpc_zf.h"
#include "timestamps.h"

/** Allowed time deviation in microseconds */
#define TST_PRECISION 500000

/** Maximum number of UDP packets to send in a bunch */
#define MAX_PKTS_PER_BUNCH 100

/** Maximum time to wait after sending a bunch, in ms */
#define MAX_WAIT_AFTER_BUNCH 10

/** Maximum number of packet bunches */
#define MAX_BUNCHES 1000

/**
 * Maximum number of reports to retrieve with a single call
 * of zfut_get_tx_timestamps().
 */
#define MAX_REPORTS 50

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zfut_p iut_z = RPC_NULL;
    int tst_s = -1;

    int pkts_num;
    int i;
    int j;

    te_vec pkts_vec = TE_VEC_INIT(ts_udp_tx_pkt_descr);
    te_vec reports_vec = TE_VEC_INIT(tarpc_zf_pkt_report);
    unsigned int dropped_cnt = 0;
    int reports_num;
    tarpc_zf_pkt_report *report;

    zfts_send_function send_func;
    te_bool large_buffer;
    te_bool few_iov;

    tarpc_timeval tv;

    rpc_zf_muxer_set_p muxer_set = RPC_NULL;
    rpc_zf_waitable_p waitable = RPC_NULL;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    ZFTS_TEST_GET_ZFUT_FUNCTION(send_func);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);

    TEST_STEP("Allocate ZF attributes and stack, enabling "
              "@b tx_timestamping.");
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_attr_set_int(pco_iut, attr, "tx_timestamping", 1);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    TEST_STEP("Allocate Zetaferno UDP TX zocket on IUT, bind it to "
              "@b iut_addr and connect it to @b tst_addr.");
    rpc_zfut_alloc(pco_iut, &iut_z, stack, iut_addr, tst_addr, 0, attr);

    TEST_STEP("Create UDP socket on Tester, bind it to @p tst_addr.");
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    TEST_STEP("Allocate ZF muxer to wait for @c EPOLLERR on the IUT "
              "zocket.");
    ts_tx_muxer_configure(pco_iut, stack, iut_z, &muxer_set, &waitable);

    TEST_STEP("In a loop, repeatedly send a bunch of packets according "
              "to @p send_func, @p large_buffer and @p few_iov. After "
              "each bunch call @b zfut_get_tx_timestamps() to retrieve "
              "a single packet report and append the report to the array "
              "of all the retrieved reports. If the report has "
              "@c ZF_PKT_REPORT_DROPPED flag set, break the loop.");
    TEST_SUBSTEP("Check that after sending a packet ZF muxer reports "
                 "@c EPOLLERR event because a new TX timestamp is "
                 "now available.");

    for (j = 0; j < MAX_BUNCHES; j++)
    {
        pkts_num = rand_range(2, MAX_PKTS_PER_BUNCH);
        for (i = 0; i < pkts_num; i++)
        {
            ts_udp_tx_pkt_descr pkt;

            rpc_gettimeofday(pco_iut, &tv, NULL);
            TIMEVAL_TO_TIMESPEC(&tv, &pkt.send_ts);

            zfts_zfut_check_send_func_ext(pco_iut, stack, iut_z, pco_tst,
                                          tst_s, send_func, large_buffer,
                                          few_iov, TRUE, &pkt.len);

            ts_tx_muxer_check(pco_iut, muxer_set, iut_z, 0, RPC_EPOLLERR,
                              "After sending data");

            CHECK_RC(te_vec_append(&pkts_vec, &pkt));
        }

        ts_get_tx_reports(pco_iut, iut_z, &reports_vec, 1, 1, TRUE);
        reports_num = te_vec_size(&reports_vec);
        if (reports_num != j + 1)
        {
            TEST_VERDICT("Wrong number of reports was read after sending "
                         "a few packets instead of a single report");
        }
        report = (tarpc_zf_pkt_report *)te_vec_get(&reports_vec,
                                                   reports_num - 1);
        if (report->flags & TARPC_ZF_PKT_REPORT_DROPPED)
            break;

        MSLEEP(rand_range(1, MAX_WAIT_AFTER_BUNCH));
    }

    TEST_STEP("Retrieve all the available packet reports calling "
              "@b zfut_get_tx_timestamps() in a loop until zero reports "
              "is retrieved. Append all the retrieved reports to the same "
              "array.");
    ts_get_tx_reports(pco_iut, iut_z, &reports_vec, 1, MAX_REPORTS, FALSE);

    TEST_STEP("Now check that reports were retieved in order for all "
              "the sent UDP packets, with TX timestamps matching their "
              "sending times, except for a few packets for which reports "
              "were dropped. Only the single packet report after a group "
              "of missing reports should have @c ZF_PKT_REPORT_DROPPED "
              "flag set.");
    ts_zfut_check_tx_reports(&reports_vec, &pkts_vec, TST_PRECISION,
                             &dropped_cnt);
    if (dropped_cnt > 1)
        TEST_VERDICT("More than one packet report has DROPPED flag set");
    else if (dropped_cnt == 0)
        TEST_VERDICT("No packet report has DROPPED flag set");

    TEST_STEP("Check that @c EPOLLERR is not reported by the muxer now.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_z, 0, 0,
                      "After getting all the reports");

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_z);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, waitable);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    te_vec_free(&reports_vec);
    te_vec_free(&pkts_vec);

    TEST_END;
}
