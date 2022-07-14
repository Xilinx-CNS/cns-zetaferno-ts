/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-tcp_shutdown Muxer behavior after TCP zocket shutdown on write
 *
 * @objective Examine muxer behavior after TCP zocket shutdown on write
 *            operation using @b zft_shutdown_tx().
 *
 * @param zf_event        Determines tested zocket and event types:
 *                        - zft.in;
 *                        - zft.out;
 *                        - zft.in_out.
 * @param event_before    If @c TRUE provoke an event before the zocket
 *                        shutdown.
 * @param close_peer      If @c TRUE, close peer socket after calling
 *                        @b zft_shutdown_tx().
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/tcp_shutdown"

#include "zf_test.h"

/* Timeout used for @b zf_muxer_wait(). */
#define MUXER_TIMEOUT 500

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;
    zfts_mzockets       mzockets = {0};

    uint32_t add_events = 0;
    uint32_t exp_events = 0;

    const char *zf_event = NULL;
    te_bool     event_before = FALSE;
    te_bool     close_peer = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_STRING_PARAM(zf_event);
    TEST_GET_BOOL_PARAM(event_before);
    TEST_GET_BOOL_PARAM(close_peer);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection to get zft zocket. */
    zfts_create_mzockets(zf_event, pco_iut, attr, stack, iut_addr,
                         pco_tst, tst_addr, &mzockets);

    /*- Allocate a muxer set and add the zocket to it with an event
     *  - @c EPOLLIN and/or @c EPOLLOUT according to @p zf_event;
     *  - plus @c EPOLLHUP, @C EPOLLERR; */

    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);

    add_events = zfts_parse_events(zf_event) | RPC_EPOLLHUP | RPC_EPOLLERR;
    rpc_zf_muxer_add_simple(pco_iut, muxer_set,
                            mzockets.mzocks[0].waitable,
                            mzockets.mzocks[0].zocket,
                            add_events);

    /*- If @p event_before is @c TRUE, provoke the event. */
    if (event_before)
    {
        zfts_mzockets_prepare_events(&mzockets, zf_event);
        zfts_mzockets_invoke_events(&mzockets);
    }

    /*
     * In case of provoking @c EPOLLOUT event, allow to free
     * some space in TX buffer, or @b zft_shutdown_tx() will
     * fail with ENOSPC.
     */
    if (add_events & RPC_EPOLLOUT)
        ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Call @b zft_shutdown_tx() on the zocket. */
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zft_shutdown_tx(pco_iut, mzockets.mzocks[0].zocket);
    if (rc < 0)
        TEST_VERDICT("zft_shutdown_tx() failed with errno %r",
                     RPC_ERRNO(pco_iut));

    if (close_peer)
    {
        rpc_close(pco_tst, mzockets.mzocks[0].peer_sock);
        mzockets.mzocks[0].peer_sock = -1;
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    /*- Call @b zf_muxer_wait():
     *  - If @p event_before is @c FALSE, then it should return no events;
     *  - @c EPOLLHUP should be returned if @p zf_event is @c "zft.out"
     *    or @c "zft.in_out";
     *  - @c EPOLLIN should be returned if @p zf_event is @c "zft.in" or
     *    @c "zft.in_out". */

    exp_events = 0;
    if (add_events & RPC_EPOLLOUT)
        exp_events = exp_events | RPC_EPOLLOUT;
    if ((add_events & RPC_EPOLLIN) && (event_before || close_peer))
        exp_events = exp_events | RPC_EPOLLIN;
    if (close_peer)
    {
        exp_events = exp_events | RPC_EPOLLHUP;

        /*
         * If socket is closed while some data is in its receive
         * buffer, it will send RST.
         */
        if ((add_events & RPC_EPOLLOUT) && event_before)
        {
            exp_events = exp_events | RPC_EPOLLERR;
            mzockets.mzocks[0].expect_econnreset = TRUE;
        }
    }

    ZFTS_CHECK_MUXER_EVENTS(
                pco_iut, muxer_set,
                "The first call", MUXER_TIMEOUT, NULL,
                { exp_events, { .u32 = mzockets.mzocks[0].zocket } });

    /*- Release the event if required. */
    if (event_before)
        zfts_mzockets_process_events(&mzockets);

    /*- If @p zf_event is @c "zft.in" or @c "zft.in_out":
     *  - send a packet from tester;
     *  - call @c zf_muxer_wait() - check that @c EPOLLIN
     *    event is returned;
     *  - read and check data. */

    if ((add_events & RPC_EPOLLIN) && !close_peer)
    {
        int exp_events2 = RPC_EPOLLIN;

        if (add_events & RPC_EPOLLOUT)
            exp_events2 = exp_events2 | RPC_EPOLLOUT;

        zfts_mzockets_prepare_events(&mzockets, "in");
        zfts_mzockets_invoke_events(&mzockets);

        ZFTS_CHECK_MUXER_EVENTS(
                    pco_iut, muxer_set,
                    "The second call", MUXER_TIMEOUT, NULL,
                    { exp_events2, { .u32 = mzockets.mzocks[0].zocket } });

        zfts_mzockets_process_events(&mzockets);
    }

    TEST_SUCCESS;

cleanup:

    zfts_destroy_mzockets(&mzockets);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
