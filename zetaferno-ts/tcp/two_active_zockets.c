/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP tests
 *
 * $Id$
 */

/**
 * @page tcp-two_active_zockets Coexistence of two active TCP zockets
 *
 * @objective  Create two TCP zocket in single or two different ZF stacks.
 *             Bind and connect them to various combinations of local and
 *             remote addresses including partly coinciding (IP or port).
 *             Check data can be transmitted in both directions with both
 *             connections if they are established.
 *
 * @param pco_iut   PCO on IUT.
 * @param pco_tst   PCO on TST.
 * @param iut_addr1 IUT address.
 * @param iut_addr2 IUT address.
 * @param tst_addr1 Tester address.
 * @param tst_addr2 Tester address.
 * @param single_stack         Use single ZF stack for both zockets.
 * @param first_bind_addr      Address to bind the first zocket:
 *                              - @c NULL;
 *                              - @p iut_addr1:port1;
 *                              - do not bind.
 * @param second_bind_addr     Address to bind the second zocket:
 *                              - @c NULL;
 *                              - @p iut_addr{1,2}:port{1,2};
 *                              - do not bind.
 * @param first_connect_addr   Address to connect the first zocket:
 *                              - @p iut_addr1:port1;
 * @param second_connect_addr  Address to connect the second zocket:
 *                              - @p iut_addr{1,2}:port{1,2};
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

#define TE_TEST_NAME  "tcp/two_active_zockets"

#include "zf_test.h"
#include "rpc_zf.h"

#define ADDR_COMP_LEN 20

/**
 * Get address composed of two given addresses.
 *
 * @param addr1         The first address
 * @param addr1         The second address
 * @param addr_makeup   What part (IP address, port) of what address to use
 *                      (e.g. addr1:port2)
 *
 * @return Pointer to composed address
 */
static struct sockaddr_storage *
get_addr(const struct sockaddr *addr1,
         const struct sockaddr *addr2,
         const char *addr_makeup)
{
    static struct sockaddr_storage addr;

    const char *s;

    if (strcmp(addr_makeup, "null") == 0)
        return NULL;

    s = strchr(addr_makeup, ':');
    if (s == NULL)
        TEST_FAIL("Unrecognized address makeup: %s", addr_makeup);
    s++;

    if (strcmp_start("addr1", addr_makeup) == 0)
        tapi_sockaddr_clone_exact(addr1, &addr);
    else if (strcmp_start("addr2", addr_makeup) == 0)
        tapi_sockaddr_clone_exact(addr2, &addr);
    else
        TEST_FAIL("Failed to parse address in %s", addr_makeup);

    if (strcmp_start("port1", s) == 0)
        te_sockaddr_set_port(SA(&addr), te_sockaddr_get_port(addr1));
    else if (strcmp_start("port2", s) == 0)
        te_sockaddr_set_port(SA(&addr), te_sockaddr_get_port(addr2));
    else
        TEST_FAIL("Failed to parse port in %s", addr_makeup);

    return &addr;
}

/**
 * Bind a zocket handle to requested address.
 *
 * @param rpcs            RPC server
 * @param zocket_handle   RPC pointer ID of zocket handle
 * @param addr1           The first address
 * @param addr2           The second address
 * @param addr_makeup     How to compose binding address
 *                        from the two previous arguments
 *                        (for example, "addr1:port2")
 *
 * @return 0 on success or status code in case of failure
 */
static te_errno
bind_zocket(rcf_rpc_server *rpcs,
            rpc_zft_handle_p zocket_handle,
            const struct sockaddr *addr1,
            const struct sockaddr *addr2,
            const char *addr_makeup)
{
    te_errno rc;

    if (strcmp(addr_makeup, "do_not_bind") == 0)
        return 0;

    RPC_AWAIT_IUT_ERROR(rpcs);
    rc = rpc_zft_addr_bind(rpcs, zocket_handle,
                           SA(get_addr(addr1, addr2,
                                       addr_makeup)),
                           0);

    return rc;
}

/**
 * Check that listening socket is ready to accept incoming
 * connection.
 *
 * @param rpcs        RPC server
 * @param sock        Listening socket
 * @param err_msg     Error message to print in verdict in case
 *                    of failure
 */
static void
check_listening_socket(rcf_rpc_server *rpcs,
                       int sock, const char *err_msg)
{
    struct rpc_pollfd poll_fd;

    te_errno rc;

    poll_fd.fd = sock;
    poll_fd.events = RPC_POLLIN;
    poll_fd.revents = 0;

    rc = rpc_poll(rpcs, &poll_fd, 1, 1000);
    if (rc == 0)
        TEST_VERDICT(err_msg);
}

