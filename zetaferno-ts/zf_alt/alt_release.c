/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-alt_release Allocate and release ZF alternatives many times.
 *
 * @objective Allocate and release ZF alternatives many times, check for
 *            resources leakage.
 *
 * @param iterations_num    How many times to allocate and release
 *                          alternatives.
 * @param alt_action        Action to do with alternatives:
 *                          - none (do nothing)
 *                          - send (send an alternative)
 *                          - cancel (cancel all alternatives)
 * @param alt_release       How to release the alternative:
 *                          - queue (release queue)
 *                          - stack (re-create stack)
 * @param open_method       How to open connection:
 *                          - @c active
 *                          - @c passive
 *                          - @c passive_close (close listener after
 *                            passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "zf_alt/alt_release"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_sockets.h"

/**< How long to wait for a sent packet on a peer (milliseconds). */
#define DATA_WAIT_TIMEOUT 500

/** Actions to perform on alternatives. */
typedef enum {
    ZFTS_ALT_ACTION_NONE,     /**< Do nothing. */
    ZFTS_ALT_ACTION_SEND,     /**< @b zf_alternatives_send(). */
    ZFTS_ALT_ACTION_CANCEL,   /**< @b zf_alternatives_cancel(). */
} zfts_alt_action_t;

/** List of action types to be passed to @b TEST_GET_ENUM_PARAM(). */
#define ZFTS_ALT_ACTION_TYPES \
    { "none",   ZFTS_ALT_ACTION_NONE }, \
    { "send",   ZFTS_ALT_ACTION_SEND }, \
    { "cancel", ZFTS_ALT_ACTION_CANCEL }

/** How to release alternatives. */
typedef enum {
    ZFTS_ALT_RELEASE_QUEUE,   /**< Call @b zf_alternatives_release(). */
    ZFTS_ALT_RELEASE_STACK,   /**< Free stack. */
} zfts_alt_release_t;

/**
 * List of alternatives release methods to be passed to @b
 * TEST_GET_ENUM_PARAM().
 */
#define ZFTS_ALT_RELEASE_TYPES \
    { "queue", ZFTS_ALT_RELEASE_QUEUE }, \
    { "stack", ZFTS_ALT_RELEASE_STACK }

/** Packet size used in this test. */
#define PKT_SIZE 512

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr_storage cur_iut_addr;
    struct sockaddr_storage cur_tst_addr;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p iut_zftl = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    int       tst_s = -1;

    int       iterations_num;
    int       i;

    int n_alts;
    int n_alts_send;
    int alt_count_max;
    int j;
    int k;
    int l;
    int m;

    te_dbuf send_data = TE_DBUF_INIT(0);
    te_dbuf recv_data = TE_DBUF_INIT(0);

    rpc_zf_althandle  *alts;
    te_bool           *alts_send;

    zfts_alt_action_t   alt_action;
    zfts_alt_release_t  alt_release;

    rpc_iovec          **iovs;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(iterations_num);
    TEST_GET_ENUM_PARAM(alt_action, ZFTS_ALT_ACTION_TYPES);
    TEST_GET_ENUM_PARAM(alt_release, ZFTS_ALT_RELEASE_TYPES);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    CHECK_RC(tapi_sh_env_get_int(pco_iut, "ZFTS_ALT_COUNT_DEF",
                                 &alt_count_max));

    alts = (rpc_zf_althandle *)tapi_calloc(sizeof(*alts), alt_count_max);
    alts_send = (te_bool *)tapi_calloc(sizeof(*alts_send), alt_count_max);
    iovs = (rpc_iovec **)tapi_calloc(sizeof(*iovs), alt_count_max);

    for (j = 0; j < alt_count_max; j++)
    {
        rpc_alloc_iov(&iovs[j], ZFTS_IOVCNT,
                      1, PKT_SIZE);
    }

    rpc_zf_init(pco_iut);

    /*- Allocate ZF attributes. */
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- In the loop @p iterations_num times: */
    for (i = 0; i < iterations_num; i++)
    {
        RING("Iteration %d", i);

        /*- Allocate stack once or at the beginning of every iteration
         * (if @p alt_release is @c "stack"). */
        if (i == 0 || alt_release == ZFTS_ALT_RELEASE_STACK)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zf_stack_alloc(pco_iut, attr, &stack);
            if (rc < 0)
                TEST_VERDICT("zf_stack_alloc() failed with errno %r",
                             RPC_ERRNO(pco_iut));
        }

        /*- Establish TCP connection once or at the beginning of
         * every iteration (if @p alt_release is @c "stack"). */
        if (i == 0 || alt_release == ZFTS_ALT_RELEASE_STACK)
        {
            tapi_sockaddr_clone(pco_iut, iut_addr, &cur_iut_addr);
            tapi_sockaddr_clone(pco_tst, tst_addr, &cur_tst_addr);

            zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                         attr, stack,
                                         &iut_zftl, &iut_zft,
                                         SA(&cur_iut_addr),
                                         pco_tst, &tst_s,
                                         SA(&cur_tst_addr),
                                         -1, -1);
        }

        /*- Allocate random number of ZF alternatives. */

        n_alts = rand_range(1, alt_count_max);

        for (j = 0; j < n_alts; j++)
        {
            ZFTS_CHECK_RPC(rpc_zf_alternatives_alloc(pco_iut, stack,
                                                     attr, &alts[j]),
                           pco_iut, "zf_alternatives_alloc()");
            alts_send[j] = FALSE;
        }

        /*- Queue data to random number of alternatives (putting
         * several data portions to each alternative). */

        if (alt_action == ZFTS_ALT_ACTION_SEND)
            n_alts_send = rand_range(1, n_alts);
        else
            n_alts_send = rand_range(0, n_alts);

        k = n_alts_send;
        while (k > 0)
        {
            l = rand_range(0, n_alts - 1);
            if (alts_send[l])
                continue;

            for (m = 0; m < ZFTS_IOVCNT; m++)
            {
                  RPC_AWAIT_ERROR(pco_iut);
                  rc = rpc_zft_alternatives_queue(pco_iut, iut_zft,
                                                  alts[l],
                                                  &iovs[l][m],
                                                  1, 0);
                  if (rc < 0 && RPC_ERRNO(pco_iut) == RPC_EBUSY)
                  {
                      rpc_zf_process_events(pco_iut, stack);
                      RPC_AWAIT_ERROR(pco_iut);
                      rc = rpc_zft_alternatives_queue(pco_iut, iut_zft,
                                                      alts[l],
                                                      &iovs[l][m],
                                                      1, 0);
                  }
                  if (rc < 0)
                      TEST_VERDICT("zft_alternatives_queue() "
                                   "failed with errno %r",
                                   RPC_ERRNO(pco_iut));
            }

            alts_send[l] = TRUE;
            k--;
        }

        switch (alt_action)
        {
            case ZFTS_ALT_ACTION_NONE:

                break;

            case ZFTS_ALT_ACTION_SEND:

                /*- If @p alt_action is @c "send", send one of
                 * the alternatives; read and check data on Tester. */

                do {
                    k = rand_range(0, n_alts - 1);
                    if (alts_send[k])
                        break;
                } while (TRUE);

                ZFTS_CHECK_RPC(
                    rpc_zf_alternatives_send(pco_iut, stack, alts[k]),
                    pco_iut, "zf_alternatives_send()");

                rpc_zf_process_events(pco_iut, stack);

                rpc_iov2dbuf(iovs[k], ZFTS_IOVCNT, &send_data);

                te_dbuf_free(&recv_data);
                rc = zfts_sock_wait_data(pco_tst, tst_s, DATA_WAIT_TIMEOUT);
                if (rc <= 0)
                    TEST_VERDICT("Failed to wait for data sent from IUT");

                if (tapi_sock_read_data(pco_tst, tst_s, &recv_data) < 0)
                    TEST_VERDICT("Failed to read data from peer");

                ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, send_data.ptr,
                                         recv_data.len, send_data.len,
                                         " from IUT");

                break;

            case ZFTS_ALT_ACTION_CANCEL:

                /*- If @p alt_action is @c "cancel", cancel all the
                 * alernatives. */

                for (j = 0; j < n_alts; j++)
                {
                    ZFTS_CHECK_RPC(
                        rpc_zf_alternatives_cancel(pco_iut, stack,
                                                   alts[j]),
                        pco_iut, "zf_alternatives_cancel()");

                    rpc_zf_process_events(pco_iut, stack);
                }

                break;
        }

        switch (alt_release)
        {
            case ZFTS_ALT_RELEASE_QUEUE:

                /*- If @p alt_release is @c "queue". call
                 * @b zf_alternatives_release() for all alternatives. */

                for (j = 0; j < n_alts; j++)
                {
                    ZFTS_CHECK_RPC(
                        rpc_zf_alternatives_release(pco_iut, stack,
                                                    alts[j]),
                        pco_iut, "zf_alternatives_release()");

                    rpc_zf_process_events(pco_iut, stack);
                }

                break;

            case ZFTS_ALT_RELEASE_STACK:

                /*- If @p alt_release is @c "stack", free the stack. */

                ZFTS_CHECK_RPC(rpc_zf_stack_free(pco_iut, stack),
                               pco_iut, "zf_stack_free()");
                stack = RPC_NULL;

                rpc_release_rpc_ptr(pco_iut, iut_zft,
                                    RPC_TYPE_NS_ZFT);
                iut_zft = RPC_NULL;

                if (iut_zftl != RPC_NULL)
                {
                    rpc_release_rpc_ptr(pco_iut, iut_zftl,
                                        RPC_TYPE_NS_ZFTL);
                    iut_zftl = RPC_NULL;
                }

                break;
        }
    }

    TEST_SUCCESS;

cleanup:

    for (j = 0; j < alt_count_max; j++)
    {
        rpc_free_iov(iovs[j], ZFTS_IOVCNT);
    }

    if (tst_s >= 0)
        CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
