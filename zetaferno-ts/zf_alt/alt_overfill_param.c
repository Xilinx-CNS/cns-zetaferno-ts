/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 */

/**
 * @page zf_alt-alt_overfill_param Gradually overfill a few alternative queues.
 *
 * @objective Gradually overfill all available alternatives to
 *            exercise limits.
 *
 * @param data_size         Data size to be queued by one call:
 *                          - @c 1
 *                          - @c 1000
 *                          - @c 4000
 * @param alt_buf_size_min  Use minimum value of alt_buf_size
 *                          (alt_count * 1024) if @c TRUE, otherwise
 *                          use the maximum value.
 * @param open_method       How to open connection:
 *                          - @c active
 *                          - @c passive
 *                          - @c passive_close (close listener after
 *                          passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "zf_alt/alt_overfill_param"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr_storage iut_addr_new;
    struct sockaddr_storage tst_addr_new;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zftl_p  iut_zftl = RPC_NULL;
    rpc_zft_p   iut_zft = RPC_NULL;
    int         tst_s = -1;

    int alt_count;
    int alt_buf_size;
    int alt_count_max;
    int alt_buf_size_max;
    int i;

    rpc_zf_althandle *alts;
    rpc_iovec         iov;
    te_dbuf          *alt_queued;
    te_dbuf           tst_received = TE_DBUF_INIT(0);
    int               send_alt_id;

    int       data_size;
    te_bool   alt_buf_size_min;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(data_size);
    TEST_GET_BOOL_PARAM(alt_buf_size_min);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    rpc_make_iov(&iov, 1, data_size, data_size);

    CHECK_RC(tapi_sh_env_get_int(pco_iut, "ZFTS_ALT_COUNT_DEF",
                                 &alt_count_max));
    CHECK_RC(tapi_sh_env_get_int(pco_iut, "ZFTS_ALT_BUF_SIZE_DEF",
                                 &alt_buf_size_max));

    alts = (rpc_zf_althandle *)tapi_calloc(sizeof(*alts), alt_count_max);
    alt_queued = (te_dbuf *)tapi_calloc(sizeof(*alt_queued), alt_count_max);

    /*- Allocate ZF attributes. */
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- Iterate in the loop alt_count=[1; alt_count_max]: */
    for (alt_count = 1; alt_count < alt_count_max; alt_count++)
    {
        RING("Checking alt_count=%d", alt_count);

        if (alt_buf_size_min)
            alt_buf_size = ZFTS_ALT_BUF_RESERVED * alt_count;
        else
            alt_buf_size = alt_buf_size_max -
                           ZFTS_ALT_BUF_RESERVED * alt_count;

        /*-- Set attribute @c alt_count value according to
         * the loop iteration. */
        rpc_zf_attr_set_int(pco_iut, attr, "alt_count", alt_count);


        /*-- Set attribute @c alt_buf_size value according to
         * @p alt_buf_size_min. */
        rpc_zf_attr_set_int(pco_iut, attr, "alt_buf_size", alt_buf_size);

        /*-- Allocate ZF stack. */
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_stack_alloc(pco_iut, attr, &stack);
        if (rc < 0)
            TEST_VERDICT("zf_stack_alloc() failed with errno %r",
                         RPC_ERRNO(pco_iut));

        CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr, &iut_addr_new));
        CHECK_RC(tapi_sockaddr_clone(pco_tst, tst_addr, &tst_addr_new));

        /*-- Establish TCP connection to get ZFT zocket. */
        zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                     attr, stack,
                                     &iut_zftl, &iut_zft, SA(&iut_addr_new),
                                     pco_tst, &tst_s, SA(&tst_addr_new),
                                     -1, -1);

        /*-- Allocate @b alt_count alternatives. */
        for (i = 0; i < alt_count; i++)
        {
            rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alts[i]);
            te_dbuf_free(&alt_queued[i]);
        }

        /*-- In a loop until queuing fails, fill all alternatives
         * gradually by queueing @p data_size bytes to every alternative. */
        while (TRUE)
        {
            for (i = 0; i < alt_count; i++)
            {
                te_fill_buf(iov.iov_base, iov.iov_len);

                RPC_AWAIT_ERROR(pco_iut);
                rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alts[i],
                                                &iov, 1, 0);
                if (rc < 0)
                {
                    if (RPC_ERRNO(pco_iut) != RPC_ENOBUFS &&
                        RPC_ERRNO(pco_iut) != RPC_EMSGSIZE)
                        TEST_VERDICT("zft_alternatives_queue() failed with "
                                     "unexpected errno %r",
                                     RPC_ERRNO(pco_iut));
                    break;
                }
                te_dbuf_append(&alt_queued[i], iov.iov_base, iov.iov_len);
                rpc_zf_process_events(pco_iut, stack);
            }
            if (rc < 0)
                break;
        }

        for (i = 0; i < alt_count; i++)
        {
            if (alt_queued[i].len == 0)
                TEST_VERDICT("Failed to queue any data on "
                             "one of alternatives");
        }

        /*-- Send queued data from one of alternatives chosen
         * randomly. */
        send_alt_id = rand_range(0, alt_count - 1);
        rpc_zf_alternatives_send(pco_iut, stack, alts[send_alt_id]);
        rpc_zf_process_events(pco_iut, stack);

        /*-- Cancel other alternatives. */
        for (i = 0; i < alt_count; i++)
        {
            if (i != send_alt_id)
                rpc_zf_alternatives_cancel(pco_iut, stack, alts[i]);
        }
        rpc_zf_process_events(pco_iut, stack);

        /*-- Read and check data on Tester. */
        te_dbuf_free(&tst_received);
        zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s,
                               &tst_received);
        ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr,
                                 alt_queued[send_alt_id].ptr,
                                 tst_received.len,
                                 alt_queued[send_alt_id].len,
                                 " from IUT");

        /*-- Queue and send some data from a randomly chosen alternative. */

        te_fill_buf(iov.iov_base, iov.iov_len);
        send_alt_id = rand_range(0, alt_count - 1);
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alts[send_alt_id],
                                        &iov, 1, 0);
        if (rc < 0)
            TEST_VERDICT("Failed to queue data to an alternative "
                         "after processing previously queued data");

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_alternatives_send(pco_iut, stack, alts[send_alt_id]);
        if (rc < 0)
            TEST_VERDICT("Failed to send data from an alternative "
                         "after processing previously queued data");

        rpc_zf_process_events(pco_iut, stack);

        /*-- Receive and check data on Tester. */
        te_dbuf_free(&tst_received);
        zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s,
                               &tst_received);
        ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr,
                                 iov.iov_base,
                                 tst_received.len,
                                 iov.iov_len,
                                 " finally from IUT");

        /*-- Release alternatives. */
        for (i = 0; i < alt_count; i++)
        {
            rpc_zf_alternatives_release(pco_iut, stack, alts[i]);
        }

        /*-- Release ZFT zocket. */
        ZFTS_FREE(pco_iut, zft, iut_zft);
        ZFTS_FREE(pco_iut, zftl, iut_zftl);

        /*-- Close Tester socket. */
        RPC_CLOSE(pco_tst, tst_s);

        /*-- Release stack. */
        rpc_zf_stack_free(pco_iut, stack);
        stack = RPC_NULL;
    }

    TEST_SUCCESS;

cleanup:

    rpc_release_iov(&iov, 1);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
