/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Bad Parameters and Boundary Values
 *
 * $Id$
 */

/** @page bnbvalue-bad_endpoints_limit Using inappropriate values for attributes setting endpoints limits
 *
 * @objective Check that ZF stack cannot be created if wrong values
 *            are set for attributes setting endpoints limits.
 *
 * @param pco_iut       PCO on IUT.
 * @param attibute      Which attribute to set:
 *                      - @c "max_udp_rx_endpoints";
 *                      - @c "max_udp_tx_endpoints";
 *                      - @c "max_tcp_endpoints";
 *                      - @c "max_tcp_listen_endpoints".
 * @param value         Value to set for @p attribute.
 *                      - @c 0
 *                      - @c 65
 *                      - @c -10
 *
 * @type conformance, robustness
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME  "bnbvalue/bad_endpoints_limits"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    const char *attribute;
    int         value;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_STRING_PARAM(attribute);
    TEST_GET_INT_PARAM(value);

    rpc_zf_init(pco_iut);

    /*- Allocate Zetaferno attributes object. */
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Set @p attribute to @p value. */
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_attr_set_int(pco_iut, attr, attribute, value);
    if (rc < 0)
    {
        RING_VERDICT("zf_attr_set_int() fails with errno %r "
                     "when trying to set wrong value to attribute",
                     RPC_ERRNO(pco_iut));
        TEST_SUCCESS;
    }

    /*- Try to allocate ZF stack with the attributes object
     * and check that it fails. */
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_stack_alloc(pco_iut, attr, &stack);
    if (rc >= 0)
        TEST_VERDICT("Creating ZF stack with bad attribute "
                     "value was successful");
    else if (RPC_ERRNO(pco_iut) != RPC_EINVAL)
        TEST_VERDICT("zf_stack_alloc() fails with unexpected errno %r",
                     RPC_ERRNO(pco_iut));

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
