/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Zetaferno Direct API Test Suite
 *
 * Implementation of common functions for timestamps tests.
 *
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#include "timestamps.h"
#include "tapi_rpc_internal.h"

/**
 * Check whether zocket is UDP TX one or not.
 *
 * @param rpcs        RPC server.
 * @param z           RPC pointer to the zocket.
 *
 * @return @c TRUE if the zocket is UDP TX zocket, @c FALSE otherwise.
 */
static te_bool
tx_zocket_is_udp(rcf_rpc_server *rpcs, rpc_ptr z)
{
    const char *ptr_ns = NULL;

    ptr_ns = tapi_rpc_namespace_get(rpcs, z);
    if (ptr_ns == NULL)
        TEST_FAIL("Failed to get namespace of ZF zocket RPC pointer");
    if (strcmp(ptr_ns, RPC_TYPE_NS_ZFUT) == 0)
        return TRUE;

    return FALSE;
}

/* See description in timestamps.h */
void
ts_get_tx_reports(rcf_rpc_server *rpcs, rpc_ptr z,
                  te_vec *reports_vec,
                  int min_num, int max_num,
                  te_bool single_call)
{
    tarpc_zf_pkt_report *reports;
    int rc;
    int reports_num;

    te_bool udp;
    char *func_name;

    udp = tx_zocket_is_udp(rpcs, z);
    if (udp)
        func_name = "zfut_get_tx_timestamps";
    else
        func_name = "zft_get_tx_timestamps";

    reports = tapi_calloc(max_num, sizeof(*reports));

    while (TRUE)
    {
        reports_num = rand_range(min_num, max_num);
        RPC_AWAIT_ERROR(rpcs);
        if (udp)
        {
            rc = rpc_zfut_get_tx_timestamps(rpcs, z, reports,
                                            &reports_num);
        }
        else
        {
            rc = rpc_zft_get_tx_timestamps(rpcs, z, reports,
                                           &reports_num);
        }

        if (rc < 0)
        {
            free(reports);
            TEST_VERDICT("%s() failed with error " RPC_ERROR_FMT,
                         func_name, RPC_ERROR_ARGS(rpcs));
        }
        else if (reports_num < 0)
        {
            free(reports);
            TEST_VERDICT("%s() returned negative number of reports",
                         func_name);
        }
        else if (reports_num == 0)
        {
            break;
        }

        CHECK_RC(te_vec_append_array(reports_vec, reports, reports_num));

        if (single_call)
            break;
    }

    free(reports);
}

