/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-alt_reuse Reuse an alternative with a different zocket.
 *
 * @objective Use an alternative by two zockets.
 *
 * @param first_send      Send alternative for the first zocket if @c TRUE.
 * @param first_cancel    Cancel alternative after queueing data
 *                        for the first zocket if @c TRUE, @c FALSE is not
 *                        applicable if @p send is @c FALSE.
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

#define TE_TEST_NAME "zf_alt/alt_reuse"

#include "zf_test.h"
#include "rpc_zf.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr_storage iut_addr2;
    struct sockaddr_storage tst_addr2;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p iut_zftl1 = RPC_NULL;
    rpc_zft_p iut_zft1 = RPC_NULL;
    int       tst_s1 = -1;
    rpc_zft_p iut_zftl2 = RPC_NULL;
    rpc_zft_p iut_zft2 = RPC_NULL;
    int       tst_s2 = -1;

    rpc_zf_althandle  alt;

    rpc_iovec iov1;
    rpc_iovec iov2;

    te_dbuf   iut_sent1 = TE_DBUF_INIT(0);
    te_dbuf   iut_sent2 = TE_DBUF_INIT(0);
    te_dbuf   tst_received1 = TE_DBUF_INIT(0);
    te_dbuf   tst_received2 = TE_DBUF_INIT(0);

    te_bool first_send;
    te_bool first_cancel;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(first_send);
    TEST_GET_BOOL_PARAM(first_cancel);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    rpc_make_iov(&iov1, 1, 1, ZFTS_TCP_DATA_MAX);
    rpc_make_iov(&iov2, 1, 1, ZFTS_TCP_DATA_MAX);

    CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr, &iut_addr2));
    CHECK_RC(tapi_sockaddr_clone(pco_tst, tst_addr, &tst_addr2));

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish two TCP connections to get zft zockets. */
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl1, &iut_zft1, iut_addr,
                                 pco_tst, &tst_s1, tst_addr,
                                 -1, -1);
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl2, &iut_zft2, SA(&iut_addr2),
                                 pco_tst, &tst_s2, SA(&tst_addr2),
                                 -1, -1);

    /*- Allocate one alternative. */
    rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alt);

    /*- Queue data to the alternative on the first zocket. */
    rpc_zft_alternatives_queue(pco_iut, iut_zft1, alt,
                               &iov1, 1, 0);

    /*- If @p first_send is @c TRUE - send the data. */
    if (first_send)
    {
        rpc_zf_alternatives_send(pco_iut, stack, alt);
        rpc_zf_process_events(pco_iut, stack);
        te_dbuf_append(&iut_sent1, iov1.iov_base, iov1.iov_len);
    }

    /*- If @p first_cancel is @c TRUE - cancel the alternative. */
    if (first_cancel)
        ZFTS_CHECK_RPC(rpc_zf_alternatives_cancel(pco_iut, stack, alt),
                       pco_iut, "zf_alternative_cancel()");

    rpc_zf_process_events(pco_iut, stack);

    /*- Queue data to the alternative on the second zocket. */
    ZFTS_CHECK_RPC(rpc_zft_alternatives_queue(pco_iut, iut_zft2, alt,
                                              &iov2, 1, 0),
                   pco_iut,
                   "zft_alternatives_queue() for the second zocket");

    /*- Send the data. */
    ZFTS_CHECK_RPC(rpc_zf_alternatives_send(pco_iut, stack, alt),
                   pco_iut,
                   "zf_alternatives_send() for the second zocket");
    rpc_zf_process_events(pco_iut, stack);
    te_dbuf_append(&iut_sent2, iov2.iov_base, iov2.iov_len);

    /*- Queue data to the alternative on the first zocket. */
    ZFTS_CHECK_RPC(rpc_zft_alternatives_queue(pco_iut, iut_zft1, alt,
                                              &iov1, 1, 0),
                   pco_iut,
                   "final zft_alternatives_queue() for the first zocket");

    /*- Send the data. */
    ZFTS_CHECK_RPC(rpc_zf_alternatives_send(pco_iut, stack, alt),
                   pco_iut,
                   "final zf_alternatives_send() for the first zocket");
    rpc_zf_process_events(pco_iut, stack);
    te_dbuf_append(&iut_sent1, iov1.iov_base, iov1.iov_len);

    /*- Read and check all data on Tester. */

    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s1,
                           &tst_received1);
    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s2,
                           &tst_received2);

    ZFTS_CHECK_RECEIVED_DATA(tst_received1.ptr, iut_sent1.ptr,
                             tst_received1.len, iut_sent1.len,
                             " from IUT through the first connection");
    ZFTS_CHECK_RECEIVED_DATA(tst_received2.ptr, iut_sent2.ptr,
                             tst_received2.len, iut_sent2.len,
                             " from IUT through the second connection");

    TEST_SUCCESS;

cleanup:

    te_dbuf_free(&iut_sent1);
    te_dbuf_free(&iut_sent2);
    te_dbuf_free(&tst_received1);
    te_dbuf_free(&tst_received2);
    rpc_release_iov(&iov1, 1);
    rpc_release_iov(&iov2, 1);

    CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alt);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft1);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s2);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl2);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft2);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
