/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id:$
 */

/**
 * @page udp_rx-locked_buffers Buffers are locked until @c zfur_zc_recv_done is performed
 *
 * @objective  Receive some data on UDP RX zocket usng @c rpc_zfur_zc_recv(),
 *             but do not call @c rpc_zfur_zc_recv_done(). Overfill RX
 *             buffer of the second zocket, send/receive some data using aux
 *             UDP zockets. Check that read buffers of the first RX zocket
 *             are left untouched after all.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param iut_addr      IUT address.
 * @param tst_addr      Tester address.
 * @param same_laddr    Use the same @b laddr for UDP TX zocket.
 * @param send_func     Determine function to send datagrams from UDP zocket.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/locked_buffers"

#include "zf_test.h"
#include "rpc_zf.h"

/* Zockets number. */
#define ZOCKETS_NUM 3

/* Iterations number in the loop to send/receive datagrams. */
#define ITERATIONS_NUM 20

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    te_bool                same_laddr;
    zfts_send_function     send_func;

    struct sockaddr *iut_addr_bind[ZOCKETS_NUM] = {};
    rpc_zf_stack_p    stack = RPC_NULL;
    rpc_zfur_p        iut_urx1 = RPC_NULL;
    rpc_zfur_p        iut_urx2 = RPC_NULL;
    rpc_zfut_p        iut_utx = RPC_NULL;
    rpc_zf_attr_p     attr = RPC_NULL;
    rpc_iovec         sndiov[ZFTS_MAX_DGRAMS_NUM];
    rpc_iovec         rcviov_read[ZFTS_MAX_DGRAMS_NUM];
    rpc_iovec         rcviov_check[ZFTS_MAX_DGRAMS_NUM];
    rpc_zfur_msg      msg_read = { .iovcnt = ZFTS_MAX_DGRAMS_NUM,
                                   .iov = rcviov_read };
    rpc_zfur_msg      msg_check = { .iovcnt = ZFTS_MAX_DGRAMS_NUM,
                                    .iov = rcviov_check };

    int tst_s;
    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(same_laddr);
    ZFTS_TEST_GET_ZFUT_FUNCTION(send_func);

    rpc_make_iov(sndiov, ZFTS_MAX_DGRAMS_NUM, 1, ZFTS_DGRAM_MAX);
    rpc_make_iov(rcviov_read, ZFTS_MAX_DGRAMS_NUM, ZFTS_DGRAM_MAX,
                 ZFTS_DGRAM_MAX);
    rpc_make_iov(rcviov_check, ZFTS_MAX_DGRAMS_NUM, ZFTS_DGRAM_MAX,
                 ZFTS_DGRAM_MAX);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    for (i = 0; i < ZOCKETS_NUM; i++)
    {
        iut_addr_bind[i] = tapi_sockaddr_clone_typed(iut_addr,
                                                     TAPI_ADDRESS_SPECIFIC);
        CHECK_RC(tapi_allocate_set_port(pco_iut, iut_addr_bind[i]));
    }

    /*- Allocate and bind two UDP RX zockets. */
    rpc_zfur_alloc(pco_iut, &iut_urx1, stack, attr);
    rpc_zfur_addr_bind(pco_iut, iut_urx1, iut_addr_bind[0], tst_addr, 0);
    rpc_zfur_alloc(pco_iut, &iut_urx2, stack, attr);
    rpc_zfur_addr_bind(pco_iut, iut_urx2, iut_addr_bind[1], tst_addr, 0);

    /*- Allocate and bind Zetaferno UDP TX zocket. */
    rpc_zfut_alloc(pco_iut, &iut_utx, stack,
                   same_laddr ? iut_addr_bind[0] : iut_addr_bind[2],
                   tst_addr, 0, attr);

    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    /*- Send from tester a few datagrams to the first RX zocket. */
    zfts_sendto_iov(pco_tst, tst_s, sndiov, ZFTS_MAX_DGRAMS_NUM,
                    iut_addr_bind[0]);

    /*- Process the stack events and receive datagrams on the first RX
     * zocket using @c rpc_zfur_zc_recv(). */
    rpc_zf_process_events(pco_iut, stack);
    rpc_zfur_zc_recv(pco_iut, iut_urx1, &msg_read, 0);
    rpc_iovec_cmp_strict(sndiov, rcviov_read, msg_read.iovcnt);

    /*- Overfill RX buffer of the second RX zocket. */
    zfts_sendto_iov(pco_tst, tst_s, sndiov, ZFTS_MAX_DGRAMS_NUM,
            iut_addr_bind[1]);

    /*- Completely read all datagrams of the second RX zocket. */
    zfts_zfur_zc_recv_all(pco_iut, stack, iut_urx2, rcviov_check,
                          msg_check.iovcnt);
    rpc_iovec_cmp_strict(sndiov, rcviov_check, msg_check.iovcnt);

    /*- Send/receive some datagrams using TX and the second RX zockets. */
    for (i = 0; i < ITERATIONS_NUM; i++)
    {
        if (rand_range(0,1))
            zfts_zfur_check_reception(pco_iut, stack, iut_urx2, pco_tst,
                                      tst_s, iut_addr_bind[1]);
        else
            zfts_zfut_check_send_func(pco_iut, stack, iut_utx, pco_tst,
                                      tst_s, send_func, FALSE, FALSE);
    }

    /*- Check that data which were read from the first RX zocket stays
     * unchanged. */
    rpc_zfur_read_zfur_msg(pco_iut, msg_read.ptr, &msg_check);

    /*- Perform @c rpc_zfur_zc_recv_done() to release buffers. */
    rpc_zfur_zc_recv_done(pco_iut, iut_urx1, &msg_read);

    rpc_iovec_cmp_strict(msg_read.iov, msg_check.iov, msg_read.iovcnt);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_urx1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_urx2);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_utx);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    TEST_END;
}
