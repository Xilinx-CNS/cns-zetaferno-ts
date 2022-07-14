/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-no_extra_data No extra data is sent after alternative cancelling.
 *
 * @objective Check that no extra packets is sent due to alternative
 *            cancelling or events processing.
 *
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

#define TE_TEST_NAME "zf_alt/no_extra_data"

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

    rpc_zf_althandle *alts;
    int               n_alts;
    int               alt_count_max;
    int               i;
    rpc_iovec         iov;

    te_dbuf recv_data = TE_DBUF_INIT(0);

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    CHECK_RC(tapi_sh_env_get_int(pco_iut, "ZFTS_ALT_COUNT_DEF",
                                 &alt_count_max));

    alts = (rpc_zf_althandle *)tapi_calloc(sizeof(*alts), alt_count_max);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection to get zft zocket. */
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- Allocate random number of alternatives [1; max]. */
    n_alts = rand_range(1, alt_count_max);
    for (i = 0; i < n_alts; i++)
    {
        rpc_zf_alternatives_alloc(pco_iut, stack,
                                  attr, &alts[i]);
    }

    /*- Queue data to all alternatives. */
    for (i = 0; i < n_alts; i++)
    {
        rpc_make_iov(&iov, 1, 1, ZFTS_TCP_DATA_MAX);

        rpc_zft_alternatives_queue(pco_iut, iut_zft,
                                   alts[i],
                                   &iov, 1, 0),

        rpc_release_iov(&iov, 1);
    }

    /*- Process events. */
    rpc_zf_process_events(pco_iut, stack);

    /*- Cancel all alternatives. */
    for (i = 0; i < n_alts; i++)
    {
        rpc_zf_alternatives_cancel(pco_iut, stack, alts[i]);
    }

    /*- Process events. */
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Check that no data is received by Tester. */
    CHECK_RC(tapi_sock_read_data(pco_tst, tst_s, &recv_data));
    if (recv_data.len > 0)
        TEST_VERDICT("Some data was received on Tester after queueing and "
                     "cancelling data on ZF alternatives");

    TEST_SUCCESS;

cleanup:

    for (i = 0; i < n_alts; i++)
    {
        CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alts[i]);
    }

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
