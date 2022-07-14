/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-gone_event An event has gone when @b zf_muxer_wait is called
 *
 * @objective Check that an event is not reported if it is purged before
 *            call @b zf_muxer_wait().
 *
 * @param zf_event    Determines tested zocket and event types:
 *                    - urx.in;
 *                    - zft.in;
 *                    - zft-act.out;
 *                    - zft-pas.out;
 *                    - zftl.in.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/gone_event"

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

    /*- Create a zocket according to @p zf_event. */
    zfts_create_mzockets(zf_event, pco_iut, attr, stack, iut_addr,
                         pco_tst, tst_addr, &mzockets);

    /*- Allocate a muxer set and add the zocket to it with
     * appropriate events. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    zfts_mzockets_prepare_events(&mzockets, zf_event);
    zfts_mzockets_add_exp_events(muxer_set, &mzockets);

    /*- Provoke an event according to @p zf_event. */
    zfts_mzockets_invoke_events(&mzockets);

    /*- Release the event - read/send data or accept connection. */
    zfts_mzockets_process_events(&mzockets);

    /* If @p zf_event is @c "zft-act.out" or @c "zft-pas.out",
     * overfill buffer of TCP zocket again to remove
     * @c EPOLLOUT event. */
    if (strcmp(zf_event, "zft-act.out") == 0 ||
        strcmp(zf_event, "zft-pas.out") == 0)
        zfts_mzockets_prepare_events(&mzockets, zf_event);

    /*- Call @b zf_muxer_wait() - check that no events is returned. */
    zfts_muxer_wait_check_no_evts(pco_iut, muxer_set, 0, "Error");

    TEST_SUCCESS;

cleanup:

    zfts_destroy_mzockets(&mzockets);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
