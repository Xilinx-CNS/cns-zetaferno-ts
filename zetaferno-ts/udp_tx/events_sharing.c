/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP TX tests
 *
 * $Id:$
 */

/**
 * @page udp_tx-events_sharing Events queue sharing by TX and RX zockets.
 *
 * @objective Create RX and TX zockets in single stack, send and receive a few
 *            datagrams using the zockets, then process all events. Check data
 *            is delivered in both directions.
 *
 * @param func          Transmitting function.
 * @param large_buffer  Use large data buffer to send.
 * @param few_iov       Use several iov vectors.
 * @param in_num        Number of incoming datagrams:
 *                      - @c 10
 * @param out_num       Number of outgoing datagrams:
 *                      - @c 10
 * @param send_first Send or receive datagrams first.
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME "udp_tx/events_sharing"

#include "zf_test.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    zfts_send_function     func;
    te_bool                large_buffer;
    te_bool                few_iov;
    te_bool                send_first;
    int                    in_num;
    int                    out_num;

    rpc_zf_attr_p    attr = RPC_NULL;
    rpc_zf_stack_p   stack = RPC_NULL;
    rpc_zfur_p       urx = RPC_NULL;
    rpc_zfut_p       utx = RPC_NULL;
    rpc_iovec       *sndiov_tst;
    zfts_zfut_buf   *snd;
    rpc_iovec       *rcviov;
    int rx_num;
    int tst_s;
    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);
    TEST_GET_INT_PARAM(in_num);
    TEST_GET_INT_PARAM(out_num);
    TEST_GET_BOOL_PARAM(send_first);

    snd = tapi_calloc(out_num, sizeof(*snd));
    sndiov_tst = tapi_calloc(in_num, sizeof(*sndiov_tst));

    for (i = 0; i < out_num; i++)
        zfts_zfut_buf_make_param(large_buffer, few_iov, snd + i);

    rpc_make_iov(sndiov_tst, in_num, 1, ZFTS_DGRAM_MAX);

    rx_num = in_num > out_num ? in_num : out_num;
    rcviov = tapi_calloc(rx_num , sizeof(*rcviov));
    rpc_make_iov(rcviov, rx_num, ZFTS_DGRAM_LARGE, ZFTS_DGRAM_LARGE);

    /*- Create UDP TX and RX zockets in single ZF stack. */
    zfts_create_stack(pco_iut, &attr, &stack);
    rpc_zfur_alloc(pco_iut, &urx, stack, attr);
    rpc_zfur_addr_bind(pco_iut, urx, SA(iut_addr), tst_addr, 0);
    rpc_zfut_alloc(pco_iut, &utx, stack, iut_addr, tst_addr, 0, attr);

    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);
    rpc_connect(pco_tst, tst_s, iut_addr);

    /*- The following two actions order is dependent on @p send_first : */
    /*-- send a few @p in_num datagrams from tester; */
    if (!send_first)
        zfts_sendto_iov(pco_tst, tst_s, sndiov_tst, in_num, NULL);

    /*-- send a few @p out_num datagrams from TX zocket. */
    for (i = 0; i < out_num; i++)
    {
        zfts_zfut_send_ext(pco_iut, stack, utx, snd + i, func);
    }

    if (send_first)
        zfts_sendto_iov(pco_tst, tst_s, sndiov_tst, in_num, NULL);

    /*- Read and check datagrams on tester. */
    for (i = 0; i < out_num; i++)
    {
        rcviov[i].iov_len = rpc_recv(pco_tst, tst_s, rcviov[i].iov_base,
                                     rcviov[i].iov_rlen, 0);

        if (rcviov[i].iov_len != snd[i].len)
            TEST_VERDICT("Datagram with incorrect length was received");
        CHECK_BUFS_EQUAL(rcviov[i].iov_base, snd[i].data, snd[i].len);
    }

    /*- Process all events on the stack. */
    /*- Read and check datagrams on RX zocket. */
    zfts_zfur_zc_recv_all(pco_iut, stack, urx, rcviov, in_num);
    rpc_iovec_cmp_strict(sndiov_tst, rcviov, in_num);

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, utx);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, urx);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
