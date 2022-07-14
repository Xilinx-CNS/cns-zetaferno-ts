/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Timestamps tests
 */

/**
 * @page timestamps-tcp_rx Obtaining RX timestamps for received TCP packets
 *
 * @objective Check that when RX timestamps are configured, they can
 *            be obtained for TCP packets received with @b zft_zc_recv().
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param iut_addr      Network address on IUT.
 * @param tst_addr      Network address on Tester.
 * @param open_method   How to open connection:
 *                      - @c active
 *                      - @c passive
 *                      - @c passive_close (close listener after
 *                        passively establishing connection)
 * @param pkts_num      Number of packets to send and receive.
 * @param iovcnt        Number of IOVs which can be retrieved by a single
 *                      @b zft_zc_recv() call:
 *                      - @c 1
 *                      - @c -1 (means "random")
 * @param done_some     If @c TRUE, call @b zft_zc_recv_done_some() instead
 *                      of @b zft_zc_recv_done() after each @b zft_zc_recv()
 *                      call, initially acknowledging only part of the
 *                      retrieved data.
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "timestamps/tcp_rx"

#include "zf_test.h"
#include "rpc_zf.h"
#include "timestamps.h"

/** Allowed time deviation in microseconds */
#define TST_PRECISION 500000

/** Maximum length of TCP packet payload */
#define MAX_PKT_LEN 1000

/** Maximum number of packets to be sent in a single bunch */
#define MAX_BUNCH_SIZE 10

/**
 * How many times a packet can be split between multiple
 * zft_zc_recv() calls due to zft_zc_recv_done_some().
 */
#define MAX_DONE_SOME_SPLITS 2

/** Maximum number of IOVs retrieved by a single zft_zc_recv() call */
#define MAX_IOVS 5

/** Description of an TCP packet */
typedef struct pkt_descr {
    char buf[MAX_PKT_LEN];      /**< Payload */
    size_t len;                 /**< Payload length */
    char *cur_pos;              /**< Position in buffer up to which
                                     data was received and acknowleged
                                     with zft_zc_recv_done_some(). */
    size_t left_len;            /**< Data which still should be received
                                     and acknowledged. */
    int done_some_cnt;          /**< How many times this packet was split
                                     due to zft_zc_recv_done_some(). */
    tarpc_timespec send_ts;     /**< Send timestamp */
    tarpc_timespec recv_ts;     /**< Receive timestamp */
    te_bool recv_ts_set;        /**< RX timestamp is already obtained for
                                     this packet */
} pkt_descr;

/**
 * Send a bunch of packets from Tester to IUT.
 *
 * @param pco_tst         RPC server on Tester.
 * @param pco_iut         RPC server on IUT.
 * @param tst_s           TCP socket on Tester.
 * @param pkts            Where to save information about sent
 *                        packets.
 * @param left_pkts       Maximum number of packets which can be sent.
 *
 * @return Number of sent packets.
 */
