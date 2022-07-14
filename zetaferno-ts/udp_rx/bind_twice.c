/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id:$
 */

/**
 * @page udp_rx-bind_twice  Bind twice to the same local and remote addresses couple
 *
 * @objective  Try to bind to the same addresses couple twice, check that
 *             the second call fails gracefully.
 *
 * @param pco_iut           PCO on IUT.
 * @param pco_tst           PCO on TST.
 * @param iut_addr          IUT address.
 * @param tst_addr          Tester address.
 * @param local_addr_type   Local address type.
 * @param remote_addr_type  Remote address type.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/bind_twice"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    tapi_address_type      local_addr_type;
    tapi_address_type      remote_addr_type;

    struct sockaddr *local_addr = NULL;
    struct sockaddr *remote_addr = NULL;
    rpc_zf_attr_p    attr = RPC_NULL;
    rpc_zf_stack_p   stack = RPC_NULL;
    rpc_zfur_p       iut_s = RPC_NULL;
    int              tst_s = -1;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_TYPED_ADDR(tst_addr, remote_addr_type, remote_addr);
    TEST_GET_TYPED_ADDR(iut_addr, local_addr_type, local_addr);

    /*- Allocate Zetaferno attributes and stack. */
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Allocate Zetaferno UDP RX zocket. */
    rpc_zfur_alloc(pco_iut, &iut_s, stack, attr);

    /*- Bind the zocket, local and remote addresses can be an IP address,
     *  @c INADDR_ANY or @c NULL. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zfur_addr_bind(pco_iut, iut_s, local_addr, remote_addr, 0);
    if (rc != 0)
        TEST_VERDICT("UDP RX zocket binding failed with errno %r",
                     RPC_ERRNO(pco_iut));

    /*- Check that attempt to bind to the same addresses couple fails with
     * appropriate error code. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zfur_addr_bind(pco_iut, iut_s, local_addr, remote_addr, 0);
    if (rc == 0)
        TEST_VERDICT("Successful bind to the same addresses couple twice");
    else if (RPC_ERRNO(pco_iut) != RPC_EADDRINUSE)
        TEST_VERDICT("The second bind failed with wrong errno: %r instead "
                     "of %r", RPC_ERRNO(pco_iut), RPC_EADDRINUSE);

    /*- Create socket on tester and send a few datagrams. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    /*- Read datagrams on the zocket. */
    zfts_zfur_check_reception(pco_iut, stack, iut_s, pco_tst, tst_s,
                              iut_addr);

    /*- Release the zocket. */
    rpc_zfur_free(pco_iut, iut_s);

    /*- Allocate new UDP RX zocket. */
    rpc_zfur_alloc(pco_iut, &iut_s, stack, attr);

    /*- Bind it to the same address couple as the previous. */
    rpc_zfur_addr_bind(pco_iut, iut_s, local_addr, remote_addr, 0);

    /*- Send a few datagrams from tester and read them on the new zocket. */
    zfts_zfur_check_reception(pco_iut, stack, iut_s, pco_tst, tst_s,
                              iut_addr);

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    free(local_addr);
    free(remote_addr);

    TEST_END;
}
