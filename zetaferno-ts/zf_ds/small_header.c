/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Delegated Sends tests
 */

/**
 * @page zf_ds-small_header Passing too small headers size to zf_delegated_send_prepare()
 *
 * @objective Check that @b zf_delegated_send_prepare() correctly processes
 *            too small headers buffer size.
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

#define TE_TEST_NAME  "zf_ds/small_header"

#include "zf_test.h"
#include "rpc_zf.h"

#include "zf_ds_test.h"

/* Length of headers which is certainly too small */
#define TOO_SMALL_HLEN 10

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
    int returned_hlen;

    te_bool ds_send;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(iut_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(ds_send);

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, create RAW socket on IUT to "
                  "send data from it.");
        raw_socket = rpc_socket(pco_iut, RPC_AF_PACKET, RPC_SOCK_RAW,
                                RPC_IPPROTO_RAW);
    }

    TEST_STEP("Allocate ZF attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Establish TCP connection between a zocket on IUT and "
              "a socket on Tester.");
    zfts_establish_tcp_conn(TRUE, pco_iut, attr, stack, &iut_zft,
                            iut_addr, pco_tst, &tst_s, tst_addr);

    TEST_STEP("Prepare some data for delegated sends.");
    send_len = rand_range(1, sizeof(send_buf));
    te_fill_buf(send_buf, send_len);

    memset(&ds, 0, sizeof(ds));
    ds.headers = headers;
    ds.headers_size = rand_range(0, TOO_SMALL_HLEN);

    TEST_STEP("Call @b zf_delegated_send_prepare() with too small "
              "@b headers_size on IUT. Check that it fails with "
              "@c ZF_DELEGATED_SEND_RC_SMALL_HEADER and returns "
              "required headers length in @b headers_len.");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, send_len,
                                       0, 0, &ds);

    if (rc == ZF_DELEGATED_SEND_RC_OK)
    {
        TEST_VERDICT("zf_delegated_send_prepare() succeeded with too "
                     "small headers size");
    }
    else if (rc != ZF_DELEGATED_SEND_RC_SMALL_HEADER)
    {
        TEST_VERDICT("zf_delegated_send_prepare() with too small headers "
                     "size failed with unexpected return value %s",
                     zf_delegated_send_rc_rpc2str(rc));
    }

    returned_hlen = ds.headers_len;
    if (returned_hlen > (int)sizeof(headers))
    {
        TEST_VERDICT("Too big headers buffer size is reported to be "
                     "necessary");
    }
    else if (returned_hlen <= ds.headers_size)
    {
        TEST_VERDICT("Failed zf_delegated_send_prepare() did not set "
                     "headers_len to a bigger value");
    }

    TEST_STEP("Call @b zf_delegated_send_prepare() on IUT again with "
              "@b headers_size set to @b headers_len returned before, "
              "check that it succeeds.");

    ds.headers_size = returned_hlen;

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, send_len,
                                       0, 0, &ds);
    zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_OK, send_len,
                          rc, &ds, "The second call of ");

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, send the data prepared for "
                  "delegated sends via RAW socket from IUT.");
        zfts_ds_raw_send(pco_iut, iut_if->if_index, raw_socket,
                         &ds, send_buf, send_len, FALSE);
    }

    TEST_STEP("Call @b zf_delegated_send_complete() on IUT, passing to it "
              "the data prepared for delegated sends.");
    RPC_AWAIT_ERROR(pco_iut);
    rc = zfts_ds_complete_buf(pco_iut, iut_zft, send_buf, send_len, 0);
    zfts_check_ds_complete(pco_iut, rc, send_len, "");

    TEST_STEP("Call @b zf_reactor_perform() in a loop for a while on IUT.");
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    TEST_STEP("Receive and check sent data on the Tester socket.");
    rc = rpc_recv(pco_tst, tst_s, recv_buf, sizeof(recv_buf), 0);
    ZFTS_CHECK_RECEIVED_DATA(recv_buf, send_buf, rc, send_len);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_iut, raw_socket);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
