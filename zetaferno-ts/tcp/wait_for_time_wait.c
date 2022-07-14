/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-wait_for_time_wait Test tcp_wait_for_time_wait attribute
 *
 * @objective Check that tcp_wait_for_time_wait attribute works as
 *            expected.
 *
 * @param pco_iut                   PCO on IUT.
 * @param pco_tst                   PCO on TST.
 * @param iut_addr                  IUT address.
 * @param tst_addr                  Tester address.
 * @param tcp_wait_for_time_wait    Value of tcp_wait_for_time_wait
 *                                  attribute to set:
 *                                  - do not set (default value is @c 0);
 *                                  - @c 0;
 *                                  - @c 1.
 * @param open_method               How to open connection:
 *                                  - @c active
 *                                  - @c passive
 *                                  - @c passive_close (close listener after
 *                                    passively establishing connection)
 * @param use_muxer                 If @c TRUE, use @b zf_muxer_wait()
 *                                  to check whether stack is quiescent;
 *                                  otherwise use @b zf_stack_is_quiescent()
 *                                  for this.
 * @param muxer_timeout             If @p use_muxer is @c TRUE, pass this
 *                                  value as timeout to @b zf_muxer_wait().
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "tcp/wait_for_time_wait"

#include "zf_test.h"
#include "rpc_zf.h"

/**
 * How long to wait until stack becomes quiescent before giving up,
 * in milliseconds.
 */
#define MAX_TIME 200000

/**
 * Minimum amount of time (in milliseconds) that stack
 * should be not quiescent if we do not expect it to be
 * quiescent already.
 */
#define MIN_TIME_NO_QUIESCENT 500

/**
 * Check that zf_muxer_wait() reports stack quiescence when expected.
 *
 * @param rpcs            RPC server.
 * @param stack           RPC pointer of ZF stack object.
 * @param muxer_timeout   Timeout to pass to zf_muxer_wait().
 * @param exp_quiescent   Whether to expect that stack is already
 *                        quiescent, or to expect that it will become
 *                        quiescent after some delay.
 */
static void
check_muxer(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
            int muxer_timeout, te_bool exp_quiescent)
{
#define MAX_EVENTS 2

    rpc_zf_muxer_set_p      muxer_set = RPC_NULL;
    rpc_zf_waitable_p       stack_waitable = RPC_NULL;
    struct rpc_epoll_event  events[MAX_EVENTS];
    int                     rc;

    struct timeval tv_start;
    struct timeval tv_end;
    int            time_passed;
    te_bool        failed = TRUE;

    rpc_zf_muxer_alloc(rpcs, stack, &muxer_set);

    stack_waitable = rpc_zf_stack_to_waitable(rpcs, stack);

    rpc_zf_muxer_add_simple(rpcs, muxer_set, stack_waitable,
                            stack, RPC_EPOLLSTACKHUP);

    gettimeofday(&tv_start, NULL);

    do {
        if (muxer_timeout < 0)
            rpcs->timeout = MAX_TIME;
        RPC_AWAIT_ERROR(rpcs);
        rc = rpc_zf_muxer_wait(rpcs, muxer_set, events,
                               MAX_EVENTS, muxer_timeout);

        gettimeofday(&tv_end, NULL);
        time_passed = TE_US2MS(TIMEVAL_SUB(tv_end, tv_start));

        if (rc < 0)
        {
            ERROR_VERDICT("zf_muxer_wait() unexpectedly failed "
                          "with errno %r", RPC_ERRNO(rpcs));
            break;
        }
        else if (rc > 1)
        {
            ERROR_VERDICT("zf_muxer_wait() returned strange result");
            break;
        }
        else if (rc == 0)
        {
            if (time_passed > MAX_TIME)
            {
                ERROR_VERDICT("Waiting for stack quiescence took "
                              "too much time");
                break;
            }
            else if (exp_quiescent)
            {
                ERROR_VERDICT("zf_muxer_wait() returned no event "
                              "when stack is expected to be quiescent");
                break;
            }
            else if (muxer_timeout < 0 ||
                     rpcs->duration <
                          (long unsigned int)TE_MS2US(muxer_timeout) / 2)
            {
                ERROR_VERDICT("zf_muxer_wait() terminated too early "
                              "returning no event");
                break;
            }
        }
        else if (rc == 1)
        {
            if (!exp_quiescent && time_passed < MIN_TIME_NO_QUIESCENT)
            {
                ERROR_VERDICT("zf_muxer_wait() too early reported "
                              "stack as quiescent");
                break;
            }
            else if (events[0].events != RPC_EPOLLSTACKHUP)
            {
                ERROR_VERDICT("zf_muxer_wait() reported unexpected "
                              "events %s",
                              epoll_event_rpc2str(events[0].events));
                break;
            }
        }

        if (rc > 0)
        {
            failed = FALSE;
            break;
        }

        SLEEP(1);
    } while (TRUE);

    rpc_zf_muxer_free(rpcs, muxer_set);
    rpc_zf_waitable_free(rpcs, stack_waitable);

    if (failed)
        TEST_STOP;
}

