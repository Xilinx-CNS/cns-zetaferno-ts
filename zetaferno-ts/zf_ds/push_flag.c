/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Delegated Sends tests
 */

/**
 * @page zf_ds-push_flag Setting TCP PUSH flag for delegated sends
 *
 * @objective Check that TCP PUSH flag is set as expected by
 *            @b zf_delegated_send_tcp_update().
 *
 * @param env                 Testing environment:
 *                            - @ref arg_types_env_peer2peer
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "zf_ds/push_flag"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_tcp.h"
#include "tapi_sockets.h"
#include "zf_ds_test.h"

/** Minimum size of data to reserve for delegated sends */
#define MIN_DS_LEN 3000
/** Maximum size of data to reserve for delegated sends */
#define MAX_DS_LEN 5000

/** Structure storing results of processing of captured packets */
typedef struct csap_data {
    te_bool push_set;   /**< Is set to TRUE if TCP PUSH flag
                             was encountered */
    te_bool failed;     /**< Is set to TRUE if some failure
                             occurred when processing packets */
} csap_data;

/**
 * Callback for processing packets captured by CSAP.
 *
 * @param pkt             Captured packet.
 * @param user_data       Pointer to csap_data structure.
 */
static void
csap_cb(asn_value *pkt, void *user_data)
{
    csap_data *data = (csap_data *)user_data;
    uint32_t flags;
    te_errno rc;

    rc = asn_read_uint32(pkt, &flags, "pdus.0.#tcp.flags.#plain");
    if (rc != 0)
    {
        ERROR("Failed to get TCP flags, rc=%r", rc);
        data->failed = TRUE;
        goto cleanup;
    }

    if (flags & TCP_PSH_FLAG)
        data->push_set = TRUE;
    else
        data->push_set = FALSE;

    RING("A packet was captured, TCP PUSH flag is %sset",
         (data->push_set ? "" : "not "));

cleanup:

    asn_free_value(pkt);
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

    rpc_iovec raw_iovs[2];
    char headers[ZFTS_TCP_HDRS_MAX];
    struct zf_ds ds;
    int ds_len;
    int rem_len;
    int pkt_len;
    int set_push;

    csap_handle_t csap = CSAP_INVALID_HANDLE;
    char send_buf[ZFTS_TCP_DATA_MAX];
    char recv_buf[ZFTS_TCP_DATA_MAX];

    tapi_tad_trrecv_cb_data cb_data;
    csap_data user_data;
    unsigned int num;
    te_bool readable;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(iut_if);
    TEST_GET_IF(tst_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);

    /* Do not print out packets captured by CSAP. */
    rcf_tr_op_log(FALSE);

    TEST_STEP("Create RAW socket on IUT to send data from it.");
    raw_socket = rpc_socket(pco_iut, RPC_AF_PACKET, RPC_SOCK_RAW,
                            RPC_IPPROTO_RAW);

    TEST_STEP("Allocate ZF attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Establish TCP connection between a zocket on IUT and "
              "a socket on Tester.");
    zfts_establish_tcp_conn(TRUE, pco_iut, attr, stack, &iut_zft,
                            iut_addr, pco_tst, &tst_s, tst_addr);

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

    memset(&ds, 0, sizeof(ds));
    ds.headers = headers;
    ds.headers_size = sizeof(headers);

    ds_len = rand_range(MIN_DS_LEN, MAX_DS_LEN);

    TEST_STEP("Reserve some bytes for delegated sends with "
              "@b zf_delegated_send_prepare().");

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_delegated_send_prepare(pco_iut, iut_zft, ds_len,
                                       0, 0, &ds);
    zfts_check_ds_prepare(ZF_DELEGATED_SEND_RC_OK, ds_len,
                          rc, &ds, "");

    memset(&user_data, 0, sizeof(user_data));
    memset(&cb_data, 0, sizeof(cb_data));
    cb_data.callback = &csap_cb;
    cb_data.user_data = &user_data;

    TEST_STEP("Send packets in a loop via the RAW socket until all the "
              "data reserved for delegated sends is sent; receive "
              "and check every packet on Tester.");
    TEST_SUBSTEP("For each packet set TCP PUSH flag randomly with "
                 "@b zf_delegated_send_tcp_update().");
    TEST_SUBSTEP("On Tester capture every packet with CSAP and check that "
                 "it has TCP PUSH flag set or not set as expected.");

    rem_len = ds_len;
    while (rem_len > 0)
    {
        set_push = rand_range(0, 1);
        pkt_len = rand_range(1, ZFTS_TCP_DATA_MAX);
        if (pkt_len > rem_len)
            pkt_len = rem_len;

        te_fill_buf(send_buf, pkt_len);

        rpc_zf_delegated_send_tcp_update(pco_iut, &ds, pkt_len, set_push);

        memset(&raw_iovs, 0, sizeof(raw_iovs));
        raw_iovs[0].iov_base = ds.headers;
        raw_iovs[0].iov_len = raw_iovs[0].iov_rlen = ds.headers_len;
        raw_iovs[1].iov_base = send_buf;
        raw_iovs[1].iov_len = raw_iovs[1].iov_rlen = pkt_len;

        CHECK_RC(tapi_sock_raw_tcpv4_send(pco_iut, raw_iovs,
                                          TE_ARRAY_LEN(raw_iovs),
                                          iut_if->if_index, raw_socket,
                                          TRUE));

        rpc_zf_delegated_send_tcp_advance(pco_iut, &ds, pkt_len);

        RPC_GET_READABILITY(readable, pco_tst, tst_s,
                            TAPI_WAIT_NETWORK_DELAY);
        if (!readable)
        {
            TEST_VERDICT("Tester socket did not become readable after "
                         "sending data from IUT");
        }

        rc = rpc_recv(pco_tst, tst_s, recv_buf, sizeof(recv_buf), 0);
        ZFTS_CHECK_RECEIVED_DATA(recv_buf, send_buf, rc, pkt_len, "");

        CHECK_RC(tapi_tad_trrecv_get(pco_tst->ta, 0, csap, &cb_data,
                                     &num));
        if (num == 0)
        {
            TEST_VERDICT("CSAP did not capture a packet");
        }
        else if (num > 1)
        {
            TEST_VERDICT("CSAP captured more than a single packet");
        }
        else if (user_data.failed)
        {
            TEST_VERDICT("Failed to process captured packet");
        }
        else if (user_data.push_set != (set_push != 0 ? TRUE : FALSE))
        {
            TEST_VERDICT("TCP PUSH flag was %sset unexpectedly in the "
                         "captured packet",
                         (user_data.push_set ? "" : "not "));
        }

        RPC_AWAIT_ERROR(pco_iut);
        rc = zfts_ds_complete_buf(pco_iut, iut_zft, send_buf, pkt_len, 0);
        zfts_check_ds_complete(pco_iut, rc, pkt_len, "");

        rem_len -= pkt_len;
    }

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_iut, raw_socket);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_CHECK_RC(tapi_tad_csap_destroy(pco_tst->ta, 0, csap));

    TEST_END;
}
