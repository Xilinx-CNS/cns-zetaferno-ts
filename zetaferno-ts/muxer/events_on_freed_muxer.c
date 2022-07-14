/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-events_on_freed_muxer Event on freed multiplexer
 *
 * @objective Multiplexer is not completely freed until all of endpoints
 *            are removed from the set. Provoke an event on a zocket
 *            when the muxer is freed, but the zocket is still in the set.
 *
 * @param zf_event      Determines tested zocket and event types:
 *                      - urx.in;
 *                      - zft.in;
 *                      - zft.out;
 *                      - zftl.in;
 *                      - urx.in,zft.in,zft.out,zftl.in
 *                        (all kinds of zockets).
 * @param before_free   Provoke an event on a zocket before muxer
 *                      release if @c TRUE.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/events_on_freed_muxer"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    const char *zf_event = NULL;
    te_bool     before_free = FALSE;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;

    zfts_mzockets mzockets = {0};

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_STRING_PARAM(zf_event);
    TEST_GET_BOOL_PARAM(before_free);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate zocket(s) according to @p zf_event. */
    zfts_create_mzockets(zf_event, pco_iut, attr, stack, iut_addr,
                         pco_tst, tst_addr, &mzockets);

    /*- Allocate a muxer set and add the zocket(s) to it with events
     * from @p zf_event. */

    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    zfts_mzockets_prepare_events(&mzockets, zf_event);
    zfts_mzockets_add_exp_events(muxer_set, &mzockets);

    /*- If @p before_free is @c TRUE, invoke events on zockets. */
    if (before_free)
        zfts_mzockets_invoke_events(&mzockets);

    /*- Call @b zf_muxer_free(). */
    rpc_zf_muxer_free(pco_iut, muxer_set);

    /*- If @p before_free is @c FALSE, invoke events on zockets. */
    if (!before_free)
        zfts_mzockets_invoke_events(&mzockets);

    /*- Process invoked events on zockets (read data,
     * accept incoming connection, etc.). */
    zfts_mzockets_process_events(&mzockets);

    /*- Delete zocket(s) from the muxer set. */
    zfts_mzockets_del(&mzockets);

    TEST_SUCCESS;

cleanup:

    zfts_destroy_mzockets(&mzockets);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

