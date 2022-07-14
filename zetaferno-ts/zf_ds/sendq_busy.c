/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Delegated Sends tests
 */

/**
 * @page zf_ds-sendq_busy Calling zf_delegated_send_prepare() when send queue is filled
 *
 * @objective Check that @b zf_delegated_send_prepare() fails when called
 *            on a zocket with filled send queue.
 *
 * @param env                 Testing environment:
 *                            - @ref arg_types_env_peer2peer
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

#define TE_TEST_NAME  "zf_ds/sendq_busy"

#include "zf_test.h"
#include "rpc_zf.h"

#include "zf_ds_test.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    const struct if_nameindex *iut_if = NULL;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    int tst_s = -1;
    int raw_socket = -1;

    struct zf_ds ds;
    char headers[ZFTS_TCP_HDRS_MAX];
    char send_buf[ZFTS_TCP_DATA_MAX];
    char recv_buf[ZFTS_TCP_DATA_MAX];
    int send_len;

    te_dbuf sent_data = TE_DBUF_INIT(0);
    te_dbuf recv_data = TE_DBUF_INIT(0);

    te_bool ds_send;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(iut_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(ds_send);

    TEST_STEP("Allocate ZF attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Establish TCP connection between a zocket on IUT and "
              "a socket on Tester.");
    zfts_establish_tcp_conn(TRUE, pco_iut, attr, stack, &iut_zft,
                            iut_addr, pco_tst, &tst_s, tst_addr);

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, create RAW socket on IUT "
                  "to send data from it.");
        raw_socket = rpc_socket(pco_iut, RPC_AF_PACKET, RPC_SOCK_RAW,
                                RPC_IPPROTO_RAW);
    }

    TEST_STEP("Call @b zft_send_single() in a loop on IUT until it "
              "fails with @c EAGAIN.");
    while (TRUE)
    {
        te_fill_buf(send_buf, sizeof(send_buf));

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_send_single(pco_iut, iut_zft, send_buf,
                                 sizeof(send_buf), 0);
        if (rc < 0)
        {
            if (RPC_ERRNO(pco_iut) != RPC_EAGAIN)
            {
                TEST_VERDICT("zft_send_single() failed with unexpected "
                             "error " RPC_ERROR_FMT,
                             RPC_ERROR_ARGS(pco_iut));
            }

            break;
        }

        CHECK_RC(te_dbuf_append(&sent_data, send_buf, rc));
    }

    TEST_STEP("Prepare some data for delegated sends.");
    send_len = rand_range(1, sizeof(send_buf));
    te_fill_buf(send_buf, send_len);

    memset(&ds, 0, sizeof(ds));
    ds.headers = headers;
    ds.headers_size = sizeof(headers);

    TEST_STEP("Call @b zf_delegated_send_prepare() on IUT and check "
              "that it fails with @c ZF_DELEGATED_SEND_RC_SENDQ_BUSY.");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, send_len,
                                       0, 0, &ds);

    if (rc != ZF_DELEGATED_SEND_RC_SENDQ_BUSY)
    {
        TEST_VERDICT("The first call of zf_delegated_send_prepare() "
                     "returned %s instead of SENDQ_BUSY after overfilling "
                     "send queue", zf_delegated_send_rc_rpc2str(rc));
    }

    TEST_STEP("Read all the data from the Tester socket, processing "
              "events on IUT.");

    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s, &recv_data);
    ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, sent_data.ptr,
                             recv_data.len, sent_data.len,
                             " before performing delegated send");

    TEST_STEP("Call @b zf_delegated_send_prepare() on IUT again, check "
              "that now it succeeds.");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, send_len,
                                       0, 0, &ds);
    zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_OK, send_len,
                          rc, &ds, "The second call of ");

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, send the data prepared for "
                  "delegated sends via the RAW socket from IUT.");

        zfts_ds_raw_send(pco_iut, iut_if->if_index, raw_socket,
                         &ds, send_buf, send_len, FALSE);
    }

    TEST_STEP("Call @b zf_delegated_send_complete() on IUT, passing "
              "to it the data prepared for delegated sends.");

    RPC_AWAIT_ERROR(pco_iut);
    rc = zfts_ds_complete_buf(pco_iut, iut_zft, send_buf,
                              send_len, 0);
    zfts_check_ds_complete(pco_iut, rc, send_len, "");

    TEST_STEP("Call @b zf_reactor_perform() in a loop for a while on IUT.");
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    TEST_STEP("Receive and check data on Tester.");
    rc = rpc_recv(pco_tst, tst_s, recv_buf, sizeof(recv_buf), 0);
    ZFTS_CHECK_RECEIVED_DATA(recv_buf, send_buf, rc, send_len);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_iut, raw_socket);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    te_dbuf_free(&sent_data);
    te_dbuf_free(&recv_data);

    TEST_END;
}
