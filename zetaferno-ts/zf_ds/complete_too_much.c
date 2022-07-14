/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Delegated Sends tests
 */

/**
 * @page zf_ds-complete_too_much Passing too much data to zf_delegated_send_complete()
 *
 * @objective Check that @b zf_delegated_send_complete() fails with
 *            @c EMSGSIZE when more data is passed to it than was
 *            reserved for delegated sends.
 *
 * @param env                 Testing environment:
 *                            - @ref arg_types_env_peer2peer
 * @param ds_send             If @c TRUE, actually send data reserved
 *                            for delegated sends over a RAW socket;
 *                            otherwise let ZF transmit it after
 *                            completion.
 * @param extend_win          If @c TRUE, try to extend delegated window
 *                            after @b zf_delegated_send_complete() fails
 *                            with @c EMSGSIZE.
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "zf_ds/complete_too_much"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_tcp.h"
#include "tapi_route_gw.h"
#include "zf_ds_test.h"

/*
 * Maximum number of bytes to reserve with the first call of
 * zf_delegated_send_prepare().
 */
#define MAX_FIRST_LEN 5000

/*
 * Maximum number of extra packets to pass to
 * zf_delegated_send_complete().
 */
#define MAX_EXTRA_BUFS 10

/* Maximum total number of bytes which can be sent. */
#define MAX_TOTAL_LEN (MAX_FIRST_LEN + MAX_EXTRA_BUFS * ZFTS_TCP_DATA_MAX)

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

    char headers[ZFTS_TCP_HDRS_MAX];
    struct zf_ds ds;
    int ds_len;
    int full_len;

    te_bool ds_send;
    te_bool extend_win;

    char buf[MAX_TOTAL_LEN];
    rpc_iovec *iovs = NULL;
    int iovlen = 0;
    int ext_iovlen = 0;
    int i;

    te_dbuf recv_data = TE_DBUF_INIT(0);

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(iut_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(ds_send);
    TEST_GET_BOOL_PARAM(extend_win);

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

    memset(&ds, 0, sizeof(ds));
    ds.headers = headers;
    ds.headers_size = sizeof(headers);

    ds_len = rand_range(1, MAX_FIRST_LEN);

    TEST_STEP("Reserve some bytes for delegated sends by calling "
              "@b zf_delegated_send_prepare().");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, ds_len,
                                       0, 0, &ds);
    zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_OK, ds_len,
                          rc, &ds, "The first call of ");

    te_fill_buf(buf, MAX_TOTAL_LEN);
    zfts_split_in_iovs(buf, ds_len, ZFTS_TCP_DATA_MAX / 2,
                       ZFTS_TCP_DATA_MAX,
                       &iovs, &iovlen);

    ext_iovlen = iovlen + rand_range(1, MAX_EXTRA_BUFS);
    iovs = (rpc_iovec *)tapi_realloc(iovs, ext_iovlen * sizeof(*iovs));

    full_len = ds_len;
    for (i = iovlen; i < ext_iovlen; i++)
    {
        iovs[i].iov_base = buf + full_len;
        iovs[i].iov_len = iovs[i].iov_rlen =
                              rand_range(1, ZFTS_TCP_DATA_MAX);
        full_len += iovs[i].iov_len;
    }

    if (ds_send)
    {
        TEST_STEP("If @p ds_send is @c TRUE, send data over the RAW "
                  "socket from IUT. If @p extend_win is @c TRUE, send "
                  "all the data which will be passed to "
                  "@b zf_delegated_send_complete() next; otherwise - "
                  "only data fitting into the reserved @b delegated_wnd.");
        zfts_ds_raw_send_iov(pco_iut, iut_if->if_index, raw_socket,
                             &ds, iovs,
                             (extend_win ? ext_iovlen : iovlen),
                             FALSE);
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    TEST_STEP("Call @b zf_delegated_send_complete(), passing to it "
              "more data than reserved @b delegated_wnd allows. Check that "
              "it fails with @c EMSGSIZE.");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_complete(pco_iut, iut_zft,
                                        iovs, ext_iovlen, 0);
    if (rc >= 0)
    {
        TEST_VERDICT("zf_delegated_send_complete() succeeded with too "
                     "much data");
    }
    else if (RPC_ERRNO(pco_iut) != RPC_EMSGSIZE)
    {
        WARN_VERDICT("zf_delegated_send_complete() failed with unexpected "
                     "error %r when too much data was passed to it",
                     RPC_ERRNO(pco_iut));
    }

    if (extend_win)
    {
        TEST_STEP("If @p extend_win is @c TRUE, call "
                  "@b zf_delegated_send_prepare() again to extend "
                  "@b delegated_wnd, so that all the data will fit "
                  "into it.");

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, full_len,
                                           full_len, 0, &ds);
        zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_OK, full_len,
                              rc, &ds, "The second call of ");

        ds_len = full_len;
        iovlen = ext_iovlen;
    }

    TEST_STEP("Call @b zf_delegated_send_complete() again, passing "
              "to it data fitting into the current @b delegated_wnd. "
              "Check that it succeeds.");

    rc = rpc_zf_delegated_send_complete(pco_iut, iut_zft,
                                        iovs, iovlen, 0);
    zfts_check_ds_complete(pco_iut, rc, ds_len,
                           "The second call of ");

    TEST_STEP("Receive and check all the sent data on Tester.");
    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s, &recv_data);
    ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, buf,
                             recv_data.len, ds_len, "");

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_iut, raw_socket);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    free(iovs);
    te_dbuf_free(&recv_data);

    TEST_END;
}
