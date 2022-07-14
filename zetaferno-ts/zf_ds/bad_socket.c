/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Delegated Sends tests
 */

/**
 * @page zf_ds-bad_socket Calling zf_delegated_send_prepare() in inappropriate TCP state
 *
 * @objective Check that @b zf_delegated_send_prepare() fails when called
 *            on a zocket in inappropriate TCP state.
 *
 * @param env                 Testing environment:
 *                            - @ref arg_types_env_peer2peer
 * @param close_way           How to close TCP connection before calling
 *                            @b zf_delegated_send_prepare():
 *                            - @c iut_shutdown (TX shutdown on IUT)
 *                            - @c tst_shutdown (TX shutdown on Tester)
 *                            - @c iut_tst_shutdown (TX shutdown on IUT,
 *                              after that on Tester)
 *                            - @c tst_iut_shutdown (TX shutdown on Tester,
 *                              after that on IUT)
 *                            - @c tst_rst (RST from Tester)
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

#define TE_TEST_NAME  "zf_ds/bad_socket"

#include "zf_test.h"
#include "rpc_zf.h"

#include "zf_ds_test.h"

/* Values of "close_way" parameter */
enum {
    CLOSE_IUT_SHUTDOWN,
    CLOSE_TST_SHUTDOWN,
    CLOSE_IUT_TST_SHUTDOWN,
    CLOSE_TST_IUT_SHUTDOWN,
    CLOSE_TST_RST,
};

/* Mapping list of "close_way" parameter for TEST_GET_ENUM_PARAM()  */
#define CLOSE_WAYS \
    { "iut_shutdown", CLOSE_IUT_SHUTDOWN },         \
    { "tst_shutdown", CLOSE_TST_SHUTDOWN },         \
    { "iut_tst_shutdown", CLOSE_IUT_TST_SHUTDOWN }, \
    { "tst_iut_shutdown", CLOSE_TST_IUT_SHUTDOWN }, \
    { "tst_rst", CLOSE_TST_RST }

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

    int close_way;
    te_bool ds_send;
    tarpc_linger lopt;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(iut_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(close_way, CLOSE_WAYS);
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

    if (close_way == CLOSE_TST_RST)
    {
        TEST_STEP("If @p close_way is @c tst_rst, set @c SO_LINGER "
                  "with zero value on the Tester socket and @b close() "
                  "it.");

        lopt.l_onoff = 1;
        lopt.l_linger = 0;
        rpc_setsockopt(pco_tst, tst_s, RPC_SO_LINGER, &lopt);

        RPC_CLOSE(pco_tst, tst_s);
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    TEST_STEP("Otherwise call @b zft_shutdown_tx() on IUT and/or "
              "@b shutdown(@c SHUT_WR) on Tester as specified by "
              "@p close_way.");

    if (close_way == CLOSE_IUT_SHUTDOWN ||
        close_way == CLOSE_IUT_TST_SHUTDOWN)
    {
        rpc_zft_shutdown_tx(pco_iut, iut_zft);
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    if (close_way == CLOSE_TST_SHUTDOWN ||
        close_way == CLOSE_TST_IUT_SHUTDOWN ||
        close_way == CLOSE_IUT_TST_SHUTDOWN)
    {
        rpc_shutdown(pco_tst, tst_s, RPC_SHUT_WR);
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    if (close_way == CLOSE_TST_IUT_SHUTDOWN)
    {
        rpc_zft_shutdown_tx(pco_iut, iut_zft);
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    TEST_STEP("Prepare some data for delegated sends.");
    send_len = rand_range(1, sizeof(send_buf));
    te_fill_buf(send_buf, send_len);

    memset(&ds, 0, sizeof(ds));
    ds.headers = headers;
    ds.headers_size = sizeof(headers);

    TEST_STEP("Call @b zf_delegated_send_prepare(), check that it "
              "fails with @c ZF_DELEGATED_SEND_RC_BAD_SOCKET unless "
              "@p close_way is @c tst_shutdown.");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, send_len,
                                       0, 0, &ds);
    zfts_check_ds_prepare((close_way == CLOSE_TST_SHUTDOWN ?
                              ZF_DELEGATED_SEND_RC_OK :
                              ZF_DELEGATED_SEND_RC_BAD_SOCKET),
                          send_len, rc, &ds, "");

    if (rc == ZF_DELEGATED_SEND_RC_OK)
    {
        TEST_STEP("If @b zf_delegated_send_prepare() succeeded, send "
                  "the prepared data via the RAW socket from IUT (unless "
                  "@p ds_send is @c FALSE), pass the prepared data to "
                  "@b zf_delegated_send_complete(), and then receive "
                  "and check it on Tester.");

        if (ds_send)
        {
            zfts_ds_raw_send(pco_iut, iut_if->if_index, raw_socket,
                             &ds, send_buf, send_len, FALSE);
        }

        RPC_AWAIT_ERROR(pco_iut);
        rc = zfts_ds_complete_buf(pco_iut, iut_zft, send_buf,
                                  send_len, 0);
        zfts_check_ds_complete(pco_iut, rc, send_len, "");

        ZFTS_WAIT_NETWORK(pco_iut, stack);

        rc = rpc_recv(pco_tst, tst_s, recv_buf, sizeof(recv_buf), 0);
        ZFTS_CHECK_RECEIVED_DATA(recv_buf, send_buf, rc, send_len);
    }

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_iut, raw_socket);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
