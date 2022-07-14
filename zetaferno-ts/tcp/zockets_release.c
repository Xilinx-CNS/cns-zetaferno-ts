/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-zockets_release Open-close TCP zockets multiple times
 *
 * @objective Open-close up to maximum zockets number in the loop multiple
 *            times.
 *
 * @param pco_iut           PCO on IUT.
 * @param pco_tst           PCO on TST.
 * @param iut_addr          IUT address.
 * @param tst_addr          Tester address.
 * @param iterations_num    The main loop iterations number:
 *                          - 1000.
 * @param recreate_stack    Re-create ZF stack on each iteration if @c TRUE.
 * @param active            Determine if created zockets should be accepted
 *                          with listener or allocated with
 *                          @c rpc_zft_alloc().
 * @param recreate_listener Create-close listener zocket on each iteration
 *                          if @c TRUE. These iterations are only applicable
 *                          if @p active is @c FALSE.
 * @param read_all          Read all data on zockets if @c TRUE.
 * @param close_zockets     If @c TRUE, close IUT zockets when closing
 *                          connections; otherwise do not close them.
 * @param close_iut_first   Close IUT zocket first.
 * @param zero_state_wait   If @c TRUE, set @b tcp_finwait_ms and
 *                          @b tcp_timewait_ms to zero.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @note Macro @p ZFTS_WAIT_NETWORK() is not applicable in the test, i.e. we
 * should use a faster way of calling @b zf_reactor to establish
 * connections.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "tcp/zockets_release"

#include "zf_test.h"
#include "rpc_zf.h"

/** Maximum number of TCP connections opened simultaneously. */
#define MAX_CONNS 8

/** Minimum number of TCP connections opened simultaneously. */
#define MIN_CONNS 4

/** Maximum time for running this test, in seconds. */
#define MAX_TIME 600

