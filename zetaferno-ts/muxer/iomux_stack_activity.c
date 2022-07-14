/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 */

/**
 * @page muxer-iomux_stack_activity Track ZF stack activity using various iomuxes.
 *
 * @objective Check that an iomux can be used to wait before calling
 *            @b zf_muxer_wait().
 *
 * @param iomux               I/O multiplexer type:
 *                            - @c select
 *                            - @c pselect
 *                            - @c poll
 *                            - @c ppoll
 *                            - @c epoll_wait
 *                            - @c epoll_pwait
 * @param edge_triggered      If @c TRUE, check events in edge-triggered
 *                            mode (makes sense only for epoll functions).
 * @param zf_event            Tested event:
 *                            - @c urx.in
 *                            - @c zft-act.in
 *                            - @c zft-act.out
 *                            - @c zft-pas.in
 *                            - @c zft-pas.out
 *                            - @c zftl.in
 * @param timeout             Iomux timeout value:
 *                            - @c -1
 *                            - @c 0
 *                            - @c 500
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/iomux_stack_activity"

#include "zf_test.h"
#include "rpc_zf.h"

#include "tapi_iomux.h"

/**
 * How long to check for events with help of
 * rpc_zf_muxer_wait() until giving up,
 * milliseconds.
 */
#define EVTS_PROCESS_TIMEOUT 500

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    zfts_mzockets        mzockets = {0};
    int                  fd = -1;
    tapi_iomux_handle   *iomux_handle = NULL;
    tapi_iomux_evt_fd   *evts = NULL;

    tapi_iomux_type   iomux;
    te_bool           edge_triggered;
    int               timeout;
    const char       *zf_event = NULL;

    struct timeval  tv_start;
    struct timeval  tv_cur;

    zfts_mzocket *mzocket = NULL;
    rpc_zf_muxer_set_p muxer_set = RPC_NULL;

    te_bool no_evts_verdict = FALSE;

    te_bool test_failed = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_TE_IOMUX_FUNC(iomux);
    TEST_GET_BOOL_PARAM(edge_triggered);
    TEST_GET_STRING_PARAM(zf_event);
    TEST_GET_INT_PARAM(timeout);

    gettimeofday(&tv_cur, NULL);
    RING("Start gettimeofday %d.%d", (int)tv_cur.tv_sec, (int)tv_cur.tv_usec);

    TEST_STEP("Allocate ZF attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Create a zocket according to @p zf_event.");
    zfts_create_mzockets(zf_event, pco_iut, attr, stack, iut_addr,
                         pco_tst, tst_addr, &mzockets);

    TEST_STEP("Create a fd for the stack using @b zf_waitable_fd_get().");
    rpc_zf_waitable_fd_get(pco_iut, stack, &fd);

    TEST_STEP("Prepare an event if required.");
    zfts_mzockets_prepare_events(&mzockets, zf_event);

    TEST_STEP("Allocate muxer set with @b zf_muxer_alloc(); add a zocket "
              "with an event from @p zf_event to it.");
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    zfts_mzockets_add_exp_events(muxer_set, &mzockets);

    TEST_STEP("Make preparations for calling @p iomux and add the stack "
              "fd to the iomux set.");
    iomux_handle = tapi_iomux_create(pco_iut, iomux);
    tapi_iomux_add(iomux_handle, fd,
                   EVT_RD | EVT_WR | EVT_EXC | EVT_ERR |
                   EVT_HUP | EVT_RDHUP | (edge_triggered ? EVT_ET : 0));

    TEST_STEP("Check that ZF stack is quiescent.");
    ZFTS_WAIT_NETWORK(pco_iut, stack);
    rc = rpc_zf_stack_is_quiescent(pco_iut, stack);
    if (zfts_zocket_type_zft(mzockets.mzocks[0].zock_type))
    {
        if (rc != 0)
            ERROR_VERDICT("Stack is quiescent before calling iomux despite "
                          "having an open TCP connection");
    }
    else
    {
        if (rc == 0)
            ERROR_VERDICT("Stack is not quiescent before calling iomux");
    }

    TEST_STEP("Invoke an event according to @p zf_event.");
    zfts_mzockets_invoke_events(&mzockets);

    gettimeofday(&tv_start, NULL);
    RING("gettimeofday %d.%d", (int)tv_start.tv_sec, (int)tv_start.tv_usec);

    TEST_STEP("In a loop call @b zf_waitable_fd_prime(), @p iomux function "
              "and @b rpc_zf_muxer_wait(timeout=0) until "
              "@b rpc_zf_muxer_wait() reports the expected event or "
              "@c EVTS_PROCESS_TIMEOUT expires.");

    while (TRUE)
    {
        gettimeofday(&tv_cur, NULL);
        RING("gettimeofday %d.%d, diff %ld us", (int)tv_cur.tv_sec,
             (int)tv_cur.tv_usec, TIMEVAL_SUB(tv_cur, tv_start));
        if (TIMEVAL_SUB(tv_cur, tv_start) > TE_MS2US(EVTS_PROCESS_TIMEOUT))
        {
            ERROR_VERDICT("Muxer did not return an event in time");
            test_failed = TRUE;
            break;
        }

        rpc_zf_waitable_fd_prime(pco_iut, stack);

        RPC_AWAIT_ERROR(pco_iut);
        rc = tapi_iomux_call(iomux_handle, timeout, &evts);
        if (rc < 0)
        {
            TEST_VERDICT("Iomux unexpectedly failed with %r",
                         RPC_ERRNO(pco_iut));
        }
        else if (rc == 0 &&
                 (timeout < 0 ||
                  TE_US2MS(pco_iut->duration) < (unsigned)timeout))
        {
            if (!no_evts_verdict)
                ERROR_VERDICT("Iomux unexpectedly terminated "
                              "returning no events before timeout "
                              "expired");
            no_evts_verdict = TRUE;
            test_failed = TRUE;
        }
        else if (rc > 0)
        {
            if (rc > 1)
                TEST_VERDICT("Iomux returned too big value");

            if (zfts_mzockets_check_events(pco_iut, muxer_set, 0, TRUE,
                                           &mzockets, "Checking ZF muxer"))
            {
                RING("Muxer returned the expected event");

                break;
            }
        }

        /* Hide all subsequent successful RPC calls - they create huge logs. */
        pco_iut->silent_pass = pco_iut->silent_pass_default = TRUE;
    }
    pco_iut->silent_pass = pco_iut->silent_pass_default = FALSE;

    TEST_STEP("Check that the event was processed: we can read data or "
              "accept connection in case of @c EPOLLIN event, or write "
              "data in case of @c EPOLLOUT event - without calling "
              "@b rpc_zf_process_events() or @b rpc_zf_muxer_wait() "
              "again.");

    mzocket = &mzockets.mzocks[0];

    if (mzocket->exp_events & RPC_EPOLLIN)
    {
        if (mzocket->zock_type == ZFTS_ZOCKET_URX ||
            zfts_zocket_type_zft(mzocket->zock_type))
        {
            te_dbuf received_data = TE_DBUF_INIT(0);

            if (mzocket->zock_type == ZFTS_ZOCKET_URX)
            {
                zfts_zfur_read_data(mzocket->pco_iut,
                                    mzocket->stack,
                                    mzocket->zocket,
                                    &received_data);
            }
            else
            {
                zfts_zft_read_data(mzocket->pco_iut,
                                   mzocket->zocket,
                                   &received_data,
                                   NULL);
            }

            ZFTS_CHECK_RECEIVED_DATA(received_data.ptr,
                                     mzocket->tst_sent_data.ptr,
                                     received_data.len,
                                     mzocket->tst_sent_data.len,
                                     " from peer socket");

            te_dbuf_free(&mzocket->tst_sent_data);
            te_dbuf_free(&received_data);
        }
        else if (mzocket->zock_type == ZFTS_ZOCKET_ZFTL)
        {
            rpc_zft_p zft_accepted = RPC_NULL;
            int       peer_sock;

            rpc_zftl_accept(mzocket->pco_iut, mzocket->zocket,
                            &zft_accepted);
            peer_sock = mzocket->lpeers[0].lpeer_sock;

            rpc_fcntl(mzocket->pco_tst, peer_sock,
                      RPC_F_SETFL, 0);
            zfts_zft_check_connection(mzocket->pco_iut,
                                      mzocket->stack,
                                      zft_accepted,
                                      mzocket->pco_tst,
                                      peer_sock);

            rpc_zft_free(mzocket->pco_iut, zft_accepted);
            rpc_close(mzocket->pco_tst, peer_sock);
            mzocket->lpeers[0].lpeer_sock = -1;
        }
    }

    if (mzocket->exp_events & RPC_EPOLLOUT)
    {
        if (zfts_zocket_type_zft(mzocket->zock_type))
        {
            char      data[ZFTS_TCP_DATA_MAX];
            rpc_iovec iov;

            te_fill_buf(data, ZFTS_TCP_DATA_MAX);
            iov.iov_base = data;
            iov.iov_len = ZFTS_TCP_DATA_MAX;
            iov.iov_rlen = iov.iov_len;

            RPC_AWAIT_ERROR(mzocket->pco_iut);
            rc = rpc_zft_send(mzocket->pco_iut, mzocket->zocket,
                              &iov, 1, 0);
            if (rc <= 0)
                TEST_VERDICT("Sending additional data failed after "
                             "provoking OUT event");

            te_dbuf_append(&mzocket->iut_sent_data,
                           data, rc);

            zfts_zft_peer_read_all(mzocket->pco_iut, mzocket->stack,
                                   mzocket->pco_tst, mzocket->peer_sock,
                                   &mzocket->tst_recv_data);

            ZFTS_CHECK_RECEIVED_DATA(mzocket->tst_recv_data.ptr,
                                     mzocket->iut_sent_data.ptr,
                                     mzocket->tst_recv_data.len,
                                     mzocket->iut_sent_data.len,
                                     " from zocket after overfilling "
                                     "its send buffer");
            te_dbuf_free(&mzocket->iut_sent_data);
            te_dbuf_free(&mzocket->tst_recv_data);
        }
    }

    TEST_STEP("Check that ZF stack is quiescent.");
    ZFTS_WAIT_NETWORK(pco_iut, stack);
    rc = rpc_zf_stack_is_quiescent(pco_iut, stack);
    if (zfts_zocket_type_zft(mzockets.mzocks[0].zock_type))
    {
        if (rc != 0)
            ERROR_VERDICT("Stack is quiescent after calling iomux despite "
                          "having an open TCP connection");
    }
    else
    {
        if (rc == 0)
            ERROR_VERDICT("Stack is not quiescent after processing events");
    }

    if (test_failed)
        TEST_STOP;
    TEST_SUCCESS;

cleanup:
    pco_iut->silent_pass = pco_iut->silent_pass_default = FALSE;

    tapi_iomux_destroy(iomux_handle);
    zfts_destroy_mzockets(&mzockets);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
