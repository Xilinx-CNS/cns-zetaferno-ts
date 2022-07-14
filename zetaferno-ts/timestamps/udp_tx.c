/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Timestamps tests
 */

/**
 * @page timestamps-udp_tx Obtaining TX timestamps for sent UDP packets
 *
 * @objective Check that when TX timestamps are configured, they can
 *            be obtained for sent UDP packets.
 *
 * @param pco_iut           PCO on IUT.
 * @param pco_tst           PCO on TST.
 * @param iut_addr          Network address on IUT.
 * @param tst_addr          Network address on Tester.
 * @param pkts_num          Number of packets to send and receive.
 * @param multiple_reports  If @c TRUE, pass less report structures
 *                          to @b zfut_get_tx_timestamps() than
 *                          @p pkts_num, to retrieve reports in
 *                          multiple calls.
 * @param send_func         Function to use for sending:
 *                          - @b zfut_send_single()
 *                          - @b zfut_send()
 * @param large_buffer      If @c TRUE, send large UDP packet so
 *                          that it can be fragmented (should be
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

#define TE_TEST_NAME  "timestamps/udp_tx"

#include "zf_test.h"
#include "rpc_zf.h"
#include "timestamps.h"

/** Allowed time deviation in microseconds */
#define TST_PRECISION 500000

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

    te_vec pkts_vec = TE_VEC_INIT(ts_udp_tx_pkt_descr);
    te_vec reports_vec = TE_VEC_INIT(tarpc_zf_pkt_report);
    unsigned int dropped_cnt = 0;

    te_bool multiple_reports;
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
    TEST_GET_INT_PARAM(pkts_num);
    TEST_GET_BOOL_PARAM(multiple_reports);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);
    ZFTS_TEST_GET_ZFUT_FUNCTION(send_func);

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

    TEST_STEP("Send @p pkts_num packets from the IUT zocket using "
              "@p send_func, receive and check them on Tester. "
              "Remember time when each packet is sent, wait "
              "for a while after sending a packet.");
    TEST_SUBSTEP("Check that after sending a packet ZF muxer reports "
                 "@c EPOLLERR event because a new TX timestamp is "
                 "now available.");
    for (i = 0; i < pkts_num; i++)
    {
        ts_udp_tx_pkt_descr pkt;

        rpc_gettimeofday(pco_iut, &tv, NULL);
        TIMEVAL_TO_TIMESPEC(&tv, &pkt.send_ts);

        zfts_zfut_check_send_func_ext(pco_iut, stack, iut_z, pco_tst, tst_s,
                                      send_func, large_buffer, few_iov,
                                      TRUE, &pkt.len);

        ts_tx_muxer_check(pco_iut, muxer_set, iut_z, 0, RPC_EPOLLERR,
                          "After sending data");

        CHECK_RC(te_vec_append(&pkts_vec, &pkt));
        MSLEEP(rand_range(1, 100));
    }

    TEST_STEP("Call @b zfut_get_tx_timestamps() in a loop until it "
              "returns zero packet reports. If @p multiple_reports "
              "is @c TRUE, pass less report structures to it than "
              "@p pkts_num each time, so that reports will be "
              "obtained via multiple calls.");
    ts_get_tx_reports(pco_iut, iut_z, &reports_vec,
                      (multiple_reports ? 1 : pkts_num),
                      (multiple_reports ? pkts_num - 1 : pkts_num),
                      FALSE);

    TEST_STEP("Check that in every packet report @b start field "
              "is equal to the number of the corresponding packet "
              "in the sequence of all the sent packets, "
              "@b bytes - to payload size of that packet, "
              "@b timestamp matches time of sending, and @b flags "
              "are @c ZF_PKT_REPORT_CLOCK_SET | "
              "@c ZF_PKT_REPORT_IN_SYNC.");
    TEST_STEP("Check that number of received packet reports is equal "
              "to @p pkts_num.");

    ts_zfut_check_tx_reports(&reports_vec, &pkts_vec, TST_PRECISION,
                             &dropped_cnt);
    if (dropped_cnt > 0)
        TEST_VERDICT("Some packet reports were dropped");

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