/**
 * Check that zf_stack_is_quiescent() reports stack quiescence
 * when expected.
 *
 * @param rpcs            RPC server.
 * @param stack           RPC pointer of ZF stack object.
 * @param exp_quiescent   Whether to expect that stack is already
 *                        quiescent, or to expect that it will become
 *                        quiescent after some delay.
 */
static void
check_zf_stack_is_quiescent(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                            te_bool exp_quiescent)
{
    struct timeval tv_start;
    struct timeval tv_end;
    int            time_passed;
    int            rc;

    gettimeofday(&tv_start, NULL);

    do {
        rpc_zf_process_events(rpcs, stack);

        RPC_AWAIT_ERROR(rpcs);
        rc = rpc_zf_stack_is_quiescent(rpcs, stack);

        gettimeofday(&tv_end, NULL);
        time_passed = TE_US2MS(TIMEVAL_SUB(tv_end, tv_start));

        if (rc < 0)
        {
            TEST_VERDICT("zf_stack_is_quiescent() unexpectedly failed "
                         "with errno %r", RPC_ERRNO(rpcs));
        }
        else if (rc == 0)
        {
            if (exp_quiescent)
                TEST_VERDICT("zf_stack_is_quiescent() did not report "
                             "stack as quiescent when it is expected "
                             "to be quiescent already");
            else if (time_passed > MAX_TIME)
                TEST_VERDICT("Waiting for stack quiescence took "
                             "too much time");
        }
        else
        {
            break;
        }

        SLEEP(1);
    } while (TRUE);

    if (!exp_quiescent && time_passed < MIN_TIME_NO_QUIESCENT)
        TEST_VERDICT("zf_stack_is_quiescent() reported stack as quiescent "
                     "too early");
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p           iut_zft = RPC_NULL;
    rpc_zftl_p          iut_zftl = RPC_NULL;
    int                 tst_s = -1;

    int       tcp_wait_for_time_wait = -1;

    zfts_conn_open_method open_method;

    te_bool use_muxer;
    int     muxer_timeout;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(tcp_wait_for_time_wait);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_BOOL_PARAM(use_muxer);
    TEST_GET_INT_PARAM(muxer_timeout);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Set attribute @b tcp_wait_for_time_wait to
     * @p tcp_wait_for_time_wait. */
    if (tcp_wait_for_time_wait < 0)
        tcp_wait_for_time_wait = 0; /* Default value */
    else
        rpc_zf_attr_set_int(pco_iut, attr, "tcp_wait_for_time_wait",
                            tcp_wait_for_time_wait);

    /*- Allocate Zetaferno stack. */
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Establish TCP connection actively or passively according to
     * @p open_method. */

    zfts_establish_tcp_conn_ext2(open_method,
                                 pco_iut, attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- Close the zocket on IUT. */
    rpc_zft_free(pco_iut, iut_zft);
    iut_zft = RPC_NULL;

    /*- Close the tester socket. */
    rpc_close(pco_tst, tst_s);
    tst_s = -1;

    ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Check if the stack is quiescent while the zocket
     * is in @b TIME_WAIT state: if @p tcp_wait_for_time_wait
     * is @c 0, stack should be reported as quiescent
     * immediately by @b zf_muxer_wait() or @b zf_stack_is_quiescent();
     * otherwise it should be reported quiescent after
     * some delay.
     */

    if (use_muxer)
        check_muxer(pco_iut, stack, muxer_timeout,
                    (tcp_wait_for_time_wait == 0));
    else
        check_zf_stack_is_quiescent(pco_iut, stack,
                                    (tcp_wait_for_time_wait == 0));

    /*- If @p tcp_wait_for_time_wait is @c 1, check that
     * TCP connection with the same IUT address can be established. */
    if (tcp_wait_for_time_wait != 0)
        zfts_establish_tcp_conn_ext((open_method == ZFTS_CONN_OPEN_ACT),
                                    pco_iut, attr, stack,
                                    iut_zftl, &iut_zft, iut_addr,
                                    pco_tst, &tst_s, tst_addr,
                                    -1, -1);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
