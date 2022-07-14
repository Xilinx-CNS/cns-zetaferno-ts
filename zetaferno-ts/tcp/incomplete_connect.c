/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-incomplete_connect Call a TCP function with incomplete connection.
 *
 * @objective Call a TCP function on the zocket when TCP connection fails
 *            or is in progress.
 *
 * @param status    Connection status:
 *                  - refused (RST in answer to SYN is sent from tester)
 *                  - timeout (connection attempt is aborted by timeout)
 *                  - delayed (connection is not established, but not
 *                    aborted yet (SYN retransmits))
 * @param function  Function to be called after connection:
 *                  - zft_shutdown_tx
 *                  - zft_free
 *                  - zft_getname
 *                  - zft_zc_recv
 *                  - zft_recv
 *                  - zft_send
 *                  - zft_addr_bind
 *                  - zft_alternatives_queue
 *                  - zft_send_space
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/incomplete_connect"

#include "zf_test.h"
#include "rpc_zf.h"

/* Minimum known value of alteratives */
#define ZFTS_ALT_COUNT_DEF 15

/* Minimum known value of buffer size for alteratives */
#define ZFTS_ALT_BUF_SIZE_DEF 50000
/* Tested functions. */
typedef enum {
    ZFTS_ZFT_SHUTDOWN_TX = 0,     /* @b zft_shutdown_tx(). */
    ZFTS_ZFT_FREE,                /* @b zft_free(). */
    ZFTS_ZFT_GETNAME,             /* @b zft_getname(). */
    ZFTS_ZFT_ZC_RECV,             /* @b zft_zc_recv(). */
    ZFTS_ZFT_RECV,                /* @b zft_recv(). */
    ZFTS_ZFT_SEND,                /* @b zft_send(). */
    ZFTS_ZFT_ADDR_BIND,           /* @b zft_addr_bind(). */
    ZFTS_ZFT_ALTERNATIVES_QUEUE,  /* @b zft_alternatives_queue(). */
    ZFTS_ZFT_SEND_SPACE,          /* @b zft_send_space() */
} zfts_zft_func;

/* List of tested functions to be used with @b TEST_GET_ENUM_PARAM(). */
#define ZFTS_ZFT_FUNCS \
    { "zft_shutdown_tx",        ZFTS_ZFT_SHUTDOWN_TX }, \
    { "zft_free",               ZFTS_ZFT_FREE }, \
    { "zft_getname",            ZFTS_ZFT_GETNAME }, \
    { "zft_zc_recv",            ZFTS_ZFT_ZC_RECV }, \
    { "zft_recv",               ZFTS_ZFT_RECV }, \
    { "zft_send",               ZFTS_ZFT_SEND }, \
    { "zft_addr_bind",          ZFTS_ZFT_ADDR_BIND }, \
    { "zft_alternatives_queue", ZFTS_ZFT_ALTERNATIVES_QUEUE }, \
    { "zft_send_space",         ZFTS_ZFT_SEND_SPACE }

/**
 * Check whether errno is expected or not.
 *
 * @param func      Tested function type.
 * @param problem   Tested connection problem.
 * @param err       Obtained errno.
 *
 * @return @c TRUE if errno is OK, @c FALSE otherwise.
 */
