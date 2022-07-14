/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-alt_overfill Overfill an alternative queue.
 *
 * @objective Put data to an alternative queue until fail.
 *
 * @param data_size     Data size to be queued by one call:
 *                      - @c 1
 *                      - @c 1000
 *                      - @c 4000
 * @param open_method   How to open connection:
 *                      - @c active
 *                      - @c passive
 *                      - @c passive_close (close listener after
 *                        passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "zf_alt/alt_overfill"

#include "zf_test.h"
#include "rpc_zf.h"

/*
 * How much data (in bytes) should be possible to queue
 * to ZF alternative.
 */
#define QUEUED_DATA_SIZE_LIMIT   10000

/*
 * How much buffers should be possible to queue to ZF alternative,
 * if @c QUEUED_DATA_SIZE_LIMIT is not exceeded.
 */
#define QUEUED_BUFS_NUM_LIMIT    64

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zftl_p  iut_zftl = RPC_NULL;
    rpc_zft_p   iut_zft = RPC_NULL;
    int         tst_s = -1;

    rpc_zf_althandle  alt;

    te_dbuf   iut_sent = TE_DBUF_INIT(0);
    te_dbuf   tst_received = TE_DBUF_INIT(0);

    rpc_iovec iov;
    int       n_queued_bufs;
    int       data_size;

    ssize_t   init_space = -1;
    ssize_t   cur_space = -1;
    ssize_t   prev_space = -1;
    te_bool   no_decr_space = FALSE;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(data_size);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    rpc_make_iov(&iov, 1, data_size, data_size);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection to get zft zocket. */
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- Allocate one alternative. */
    rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alt);

    /*- Call @b zft_alternatives_queue() until it fails.
     * Check that after each successful call
     * @b zf_alternatives_free_space() returns smaller value. */

    init_space = rpc_zf_alternatives_free_space(pco_iut, stack, alt);

    n_queued_bufs = 0;
    do {
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alt,
                                        &iov, 1, 0);
        if (rc < 0)
        {
            if (RPC_ERRNO(pco_iut) != RPC_ENOBUFS &&
                RPC_ERRNO(pco_iut) != RPC_EMSGSIZE)
                ERROR_VERDICT("zft_alternatives_queue() failed with "
                              "unexpected errno %r", RPC_ERRNO(pco_iut));

            break;
        }

        cur_space = rpc_zf_alternatives_free_space(pco_iut, stack, alt);
        if (prev_space >= 0 && cur_space >= prev_space)
        {
            if (!no_decr_space)
            {
                ERROR_VERDICT("zf_alternatives_free_space() did not return "
                              "decreased value after queueing more data");
                no_decr_space = TRUE;
            }
        }
        prev_space = cur_space;

        te_dbuf_append(&iut_sent, iov.iov_base, iov.iov_len);
        n_queued_bufs++;
    } while (TRUE);

    /*- Check that either at least @c QUEUE_DATA_SIZE_LIMIT bytes were
     * queued, or at least @c QUEUED_BUFS_NUM_LIMIT buffers were
     * queued. */

    if (n_queued_bufs < QUEUED_BUFS_NUM_LIMIT &&
        iut_sent.len < QUEUED_DATA_SIZE_LIMIT)
        ERROR_VERDICT("Too little data and too few buffers were "
                      "queued to ZF alternative before "
                      "zft_alternatives_queue() failed");

    /*- Send queued data. */
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_alternatives_send(pco_iut, stack, alt);
    if (rc < 0)
        TEST_VERDICT("zf_alternatives_send() unexpectedly "
                     "failed with errno %r", RPC_ERRNO(pco_iut));

    rpc_zf_process_events(pco_iut, stack);

    /*- Read and check data on Tester. */
    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s,
                           &tst_received);
    ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr, iut_sent.ptr,
                             tst_received.len, iut_sent.len,
                             " from IUT after overfilling queue");

    /*- Process events for a while to make sure that ACKs from
     * Tester were processed, so that sending all data from the alternative
     * queue is completed. */
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Check that now @b zf_alternatives_free_space() returns
     * the same value as it did before filling the queue. */
    cur_space = rpc_zf_alternatives_free_space(pco_iut, stack, alt);
    if (cur_space > init_space ||
        init_space - cur_space > ZFTS_FREE_ALT_SPACE_MAX_DIFF)
    {
        ERROR_VERDICT("After sending all queued data "
                      "zf_alternatives_free_space() returned %s "
                      "than initial value",
                      (cur_space > init_space ? "more" : "less"));
    }

    /*- Queue and send some data. */

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alt,
                                    &iov, 1, 0);
    if (rc < 0)
        TEST_VERDICT("Final zft_alternatives_queue() unexpectedly "
                     "failed with errno %r", RPC_ERRNO(pco_iut));

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_alternatives_send(pco_iut, stack, alt);
    if (rc < 0)
        TEST_VERDICT("Final zf_alternatives_send() unexpectedly "
                     "failed with errno %r", RPC_ERRNO(pco_iut));

    rpc_zf_process_events(pco_iut, stack);

    /*- Read and check data on Tester. */
    te_dbuf_free(&tst_received);
    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s,
                           &tst_received);
    ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr, iov.iov_base,
                             tst_received.len, iov.iov_len,
                             " from IUT after sending more data");

    TEST_SUCCESS;

cleanup:

    te_dbuf_free(&iut_sent);
    te_dbuf_free(&tst_received);
    rpc_release_iov(&iov, 1);

    CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alt);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
