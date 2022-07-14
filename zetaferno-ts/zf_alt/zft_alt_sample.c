/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * ZF Alternative Queues tests
 *
 * $Id$
 */

/**
 * @page zf_alt-zft_alt_sample ZF alternatives API sanity test.
 *
 * @objective Send some data over TCP using alternatives API.
 *
 * @param iovcnt        IOVs number.
 * @param data_size     Size of data to send.
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

#define TE_TEST_NAME "zf_alt/zft_alt_sample"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_sockets.h"

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

    int iovcnt;
    int data_size;

    int n_alts;
    int alt_count_max;
    int i;

    rpc_zf_althandle *alts;

    te_dbuf send_data = TE_DBUF_INIT(0);
    te_dbuf recv_data = TE_DBUF_INIT(0);

    rpc_iovec *iov = NULL;

    ssize_t init_space = 0;
    ssize_t cur_space = 0;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(iovcnt);
    TEST_GET_INT_PARAM(data_size);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    CHECK_RC(tapi_sh_env_get_int(pco_iut, "ZFTS_ALT_COUNT_DEF",
                                 &alt_count_max));

    alts = (rpc_zf_althandle *)tapi_calloc(sizeof(*alts), alt_count_max);

    rpc_alloc_iov(&iov, iovcnt, data_size, data_size);
    rpc_iov2dbuf(iov, iovcnt, &send_data);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection to get zft zocket. */
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- Allocate random [1; max] alternatives number. */
    n_alts = rand_range(1, alt_count_max);
    for (i = 0; i < n_alts; i++)
    {
        ZFTS_CHECK_RPC(rpc_zf_alternatives_alloc(pco_iut, stack,
                                            attr, &alts[i]),
                       pco_iut, "zf_alternatives_alloc()");
    }

    /*- Queue a packet for sending:
     *  - use random alternative;
     *  - use @p iovcnt and @p data_size as the packet parameters. */

    i = rand_range(0, n_alts - 1);
    RING("Using alternative queue %d", i);

    /*- Get initial value returned by @b zf_alternatives_free_space(). */
    init_space = rpc_zf_alternatives_free_space(pco_iut, stack, alts[i]);

    ZFTS_CHECK_RPC(rpc_zft_alternatives_queue(pco_iut, iut_zft, alts[i],
                                              iov, iovcnt, 0),
                   pco_iut, "First call of zft_alternatives_queue()");

    /*- Cancel the alternative using @b zf_alternatives_cancel(). */
    ZFTS_CHECK_RPC(rpc_zf_alternatives_cancel(pco_iut, stack, alts[i]),
                   pco_iut, "zf_alternatives_cancel()");

    /*- Process ZF events. */
    ZFTS_CHECK_RPC(rpc_zf_process_events(pco_iut, stack),
                   pco_iut, "First rpc_zf_process_events() call");

    /*- Check that after canceling alternative
     * @b zf_alternatives_free_space() returns the same value as
     * before queueing data. */
    cur_space = rpc_zf_alternatives_free_space(pco_iut, stack, alts[i]);
    if (cur_space > init_space ||
        init_space - cur_space > ZFTS_FREE_ALT_SPACE_MAX_DIFF)
        ERROR_VERDICT("After cancelling queued data "
                      "zf_alternatives_free_space() returned %s "
                      "than before queueing",
                      (cur_space > init_space ? "more" : "less"));

    /*- Queue another packet for sending. */
    ZFTS_CHECK_RPC(rpc_zft_alternatives_queue(pco_iut, iut_zft, alts[i],
                                         iov, iovcnt, 0),
                   pco_iut, "Second call of zf_alternatives_queue()");

    /*- Send the alternative using @b zf_alternatives_send(). */
    ZFTS_CHECK_RPC(rpc_zf_alternatives_send(pco_iut, stack, alts[i]),
                   pco_iut, "zf_alternatives_send()");

    /*- Release the alternatives queues with
     * @b zf_alternatives_release(). */
    for (i = 0; i < n_alts; i++)
    {
        ZFTS_CHECK_RPC(rpc_zf_alternatives_release(pco_iut, stack, alts[i]),
                       pco_iut, "zf_alternatives_release()");
    }

    /*- Process ZF events. */
    ZFTS_CHECK_RPC(rpc_zf_process_events(pco_iut, stack),
                   pco_iut, "Second rpc_zf_process_events() call");

    /*- Read and check data on tester. */

    if (tapi_sock_read_data(pco_tst, tst_s, &recv_data) < 0)
        TEST_VERDICT("Failed to read data from peer");

    ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, send_data.ptr,
                             recv_data.len, send_data.len,
                             " from IUT");

    TEST_SUCCESS;

cleanup:

    rpc_free_iov(iov, iovcnt);

    te_dbuf_free(&send_data);
    te_dbuf_free(&recv_data);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
