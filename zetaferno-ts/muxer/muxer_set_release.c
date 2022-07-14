/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-muxer_set_release Allocate and release a muxer set a lot of times
 *
 * @objective In a loop allocate and release a muxer set, which can have
 *            a zocket in it.
 *
 * @param set_state           Whether there is a zocket in the muxer set:
 *                            - none (no zocket in the set)
 *                            - add (add a zocket)
 *                            - del (add a zocket and then delete it from
 *                              the set)
 *                            - del_late (add a zocket and delete it when
 *                              muxer is released)
 * @param call_wait           Call @b zf_muxer_wait() if @c TRUE.
 * @param iterations_number   The main loop iterations number:
 *                            - @c 1000
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/muxer_set_release"

#include "zf_test.h"

/**
 * State of muxer set.
 */
typedef enum {
    ZFTS_MSET_NONE,       /**< No zockets. */
    ZFTS_MSET_ADD,        /**< A zocket is in a set. */
    ZFTS_MSET_DEL,        /**< A zocket was added and deleted. */
    ZFTS_MSET_DEL_LATE,   /**< A zocket was added and will be deleted
                               after muxer set is released. */
} zfts_mset_state;

/**
 * List of muxer set states to be passed to macro @b TEST_GET_ENUM_PARAM.
 */
#define ZFTS_MSET_STATES \
    { "none",     ZFTS_MSET_NONE }, \
    { "add",      ZFTS_MSET_ADD }, \
    { "del",      ZFTS_MSET_DEL }, \
    { "del_late", ZFTS_MSET_DEL_LATE }

/* Disable/enable RPC logging. */
#define VERBOSE_LOGGING FALSE

int
main(int argc, char *argv[])
{
    rcf_rpc_server          *pco_iut = NULL;
    rcf_rpc_server          *pco_tst = NULL;
    const struct sockaddr   *iut_addr = NULL;
    const struct sockaddr   *tst_addr = NULL;
    struct sockaddr_storage  iut_addr_aux;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    int                 tst_s = -1;
    rpc_zfur_p          iut_zfur = RPC_NULL;
    rpc_zf_waitable_p   iut_zfur_waitable = RPC_NULL;
    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;

    struct rpc_epoll_event     event;

    char                send_data[ZFTS_DGRAM_MAX];
    char                recv_data[ZFTS_DGRAM_MAX];

    zfts_mset_state     set_state = ZFTS_MSET_NONE;
    te_bool             call_wait = FALSE;
    int                 iterations_number = 0;

    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(set_state, ZFTS_MSET_STATES);
    TEST_GET_BOOL_PARAM(call_wait);
    TEST_GET_INT_PARAM(iterations_number);

    te_fill_buf(send_data, ZFTS_DGRAM_MAX);
    tapi_sockaddr_clone_exact(iut_addr, &iut_addr_aux);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate UDP RX zocket if @p set_state is not @c none. */
    if (set_state != ZFTS_MSET_NONE)
    {
        rpc_zfur_alloc(pco_iut, &iut_zfur, stack, attr);
        rpc_zfur_addr_bind(pco_iut, iut_zfur,
                           SA(&iut_addr_aux), tst_addr, 0);
        iut_zfur_waitable = rpc_zfur_to_waitable(pco_iut, iut_zfur);

        tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                           RPC_SOCK_DGRAM, RPC_PROTO_DEF);
        rpc_bind(pco_tst, tst_s, tst_addr);
        rpc_connect(pco_tst, tst_s, SA(&iut_addr_aux));
    }

    /*- In the loop do @p iterations_number times: */

    if (!VERBOSE_LOGGING)
    {
        pco_iut->silent_pass = pco_iut->silent_pass_default = TRUE;
        pco_tst->silent_pass = pco_tst->silent_pass_default = TRUE;
    }

    RING("Starting a loop repeatedly allocating and releasing muxer set "
         "%d times", iterations_number);

    for (i = 0; i < iterations_number; i++)
    {
        /*- Allocate a muxer set. */
        rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);

        /*- Add the UDP RX zocket to the the set if @p set_state is
         * not @c none. Send a datagram from Tester. */
        if (set_state != ZFTS_MSET_NONE)
        {
            rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_zfur_waitable,
                                    iut_zfur, RPC_EPOLLIN);
            rpc_send(pco_tst, tst_s, send_data, ZFTS_DGRAM_MAX, 0);
            ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);
        }

        /*- Call @b zf_muxer_wait() with zero timeout if @p call_wait
         * is @c TRUE. */
        if (call_wait)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zf_muxer_wait(pco_iut, muxer_set, &event, 1, 0);
            if (rc < 0)
                TEST_VERDICT("zf_muxer_wait () failed with errno %r",
                             RPC_ERRNO(pco_iut));
            else if (rc < 1 && set_state != ZFTS_MSET_NONE)
                TEST_VERDICT("zf_muxer_wait() has not returned events");
            else if (rc > 0 && set_state == ZFTS_MSET_NONE)
                TEST_VERDICT("zf_muxer_wait() returned unexpected events");
            else if (rc > 1 || (rc == 1 && event.data.u32 != iut_zfur))
                TEST_VERDICT("zf_muxer_wait() returned strange result");
        }

        /*- Delete the zocket from the set if @p set_state is @c del. */
        if (set_state == ZFTS_MSET_DEL)
            rpc_zf_muxer_del(pco_iut, iut_zfur_waitable);

        /*- Release the muxer set. */
        rpc_zf_muxer_free(pco_iut, muxer_set);

        /*- Delete the zocket from the set if @p set_state is
         * @c del_late. */
        if (set_state == ZFTS_MSET_DEL_LATE)
            rpc_zf_muxer_del(pco_iut, iut_zfur_waitable);

        /*- Read data on the zocket if required. */
        if (set_state != ZFTS_MSET_NONE)
        {
            rc = zfts_zfur_recv(pco_iut, iut_zfur,
                                recv_data, ZFTS_DGRAM_MAX);
            ZFTS_CHECK_RECEIVED_DATA(recv_data, send_data,
                                     rc, ZFTS_DGRAM_MAX,
                                     " from IUT");
        }
    }

    RING("Finished the loop");

    TEST_SUCCESS;

cleanup:

    pco_iut->silent_pass = pco_iut->silent_pass_default = FALSE;
    pco_tst->silent_pass = pco_tst->silent_pass_default = FALSE;

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_zfur);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zfur_waitable);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

