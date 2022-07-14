/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-two_zockets_del Delete one by one zockets from a muxer set
 *
 * @objective Check that after removing a zocket from a muxer set, the
 *            muxer can return events only for the left zockets.
 *
 * @param zf_event_first    Determines the first zocket type and expected
 *                          event for it:
 *                          - urx.in;
 *                          - zft.in;
 *                          - zft.out;
 *                          - zftl.in.
 * @param zf_event_second   Determines the second zocket type and expected
 *                          event for it:
 *                          - urx.in;
 *                          - zft.in;
 *                          - zft.out;
 *                          - zftl.in.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/two_zockets_del"

#include "zf_test.h"

/* Maximum length of string with events descriptions. */
#define MAX_EVT_STR_LEN 100

/* Timeout for @b zf_muxer_wait(). */
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

    const char *zf_event_first = NULL;
    const char *zf_event_second = NULL;

    char events_str[MAX_EVT_STR_LEN] = "";

    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;
    zfts_mzockets       mzockets = {0};

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_STRING_PARAM(zf_event_first);
    TEST_GET_STRING_PARAM(zf_event_second);

    snprintf(events_str, MAX_EVT_STR_LEN, "%s,%s",
             zf_event_first, zf_event_second);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Create two zockets according to
     * @p zf_event_first and @p zf_event_second. */
    zfts_create_mzockets(events_str, pco_iut, attr, stack, iut_addr,
                         pco_tst, tst_addr, &mzockets);

    /*- Allocate a muxer set and add zockets to it with
     * the appropriate events. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    zfts_mzockets_prepare_events(&mzockets, events_str);
    zfts_mzockets_add_exp_events(muxer_set, &mzockets);

    /*- Provoke events on both zockets according to
     * @p zf_event_first and @p zf_event_second. */
    zfts_mzockets_invoke_events(&mzockets);

    /*
     * This is necessary to do to ensure that for both zockets
     * events are processed before @b zf_muxer_wait() returns;
     * otherwise it may wait until the first event and return.
     */
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Call @b zf_muxer_wait() - check that it returns events
     * on both zockets. */
    zfts_mzockets_check_events(
                      pco_iut, muxer_set, MUXER_TIMEOUT, FALSE,
                      &mzockets, "When two zockets are in muxer set");

    /*- Release the events. */
    zfts_mzockets_process_events(&mzockets);

    /*- Provoke events on both zockets according to
     * @p zf_event_first and @p zf_event_second. */
    zfts_mzockets_prepare_events(&mzockets, events_str);
    zfts_mzockets_invoke_events(&mzockets);

    /*- Delete the first zocket from the muxer set. */
    zfts_mzocket_del(&mzockets.mzocks[0]);

    ZFTS_WAIT_NETWORK(pco_iut, stack);
    /*- Call @b zf_muxer_wait() - check that it returns events
     * on the second zocket only. */
    zfts_mzockets_check_events(
                      pco_iut, muxer_set, MUXER_TIMEOUT, FALSE,
                      &mzockets, "After removing the first zocket");

    /*- Release the events. */
    zfts_mzockets_process_events(&mzockets);

    /*- Provoke events on both zockets according to
     * @p zf_event_first and @p zf_event_second. */
    zfts_mzockets_prepare_events(&mzockets, events_str);
    zfts_mzockets_invoke_events(&mzockets);

    /*- Delete the second zocket from the muxer set. */
    zfts_mzocket_del(&mzockets.mzocks[1]);

    /*- Call @b zf_muxer_wait() - check that it returns no events. */
    zfts_mzockets_check_events(
                      pco_iut, muxer_set, MUXER_TIMEOUT, FALSE,
                      &mzockets, "After removing the second zocket");

    /*- Release the events. */
    zfts_mzockets_process_events(&mzockets);

    /*- Provoke events on both zockets according to
     * @p zf_event_first and @p zf_event_second. */
    zfts_mzockets_prepare_events(&mzockets, events_str);
    zfts_mzockets_invoke_events(&mzockets);

    /*- Call @b zf_muxer_wait() - check that it returns no events. */
    zfts_mzockets_check_events(
                      pco_iut, muxer_set, MUXER_TIMEOUT, FALSE,
                      &mzockets,
                      "The second call after removing the second zocket");

    /*- Release the events. */
    zfts_mzockets_process_events(&mzockets);

    TEST_SUCCESS;

cleanup:

    zfts_destroy_mzockets(&mzockets);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
