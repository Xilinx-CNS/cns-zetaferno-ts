/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 */

/**
 * @page udp_rx-buffers_number Use various buffers number (iovcnt) in random order
 *
 * @objective  Read datagrams using various buffers number (iovcnt) in
 *             random order a lot of times, check for resources leakage.
 *
 * @param env           Testing environment:
 *                      - @ref arg_types_env_peer2peer
 * @param bunches_num   Datagrams bunches number to be sent.
 * @param bunch_size    Datagrams number to be sent in one bunch.
 * @param iovcnt        Iov vectors number to receive datagrams or @c -1 to
 *                      use random value in range [1, bunch_size + 10] for
 *                      each bunch.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/buffers_number"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_mem.h"

/* Extra iov buffers number. */
#define EXTRA_IOVEC_NUM 10

/* Disable/enable RPC logging. */
#define VERBOSE_LOGGING FALSE

/**
 * Read all available datagrams and check that they correspond to sent.
 *
 * @param rpcs          RPC server handle.
 * @param urx           UDP RX zocket.
 * @param muxer_set     Muxer used to wait until datagrams arrive.
 * @param rcviov        Iov vectors with receive buffers.
 * @param rcviovcnt     Receive iov vectors number.
 * @param sndiov        Sent datagrams array.
 * @param bunch_size    Datagrams number in the sent bunch.
 * @param use_rand      Use random value @b zfur_msg.iovcnt if @c TRUE.
 */
static void
read_check_datagrams(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                     rpc_zf_muxer_set_p muxer_set,
                     rpc_iovec *rcviov, int rcviovcnt, rpc_iovec *sndiov,
                     int bunch_size, te_bool use_rand)
{
    int rxn = 0;
    int rx_len;
    size_t res;
    te_bool muxer_called;

    zfts_zock_descr zock_descrs[] = {
                        { &urx, "UDP RX zocket" },
                        { NULL, "" }
                        };

    ZFTS_CHECK_MUXER_EVENTS(
                 rpcs, muxer_set,
                 "Before receiving any datagrams",
                 -1, zock_descrs,
                 { RPC_EPOLLIN, { .u32 = urx } });
    muxer_called = TRUE;

    do {
        if (use_rand)
            rx_len = rand_range(1, rcviovcnt);
        else
            rx_len = rcviovcnt;

        res = zfts_zfur_zc_recv(rpcs, urx, rcviov, rx_len);
        if (res == 0)
        {
            if (muxer_called)
            {
                TEST_VERDICT("ZF muxer reported EPOLLIN but no "
                             "datagrams are retrieved");
            }

            ZFTS_CHECK_MUXER_EVENTS(
                         rpcs, muxer_set,
                         "After zero datagrams were retrieved",
                         -1, zock_descrs,
                         { RPC_EPOLLIN, { .u32 = urx } });
            muxer_called = TRUE;
            continue;
        }

        muxer_called = FALSE;

        rpc_iovec_cmp_strict(sndiov + rxn, rcviov, res);

        rxn += res;
    } while (rxn < bunch_size);

    res = zfts_zfur_zc_recv(rpcs, urx, rcviov, 1);
    if (res != 0)
        TEST_VERDICT("Extra datagram is received");
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    int                    bunches_num;
    int                    bunch_size;

    rpc_zf_stack_p    stack = RPC_NULL;
    rpc_zfur_p        iut_s = RPC_NULL;
    rpc_zf_attr_p     attr = RPC_NULL;
    rpc_iovec        *sndiov;
    rpc_iovec        *rcviov;
    te_bool           use_rand = FALSE;

    int tst_s;
    int iovcnt;
    int i;

    rpc_zf_muxer_set_p muxer_set = RPC_NULL;
    rpc_zf_waitable_p iut_waitable = RPC_NULL;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(bunches_num);
    TEST_GET_INT_PARAM(bunch_size);
    TEST_GET_INT_PARAM(iovcnt);

    if (iovcnt == -1)
    {
        iovcnt = bunch_size + EXTRA_IOVEC_NUM;
        use_rand = TRUE;
    }

    TEST_STEP("Allocate ZF attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Create and bind UDP RX zocket on IUT.");
    rpc_zfur_alloc(pco_iut, &iut_s, stack, attr);
    rpc_zfur_addr_bind(pco_iut, iut_s, SA(iut_addr), tst_addr, 0);

    sndiov = tapi_calloc(bunch_size, sizeof(*sndiov));
    rcviov = tapi_calloc(iovcnt, sizeof(*rcviov));
    rpc_make_iov(rcviov, iovcnt, ZFTS_DGRAM_MAX, ZFTS_DGRAM_MAX);

    TEST_STEP("Create and bind Tester UDP socket.");
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    TEST_STEP("Allocate and configure ZF muxer on IUT to wait for "
              "@c EPOLLIN on the IUT UDP zocket.");
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    iut_waitable = rpc_zfur_to_waitable(pco_iut, iut_s);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_waitable,
                            iut_s, RPC_EPOLLIN);

    if (!VERBOSE_LOGGING)
    {
        pco_iut->silent_pass = pco_iut->silent_pass_default = TRUE;
        pco_tst->silent_pass = pco_tst->silent_pass_default = TRUE;
    }

    TEST_STEP("In a loop do @p bunches_num times:");
    TEST_SUBSTEP("Send a bunch of @p bunch_size datagrams from Tester.");
    TEST_SUBSTEP("With help of ZF muxer wait for datagrams arrival on "
                 "IUT, receive and check them.");
    TEST_SUBSTEP("When receiving datagrams, pass number of IO vectors "
                 "to @b zfts_zfur_zc_recv() computed according to "
                 "@p iovcnt parameter. Call @b zfts_zfur_zc_recv() "
                 "as many times as necessary to receive all the "
                 "datagrams from the bunch.");

    for (i = 0; i < bunches_num; i++)
    {
        RING("Bunch number %d, sent datagrams number %d", i, i * bunch_size);

        rpc_make_iov(sndiov, bunch_size, 1, ZFTS_DGRAM_MAX);
        zfts_sendto_iov(pco_tst, tst_s, sndiov, bunch_size, iut_addr);

        read_check_datagrams(pco_iut, iut_s, muxer_set, rcviov, iovcnt,
                             sndiov, bunch_size, use_rand);

        rpc_release_iov(sndiov, bunch_size);
    }

    TEST_SUCCESS;

cleanup:
    pco_iut->silent_pass = pco_iut->silent_pass_default = FALSE;
    pco_tst->silent_pass = pco_tst->silent_pass_default = FALSE;

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_waitable);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    TEST_END;
}
