/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-listen_address_types Test possible kinds of local address types to bind listener zocket to
 *
 * @objective Pass a specific address, @c INADDR_ANY or @c NULL, using
 *            non-zero or zero port, to @a zftl_listen() call. Check that
 *            connections can be accepted in all cases when the call is
 *            successful.
 *
 * @param pco_iut             PCO on IUT.
 * @param pco_tst             PCO on TST.
 * @param iut_addr            IUT address.
 * @param tst_addr            Tester address.
 * @param addr_type           Local address type:
 *                            - @c IPv4:\<non-zero port>
 *                            - @c INADDR_ANY:\<non-zero port>
 *                            - @c IPv4:\<zero port>
 *                            - @c INADDR_ANY:\<zero port>
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "tcp/listen_address_types"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr       *listen_addr = NULL;
    tapi_address_type      addr_type;

    struct sockaddr_storage connect_addr;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zftl_p     iut_tl = RPC_NULL;
    rpc_zft_p      iut_ts = RPC_NULL;
    int            tst_s = -1;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(addr_type, TAPI_ADDRESS_TYPE);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    listen_addr = tapi_sockaddr_clone_typed(iut_addr, addr_type);
    tapi_sockaddr_clone_exact(iut_addr, &connect_addr);

    /*- Create listening zocket:
     *  - use as @b laddr address:port which type is specified
     *    by @p addr_type. */
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zftl_listen(pco_iut, stack, listen_addr, attr, &iut_tl);
    /*- If the zocket creation is succeessful and we can know to which
     * address to connect, continue test execution, otherwise exit
     * with a verdict. */
    if (rc < 0)
        TEST_VERDICT("zftl_listen() failed with errno %r",
                     RPC_ERRNO(pco_iut));
    else if (addr_type == TAPI_ADDRESS_NULL)
        TEST_VERDICT("zftl_listen() succeeded with NULL address");
    else if (addr_type == TAPI_ADDRESS_SPECIFIC_ZERO_PORT ||
             addr_type == TAPI_ADDRESS_WILDCARD_ZERO_PORT)
    {
        RPC_AWAIT_ERROR(pco_iut);
        rpc_zftl_getname(pco_iut, iut_tl, SIN(&connect_addr));
        if (!RPC_IS_CALL_OK(pco_iut))
            TEST_VERDICT("zftl_getname() failed with errno %r",
                         RPC_ERRNO(pco_iut));
    }

    /*- Create TCP socket on Tester and connect it to IUT zocket. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_STREAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    pco_tst->op = RCF_RPC_CALL;
    rpc_connect(pco_tst, tst_s, SA(&connect_addr));

    ZFTS_WAIT_NETWORK(pco_iut, stack);

    pco_tst->op = RCF_RPC_WAIT;
    RPC_AWAIT_ERROR(pco_tst);
    rc = rpc_connect(pco_tst, tst_s, SA(&connect_addr));
    if (rc < 0)
        TEST_VERDICT("connect() failed with errno %r",
                     RPC_ERRNO(pco_tst));

    /*- Accept connection on the listening zocket. */
    rpc_zftl_accept(pco_iut, iut_tl, &iut_ts);

    /*- Check data transmission. */
    zfts_zft_check_connection(pco_iut, stack, iut_ts,
                              pco_tst, tst_s);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_tl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_ts);

    free(listen_addr);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

