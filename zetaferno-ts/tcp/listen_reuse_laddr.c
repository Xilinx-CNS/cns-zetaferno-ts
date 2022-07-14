/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-listen_reuse_laddr Try to create two listener zockets with the same laddr.
 *
 * @objective Create two listener zockets binding them to the completely or
 *            partly same address:port couple.
 *
 * @param first_addr_type  Local address type of the first listener:
 *                         - all supported address types
 * @param second_addr_type Local address type of the second listener:
 *                         - all supported address types
 * @param same_stack       Use the same stack for both listener zockets if
 *                         @c TRUE.
 * @param close_listener   Accept TCP connection on the zocket and close the
 *                         listener if @c TRUE.
 * @param close_accepted   Accept TCP connection on the zocket and close the
 *                         accepted zocket if @c TRUE.
 * @param close_iut_first  Close accepted zocket first and then tester socket
 *                         if @c TRUE and @p close_accepted is TRUE.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/listen_reuse_laddr"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server          *pco_iut = NULL;
    rcf_rpc_server          *pco_tst = NULL;
    const struct sockaddr   *iut_addr = NULL;
    const struct sockaddr   *tst_addr = NULL;

    tapi_address_type      first_addr_type;
    tapi_address_type      second_addr_type;
    struct sockaddr       *listen_addr1 = NULL;
    struct sockaddr       *listen_addr2 = NULL;

    rpc_zf_attr_p   attr = RPC_NULL;
    rpc_zf_stack_p  stack1 = RPC_NULL;
    rpc_zf_stack_p  stack2 = RPC_NULL;

    rpc_zftl_p     iut_zftl1 = RPC_NULL;
    rpc_zftl_p     iut_zftl2 = RPC_NULL;
    rpc_zft_p      iut_zft1 = RPC_NULL;
    int            tst_s = -1;

    te_bool same_stack;
    te_bool close_listener;
    te_bool close_accepted;
    te_bool close_iut_first;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(first_addr_type, TAPI_ADDRESS_TYPE);
    TEST_GET_ENUM_PARAM(second_addr_type, TAPI_ADDRESS_TYPE);
    TEST_GET_BOOL_PARAM(same_stack);
    TEST_GET_BOOL_PARAM(close_listener);
    TEST_GET_BOOL_PARAM(close_accepted);
    TEST_GET_BOOL_PARAM(close_iut_first);

    listen_addr1 = tapi_sockaddr_clone_typed(iut_addr,
                                             first_addr_type);
    listen_addr2 = tapi_sockaddr_clone_typed(iut_addr,
                                             second_addr_type);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Allocate Zetaferno stack once or for each zocket in dependence on
     * @p same_stack. */

    rpc_zf_stack_alloc(pco_iut, attr, &stack1);

    if (same_stack)
        stack2 = stack1;
    else
        rpc_zf_stack_alloc(pco_iut, attr, &stack2);

    /*- Create first listener zocket, use @b laddr which type is specified
     * by @p first_addr_type. */
    rpc_zftl_listen(pco_iut, stack1, listen_addr1, attr, &iut_zftl1);

    /*- Create socket on tester and connect it to the zocket.
     *- Accept the connection on IUT. */
    zfts_connect_to_zftl(pco_iut, stack1, iut_zftl1, &iut_zft1,
                         listen_addr1, pco_tst, &tst_s, tst_addr);

    /*- Close the listener zocket if @p close_listener is @c TRUE. */
    if (close_listener)
    {
        rpc_zftl_free(pco_iut, iut_zftl1);
        iut_zftl1 = RPC_NULL;
    }

    /*- Close the accepted zocket before or after tester socket
     * closing in dependence on @p close_iut_first, if
     * @p close_accepted is @c TRUE. */
    if (close_accepted)
    {
        if (close_iut_first)
        {
            rpc_zft_free(pco_iut, iut_zft1);
            iut_zft1 = RPC_NULL;
        }

        rpc_close(pco_tst, tst_s);
        tst_s = -1;

        if (!close_iut_first)
        {
            ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack1);
            rpc_zft_free(pco_iut, iut_zft1);
            iut_zft1 = RPC_NULL;
        }
    }

    /* Wait a bit and process events to make sure that all connection
     * finalizing actions are done. */
    ZFTS_WAIT_NETWORK(pco_iut, stack1);

    /*- Create second listener zocket, use @b laddr which type is
     * specified by @p second_addr_type. The call should fail unless
     * listener and accepted zockets are closed and
     * tester socket is closed before closing accepted zocket. */

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zftl_listen(pco_iut, stack2, listen_addr2, attr, &iut_zftl2);
    if (close_listener && close_accepted && !close_iut_first)
    {
        if (rc < 0)
            TEST_VERDICT("The second zftl_listen() unexpectedly "
                         "failed with errno %r",
                         RPC_ERRNO(pco_iut));
    }
    else
    {
        if (rc >= 0)
            TEST_VERDICT("The second zftl_listen() unexpectedly "
                         "succeeded");
        else if (RPC_ERRNO(pco_iut) != RPC_EADDRINUSE)
            TEST_VERDICT("The second zftl_listen() failed with unexpected "
                         "errno %r", RPC_ERRNO(pco_iut));
    }

    TEST_SUCCESS;

cleanup:

    free(listen_addr1);
    free(listen_addr2);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl2);

    /*
     * Note: this check should be performed before releasing stack1 due to the
     * fact that after that the value of stack1 will be reset to RPC_NULL.
     */
    if (stack2 != stack1)
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack2);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack1);

    CLEANUP_RPC_ZF_ATTR_FREE(pco_iut, attr);
    CLEANUP_RPC_ZF_DEINIT(pco_iut);

    TEST_END;
}

