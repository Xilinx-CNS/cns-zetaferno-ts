/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 */

/**
 * @page muxer-muxer_inout Process events using multiplexer expecting both incoming/outcoming events
 *
 * @objective Add zocket to a muxer set, send traffic from Tester and check
 *            that multiplexer handles all events gracefully.
 *
 * @param env           Testing environment:
 *                      - @ref arg_types_env_peer2peer
 * @param overlapped    Append @c ZF_EPOLLIN_OVERLAPPED to expected events
 * @param zocket_type   Determines tested zocket type:
 *                      - zft-act
 *                      - zft-pas
 * @param timeout       Timeout to pass to @ref rpc_zf_muxer_wait()
 *                      - @c -1
 *                      - @c 500
 *
 * @par Scenario:
 *
 * @author Sergey Nikitin <Sergey.Nikitin@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/muxer_inout"

#include "zf_test.h"

/* Size of a data chunk to send. */
#define CHUNK_SIZE 256

/* Number of chunks to send. */
#define CHUNKS_NUM 10

/* Total size of data to send. */
#define DATA_SIZE (CHUNK_SIZE * CHUNKS_NUM)

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p       attr = RPC_NULL;
    rpc_zf_stack_p      stack = RPC_NULL;
    rpc_zft_p           iut_z = RPC_NULL;
    rpc_zf_waitable_p   iut_z_waitable = RPC_NULL;
    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;
    zfts_zock_descr     zock_descrs[] = {
                            {&iut_z, "IUT zocket"},
                            {NULL, ""},
                        };

    te_bool overlapped = FALSE;
    zfts_zocket_type zocket_type;
    int timeout = -1;
    struct sockaddr *iut_addr_aux = NULL;
    int tst_s = -1;
    char send_data[DATA_SIZE];
    char recv_data[DATA_SIZE];
    int i = 0;
    uint32_t events = 0;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(overlapped);
    TEST_GET_ENUM_PARAM(zocket_type, ZFTS_ZOCKET_TYPES);
    TEST_GET_INT_PARAM(timeout);

    te_fill_buf(send_data, DATA_SIZE);

    TEST_STEP("Allocate ZF attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Create a zocket according to @p zocket_type and its peer "
              "on Tester.");
    CHECK_RC(tapi_sockaddr_clone2(iut_addr, &iut_addr_aux));
    zfts_create_zocket(zocket_type, pco_iut, attr, stack, iut_addr_aux,
                       pco_tst, tst_addr, &iut_z, &iut_z_waitable, &tst_s);

    TEST_STEP("Allocate a multiplexer set.");
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);

    TEST_STEP("Add ZF waitable object to the multiplexer set, waiting "
              "for @c EPOLLIN and @c EPOLLOUT events.");
    TEST_SUBSTEP("If @p overlapped is @c TRUE, append @c ZF_EPOLLIN_OVERLAPPED "
                 "to expected events.");
    events = RPC_EPOLLIN | RPC_EPOLLOUT;
    if (overlapped)
        events |= RPC_ZF_EPOLLIN_OVERLAPPED;
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_z_waitable, iut_z, events);

    TEST_STEP("Call @b zf_muxer_wait() and check that no event except "
              "@c EPOLLOUT will be triggered.");
    ZFTS_CHECK_MUXER_EVENTS(pco_iut, muxer_set,
                            "zf_muxer_wait() called before data is sent",
                            timeout, zock_descrs,
                            {RPC_EPOLLOUT, {.u32 = iut_z}});

    TEST_STEP("Send @b CHUNKS_NUM chunks of data from Tester "
              "and check events on IUT in a loop.");
    for (i = 0; i < CHUNKS_NUM; i++)
    {
        struct rpc_epoll_event ev;

        RPC_SEND(rc, pco_tst, tst_s, &send_data[CHUNK_SIZE * i],
                 CHUNK_SIZE, 0);
        TAPI_WAIT_NETWORK;
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_muxer_wait(pco_iut, muxer_set, &ev, 1, timeout);
        if (rc < 0)
        {
            TEST_VERDICT("zf_muxer_wait() failed with errno %r",
                         RPC_ERRNO(pco_iut));
        }
        else if (rc == 0)
        {
            TEST_VERDICT("zf_muxer_wait() returned zero events");
        }
    }

    TEST_STEP("Receive all data on IUT and check its correctness.");
    zfts_set_def_tcp_recv_func(ZFTS_TCP_RECV_ZFT_RECV);
    rc = zfts_zft_recv(pco_iut, iut_z, recv_data, DATA_SIZE);
    ZFTS_CHECK_RECEIVED_DATA(recv_data, send_data, rc, DATA_SIZE, "");

    TEST_SUCCESS;

cleanup:
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_z_waitable);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_z);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);
    free(iut_addr_aux);
    TEST_END;
}
