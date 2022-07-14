/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Delegated Sends tests
 */

/**
 * @page zf_ds-simple_ds Sending data with Delegated Sends API
 *
 * @objective Check that data can be sent with Delegated Sends API
 *
 * @param pco_iut             PCO on IUT.
 * @param pco_tst             PCO on Tester.
 * @param iut_addr            Network address on IUT.
 * @param tst_addr            Network address on Tester.
 * @param iut_if              Network interface on IUT.
 * @param tst_if              Network interface on Tester.
 * @param send_before         If @c TRUE, send a packet before starting
 *                            delegated sends.
 * @param send_after          If @c TRUE, send a packet after finishing
 *                            delegated sends.
 * @param ds_packets          How many packets passed to
 *                            @b zf_delegated_send_complete() should be
 *                            actually sent to peer before calling that
 *                            function:
 *                            - @c none
 *                            - @c some
 *                            - @c all
 * @param single_packet       If @c TRUE, only the single packet should be
 *                            sent via Delegated Sends API; otherwise -
 *                            a few packets.
 * @param use_cancel          If @c TRUE, send less data than reserved for
 *                            delegated sends, and call
 *                            @b zf_delegated_send_cancel() after
 *                            @b zf_delegated_send_complete().
 * @param reactor_after_send  If @c TRUE, call zf_reactor_perform() in
 *                            a loop for a while after every delegated
 *                            send.
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "zf_ds/simple_ds"

#include "zf_test.h"
#include "rpc_zf.h"
#include "te_vector.h"
#include "tapi_sockets.h"
#include "tapi_tad.h"
#include "tapi_tcp.h"

/** Maximum size of data sent from IUT by all sending calls */
#define MAX_TOTAL_LEN 10000

/**
 * How long to process events after each delegated send, in ms
 * (if requested).
 */
#define WAIT_AFTER_SEND 100

/**
 * How long to process events before calling recv() at the end, in ms.
 */
#define WAIT_BEFORE_RECV 2000

/**
 * Possible values of "ds_packets" argument telling how many
 * packets passed to zf_delegated_send_complete() should be
 * actually sent before it.
 * */
enum {
    DS_PACKETS_NONE,    /**< Do not send any of the packets passed to
                             zf_delegated_send_complete() */
    DS_PACKETS_SOME,    /**< Send only some of the packets */
    DS_PACKETS_ALL,     /**< Send all the packets */
};

/** List of the values of "ds_packets" argument for TEST_GET_ENUM_PARAM() */
#define DS_PACKETS \
    { "none", DS_PACKETS_NONE },      \
    { "some", DS_PACKETS_SOME },      \
    { "all", DS_PACKETS_ALL }

/** Input and output values of CSAP callback */
typedef struct csap_data {
    /* Input values */
    long int send_gap_start;      /**< If not negative, this is number of
                                       bytes which are expected to be
                                       sent in order */

    /* Output values */

    te_bool pkts_captured;        /**< Set to TRUE on capturing the
                                       first packet */

    uint32_t init_seqn;           /**< Set to the SEQN of the first
                                       packet */
    uint32_t exp_seqn;            /**< Set to the SEQN expected in
                                       the next packet */
    uint32_t total_len;           /**< Total length of data in packets
                                       captured so far (computed as maximum
                                       SEQN + payload length, not taking
                                       into account any gaps) */

    te_bool unexp_seqn;           /**< Set to TRUE when unexpected SEQN
                                       is encountered (before
                                       send_gap_start if it is not
                                       negative, or among all packets) */
    te_bool failed;               /**< Set to TRUE if some failure occurred
                                       when processing captured packets */
} csap_data;

/**
 * CSAP callback to process captured IUT packets.
 *
 * @param pkt         Captured packet.
 * @param user_data   Pointer to csap_data.
 */
