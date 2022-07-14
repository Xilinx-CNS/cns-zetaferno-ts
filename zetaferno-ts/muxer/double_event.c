/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-double_event An event happens twice
 *
 * @objective Check that an event which happens twice is reported
 *            only once.
 *
 * @param zf_event    Determines tested zocket and event types:
 *                    - urx.in;
 *                    - zft.in;
 *                    - zft.out;
 *                    - zftl.in.
 * @param muxer_wait  Call extra @b zf_muxer_wait() before an event is
 *                    invoked in the second time if @c TRUE.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/double_event"

#include "zf_test.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    const char *zf_event = NULL;
    te_bool     muxer_wait = FALSE;

    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;
    zfts_mzockets       mzockets = {0};

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_STRING_PARAM(zf_event);
    TEST_GET_BOOL_PARAM(muxer_wait);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Create a zocket according to @p zf_event. */
    zfts_create_mzockets(zf_event, pco_iut, attr, stack, iut_addr,
                         pco_tst, tst_addr, &mzockets);

    /* Specify @c TCP_NODELAY option on the tester socket to make sure data
     * is sent by both @c send() calls, without waiting for ACK from IUT. */
    if (zfts_zocket_type_zft(mzockets.mzocks->zock_type))
    {
        rpc_setsockopt_int(pco_tst, mzockets.mzocks->peer_sock,
                           RPC_TCP_NODELAY, 1);
    }

    /*- Allocate a muxer set and add the zocket to it with
     * the appropriate event. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    zfts_mzockets_prepare_events(&mzockets, zf_event);
    zfts_mzockets_add_exp_events(muxer_set, &mzockets);

    /*- Provoke an event according to @p zf_event. */
    zfts_mzockets_invoke_events(&mzockets);

    /*- If @p muxer_wait is @c TRUE
     *  - call @b zf_muxer_wait(timeout=-1) - the event should be returned.
     *  - call @b zf_muxer_wait(timeout=0) - this time no events is
     *    returned. */
    if (muxer_wait)
    {
        zfts_mzockets_check_events(
                        pco_iut, muxer_set, -1, FALSE,
                        &mzockets, "The first call after the first event");
        zfts_muxer_wait_check_no_evts(
                                 pco_iut, muxer_set, 0,
                                 "The second call after the first event");
    }

    /*- Provoke an event in dependence on @p zf_event. */
    zfts_mzockets_prepare_events(&mzockets, zf_event);
    zfts_mzockets_invoke_events(&mzockets);

    /*- Drain the event queue for TCP zockets. */
    if (!strcmp(zf_event, "urx.in") == 0)
        ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Call @b zf_muxer_wait(timeout=-1) - check that an event which
     *  happens twice is reported. */
    zfts_mzockets_check_events(
                      pco_iut, muxer_set, -1, FALSE,
                      &mzockets, "The first call after the second event");

    /*- Call @b zf_muxer_wait(timeout=0) - no events is returned. */
    zfts_muxer_wait_check_no_evts(
                             pco_iut, muxer_set, 0,
                             "The second call after the second event");

    /*- Release the events - read/send data or accept connection. */
    zfts_mzockets_process_events(&mzockets);

    TEST_SUCCESS;

cleanup:

    zfts_destroy_mzockets(&mzockets);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
