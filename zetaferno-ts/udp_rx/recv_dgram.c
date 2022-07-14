/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id:$
 */

/**
 * @page udp_rx-recv_dgram Datagrams reception using Zetaferno Direct API
 *
 * @objective  Read a datagram using Zetaferno Direct API receive functions.
 *
 * @param pco_iut   PCO on IUT.
 * @param pco_tst   PCO on TST.
 * @param func      Receiving function.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/recv_dgram"

#include "zf_test.h"
#include "rpc_zf.h"

/* Timeout to sleep before another attempt to read a datagram. */
#define ATTEMPTS_SLEEP 10

/* Maximum number of attempts reading datagrams. */
#define MAX_ATTEMPTS 100

/**
 * Read datagrams using @b rpc_zfur_zc_recv. Do all required routines to
 * read received datagrams.
 *
 * @param rpcs      RPC server handle.
 * @param urx       ZF UDP RX zocket.
 * @param iov       Iov vectors array.
 * @param iovcnt    The array length.
 */
static void
zc_recv(rcf_rpc_server *rpcs, rpc_zfur_p urx, rpc_iovec *iov, int iovcnt)
{
    rpc_zfur_msg msg;
    int i;

    for (i = 0; i < iovcnt; i++)
    {
        memset(&msg, 0, sizeof(msg));
        msg.iovcnt = iovcnt - i;
        msg.iov = iov + i;

        rpc_zfur_zc_recv(rpcs, urx, &msg, 0);
        if (msg.iovcnt == 0)
            TEST_VERDICT("Too few datagrams were received");
        if (msg.iovcnt > 1)
            TEST_VERDICT("Too many datagrams were received by one call");

        rpc_zfur_pkt_get_header(rpcs, urx, &msg, NULL, 0, NULL, 0);
        rpc_zfur_zc_recv_done(rpcs, urx, &msg);

        if (msg.dgrams_left != iovcnt - i - 1)
        {
            ERROR("dgrams_left value is %d instead of %d", msg.dgrams_left,
                  iovcnt - i - 1);
            TEST_VERDICT("Incorrect dgrams_left value was returned");
        }

    }

    rpc_zfur_zc_recv(rpcs, urx, &msg, 0);
    if (msg.iovcnt != 0)
        TEST_VERDICT("zfur_zc_recv unexpectedly returned iovcnt > 0");
    if (msg.dgrams_left != 0)
        TEST_VERDICT("zfur_zc_recv unexpectedly returned dgrams_left > 0");
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    zfts_recv_function     func;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zfur_p     iut_s = RPC_NULL;
    rpc_iovec      rcviov[ZFTS_IOVCNT];
    rpc_iovec      sndiov[ZFTS_IOVCNT];
    int tst_s;
    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(func, ZFTS_RECV_FUNCTIONS);

    rpc_make_iov(sndiov, ZFTS_IOVCNT, 1, ZFTS_DGRAM_MAX);
    rpc_make_iov(rcviov, ZFTS_IOVCNT, ZFTS_DGRAM_MAX, ZFTS_DGRAM_MAX);

    /*- Allocate Zetaferno attributes and stack. */
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Allocate Zetaferno UDP RX zocket. */
    rpc_zfur_alloc(pco_iut, &iut_s, stack, attr);

    /*- Bind the zocket using both local and remote addresses. */
    rpc_zfur_addr_bind(pco_iut, iut_s, SA(iut_addr), tst_addr, 0);

    /*- Create UDP socket on tester. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);
    rpc_connect(pco_tst, tst_s, iut_addr);

    /*- Send a few datagrams from the tester. */
    for (i = 0; i < ZFTS_IOVCNT; i++)
        rpc_send(pco_tst, tst_s, sndiov[i].iov_base, sndiov[i].iov_len, 0);

    ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Receive datagrams on IUT using ZF UDP RX functions. */
    switch (func)
    {
        case ZFTS_ZFUR_ZC_RECV:
            zc_recv(pco_iut, iut_s, rcviov, ZFTS_IOVCNT);
            break;

        default:
            TEST_FAIL("Invalid receive function");
    }

    /*- Check received datagrams length and data. */
    rpc_iovec_cmp_strict(sndiov, rcviov, ZFTS_IOVCNT);

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    rpc_release_iov(rcviov, ZFTS_IOVCNT);
    rpc_release_iov(sndiov, ZFTS_IOVCNT);

    TEST_END;
}