/** Verdict that will be printed in case of unexpected jump to cleanup. */
static char *cleanup_verdict = NULL;

/**
 * Perform RCP call anticipating possible jump to cleanup in case of
 * RPC server death.
 *
 * @param rpcs_       RPC server
 * @param expr_       RPC call expression
 * @param err_msg_    Error message to print in verdict in
 *                    case of RPC call failure.
 */
#define AWAIT_DEAD_RPC(rpcs_, expr_, err_msg_) \
    do {                                                                \
        RPC_AWAIT_IUT_ERROR(rpcs_);                                     \
        cleanup_verdict = err_msg_;                                     \
        rc = expr_;                                                     \
        cleanup_verdict = NULL;                                         \
        if (rc != 0)                                                    \
            TEST_VERDICT("%s, errno %r", err_msg_, RPC_ERRNO(rpcs_));   \
    } while (0)

/**
 * Check if test address arguments denote the same specific address.
 *
 * @param addr1     The first address argument
 * @param addr2     The second address argument
 *
 * @return @c TRUE if addresses are the same, @c FALSE otherwise
 */
te_bool
same_addrs(const char *addr1, const char *addr2)
{
    if (strcmp(addr1, addr2) == 0 &&
        strcmp(addr1, "null") != 0 &&
        strcmp(addr1, "do_not_bind") != 0)
    {
        return TRUE;
    }

    return FALSE;
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server          *pco_iut = NULL;
    rcf_rpc_server          *pco_tst = NULL;
    const struct sockaddr   *iut_addr1 = NULL;
    const struct sockaddr   *iut_addr2 = NULL;
    const struct sockaddr   *tst_addr1 = NULL;
    const struct sockaddr   *tst_addr2 = NULL;

    struct sockaddr_storage    tst_listen_addr1;
    struct sockaddr_storage    tst_listen_addr2;
    struct sockaddr_storage   *addr;

    const char *first_bind_addr = NULL;
    const char *second_bind_addr = NULL;
    const char *first_connect_addr = NULL;
    const char *second_connect_addr = NULL;

    te_bool   single_stack = FALSE;

    rpc_zf_attr_p   attr = RPC_NULL;
    rpc_zf_stack_p  stack1 = RPC_NULL;
    rpc_zf_stack_p  stack2 = RPC_NULL;

    rpc_zft_handle_p  iut_zft_handle1 = RPC_NULL;
    rpc_zft_handle_p  iut_zft_handle2 = RPC_NULL;
    rpc_zft_p         iut_zft1 = RPC_NULL;
    rpc_zft_p         iut_zft2 = RPC_NULL;

    int tst_s_listening1 = -1;
    int tst_s_listening2 = -1;
    int tst_s1 = -1;
    int tst_s2 = -1;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr1);
    TEST_GET_ADDR(pco_iut, iut_addr2);
    TEST_GET_ADDR(pco_tst, tst_addr1);
    TEST_GET_ADDR(pco_tst, tst_addr2);
    TEST_GET_BOOL_PARAM(single_stack);
    TEST_GET_STRING_PARAM(first_bind_addr);
    TEST_GET_STRING_PARAM(second_bind_addr);
    TEST_GET_STRING_PARAM(first_connect_addr);
    TEST_GET_STRING_PARAM(second_connect_addr);
    TEST_GET_SET_DEF_RECV_FUNC(recv_func);

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_stack_alloc(pco_iut, attr, &stack1);

    if (single_stack)
        stack2 = stack1;
    else
        rpc_zf_stack_alloc(pco_iut, attr, &stack2);

    /*- Allocate two TCP zockets: use single or two ZF stacks
     * in dependence on @p single_stack. */

    rpc_zft_alloc(pco_iut, stack1, attr, &iut_zft_handle1);
    rpc_zft_alloc(pco_iut, stack2, attr, &iut_zft_handle2);

    /*- Bind the first zocket to @c NULL, @p iut_addr1:port1 or
     * do not bind it in dependence on @p first_bind_addr. */
    rc = bind_zocket(pco_iut, iut_zft_handle1, iut_addr1, iut_addr2,
                     first_bind_addr);
    if (rc != 0)
        TEST_VERDICT("Binding the first zocket failed: errno %r",
                     RPC_ERRNO(pco_iut));

    /*- Bind the second zocket to @c NULL, @p iut_addr{1,2}:port{1,2} or
     * do not bind it in dependence on @p second_bind_addr */
    rc = bind_zocket(pco_iut, iut_zft_handle2, iut_addr1, iut_addr2,
                     second_bind_addr);
    if (rc == 0 && same_addrs(first_bind_addr, second_bind_addr))
    {
        TEST_VERDICT("The second zocket was bound to the same address as "
                     "the first one");
    }
    else if (rc != 0)
    {
        if (same_addrs(first_bind_addr, second_bind_addr) &&
            RPC_ERRNO(pco_iut) == RPC_EADDRINUSE)
            TEST_SUCCESS;
        else
            TEST_VERDICT("Binding the second zocket failed: errno %r",
                         RPC_ERRNO(pco_iut));
    }

    addr = get_addr(tst_addr1, tst_addr2, first_connect_addr);
    if (addr == NULL)
        TEST_FAIL("Trying to connect the first zocket to NULL address");
    tapi_sockaddr_clone_exact(SA(addr), &tst_listen_addr1);

    addr = get_addr(tst_addr1, tst_addr2, second_connect_addr);
    if (addr == NULL)
        TEST_FAIL("Trying to connect the second zocket to NULL address");
    tapi_sockaddr_clone_exact(SA(addr), &tst_listen_addr2);

    tst_s_listening1 =
      rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                 RPC_PROTO_DEF, FALSE, FALSE,
                                 SA(&tst_listen_addr1));
    rpc_listen(pco_tst, tst_s_listening1, ZFTS_LISTEN_BACKLOG_DEF);

    if (strcmp(first_connect_addr, second_connect_addr) == 0)
        tst_s_listening2 = tst_s_listening1;
    else
    {
        tst_s_listening2 =
          rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                     RPC_PROTO_DEF, FALSE, FALSE,
                                     SA(&tst_listen_addr2));
        rpc_listen(pco_tst, tst_s_listening2, ZFTS_LISTEN_BACKLOG_DEF);
    }

    /*- Connect the first zocket to tester @p tst_addr1:tst_port1. */
    AWAIT_DEAD_RPC(pco_iut,
                   rpc_zft_connect(pco_iut, iut_zft_handle1,
                                   SA(&tst_listen_addr1), &iut_zft1),
                   "zft_connect() failed for the first zocket handle");
    iut_zft_handle1 = RPC_NULL;
    ZFTS_WAIT_NETWORK(pco_iut, stack1);

    /*- Connect the second zocket to tester
     * @p tst_addr{1,2}:tst_port{1,2} according to
     * @p second_connect_addr. */
    AWAIT_DEAD_RPC(pco_iut,
                   rpc_zft_connect(pco_iut, iut_zft_handle2,
                                   SA(&tst_listen_addr2), &iut_zft2),
                   "zft_connect() failed for the second zocket handle");
    iut_zft_handle2 = RPC_NULL;
    ZFTS_WAIT_NETWORK(pco_iut, stack2);

    check_listening_socket(pco_tst, tst_s_listening1,
                           "Listening socket does not see "
                           "the first connection");
    tst_s1 = rpc_accept(pco_tst, tst_s_listening1, NULL, NULL);

    check_listening_socket(pco_tst, tst_s_listening2,
                           "Listening socket does not see "
                           "the second connection");
    tst_s2 = rpc_accept(pco_tst, tst_s_listening2, NULL, NULL);

    /*- Check that data can be transmitted in both directions by established
     * connections. */
    zfts_zft_check_connection(pco_iut, stack1, iut_zft1, pco_tst, tst_s1);
    zfts_zft_check_connection(pco_iut, stack2, iut_zft2, pco_tst, tst_s2);

    TEST_SUCCESS;

cleanup:

    if (cleanup_verdict != NULL)
        ERROR_VERDICT("%s, errno %r", cleanup_verdict, RPC_ERRNO(pco_iut));

    CLEANUP_RPC_CLOSE(pco_tst, tst_s_listening1);
    if (tst_s_listening2 != tst_s_listening1)
        CLEANUP_RPC_CLOSE(pco_tst, tst_s_listening2);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s1);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s2);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft1);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft2);

    CLEANUP_RPC_ZFT_HANDLE_FREE(pco_iut, iut_zft_handle1);
    CLEANUP_RPC_ZFT_HANDLE_FREE(pco_iut, iut_zft_handle2);

    /*
     * Note: this check should be performed before releasing stack1 due to the
     * fact that after that the value of stack1 will be reset to RPC_NULL.
     */
    if (stack2 != stack1)
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack2);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stack1);

    CLEANUP_RPC_ZF_ATTR_FREE(pco_iut, attr);
    CLEANUP_RPC_ZF_DEINIT(pco_iut);

    TEST_END;
}
