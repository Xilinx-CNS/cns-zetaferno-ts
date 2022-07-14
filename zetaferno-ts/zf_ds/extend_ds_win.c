/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Delegated Sends tests
 */

/**
 * @page zf_ds-extend_ds_win Extending window for delegated sends
 *
 * @objective Check that @b zf_delegated_send_prepare() can be called
 *            the second time to extend the window reserved for
 *            delegated sends before all bytes in the currently reserved
 *            window are completed or cancelled.
 *
 * @param env                 Testing environment:
 *                            - @ref arg_types_peer2peer_gw
 * @param second_rc           What should the second
 *                            @b zf_delegated_send_prepare() call return:
 *                            - @c ok
 *                            - @c nowin
 *                            - @c nocwin
 * @param ds_send             If @c TRUE, actually send data reserved
 *                            for delegated sends over a RAW socket;
 *                            otherwise let ZF transmit it after
 *                            completion.
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "zf_ds/extend_ds_win"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_tcp.h"

#include "zf_ds_test.h"

#include "tapi_route_gw.h"

/*
 * Minimum number of bytes reserved with the first
 * zf_delegated_send_prepare() call.
 */
#define FIRST_SIZE_MIN 3000

/*
 * Maximum number of bytes reserved with the first
 * zf_delegated_send_prepare() call.
 */
#define FIRST_SIZE_MAX 6000

/*
 * Maximum number of bytes reserved with the second
 * zf_delegated_send_prepare() call.
 */
#define SECOND_SIZE_MAX 10000

/* Maximum number of bytes to send at once */
#define MAX_SIZE MAX(FIRST_SIZE_MAX, SECOND_SIZE_MAX)

/** How long to wait until congestion window reduction, in ms */
#define WAIT_CWIN_REDUCED 3000

/* Possible values of "second_rc" parameter */
#define DS_PREPARE_RC \
    { "ok", ZF_DELEGATED_SEND_RC_OK },          \
    { "nowin", ZF_DELEGATED_SEND_RC_NOWIN },    \
    { "nocwin", ZF_DELEGATED_SEND_RC_NOCWIN }

/*
 * How long to wait for ZF to send a new packet before checking
 * again whether it is received, in ms.
 */
#define WAIT_PACKET_TIME 100

/*
 * How many times to try to receive all the expected data from IUT
 * before giving up.
 */
#define MAX_RECV_ATTEMPTS 100

/*
 * Maximum length of payload when a single packet should be sent
 * (CSAP does not currently advertise TCP MSS, so IUT assumes the
 * minimum guaranteed size of 536 bytes).
 */
#define SINGLE_PKT_LEN 500

/**
 * Receive and check data sent from IUT.
 *
 * @param pco_iut               RPC server on IUT.
 * @param stack                 ZF stack.
 * @param tcp_conn              TCP socket emulation on Tester.
 * @param recv_dbuf             te_dbuf to use for data receiving.
 * @param sent_dbuf             te_dbuf with sent data.
 * @param ack_last_pkt          If TRUE, all received packets should be
 *                              acked. Otherwise the last received packet
 *                              should not be acked.
 * @param msg                   Message to print in verdicts.
 */
