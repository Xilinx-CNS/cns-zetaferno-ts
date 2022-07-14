/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-zft_alt_send Send data using TCP alternatives.
 *
 * @objective Queue data to a few alternatives and send it in the loop.
 *
 * @param iterations_num  The main loop iterations number:
 *                        - @c 1000
 * @param iovcnt          IOV vectors number:
 *                        - @c 1
 * @param data_size       Data buffer size:
 *                        - @c 1
 *                        - @c 1400
 *                        - @c 4000
 * @param n_alts          Alternatives number:
 *                        - @c 2
 *                        - @c -1 (max)
 * @param use_one         Queue data to only one alternative if @c TRUE,
 *                        else fill all alternatives.
 * @param cancel          Cancel one of alternatives if @c TRUE.
 * @param open_method     How to open connection:
 *                        - @c active
 *                        - @c passive
 *                        - @c passive_close (close listener after
 *                          passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "zf_alt/zft_alt_send"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_sockets.h"

/**< How long to wait for a sent packet on a peer (milliseconds). */
#define DATA_WAIT_TIMEOUT 500

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p iut_zftl = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    int       tst_s = -1;

    int       iterations_num;
    int       iovcnt;
    int       data_size;
    int       alt_count_max;
    int       n_alts;
    int       mss;
    te_bool   use_one;
    te_bool   cancel;
    te_bool   reduce_to_mss = FALSE;

    te_dbuf send_data = TE_DBUF_INIT(0);
    te_dbuf recv_data = TE_DBUF_INIT(0);

    int i;
    int iter;
    int send_alt_idx;
    int cancel_alt_idx;

    rpc_zf_althandle *alts;
    te_bool          *alt_filled;
    rpc_iovec       **iovs;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(iterations_num);
    TEST_GET_INT_PARAM(iovcnt);
    TEST_GET_INT_PARAM(data_size);
    TEST_GET_INT_PARAM(n_alts);
    TEST_GET_BOOL_PARAM(use_one);
    TEST_GET_BOOL_PARAM(cancel);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    CHECK_RC(tapi_sh_env_get_int(pco_iut, "ZFTS_ALT_COUNT_DEF",
                                 &alt_count_max));

    alts = (rpc_zf_althandle *)tapi_calloc(sizeof(*alts), alt_count_max);
    alt_filled = (te_bool *)tapi_calloc(sizeof(*alt_filled), alt_count_max);
    iovs = (rpc_iovec **)tapi_calloc(sizeof(*iovs), alt_count_max);

    if (n_alts < 0)
        n_alts = alt_count_max;

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection to get zft zocket. */
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    mss = MIN(rpc_zft_get_mss(pco_iut, iut_zft), data_size);

    /*- Allocate @p n_alts alternatives. */
    for (i = 0; i < n_alts; i++)
    {
        rpc_zf_alternatives_alloc(pco_iut, stack,
                                  attr, &alts[i]);
        alt_filled[i] = FALSE;

        rpc_alloc_iov(&iovs[i], iovcnt, data_size, data_size);
    }

    /*- In the loop @p iterations_num times: */
    for (iter = 0; iter < iterations_num; iter++)
    {
        RING("Iteration %d", iter + 1);

        send_alt_idx = rand_range(0, n_alts - 1);

        /*- Queue packets for sending:
         *  - use @p iovcnt and @p data_size as the packet paramters;
         *  - if @p use_one is @c TRUE, fill only one alternative;
         *    else queue data to all alternatives. */

        if (use_one)
        {
            if (!alt_filled[send_alt_idx])
            {
                if (reduce_to_mss)
                    iovs[send_alt_idx]->iov_len = mss;

                RPC_AWAIT_ERROR(pco_iut);
                rc = rpc_zft_alternatives_queue(pco_iut, iut_zft,
                                                alts[send_alt_idx],
                                                iovs[send_alt_idx],
                                                iovcnt, 0);
                if (rc != 0)
                {
                    if (RPC_ERRNO(pco_iut) != RPC_EMSGSIZE)
                    {
                        TEST_VERDICT("zft_alternatives_queue() failed"
                                     " with errno %s",
                                     errno_rpc2str(RPC_ERRNO(pco_iut)));
                    }
                    else
                    {
                        if (reduce_to_mss)
                        {
                            TEST_VERDICT("Failed to increase congestion "
                                         "window");
                        }
                        else
                        {
                            reduce_to_mss = TRUE;
                            continue;
                        }
                    }
                }
                else
                {
                    alt_filled[send_alt_idx] = TRUE;
                }
            }
        }
        else
        {
            for (i = 0; i < n_alts; i++)
            {
                if (!alt_filled[i])
                {
                    if (reduce_to_mss)
                        iovs[i]->iov_len = mss;

                    RPC_AWAIT_ERROR(pco_iut);
                    rc = rpc_zft_alternatives_queue(pco_iut, iut_zft,
                                                    alts[i],
                                                    iovs[i], iovcnt, 0);
                    if (rc != 0)
                    {
                        if (RPC_ERRNO(pco_iut) != RPC_EMSGSIZE)
                        {
                            TEST_VERDICT("zft_alternatives_queue() failed"
                                         " with errno %s",
                                         errno_rpc2str(RPC_ERRNO(pco_iut)));
                        }
                        else
                        {
                            if (reduce_to_mss)
                            {
                                TEST_VERDICT("Failed to increase congestion "
                                             "window");
                            }
                            else
                            {
                                reduce_to_mss = TRUE;
                                i--; /* Rerun the current iteration */
                                continue;
                            }
                        }
                    }
                    else
                    {
                        alt_filled[i] = TRUE;
                    }
                }
            }
        }

        /*- If @p cancel is @c TRUE, cancel random alternative. */
        if (cancel)
        {
            do {
                cancel_alt_idx = rand_range(0, n_alts - 1);
            } while (cancel_alt_idx == send_alt_idx);

            rpc_zf_alternatives_cancel(pco_iut, stack,
                                       alts[cancel_alt_idx]);
            alt_filled[cancel_alt_idx] = FALSE;

            rpc_zf_process_events(pco_iut, stack);
        }

        /*- Send a random alternative. */
        rpc_zf_alternatives_send(pco_iut, stack, alts[send_alt_idx]);
        alt_filled[send_alt_idx] = FALSE;

        rpc_zf_process_events(pco_iut, stack);

        /*- Read and check data on tester. */

        rpc_iov2dbuf(iovs[send_alt_idx], iovcnt, &send_data);

        rc = zfts_sock_wait_data(pco_tst, tst_s, DATA_WAIT_TIMEOUT);
        if (rc <= 0)
            TEST_VERDICT("Failed to wait for data sent from IUT");

        te_dbuf_free(&recv_data);
        if (tapi_sock_read_data(pco_tst, tst_s, &recv_data) < 0)
            TEST_VERDICT("Failed to read data from peer");

        ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, send_data.ptr,
                                 recv_data.len, send_data.len,
                                 " from IUT");

        /*- Cancel all alternatives which were not sent or
         * canceled previously. */
        for (i = 0; i < n_alts; i++)
        {
            if (alt_filled[i])
            {
                rpc_zf_alternatives_cancel(pco_iut, stack, alts[i]);
                rpc_zf_process_events(pco_iut, stack);
                alt_filled[i] = FALSE;
            }
        }

        if (reduce_to_mss)
        {
            for (i = 0; i < n_alts; i++)
                iovs[i]->iov_len = data_size;

            reduce_to_mss = FALSE;
        }
    }

    TEST_SUCCESS;

cleanup:

    for (i = 0; i < n_alts; i++)
    {
        CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alts[i]);

        rpc_free_iov(iovs[i], iovcnt);
    }

    te_dbuf_free(&send_data);
    te_dbuf_free(&recv_data);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
