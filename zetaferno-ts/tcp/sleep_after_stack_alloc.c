/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 */

/**
 * @page tcp-sleep_after_stack_alloc Sleeping after stack allocation before creating a zocket
 *
 * @objective Check whether unnecessary SYN retransmits happen if
 *            some sleep is introduced between ZF stack allocation
 *            and creating a zocket.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on Tester.
 * @param iut_addr      Network address on IUT.
 * @param tst_addr      Network address on Tester.
 * @param tst_if        Network interface on Tester.
 * @param open_method   How to open connection:
 *                      - @c active
 *                      - @c passive
 *                      - @c passive_close (close listener after
 *                        passively establishing connection)
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "tcp/sleep_after_stack_alloc"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_tad.h"
#include "tapi_tcp.h"

/** Auxiliary data passed to the CSAP callback */
typedef struct csap_data {
    int syn_count;    /**< Incremented by one on encountering
                           a SYN or SYN-ACK packet */
    te_bool failed;   /**< Will be set to TRUE if processing
                           of some packets was not successful
                           in the callback */
} csap_data;

/**
 * CSAP callback processing packets sent from IUT.
 *
 * @param pkt         Captured packet.
 * @param user_data   Pointer to csap_data.
 */
static void
csap_cb(asn_value *pkt, void *user_data)
{
    csap_data *data = (csap_data *)user_data;
    uint32_t flags;
    te_errno rc;

    rc = asn_read_uint32(pkt, &flags, "pdus.0.#tcp.flags");
    if (rc != 0)
    {
        data->failed = TRUE;
        ERROR("Failed to obtain TCP flags, rc=%r", rc);
        goto cleanup;
    }
    else if (flags & TCP_SYN_FLAG)
    {
        data->syn_count++;
    }

    RING("Captured packet with TCP flags=0x%x (SYN flag is %sset)",
         flags, ((flags & TCP_SYN_FLAG) ? "" : "not "));

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
    const struct if_nameindex *tst_if = NULL;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    rpc_zftl_p iut_zftl = RPC_NULL;
    int tst_s = -1;

    csap_handle_t csap = CSAP_INVALID_HANDLE;
    tapi_tad_trrecv_cb_data cb_data;
    csap_data user_data;
    unsigned int pkts_num = 0;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(tst_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    /* Do not print out packets captured by CSAP. */
    rcf_tr_op_log(FALSE);

    TEST_STEP("Create a CSAP on Tester host to capture packets sent "
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
    SLEEP(2);

    TEST_STEP("Create ZF stack on IUT.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Wait for 2 seconds.");
    SLEEP(2);

    TEST_STEP("Create TCP Zocket on IUT and TCP socket on Tester, "
              "establish TCP connection according to @p open_method.");

    zfts_establish_tcp_conn_ext2(open_method,
                                 pco_iut, attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    TEST_STEP("Check sending/receiving in both directions over the "
              "established connection.");
    zfts_zft_check_connection(pco_iut, stack, iut_zft, pco_tst, tst_s);

    TEST_STEP("Check packets captured by the CSAP; only the single SYN "
              "or SYN-ACK should be sent from IUT.");

    memset(&cb_data, 0, sizeof(cb_data));
    cb_data.callback = &csap_cb;
    cb_data.user_data = &user_data;
    memset(&user_data, 0, sizeof(user_data));

    CHECK_RC(tapi_tad_trrecv_stop(pco_tst->ta, 0, csap, &cb_data,
                                  &pkts_num));
    if (pkts_num == 0)
        TEST_VERDICT("CSAP has not captured any packets");

    if (user_data.failed)
        TEST_STOP;

    if (user_data.syn_count == 0)
        TEST_VERDICT("No SYN packets was detected");
    else if (user_data.syn_count > 1)
        TEST_VERDICT("Unnecessary SYN retransmit was detected");

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack);
    ZFTS_FREE(pco_iut, zf_attr, attr);
    CLEANUP_RPC_ZF_DEINIT(pco_iut);

    CLEANUP_CHECK_RC(tapi_tad_csap_destroy(pco_tst->ta, 0, csap));

    TEST_END;
}
