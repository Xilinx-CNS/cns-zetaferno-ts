/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id:$
 */

/**
 * @page tcp-endpoints_limit Examine active open TCP zockets number limit
 *
 * @objective Specify maximum number of endpoints using ZF attribute
 *            @b max_tcp_endpoints, allocate all possible zockets and check
 *            that they can send/receive data.
 *
 * @param pco_iut             PCO on IUT.
 * @param pco_tst             PCO on TST.
 * @param iut_addr            IUT address.
 * @param tst_addr            Tester address.
 * @param max_endpoints       Endpoints number to specify with attribute
 *                            @b max_tcp_endpoints:
 *                            - do not set (default endpoints number
 *                              is @c 64);
 *                            - @c 1;
 *                            - @c 10;
 *                            - @c 64;
 * @param connect_after_bind   Connect each zocket just after binding
 *                             if @c TRUE, otherwise connect zockets
 *                             when all of them are allocated and bound.
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

#define TE_TEST_NAME  "tcp/endpoints_limit"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr_storage iut_bind_addr;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    int       max_tcp_endpoints;
    te_bool   connect_after_bind;

    rpc_zft_handle_p    iut_zft_handles[ZFTS_MAX_TCP_ENDPOINTS + 1];
    rpc_zft_p           iut_zfts[ZFTS_MAX_TCP_ENDPOINTS + 1];
    int                 tst_socks[ZFTS_MAX_TCP_ENDPOINTS + 1];

    int tst_s_listening = -1;

    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(max_tcp_endpoints);
    TEST_GET_BOOL_PARAM(connect_after_bind);

    ZFTS_INIT_STATIC_ARRAY(iut_zft_handles, RPC_NULL);
    ZFTS_INIT_STATIC_ARRAY(iut_zfts, RPC_NULL);
    ZFTS_INIT_STATIC_ARRAY(tst_socks, -1);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Set attribute @b max_tcp_endpoints to @p max_endpoints if
     * it is greater than zero */
    if (max_tcp_endpoints < 0)
        max_tcp_endpoints = ZFTS_MAX_TCP_ENDPOINTS;
    else
        rpc_zf_attr_set_int(pco_iut, attr, "max_tcp_endpoints",
                            max_tcp_endpoints);

    /*- Allocate Zetaferno stack. */
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    tst_s_listening =
      rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                 RPC_PROTO_DEF, FALSE, FALSE, tst_addr);
    rpc_listen(pco_tst, tst_s_listening, 1);

    /*- Allocate and bind maximum number of zockets:
     *  - connect zockets if @p connect_after_bind is @c TRUE. */
    /*- Try to allocate one more zocket, the call should fail. */
    for (i = 0; i < max_tcp_endpoints + 1; i++)
    {
        RING("Allocating and binding zocket number %d", i + 1);
        tapi_sockaddr_clone(pco_iut, iut_addr, &iut_bind_addr);

        RPC_AWAIT_IUT_ERROR(pco_iut);
        rc = rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handles[i]);
        if (rc < 0)
        {
            if (i >= max_tcp_endpoints)
            {
                if (RPC_ERRNO(pco_iut) != RPC_ENOBUFS)
                    ERROR_VERDICT("zft_alloc() failed with "
                                  "unexpected errno %r",
                                  RPC_ERRNO(pco_iut));

                break; /* Expected behavior */
            }
            else
            {
                TEST_VERDICT("zft_alloc() unexpectedly failed "
                             "with errno %r", RPC_ERRNO(pco_iut));
            }
        }

        rpc_zft_addr_bind(pco_iut, iut_zft_handles[i],
                          SA(&iut_bind_addr), 0);

        if (connect_after_bind)
        {
            rpc_zft_connect(pco_iut, iut_zft_handles[i],
                            tst_addr, &iut_zfts[i]);
            iut_zft_handles[i] = RPC_NULL;
            ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);

            tst_socks[i] = rpc_accept(pco_tst, tst_s_listening,
                                      NULL, NULL);
        }
    }

    /*- If @p connect_after_bind is @c FALSE:
     *  - connect all zockets now. */
    if (!connect_after_bind)
    {
        for (i = 0; i < max_tcp_endpoints; i++)
        {
            rpc_zft_connect(pco_iut, iut_zft_handles[i],
                            tst_addr, &iut_zfts[i]);
            iut_zft_handles[i] = RPC_NULL;
            ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);

            tst_socks[i] = rpc_accept(pco_tst, tst_s_listening,
                                      NULL, NULL);
        }
    }

    /*- Check data transmission on all established connections. */
    for (i = 0; i < max_tcp_endpoints; i++)
    {
        zfts_zft_check_connection(pco_iut, stack, iut_zfts[i],
                                  pco_tst, tst_socks[i]);
    }

    TEST_SUCCESS;

cleanup:

    for (i = 0; i < ZFTS_MAX_TCP_ENDPOINTS + 1; i++)
    {
        CLEANUP_RPC_CLOSE(pco_tst, tst_socks[i]);

        CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zfts[i]);

        CLEANUP_RPC_ZFT_HANDLE_FREE(pco_iut, iut_zft_handles[i]);
    }

    CLEANUP_RPC_CLOSE(pco_tst, tst_s_listening);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
