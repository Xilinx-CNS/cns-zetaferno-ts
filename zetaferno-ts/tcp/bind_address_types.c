/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-bind_address_types Test possible kinds of local address types to bind.
 *
 * @objective  Pass a specific address, @c INADDR_ANY or @c NULL, using
 *             nonzero or zero port, to @a zft_addr_bind() call. Check that
 *             data can be sent in all cases when bind call is successful.
 *
 * @note At the moment wildcard addresses and zero port are not supported.
 *
 * @param pco_iut   PCO on IUT.
 * @param pco_tst   PCO on TST.
 * @param iut_addr  IUT address.
 * @param tst_addr  Tester address.
 * @param addr_type Local address type:
 *      - @c IPv4:\<non-zero port>
 *      - @c INADDR_ANY:\<non-zero port>
 *      - @c IPv4:\<zero port>
 *      - @c INADDR_ANY:\<zero port>
 *      - @c NULL
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

#define TE_TEST_NAME  "tcp/bind_address_types"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    tapi_address_type      local_addr_type;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_handle_p  iut_zft_handle = RPC_NULL;
    rpc_zft_p         iut_zft = RPC_NULL;

    int            tst_s = -1;
    int            tst_s_listening = -1;

    struct rpc_pollfd poll_fd;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(local_addr_type, TAPI_ADDRESS_TYPE);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate TCP zocket. */
    rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);

    /*- Bind the zocket to address:port which type
     * is specified by @p addr_type. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = zfts_zft_handle_bind(pco_iut, iut_zft_handle,
                              local_addr_type, iut_addr, 0);
    if (rc < 0)
    {
        /* Expected behavior. */
        if (local_addr_type == TAPI_ADDRESS_NULL &&
            RPC_ERRNO(pco_iut) == RPC_EFAULT)
            TEST_SUCCESS;

        TEST_VERDICT("Function zft_addr_bind() unexpectedly "
                     "failed with errno %r",
                     RPC_ERRNO(pco_iut));
    }

    /*- Create listening socket on tester. */
    tst_s_listening =
      rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                 RPC_PROTO_DEF, FALSE, FALSE, tst_addr);
    rpc_listen(pco_tst, tst_s_listening, 1);

    /*- Connect the zocket to the tester socket. */
    rpc_zft_connect(pco_iut, iut_zft_handle, tst_addr, &iut_zft);
    iut_zft_handle = RPC_NULL;
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    poll_fd.fd = tst_s_listening;
    poll_fd.events = RPC_POLLIN;
    poll_fd.revents = 0;

    rc = rpc_poll(pco_tst, &poll_fd, 1, 1000);
    if (rc == 0)
        TEST_VERDICT("Listening TCP socket does not see "
                     "incoming connection");

    tst_s = rpc_accept(pco_tst, tst_s_listening, NULL, NULL);

    /*- Check that data can be transmitted in both directions. */
    zfts_zft_check_connection(pco_iut, stack, iut_zft, pco_tst, tst_s);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s_listening);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);

    CLEANUP_RPC_ZFT_HANDLE_FREE(pco_iut, iut_zft_handle);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
