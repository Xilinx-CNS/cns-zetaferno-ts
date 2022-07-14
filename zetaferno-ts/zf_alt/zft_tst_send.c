/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-zft_tst_send Receive data from the peer while there is queued data in alternatives.
 *
 * @objective Queue some data to alternatives, send data from tester, send
 *            alternatives.
 *
 * @param little        Send one 1-byte data packet @c TRUE, else
 *                      send a lot of data.
 * @param open_method   How to open connection:
 *                      - @c active
 *                      - @c passive
 *                      - @c passive_close (close listener after
 *                        passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "zf_alt/zft_tst_send"

#include "zf_test.h"
#include "rpc_zf.h"

/*
 * How much data to pass to send() on Tester, if it is required
 * to send a lot of data.
 */
#define PKT_LEN 10240

/*
 * How many times to call send() on Tester, if it is required
 * to send a lot of data.
 */
#define PKTS_NUM 10

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p iut_zftl = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    int       tst_s = -1;

    rpc_zf_althandle *alts;
    int               n_alts;
    int               alt_count_max;
    rpc_iovec        *iovs;
    int               i;
    int               j;

    char send_buf[PKT_LEN];

    te_dbuf           tst_sent = TE_DBUF_INIT(0);
    te_dbuf           tst_received = TE_DBUF_INIT(0);
    te_dbuf           iut_sent = TE_DBUF_INIT(0);
    te_dbuf           iut_received = TE_DBUF_INIT(0);

    te_bool                 little;
    zfts_conn_open_method   open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(little);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    CHECK_RC(tapi_sh_env_get_int(pco_iut, "ZFTS_ALT_COUNT_DEF",
                                 &alt_count_max));

    alts = (rpc_zf_althandle *)tapi_calloc(sizeof(*alts), alt_count_max);
    iovs = (rpc_iovec *)tapi_calloc(sizeof(*iovs), alt_count_max);

    te_fill_buf(send_buf, PKT_LEN);

    n_alts = rand_range(1, alt_count_max);
    for (i = 0; i < n_alts; i++)
    {
        rpc_make_iov(&iovs[i], 1, 1, ZFTS_TCP_DATA_MAX);
    }

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection to get zft zocket. */
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- Allocate random number [1; max] of alternatives. */
    for (i = 0; i < n_alts; i++)
    {
        rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alts[i]);
    }

    /*- Queue data to all alternatives. */
    for (i = 0; i < n_alts; i++)
    {
        ZFTS_CHECK_RPC(rpc_zft_alternatives_queue(pco_iut, iut_zft,
                                                  alts[i], &iovs[i], 1, 0),
                      pco_iut, "zft_alternatives_queue()");
    }

    /*- Send data from Tester:
     *  - if @p little is @c TRUE send one 1-byte data packet;
     *  - else send a lot of data (100KB);
     *  - read and check data on IUT. */

    if (little)
    {
        rpc_send(pco_tst, tst_s, send_buf, 1, 0);
        te_dbuf_append(&tst_sent, send_buf, 1);
    }
    else
    {
        for (j = 0; j < PKTS_NUM; j++)
        {
            rpc_send(pco_tst, tst_s, send_buf, PKT_LEN, 0);
            te_dbuf_append(&tst_sent, send_buf, PKT_LEN);
            rpc_zf_process_events(pco_iut, stack);
        }
    }

    ZFTS_WAIT_NETWORK(pco_iut, stack);
    zfts_zft_read_all(pco_iut, stack, iut_zft, &iut_received);
    ZFTS_CHECK_RECEIVED_DATA(iut_received.ptr, tst_sent.ptr,
                             iut_received.len, tst_sent.len,
                             " from Tester");

    /*- Send data from random alternative queue. */
    i = rand_range(0, n_alts - 1);
    ZFTS_CHECK_RPC(rpc_zf_alternatives_send(pco_iut, stack, alts[i]),
                   pco_iut, "zf_alternatives_send()");
    ZFTS_CHECK_RPC(rpc_zf_process_events(pco_iut, stack),
                   pco_iut,
                   "rpc_zf_process_events() after "
                   "sending ZF alternative");
    te_dbuf_append(&iut_sent, iovs[i].iov_base, iovs[i].iov_len);

    /*- Send some more data using @b zft_send(). */
    for (i = 0; i < n_alts; i++)
    {
        rc = rpc_zft_send(pco_iut, iut_zft, &iovs[i], 1, 0);
        rpc_zf_process_events(pco_iut, stack);
        te_dbuf_append(&iut_sent, iovs[i].iov_base, rc);
    }

    /*- Read and check data on Tester. */
    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s,
                           &tst_received);
    ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr, iut_sent.ptr,
                             tst_received.len, iut_sent.len,
                             " from IUT");

    TEST_SUCCESS;

cleanup:

    for (i = 0; i < n_alts; i++)
    {
        CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alts[i]);
        rpc_release_iov(&iovs[i], 1);
    }

    te_dbuf_free(&tst_sent);
    te_dbuf_free(&tst_received);
    te_dbuf_free(&iut_sent);
    te_dbuf_free(&iut_received);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
