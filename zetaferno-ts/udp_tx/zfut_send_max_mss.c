/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP TX tests
 *
 * $Id$
 */

/**
 * @page udp_tx-zfut_send_max_mss Check zfut_get_mss() returns a real maximum.
 *
 * @objective Get MSS and try to send a datagram of this and bigger size.
 *
 * @param send_func       Send function type:
 *                        - zfut_send_single
 *                        - zfut_send
 * @param dont_fragment   If @c TRUE, pass @c ZFUT_FLAG_DONT_FRAGMENT
 *                        to @b zfut_send().
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "udp_tx/zfut_send_max_mss"

#include "zf_test.h"
#include "rpc_zf.h"

#include "te_dbuf.h"

/**
 * Call specified send function on UDP zocket,
 * passing flags if applicable.
 *
 * @param rpcs        RPC server.
 * @param send_func   Send function.
 * @param buf         Buffer with data to send.
 * @param len         Data length.
 * @param utx         UDP zocket.
 *
 * @return Value returned by send function.
 */
static ssize_t
zfut_send_mss(rcf_rpc_server *rpcs, zfts_send_function send_func,
              rpc_zfut_p utx, char *buf, int len, int flags)
{
    switch (send_func)
    {
        case ZFTS_ZFUT_SEND_SINGLE:
            return rpc_zfut_send_single(rpcs, utx, buf, len);

        case ZFTS_ZFUT_SEND:
        {
            rpc_iovec iov;

            iov.iov_base = buf;
            iov.iov_len = iov.iov_rlen = len;

            return rpc_zfut_send(rpcs, utx, &iov, 1, flags);
        }

        default:
            TEST_FAIL("Unknown send function");
    }

    return -1;
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p     attr = RPC_NULL;
    rpc_zf_stack_p    stack = RPC_NULL;
    rpc_zfut_p        iut_utx = RPC_NULL;
    int               tst_s = -1;
    int               mss = 0;
    int               flags = 0;
    char             *send_buf = NULL;
    int               send_buf_size = 0;
    char             *recv_buf = NULL;
    int               recv_buf_size = 0;

    te_dbuf           iut_sent = TE_DBUF_INIT(0);
    te_dbuf           tst_received = TE_DBUF_INIT(0);

    zfts_send_function    send_func;
    te_bool               dont_fragment;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    ZFTS_TEST_GET_ZFUT_FUNCTION(send_func);
    TEST_GET_BOOL_PARAM(dont_fragment);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate UDP TX zocket. */
    rpc_zfut_alloc(pco_iut, &iut_utx, stack, iut_addr, tst_addr, 0, attr);

    /*- Create UDP socket on Tester. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);
    rpc_connect(pco_tst, tst_s, iut_addr);

    /*- Get MSS using function @p zfut_get_mss(). */
    mss = rpc_zfut_get_mss(pco_iut, iut_utx);

    if (dont_fragment)
        flags = RPC_ZFUT_FLAG_DONT_FRAGMENT;

    send_buf_size = mss + 1;
    send_buf = te_make_buf_by_len(send_buf_size);

    /*- Send a datagram:
     *  - use send function @p send_func;
     *  - the datagram length is equal to the returned MSS value. */

    rc = zfut_send_mss(pco_iut, send_func, iut_utx,
                       send_buf, mss, flags);
    if (rc < mss)
        TEST_VERDICT("Instead of MSS bytes less was sent");

    rpc_zf_process_events(pco_iut, stack);
    te_dbuf_append(&iut_sent, send_buf, mss);

    if (send_func != ZFTS_ZFUT_SEND_SINGLE)
    {
        /*- If @p send_func is @c zft_send, try send one more datagram
         * but with length = MSS + 1: if @c ZFUT_FLAG_DONT_FRAGMENT flag
         * is specified, the call should fail with @c EMSGSIZE, else it
         * should succeed */

        RPC_AWAIT_ERROR(pco_iut);
        rc = zfut_send_mss(pco_iut, send_func, iut_utx,
                           send_buf, mss + 1, flags);
        if (rc < 0)
        {
            if (send_func == ZFTS_ZFUT_SEND && !dont_fragment)
                TEST_VERDICT("zft_send() failed unexpectedly with errno %r",
                             RPC_ERRNO(pco_iut));
            else if (RPC_ERRNO(pco_iut) != RPC_EMSGSIZE)
                TEST_VERDICT("Send function failed with unexpected "
                             "errno %r",
                             RPC_ERRNO(pco_iut));
        }
        else if (rc < mss + 1)
        {
            ERROR_VERDICT("Less than expected was sent when "
                          "trying to send MSS + 1 bytes");
        }

        if (rc > 0)
            te_dbuf_append(&iut_sent, send_buf, rc);
    }

    ZFTS_WAIT_NETWORK(pco_iut, stack);

    /*- Read and check data on Tester. Check that no extra data
     * comes to Tester. */

    recv_buf_size = 3 * mss;
    recv_buf = tapi_calloc(recv_buf_size, 1);

    while (TRUE)
    {
        RPC_AWAIT_ERROR(pco_tst);
        rc = rpc_recv(pco_tst, tst_s, recv_buf, recv_buf_size,
                      RPC_MSG_DONTWAIT);
        if (rc < 0)
        {
            if (RPC_ERRNO(pco_tst) != RPC_EAGAIN)
                TEST_VERDICT("recv() on Tester failed with "
                             "unexpected errno %r", RPC_ERRNO(pco_tst));

            break;
        }
        te_dbuf_append(&tst_received, recv_buf, rc);
    }

    ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr, iut_sent.ptr,
                             tst_received.len, iut_sent.len,
                             " from IUT");

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_utx);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    free(send_buf);
    free(recv_buf);
    te_dbuf_free(&iut_sent);
    te_dbuf_free(&tst_received);

    TEST_END;
}