static int
send_packets(rcf_rpc_server *pco_tst, rcf_rpc_server *pco_iut,
             int tst_s, pkt_descr *pkts, int left_pkts)
{
    tarpc_timeval tv;
    int pkts_to_send = rand_range(1, MIN(MAX_BUNCH_SIZE, left_pkts));
    int i;
    int rc;

    /*
     * In this loop delays should be avoided, or there may be issues
     * with TCP retransmits or packing multiple buffers in a single
     * packet.
     *
     * Calling zf_reactor_perform() in this loop to avoid it seems to be
     * forbidden in a header description of zft_pkt_get_timestamp():
     * "This function must be called after zf_reactor_perform() returns
     * a value greater than zero, and before zf_reactor_perform() is
     * called again".
     */
    for (i = 0; i < pkts_to_send; i++)
    {
        pkts[i].len = rand_range(1, sizeof(pkts[i].buf));
        te_fill_buf(pkts[i].buf, pkts[i].len);
        pkts[i].cur_pos = pkts[i].buf;
        pkts[i].left_len = pkts[i].len;

        rpc_gettimeofday(pco_iut, &tv, NULL);
        TIMEVAL_TO_TIMESPEC(&tv, &pkts[i].send_ts);
        RPC_SEND(rc, pco_tst, tst_s, pkts[i].buf, pkts[i].len, 0);
    }

    return pkts_to_send;
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zft_p iut_s = RPC_NULL;
    rpc_zftl_p iut_zftl = RPC_NULL;
    int tst_s = -1;

    zfts_conn_open_method open_method;
    int iovcnt;
    te_bool done_some;

    rpc_zft_msg msg;
    rpc_iovec iov[MAX_IOVS];

    pkt_descr *pkts = NULL;
    int pkts_num;
    int i;
    int j;
    int k;

    int recv_len = 0;
    int done_len = 0;
    int part_len = 0;
    int prev_part_len = 0;
    int sent_pkts = 0;

    tarpc_timespec ts;
    unsigned int flags;


    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_INT_PARAM(pkts_num);
    TEST_GET_INT_PARAM(iovcnt);
    TEST_GET_BOOL_PARAM(done_some);

    TEST_STEP("Allocate ZF attributes and stack, enabling "
              "@b rx_timestamping.");
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_attr_set_int(pco_iut, attr, "rx_timestamping", 1);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    TEST_STEP("Create TCP Zocket on IUT and TCP socket on Tester, "
              "establish TCP connection according to @p open_method.");

    zfts_establish_tcp_conn_ext2(open_method,
                                 pco_iut, attr, stack,
                                 &iut_zftl, &iut_s, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    TEST_STEP("Enable @c TCP_NODELAY option on the Tester socket to ensure "
              "that TCP packets of specified sizes are sent.");
    rpc_setsockopt_int(pco_tst, tst_s, RPC_TCP_NODELAY, 1);

    TEST_STEP("Send the first bunch of TCP packets from the Tester "
              "socket.");

    pkts = tapi_calloc(pkts_num, sizeof(*pkts));

    sent_pkts += send_packets(pco_tst, pco_iut, tst_s, pkts, pkts_num);

    TEST_STEP("Wait for a while calling @b zf_reactor_perform() in a loop "
              "until it returns non-zero.");
    rpc_zf_wait_for_event(pco_iut, stack);

    TEST_STEP("In a loop call @b zft_zc_recv() until @p pkts_num packets "
              "are received. Choose number of IOVs passed to this "
              "function according to @p iovcnt.");
    TEST_SUBSTEP("After each @b zft_zc_recv() call, for each retrieved IOV "
                 "call @b zft_pkt_get_timestamp() and check that retrieved "
                 "timestamp does not differ too much from the time when "
                 "the packet was sent.");
    TEST_SUBSTEP("If @p done_some is @c TRUE, call "
                 "@b zft_zc_recv_done_some() instead of "
                 "@b zft_zc_recv_done() after each @b zft_zc_recv() "
                 "call, firstly acknowledging only part of the data. Check "
                 "that when unacknowledged part of the same packet is "
                 "retrieved again by the next @b zft_zc_recv() call, "
                 "@b zft_pkt_get_timestamp() reports the same RX timestamp "
                 "for it.");
    TEST_SUBSTEP("If all the previously sent packets were received and "
                 "@p pkts_num packets was not sent yet, wait for a while "
                 "and send another bunch of packets.");
    TEST_SUBSTEP("If @b zft_zc_recv() reports @b pkts_left to be zero when "
                 "some packets are still to be received, call "
                 "@b zf_reactor_perform() in a loop again until it returns "
                 "non-zero.");

    for (i = 0; i < pkts_num; )
    {
        memset(iov, 0, sizeof(iov));
        memset(&msg, 0, sizeof(msg));
        msg.iovcnt = (iovcnt > 0 ? iovcnt : rand_range(1, MAX_IOVS));
        msg.iov = iov;

        rpc_zft_zc_recv(pco_iut, iut_s, &msg, 0);
        if (msg.iovcnt < 0)
        {
            TEST_VERDICT("zft_zc_recv() set msg.iovcnt to a negative "
                         "value");
        }
        else if (msg.iovcnt == 0)
        {
            /*
             * No data was returned - let's wait for another event from
             * zf_reactor_perform()
             */
            rpc_zf_wait_for_event(pco_iut, stack);
            continue;
        }

        for (j = 0; j < msg.iovcnt; j++)
        {
            if (iov[j].iov_len != pkts[i].left_len ||
                memcmp(iov[j].iov_base, pkts[i].cur_pos,
                       pkts[i].left_len) != 0)
            {
                TEST_VERDICT("zft_zc_recv() returned unexpected data");
            }

            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zft_pkt_get_timestamp(pco_iut, iut_s, msg.ptr,
                                            &ts, j, &flags);
            if (rc < 0)
            {
                TEST_VERDICT("zft_pkt_get_timestamp() failed unexpectedly "
                             "with error " RPC_ERROR_FMT,
                             RPC_ERROR_ARGS(pco_iut));
            }

            if (~flags & TARPC_ZF_SYNC_FLAG_CLOCK_SET)
            {
                TEST_VERDICT("NIC clock is not reported to be set by "
                             "zft_pkt_get_timestamp()");
            }

            if (~flags & TARPC_ZF_SYNC_FLAG_CLOCK_IN_SYNC)
            {
                TEST_VERDICT("NIC clock is not reported to be synchronized by "
                             "zft_pkt_get_timestamp()");
            }

            RING("Packet %d: send time " TE_PRINTF_TS_FMT ", "
                 "receive time " TE_PRINTF_TS_FMT,
                 i, TE_PRINTF_TS_VAL(pkts[i].send_ts),
                 TE_PRINTF_TS_VAL(ts));

            if (!pkts[i].recv_ts_set)
            {
                memcpy(&pkts[i].recv_ts, &ts, sizeof(ts));
                pkts[i].recv_ts_set = TRUE;

                if (i > 0)
                {
                    if (ts_cmp(&pkts[i].recv_ts, &pkts[i - 1].recv_ts) < 0)
                    {
                        TEST_VERDICT("The next packet has smaller RX "
                                     "timestamp");
                    }
                }

                if (ts_check_deviation(&pkts[i].recv_ts, &pkts[i].send_ts,
                                       0, TST_PRECISION))
                {
                    TEST_VERDICT("RX timestamp of a packet differs too much "
                                 "from its sending time");
                }
            }
            else
            {
                if (ts_cmp(&pkts[i].recv_ts, &ts) != 0)
                {
                    TEST_VERDICT("Reported RX timestamp for a packet "
                                 "retrieved the second time after "
                                 "zft_zc_recv_done_some() differs from "
                                 "RX timestamp reported for the same "
                                 "packet before");
                }
            }

            i++;
        }

        if (done_some)
        {
            recv_len = rpc_iov_data_len(iov, msg.iovcnt);
            k = i - msg.iovcnt;

            /*
             * Do not split the same packet too many times.
             */
            if (pkts[k].done_some_cnt >= MAX_DONE_SOME_SPLITS)
            {
                done_len = recv_len;
            }
            else
            {
                done_len = rand_range(1, MAX(1, recv_len - 1));

                part_len = 0;
                for (j = 0; j < msg.iovcnt; j++, k++)
                {
                    prev_part_len = part_len;
                    part_len += msg.iov[j].iov_len;
                    if (part_len > done_len)
                    {
                        pkts[k].cur_pos += done_len - prev_part_len;
                        pkts[k].left_len -= done_len - prev_part_len;
                        pkts[k].done_some_cnt++;
                        break;
                    }
                }

                i = k;
            }

            rpc_zft_zc_recv_done_some(pco_iut, iut_s, &msg, done_len);
        }
        else
        {
            rpc_zft_zc_recv_done(pco_iut, iut_s, &msg);
        }

        rpc_release_iov(iov, msg.iovcnt);

        if (i == sent_pkts && i < pkts_num)
        {
            /*
             * Wait for a while to increase timestamp for the next
             * bunch of packets; keep IUT TCP zocket responsive
             * during this time.
             */
            rpc_zf_process_events_long(
                            pco_iut, stack,
                            rand_range(1, ZFTS_WAIT_EVENTS_TIMEOUT));

            sent_pkts += send_packets(pco_tst, pco_iut, tst_s,
                                      pkts + i, pkts_num - i);
        }

        if (i < pkts_num && msg.pkts_left == 0 &&
            !(done_some && done_len < recv_len))
        {
            rpc_zf_wait_for_event(pco_iut, stack);
        }
    }

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    free(pkts);

    TEST_END;
}
