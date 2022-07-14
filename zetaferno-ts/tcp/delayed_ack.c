/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-delayed_ack Test tcp_delayed_ack attribute
 *
 * @objective Check that tcp_delayed_ack attribute works as
 *            expected.
 *
 * @param pco_iut                   PCO on IUT.
 * @param pco_tst                   PCO on TST.
 * @param iut_addr                  IUT address.
 * @param tst_addr                  Tester address.
 * @param tcp_delayed_ack           Value of tcp_delayed_ack
 *                                  attribute to set:
 *                                  - do not set (default value is @c 0);
 *                                  - @c 0;
 *                                  - @c 1.
 * @param active                    Whether TCP connection should be opened
 *                                  actively or passively.
 * @param delay_reactor             If @c TRUE, call @a zf_reactor_perform()
 *                                  after packets were sent, otherwise
 *                                  call it repeatedly in the same time as
 *                                  packets are sent.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "tcp/delayed_ack"

#include "zf_test.h"
#include "rpc_zf.h"

#include "tapi_tad.h"
#include "tapi_ip4.h"
#include "tapi_tcp.h"

#include "te_dbuf.h"

/* Number of packets to send. */
#define PACKETS_NUM           10
/* Length of a test packet. */
#define PACKET_LEN            100
/* How long to iterate @a zf_reactor_perform(), in milliseconds. */
#define EVENTS_TIMEOUT        1000

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p           iut_zft = RPC_NULL;
    int                 tst_s = -1;
    int                 val = 0;

    int       tcp_delayed_ack = -1;
    te_bool   active = FALSE;
    te_bool   delay_reactor = FALSE;

    csap_handle_t sniff_csap;
    int           sniff_sid;
    asn_value    *sniff_pattern;
    unsigned int  sniff_num = 0;
    unsigned int  i;
    char          buf[PACKET_LEN];

    const struct if_nameindex *tst_if = NULL;

    te_dbuf       sent_data = TE_DBUF_INIT(0);
    te_dbuf       received_data = TE_DBUF_INIT(0);

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_IF(tst_if);
    TEST_GET_INT_PARAM(tcp_delayed_ack);
    TEST_GET_BOOL_PARAM(active);
    TEST_GET_BOOL_PARAM(delay_reactor);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Set attribute @b tcp_delayed_ack to
     * @p tcp_delayed_ack. */
    if (tcp_delayed_ack < 0)
        tcp_delayed_ack = 1; /* Default value */
    else
        rpc_zf_attr_set_int(pco_iut, attr, "tcp_delayed_ack",
                            tcp_delayed_ack);

    /*- Allocate Zetaferno stack. */
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Establish TCP connection actively or passively in dependence
     * on @p active. */
    zfts_establish_tcp_conn(active, pco_iut, attr, stack,
                            &iut_zft, iut_addr,
                            pco_tst, &tst_s, tst_addr);

    /* This is done to be sure that each @a send() call results
     * in sending new packet. */
    val = 1;
    rpc_setsockopt(pco_tst, tst_s, RPC_TCP_NODELAY, &val);

    /*- Create and run CSAP on Tester to count ACKs. */
    rcf_ta_create_session(pco_tst->ta, &sniff_sid);

    CHECK_RC(tapi_tcp_ip4_eth_csap_create(pco_tst->ta, sniff_sid,
                                          tst_if->if_name,
                                          TAD_ETH_RECV_DEF |
                                          TAD_ETH_RECV_NO_PROMISC,
                                          NULL, NULL,
                                          SIN(tst_addr)->sin_addr.s_addr,
                                          SIN(iut_addr)->sin_addr.s_addr,
                                          SIN(tst_addr)->sin_port,
                                          SIN(iut_addr)->sin_port,
                                          &sniff_csap));

    CHECK_RC(tapi_tcp_pattern(0, 0, FALSE, FALSE, &sniff_pattern));

    asn_free_subvalue(sniff_pattern, "0.pdus.0.#tcp.flags");

    CHECK_RC(asn_write_int32(sniff_pattern, TCP_ACK_FLAG,
                             "0.pdus.0.#tcp.flags.#mask.v"));
    CHECK_RC(asn_write_int32(sniff_pattern, TCP_ACK_FLAG,
                             "0.pdus.0.#tcp.flags.#mask.m"));

    CHECK_RC(tapi_tad_trrecv_start(pco_tst->ta, sniff_sid, sniff_csap,
                                   sniff_pattern, TAD_TIMEOUT_INF, 0,
                                   RCF_TRRECV_COUNT));

    /*- If not @p delay_reactor, start calling @a zf_reactor_perform()
     * repeatedly with help of @a rpc_zf_process_events_long(). */
    if (!delay_reactor)
    {
        pco_iut->op = RCF_RPC_CALL;
        rpc_zf_process_events_long(pco_iut, stack, EVENTS_TIMEOUT);
    }

    /*- Send @c PACKETS_NUM packets from Tester. */
    for (i = 0; i < PACKETS_NUM; i++)
    {
        te_fill_buf(buf, PACKET_LEN);
        rc = rpc_send(pco_tst, tst_s, buf, PACKET_LEN, 0);
        if (rc != PACKET_LEN)
            TEST_FAIL("send() returned %d instead of %d",
                      rc, PACKET_LEN);
        te_dbuf_append(&sent_data, buf, rc);
    }

    /*- If @p delay_reactor, call zf_reactor_perform()
     * repeatedly for a while to process all the events. Otherwise,
     * wait until rpc_zf_process_events_long() terminates.*/
    if (delay_reactor)
    {
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }
    else
    {
        pco_iut->op = RCF_RPC_WAIT;
        rpc_zf_process_events_long(pco_iut, stack, EVENTS_TIMEOUT);
    }

    TAPI_WAIT_NETWORK;

    rcf_ta_trrecv_stop(pco_tst->ta, sniff_sid, sniff_csap,
                       NULL, NULL, &sniff_num);

    RING("%u ACKs were received", sniff_num);

    /*- Check that if TCP delayed ACK attribute is disabled, then
     * the number of ACKs received is the same as the number of
     * packets sent; and if TCP delayed ACK attribute is enabled,
     * then the number of ACKs received is less than the number of
     * packets sent. */
    if (sniff_num > PACKETS_NUM)
        TEST_VERDICT("More ACKs were received than packets were sent");
    else if (sniff_num < PACKETS_NUM && tcp_delayed_ack == 0)
        TEST_VERDICT("tcp_delayed_ack is disabled, but less ACKs "
                     "were received than packets were sent");
    else if (sniff_num == PACKETS_NUM && tcp_delayed_ack == 1)
        TEST_VERDICT("tcp_delayed_ack is enabled, but for each sent packet "
                     "an ACK was received");

    /*- Receive data on IUT and check it. */
    zfts_zft_read_data(pco_iut, iut_zft, &received_data, NULL);

    ZFTS_CHECK_RECEIVED_DATA(received_data.ptr, sent_data.ptr,
                             received_data.len, sent_data.len,
                             " from Tester");

    TEST_SUCCESS;

cleanup:

    CLEANUP_CHECK_RC(tapi_tad_csap_destroy(pco_tst->ta, sniff_sid,
                                           sniff_csap));

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    te_dbuf_free(&sent_data);
    te_dbuf_free(&received_data);

    TEST_END;
}
