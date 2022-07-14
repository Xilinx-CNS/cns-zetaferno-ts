/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Delegated Sends tests
 */

/**
 * @page zf_ds-complete_overfill Passing too many iovecs to zf_delegated_send_complete()
 *
 * @objective Check that @b zf_delegated_send_complete() correctly
 *            processes the case when too many iovecs are passed to it.
 *
 * @param env                   Testing environment:
 *                              - @ref arg_types_env_peer2peer_gw
 * @param ds_send               If @c TRUE, actually send data reserved
 *                              for delegated sends over a RAW socket;
 *                              otherwise let ZF transmit it after
 *                              completion.
 * @param break_conn            If @c TRUE, break network connection
 *                              before calling the first
 *                              @b zf_delegated_send_complete().
 * @param acks_before_complete  If @c TRUE, ACKs to sent data should
 *                              be processed before calling
 *                              @b zf_delegated_send_complete() the
 *                              first time.
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "zf_ds/complete_overfill"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_tcp.h"
#include "tapi_route_gw.h"
#include "zf_ds_test.h"

/* How many bytes to ask for delegated sends the first time */
#define FIRST_DS_LEN 100000

/*
 * Maximum number of bytes to reserve for delegated sends
 * the second time.
 */
#define MAX_SECOND_DS_LEN 4000

/* Minimum TCP MSS. */
#define MIN_MSS 536

/* Maximum length of a small packet */
#define MAX_SMALL_PKT_LEN 10

/*
 * How long to wait for IUT data to be retransmitted after
 * repairing network connection (ms).
 */
#define WAIT_RETRANSMITS 3000

/**
 * Call rpc_zf_delegated_send_complete() with disabled RPC logging.
 * This function is used because a lot of small iovecs look bad
 * in log.
 *
 * @param rpcs          RPC server.
 * @param zft           TCP zocket on IUT.
 * @param iovs          Array of rpc_iovec structures.
 * @param iovlen        Number of elements in the array.
 * @param flags         Flags for zf_delegated_send_complete().
 *
 * @return Return value of zf_delegated_send_complete().
 */
static int
send_complete_silent(rcf_rpc_server *rpcs, rpc_zft_p zft,
                     rpc_iovec *iovs, int iovlen, int flags)
{
    int rc;

    rpcs->silent = TRUE;
    RPC_AWAIT_ERROR(rpcs);
    rc = rpc_zf_delegated_send_complete(rpcs, zft, iovs, iovlen, flags);
    RING("zf_delegated_send_complete() returned %d, errno=%r",
         rc, RPC_ERRNO(rpcs));

    return rc;
}

