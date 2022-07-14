/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id:$
 */

/**
 * @page udp_rx-events_queue  Datagrams reception by a few UDP RX zockets
 *
 * @objective  Send datagrams to a few UDP zockets in random order, check
 *             that datagrams are delivered to proper zocket and no loss.
 *
 * @param pco_iut           PCO on IUT.
 * @param pco_tst           PCO on TST.
 * @param zockets_num       RX zockets number.
 * @param bunches_num       Datagrams bunches number to be sent.
 * @param bunch_size        Datagrams number to be sent in a one bunch.
 * @param read_all          Read all available datagrams from a zocket if
 *                          @c TRUE, otherwise read by one datagram from
 *                          different zockets.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/events_queue"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_mem.h"

/* Disable/enable RPC logging. */
#define VERBOSE_LOGGING FALSE

/**
 * Read datagrams bunch by one datagram with different zockets.
 *
 * @param rpcs          RPC server handle.
 * @param stack         Zetaferno stack handle.
 * @param urx           UDP RX zocket.
 * @param zid           Zockets index array, each value corresponds to
 *                      target zocket for each sent datagram.
 * @param sndiov        Sent datagrams array.
 * @param bunch_size    Datagrams number in the sent bunch.
 */
static void
read_bunch_by_dgram(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                    rpc_zfur_p *urx, int *zid, rpc_iovec *sndiov,
                    int bunch_size)
{
    rpc_iovec rcviov;
    int i;

    rpc_make_iov(&rcviov, 1, ZFTS_DGRAM_MAX, ZFTS_DGRAM_MAX);

    rpc_zf_process_events(rpcs, stack);

    for (i = 0; i < bunch_size; i++)
    {
        if (zfts_zfur_zc_recv(rpcs, urx[zid[i]], &rcviov, 1) == 0)
        {
            ERROR("Datagram number %d is lost", i);
            TEST_VERDICT("One of datagrams is lost");
        }

        rpc_iovec_cmp_strict(sndiov + i, &rcviov, 1);
    }
}

/**
 * Check that sent datagrams correspond to received.
 *
 * @param sndiov        Sent datagrams array.
 * @param rcviov        Received datagrams array.
 * @param zid           Zockets index array, each value corresponds to
 *                      target zocket for each sent datagram.
 * @param bunch_size    Datagrams number in the sent bunch.
 * @param idx           Index of the checked zocket.
 */
static void
check_dgrams(rpc_iovec *sndiov, rpc_iovec *rcviov, int *zid, int bunch_size,
             int idx)
{
    int s;

    for (s = 0; s < bunch_size; s++)
    {
        if (zid[s] == idx)
            rpc_iovec_cmp_strict(sndiov + s, rcviov++, 1);
    }
}

/**
 * Read datagrams bunch - sequentially read all from each zocket.
 *
 * @param rpcs          RPC server handle.
 * @param stack         Zetaferno stack handle.
 * @param urx           UDP RX zocket.
 * @param zockets_num   zockets number.
 * @param zid           Zockets index array, each value corresponds to
 *                      target zocket for each sent datagram.
 * @param sndiov        Sent datagrams array.
 * @param bunch_size    Datagrams number in the sent bunch.
 */
static void
read_bunch_by_zockets(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                                rpc_zfur_p *urx, int zockets_num, int *zid,
                                rpc_iovec *sndiov, int bunch_size)
{
    rpc_iovec *rcviov;
    size_t res;
    size_t count;
    size_t dgrams_num;
    int i;
    int j;

    rcviov = tapi_calloc(bunch_size, sizeof(*rcviov));
    rpc_make_iov(rcviov, bunch_size, ZFTS_DGRAM_MAX, ZFTS_DGRAM_MAX);

    rpc_zf_process_events(rpcs, stack);

    for (i = 0; i < zockets_num; i++)
    {
        count = 0;
        dgrams_num = 0;
        for (j = 0; j < bunch_size; j++)
        {
            if (zid[j] == i)
                count++;
        }

        while (dgrams_num < count)
        {
            res = zfts_zfur_zc_recv(rpcs, urx[i], rcviov + dgrams_num,
                                    count - dgrams_num);
            if (res > 1)
                TEST_VERDICT("Zero-copy receive returned iovcnt > 1");
            if (res == 0)
                break;
            dgrams_num++;
        }

        if (dgrams_num != count)
        {
            ERROR("Read not enough datagrams: %"TE_PRINTF_SIZE_T"u instead"
                  " of %"TE_PRINTF_SIZE_T"u", res, count);
            TEST_VERDICT("Missing datagrams");
        }

        check_dgrams(sndiov, rcviov, zid, bunch_size, i);
    }

    rpc_release_iov(rcviov, bunch_size);
    free(rcviov);
}


int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    int                    zockets_num;
    int                    bunches_num;
    int                    bunch_size;
    te_bool                read_all;

    struct sockaddr **iut_addr_bind = NULL;
    rpc_zfur_p       *iut_s = NULL;
    rpc_zf_attr_p     attr = RPC_NULL;
    rpc_zf_stack_p    stack = RPC_NULL;
    rpc_iovec        *sndiov;

    int tst_s;
    int *zid = NULL;
    int i;
    int j;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(zockets_num);
    TEST_GET_INT_PARAM(bunches_num);
    TEST_GET_INT_PARAM(bunch_size);
    TEST_GET_BOOL_PARAM(read_all);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    iut_addr_bind = tapi_calloc(zockets_num, sizeof(*iut_addr_bind));
    iut_s = tapi_calloc(zockets_num, sizeof(*iut_s));
    zid = tapi_calloc(bunch_size, sizeof(*zid));
    sndiov = tapi_calloc(bunch_size, sizeof(*sndiov));

    /*- Create a few zockets in one ZF stack. */
    for (i = 0; i < zockets_num; i++)
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

    if (!VERBOSE_LOGGING)
    {
        pco_iut->silent_pass = pco_iut->silent_pass_default = TRUE;
        pco_tst->silent_pass = pco_tst->silent_pass_default = TRUE;
    }

    RING("In the loop: send datagrams bunches in random order to different "
         "zockets, read datagrams and check data.");

    for (i = 0; i < bunches_num; i++)
    {
        RING("Bunch number %d, sent datagrams number %d", i, i * bunch_size);

        rpc_make_iov(sndiov, bunch_size, 1, ZFTS_DGRAM_MAX);

        /*- Send datagrams to zockets in a random order. */
        for (j = 0; j < bunch_size; j++)
        {
            zid[j] = rand_range(0, zockets_num - 1);
            rpc_sendto(pco_tst, tst_s, sndiov[j].iov_base,
                       sndiov[j].iov_len, 0, iut_addr_bind[zid[j]]);
        }

        /*- Read datagrams in the following ways in dependence on @p read_all:
         *      - by one datagram in random order;
         *      - read all from each zocket sequentially.
         */
        /*- Check that datagrams are delivered to proper zocket and no
         * loss.
         */
        if (read_all)
            read_bunch_by_zockets(pco_iut, stack, iut_s, zockets_num, zid,
                                  sndiov, bunch_size);
        else
            read_bunch_by_dgram(pco_iut, stack, iut_s, zid, sndiov,
                                bunch_size);

        rpc_release_iov(sndiov, bunch_size);
    }

    TEST_SUCCESS;

cleanup:
    pco_iut->silent_pass = pco_iut->silent_pass_default = FALSE;
    pco_tst->silent_pass = pco_tst->silent_pass_default = FALSE;

    for (i = 0; i < zockets_num; i++)
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s[i]);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    TEST_END;
}
