/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Timestamps tests
 */

/**
 * @page timestamps-alloc_stack_sleep_connect Sleeping between stack allocation and creating TCP zocket
 *
 * @objective Check that when there is some sleep betwen allocating ZF
 *            stack and creating TCP zocket, all TX timestamps can
 *            still be obtained.
 *
 * @note This test is intended to reproduce the second issue from ON-12486.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on Tester.
 * @param iut_addr      Network address on IUT.
 * @param tst_addr      Network address on Tester.
 * @param open_method   How to open connection:
 *                      - @c active
 *                      - @c passive
 *                      - @c passive_close (close listener after
 *                        passively establishing connection)
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "timestamps/alloc_stack_sleep_connect"

#include "zf_test.h"
#include "rpc_zf.h"
#include "timestamps.h"

/** Number of packets to send */
#define PKTS_NUM 5

/**
 * Maximum number of timestamp reports to obtain with a single call of
 * zft_get_tx_timestamps()
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
    rpc_zft_p iut_zft = RPC_NULL;
    rpc_zftl_p iut_zftl = RPC_NULL;
    int tst_s = -1;

    rpc_zf_muxer_set_p muxer_set = RPC_NULL;
    rpc_zf_waitable_p waitable = RPC_NULL;

    unsigned int i;
    unsigned int total_sent = 0;
    unsigned int max_offset = 0;

    te_vec reports = TE_VEC_INIT(tarpc_zf_pkt_report);
    tarpc_zf_pkt_report *report;
    tarpc_timespec ts_prev;
    tarpc_timeval tv;
    int64_t start_ts;
    int64_t end_ts;
    int64_t cur_ts;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    TEST_STEP("Allocate ZF attributes and stack, enabling "
              "@b tx_timestamping.");
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_attr_set_int(pco_iut, attr, "tx_timestamping", 1);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*
     * One second is not enough here. This test is written
     * after a reproducer for ON-12486.
     */
    TEST_STEP("Wait for 2 seconds.");
    SLEEP(2);

    rpc_gettimeofday(pco_iut, &tv, NULL);
    start_ts = TS_TIMEVAL2MS(tv) - TE_US2MS(TS_DEF_PRECISION);

    TEST_STEP("Create TCP Zocket on IUT and TCP socket on Tester, "
              "establish TCP connection according to @p open_method.");
    zfts_establish_tcp_conn_ext2(open_method,
                                 pco_iut, attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    TEST_STEP("Allocate ZF muxer to wait for @c EPOLLERR on the IUT "
              "zocket.");
    ts_tx_muxer_configure(pco_iut, stack, iut_zft, &muxer_set, &waitable);

    TEST_STEP("Check that @c EPOLLERR is reported now because "
              "TX timestamp is available for SYN packet.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft, 0, RPC_EPOLLERR,
                      "After connection establishment");

    TEST_STEP("Send a few packets from IUT and receive them on Tester.");
    TEST_SUBSTEP("Check that after sending new packets @c EPOLLERR is "
                 "reported by the muxer.");
    for (i = 0; i < PKTS_NUM; i++)
    {
        total_sent += zfts_zft_check_sending(pco_iut, stack, iut_zft,
                                             pco_tst, tst_s);

        ts_tx_muxer_check(pco_iut, muxer_set, iut_zft,
                          0, RPC_EPOLLERR,
                          "After sending data");
    }

    RING("Sent %u bytes from IUT", total_sent);

    rpc_gettimeofday(pco_iut, &tv, NULL);
    end_ts = TS_TIMEVAL2MS(tv) + TE_US2MS(TS_DEF_PRECISION);

    TEST_STEP("Obtain all the TX timestamp reports with "
              "@b zft_get_tx_timestamps().");
    ts_get_tx_reports(pco_iut, iut_zft, &reports,
                      MAX_REPORTS, MAX_REPORTS, FALSE);
    if (te_vec_size(&reports) == 0)
        TEST_VERDICT("No timestamp reports was received");

    TEST_STEP("Check that there are timestamp reports for all the "
              "sent data.");
    for (i = 0; i < te_vec_size(&reports); i++)
    {
        report = (tarpc_zf_pkt_report *)te_vec_get(&reports, i);

        if (i > 0 && ts_cmp(&ts_prev, &report->timestamp) > 0)
            TEST_VERDICT("The next report has smaller timestamp");

        cur_ts = TS_TIMESPEC2MS(report->timestamp);
        if (cur_ts < start_ts || cur_ts > end_ts)
            TEST_VERDICT("Reported timestamp is out of range");

        if (!(report->flags & (TARPC_ZF_PKT_REPORT_TCP_SYN |
                               TARPC_ZF_PKT_REPORT_TCP_RETRANS)))
        {
            if (max_offset != report->start)
            {
                TEST_VERDICT("Timestamp report has unexpected 'start' "
                             "field value");
            }
            max_offset = report->start + report->bytes;
            if (max_offset > total_sent)
            {
                TEST_VERDICT("Timestamp report has too big 'start' or "
                             "'bytes' field value");
            }
        }

        memcpy(&ts_prev, &report->timestamp, sizeof(ts_prev));
    }

    if (max_offset < total_sent)
    {
        TEST_VERDICT("Timestamp report(s) for some of the sent data "
                     "is missing");
    }

    TEST_STEP("Check that @c EPOLLERR is not reported by the muxer now.");
    ts_tx_muxer_check(pco_iut, muxer_set, iut_zft, 0, 0,
                      "After getting all the reports");

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    te_vec_free(&reports);

    TEST_END;
}
