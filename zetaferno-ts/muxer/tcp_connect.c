/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-tcp_connect Multiplexer event after a connection attempt
 *
 * @objective Check multiplexer event which is returned when
 *            connection attempt fails or succeeds.
 *
 * @param status    Connection status:
 *                  - refused (RST in answer to SYN is sent from tester)
 *                  - timeout (connection attempt is aborted by timeout)
 *                  - delayed (connection is established after some delay
 *                    (few SYN retransmits))
 * @param timeout   Muxer timeout value (milliseconds):
 *                  - -1;
 *                  - 0;
 *                  - 500;
 *                  - 10000;
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/tcp_connect"

#include "zf_test.h"
#include "rpc_zf.h"

/**
 * If timeout is not negative but less than this number,
 * we call @b zf_muxer_wait() several times repeatedly;
 * otherwise we call it the single time with @c RCF_RPC_CALL.
 */
#define TIMEOUT_LIMIT 10000

/**
 * Call @b zf_muxer_wait() and check what it returns.
 *
 * @param exp_rc_       Expected return value.
 * @param exp_events_   Expected events.
 * @param err_msg_      String to be added to verdict in case
 *                      of failure.
 */
#define CHECK_MUXER_WAIT_RESULT(exp_rc_, exp_events_, err_msg_) \
    do {                                                              \
        RPC_AWAIT_ERROR(pco_iut);                                     \
        rc = rpc_zf_muxer_wait(pco_iut, muxer_set,                    \
                               &event, 1, timeout);                   \
        if (rc < 0)                                                   \
        {                                                             \
            TEST_VERDICT("%s: zf_muxer_wait() failed unexpectedly "   \
                         "with errno %r",                             \
                         err_msg_, RPC_ERRNO(pco_iut));               \
        }                                                             \
        else if (rc > 1)                                              \
        {                                                             \
            TEST_VERDICT("%s: zf_muxer_wait() returned strange "      \
                         "value %d", err_msg_, rc);                   \
        }                                                             \
        else if (rc != exp_rc_)                                       \
        {                                                             \
            if (rc == 0)                                              \
                TEST_VERDICT("%s: zf_muxer_wait() unexpectedly "      \
                             "returned no events", err_msg_);         \
            else                                                      \
                TEST_VERDICT("%s: zf_muxer_wait() unexpectedly "      \
                             "returned events %s", err_msg_,          \
                             epoll_event_rpc2str(event.events));      \
        }                                                             \
        else if (rc > 0 && exp_events_ != event.events)               \
        {                                                             \
            TEST_VERDICT("%s: zf_muxer_wait() "                       \
                         "returned unexpected events %s", err_msg_,   \
                         epoll_event_rpc2str(event.events));          \
        }                                                             \
    } while (0)

/**
 * Call @b zf_muxer_wait() in blocking or non-blocking manner if required,
 * and/or check that it terminates or not as expected.
 *
 * @param first_call_     Whether this is the first time we call/check
 *                        @b zf_muxer_wait(), or not.
 * @param exp_rc_         Expected return value.
 * @param exp_evts_       Expected events.
 * @param err_msg_        String to be added to verdict in case of failure.
 */
