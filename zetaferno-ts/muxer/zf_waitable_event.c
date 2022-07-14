/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-zf_waitable_event Get waitable event by @b zf_waitable handle
 *
 * @objective Get waitable event of a zocket using @b zf_waitable_event().
 *
 * @param zocket_type     Determines tested zocket type:
 *                        - urx;
 *                        - utx;
*                         - zft;
 *                        - zftl.
 * @param zf_event        Expected event:
 *                        - IN;
 *                        - OUT;
 *                        - ERR;
 *                        - HUP;
 *                        - ALL (IN, OUT, ERR, HUP).
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/zf_waitable_event"

#include "zf_test.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr_storage iut_addr_aux;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zf_muxer_set_p        muxer_set = RPC_NULL;
    uint32_t                  exp_event;
    struct rpc_epoll_event    ret_event;

    zfts_zocket_type    zocket_type;
    const char         *zf_event = NULL;

    rpc_ptr             iut_zock = RPC_NULL;
    rpc_zf_waitable_p   waitable = RPC_NULL;
    int                 tst_s = -1;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(zocket_type, ZFTS_ZOCKET_TYPES);
    TEST_GET_STRING_PARAM(zf_event);

    exp_event = zfts_parse_events(zf_event);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Create a zocket according to @p zocket_type. */
    /*- Get zf_waitable handle. */
    tapi_sockaddr_clone_exact(iut_addr, &iut_addr_aux);
    zfts_create_zocket(zocket_type, pco_iut, attr, stack, SA(&iut_addr_aux),
                       pco_tst, tst_addr, &iut_zock, &waitable, &tst_s);

    /*- Allocate a muxer set and add the zocket to it with
     * the appropriate event. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, waitable,
                            iut_zock, exp_event);

    /*- Call @b zf_waitable_event()
     *  - specified events should be returned. */
    rpc_zf_waitable_event(pco_iut, waitable, &ret_event);
    if (ret_event.events != exp_event ||
        ret_event.data.u32 != iut_zock)
        TEST_VERDICT("Event returned by zf_waitable_event() is different "
                     "from expected");

    TEST_SUCCESS;

cleanup:

    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, waitable);
    if (iut_zock != RPC_NULL)
        zfts_destroy_zocket(zocket_type, pco_iut, iut_zock);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    TEST_END;
}
