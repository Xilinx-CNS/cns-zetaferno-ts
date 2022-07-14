/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-two_muxers Process incoming events using two multiplexers
 *
 * @objective Add two different zockets to two muxer sets, invoke incoming
 *            events on the both muxers in different order.
 *
 * @param first_zocket_type   Type of zocket to be processed by the
 *                            first muxer:
 *                            - urx;
 *                            - zftl;
 *                            - zft;
 * @param second_zocket_type  Type of zocket to be processed by the
 *                            second muxer:
 *                            - urx;
 *                            - zftl;
 *                            - zft;
 * @param first_event         Invoke an event on the first muxer.
 * @param second_event        Invoke an event on the second muxer.
 * @param second_muxer_first  Invoke an event on the second muxer firstly if
 *                            @c TRUE, this value is applicable if both @p
 *                            first_event and @p second_event are @c TRUE.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/two_muxers"

#include "zf_test.h"
#include "rpc_zf.h"

/* Timeout used for @b zf_muxer_wait(). */
#define MUXER_TIMEOUT 100

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr_storage iut_addr2;
    struct sockaddr_storage tst_addr2;

    rpc_zf_muxer_set_p  muxer_set1 = RPC_NULL;
    rpc_zf_muxer_set_p  muxer_set2 = RPC_NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    zfts_zocket_type  first_zocket_type;
    zfts_zocket_type  second_zocket_type;
    te_bool           first_event;
    te_bool           second_event;
    te_bool           second_muxer_first;

    zfts_mzocket mzock1 = {0};
    zfts_mzocket mzock2 = {0};

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(first_zocket_type, ZFTS_ZOCKET_TYPES);
    TEST_GET_ENUM_PARAM(second_zocket_type, ZFTS_ZOCKET_TYPES);
    TEST_GET_BOOL_PARAM(first_event);
    TEST_GET_BOOL_PARAM(second_event);
    TEST_GET_BOOL_PARAM(second_muxer_first);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Create zockets in accordance to parameters
     * @p first_zocket_type and @p second_zocket_type. */

    zfts_create_mzocket(first_zocket_type, pco_iut, attr, stack,
                        iut_addr, pco_tst, tst_addr,
                        "the first zocket", &mzock1);

    CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr, &iut_addr2));
    CHECK_RC(tapi_sockaddr_clone(pco_tst, tst_addr, &tst_addr2));
    zfts_create_mzocket(second_zocket_type, pco_iut, attr, stack,
                        SA(&iut_addr2), pco_tst, SA(&tst_addr2),
                        "the second zocket", &mzock2);

    /*- Allocate two multiplexer sets. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set1);
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set2);

    /*- Add pointers to @a zf_waitable handles of zockets
     * to different muxer sets. */

    if (first_event)
        zfts_mzocket_prepare_events(&mzock1, RPC_EPOLLIN);
    if (second_event)
        zfts_mzocket_prepare_events(&mzock2, RPC_EPOLLIN);

    zfts_mzocket_add_events(muxer_set1, &mzock1, RPC_EPOLLIN);
    zfts_mzocket_add_events(muxer_set2, &mzock2, RPC_EPOLLIN);

    /*- Invoke events on muxers
     *  - on the first zocket if @p first_event is @c TRUE;
     *  - on the second zocket if @p second_event is @c TRUE;
     *  - if both - order dependence on @p second_muxer_first. */

    if (second_event && second_muxer_first)
        zfts_mzocket_invoke_events(&mzock2);

    if (first_event)
        zfts_mzocket_invoke_events(&mzock1);

    if (second_event && !second_muxer_first)
        zfts_mzocket_invoke_events(&mzock2);

    zfts_mzocket_check_events(pco_iut, muxer_set1, MUXER_TIMEOUT,
                              &mzock1, "The first muxer set");
    zfts_mzocket_check_events(pco_iut, muxer_set2, MUXER_TIMEOUT,
                              &mzock2, "The second muxer set");

    /*- Read and check data or establish connection using zockets. */

    if (first_event)
        zfts_mzocket_process_events(&mzock1);

    if (second_event)
        zfts_mzocket_process_events(&mzock2);

    TEST_SUCCESS;

cleanup:

    zfts_destroy_mzocket(&mzock1);
    zfts_destroy_mzocket(&mzock2);

    ZFTS_FREE(pco_iut, zf_muxer, muxer_set1);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set2);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
