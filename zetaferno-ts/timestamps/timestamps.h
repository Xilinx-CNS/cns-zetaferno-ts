/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Zetaferno Direct API Test Suite
 *
 * Common macros and functions for timestamps API.
 *
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#ifndef __ZETAFERNO_TS_TIMESTAMPS_H__
#define __ZETAFERNO_TS_TIMESTAMPS_H__

#include "zf_test.h"
#include "te_vector.h"
#include "tapi_tad.h"
#include "tapi_tcp.h"

/** Default timestamps precision in us. */
#define TS_DEF_PRECISION 500000

/**
 * Convert struct timeval or tarpc_timeval to milliseconds since epoch.
 *
 * @param _tv     Value to convert.
 */
#define TS_TIMEVAL2MS(_tv) \
    ((_tv).tv_sec * (int64_t)1000 + (_tv).tv_usec / (int64_t)1000)

/**
 * Convert struct timespec or tarpc_timespec to milliseconds since epoch.
 *
 * @param _ts     Value to convert.
 */
#define TS_TIMESPEC2MS(_ts) \
    ((_ts).tv_sec * (int64_t)1000 + (_ts).tv_nsec / (int64_t)1000000)

/**
 * Compare two tarpc_timespec structures values.
 *
 * @param first      First timestamp
 * @param second     Second timestamp
 *
 * @return Comparison result.
 *
 * @retval @c 1     if @p first is greater than @p second
 * @retval @c -1    if @p first is less than @p second
 * @retval @c 0     if @p first and @p second are equal
 */
static inline int ts_cmp(tarpc_timespec *first, tarpc_timespec *second)
{
    if (first->tv_sec > second->tv_sec)
        return 1;
    if (first->tv_sec < second->tv_sec)
        return -1;

    if (first->tv_nsec > second->tv_nsec)
        return 1;
    if (first->tv_nsec < second->tv_nsec)
        return -1;

    return 0;
}

/**
 * Calculate absolute difference in microseconds between two tarpc_timespec
 * structures.
 *
 * @param first      First timestamp
 * @param second     Second timestamp
 *
 * @return Difference in microseconds
 */
static inline int64_t ts_timespec_diff_us(tarpc_timespec *first,
                                          tarpc_timespec *second)
{
    return llabs((first->tv_sec - second->tv_sec) * (int64_t)1000000 +
                 (first->tv_nsec - second->tv_nsec) / (int64_t)1000);
}

/**
 * Check that difference between two timestamps is not more then
 * allowed @p deviation.
 *
 * @param first      First timestamp
 * @param second     Second timestamp
 * @param deviation  Expected difference in microseconds
 * @param precision  Allowed precision in microseconds
 *
 * @return @c TRUE if the deviation does not satisfy allowed conditions,
 *         @c FALSE otherwise.
 */
static inline te_bool ts_check_deviation(tarpc_timespec *first,
                                         tarpc_timespec *second,
                                         unsigned int deviation,
                                         unsigned int precision)
{
    int64_t diff = ts_timespec_diff_us(first, second);
    int64_t min_diff = (precision < deviation ? deviation - precision : 0);
    int64_t max_diff = deviation + precision;

    return (diff < min_diff || diff > max_diff);
}

/** Description of a sent UDP packet */
typedef struct ts_udp_tx_pkt_descr {
    size_t len;                 /**< Payload length */
    tarpc_timespec send_ts;     /**< Send timestamp */
} ts_udp_tx_pkt_descr;

/**
 * Obtain all available reports about sent packets by repeatedly calling
 * @b zfut_get_tx_timestamps() or @b zft_get_tx_timestamps().
 *
 * @param rpcs          RPC server.
 * @param z             TCP or UDP TX zocket.
 * @param reports_vec   TE vector to which to append retrieved reports.
 * @param min_num       Minimum number of reports to retrieve by a single
 *                      function call.
 * @param max_num       Maximum number of reports to retrieve by a single
 *                      function call.
 * @param single_call   If @c TRUE, call @b zfut_get_tx_timestamps()
 *                      only the single time.
 */
extern void ts_get_tx_reports(rcf_rpc_server *rpcs, rpc_ptr z,
                              te_vec *reports_vec,
                              int min_num, int max_num,
                              te_bool single_call);

/**
 * Check that obtained reports about sent UDP packets match the sent
 * packets.
 *
 * @param reports_vec         Vector of reports.
 * @param pkts_vec            Vector of packets descriptions.
 * @param precision           Precision when comparing timestamps,
 *                            in microseconds.
 * @param dropped_cnt         Where to save number of packet reports with
 *                            @c ZF_PKT_REPORT_DROPPED flag set.
 */
extern void ts_zfut_check_tx_reports(te_vec *reports_vec, te_vec *pkts_vec,
                                     int precision,
                                     unsigned int *dropped_cnt);

