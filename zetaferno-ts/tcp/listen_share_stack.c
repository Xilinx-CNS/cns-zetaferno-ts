/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-listen_share_stack ZF stack sharing by two listener zockets
 *
 * @objective  Accept maximum number of connections by two listener zockets.
 *             Connections should be successively accepted until file
 *             descriptors limit is exceed. Check that exceeding of file
 *             descriptors limit is properly handled by both listeners.
 *
 * @param pco_iut         PCO on IUT.
 * @param pco_tst         PCO on TST.
 * @param iut_addr1       IUT address.
 * @param iut_addr2       IUT address.
 * @param same_addr       Use the same address for the second listener
 *                        zocket if @c TRUE.
 * @param same_port       Use the same port for the second listener
 *                        zocket if @c TRUE.
 * @param overflow_limit  if @c TRUE, try to establish more connections
 *                        than allowed.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "tcp/listen_share_stack"

#include "zf_test.h"
#include "rpc_zf.h"

/** Maximum number of connections we can try to establish */
#define MAX_CONNS   1000

/** The first IUT IP address */
static const struct sockaddr   *iut_addr1 = NULL;
/** The second IUT IP address */
static const struct sockaddr   *iut_addr2 = NULL;

/** IUT RPC server pointer */
static rcf_rpc_server          *pco_iut = NULL;
/** Tester RPC server pointer */
static rcf_rpc_server          *pco_tst = NULL;

/** Zetaferno attributes */
static rpc_zf_attr_p  attr = RPC_NULL;
/** Zetaferno stack */
static rpc_zf_stack_p stack = RPC_NULL;

/** Address of the first listening zocket */
static struct sockaddr_storage    iut_listen_addr1;
/** Address of the second listening zocket */
static struct sockaddr_storage    iut_listen_addr2;

/** Listening zockets */
static rpc_zftl_p     iut_tl_zocks[2];

/** Array of TCP connection */
static zfts_tcp_conn   conns[MAX_CONNS];
/** Number of connections in the array */
static int             conns_num = 0;

/** Numbers of connections established for each listening zocket */
static int n_iut_conns[2] = { 0, };

/**
 * Get string representation of TCP connection group ID.
 *
 * @param group_id    Group ID
 *
 * @return "first" or "second"
 */
static char *
group2str(int group_id)
{
    if (group_id == 0)
        return "first";
    else
        return "second";
}

/**
 * Establish TCP connection.
 *
 * @param group_id      if @c 0, establish connection via the first
 *                      listening zocket; if @c 1, use the second
 *                      one
 * @param exp_failure   If @c TRUE, expect connection failure and
 *                      do not print some verdicts.
 *
 * @return @c 0 on success, @c -1 on falure
 */
static int
establish_tcp_conn(int group_id, te_bool exp_failure)
{
    struct sockaddr *iut_addr;
    zfts_tcp_conn   *conn;

    int rc;

    rpc_zftl_p iut_tl;

    int result = 0;

    conn = &conns[conns_num++];
    conn->group_id = group_id;
    conn->iut_zft = RPC_NULL;
    conn->tst_s = -1;

    if (group_id == 0)
    {
        iut_addr = SA(&iut_listen_addr1);
        iut_tl = iut_tl_zocks[0];
    }
    else
    {
        iut_addr = SA(&iut_listen_addr2);
        iut_tl = iut_tl_zocks[1];
    }

    conn->tst_s = rpc_socket(pco_tst,
                             rpc_socket_domain_by_addr(iut_addr1),
                             RPC_SOCK_STREAM, RPC_PROTO_DEF);
    rpc_fcntl(pco_tst, conn->tst_s, RPC_F_SETFL, RPC_O_NONBLOCK);

    RPC_AWAIT_IUT_ERROR(pco_tst);
    rc = rpc_connect(pco_tst, conn->tst_s, iut_addr);
    if (rc < 0 && RPC_ERRNO(pco_tst) != RPC_EINPROGRESS)
    {
        ERROR_VERDICT("The first nonblocking connect() "
                      "call failed with errno %r", RPC_ERRNO(pco_tst));
        result = -1;
    }

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_process_events_long(pco_iut, stack,
                                    ZFTS_WAIT_EVENTS_TIMEOUT);
    if (rc < 0)
    {
        if (!exp_failure)
            ERROR_VERDICT("Failed to process incoming connection, errno %r",
                          RPC_ERRNO(pco_iut));
        result = -1;
    }

    TAPI_WAIT_NETWORK;
    RPC_AWAIT_IUT_ERROR(pco_tst);
    rc = rpc_connect(pco_tst, conn->tst_s, iut_addr);
    if (rc < 0 && RPC_ERRNO(pco_tst) != RPC_EISCONN)
    {
        if (!exp_failure)
            ERROR_VERDICT("connect() failed, errno %r",
                          RPC_ERRNO(pco_tst));
        result = -1;
    }
    rpc_fcntl(pco_tst, conn->tst_s, RPC_F_SETFL, 0);

    if (!rcf_rpc_server_is_alive(pco_iut))
    {
        if (exp_failure)
            ERROR_VERDICT("pco_iut is dead after trying to "
                          "process incoming connection when "
                          "maximum number of TCP connections is "
                          "already opened");
        else
            ERROR_VERDICT("pco_iut is dead after trying to process "
                          "incoming connection");
        return -1;
    }

    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zftl_accept(pco_iut, iut_tl, &conn->iut_zft);
    if (rc < 0)
    {
        if (!exp_failure)
            ERROR_VERDICT("zftl_accept() failed, errno %r",
                          RPC_ERRNO(pco_iut));

        /* Close tester socket if connection is not established, otherwise
         * the next tester socket will compete with this one establishing
         * new connection. */
        RPC_CLOSE(pco_tst, conn->tst_s);
        result = -1;
    }
    else if (exp_failure)
    {
        TEST_VERDICT("Connection was successfully established "
                     "using the %s listening zocket when "
                     "maximum number of open connections is already "
                     "reached", group2str(group_id));
    }

    if (result == 0)
        n_iut_conns[group_id]++;

    return result;
}