static void
csap_cb(asn_value *pkt, void *user_data)
{
#define CB_CHECK_RC(_expr) \
    do {                                                        \
        te_errno _rc = (_expr);                                 \
        if (_rc != 0)                                           \
        {                                                       \
            ERROR(#_expr " failed returning %r", _rc);          \
            data->failed = TRUE;                                \
            goto cleanup;                                       \
        }                                                       \
    } while (0)

    csap_data *data = (csap_data *)user_data;

    uint32_t seqn;
    uint32_t payload_len;
    uint32_t total_len;

    te_string pkt_log = TE_STRING_INIT_STATIC(1024);
    const char *pkt_log_pref = "";
    te_bool pkt_err = FALSE;
    te_bool pkt_warn = FALSE;

    if (data->failed)
        goto cleanup;

    CB_CHECK_RC(asn_read_uint32(pkt, &seqn, "pdus.0.#tcp.seqn.#plain"));
    CB_CHECK_RC(tapi_tcp_get_hdrs_payload_len(pkt, NULL, &payload_len));

    if (!data->pkts_captured)
    {
        if (payload_len > 0)
        {
            ERROR("The first packet should be SYN, but it has non-empty "
                  "payload");
            data->failed = TRUE;
        }

        /*
         * SEQN is incremented here because the first packet should be SYN,
         * and the first SEQN of data packet will be greater by 1.
         */
        data->init_seqn = seqn + 1;
        data->exp_seqn = data->init_seqn;
        data->pkts_captured = TRUE;

        RING("The initial packet was captured");
    }
    else
    {
        if (seqn != data->exp_seqn)
        {
            if (data->send_gap_start < 0 ||
                (long int)(seqn - data->init_seqn) < data->send_gap_start)
            {
                data->unexp_seqn = TRUE;
                pkt_err = TRUE;
                pkt_log_pref = "Unexpected SEQN: ";
            }
            else
            {
                pkt_warn = TRUE;
                pkt_log_pref = "Unexpected SEQN (ignored): ";
            }
        }
        else
        {
            data->exp_seqn = seqn + payload_len;
        }

        total_len = (seqn - data->init_seqn) + payload_len;
        if (total_len > data->total_len)
            data->total_len = total_len;

        te_string_append(&pkt_log, "%sPacket with relative SEQN %u and "
                         "payload length %u was captured",
                         pkt_log_pref, seqn - data->init_seqn, payload_len);
        if (pkt_err)
            ERROR("%s", pkt_log.ptr);
        else if (pkt_warn)
            WARN("%s", pkt_log.ptr);
        else
            RING("%s", pkt_log.ptr);
    }

cleanup:

    asn_free_value(pkt);
}

/**
 * Check packets captured by CSAP.
 *
 * @param ta            TA on which CSAP is created.
 * @param csap          CSAP handle.
 * @param user_data     Pointer to csap_data.
 * @param sent_bytes    Number of bytes actually sent so far.
 * @param stage         Prefix for verdicts in case of failure.
 */
static void
check_csap(const char *ta, csap_handle_t csap,
           csap_data *user_data, unsigned int sent_bytes,
           const char *stage)
{
    tapi_tad_trrecv_cb_data cb_data;

    memset(&cb_data, 0, sizeof(cb_data));
    cb_data.callback = &csap_cb;
    cb_data.user_data = user_data;

    CHECK_RC(tapi_tad_trrecv_get(ta, 0, csap, &cb_data, NULL));
    if (!user_data->pkts_captured)
    {
        TEST_VERDICT("%s: no IUT packets were captured", stage);
    }
    if (user_data->failed)
    {
        TEST_VERDICT("%s: failed to process captured IUT packets",
                     stage);
    }
    if (user_data->unexp_seqn)
    {
        TEST_VERDICT("%s: packets with unexpected SEQN were detected "
                     "before any possible gap in sent data", stage);
    }

    if (user_data->total_len != sent_bytes)
    {
        ERROR("%u bytes were sent in captured packets, while %u should "
              "have been", user_data->total_len, sent_bytes);

        TEST_VERDICT("%s: %s data than expected was found in "
                     "captured packets", stage,
                     (user_data->total_len < sent_bytes ? "less" : "more"));
    }
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    const struct if_nameindex *iut_if = NULL;
    const struct if_nameindex *tst_if = NULL;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p iut_zft = RPC_NULL;
    int tst_s = -1;
    int raw_socket = -1;

    char send_buf[MAX_TOTAL_LEN];
    char recv_buf[MAX_TOTAL_LEN];
    char headers[ZFTS_TCP_HDRS_MAX];
    struct zf_ds ds;
    int len = 0;

    te_vec iovs = TE_VEC_INIT(rpc_iovec);
    rpc_iovec iov;
    rpc_iovec *iov_ptr;
    rpc_iovec raw_iovs[2];

    int i;
    int mss = 0;
    int total_len = 0;
    int max_ds_len = 0;
    int min_ds_len = 0;
    int ds_req_len = 0;
    int ds_rem_len = 0;
    int ds_sent_len = 0;
    int sent_before_complete = 0;
    int ds_sent_num = 0;
    int max_len = 0;

    te_bool first_packet;
    te_bool readable;

    csap_handle_t csap = CSAP_INVALID_HANDLE;
    csap_data user_data;

    te_bool send_before;
    te_bool send_after;
    int ds_packets;
    te_bool single_packet;
    te_bool use_cancel;
    te_bool reactor_after_send;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(iut_if);
    TEST_GET_IF(tst_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(send_before);
    TEST_GET_BOOL_PARAM(send_after);
    TEST_GET_ENUM_PARAM(ds_packets, DS_PACKETS);
    TEST_GET_BOOL_PARAM(single_packet);
    TEST_GET_BOOL_PARAM(use_cancel);
    TEST_GET_BOOL_PARAM(reactor_after_send);

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
    SLEEP(1);

    memset(&user_data, 0, sizeof(user_data));
    user_data.send_gap_start = -1;

    te_fill_buf(send_buf, sizeof(send_buf));

    TEST_STEP("Allocate ZF attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);
    TEST_STEP("Establish TCP connection between a zocket on IUT and "
              "a socket on Tester.");
    zfts_establish_tcp_conn(TRUE, pco_iut, attr, stack, &iut_zft,
                            iut_addr, pco_tst, &tst_s, tst_addr);

    if (ds_packets != DS_PACKETS_NONE)
    {
        TEST_STEP("If @p ds_packets is not @c none, create a raw socket "
                  "on IUT to perform delegated sends.");
        raw_socket = rpc_socket(pco_iut, RPC_AF_PACKET, RPC_SOCK_RAW,
                                RPC_IPPROTO_RAW);
    }

    total_len = 0;
    if (send_before)
    {
        TEST_STEP("If @p send_before is @c TRUE, send some data with "
                  "@b zft_send_single() from IUT.");
        len = rand_range(1, ZFTS_TCP_DATA_MAX);
        rpc_zft_send_single(pco_iut, iut_zft, send_buf, len, 0);
        total_len += len;

        /*
         * ON-13896: call zf_reactor_perform() before completing
         * a delegated reservation.
         */
        rpc_zf_process_events(pco_iut, stack);
    }
    sent_before_complete = total_len;

    memset(&ds, 0, sizeof(ds));
    ds.headers = headers;
    ds.headers_size = sizeof(headers);

    max_ds_len = MAX_TOTAL_LEN - total_len;
    if (send_after)
        max_ds_len -= ZFTS_TCP_DATA_MAX;

    mss = rpc_zft_get_mss(pco_iut, iut_zft);
    if (single_packet)
    {
        min_ds_len = 1;
        max_ds_len = MIN(mss, max_ds_len);
    }
    else
    {
        min_ds_len = 2;
    }

    if (max_ds_len < min_ds_len)
    {
        TEST_FAIL("Failed to compute number of DS bytes to reserve; "
                  "either MAX_TOTAL_LEN or TCP MSS is too small");
    }
    ds_sent_len = rand_range(min_ds_len, max_ds_len);

    ds_req_len = ds_sent_len;
    if (use_cancel)
        ds_req_len += rand_range(1, ds_req_len);

    TEST_STEP("Call @b zf_delegated_send_prepare() on IUT, asking it "
              "to reserve enough bytes to send required number of packets "
              "(and more if @b use_cancel is @c TRUE).");
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, ds_req_len,
                                       0, 0, &ds);

    TEST_STEP("Check that it succeeded and reserved no more bytes "
              "than requested.");
    if (rc != ZF_DELEGATED_SEND_RC_OK)
    {
        TEST_VERDICT("zf_delegated_send_prepare() returned %s",
                     zf_delegated_send_rc_rpc2str(rc));
    }
    if (ds.delegated_wnd > ds_req_len)
    {
        TEST_VERDICT("zf_delegated_send_prepare() set delegated_wnd field "
                     "to a bigger value than requested");
    }
    else if (ds.delegated_wnd < ds_req_len)
    {
        WARN("zf_delegated_send_prepare() set delegated_wnd field to "
             "a smaller value than requested");
        if (ds.delegated_wnd < min_ds_len ||
            (use_cancel && ds.delegated_wnd < min_ds_len + 1))
        {
            TEST_VERDICT("Too few bytes were reserved by "
                         "zf_delegated_send_prepare()");
        }
    }
    if (ds.mss != mss)
    {
        TEST_VERDICT("zf_delegated_send_prepare() set mss field to an "
                     "unexpected value");
    }

    if (use_cancel)
    {
        if (ds_sent_len >= ds.delegated_wnd)
            ds_sent_len = rand_range(min_ds_len, ds.delegated_wnd - 1);
    }
    else
    {
        ds_sent_len = ds.delegated_wnd;
    }

    TEST_STEP("Fill an array of iovecs with buffers to be sent and to "
              "be passed to @b zf_delegated_send_complete(). If "
              "@p single_packet is @c TRUE, it should contain only a "
              "single buffer; otherwise - a few of them.");
    RING("%d bytes will be sent wia Delegated Sends API", ds_sent_len);
    ds_rem_len = ds_sent_len;
    first_packet = TRUE;
    while (ds_rem_len > 0)
    {
        max_len = MIN(ds_rem_len, mss);
        if (!single_packet && first_packet &&
            max_len == ds_rem_len)
        {
            max_len--;
        }

        if (single_packet)
        {
            iov.iov_len = max_len;
        }
        else
        {
            /*
             * Computing it this way avoids creating a "tail"
             * of progressively smaller packets.
             */
            iov.iov_len = rand_range(1, MIN(mss, ds_sent_len));
            if (iov.iov_len > (size_t)max_len)
                iov.iov_len = max_len;
        }

        iov.iov_rlen = iov.iov_len;
        iov.iov_base = send_buf + total_len;
        CHECK_RC(TE_VEC_APPEND(&iovs, iov));

        total_len += iov.iov_len;
        ds_rem_len -= iov.iov_len;
        first_packet = FALSE;
    }

    if (ds_packets != DS_PACKETS_NONE)
    {
        ds_sent_num = te_vec_size(&iovs);
        if (ds_packets == DS_PACKETS_SOME)
            ds_sent_num = rand_range(1, ds_sent_num - 1);

        TEST_STEP("If @p ds_packets is not @c none, send some (if "
                  "@p ds_packets is @c some) or all buffers from the "
                  "iovec array via the raw socket from IUT.");
        TEST_SUBSTEP("Use @b zf_delegated_send_tcp_update() before "
                     "sending and @b zf_delegated_send_tcp_advance() "
                     "after it to update packet headers.");
        TEST_SUBSTEP("Call @b zf_reactor_perform() in a loop for a while "
                     "after each send if @b reactor_after_send is "
                     "@c TRUE.");

        for (i = 0; i < ds_sent_num; i++)
        {
            iov_ptr = (rpc_iovec *)te_vec_get(&iovs, i);
            rpc_zf_delegated_send_tcp_update(pco_iut, &ds,
                                             iov_ptr->iov_len, 1);

            memset(&raw_iovs, 0, sizeof(raw_iovs));
            raw_iovs[0].iov_base = ds.headers;
            raw_iovs[0].iov_len = raw_iovs[0].iov_rlen = ds.headers_len;
            raw_iovs[1].iov_base = iov_ptr->iov_base;
            raw_iovs[1].iov_len = raw_iovs[1].iov_rlen = iov_ptr->iov_len;

            CHECK_RC(tapi_sock_raw_tcpv4_send(pco_iut, raw_iovs, 2,
                                              iut_if->if_index,
                                              raw_socket, TRUE));

            rpc_zf_delegated_send_tcp_advance(pco_iut, &ds,
                                              iov_ptr->iov_len);

            if (reactor_after_send)
                rpc_zf_process_events_long(pco_iut, stack, WAIT_AFTER_SEND);

            sent_before_complete += iov_ptr->iov_len;
        }

        TEST_STEP("If some packets were sent from the raw IUT socket, "
                  "check that CSAP captured them and their SEQNs "
                  "were in order.");
        check_csap(pco_tst->ta, csap, &user_data, sent_before_complete,
                   "Before calling zf_delegated_send_complete()");
    }

    TEST_STEP("Call @b zf_delegated_send_complete() on IUT, passing "
              "to it all the buffers from the iovec array. Check that "
              "it returns sum of their lengths.");
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_complete(pco_iut, iut_zft,
                                        (rpc_iovec *)(iovs.data.ptr),
                                        te_vec_size(&iovs), 0);
    if (rc < 0)
    {
        TEST_VERDICT("zf_delegated_send_complete() failed unexpectedly "
                     "with errno " RPC_ERROR_FMT, RPC_ERROR_ARGS(pco_iut));
    }
    else if (rc != ds_sent_len)
    {
        ERROR("%d bytes are completed instead of %d", rc, ds_sent_len);
        TEST_VERDICT("zf_delegated_send_complete() returned unexpected "
                     "value");
    }

    if (use_cancel)
    {
        TEST_STEP("If @p use_cancel is @c TRUE, call "
                  "@b zf_delegated_send_cancel() on the IUT zocket.");
        rpc_zf_delegated_send_cancel(pco_iut, iut_zft);
    }

    if (send_after)
    {
        TEST_STEP("If @p send_after is @c TRUE, send a packet from "
                  "the IUT zocket with @b zft_send_single().");
        len = rand_range(1, ZFTS_TCP_DATA_MAX);
        rpc_zft_send_single(pco_iut, iut_zft, send_buf + total_len,
                            len, 0);
        total_len += len;
    }

    TEST_STEP("Call @b zf_reactor_perform() for a while.");
    rpc_zf_process_events_long(pco_iut, stack, WAIT_BEFORE_RECV);

    TEST_STEP("Check that the Tester socket is readable.");
    RPC_GET_READABILITY(readable, pco_tst, tst_s, 0);
    if (!readable)
        TEST_VERDICT("Tester socket is not readable");

    TEST_STEP("Receive data on the Tester socket, check that all the "
              "data sent from IUT via various send calls and data "
              "passed to @b zf_delegated_send_complete() without "
              "sending was received.");
    RPC_AWAIT_ERROR(pco_tst);
    rc = rpc_recv(pco_tst, tst_s, recv_buf, sizeof(recv_buf), 0);
    if (rc < 0)
    {
        TEST_VERDICT("recv() failed on Tester with errno %r",
                     RPC_ERRNO(pco_tst));
    }
    else if (rc != total_len)
    {
        ERROR("%d bytes were received instead of %d", rc, total_len);
        TEST_VERDICT("Unexpected number of bytes was received on Tester");
    }
    else if (memcmp(send_buf, recv_buf, total_len) != 0)
    {
        TEST_VERDICT("Data received on Tester does not match data sent "
                     "from IUT");
    }

    if (ds_packets != DS_PACKETS_ALL && send_after)
        user_data.send_gap_start = sent_before_complete;

    TEST_STEP("Check packets captured by the CSAP on Tester. If "
              "@b ds_packets is not @c all and @b send_after "
              "is @c TRUE, check that all the packets actually "
              "sent before @b zf_delegated_send_complete() "
              "were sent in order and were not retransmitted. "
              "Otherwise check that this is true for all the "
              "captured packets.");
    check_csap(pco_tst->ta, csap, &user_data, total_len,
               "At the end of the test");

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_CLOSE(pco_iut, raw_socket);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_CHECK_RC(tapi_tad_csap_destroy(pco_tst->ta, 0, csap));

    te_vec_free(&iovs);

    TEST_END;
}