/* See description in timestamps.h */
void
ts_zfut_check_tx_reports(te_vec *reports_vec, te_vec *pkts_vec,
                         int precision, unsigned int *dropped_cnt)
{
#define PKT_REPORT_ERROR_MSG \
    do {                                                            \
        ERROR("Error found in the packet report %u with start=%u",  \
              i, report->start);                                    \
    } while (0)

    unsigned int i;
    tarpc_zf_pkt_report *report = NULL;
    ts_udp_tx_pkt_descr *pkt = NULL;
    tarpc_timespec *prev_ts = NULL;
    int prev_start = -1;
    unsigned int exp_flags = 0;
    unsigned int dropped = 0;

    if (te_vec_size(reports_vec) > te_vec_size(pkts_vec))
        TEST_VERDICT("Too many packet reports were retrieved");
    if (te_vec_size(pkts_vec) == 0)
        return;

    for (i = 0; i < te_vec_size(reports_vec); i++)
    {
        report = (tarpc_zf_pkt_report *)te_vec_get(reports_vec, i);
        if (prev_ts != NULL)
        {
            if (ts_cmp(&report->timestamp, prev_ts) < 0)
            {
                PKT_REPORT_ERROR_MSG;
                TEST_VERDICT("The next packet report has smaller "
                             "timestamp");
            }
        }

        exp_flags = (TARPC_ZF_PKT_REPORT_CLOCK_SET |
                     TARPC_ZF_PKT_REPORT_IN_SYNC);

        if (prev_start >= 0 && report->start <= (unsigned int)prev_start)
        {
            PKT_REPORT_ERROR_MSG;
            TEST_VERDICT("The next packet report has not greater "
                         "'start' field");
        }
        else if (report->start >= te_vec_size(pkts_vec))
        {
            PKT_REPORT_ERROR_MSG;
            TEST_VERDICT("Packet report has too big 'start' field");
        }

        if (report->start - prev_start > 1)
        {
            dropped++;
            exp_flags |= TARPC_ZF_PKT_REPORT_DROPPED;
            if (!(report->flags & TARPC_ZF_PKT_REPORT_DROPPED))
            {
                PKT_REPORT_ERROR_MSG;
                TEST_VERDICT("Report for some packet(s) is missed but "
                             "no DROPPED flag is set");
            }
        }
        else
        {
            if (report->flags & TARPC_ZF_PKT_REPORT_DROPPED)
            {
                TEST_VERDICT("DROPPED flag is set in a packet report "
                             "before which no reports are dropped");
            }
        }

        if (report->flags != exp_flags)
        {
            PKT_REPORT_ERROR_MSG;
            TEST_VERDICT("Packet report has flags %s instead of %s",
                         zf_pkt_report_flags_rpc2str(report->flags),
                         zf_pkt_report_flags_rpc2str(exp_flags));
        }

        pkt = (ts_udp_tx_pkt_descr *)te_vec_get(pkts_vec, report->start);
        if (report->bytes != pkt->len)
        {
            ERROR("Packet report %u with start=%u has bytes=%u, while "
                  "payload length of the packet is %u",
                  i, report->start, report->bytes,
                  (unsigned int)(pkt->len));
            TEST_VERDICT("'bytes' field of the packet report does not "
                         "match length of payload in the sent packet");
        }

        if (ts_check_deviation(&report->timestamp, &pkt->send_ts, 0,
                               precision))
        {
            PKT_REPORT_ERROR_MSG;
            TEST_VERDICT("Timestamp from the packet report does not "
                         "match sending time of the packet");
        }

        prev_ts = &report->timestamp;
        prev_start = report->start;
    }

    if (prev_start < 0 ||
        (unsigned int)prev_start < te_vec_size(pkts_vec) - 1)
    {
        TEST_VERDICT("Not all the expected packet reports were "
                     "retrieved");
    }

    *dropped_cnt = dropped;
#undef PKT_REPORT_ERROR_MSG
}