int
main(int argc, char *argv[])
{
    TAPI_DECLARE_ROUTE_GATEWAY_PARAMS;

    tapi_route_gateway gw;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    rpc_zftl_p iut_zftl = RPC_NULL;
    int raw_socket = -1;
    int tst_s = -1;

    struct zf_ds ds;
    char headers[ZFTS_TCP_HDRS_MAX];
    int exp_rc;
    int exp_ds_wnd;
    int second_ds_len;
    size_t send_space = 0;

    char buf[FIRST_DS_LEN];
    te_dbuf recv_data = TE_DBUF_INIT(0);
    te_bool readable;

    rpc_iovec *iovs = NULL;
    int iovlen = 0;
    int rem_len = 0;
    int new_len = 0;
    int tot_len = 0;
    rpc_iovec iov;
    int i;

    rpc_iovec *iovs_aux = NULL;
    int iovlen_aux = 0;

    te_bool ds_send;
    te_bool break_conn;
    te_bool acks_before_complete;

    TEST_START;
    TAPI_GET_ROUTE_GATEWAY_PARAMS;
    TEST_GET_BOOL_PARAM(ds_send);
    TEST_GET_BOOL_PARAM(break_conn);
    TEST_GET_BOOL_PARAM(acks_before_complete);

    if (acks_before_complete && (!ds_send || break_conn))
    {
        TEST_FAIL("acks_before_complete=TRUE makes no sense if "
                  "ACKs are not expected");
    }

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, create RAW socket on IUT to "
                  "send data from it.");
        raw_socket = rpc_socket(pco_iut, RPC_AF_PACKET, RPC_SOCK_RAW,
                                RPC_IPPROTO_RAW);
    }

    TEST_STEP("Configure routing between IUT and Tester via a "
              "gateway host.");
    TAPI_INIT_ROUTE_GATEWAY(gw);
    CHECK_RC(tapi_route_gateway_configure(&gw));
    CFG_WAIT_CHANGES;

    TEST_STEP("Allocate ZF attributes and stack on IUT.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Create TCP socket on Tester, bind it to @p tst_addr.");
    tst_s = rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                       RPC_PROTO_DEF, FALSE, FALSE,
                                       tst_addr);
    TEST_STEP("Set @c TCP_MAXSEG for the Tester socket to a small value. "
              "This is done because on Zetaferno size of send queue is "
              "proportional to MSS.");
    rpc_setsockopt_int(pco_tst, tst_s, RPC_TCP_MAXSEG, MIN_MSS);

    TEST_STEP("Create a listener zocket on IUT.");
    rpc_zftl_listen(pco_iut, stack, iut_addr, attr, &iut_zftl);

    TEST_STEP("Establish TCP connection, obtaining connected "
              "TCP zocket on IUT with @b zftl_accept().");

    pco_tst->op = RCF_RPC_CALL;
    rpc_connect(pco_tst, tst_s, iut_addr);

    ZFTS_WAIT_NETWORK(pco_iut, stack);

    rpc_connect(pco_tst, tst_s, iut_addr);
    rpc_zftl_accept(pco_iut, iut_zftl, &iut_zft);

    if (break_conn)
    {
        TEST_STEP("If @p break_conn is @c TRUE, break connection "
                  "between the gateway and Tester, so that "
                  "Tester does not send ACKs to packets from IUT.");
        CHECK_RC(tapi_route_gateway_break_gw_tst(&gw));
        CFG_WAIT_CHANGES;
    }

    memset(&ds, 0, sizeof(ds));
    ds.headers = headers;
    ds.headers_size = sizeof(headers);

    rpc_zft_send_space(pco_iut, iut_zft, &send_space);

    TEST_STEP("Call @b zf_delegated_send_prepare() on IUT the first time, "
              "asking for large number of bytes. Check that returned "
              "@b delegated_wnd is set to minimum of @b send_wnd and "
              "value returned by @b zft_send_space().");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, FIRST_DS_LEN,
                                       FIRST_DS_LEN, 0, &ds);
    exp_ds_wnd = MIN(ds.send_wnd, (int)send_space);
    zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_OK, exp_ds_wnd,
                          rc, &ds, "The first call of ");

    te_fill_buf(buf, ds.delegated_wnd);
    iovs = tapi_calloc(ds.delegated_wnd, sizeof(*iovs));

    tot_len = ds.delegated_wnd;
    for (i = 0; i < ds.delegated_wnd && tot_len > 0; i++)
    {
        iovs[i].iov_base = buf + ds.delegated_wnd - tot_len;

        new_len = rand_range(1, MAX_SMALL_PKT_LEN);
        if (new_len > tot_len)
            new_len = tot_len;

        iovs[i].iov_len = iovs[i].iov_rlen = new_len;
        tot_len -= new_len;
    }
    iovlen = i;
    tot_len = ds.delegated_wnd;

    if (ds_send && !break_conn)
    {
        TEST_STEP("If @p ds_send is @c TRUE and @p break_conn is @c FALSE, "
                  "send all data reserved for delegated sends via the RAW "
                  "socket from IUT and wait for a while, letting ACKs to "
                  "arrive.");
        zfts_ds_raw_send_iov(pco_iut, iut_if->if_index, raw_socket,
                             &ds, iovs, iovlen, TRUE);
        RING("Sent %d bytes in %d packets over RAW socket",
             tot_len, iovlen);

        if (acks_before_complete)
        {
            TEST_SUBSTEP("If @p acks_before_complete is @c TRUE, call "
                         "@b zf_reactor_perform() in a loop for a while "
                         "on IUT to process ACKs from Tester.");
            ZFTS_WAIT_NETWORK(pco_iut, stack);
        }
        else
        {
            TAPI_WAIT_NETWORK;
        }
    }

    TEST_STEP("Call @b zf_delegated_send_complete() the first time, "
              "passing to it array of many small iovecs containing "
              "@b delegated_wnd bytes. Check that it accepts all "
              "the data if ACKs are already received and processed "
              "(@p acks_before_complete is @c TRUE), and accepts "
              "only part of the data otherwise.");
    rc = send_complete_silent(pco_iut, iut_zft, iovs, iovlen, 0);

    if (rc < 0)
    {
        TEST_VERDICT("The first call of zf_delegated_send_complete() "
                     "failed with %r", RPC_ERRNO(pco_iut));
    }
    else if (rc > tot_len)
    {
        TEST_VERDICT("The first call of zf_delegated_send_complete() "
                     "returned too big value");
    }
    else if (rc == tot_len)
    {
        if (!acks_before_complete)
        {
            WARN_VERDICT("The first call of zf_delegated_send_complete() "
                         "accepted all data in small buffers");
        }
    }
    else if (rc == 0)
    {
        TEST_VERDICT("The first call of zf_delegated_send_complete() "
                     "returned zero");
    }
    else
    {
        if (acks_before_complete)
        {
            WARN_VERDICT("zf_delegated_send_complete() returned partial "
                         "success despite processing all ACKs before "
                         "calling it");
        }

        if (break_conn && ds_send)
        {
            /*
             * This is done so that sending over RAW socket will work
             * correctly for the remaining data. It makes no sense to
             * actually send the first portion of data over it when
             * network connection is broken.
             */
            rpc_zf_delegated_send_tcp_advance(pco_iut, &ds, rc);
        }

        rem_len = tot_len - rc;

        iov.iov_base = buf;
        iov.iov_len = iov.iov_rlen = MIN(rem_len, MIN_MSS);

        TEST_STEP("If @b zf_delegated_send_complete() reports partial "
                  "success:");

        TEST_SUBSTEP("Try to call @b zf_delegated_send_complete() the "
                     "second time, passing to it another iovec. "
                     "Check that it fails with @c EAGAIN.");

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_delegated_send_complete(pco_iut, iut_zft, &iov,
                                            1, 0);
        if (rc >= 0)
        {
            TEST_VERDICT("The second call of zf_delegated_send_complete() "
                         "succeeded after the first call returned partial "
                         "success");
        }
        else if (RPC_ERRNO(pco_iut) != RPC_EAGAIN)
        {
            TEST_VERDICT("The second call of zf_delegated_send_complete() "
                         "failed with unexpected errno %r",
                         RPC_ERRNO(pco_iut));
        }

        TEST_SUBSTEP("Try to call @b zf_delegated_send_prepare() again, "
                     "check that it returns @c SENDQ_BUSY.");

        exp_rc = ZF_DELEGATED_SEND_RC_SENDQ_BUSY;
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, rem_len,
                                           rem_len, 0, &ds);
        if (rc == ZF_DELEGATED_SEND_RC_OK)
        {
            WARN_VERDICT("The second call of zf_delegated_send_prepare() "
                         "succeeded after zf_delegated_send_complete() "
                         "failed with EAGAIN");
        }
        else if (rc != exp_rc)
        {
            WARN_VERDICT("The second call of zf_delegated_send_prepare() "
                         "returned %s instead of %s after "
                         "zf_delegated_send_complete() failed with EAGAIN",
                         zf_delegated_send_rc_rpc2str(rc),
                         zf_delegated_send_rc_rpc2str(exp_rc));
        }
    }

    if (break_conn)
    {
        TEST_STEP("If @p break_conn is @c TRUE, repair connection between "
                  "the gateway and Tester.");
        CHECK_RC(tapi_route_gateway_repair_gw_tst(&gw));
        CFG_WAIT_CHANGES;
    }

    TEST_STEP("Wait for a while calling @b zf_reactor_perform() in "
              "a loop on IUT, so that all the pending data is sent and "
              "acknowledged by Tester.");

    pco_iut->timeout = pco_iut->def_timeout + WAIT_RETRANSMITS;
    rpc_zf_process_events_long(pco_iut, stack, WAIT_RETRANSMITS);

    TEST_STEP("Check that Tester socket is now readable.");

    RPC_GET_READABILITY(readable, pco_tst, tst_s, 0);
    if (!readable)
    {
        TEST_VERDICT("After repairing connection nothing was received from "
                     "IUT");
    }

    if (rem_len != 0)
    {
        TEST_STEP("If less than @b delegated_wnd bytes were completed "
                  "before:");

        zfts_split_in_iovs(buf + tot_len - rem_len, rem_len,
                           MIN_MSS, MIN_MSS,
                           &iovs_aux, &iovlen_aux);

        if (ds_send && break_conn)
        {
            TEST_SUBSTEP("If @p ds_send and @p break_conn are @c TRUE, "
                         "send not completed data via the RAW socket.");
            zfts_ds_raw_send_iov(pco_iut, iut_if->if_index, raw_socket,
                                 &ds, iovs_aux, iovlen_aux, TRUE);
            RING("Sent %d bytes in %d packets over RAW socket",
                 rem_len, iovlen_aux);
        }

        TEST_SUBSTEP("Pass not completed data in buffers of MSS size to "
                     "@b zf_delegated_send_complete(), check that it "
                     "succeeds.");

        rc = send_complete_silent(pco_iut, iut_zft,
                                  iovs_aux, iovlen_aux, 0);
        zfts_check_ds_complete(pco_iut, rc, rem_len,
                               "The third call of ");
    }

    TEST_STEP("Receive all the data on Tester and check it.");
    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s, &recv_data);
    ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, buf,
                             recv_data.len, tot_len,
                             " after the initial sending");

    TEST_STEP("Call @b zf_delegated_send_prepare() the last time "
              "on IUT, asking for less bytes this time. Check that "
              "it succeeds.");
    second_ds_len = rand_range(1, MAX_SECOND_DS_LEN);
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, second_ds_len,
                                       second_ds_len, 0, &ds);
    zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_OK, second_ds_len,
                          rc, &ds, "The final call of ");

    TEST_STEP("Send the data reserved for delegated sends via the "
              "RAW socket from IUT if @p ds_send is @c TRUE. "
              "Then pass the data to @b zf_delegated_send_complete().");
    te_fill_buf(buf, second_ds_len);
    zfts_ds_raw_send_complete(pco_iut, iut_if->if_index, raw_socket,
                              iut_zft, &ds, buf, second_ds_len,
                              MIN_MSS / 2, MIN_MSS);

    TEST_STEP("Receive and check data on Tester.");

    te_dbuf_reset(&recv_data);
    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s, &recv_data);

    ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, buf,
                             recv_data.len, second_ds_len,
                             " after the final sending");

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_iut, raw_socket);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    te_dbuf_free(&recv_data);

    free(iovs);
    free(iovs_aux);

    TEST_END;
}
