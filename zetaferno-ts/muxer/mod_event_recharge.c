/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-mod_event_recharge Re-arm an event using @b zf_muxer_mod()
 *
 * @objective Check that an event can be returned a few times (like level-
 *            triggered events) if the expected event is updated with
 *            @b zf_muxer_mod().
 *
 * @param zf_event  Determines tested zocket and event types:
 *                  - urx.in;
 *                  - zft.in;
 *                  - zft.out;
 *                  - zftl.in.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/mod_event_recharge"

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

    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;
    zfts_mzockets       mzockets = {0};

    const char *zf_event = NULL;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_STRING_PARAM(zf_event);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Create a zocket according to @p zf_event. */
    zfts_create_mzockets(zf_event, pco_iut, attr, stack, iut_addr,
                         pco_tst, tst_addr, &mzockets);

    /*- Allocate a muxer set and add the zocket to it with no events. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    zfts_mzockets_add_events(muxer_set, &mzockets, 0);

    /*- Call @b zf_muxer_wait() with zero timeout - no events. */
    zfts_muxer_wait_check_no_evts(pco_iut, muxer_set, 0, "The first call");

    /*- Provoke an event according to @p zf_event. */
    zfts_mzockets_prepare_events(&mzockets, zf_event);
    zfts_mzockets_invoke_events(&mzockets);

    /*- Add the appropriate event for the zocket using
     * @b zf_muxer_mod(). */
    zfts_mzockets_mod_exp_events(&mzockets);

    /*- Call @b zf_muxer_wait() - the event should be returned. */
    zfts_mzockets_check_events(pco_iut, muxer_set, -1, FALSE,
                               &mzockets, "The second call");

    /*- Call @b zf_muxer_wait() - this time no events is returned. */
    zfts_muxer_wait_check_no_evts(pco_iut, muxer_set, 0, "The third call");

    /*- Call @b zf_muxer_mod() on the zocket with the same event
     *  - use @b zf_waitable_event() to get the expected event. */
    zfts_mzockets_rearm_events(&mzockets);

    /*- Call @c zf_muxer_wait() - the event should be returned. */
    zfts_mzockets_check_events(pco_iut, muxer_set, -1, FALSE,
                               &mzockets, "The last call");

    /*- Release the event - read/send data or accept connection. */
    zfts_mzockets_process_events(&mzockets);

    TEST_SUCCESS;

cleanup:

    zfts_destroy_mzockets(&mzockets);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