/* Disable/enable RPC logging. */
#define VERBOSE_LOGGING FALSE

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr_storage  iut_bind_addrs[MAX_CONNS];
    te_bool                  iut_bind_addrs_init[MAX_CONNS];
    struct sockaddr_storage  iut_listen_addr;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zftl_p     iut_tl = RPC_NULL;

    rpc_zft_handle_p    iut_zft_handles[MAX_CONNS] = { RPC_NULL, };
    rpc_zft_p           iut_zfts[MAX_CONNS] = { RPC_NULL, };
    int                 tst_socks[MAX_CONNS] = { -1, };
    int                 cur_conns_num = 0;
    int                 i = 0;
    int                 tst_s_listening = -1;

    struct timeval tv_start;
    struct timeval tv_cur;

    int     cur_iter = 0;
    int     iterations_num = 0;
    te_bool recreate_stack = FALSE;
    te_bool active = FALSE;
    te_bool recreate_listener = FALSE;
    te_bool read_all = FALSE;
    te_bool close_iut_first = FALSE;
    te_bool close_zockets = FALSE;
    te_bool zero_state_wait = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(iterations_num);
    TEST_GET_BOOL_PARAM(recreate_stack);
    TEST_GET_BOOL_PARAM(active);
    TEST_GET_BOOL_PARAM(recreate_listener);
    TEST_GET_BOOL_PARAM(read_all);
    TEST_GET_BOOL_PARAM(close_iut_first);
    TEST_GET_BOOL_PARAM(close_zockets);
    TEST_GET_BOOL_PARAM(zero_state_wait);

    if (recreate_listener && active)
        TEST_FAIL("It does not make sense to set both "
                  "recreate_listener and active to TRUE");
    if (!recreate_listener && !active && recreate_stack)
        TEST_FAIL("Combination of recreate_listener=FALSE, "
                  "active=FALSE and recreate_stack=TRUE does "
                  "not make sense");
    if (!close_zockets && close_iut_first)
        TEST_FAIL("If close_zockets = FALSE, setting close_iut_first "
                  "to TRUE does not make sense");

    for (i = 0; i < MAX_CONNS; i++)
    {
        iut_bind_addrs_init[i] = FALSE;
    }

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    if (zero_state_wait)
    {
        rpc_zf_attr_set_int(pco_iut, attr, "tcp_finwait_ms", 0);
        rpc_zf_attr_set_int(pco_iut, attr, "tcp_timewait_ms", 0);
    }

    if (active)
    {
        tst_s_listening =
          rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                     RPC_PROTO_DEF, FALSE, FALSE, tst_addr);

        rpc_setsockopt_int(pco_tst, tst_s_listening,
                           RPC_SO_REUSEADDR, 1);

        /* Specify zero linger because while there is a tester socket in
         * TIME_WAIT state bound to the same address couplet it can send ACK
         * with an ACK number from the previous connection instead of a new
         * SYN-ACK when answering a new SYN.
         */
        if (!close_iut_first && read_all)
        {
            tarpc_linger optval = {.l_onoff = 1, .l_linger = 0};
            rpc_setsockopt(pco_tst, tst_s_listening, RPC_SO_LINGER,
                           &optval);
        }
        rpc_listen(pco_tst, tst_s_listening, 1);
    }

    CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr, &iut_listen_addr));

    gettimeofday(&tv_start, NULL);

    if (!VERBOSE_LOGGING)
    {
        pco_iut->silent_pass = pco_iut->silent_pass_default = TRUE;
        pco_tst->silent_pass = pco_tst->silent_pass_default = TRUE;
    }

    /*- Perform @p iterations_num iterations in loop */
    for (cur_iter = 0; cur_iter < iterations_num; cur_iter++)
    {
        gettimeofday(&tv_cur, NULL);
        if (TIMEVAL_SUB(tv_cur, tv_start) > TE_SEC2US(MAX_TIME))
        {
            ERROR("%d seconds passed since test start",
                  (int)TE_US2SEC(TIMEVAL_SUB(tv_cur, tv_start)));
            TEST_VERDICT("The test took too much time");
        }

        RING("Iteration %d", cur_iter + 1);

        /*- If @p recreate_stack, allocate ZF stack at the beginning of
         * every iteration, otherwise do it only at the beginning of
         * the first one. */
        if (cur_iter == 0 || recreate_stack)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zf_stack_alloc(pco_iut, attr, &stack);
            if (rc < 0)
                TEST_VERDICT("zf_stack_alloc() failed with errno %r",
                             RPC_ERRNO(pco_iut));
        }

        /*- If @p recreate_listener, create listener zocket at the
         * beginning of every iteration, otherwise do it only at the
         * beginning of the first one. */
        if (!active)
        {
            if (cur_iter == 0 || recreate_listener)
            {
                if (!recreate_stack && close_iut_first)
                {
                    /* Allocate new address for listening zocket
                     * since previous address may be still occupied
                     * by connections in TIME_WAIT state. */
                    CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr,
                                                 &iut_listen_addr));
                }

                RPC_AWAIT_ERROR(pco_iut);
                rc = rpc_zftl_listen(pco_iut, stack,
                                     SA(&iut_listen_addr), attr, &iut_tl);
                if (rc < 0)
                    TEST_VERDICT("zftl_listen() failed with errno %r",
                                 RPC_ERRNO(pco_iut));
            }
        }

        /*- if @p active is @c TRUE:
         *  - allocate a few (random number) zockets;
         *  - bind all of them;
         *  - connect to tester;
         *  - accept connections on tester listeners; */
        /*- else:
         *  - connect from tester;
         *  - accept connections by the listener zocket. */

        cur_conns_num = rand_range(MIN_CONNS, MAX_CONNS);

        for (i = 0; i < cur_conns_num; i++)
        {
            RING("Creating connection %d", i + 1);

            if (active)
            {
                if (!iut_bind_addrs_init[i] || !recreate_stack)
                {
                    CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr,
                                                 &iut_bind_addrs[i]));
                    iut_bind_addrs_init[i] = TRUE;
                }

                RPC_AWAIT_ERROR(pco_iut);
                rc = rpc_zft_alloc(pco_iut, stack, attr,
                                   &iut_zft_handles[i]);
                if (rc < 0)
                    TEST_VERDICT("zft_alloc() failed with errno %r",
                                 RPC_ERRNO(pco_iut));

                rpc_zft_addr_bind(pco_iut, iut_zft_handles[i],
                                  SA(&iut_bind_addrs[i]), 0);
                rpc_zft_connect(pco_iut, iut_zft_handles[i],
                                tst_addr, &iut_zfts[i]);
                iut_zft_handles[i] = RPC_NULL;
                ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);

                tst_socks[i] = rpc_accept(pco_tst, tst_s_listening,
                                          NULL, NULL);
                rpc_setsockopt_int(pco_tst, tst_socks[i],
                                   RPC_SO_REUSEADDR, 1);
            }
            else
            {
                tst_socks[i] = rpc_socket(
                                   pco_tst,
                                   rpc_socket_domain_by_addr(tst_addr),
                                   RPC_SOCK_STREAM, RPC_PROTO_DEF);

#if 0
                /* This sometimes results in pco_tst becoming
                 * unresponsive due to some TE bug. */
                pco_tst->op = RCF_RPC_CALL;
                rpc_connect(pco_tst, tst_socks[i], SA(&iut_listen_addr));

                RPC_AWAIT_ERROR(pco_iut);
                rc = rpc_zf_wait_for_event(pco_iut, stack);
                if (rc < 0)
                    TEST_VERDICT("rpc_zf_wait_for_event() failed "
                                 "with errno %r while processing "
                                 "incoming connection",
                                 RPC_ERRNO(pco_iut));

                rpc_zf_process_events(pco_iut, stack);

                pco_tst->op = RCF_RPC_WAIT;
                rpc_connect(pco_tst, tst_socks[i], SA(&iut_listen_addr));
#else
                rpc_fcntl(pco_tst, tst_socks[i], RPC_F_SETFL,
                          RPC_O_NONBLOCK);
                RPC_AWAIT_IUT_ERROR(pco_tst);
                rc = rpc_connect(pco_tst, tst_socks[i],
                                 SA(&iut_listen_addr));
                if (rc >= 0)
                    TEST_VERDICT("Nonblocking connect() "
                                 "successed unexpectedly");
                else if (RPC_ERRNO(pco_tst) != RPC_EINPROGRESS)
                    TEST_STOP;

                RPC_AWAIT_ERROR(pco_iut);
                rc = rpc_zf_wait_for_event(pco_iut, stack);
                if (rc < 0)
                    TEST_VERDICT("rpc_zf_wait_for_event() failed "
                                 "with errno %r while processing "
                                 "incoming connection",
                                 RPC_ERRNO(pco_iut));

                rpc_zf_process_events(pco_iut, stack);

                rpc_fcntl(pco_tst, tst_socks[i], RPC_F_SETFL, 0);
                rpc_connect(pco_tst, tst_socks[i], SA(&iut_listen_addr));
#endif

                RPC_AWAIT_ERROR(pco_iut);
                rc = rpc_zftl_accept(pco_iut, iut_tl, &iut_zfts[i]);
                if (rc < 0)
                    TEST_VERDICT("zftl_accept() failed with errno %r",
                                 RPC_ERRNO(pco_iut));
            }
        }

        /*- send few packets from/to each zocket; */
        /*- read data:
         *  - read all data on each zocket if @p read_all is @c TRUE;
         *  - else read only part of data on each zocket. */

        for (i = 0; i < cur_conns_num; i++)
        {
            if (read_all)
            {
                zfts_zft_check_connection(pco_iut, stack, iut_zfts[i],
                                          pco_tst, tst_socks[i]);
            }
            else
            {
                zfts_zft_check_sending(pco_iut, stack, iut_zfts[i],
                                       pco_tst, tst_socks[i]);
                zfts_zft_check_partial_receiving(pco_iut, stack,
                                                 iut_zfts[i],
                                                 pco_tst, tst_socks[i]);
            }
        }

        /*- if @p close_iut_first is @c TRUE:
         *  - close zockets;
         *  - close tester sockets;
         *  - in this case the zocket goes to @b TIME_WAIT state, but it
         *    also should be processed correctly; */
        /*- else:
         *  - close tester sockets;
         *  - if @p close_zockets, close zockets. */

        for (i = 0; i < cur_conns_num; i++)
        {
            RING("Closing connection %d", i + 1);

            if (!close_iut_first)
            {
                rpc_close(pco_tst, tst_socks[i]);
                ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);
            }

            if (close_zockets)
            {
                rpc_zft_free(pco_iut, iut_zfts[i]);
                iut_zfts[i] = RPC_NULL;
            }
            else
            {
                rpc_release_rpc_ptr(pco_iut, iut_zfts[i],
                                    RPC_TYPE_NS_ZFT);
                iut_zfts[i] = RPC_NULL;
            }

            if (close_iut_first)
                rpc_close(pco_tst, tst_socks[i]);

            rpc_zf_process_events(pco_iut, stack);
        }

        /*- Free listener zocket here if @p recreate_listener is @c TRUE. */
        if (!active)
        {
            if (recreate_listener)
            {
                rpc_zftl_free(pco_iut, iut_tl);
                iut_tl = RPC_NULL;
            }
        }

        /*- Free ZF stack here if @p recreate_stack is @c TRUE. */
        if (recreate_stack)
        {
            rpc_zf_stack_free(pco_iut, stack);
            stack = RPC_NULL;
        }
    }

    TEST_SUCCESS;

cleanup:
    pco_iut->silent_pass = pco_iut->silent_pass_default = FALSE;
    pco_tst->silent_pass = pco_tst->silent_pass_default = FALSE;

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_tl);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack);

    CLEANUP_RPC_ZF_ATTR_FREE(pco_iut, attr);

    CLEANUP_RPC_ZF_DEINIT(pco_iut);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s_listening);

    TEST_END;
}