static te_bool
check_errno(zfts_zft_func func, zfts_conn_problem problem, te_errno err)
{
    if (problem == ZFTS_CONN_REFUSED || problem == ZFTS_CONN_TIMEOUT)
    {
        if (func == ZFTS_ZFT_SHUTDOWN_TX)
        {
            if (err == RPC_ENOTCONN)
                return TRUE;
            else
                return FALSE;
        }
        else
        {
            if ((err == RPC_ECONNREFUSED && problem == ZFTS_CONN_REFUSED) ||
                err == RPC_EINVAL || err == RPC_ENOTCONN ||
                (err == RPC_ETIMEDOUT && problem == ZFTS_CONN_TIMEOUT))
                return TRUE;
            else
                return FALSE;
        }
    }
    else if (problem == ZFTS_CONN_DELAYED)
    {
        if (func == ZFTS_ZFT_ADDR_BIND)
        {
            if (err == RPC_EADDRINUSE)
                return TRUE;
            else
                return FALSE;
        }
        else if (func == ZFTS_ZFT_SEND_SPACE)
        {
            if (err == RPC_ENOTCONN)
                return TRUE;
        }
        else
        {
            if (err == RPC_EAGAIN || err == RPC_EINVAL)
                return TRUE;
            else
                return FALSE;
        }
    }

    return FALSE;
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    rcf_rpc_server *pco_gw = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    const struct sockaddr *gw_iut_addr = NULL;
    const struct sockaddr *gw_tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    struct sockaddr_in laddr;
    struct sockaddr_in raddr;

    int tst_s = -1;

    rpc_zft_handle_p    iut_zft_handle = RPC_NULL;
    rpc_zft_handle_p    iut_zft_handle2 = RPC_NULL;
    rpc_zft_p           iut_zft = RPC_NULL;

    rpc_iovec       iov[ZFTS_IOVCNT];
    int             iovcnt = ZFTS_IOVCNT;
    rpc_zft_msg     msg = {.iovcnt = ZFTS_IOVCNT, .iov = iov};
    size_t          send_space = 0;

    zfts_conn_problem status;
    zfts_zft_func     function;

    rpc_zf_althandle  alt;

    te_bool   exp_failed;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_gw);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ADDR_NO_PORT(gw_iut_addr);
    TEST_GET_ADDR_NO_PORT(gw_tst_addr);
    TEST_GET_ENUM_PARAM(status, ZFTS_CONN_PROBLEMS);
    TEST_GET_ENUM_PARAM(function, ZFTS_ZFT_FUNCS);

    rpc_make_iov(iov, ZFTS_IOVCNT,
                 ZFTS_TCP_DATA_MAX, ZFTS_TCP_DATA_MAX);

    /*- Configure gateway connecting IUT and Tester;
     * break connection if @p status is not @c refused. */
    ZFTS_CONFIGURE_GATEWAY;
    if (status == ZFTS_CONN_REFUSED)
        ZFTS_GATEWAY_SET_FORWARDING(TRUE);
    else
        ZFTS_GATEWAY_SET_FORWARDING(FALSE);
    CFG_WAIT_CHANGES;

    /*- Allocate ZF attributes and stack. */

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    if (function == ZFTS_ZFT_ALTERNATIVES_QUEUE)
    {
        rpc_zf_attr_set_int(pco_iut, attr, "alt_count",
                            ZFTS_ALT_COUNT_DEF);
        rpc_zf_attr_set_int(pco_iut, attr, "alt_buf_size",
                            ZFTS_ALT_BUF_SIZE_DEF);
    }

    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    if (function == ZFTS_ZFT_ALTERNATIVES_QUEUE)
        rpc_zf_alternatives_alloc(pco_iut, stack,
                                  attr, &alt);

    /*- Allocate and bind active open TCP zocket. */
    rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);
    rpc_zft_addr_bind(pco_iut, iut_zft_handle, iut_addr, 0);

    /*- Perform zft_connect(). */
    rpc_zft_connect(pco_iut, iut_zft_handle, tst_addr, &iut_zft);
    rpc_zf_process_events(pco_iut, stack);

    /*- If @p status is not @c delayed, wait until TCP connection
     * is dropped. */
    if (status != ZFTS_CONN_DELAYED)
        zfts_wait_for_final_tcp_state(pco_iut, stack, iut_zft);

    /*- Call function @p function. If @p function is @c zft_addr_bind,
     * create additional zft_handle and try to bind it to
     * the same address. */

    if (function == ZFTS_ZFT_ADDR_BIND)
        rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle2);

    rc = 0;
    RPC_AWAIT_ERROR(pco_iut);
    switch (function)
    {
        case ZFTS_ZFT_SHUTDOWN_TX:
            rc = rpc_zft_shutdown_tx(pco_iut, iut_zft);
            break;

        case ZFTS_ZFT_FREE:
            rc = rpc_zft_free(pco_iut, iut_zft);
            break;

        case ZFTS_ZFT_GETNAME:
            rpc_zft_getname(pco_iut, iut_zft, &laddr, &raddr);
            if (!RPC_IS_CALL_OK(pco_iut))
                rc = -1;

            break;

        case ZFTS_ZFT_ZC_RECV:
            rpc_zft_zc_recv(pco_iut, iut_zft, &msg, 0);
            if (!RPC_IS_CALL_OK(pco_iut))
                rc = -1;

            break;

        case ZFTS_ZFT_RECV:
            rc = rpc_zft_recv(pco_iut, iut_zft, iov, iovcnt, 0);
            break;

        case ZFTS_ZFT_SEND:
            rc = rpc_zft_send(pco_iut, iut_zft, iov, iovcnt, 0);
            break;

        case ZFTS_ZFT_ADDR_BIND:
            rc = rpc_zft_addr_bind(pco_iut, iut_zft_handle2, iut_addr, 0);
            break;

        case ZFTS_ZFT_ALTERNATIVES_QUEUE:
            rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alt,
                                            iov, 1, 0);
            break;

        case ZFTS_ZFT_SEND_SPACE:
            rc = rpc_zft_send_space(pco_iut, iut_zft, &send_space);
            break;

        default:
            TEST_FAIL("Unknown function type");
    }

    /*- Check that data transmission functions fail with reasonable errno
     * (except @c zft_zc_recv which does not return value),
     * @c zft_addr_bind fails with @c EADDRINUSE if @p status is @c delayed
     * and succeeds otherwise, @c zft_shutdown_tx fails with @c ENOTCONN
     * if @p status is @c timeout or @c refused and succeeds otherwise,
     * and other functions succeed. */

    if (function == ZFTS_ZFT_FREE || function == ZFTS_ZFT_GETNAME ||
        function == ZFTS_ZFT_ZC_RECV)
    {
        exp_failed = FALSE;
    }
    else if (function == ZFTS_ZFT_SHUTDOWN_TX)
    {
        if (status == ZFTS_CONN_DELAYED)
            exp_failed = FALSE;
        else
            exp_failed = TRUE;
    }
    else if (function == ZFTS_ZFT_ADDR_BIND)
    {
        if (status == ZFTS_CONN_DELAYED)
            exp_failed = TRUE;
        else
            exp_failed = FALSE;
    }
    else
    {
        exp_failed = TRUE;
    }

    if (rc < 0)
    {
        if (!exp_failed)
            TEST_VERDICT("Tested function failed unexpectedly "
                         "with errno %r", RPC_ERRNO(pco_iut));
        else if (!check_errno(function, status, RPC_ERRNO(pco_iut)))
            TEST_VERDICT("Tested function failed with "
                         "strange errno %r",
                         RPC_ERRNO(pco_iut));
    }
    else
    {
        if (exp_failed)
            TEST_VERDICT("Tested function unexpectedly succeeded");
    }

    if (iut_zft_handle2 != RPC_NULL)
        rpc_zft_handle_free(pco_iut, iut_zft_handle2);

    /*- If @p function is not @c zft_free,
     * free the zocket. */
    if (function != ZFTS_ZFT_FREE)
        rpc_zft_free(pco_iut, iut_zft);

    /*- If connection was broken, repair it. */
    if (status != ZFTS_CONN_REFUSED)
    {
        ZFTS_GATEWAY_SET_FORWARDING(TRUE);
        CFG_WAIT_CHANGES;
    }

    /*- Allocate new zocket and bind it on the same address,
     * establish TCP connection with a peer on Tester. */

    zfts_establish_tcp_conn(TRUE, pco_iut, attr, stack,
                            &iut_zft, iut_addr,
                            pco_tst, &tst_s, tst_addr);

    /*- Check data transmission over established connection. */
    zfts_zft_check_connection(pco_iut, stack, iut_zft, pco_tst, tst_s);

    TEST_SUCCESS;

cleanup:

    if (function == ZFTS_ZFT_ALTERNATIVES_QUEUE)
        CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alt);

    rpc_release_iov(iov, ZFTS_IOVCNT);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
