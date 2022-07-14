/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id:$
 */

/**
 * @page udp_rx-zockets_release  Open-close UDP RX zockets multiple times.
 *
 * @objective Open-close up to maximum zockets number in the loop multiple
 *            times.
 *
 * @param pco_iut           PCO on IUT.
 * @param pco_tst           PCO on TST.
 * @param iterations_num    The main loop iterations number.
 * @param recreate_stack    Re-create ZF stack on each iteration if @c TRUE.
 * @param close_zocket      Close IUT zocket if @c TRUE.
 * @param read_all          Read all data from zockets before closing if it
 *                          is @c TRUE.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 * -# Create ZF stack here or in the loop in dependence on @p recreate_stack.
 * -# In the loop:
 *      -# create ZF stack here if @p recreate_stack is @c TRUE;
 *      -# allocate a few (random number) UDP RX zockets;
 *      -# bind all of them;
 *      -# send few datagrams to each zocket;
 *      -# read only one or all datagrams on each zocket in dependence
 *         on @p read_all;
 *      -# close zockets;
 *      -# free ZF stack here if @p recreate_stack is @c TRUE.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/zockets_release"

#include "zf_test.h"
#include "rpc_zf.h"

/* Disable/enable RPC logging. */
#define VERBOSE_LOGGING FALSE

/**
 * Send a few datagrams from tester socket, but read only one on IUT zocket.
 *
 * @param pco_iut   IUT RPC server.
 * @param stack     RPC pointer to Zetaferno stack object.
 * @param urx       RPC pointer to Zetaferno UDP RX zocket object.
 * @param pco_tst   Tester RPC server.
 * @param tst_s     Tester socket.
 * @param iut_addr  IUT address, can be @c NULL.
 * @param read_all  Read all or only one datagram.
 */
void
check_reception(rcf_rpc_server *pco_iut, rpc_zf_stack_p stack,
                rpc_zfur_p urx, rcf_rpc_server *pco_tst,
                int tst_s, const struct sockaddr *iut_addr, te_bool read_all)
{
    rpc_iovec  sndiov[ZFTS_IOVCNT];
    rpc_iovec  rcviov[ZFTS_IOVCNT];
    int        read_num = read_all ? ZFTS_IOVCNT : 1;

    rpc_make_iov(sndiov, ZFTS_IOVCNT, 1, ZFTS_DGRAM_MAX);
    rpc_make_iov(rcviov, read_num, ZFTS_DGRAM_MAX, ZFTS_DGRAM_MAX);

    zfts_sendto_iov(pco_tst, tst_s, sndiov, ZFTS_IOVCNT, iut_addr);
    zfts_zfur_zc_recv_all(pco_iut, stack, urx, rcviov, read_num);

    rpc_iovec_cmp_strict(sndiov, rcviov, read_num);

    rpc_release_iov(sndiov, ZFTS_IOVCNT);
    rpc_release_iov(rcviov, read_num);
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    int                    iterations_num;
    te_bool                recreate_stack;
    te_bool                read_all;
    te_bool                close_zocket;

    struct sockaddr *iut_addr_bind[ZFTS_MAX_UDP_ENDPOINTS] = {NULL};
    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zfur_p     iut_s[ZFTS_MAX_UDP_ENDPOINTS] = {0};
    size_t count = 0;
    int bunch_size;
    int tst_s;
    int i;
    int j;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(iterations_num);
    TEST_GET_BOOL_PARAM(recreate_stack);
    TEST_GET_BOOL_PARAM(close_zocket);
    TEST_GET_BOOL_PARAM(read_all);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    if (!recreate_stack)
        rpc_zf_stack_alloc(pco_iut, attr, &stack);

    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    if (!VERBOSE_LOGGING)
    {
        pco_iut->silent_pass = pco_iut->silent_pass_default = TRUE;
        pco_tst->silent_pass = pco_tst->silent_pass_default = TRUE;
    }

    RING("In the loop: create a bunch of zockets, bind them, read a "
         "datagram by each zocket, close zockets.");
    for (i = 0; i < iterations_num; i++)
    {
        if (recreate_stack)
            rpc_zf_stack_alloc(pco_iut, attr, &stack);
        bunch_size = rand_range(1, ZFTS_MAX_UDP_ENDPOINTS);
        RING("Bunch number %d, size %d, total zockets number %"
             TE_PRINTF_SIZE_T"u", i + 1, bunch_size, count);
        for (j = 0; j < bunch_size; j++)
        {
            count++;
            RING("Zocket number %"TE_PRINTF_SIZE_T"u", count);
            rpc_zfur_alloc(pco_iut, iut_s + j, stack, attr);

            if (iut_addr_bind[j] == NULL)
            {
                iut_addr_bind[j] =
                    tapi_sockaddr_clone_typed(iut_addr,
                                              TAPI_ADDRESS_SPECIFIC);
                CHECK_RC(tapi_allocate_set_port(pco_iut, iut_addr_bind[j]));
            }

            rpc_zfur_addr_bind(pco_iut, iut_s[j], iut_addr_bind[j],
                               tst_addr, 0);

            check_reception(pco_iut, stack, iut_s[j], pco_tst, tst_s,
                            iut_addr_bind[j], read_all);
        }

        for (j = 0; j < bunch_size; j++)
        {
            if (close_zocket)
                rpc_zfur_free(pco_iut, iut_s[j]);
            else
                rpc_release_rpc_ptr(pco_iut, iut_s[j], RPC_TYPE_NS_ZFUR);
        }

        if (recreate_stack)
            rpc_zf_stack_free(pco_iut, stack);
    }

    TEST_SUCCESS;

cleanup:
    pco_iut->silent_pass = pco_iut->silent_pass_default = FALSE;
    pco_tst->silent_pass = pco_tst->silent_pass_default = FALSE;

    if (!recreate_stack)
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack);

    CLEANUP_RPC_ZF_ATTR_FREE(pco_iut, attr);
    CLEANUP_RPC_ZF_DEINIT(pco_iut);

    for (j = 0; j < ZFTS_MAX_UDP_ENDPOINTS; j++)
    {
        free(iut_addr_bind[j]);
    }

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    TEST_END;
}
