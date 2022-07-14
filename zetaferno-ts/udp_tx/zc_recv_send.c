/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP TX tests
 *
 * $Id:$
 */

/**
 * @page udp_tx-zc_recv_send Send zero-copy received datagram.
 *
 * @objective Receive a datagram using zero-copy receive, then send the
 *            received buffer using TX zocket.
 *
 * @param func          Transmitting function.
 * @param single_stack  Use single stack to receive and send datagram.
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME "udp_tx/zc_recv_send"

#include "zf_test.h"

/* Datagrams number to send. */
#define DGRAMS_NUM 1

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    zfts_send_function     func;
    te_bool                few_iov;
    te_bool                single_stack;

    rpc_zf_attr_p    attr = RPC_NULL;
    rpc_zf_stack_p   stack_rx = RPC_NULL;
    rpc_zf_stack_p   stack_tx = RPC_NULL;
    rpc_zfur_p       urx = RPC_NULL;
    rpc_zfut_p       utx = RPC_NULL;
    rpc_iovec        rcviov_read[DGRAMS_NUM];
    rpc_zfur_msg     msg = { .iovcnt = DGRAMS_NUM, .iov = rcviov_read };
    zfts_zfut_buf    snd;
    int tst_s;
    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);
    TEST_GET_BOOL_PARAM(few_iov);
    TEST_GET_BOOL_PARAM(single_stack);

    zfts_zfut_buf_make_param(FALSE, few_iov, &snd);
    rpc_make_iov(rcviov_read, DGRAMS_NUM, ZFTS_DGRAM_MAX, ZFTS_DGRAM_MAX);

    /*- Allocate Zetaferno stacks - one or two in dependence on
     * @p single_stack. */
    zfts_create_stack(pco_iut, &attr, &stack_rx);
    if (single_stack)
        stack_tx = stack_rx;
    else
        rpc_zf_stack_alloc(pco_iut, attr, &stack_tx);

    /*- Allocate UDP RX and TX zocket in single or different stacks. */
    rpc_zfur_alloc(pco_iut, &urx, stack_rx, attr);
    rpc_zfur_addr_bind(pco_iut, urx, SA(iut_addr), tst_addr, 0);
    rpc_zfut_alloc(pco_iut, &utx, stack_tx, iut_addr, tst_addr, 0, attr);

    /*- Send a datagram from tester. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);
    rpc_connect(pco_tst, tst_s, iut_addr);
    zfts_sendto_iov(pco_tst, tst_s, snd.iov, snd.iovcnt, NULL);

    /*- Process events until we get one. */
    ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack_rx);

    /*- In the single RPC call: */
    /*-- read the datagram on IUT RX zocket using @a zfur_zc_recv() */
    /*-- send the received datagram back to tester using TX zocket,
     *   use buffers returned by @a zfur_zc_recv(); */
    rpc_zfur_zc_recv_send(pco_iut, urx, &msg, utx, func);

    /*- Process all events. */
    rpc_zf_process_events(pco_iut, stack_rx);
    if (!single_stack)
        rpc_zf_process_events(pco_iut, stack_tx);

    /*- Check data received on RX zocket. */
    rpc_iovec_cmp_strict(snd.iov, rcviov_read, snd.iovcnt);

    /*- Perform @a zfur_zc_recv_done() on the RX zocket. */
    rpc_zfur_zc_recv_done(pco_iut, urx, &msg);

    /*- Read and check data on tester. */
    if (func == ZFTS_ZFUT_SEND_SINGLE)
    {
        for (i = 0; i < (int)snd.iovcnt; i++)
            rcviov_read[i].iov_len = rpc_recv(pco_tst, tst_s,
                                               rcviov_read[i].iov_base,
                                               rcviov_read[i].iov_rlen, 0);
        rpc_iovec_cmp_strict(snd.iov, rcviov_read, snd.iovcnt);
    }
    else
    {
        char *recvbuf = tapi_calloc(snd.len, 1);

        rc = rpc_recv(pco_tst, tst_s, recvbuf, snd.len, 0);
        if (rc != (int)snd.len)
            TEST_VERDICT("Incomplete data was received");
        CHECK_BUFS_EQUAL(snd.data, recvbuf, snd.len);
    }

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, utx);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, urx);
    if (!single_stack)
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack_tx);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack_rx);

    TEST_END;
}
