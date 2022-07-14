/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP TX tests
 *
 * $Id$
 */

/**
 * @page udp_tx-zockets_release Open-close UDP TX zockets multiple times.
 *
 * @objective  Open-close up to maximum zockets number in the loop multiple
 *             times.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param iut_addr      IUT address.
 * @param tst_addr      Tester address.
 * @param func          Transmitting function.
 * @param large_buffer  Use large data buffer to send.
 * @param few_iov       Use several iov vectors.
 * @param iterations_num  The main loop iterations number.
 * @param recreate_stack  Re-create ZF stack on each iteration if @c TRUE.
 * @param addr_type       Local address type:
 *      - @c IPv4:\<non-zero port>
 *      - @c INADDR_ANY:\<non-zero port>
 * @param process_events  Process stack events after send calls.
 * @param close_zocket    Close IUT zocket if @c TRUE.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 * -# Create ZF stack here or in the loop in dependence on @p recreate_stack.
 * -# In the loop:
 *      -# create ZF stack here if @p recreate_stack is @c TRUE;
 *      -# allocate a random number in range [1;64] UDP TX zockets;
 *      -# send few datagrams from each zocket;
 *      -# if @p process_events is @c TRUE:
 *        - call @a rpc_zf_process_events();
 *        - read datagrams on tester;
 *      -# close zockets;
 *      -# free ZF stack here if @p recreate_stack is @c TRUE;
 *      -# if @p recreate_stack is @c FALSE process events on the stack to
 *         avoid buffers exhaustin.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_tx/zockets_release"

#include "zf_test.h"
#include "rpc_zf.h"

/* Disable/enable RPC logging. */
#define VERBOSE_LOGGING FALSE

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
    tapi_address_type      addr_type;
    te_bool                recreate_stack;
    te_bool                close_zocket;
    te_bool                process_events;
    int                    iterations_num;

    struct sockaddr *iut_addr_bind[ZFTS_MAX_UDP_ENDPOINTS] = {NULL};
    rpc_zf_attr_p    attr = RPC_NULL;
    rpc_zf_stack_p   stack = RPC_NULL;
    rpc_zfut_p       iut_utx[ZFTS_MAX_UDP_ENDPOINTS] = {0};
    zfts_zfut_buf    snd;
    size_t           count = 0;
    te_errno         saved_rc = 0;

    int bunch_size;
    int tst_s = -1;
    int i;
    int j;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);
    TEST_GET_ENUM_PARAM(addr_type, TAPI_ADDRESS_TYPE);
    TEST_GET_BOOL_PARAM(recreate_stack);
    TEST_GET_BOOL_PARAM(close_zocket);
    TEST_GET_BOOL_PARAM(process_events);
    TEST_GET_INT_PARAM(iterations_num);

    zfts_zfut_buf_make_param(large_buffer, few_iov, &snd);

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

    RING("In the loop: create a bunch of zockets, send a few datagrams from"
         " each zocket, close zockets.");
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
            if (VERBOSE_LOGGING)
                RING("Zocket number %"TE_PRINTF_SIZE_T"u", count);

            iut_addr_bind[j] =
                tapi_sockaddr_clone_typed(iut_addr, TAPI_ADDRESS_SPECIFIC);
            CHECK_RC(tapi_allocate_set_port(pco_iut, iut_addr_bind[j]));

            rpc_zfut_alloc(pco_iut, iut_utx + j, stack, iut_addr_bind[j],
                           tst_addr, 0, attr);

            if (process_events)
            {
                rc = zfts_zfut_check_send_func_ext(pco_iut, stack,
                                                   iut_utx[j],
                                                   pco_tst, tst_s, func,
                                                   large_buffer, few_iov,
                                                   FALSE, NULL);
                if (saved_rc == 0)
                    saved_rc = rc;
            }
            else
            {
                rc = zfts_zfut_send_ext(pco_iut, stack, iut_utx[j], &snd, func);
            }
        }

        for (j = 0; j < bunch_size; j++)
        {
            if (close_zocket)
                rpc_zfut_free(pco_iut, iut_utx[j]);
            else
                rpc_release_rpc_ptr(pco_iut, iut_utx[j], RPC_TYPE_NS_ZFUT);
            free(iut_addr_bind[j]);
        }

        if (recreate_stack)
            rpc_zf_stack_free(pco_iut, stack);
        /* If stack is not re-created on each iteration we need process
         * events to avoid buffers exhaustin. */
        else if (!process_events)
          rpc_zf_process_events(pco_iut, stack);
    }

    if (saved_rc != 0)
        TEST_VERDICT("Receiving and checking data on Tester failed: %r",
                     saved_rc);

    TEST_SUCCESS;

cleanup:
    pco_iut->silent_pass = pco_iut->silent_pass_default = FALSE;
    pco_tst->silent_pass = pco_tst->silent_pass_default = FALSE;

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    if (!recreate_stack)
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack);

    CLEANUP_RPC_ZF_ATTR_FREE(pco_iut, attr);
    CLEANUP_RPC_ZF_DEINIT(pco_iut);

    TEST_END;
}
