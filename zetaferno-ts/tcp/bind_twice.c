/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-bind_twice Try to bind twice
 *
 * @objective  Attempt to bind twice should fail, no matter which address
 *             is supplied.
 *
 * @param pco_iut     PCO on IUT.
 * @param iut_addr    IUT address.
 * @param addr_type   Bind address type.
 * @param same_addr   Use the same address for the second bind call
 *                    if @c TRUE.
 * @param same_port   Use the same port for the second bind call
 *                    if @c TRUE.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/bind_twice"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    const struct sockaddr *iut_addr1 = NULL;
    const struct sockaddr *iut_addr2 = NULL;

    struct sockaddr           *iut_bind_addr1 = NULL;
    struct sockaddr_storage    iut_bind_addr2;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_handle_p  iut_zft_handle = RPC_NULL;

    tapi_address_type     addr_type;
    te_bool               same_addr;
    te_bool               same_port;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_ADDR(pco_iut, iut_addr1);
    TEST_GET_ADDR(pco_iut, iut_addr2);
    TEST_GET_ENUM_PARAM(addr_type, TAPI_ADDRESS_TYPE);
    TEST_GET_BOOL_PARAM(same_addr);
    TEST_GET_BOOL_PARAM(same_port);

    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate TCP zocket handle. */
    rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);

    /*- Bind the zocket to address:port which type
     * is specified by @p addr_type. */

    iut_bind_addr1 = tapi_sockaddr_clone_typed(iut_addr1,
                                               addr_type);

    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zft_addr_bind(pco_iut, iut_zft_handle,
                           iut_bind_addr1, 0);
    if (rc < 0)
        TEST_VERDICT("Function zft_addr_bind() unexpectedly "
                     "failed with errno %r",
                     RPC_ERRNO(pco_iut));

    if (same_port)
        tapi_sockaddr_clone_exact(iut_bind_addr1, &iut_bind_addr2);
    else
        CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_bind_addr1,
                                     &iut_bind_addr2));

    if (!same_addr)
        te_sockaddr_set_netaddr(SA(&iut_bind_addr2),
                                te_sockaddr_get_netaddr(iut_addr2));

    /*- Try to bind the second time with changed address or port value
     * according to parameters @p same_addr and @p same_port.
     * The second call of @b zft_addr_bind() should fail. */

    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zft_addr_bind(pco_iut, iut_zft_handle,
                           SA(&iut_bind_addr2), 0);
    if (rc >= 0)
        TEST_VERDICT("zft_addr_bind() unexpectedly succeeded when called "
                     "the second time");
    else if (rc < 0 && RPC_ERRNO(pco_iut) != RPC_EINVAL)
        RING_VERDICT("The second call of zft_addr_bind() failed with "
                     "unexpected errno %r", RPC_ERRNO(pco_iut));

    TEST_SUCCESS;

cleanup:

    free(iut_bind_addr1);
    CLEANUP_RPC_ZFT_HANDLE_FREE(pco_iut, iut_zft_handle);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
