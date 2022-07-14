/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-listen_backlog  Process a few connections arrived before @c zf_reactor_perform()
 *
 * @objective  Test that a few unaccepted (queued) connections can be
 *             accepted by a listener zocket. I.e. check that a few SYN
 *             requests (from different sources), which come between
 *             @c zf_reactor_perform() calls, are processed correctly.
 *
 * @param pco_iut       PCO on IUT.
 * @param pco_tst       PCO on TST.
 * @param iut_addr      IUT address.
 * @param tst_addr      Tester address.
 * @param clients_num   TCP clients number:
 *      - 3;
 * @param accept_one    Accept one of connections firstly if @c TRUE, send a
 *                      data packet together with connections requests.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @note In any case @a zf_reactor_perform should be called in @b SYN_RTO
 *       timeout, otherwise connection will be dropped.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "tcp/listen_backlog"

#include "zf_test.h"
#include "rpc_zf.h"

/** Maximum number of connections we can try to establish */
#define MAX_CONNS   1000

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zftl_p     iut_tl = RPC_NULL;
    rpc_zft_p      iut_ts = RPC_NULL;
    rpc_zft_p      iut_ts_aux = RPC_NULL;
    int            tst_s = -1;

    int                 conns_num = 0;
    int                 tst_socks[MAX_CONNS] = { -1, };
    struct sockaddr     tst_sock_names[MAX_CONNS];
    struct sockaddr     iut_zock_raddr;
    socklen_t           namelen;

    rpc_zft_p      iut_zfts[MAX_CONNS] = { RPC_NULL, };
    int            i;
    int            j;

    int       clients_num;
    te_bool   accept_one;

    char send_data[ZFTS_TCP_DATA_MAX];

    te_dbuf        recv_dbuf = TE_DBUF_INIT(0);

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);

    TEST_GET_INT_PARAM(clients_num);
    TEST_GET_BOOL_PARAM(accept_one);
    TEST_GET_SET_DEF_RECV_FUNC(recv_func);

    te_fill_buf(send_data, ZFTS_TCP_DATA_MAX);

    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Create and bind listener zocket. */
    rpc_zftl_listen(pco_iut, stack, iut_addr, attr, &iut_tl);

    /*- If @p accept_one is @c TRUE:
     *  - connect a one socket from tester to IUT;
     *  - call @p ZFTS_WAIT_NETWORK();
     *  - accept the connection on IUT zocket. */

    if (accept_one)
    {
        tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                           RPC_SOCK_STREAM, RPC_PROTO_DEF);
        rpc_bind(pco_tst, tst_s, tst_addr);

        pco_tst->op = RCF_RPC_CALL;
        rpc_connect(pco_tst, tst_s, iut_addr);

        ZFTS_WAIT_NETWORK(pco_iut, stack);
        rpc_zftl_accept(pco_iut, iut_tl, &iut_ts);

        pco_tst->op = RCF_RPC_WAIT;
        rpc_connect(pco_tst, tst_s, iut_addr);
    }

    /*- Initiate a number @p clients_num connection requests from tester
     * sockets. */

    for (i = 0; i < clients_num; i++)
    {
        tst_socks[i] = rpc_socket(pco_tst,
                                  rpc_socket_domain_by_addr(tst_addr),
                                  RPC_SOCK_STREAM, RPC_PROTO_DEF);
        conns_num++;

        rpc_fcntl(pco_tst, tst_socks[i], RPC_F_SETFL, RPC_O_NONBLOCK);

        RPC_AWAIT_IUT_ERROR(pco_tst);
        rc = rpc_connect(pco_tst, tst_socks[i], iut_addr);
        if (rc >= 0)
            TEST_VERDICT("connect() successed unexpectedly "
                         "for %d connection", i);
        else if (rc < 0 && RPC_ERRNO(pco_tst) != RPC_EINPROGRESS)
            TEST_VERDICT("connect() failed with unexpected errno %r "
                         "for %d connection", RPC_ERRNO(pco_tst), i);

        namelen = sizeof(tst_sock_names[i]);
        rpc_getsockname(pco_tst, tst_socks[i],
                        &tst_sock_names[i], &namelen);
    }

    /*- If @p accept_one is @c TRUE:
     *  - send a data packet from tester to the accepted zocket. */
    if (accept_one)
        rpc_send(pco_tst, tst_s, send_data, ZFTS_TCP_DATA_MAX, 0);

    /*- Call @p ZFTS_WAIT_NETWORK(). */
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Check that all connections can be accepted. */
    for (i = 0; i < conns_num; i++)
    {
        /*
         * Here rpc_zftl_accept() accepts connections in random order.
         */
        RPC_AWAIT_IUT_ERROR(pco_iut);
        rc = rpc_zftl_accept(pco_iut, iut_tl, &iut_ts_aux);
        if (rc < 0)
            TEST_VERDICT("Failed to accept one of the connections");

        rpc_zft_getname(pco_iut, iut_ts_aux, NULL, SIN(&iut_zock_raddr));
        for (j = 0; j < conns_num; j++)
        {
            if (tapi_sockaddr_cmp(&tst_sock_names[j],
                                  &iut_zock_raddr) == 0)
            {
                if (iut_zfts[j] != RPC_NULL)
                    TEST_FAIL("Two zockets with the same remote address "
                              "were accepted");

                RING("Connection %d was accepted by zftl_accept() call %d",
                     j, i);
                iut_zfts[j] = iut_ts_aux;
                iut_ts_aux = RPC_NULL;
                break;
            }
        }

        if (j == conns_num)
            TEST_FAIL("Connection with unknown remote address "
                      "was accepted");
    }

    /*- If @p accept_one is @c TRUE:
     *  - read the data packet on the first accepted zocket. */
    if (accept_one)
    {
        zfts_zft_read_data(pco_iut, iut_ts, &recv_dbuf, NULL);
        ZFTS_CHECK_RECEIVED_DATA(recv_dbuf.ptr, send_data,
                                 recv_dbuf.len, ZFTS_TCP_DATA_MAX,
                                 " from Tester");
    }

    /*- Send some data by each connection. */
    for (i = 0; i < conns_num; i++)
    {
        RPC_AWAIT_IUT_ERROR(pco_tst);
        rc = rpc_connect(pco_tst, tst_socks[i], iut_addr);
        if (rc < 0 && RPC_ERRNO(pco_tst) != RPC_EISCONN)
            TEST_VERDICT("connect() failed with errno %r "
                         "for accepted connection %d",
                         RPC_ERRNO(pco_tst), i);

        rpc_fcntl(pco_tst, tst_socks[i], RPC_F_SETFL, 0);

        zfts_zft_check_connection(pco_iut, stack, iut_zfts[i],
                                  pco_tst, tst_socks[i]);
    }

    TEST_SUCCESS;

cleanup:

    if (iut_ts != RPC_NULL)
    {
        CLEANUP_RPC_ZFT_SHUTDOWN_TX(pco_iut, iut_ts);
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_ts);
    }
    if (iut_ts_aux != RPC_NULL)
    {
        CLEANUP_RPC_ZFT_SHUTDOWN_TX(pco_iut, iut_ts_aux);
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_ts_aux);
    }

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    for (i = 0; i < conns_num; i++)
    {
        if (iut_zfts[i] != RPC_NULL)
        {
            CLEANUP_RPC_ZFT_SHUTDOWN_TX(pco_iut, iut_zfts[i]);
            CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zfts[i]);
        }

        CLEANUP_RPC_CLOSE(pco_tst, tst_socks[i]);
    }

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_tl);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);
    te_dbuf_free(&recv_dbuf);

    TEST_END;
}
