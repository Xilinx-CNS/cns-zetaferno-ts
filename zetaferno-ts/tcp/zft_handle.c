/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-zft_handle Test zft_handle processing without connection.
 *
 * @objective Perform all possible actions on a zft_handle zocket.
 *
 * @param bind    Bind zft_handle zocket if @c TRUE.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/zft_handle"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    const struct sockaddr *iut_addr = NULL;

    struct sockaddr_in laddr;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p      iut_zft_handle = RPC_NULL;

    te_bool bind = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_BOOL_PARAM(bind);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate zft_handle zocket. */
    rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);

    /*- Call @c zft_handle_getname(), check that it returns
     * zero address. */
    rpc_zft_handle_getname(pco_iut, iut_zft_handle, &laddr);

    if (laddr.sin_family != AF_INET ||
        laddr.sin_port != 0 ||
        laddr.sin_addr.s_addr != 0)
        ERROR_VERDICT("zft_handle_getname() returned strange "
                      "local address for not bound zft_handle");

    /*- If @p bind is TRUE:
     *  - bind zft_handle zocket;
     *  - check its address using @b zft_handle_getname(). */
    if (bind)
    {
        rpc_zft_addr_bind(pco_iut, iut_zft_handle, iut_addr, 0);
        rpc_zft_handle_getname(pco_iut, iut_zft_handle, &laddr);
        if (tapi_sockaddr_cmp(SA(&laddr), iut_addr) != 0)
            ERROR_VERDICT("zft_handle_getname() returned incorrect "
                          "local address for bound zft_handle");
    }

    /*- Free zft_handle zocket. */
    rpc_zft_handle_free(pco_iut, iut_zft_handle);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
