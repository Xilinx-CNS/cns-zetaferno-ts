/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-udp_out Check @c EPOLLOUT reporting for UDP TX zocket
 *
 * @objective Check that @c EPOLLOUT event is reported correctly
 *            for UDP TX zocket.
 *
 * @param test_muxer    If @c TRUE, call @b zf_muxer_wait();
 *                      otherwise check what @b zf_reactor_perform()
 *                      reports.
 * @param func          Transmitting function.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/udp_out"

/* How long to wait for event, in milliseconds. */
#define EVENT_TIMEOUT 1000

/* How many times call @p func until giving up. */
#define MAX_ITERS 1000000

#include <sys/time.h>

#include "zf_test.h"

/* How long to continue test loop, in seconds. */
#define TEST_TIME 30L

/* Timeout for @b rpc_iomux_flooder(), in seconds. */
#define FLOODER_TIME_WAIT  2

int
main(int argc, char *argv[])
{

    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zfut_p     iut_zfut = RPC_NULL;
    int            tst_s = -1;

    rpc_zf_waitable_p   waitable = RPC_NULL;
    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;

    zfts_send_function  func;

    struct rpc_epoll_event event;

    zfts_zfut_buf snd;
    int           buf_size;

    struct timeval tv1;
    struct timeval tv2;

    int i;
    int j;

    uint64_t sent;
    uint64_t received;

    te_bool test_muxer = FALSE;
    te_bool failed = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(test_muxer);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);

    buf_size = (func == ZFTS_ZFUT_SEND ?
                ZFTS_DGRAM_LARGE : ZFTS_DGRAM_MAX);
    zfts_zfut_buf_make(1, buf_size, buf_size, &snd);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate UDP TX zocket. */
    rpc_zfut_alloc(pco_iut, &iut_zfut, stack, iut_addr,
                   tst_addr, 0, attr);

    /*- Create peer socket on Tester. */
    tst_s = rpc_create_and_bind_socket(pco_tst, RPC_SOCK_DGRAM,
                                       RPC_PROTO_DEF, FALSE, FALSE,
                                       tst_addr);
    rpc_connect(pco_tst, tst_s, iut_addr);

    /*- If @p test_muxer, allocate muxer set and add UDP TX zocket to it
     * with @c EPOLLOUT event. Call @b zf_muxer_wait() and check that it
     * reports the event. */
    if (test_muxer)
    {
        rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
        waitable = rpc_zfut_to_waitable(pco_iut, iut_zfut);
        rpc_zf_muxer_add_simple(pco_iut, muxer_set, waitable,
                                iut_zfut, RPC_EPOLLOUT);

        rc = rpc_zf_muxer_wait(pco_iut, muxer_set, &event, 1, 0);
        if (rc < 1)
            TEST_VERDICT("zf_muxer_wait() did not report EPOLLOUT "
                         "at the beginning");
        else if (rc > 1 ||
                 event.events != RPC_EPOLLOUT ||
                 event.data.u32 != iut_zfut)
        {
            ERROR_VERDICT("zf_muxer_wait() returned unexpected event "
                          "at the beginning");
            failed = TRUE;
        }
    }

    gettimeofday(&tv1, NULL);

    /*- Perform the following steps until @c TEST_TIME seconds passes. */

    j = 0;
    while (TRUE)
    {
        j++;
        RING("Test iteration %d", j);

        sent = 0;
        received = 0;

        pco_tst->op = RCF_RPC_CALL;
        rpc_iomux_flooder(pco_tst, NULL, 0, &tst_s, 1, ZFTS_DGRAM_MAX,
                          0, FLOODER_TIME_WAIT, FUNC_DEFAULT_IOMUX,
                          NULL, &received);

        /*- Call @p func in a loop until it fails with
         * @c EAGAIN error. */
        for (i = 0; i < MAX_ITERS; i++)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = zfts_zfut_send_no_check(pco_iut, iut_zfut, &snd, func);
            if (rc < 0)
            {
                if (RPC_ERRNO(pco_iut) != RPC_EAGAIN)
                    TEST_VERDICT("Send function returned errno %r",
                                 RPC_ERRNO(pco_iut));
                break;
            }
            else if (rc == 0)
                TEST_VERDICT("Send function returned zero "
                             "instead of failing with EAGAIN");
            else
                sent += snd.len;
        }
        if (i >= MAX_ITERS)
            TEST_VERDICT("Failed to wait until zfut_send_single() "
                         "returns EAGAIN");

        /*- If @p test_muxer, call @b zf_muxer_wait() and check that
         * it reports an event. */
        if (test_muxer)
        {
            rc = rpc_zf_muxer_wait(pco_iut, muxer_set, &event, 1,
                                   EVENT_TIMEOUT);
            if (rc == 0)
            {
                ERROR_VERDICT("zf_muxer_wait() did not report EPOLLOUT "
                              "event after the loop");
                failed = TRUE;
            }
            else if (rc > 1 ||
                     event.events != RPC_EPOLLOUT ||
                     event.data.u32 != iut_zfut)
            {
                ERROR_VERDICT("zf_muxer_wait() returned unexpected event");
                failed = TRUE;
            }
        }
        else
        {
            rc = rpc_zf_process_events_long(pco_iut, stack, EVENT_TIMEOUT);
            if (rc == 0)
            {
                ERROR_VERDICT("zf_reactor_perform() returned zero");
                failed = TRUE;
            }
        }

        /*- Call @p func another time to check that
         * it is unblocked. */
        RPC_AWAIT_ERROR(pco_iut);
        rc = zfts_zfut_send_no_check(pco_iut, iut_zfut, &snd, func);
        if (rc < 0)
            TEST_VERDICT("Send function failed with errno %r after "
                         "unblocking", RPC_ERRNO(pco_iut));
        else if (rc == 0)
            TEST_VERDICT("Send function returned zero after "
                         "unblocking", RPC_ERRNO(pco_iut));
        else
            sent += snd.len;

        if (failed)
            TEST_STOP;

        rpc_iomux_flooder(pco_tst, NULL, 0, &tst_s, 1, ZFTS_DGRAM_MAX,
                          0, FLOODER_TIME_WAIT, FUNC_DEFAULT_IOMUX,
                          NULL, &received);
        RING("%lu bytes were sent, %lu bytes were received",
             (long unsigned int)sent, (long unsigned int)received);

        if (sent != received)
            TEST_VERDICT("Length of data sent does not match "
                         "length of data received");

        gettimeofday(&tv2, NULL);
        if (TIMEVAL_SUB(tv2, tv1) >= TE_SEC2US(TEST_TIME))
            break;
    }

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, waitable);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_zfut);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    zfts_zfut_buf_release(&snd);

    TEST_END;
}
