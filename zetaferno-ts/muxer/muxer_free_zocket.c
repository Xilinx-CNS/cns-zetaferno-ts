/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-muxer_free_zocket Free a zocket while it is in a muxer set
 *
 * @objective Free a zocket while it is in a muxer set, check if the muxer
 *            can work after that.
 *
 * @param zf_event    Determines tested zocket and event types:
 *                    - zft.in;
 *                    - urx.in;
 *                    - zftl.in.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/muxer_free_zocket"

#include "zf_test.h"

/* Maximum length of string specifying zocket and event types. */
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

    const char *zf_event = NULL;

    char zf_events[MAX_EVT_STR_LEN] = "";

    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;
    zfts_mzockets       mzockets = {0};

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_STRING_PARAM(zf_event);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    snprintf(zf_events, MAX_EVT_STR_LEN, "%s,%s", zf_event, zf_event);

    /*- Create two zockets in dependence on @p zf_event. */
    zfts_create_mzockets(zf_events, pco_iut, attr, stack, iut_addr,
                         pco_tst, tst_addr, &mzockets);

    /*- Allocate a muxer set and add zockets to it with
     * the appropriate events. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    zfts_mzockets_prepare_events(&mzockets, zf_events);
    zfts_mzockets_add_exp_events(muxer_set, &mzockets);

    /*- Provoke events on both zockets in dependence
     * on @p zf_event. */
    zfts_mzockets_invoke_events(&mzockets);

    if (strcmp(zf_event, "zftl.in") == 0)
        ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Free the first zocket. */
    zfts_destroy_zocket(mzockets.mzocks[0].zock_type,
                        pco_iut, mzockets.mzocks[0].zocket);
    mzockets.mzocks[0].in_mset = FALSE;

    /*- Call @c zf_muxer_wait() - check that event for the second
     * zocket is returned. */
    zfts_mzockets_check_events(pco_iut, muxer_set, MUXER_TIMEOUT,
                               FALSE, &mzockets, "Error");

    /*- Release the event on the second zocket. */
    mzockets.mzocks[0].zocket = RPC_NULL;
    zfts_mzockets_process_events(&mzockets);

    TEST_SUCCESS;

cleanup:

    zfts_destroy_mzockets(&mzockets);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