/* See description in timestamps.h */
void
ts_tcp_tx_csap_cb(asn_value *pkt, void *user_data)
{
#define CB_CHECK_RC(_expr) \
    do {                                                        \
        te_errno _rc = (_expr);                                 \
        if (_rc != 0)                                           \
        {                                                       \
            ERROR(#_expr " failed returning %r", _rc);          \
            data->failed = TRUE;                                \
            if (!data->unexp_fail)                              \
            {                                                   \
                ERROR_VERDICT("Unexpected failure happened "    \
                              "when processing captured "       \
                              "packets");                       \
            }                                                   \
            data->unexp_fail = TRUE;                            \
            goto cleanup;                                       \
        }                                                       \
    } while (0)

#define CB_ERROR(_args...) \
    do {                          \
        ERROR_VERDICT(_args);     \
        data->failed = TRUE;      \
        goto cleanup;             \
    } while (0)

    ts_tcp_tx_csap_data *data = (ts_tcp_tx_csap_data *)user_data;
    tarpc_zf_pkt_report *report;
    unsigned int exp_flags;
    uint32_t flags;
    uint32_t seqn;
    unsigned int payload_len;
    struct timeval tv;
    int64_t tx_ts_ms;
    int64_t rx_ts_ms;
    tarpc_timespec rx_ts;

    CB_CHECK_RC(asn_read_uint32(pkt, &flags, "pdus.0.#tcp.flags"));
    CB_CHECK_RC(asn_read_uint32(pkt, &seqn, "pdus.0.#tcp.seqn.#plain"));
    CB_CHECK_RC(tapi_tcp_get_hdrs_payload_len(pkt, NULL, &payload_len));
    CB_CHECK_RC(tapi_tad_get_pkt_rx_ts(pkt, &tv));

    rx_ts_ms = TS_TIMEVAL2MS(tv) + data->ms_adjust;
    rx_ts.tv_sec = rx_ts_ms / 1000LLU;
    rx_ts.tv_nsec = (rx_ts_ms % 1000LLU) * 1000000LLU;

    RING("Packet with SEQN %u, payload length %u and flags 0x%x was "
         "captured at " TE_PRINTF_TS_FMT, seqn, payload_len, flags,
         TE_PRINTF_TS_VAL(rx_ts));

    /*
     * Packets with zero payload (like ACK to SYN-ACK) are ignored
     * unless it is SYN or FIN.
     */
    if (!(flags & (TCP_SYN_FLAG | TCP_FIN_FLAG)) && payload_len == 0)
        goto cleanup;

    data->pkts_cnt++;

    if (data->failed)
        goto cleanup;

    if (data->cur_id >= te_vec_size(data->reports))
    {
        CB_ERROR("CSAP captured more packets than there are packet "
                 "reports");
    }

    report = (tarpc_zf_pkt_report *)te_vec_get(data->reports,
                                               data->cur_id);

    if (report->flags & TARPC_ZF_PKT_REPORT_TCP_RETRANS)
    {
        data->retrans_cnt++;
        if (report->flags & TARPC_ZF_PKT_REPORT_TCP_SYN)
            data->syn_retrans++;
        else if (report->flags & TARPC_ZF_PKT_REPORT_TCP_FIN)
            data->fin_retrans++;
        else
            data->data_retrans++;
    }

    if (data->cur_id > 0 && ts_cmp(&data->prev_ts, &report->timestamp) > 0)
    {
        ERROR("Report %u has smaller timestamp than the previous one",
              data->cur_id);
        CB_ERROR("The next packet report has smaller TX timestamp");
    }

    exp_flags = TARPC_ZF_PKT_REPORT_CLOCK_SET | TARPC_ZF_PKT_REPORT_IN_SYNC;
    if (flags & TCP_SYN_FLAG)
    {
        if (!data->syn_encountered)
        {
            data->init_seqn = seqn;
        }
        else
        {
            if (data->init_seqn != seqn)
            {
                CB_ERROR("Another SYN with a different SEQN "
                         "is encountered");
            }
            exp_flags |= TARPC_ZF_PKT_REPORT_TCP_RETRANS;
        }

        data->cur_offset = 0;
        exp_flags |= TARPC_ZF_PKT_REPORT_TCP_SYN;
        data->syn_encountered = TRUE;
    }
    else if (data->cur_id == 0)
    {
        CB_ERROR("The first packet is not SYN");
    }
    else
    {
        /*
         * SYN occupies 1 SEQN though it does not carry any data,
         * take it into account when calculating data offset for
         * the next packets.
        */
        data->cur_offset = seqn - data->init_seqn - 1;
    }

    if (data->fin_encountered &&
        (data->cur_offset + payload_len > data->max_offset ||
         (data->cur_offset == data->max_offset &&
          !(flags & TCP_FIN_FLAG))))
    {
        CB_ERROR("Another packet was captured after FIN packet");
    }

    if (data->cur_offset < data->max_offset)
        exp_flags |= TARPC_ZF_PKT_REPORT_TCP_RETRANS;

    data->max_offset = MAX(data->cur_offset + payload_len,
                           data->max_offset);
    if (payload_len > 0)
    {
        data->max_data_off = MAX(data->max_data_off,
                                 data->cur_offset);
    }

    if (flags & TCP_FIN_FLAG)
    {
        exp_flags |= TARPC_ZF_PKT_REPORT_TCP_FIN;

        if (data->fin_encountered)
            exp_flags |= TARPC_ZF_PKT_REPORT_TCP_RETRANS;

        data->fin_encountered = TRUE;
    }

    if (!(flags & (TCP_FIN_FLAG | TCP_SYN_FLAG)) && data->msg_more &&
        (int)payload_len < data->mss && !data->less_mss_detected)
    {
        data->first_less_mss = data->cur_offset;
        data->less_mss_detected = TRUE;
    }

    if (report->flags & TARPC_ZF_PKT_REPORT_DROPPED)
    {
        /*
         * FIXME: this code may not work well if TCP
         * retransmit is encountered. However to fix
         * this, probably switching to a different
         * approach is required where all the packets
         * are obtained from CSAP firstly and then
         * are somehow matched to packet reports,
         * instead of matching packet by packet.
         */

        if (data->cur_offset < report->start)
        {
            /* Skip a packet for which report was dropped */
            data->drop_pkts++;
            goto cleanup;
        }
        if (data->drop_pkts == 0)
        {
            CB_ERROR("DROPPED flag is set but no other reports are missed "
                     "before the current report");
        }

        data->drop_pkts = 0;
        data->drop_cnt++;
        exp_flags |= TARPC_ZF_PKT_REPORT_DROPPED;
    }

    if (report->flags != exp_flags)
    {
        ERROR("Packet report %u has unexpected flags", data->cur_id);
        CB_ERROR("Packet report has unexpected flags %s instead of %s",
                 zf_pkt_report_flags_rpc2str(report->flags),
                 zf_pkt_report_flags_rpc2str(exp_flags));
    }

    if (data->cur_offset != report->start)
    {
        ERROR("Packet report %u has unexpected 'start' field value %u "
              "instead of %u", data->cur_id, report->start,
              data->cur_offset);
        if ((flags & TCP_SYN_FLAG) && report->start == UINT_MAX)
        {
            if (!data->syn_start_verdict)
            {
                RING_VERDICT("'start' field of the report of "
                             "SYN packet is set to UINT_MAX "
                             "instead of zero");
                data->syn_start_verdict = TRUE;
            }
        }
        else
        {
            CB_ERROR("Packet report has unexpected 'start' field value");
        }
    }

    if (payload_len != report->bytes)
    {
        ERROR("Packet report %u has unexpected 'bytes' field value %u "
              "instead of %u", data->cur_id, report->bytes,
              payload_len);
        CB_ERROR("Packet report has unexpected value of 'bytes' field");
    }

    tx_ts_ms = TS_TIMESPEC2MS(report->timestamp);
    if (llabs(tx_ts_ms - rx_ts_ms) > TS_DEF_PRECISION)
    {
        ERROR("Packet report %u has timestamp " TE_PRINTF_TS_FMT
              ", RX timestamp is " TE_PRINTF_TS_FMT,
              data->cur_id,
              TE_PRINTF_TS_VAL(report->timestamp),
              TE_PRINTF_TS_VAL(rx_ts));
        CB_ERROR("TX timestamp of a packet report does not match RX "
                 "timestamp reported by CSAP");
    }

    memcpy(&data->prev_ts, &report->timestamp, sizeof(data->prev_ts));
    data->cur_id++;

cleanup:

    asn_free_value(pkt);
#undef CB_CHECK_RC
#undef CB_ERROR
}

/* See description in timestamps.h */
int64_t
ts_get_clock_adjustment(rcf_rpc_server *rpcs1,
                        rcf_rpc_server *rpcs2)
{
    tarpc_timeval tv1;
    tarpc_timeval tv2;
    int64_t ms_adjust;

    /*
     * Clocks on different hosts may be not synchronized, so
     * adjustment is computed to match RX timestamps from CSAP
     * to TX timestamps from zft_get_tx_timestamps().
     */
    rpc_gettimeofday(rpcs1, &tv1, NULL);
    rpc_gettimeofday(rpcs2, &tv2, NULL);
    ms_adjust = TS_TIMEVAL2MS(tv1) - TS_TIMEVAL2MS(tv2);
    RING("Adjustment between clocks on %s and %s is %lld ms",
         rpcs1->name, rpcs2->name, (long long int)ms_adjust);

    return ms_adjust;
}

/* See description in timestamps.h */
void
ts_tx_muxer_configure(rcf_rpc_server *rpcs, rpc_zf_stack_p stack, rpc_ptr z,
                      rpc_zf_muxer_set_p *muxer_set,
                      rpc_zf_waitable_p *waitable)
{
    te_bool udp;

    udp = tx_zocket_is_udp(rpcs, z);

    rpc_zf_muxer_alloc(rpcs, stack, muxer_set);
    if (udp)
        *waitable = rpc_zfut_to_waitable(rpcs, z);
    else
        *waitable = rpc_zft_to_waitable(rpcs, z);

    rpc_zf_muxer_add_simple(rpcs, *muxer_set, *waitable, z, RPC_EPOLLERR);
}

/* See description in timestamps.h */
void
ts_tx_muxer_check(rcf_rpc_server *rpcs, rpc_zf_muxer_set_p muxer_set,
                  rpc_ptr z, int timeout, int events,
                  const char *stage)
{
    zfts_zock_descr zock_descrs[] = {
                  { &z, "the tested zocket" },
                  { NULL, "" }
      };

    if (events != 0)
    {
        ZFTS_CHECK_MUXER_EVENTS(rpcs, muxer_set, stage, timeout,
                                zock_descrs, { events, { .u32 = z } });
    }
    else
    {
        zfts_muxer_wait_check_no_evts(rpcs, muxer_set, timeout, stage);
    }
}
