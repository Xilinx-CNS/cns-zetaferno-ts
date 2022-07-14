/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id:$
 */

/**
 * @page udp_rx-overfill_receive_queue Examine receive queue overfilling
 *
 * @objective  Transmit data from tester to a few UDP RX zockets until
 *             receive buffers are overfilled, read all the data and check
 *             that read datagrams correspond to first sent, i.e. no loss
 *             while receive queue is not overfilled.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param iut_addr      IUT address.
 * @param tst_addr      Tester address.
 * @param urx_num       Number of UDP RX zockets:
 *                          - @c 10 if @b reactor is @c FALSE;
 *                          - @c 20 if @b reactor is @c TRUE.
 * @param reactor       Call @a rpc_zf_process_events() every time a
 *                      datagram is sent if @c TRUE.
 * @param small_dgram   Send datagrams with payload length in one byte.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 * -# Create and bind @p urx_num UDP RX zockets.
 * -# In the loop:
 *      - send from tester a number of datagrams to all zockets:
 *          - call @a rpc_zf_process_events() after each sent datagram
 *            if @p reactor is @c TRUE;
 *      - sent datagrams number depends on the loop iteration number:
 *          - begin from @c 10 and multiply by two each iteration;
 *      - read all datagrams on IUT:
 *          - if not all datagrams are read - the packets limit is reached.
 * -# Check that maximum read datagrams number on the first zocket is @c 64.
 * -# Check that total number of read datagrams on all zockets no less
 *    then @c 400.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/overfill_receive_queue"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_mem.h"

/* Minimum datagrams serie length to be sent. */
#define MIN_DGRAMS_NUM 10

/* Theoretical minimum datagrams number, which should fit to a few ZF UDP
 * receive buffers. */
#define MIN_TOTAL_DGRAMS_NUM 390

/**
 * Read all available datagrams and check that they correspond to sent.
 *
 * @param rpcs          RPC server handle.
 * @param urx           UDP RX zocket.
 * @param rcviov        Iov vectors with receive buffers.
 * @param sndiov        Sent datagrams array.
 * @param bunch_size    Datagrams number in the sent bunch.
 *
 * @return Received datagrams number.
 */
static int
read_check_datagrams(rcf_rpc_server *rpcs,  rpc_zfur_p urx,
                     rpc_iovec *rcviov, rpc_iovec *sndiov, int bunch_size)
{
    int rxn = 0;
    size_t res;

    do {
        res = zfts_zfur_zc_recv(rpcs, urx, rcviov, bunch_size);
        if (res == 0)
            return rxn;

        rpc_iovec_cmp_strict(sndiov + rxn, rcviov, res);

        rxn += res;
    } while (rxn < bunch_size);

    res = zfts_zfur_zc_recv(rpcs, urx, rcviov, 1);
    if (res != 0)
        TEST_VERDICT("Extra datagram is received");

    return rxn;
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    int                    urx_num;
    te_bool                small_dgram;
    te_bool                reactor;

    struct sockaddr **iut_addr_bind = NULL;
    rpc_zf_stack_p    stack = RPC_NULL;
    rpc_zfur_p       *iut_s = NULL;
    rpc_zf_attr_p     attr = RPC_NULL;
    rpc_iovec        *sndiov;
    rpc_iovec        *rcviov;
    te_bool           final = FALSE;

    int bunch_size = MIN_DGRAMS_NUM;
    int bunch_size_prev = 0;
    int max_dgram_size;
    int *received;
    int tst_s;
    int zid;
    int total;
    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(urx_num);
    TEST_GET_BOOL_PARAM(reactor);
    TEST_GET_BOOL_PARAM(small_dgram);

    max_dgram_size = small_dgram ? 1 : ZFTS_DGRAM_MAX;

    iut_s = tapi_calloc(urx_num, sizeof(*iut_s));
    iut_addr_bind = tapi_calloc(urx_num, sizeof(*iut_addr_bind));
    received = tapi_calloc(urx_num, sizeof(*received));

    sndiov = tapi_calloc(bunch_size, sizeof(*sndiov));
    rcviov = tapi_calloc(bunch_size, sizeof(*rcviov));

    zfts_create_stack(pco_iut, &attr, &stack);

    for (i = 0; i < urx_num; i++)
    {
        rpc_zfur_alloc(pco_iut, iut_s + i, stack, attr);
        iut_addr_bind[i] = tapi_sockaddr_clone_typed(iut_addr,
                                                     TAPI_ADDRESS_SPECIFIC);
        CHECK_RC(tapi_allocate_set_port(pco_iut, iut_addr_bind[i]));
        rpc_zfur_addr_bind(pco_iut, iut_s[i], iut_addr_bind[i], tst_addr, 0);
    }

    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    while (1)
    {
        rpc_make_iov(sndiov + bunch_size_prev,
                     bunch_size - bunch_size_prev, 1, max_dgram_size);
        rpc_make_iov(rcviov + bunch_size_prev, bunch_size - bunch_size_prev,
                     ZFTS_DGRAM_MAX, ZFTS_DGRAM_MAX);

        RING("Send %d datagrams serie", bunch_size);
        for (zid = 0; zid < urx_num; zid++)
        {
            for (i = 0; i < bunch_size; i++)
            {
                rc = rpc_sendto(pco_tst, tst_s, sndiov[i].iov_base,
                                sndiov[i].iov_len, 0, iut_addr_bind[zid]);

                if (reactor)
                    rpc_zf_process_events(pco_iut, stack);
            }
        }

        rpc_zf_process_events(pco_iut, stack);

        total = 0;
        for (zid = 0; zid < urx_num; zid++)
        {
            received[zid] = read_check_datagrams(pco_iut, iut_s[zid],
                                                 rcviov, sndiov, bunch_size);
            total += received[zid];
        }

        for (zid = 0; zid < urx_num; zid++)
        {
            if (received[zid] != bunch_size)
            {
                final = TRUE;
                break;
            }
        }

        if (final)
            break;

        bunch_size_prev = bunch_size;
        bunch_size *= 2;
        sndiov = tapi_realloc(sndiov, bunch_size * sizeof(*sndiov));
        rcviov = tapi_realloc(rcviov, bunch_size * sizeof(*sndiov));
    }

    RING("Total read datagrams number %d", total);
    RING("The last bunch size %d, previous %d", bunch_size, bunch_size_prev);

    for (zid = 0; zid < urx_num; zid++)
        RING("Read datagrams number by zocket #%d: %d", zid, received[zid]);

    if (received[0] != ZFTS_MAX_DGRAMS_NUM)
        TEST_VERDICT("Unexpected datagrams number has been read on "
                     "the first UDP RX zockets after buffers overfilling");

    if (total < MIN_TOTAL_DGRAMS_NUM)
        TEST_VERDICT("Too few datagrams were read on all zockets");

    TEST_SUCCESS;

cleanup:
    if (iut_s != NULL)
    {
        for (i = 0; i < urx_num; i++)
            CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s[i]);
    }

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    TEST_END;
}