static void
receive_data(rcf_rpc_server *pco_iut, rpc_zf_stack_p stack,
             tapi_tcp_handler_t tcp_conn, te_dbuf *recv_dbuf,
             te_dbuf *sent_dbuf, te_bool ack_last_pkt,
             const char *msg)
{
    int i;
    size_t prev_len = recv_dbuf->len;

    for (i = 0; i < MAX_RECV_ATTEMPTS; i++)
    {
        rpc_zf_process_events_long(pco_iut, stack, WAIT_PACKET_TIME);
        CHECK_RC(tapi_tcp_recv_data(
                          tcp_conn, 0,
                          (ack_last_pkt ? TAPI_TCP_AUTO : TAPI_TCP_QUIET),
                          recv_dbuf));

        if (!ack_last_pkt && recv_dbuf->len > prev_len &&
            recv_dbuf->len < sent_dbuf->len)
        {
            CHECK_RC(tapi_tcp_send_ack(tcp_conn,
                                       tapi_tcp_next_ackn(tcp_conn)));
            prev_len = recv_dbuf->len;
        }

        if (recv_dbuf->len >= sent_dbuf->len)
            break;
    }

    RING("%d bytes were received, %d were expected",
         (int)(recv_dbuf->len), (int)(sent_dbuf->len));

    if (i == MAX_RECV_ATTEMPTS)
    {
        TEST_VERDICT("%s: failed to receive all the expected data", msg);
    }
    else if (recv_dbuf->len > sent_dbuf->len)
    {
        TEST_VERDICT("%s: more data than expected was received", msg);
    }
    else if (memcmp(recv_dbuf->ptr, sent_dbuf->ptr,
                    sent_dbuf->len) != 0)
    {
        TEST_VERDICT("%s: received data does not match sent data", msg);
    }

    te_dbuf_reset(sent_dbuf);
    te_dbuf_reset(recv_dbuf);
}

