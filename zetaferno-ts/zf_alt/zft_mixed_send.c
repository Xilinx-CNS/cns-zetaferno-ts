/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 */

/**
 * @page zf_alt-zft_mixed_send Send data mixing usual send and alternatives.
 *
 * @objective Check that @b zft_send() function can be called in different
 *            positions regarding @b zf_alternatives_send() calls on the same
 *            zocket.
 *
 * @param env            Testing environment:
 *                       - @ref arg_types_env_peer2peer
 * @param data_size      Data size:
 *                       - @c 1
 *                       - @c 1400
 *                       - @c 3000
 * @param open_method    How to open connection:
 *                       - @c active
 *                       - @c passive
 * @param where_zft_send Where to call the @b zft_send() function:
 *                       - @c before_alts_queue
 *                       - @c after_alts_queue
 *                       - @c before_alts_send
 * @param do_cancel      Call @b zf_alternatives_cancel after queue a message
 *                       or not
 *
 * @par Scenario:
 *
 * @author Artemii Morozov <Artemii.Morozov@oktetlabs.ru>
 */

#define TE_TEST_NAME "zf_alt/zft_mixed_send"

#include "zf_test.h"
#include "rpc_zf.h"

typedef enum {
    AZS_BEFORE_ALTS_QUEUE = 0, /**< call zft_send() before
                                    zft_alternatives_queue() */
    AZS_AFTER_ALTS_QUEUE,      /**< call zft_send() after
                                    zft_alternatives_queue() but before
                                    zf_alternatives_send() */
    AZS_AFTER_ALTS_SEND,       /**< call zft_send() after
                                    zf_alternatives_send() */
} additional_zft_send;

#define ADDITIONAL_ZFT_SEND_MAPS \
    { "before_alts_queue", AZS_BEFORE_ALTS_QUEUE }, \
    { "after_alts_queue", AZS_AFTER_ALTS_QUEUE },   \
    { "before_alts_send", AZS_AFTER_ALTS_SEND }

#define DO_ZFT_SEND_WRAPPER(_where_zft_send) \
    do {                                                                  \
        if (where_zft_send == _where_zft_send)                            \
        {                                                                 \
            rc = rpc_zft_send(pco_iut, iut_zft, &sndiov, 1, 0);           \
            if (rc != (int)sndiov.iov_len)                                \
                TEST_VERDICT("zft_send() sent the wrong amount of data"); \
            te_dbuf_append(&send_data, sndiov.iov_base, rc);              \
        }                                                                 \
    } while(0);

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p iut_zftl = RPC_NULL;
    rpc_zft_p iut_zft = RPC_NULL;
    int tst_s = -1;

    zfts_conn_open_method open_method;
    additional_zft_send where_zft_send;

    int alt_count_max;
    int n_alts;
    int i;
    int data_size;
    te_bool do_cancel;

    rpc_zf_althandle *alts;

    te_dbuf recv_data = TE_DBUF_INIT(0);
    te_dbuf send_data = TE_DBUF_INIT(0);
    rpc_iovec sndiov;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(data_size);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_ENUM_PARAM(where_zft_send, ADDITIONAL_ZFT_SEND_MAPS);
    TEST_GET_BOOL_PARAM(do_cancel);

    CHECK_RC(tapi_sh_env_get_int(pco_iut, "ZFTS_ALT_COUNT_DEF",
                                 &alt_count_max));

    alts = (rpc_zf_althandle *)tapi_calloc(sizeof(*alts), alt_count_max);

    TEST_STEP("Allocate ZF attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Establish TCP connection to get zft zocket.");
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    TEST_STEP("Allocate random number [1; max] alternatives.");
    n_alts = rand_range(1, alt_count_max);
    for (i = 0; i < n_alts; i++)
    {
        ZFTS_CHECK_RPC(rpc_zf_alternatives_alloc(
                           pco_iut, stack, attr, &alts[i]),
                       pco_iut, "zf_alternatives_alloc()");
    }

    rpc_make_iov(&sndiov, 1, data_size, data_size);

    TEST_STEP("If @p where_zft_send is @c before_alts_queue send @p data_size "
              "bytes using @b zft_send().");
    DO_ZFT_SEND_WRAPPER(AZS_BEFORE_ALTS_QUEUE);
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    TEST_STEP("Queue data to all alternatives.");
    for (i = 0; i < n_alts; i++)
    {
        rpc_zf_process_events(pco_iut, stack);
        ZFTS_CHECK_RPC(rpc_zft_alternatives_queue(
                           pco_iut, iut_zft, alts[i], &sndiov, 1, 0),
                       pco_iut, "zft_alternatives_queue()");
    }

    TEST_STEP("If @p do_cancel is @c TRUE, cancel all alternatives.");
    if (do_cancel)
    {
        for (i = 0; i < n_alts; i++)
        {
            rpc_zf_process_events(pco_iut, stack);
            ZFTS_CHECK_RPC(rpc_zf_alternatives_cancel(pco_iut, stack, alts[i]),
                           pco_iut, "zft_alternatives_cancel()");
        }
    }

    TEST_STEP("If @p where_zft_send is @c after_alts_queue send @p data_size "
              "bytes using @b zft_send().");
    DO_ZFT_SEND_WRAPPER(AZS_AFTER_ALTS_QUEUE);

    if (do_cancel)
    {
        TEST_STEP("Check that if @p do_cancel is @c TRUE the repeated "
                  "@b zft_alternatives_queue() is successful.");
        for (i = 0; i < n_alts; i++)
        {
            rpc_zf_process_events(pco_iut, stack);
            ZFTS_CHECK_RPC(rpc_zft_alternatives_queue(
                               pco_iut, iut_zft, alts[i], &sndiov, 1, 0),
                           pco_iut, "zft_alternatives_queue()");
        }
    }

    TEST_STEP("Send queued in alternatives data. If @p where_zft_send is "
              "@c after_alts_queue and @p do_cancel is @c FALSE check that "
              "@b zf_alternatives_send() returns an error and sets errno "
              "to @c EINVAL.");
    i = rand_range(0, n_alts - 1);

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_alternatives_send(pco_iut, stack, alts[i]);
    if (where_zft_send == AZS_AFTER_ALTS_QUEUE && !do_cancel)
    {
        if (rc == 0)
            TEST_VERDICT("zf_alternatives_send() unexpectedly succeeded");

        CHECK_RPC_ERRNO(pco_iut, RPC_EINVAL,
                        "zf_alternatives_send() returns negative value, but");
    }
    else
    {
        if (rc != 0)
        {
            TEST_VERDICT("zf_alternatives_send() unexpectedly failed with "
                         "errno %r", RPC_ERRNO(pco_iut));
        }
        te_dbuf_append(&send_data, sndiov.iov_base, sndiov.iov_len);
    }

    rpc_zf_process_events(pco_iut, stack);

    TEST_STEP("If @p where_zft_send is @c after_alt_send send @p data_size "
              "bytes using @b zft_send().");
    DO_ZFT_SEND_WRAPPER(AZS_AFTER_ALTS_SEND);

    TEST_STEP("Check that all previously sent data has been received.");
    rpc_read_fd2te_dbuf(pco_tst, tst_s, TAPI_WAIT_NETWORK_DELAY,
                        0, &recv_data);
    ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, send_data.ptr,
                             recv_data.len, send_data.len,
                             " from IUT");

    TEST_SUCCESS;

cleanup:
    for (i = 0; i < n_alts; i++)
        CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alts[i]);

    te_dbuf_free(&recv_data);
    te_dbuf_free(&send_data);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