/** Auxiliary data passed to CSAP callback processing TCP TX packets */
typedef struct ts_tcp_tx_csap_data {
    /* Input fields - should be set before processing captured packets */

    te_vec *reports;            /**< Array of packet reports to check
                                     against captured packets */

    int64_t ms_adjust;          /**< Adjustment to match Tester clocks to
                                     IUT clocks, in ms */

    te_bool msg_more;           /**< If @c TRUE, @c MSG_MORE flag was used
                                     when sending data */
    int mss;                    /**< IUT MSS (used to check how @c MSG_MORE
                                     works if @b msg_more is @c TRUE;
                                     otherwise ignored) */

    /*
     * Output and auxiliary fields - should be initialized to zeroes
     * before processing captured packets. On output these fields
     * should be investigated only if "failed" field is set to FALSE
     * (except for "pkts_cnt" field which is always computed).
     */

    unsigned int pkts_cnt;      /**< Number of packets which are not
                                     ACKs with no payload. */

    unsigned int cur_id;        /**< Index of the current packet report */
    unsigned int cur_offset;    /**< Current offset in the stream of TCP
                                     data */
    unsigned int max_offset;    /**< Maximum offset in the stream of TCP
                                     data */
    unsigned int max_data_off;  /**< Maximum offset of a packet with
                                     data in the stream of TCP data */
    uint32_t init_seqn;         /**< Initial SEQN got from SYN packet */

    te_bool syn_encountered;    /**< Set to @c TRUE when SYN packet
                                     is encountered */
    te_bool syn_start_verdict;  /**< Set to @c TRUE when verdict about
                                     @c UINT_MAX as value of the @b start
                                     field for SYN packet report is printed
                                     (to avoid printing it again) */
    te_bool fin_encountered;    /**< Set to @c TRUE when FIN packet
                                     is encountered */

    tarpc_timespec prev_ts;     /**< Timestamp from the previous packet
                                     report */

    te_bool less_mss_detected;  /**< Will be set to @c TRUE if a packet
                                     with payload less than MSS was
                                     detected - only if @b msg_more
                                     is @c TRUE */
    unsigned int first_less_mss; /**< Will be set to offset of the first
                                      data packet with payload less than
                                      MSS, if @b msg_more is @c TRUE */

    unsigned int drop_pkts;     /**< Number of packets skipped since the
                                     last packet report because the next
                                     reports are dropped */
    unsigned int drop_cnt;      /**< Number of times a report with
                                     @c DROPPED flag was encountered */
    unsigned int retrans_cnt;   /**< Number of times a report with
                                     @c TCP_RETRANS flag was
                                     encountered */
    unsigned int syn_retrans;   /**< Number of times SYN retransmit
                                     was encountered */
    unsigned int fin_retrans;   /**< Number of times FIN retransmit
                                     was encountered */
    unsigned int data_retrans;  /**< Number of times data retransmit
                                     was encountered */

    te_bool unexp_fail;         /**< Set to @c TRUE if unexpected
                                     failure happened (for which
                                     no specific verdict is printed) */
    te_bool failed;             /**< Set to @c TRUE if processing of
                                     captured packets failed */
} ts_tcp_tx_csap_data;

/**
 * Callback for processing packets captured by CSAP.
 *
 * @param pkt           Description of captured TCP packet.
 * @param user_data     Pointer to @ref ts_tcp_tx_csap_data.
 */
extern void ts_tcp_tx_csap_cb(asn_value *pkt, void *user_data);

/**
 * Compute time adjustment (in ms) between clocks on two hosts.
 *
 * @param rpcs1     RPC server on the first host.
 * @param rpcs2     RPC server on the second host.
 *
 * @return Difference in ms between clocks on the first host and
 *         clocks on the second host.
 */
extern int64_t ts_get_clock_adjustment(rcf_rpc_server *rpcs1,
                                       rcf_rpc_server *rpcs2);

/**
 * Allocate ZF muxer, add the tested zocket to the muxer set
 * with @c EPOLLERR event (which should be reported when
 * TX timestamp is available).
 *
 * @param rpcs              RPC server.
 * @param stack             ZF stack.
 * @param z                 Tested zocket.
 * @param muxer_set         Where to save RPC pointer to a muxer set.
 * @param waitable          Where to save RPC pointer to a ZF waitable
 *                          object.
 */
extern void ts_tx_muxer_configure(rcf_rpc_server *rpcs,
                                  rpc_zf_stack_p stack,
                                  rpc_ptr z,
                                  rpc_zf_muxer_set_p *muxer_set,
                                  rpc_zf_waitable_p *waitable);

/**
 * Check events reported by a muxer configured with
 * ts_tx_muxer_configure().
 *
 * @param rpcs              RPC server.
 * @param muxer_set         ZF muxer set.
 * @param z                 Tested zocket.
 * @param timeout           Timeout (in ms).
 * @param events            Events (either @c 0 if no events are expected,
 *                          or @c RPC_EPOLLERR).
 * @param stage             String to print in verdicts in case of failure.
 */
extern void ts_tx_muxer_check(rcf_rpc_server *rpcs,
                              rpc_zf_muxer_set_p muxer_set,
                              rpc_ptr z, int timeout, int events,
                              const char *stage);

#endif /* __ZETAFERNO_TS_TIMESTAMPS_H__ */