int
main(int argc, char *argv[])
{
    TAPI_DECLARE_ROUTE_GATEWAY_PARAMS;

    tapi_route_gateway gw;

    const struct sockaddr *gw_tst_lladdr;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    rpc_zftl_p iut_zftl = RPC_NULL;
    int raw_socket = -1;

    struct zf_ds ds;
    char headers[ZFTS_TCP_HDRS_MAX];
    char send_buf[MAX_SIZE];
    int first_size;
    int second_size;
    int first_send;
    int max_first_send;
    int cong_wnd_override = 0;
    int remained_ds = 0;
    int saved_window = 0;

    te_dbuf sent_data = TE_DBUF_INIT(0);
    te_dbuf recv_data = TE_DBUF_INIT(0);
    tapi_tcp_handler_t tcp_conn = 0;
    tapi_tcp_pos_t init_ackn;

    int second_rc;
    te_bool ds_send;

    rpc_iovec *iov = NULL;
    int iov_num;

    TEST_START;
    TAPI_GET_ROUTE_GATEWAY_PARAMS;
    TEST_GET_LINK_ADDR(gw_tst_lladdr);
    TEST_GET_ENUM_PARAM(second_rc, DS_PREPARE_RC);
    TEST_GET_BOOL_PARAM(ds_send);

    /* Do not print out packets captured by CSAP. */
    rcf_tr_op_log(FALSE);

    TEST_STEP("Configure routing between IUT and Tester via a "
              "gateway host.");
    TAPI_INIT_ROUTE_GATEWAY(gw);
    CHECK_RC(tapi_route_gateway_configure(&gw));

    TEST_STEP("Configure neighbor entry with @p alien_link_addr for "
              "@p tst_addr on the gateway, so that only a CSAP on "
              "Tester will capture IUT packets and respond to "
              "them.");
    CHECK_RC(tapi_route_gateway_break_gw_tst(&gw));

    CFG_WAIT_CHANGES;

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, create RAW socket on IUT to "
                  "send data from it.");
        raw_socket = rpc_socket(pco_iut, RPC_AF_PACKET, RPC_SOCK_RAW,
                                RPC_IPPROTO_RAW);
    }

    TEST_STEP("Allocate ZF attributes and stack on IUT.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Create a listener zocket on IUT.");
    rpc_zftl_listen(pco_iut, stack, iut_addr, attr,
                    &iut_zftl);

    TEST_STEP("Create a CSAP TCP socket emulation on Tester.");
    CHECK_RC(tapi_tcp_create_conn(
                              pco_tst->ta, tst_addr, iut_addr,
                              tst_if->if_name,
                              (const uint8_t *)(alien_link_addr->sa_data),
                              (const uint8_t *)(gw_tst_lladdr->sa_data),
                              TAPI_TCP_DEF_WINDOW, &tcp_conn));

    TEST_STEP("Establish TCP connection between Tester and IUT, "
              "obtaining connected TCP zocket with @b zftl_accept().");

    CHECK_RC(tapi_tcp_start_conn(tcp_conn, TAPI_TCP_CLIENT));
    ZFTS_WAIT_NETWORK(pco_iut, stack);
    CHECK_RC(tapi_tcp_wait_open(tcp_conn, TAPI_WAIT_NETWORK_DELAY));
    ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);

    rpc_zftl_accept(pco_iut, iut_zftl, &iut_zft);

    init_ackn = tapi_tcp_next_ackn(tcp_conn);

    memset(&ds, 0, sizeof(ds));
    ds.headers = headers;
    ds.headers_size = sizeof(headers);

    TEST_STEP("Call @b zf_delegated_send_prepare() the first time on IUT, "
              "check that it succeeds.");

    first_size = rand_range(FIRST_SIZE_MIN, FIRST_SIZE_MAX);
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, first_size,
                                       first_size, 0, &ds);
    zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_OK, first_size,
                          rc, &ds, "The first call of ");

    max_first_send = first_size / 2;
    /*
     * If we want to observe RC_NOCWIN, no more than a single packet
     * should be sent here. Otherwise the second
     * zf_delegated_send_prepare() will fail with RC_SENDQ_BUSY
     * instead.
     */
    if (second_rc == ZF_DELEGATED_SEND_RC_NOCWIN)
        max_first_send = MIN(SINGLE_PKT_LEN, max_first_send);

    first_send = rand_range(1, max_first_send);

    TEST_STEP("Prepare the first portion of data for sending. It should "
              "be less than total number of bytes reserved for delegated "
              "sends.");
    te_fill_buf(send_buf, first_send);
    zfts_split_in_iovs(send_buf, first_send, ZFTS_TCP_DATA_MAX / 2,
                       ZFTS_TCP_DATA_MAX, &iov, &iov_num);
    CHECK_RC(te_dbuf_append(&sent_data, send_buf, first_send));

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, send the first portion "
                  "of data via the RAW socket from IUT.");
        zfts_ds_raw_send_iov(pco_iut, iut_if->if_index, raw_socket,
                             &ds, iov, iov_num, FALSE);
    }
    else
    {
        TEST_STEP("If @p ds_send is @c FALSE, pass the first portion "
                  "of data to @b zf_delegated_send_complete(), so "
                  "that ZF can transmit it.");
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_delegated_send_complete(pco_iut, iut_zft, iov,
                                            iov_num, 0);
        zfts_check_ds_complete(
                       pco_iut, rc, first_send,
                       "The first call: ");
    }

    second_size = rand_range(first_size + 1, SECOND_SIZE_MAX);

    if (second_rc == ZF_DELEGATED_SEND_RC_NOWIN)
    {
        TEST_STEP("If @p second_rc is @c nowin, receive on Tester "
                  "data sent from IUT. In the ACK for the last "
                  "IUT packet send zero TCP window.");

        receive_data(pco_iut, stack, tcp_conn, &recv_data,
                     &sent_data, FALSE,
                     "Receiving the first part of data from IUT");

        saved_window = tapi_tcp_get_window(tcp_conn);
        CHECK_RC(tapi_tcp_set_window(tcp_conn, 0));
        CHECK_RC(tapi_tcp_send_ack(tcp_conn,
                                   tapi_tcp_next_ackn(tcp_conn)));

        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }
    else if (second_rc == ZF_DELEGATED_SEND_RC_OK)
    {
        /*
         * CSAP acknowledges packets too slowly, congestion window
         * is small in this test.
         */
        cong_wnd_override = second_size;

        TEST_STEP("If @p second_rc is @c ok, receive and acknowledge "
                  "data on Tester.");
        receive_data(pco_iut, stack, tcp_conn, &recv_data,
                     &sent_data, TRUE,
                     "Receiving the first part of data from IUT");
    }

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, pass the first portion "
                  "of data to @b zf_delegated_send_complete().");
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_delegated_send_complete(pco_iut, iut_zft, iov,
                                            iov_num, 0);
        zfts_check_ds_complete(
                       pco_iut, rc, first_send,
                       "The first call: ");
    }
    free(iov);
    iov = NULL;

    if (second_rc == ZF_DELEGATED_SEND_RC_NOCWIN)
    {
        TEST_STEP("If @p second_rc is @c nocwin, wait for a while "
                  "not acking data sent from IUT, so that congestion "
                  "window is reduced.");
        rpc_zf_process_events_long(pco_iut, stack, WAIT_CWIN_REDUCED);
    }

    TEST_STEP("Call @b zf_delegated_send_prepare() the second time on IUT, "
              "asking for more bytes than the first time. Check that "
              "its return value matches @p second_rc.");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, second_size,
                                       cong_wnd_override, 0, &ds);
    if (rc != second_rc)
    {
        TEST_VERDICT("The second call of zf_delegated_send_prepare() "
                     "returned %s instead of %s",
                     zf_delegated_send_rc_rpc2str(rc),
                     zf_delegated_send_rc_rpc2str(second_rc));
    }

    if (rc == ZF_DELEGATED_SEND_RC_OK)
    {
        TEST_STEP("If @c ZF_DELEGATED_SEND_RC_OK was returned, check "
                  "that @b delegated_wnd matches the requested "
                  "data size.");

        if (ds.delegated_wnd != second_size)
        {
            TEST_VERDICT("The second call of zf_delegated_send_prepare() "
                         "reserved %s bytes than expected",
                         (ds.delegated_wnd > second_size ?
                                                    "more" : "less"));
        }
    }
    else
    {
        TEST_STEP("If not OK value was returned, check that "
                  "@b delegated_wnd is equal to @b delegated_wnd "
                  "returned by the first "
                  "@b zf_delegated_send_prepare() call minus number "
                  "of bytes completed since then.");

        remained_ds = first_size - first_send;

        if (ds.delegated_wnd != remained_ds)
        {
            ERROR("ds.delegated_wnd = %d, but should be %d",
                  ds.delegated_wnd, remained_ds);

            TEST_VERDICT("The second call of zf_delegated_send_prepare() "
                         "returned expected error code but %s "
                         "delegated_wnd", (ds.delegated_wnd < remained_ds ?
                                          "decreased" : "increased"));
        }

        second_size = remained_ds;
    }

    if (second_rc == ZF_DELEGATED_SEND_RC_NOWIN)
    {
        TEST_STEP("If @p second_rc is @c nowin, send another ACK "
                  "to IUT with restored TCP window.");
        CHECK_RC(tapi_tcp_set_window(tcp_conn, saved_window));
        CHECK_RC(tapi_tcp_send_ack(tcp_conn,
                                   tapi_tcp_next_ackn(tcp_conn)));
    }

    TEST_STEP("Prepare the second portion of data to send from IUT. "
              "Its size should match current value of @b delegated_wnd.");
    te_fill_buf(send_buf, second_size);
    CHECK_RC(te_dbuf_append(&sent_data, send_buf, second_size));
    zfts_split_in_iovs(send_buf, second_size, ZFTS_TCP_DATA_MAX / 2,
                       ZFTS_TCP_DATA_MAX, &iov, &iov_num);

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, send data over the RAW "
                  "socket from IUT. Send ACK to it from Tester.");

        zfts_ds_raw_send_iov(pco_iut, iut_if->if_index, raw_socket,
                             &ds, iov, iov_num, FALSE);
        CHECK_RC(tapi_tcp_send_ack(tcp_conn,
                                   init_ackn + first_send + second_size));
    }

    TEST_STEP("Pass data to @b zf_delegated_send_complete() on IUT.");
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_complete(pco_iut, iut_zft, iov, iov_num, 0);
    zfts_check_ds_complete(pco_iut, rc, second_size, "The second call: ");

    TEST_STEP("Receive and check data on Tester.");
    receive_data(pco_iut, stack, tcp_conn, &recv_data,
                 &sent_data, TRUE,
                 "Receiving all the data from IUT");

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_iut, raw_socket);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);


    if (tcp_conn != 0)
        CLEANUP_CHECK_RC(tapi_tcp_destroy_connection(tcp_conn));

    te_dbuf_free(&sent_data);
    te_dbuf_free(&recv_data);
    free(iov);

    TEST_END;
}