#define CHECK_EVENTS(first_call_, exp_rc_, exp_evts_, err_msg_) \
    do {                                                                \
        te_bool done_ = FALSE;                                          \
                                                                        \
        if (timeout < 0 || timeout >= TIMEOUT_LIMIT)                    \
        {                                                               \
            if (first_call_)                                            \
            {                                                           \
                pco_iut->op = RCF_RPC_CALL;                             \
                rpc_zf_muxer_wait(pco_iut, muxer_set,                   \
                                  &event, 1, timeout);                  \
            }                                                           \
            else if (exp_rc_ > 0)                                       \
            {                                                           \
                CHECK_MUXER_WAIT_RESULT(1, exp_evts_, err_msg_);        \
            }                                                           \
            else                                                        \
            {                                                           \
                CHECK_RC(rcf_rpc_server_is_op_done(pco_iut, &done_));   \
                if (done_)                                              \
                    CHECK_MUXER_WAIT_RESULT(0, 0, err_msg_);            \
            }                                                           \
        }                                                               \
        else                                                            \
        {                                                               \
            if (timeout > 0 && first_call_)                             \
            {                                                           \
                pco_iut->op = RCF_RPC_CALL;                             \
                rpc_zf_muxer_wait(pco_iut, muxer_set,                   \
                                  &event, 1, timeout);                  \
                CHECK_RC(rcf_rpc_server_is_op_done(pco_iut, &done_));   \
                    if (done_)                                          \
                        CHECK_MUXER_WAIT_RESULT(0, 0, err_msg_);        \
            }                                                           \
            CHECK_MUXER_WAIT_RESULT(exp_rc_, exp_evts_, err_msg_);      \
        }                                                               \
    } while (0)

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

    int            tst_s_listening = -1;
    int            tst_s = -1;

    te_bool        route_dst_added = FALSE;
    te_bool        route_src_added = FALSE;

    int            addr_family;

    rpc_zft_handle_p    iut_zft_handle = RPC_NULL;
    rpc_zft_p           iut_zft = RPC_NULL;
    rpc_zf_waitable_p   waitable = RPC_NULL;
    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;

    struct rpc_epoll_event event;
    uint32_t               exp_events;

    int               timeout;
    zfts_conn_problem status;
    rpc_errno         zft_err;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_gw);
    TEST_GET_PCO(pco_tst);

    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ADDR_NO_PORT(gw_iut_addr);
    TEST_GET_ADDR_NO_PORT(gw_tst_addr);
    TEST_GET_ENUM_PARAM(status, ZFTS_CONN_PROBLEMS);
    TEST_GET_INT_PARAM(timeout);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    addr_family = iut_addr->sa_family;

    if (status == ZFTS_CONN_DELAYED)
    {
        /*- If @p status is @c delayed, create TCP listener
         * socket on Tester to accept connection. */
        tst_s_listening =
          rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                     RPC_PROTO_DEF, FALSE, FALSE, tst_addr);
        rpc_listen(pco_tst, tst_s_listening, 1);
    }

    /*- Break connection between IUT and tester using gateway. */

    /* Add route on 'pco_iut': 'tst_addr' via gateway 'gw_iut_addr' */
    if (tapi_cfg_add_route_via_gw(
            pco_iut->ta,
            addr_family,
            te_sockaddr_get_netaddr(tst_addr),
            te_netaddr_get_size(addr_family) * 8,
            te_sockaddr_get_netaddr(gw_iut_addr)) != 0)
    {
        TEST_FAIL("Cannot add route to the Tester");
    }
    route_dst_added = TRUE;

    /* Add route on 'pco_tst': 'iut_addr' via gateway 'gw_tst_addr' */
    if (tapi_cfg_add_route_via_gw(
            pco_tst->ta,
            addr_family,
            te_sockaddr_get_netaddr(iut_addr),
            te_netaddr_get_size(addr_family) * 8,
            te_sockaddr_get_netaddr(gw_tst_addr)) != 0)
    {
        TEST_FAIL("Cannot add route to the IUT");
    }
    route_src_added = TRUE;

    CHECK_RC(tapi_cfg_sys_set_int(pco_gw->ta, 0, NULL,
                                  "net/ipv4/ip_forward"));

    CFG_WAIT_CHANGES;

    /*- Allocate a multiplexer set. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);

    /*- Allocate and bind active open TCP zocket. */
    rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);
    rpc_zft_addr_bind(pco_iut, iut_zft_handle, iut_addr, 0);

    /*- Perform zft_connect(). */
    rpc_zft_connect(pco_iut, iut_zft_handle, tst_addr, &iut_zft);
    rpc_zf_process_events(pco_iut, stack);

    /*- Get muxer handle of the returned zocket and add to
     * the muxer set. */
    waitable = rpc_zft_to_waitable(pco_iut, iut_zft);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, waitable,
                            iut_zft,
                            RPC_EPOLLIN | RPC_EPOLLOUT |
                            RPC_EPOLLHUP | RPC_EPOLLERR);

    /*- Call @b zf_muxer_wait() the first time; if @p timeout is small,
     * check that it does not return any events. */
    CHECK_EVENTS(TRUE, 0, 0, "The first call");

    /*- Sleep 0.5 second. If @p timeout is big, check that
     * @b zf_muxer_wait() call has not terminated yet; otherwise call
     * @b zf_muxer_wait() again and check that it does not return
     * any events. */

    MSLEEP(500);
    if (timeout >= 0 && timeout < TIMEOUT_LIMIT)
        rpc_zf_process_events(pco_iut, stack);

    CHECK_EVENTS(FALSE, 0, 0, "The call after delay");

    /*- If @p status is not @c timeout, repair connection between
     * IUT and Tester. */
    if (status != ZFTS_CONN_TIMEOUT)
    {
        CHECK_RC(tapi_cfg_sys_set_int(pco_gw->ta, 1, NULL,
                                      "net/ipv4/ip_forward"));
    }

    if (timeout >= 0 && timeout < TIMEOUT_LIMIT)
        zfts_wait_for_final_tcp_state(pco_iut, stack, iut_zft);

    /*- If @p timeout is big, wait until @p zf_muxer_wait() terminates;
     * otherwise call it the third time. Check that it returns expected
     * events. */

    if (status == ZFTS_CONN_DELAYED)
        exp_events = RPC_EPOLLOUT;
    else
        exp_events = RPC_EPOLLIN | RPC_EPOLLOUT |
                     RPC_EPOLLERR | RPC_EPOLLHUP;

    CHECK_EVENTS(FALSE, 1, exp_events,
                 "The call after forwarding is fixed");

    /*- If @p status is @c timeout, repair connection between IUT
     * and Tester. */
    if (status == ZFTS_CONN_TIMEOUT)
    {
        CHECK_RC(tapi_cfg_sys_set_int(pco_gw->ta, 1, NULL,
                                      "net/ipv4/ip_forward"));
    }

    zft_err = rpc_zft_error(pco_iut, iut_zft);

    if ((status == ZFTS_CONN_TIMEOUT && zft_err != RPC_ETIMEDOUT) ||
        (status == ZFTS_CONN_REFUSED && zft_err != RPC_ECONNREFUSED) ||
        (status == ZFTS_CONN_DELAYED && zft_err != RPC_EOK))
    {
        TEST_VERDICT("zft_error() returned unexpected errno %s (%d)",
                     errno_rpc2str(zft_err), zft_err);
    }

    /*- If @p status is @c delayed, check that connection can be accepted
     * on Tester; otherwise free the zocket and establish a new connection
     * with the same addresses. */
    if (status == ZFTS_CONN_DELAYED)
    {
        tst_s = rpc_accept(pco_tst, tst_s_listening, NULL, NULL);
    }
    else
    {
        rpc_zft_free(pco_iut, iut_zft);

        zfts_establish_tcp_conn(TRUE, pco_iut, attr, stack,
                                &iut_zft, iut_addr,
                                pco_tst, &tst_s, tst_addr);
    }

    /*- Check that data can be transmitted over
     * the established connection. */
    zfts_zft_check_connection(pco_iut, stack, iut_zft, pco_tst, tst_s);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_CLOSE(pco_tst, tst_s_listening);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, waitable);

    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    if (route_dst_added &&
        tapi_cfg_del_route_via_gw(
            pco_iut->ta,
            addr_family,
            te_sockaddr_get_netaddr(tst_addr),
            te_netaddr_get_size(addr_family) * 8,
            te_sockaddr_get_netaddr(gw_iut_addr)) != 0)
    {
        ERROR("Cannot delete route to the Tester");
        result = EXIT_FAILURE;
    }

    if (route_src_added &&
        tapi_cfg_del_route_via_gw(
            pco_tst->ta,
            addr_family,
            te_sockaddr_get_netaddr(iut_addr),
            te_netaddr_get_size(addr_family) * 8,
            te_sockaddr_get_netaddr(gw_tst_addr)) != 0)
    {
        ERROR("Cannot delete route to the IUT");
        result = EXIT_FAILURE;
    }

    CFG_WAIT_CHANGES;

    TEST_END;
}

