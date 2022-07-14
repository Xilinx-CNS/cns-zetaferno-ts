/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Timestamps tests
 */

/**
 * @page timestamps-udp_rx Obtaining RX timestamps for received UDP packets
 *
 * @objective Check that when RX timestamps are configured, they can
 *            be obtained for UDP packets received with @b zfur_zc_recv().
 *
 * @param pco_iut   PCO on IUT.
 * @param pco_tst   PCO on TST.
 * @param iut_addr  Network address on IUT.
 * @param tst_addr  Network address on Tester.
 * @param pkts_num  Number of packets to send and receive.
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "timestamps/udp_rx"

#include "zf_test.h"
#include "rpc_zf.h"
#include "timestamps.h"

/** Allowed time deviation in microseconds */
#define TST_PRECISION 500000

/** Description of an UDP packet */
typedef struct pkt_descr {
    char buf[ZFTS_DGRAM_MAX];   /**< Payload */
    size_t len;                 /**< Payload length */
    tarpc_timespec send_ts;     /**< Send timestamp */
    tarpc_timespec recv_ts;     /**< Receive timestamp */
} pkt_descr;

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zfur_p iut_s = RPC_NULL;
    int tst_s = -1;

    pkt_descr *pkts = NULL;
    rpc_zfur_msg msg;
    rpc_iovec iov;
    char recv_buf[ZFTS_DGRAM_MAX];

    int pkts_num;
    int i;

    tarpc_timeval tv;
    unsigned int flags;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(pkts_num);

    TEST_STEP("Allocate ZF attributes and stack, enabling "
              "@b rx_timestamping.");
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_attr_set_int(pco_iut, attr, "rx_timestamping", 1);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    TEST_STEP("Allocate Zetaferno UDP RX zocket, bind it to local address "
              "@p iut_addr and remote address @p tst_addr.");
    rpc_zfur_alloc(pco_iut, &iut_s, stack, attr);
    rpc_zfur_addr_bind(pco_iut, iut_s, SA(iut_addr), tst_addr, 0);

    TEST_STEP("Create UDP socket on Tester, bind it to @p tst_addr and "
              "connect it to @p iut_addr.");
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);
    rpc_connect(pco_tst, tst_s, iut_addr);

    TEST_STEP("Send @p pkts_num UDP packets from the Tester socket.");
    pkts = tapi_calloc(pkts_num, sizeof(*pkts));
    for (i = 0; i < pkts_num; i++)
    {
        pkts[i].len = rand_range(1, sizeof(pkts[i].buf));
        te_fill_buf(pkts[i].buf, pkts[i].len);

        rpc_gettimeofday(pco_iut, &tv, NULL);
        TIMEVAL_TO_TIMESPEC(&tv, &pkts[i].send_ts);
        RPC_SEND(rc, pco_tst, tst_s, pkts[i].buf, pkts[i].len, 0);
        MSLEEP(rand_range(1, 100));
    }

    TEST_STEP("Wait for a while calling zf_reactor_perform() in a loop "
              "until it returns non-zero.");
    rpc_zf_wait_for_event(pco_iut, stack);

    TEST_STEP("In a loop call zfur_zc_recv() until @p pkts_num packets "
              "are received.");
    TEST_SUBSTEP("After each zfur_zc_recv() call, call "
                 "zfur_pkt_get_timestamp() and check that retrieved "
                 "timestamp does not differ too much from the time when "
                 "the packet was sent.");
    TEST_SUBSTEP("If zfur_zc_recv() reports @b dgrams_left to be zero when "
                 "some packets are still to be received, call "
                 "zf_reactor_perform() in a loop again until it returns "
                 "non-zero.");

    for (i = 0; i < pkts_num; i++)
    {
        memset(&iov, 0, sizeof(iov));
        iov.iov_base = recv_buf;
        iov.iov_len = iov.iov_rlen = sizeof(recv_buf);

        memset(&msg, 0, sizeof(msg));
        msg.iovcnt = 1;
        msg.iov = &iov;

        rpc_zfur_zc_recv(pco_iut, iut_s, &msg, 0);
        if (msg.iovcnt != 1)
        {
            TEST_VERDICT("zfur_zc_recv() set msg.iovcnt to an unexpected "
                         "value");
        }
        else if (iov.iov_len != pkts[i].len ||
                 memcmp(iov.iov_base, pkts[i].buf, pkts[i].len) != 0)
        {
            TEST_VERDICT("zfur_zc_recv() returned unexpected data");
        }

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zfur_pkt_get_timestamp(pco_iut, iut_s, msg.ptr,
                                        &pkts[i].recv_ts, 0, &flags);
        if (rc < 0)
        {
            TEST_VERDICT("zfur_pkt_get_timestamp() failed unexpectedly "
                         "with error " RPC_ERROR_FMT,
                         RPC_ERROR_ARGS(pco_iut));
        }

        if (~flags & TARPC_ZF_SYNC_FLAG_CLOCK_SET)
        {
            TEST_VERDICT("NIC clock is not reported to be set by "
                         "zfur_pkt_get_timestamp()");
        }

        if (~flags & TARPC_ZF_SYNC_FLAG_CLOCK_IN_SYNC)
        {
            TEST_VERDICT("NIC clock is not reported to be synchronized by "
                         "zfur_pkt_get_timestamp()");
        }

        if (i > 0)
        {
            if (ts_cmp(&pkts[i].recv_ts, &pkts[i - 1].recv_ts) < 0)
            {
                TEST_VERDICT("The next packet has smaller RX timestamp");
            }
        }

        if (ts_check_deviation(&pkts[i].recv_ts, &pkts[i].send_ts,
                               0, TST_PRECISION))
        {
            TEST_VERDICT("RX timestamp of a packet differs too much "
                         "from its sending time");
        }

        rpc_zfur_zc_recv_done(pco_iut, iut_s, &msg);

        if (i < pkts_num - 1 && msg.dgrams_left == 0)
            rpc_zf_wait_for_event(pco_iut, stack);
    }

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    free(pkts);

    TEST_END;
}
