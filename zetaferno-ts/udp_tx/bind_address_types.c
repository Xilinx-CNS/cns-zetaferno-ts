/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP TX tests
 *
 * $Id:$
 */

/**
 * @page udp_tx-bind_address_types Test possible kinds of local address types to bind.
 *
 * @objective  Pass a specific address, @c INADDR_ANY or @c NULL using
 *             (non-)zero port as local address argument in
 *             @a rpc_zfut_alloc() call. Check that datagrams can be sent in
 *             all cases when the call is succeeded.
 *
 * @note At the moment wildcard addresses are not supported.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param iut_addr      IUT address.
 * @param tst_addr      Tester address.
 * @param func          Transmitting function.
 * @param large_buffer  Use large data buffer to send.
 * @param few_iov       Use several iov vectors.
 * @param addr_type     Local address type:
 *      - @c IPv4:\<non-zero port>
 *      - @c INADDR_ANY:\<non-zero port>
 *      - @c IPv4:\<zero port> (unsupported)
 *      - @c INADDR_ANY:\<zero port> (unsupported)
 *      - @c NULL (unsupported)
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_tx/bind_address_types"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    zfts_send_function     func;
    te_bool                large_buffer;
    te_bool                few_iov;
    tapi_address_type      addr_type;

    struct sockaddr *laddr;
    rpc_zf_attr_p    attr = RPC_NULL;
    rpc_zf_stack_p   stack = RPC_NULL;
    rpc_zfut_p       iut_utx = RPC_NULL;
    int              tst_s = -1;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);
    TEST_GET_ENUM_PARAM(addr_type, TAPI_ADDRESS_TYPE);

    laddr = tapi_sockaddr_clone_typed(iut_addr, addr_type);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate and bind Zetaferno UDP TX zocket, @c laddr value is
     * dependent on the test argument @p addr_type. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zfut_alloc(pco_iut, &iut_utx, stack, laddr, tst_addr, 0, attr);

    switch (addr_type)
    {
        case TAPI_ADDRESS_NULL:
            if (rc >= 0)
                TEST_VERDICT("zfut_alloc() should fail if NULL is passed "
                             "to laddr");
            CHECK_RPC_ERRNO(pco_iut, RPC_EFAULT, "zfut_alloc() failed, but");

            TEST_SUCCESS;
            break;

        case TAPI_ADDRESS_SPECIFIC_ZERO_PORT:
        case TAPI_ADDRESS_WILDCARD_ZERO_PORT:
            if (rc >= 0)
                TEST_VERDICT("zfut_alloc() should fail if zero port is "
                             "passed to laddr");
            CHECK_RPC_ERRNO(pco_iut, RPC_EINVAL, "zfut_alloc() failed, but");

            TEST_SUCCESS;
            break;

        default:
            ;
    }

    /*- Report the verdict if zocket allocation fails. */
    if (rc != 0)
        TEST_VERDICT("Function zfut_alloc() failed with errno: %r",
                     RPC_ERRNO(pco_iut));

    /*- Create UDP socket on tester and bind to the local IPv4 address. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    /*- Send few datagrmas from the zocket, receive them on tester and check
     * data. */
    zfts_zfut_check_send_func(pco_iut, stack, iut_utx, pco_tst, tst_s, func,
                              large_buffer, few_iov);

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    if (iut_utx != RPC_NULL)
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_utx);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