/**
 * Close TCP connection, close/free socket and zocket.
 *
 * @param conn    TCP connection.
 */
static void
close_conn(zfts_tcp_conn *conn)
{
    if (conn->tst_s >= 0)
    {
        rpc_close(pco_tst, conn->tst_s);
        conn->tst_s = -1;
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    if (conn->iut_zft != RPC_NULL)
    {
        rpc_zft_free(pco_iut, conn->iut_zft);
        conn->iut_zft = RPC_NULL;
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }
}

/**
 * Close random TCP connection established using one
 * of the listening zockets.
 *
 * @param group_id    if @c 0, close connection established
 *                    using the first listening zocket,
 *                    if @c 1, close connection established
 *                    using the second listening zocket.
 */
static void
close_random_conn(int group_id)
{
    int i;

    for (i = 0; i < conns_num; i++)
    {
        if (conns[i].group_id == group_id &&
            conns[i].iut_zft != RPC_NULL &&
            conns[i].tst_s >= 0)
            break;
    }
    if (i == conns_num)
        TEST_FAIL("Failed to find any connection to close");

    while (1)
    {
        i = rand_range(0, conns_num - 1);
        if (conns[i].group_id == group_id &&
            conns[i].iut_zft != RPC_NULL &&
            conns[i].tst_s >= 0)
        {
            close_conn(&conns[i]);
            return;
        }
    }
}

/**
 * Check that after closing TCP connection established
 * using one of the listening zockets, both listening zockets
 * can accept new incoming connections.
 *
 * @param group_id    if @c 0, close connection established
 *                    using the first listening zocket,
 *                    if @c 1, close connection established
 *                    using the second listening zocket.
 */
static void
close_and_check(int group_id)
{
    int rc;

    close_random_conn(group_id);
    rc = establish_tcp_conn(group_id, FALSE);
    if (rc < 0)
        TEST_VERDICT("Failed to establish connection to the "
                     "%s listening zocket after some of the connections "
                     "previously established via it was closed",
                     group2str(group_id));

    close_random_conn(group_id);
    rc = establish_tcp_conn(1 - group_id, FALSE);
    if (rc < 0)
        TEST_VERDICT("Failed to establish connection to the "
                     "%s listening zocket after some of the connections "
                     "previously established via the %s listening zocket "
                     "was closed",
                     group2str(1 - group_id), group2str(group_id));
}

int
main(int argc, char *argv[])
{

    te_bool   same_addr = FALSE;
    te_bool   same_port = FALSE;
    te_bool   overflow_limit = FALSE;

    int     i;
    int     j;

    rpc_zft_p iut_zft_aux = RPC_NULL;

    int max_conns_num = 0;

    te_bool exp_failure = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr1);
    TEST_GET_ADDR(pco_iut, iut_addr2);
    TEST_GET_BOOL_PARAM(same_addr);
    TEST_GET_BOOL_PARAM(same_port);
    TEST_GET_BOOL_PARAM(overflow_limit);
    TEST_GET_SET_DEF_RECV_FUNC(recv_func);

    /*- Create two listener zockets:
     * - use the same IP or port value for the second listener zocket in
     *   dependence or arguments @p same_addr and @p same port;
     * - if IP and port are completely equal the second call should fail.
     */

    tapi_sockaddr_clone_exact(iut_addr1, &iut_listen_addr1);

    if (same_addr)
        tapi_sockaddr_clone_exact(iut_addr1, &iut_listen_addr2);
    else
        tapi_sockaddr_clone_exact(iut_addr2, &iut_listen_addr2);

    if (same_port)
        te_sockaddr_set_port(SA(&iut_listen_addr2),
                             te_sockaddr_get_port(iut_addr1));
    else
        te_sockaddr_set_port(SA(&iut_listen_addr2),
                             te_sockaddr_get_port(iut_addr2));

    zfts_create_stack(pco_iut, &attr, &stack);

    rpc_zftl_listen(pco_iut, stack, SA(&iut_listen_addr1),
                    attr, &iut_tl_zocks[0]);
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = rpc_zftl_listen(pco_iut, stack, SA(&iut_listen_addr2),
                         attr, &iut_tl_zocks[1]);
    if (rc < 0)
    {
        if (same_addr && same_port)
            TEST_SUCCESS;
        else
            TEST_VERDICT("Failed to create the second "
                         "listening zocket, errno %r", RPC_ERRNO(pco_iut));
    }
    else if (rc >= 0 && same_addr && same_port)
        TEST_VERDICT("The second listening zocket was bound to the same "
                     "address and port as the first one");

    /*- In the loop while maximum conenctions number is not reached:
     *  - connect from a tester socket to one of listener zockets (in random
     *    order);
     *  - do @a rpc_zf_process_events_long() on IUT;
     *  - accept connection by one of the listener zockets;
     *  - check that accept call on the second zocket fails;
     *  - send/receive some data;
     *
     *  If @p overflow_limit, try to establish additional TCP connection
     *  using both listening zockets and check that @a zftl_accept() fails
     *  for both of them.
     */

    max_conns_num = ZFTS_MAX_TCP_ENDPOINTS;
    if (overflow_limit)
    {
        /**
         * Two additional iterations to try to overflow maximum
         * connections limit, the first time with the first listening
         * zocket, the second time with the second one.
         */

        max_conns_num += 2;
    }

    for (i = 0; i < max_conns_num; i++)
    {
        RING("Establishing connection number %d", i + 1);

        if (i < ZFTS_MAX_TCP_ENDPOINTS)
        {
            j = rand_range(0, 1);
            /* This is done to prevent too uneven distribution from
             * happening by chance */
            if (n_iut_conns[j] - n_iut_conns[1 - j] > 10)
                j = 1 - j;
        }
        else
        {
            j = i - ZFTS_MAX_TCP_ENDPOINTS;
        }

        exp_failure = (i >= ZFTS_MAX_TCP_ENDPOINTS);

        rc = establish_tcp_conn(j, exp_failure);
        if ((!exp_failure && rc < 0) ||
            (exp_failure && rc >= 0))
            TEST_STOP;

        if (!rcf_rpc_server_is_alive(pco_iut))
            TEST_STOP;

        RPC_AWAIT_IUT_ERROR(pco_iut);
        rc = rpc_zftl_accept(pco_iut, iut_tl_zocks[1 - j], &iut_zft_aux);
        if (rc >= 0)
        {
            if (i >= ZFTS_MAX_TCP_ENDPOINTS)
                TEST_VERDICT("Connection was accepted on wrong "
                              "listening zocket after endpoints limit "
                              "was reached");
            else
                TEST_VERDICT("Connection was accepted on "
                             "wrong listening zocket");
        }

        if (i < ZFTS_MAX_TCP_ENDPOINTS)
            zfts_zft_check_connection(pco_iut, stack,
                                      conns[conns_num - 1].iut_zft,
                                      pco_tst, conns[conns_num - 1].tst_s);
    }

    /*- Close and free one of accepted connections, which was accepted
     *  by the first listener.
     *  Check the first listener can receive connections.
     *  Close and free one of accepted connections, which was accepted
     *  by the first listener.
     *  Check the second listener can receive connections. */
    RING("Checking that closing connection established with the first "
         "listening zocket allows to establish new connections");
    close_and_check(0);

    RING("Checking that closing connection established with the second "
         "listening zocket allows to establish new connections");
    /*- Repeat the previous four step, but closing a connection established
     * using the second listener zocket. */
    close_and_check(1);

    TEST_SUCCESS;

cleanup:

    for (i = 0; i < conns_num; i++)
    {
        close_conn(&conns[i]);
    }

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft_aux);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_tl_zocks[0]);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_tl_zocks[1]);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
