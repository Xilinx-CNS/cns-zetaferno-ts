/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multiplexer tests
 *
 * $Id$
 */

/**
 * @page muxer-mod_events Change expected events
 *
 * @objective Check that expected events can be changed when a zocket is
 *            already in a muxer set, and the changes take effect
 *            immediately after execution of the function
 *            @b zf_muxer_mod().
 *
 * @param zf_event    Determines tested zocket and event types:
 *                    - urx.in;
 *                    - zft.in;
 *                    - zft.out;
 *                    - zftl.in.
 * @param non_block   Use zero timeout in @c zf_muxer_wait() if @c TRUE,
 *                    else - @c 1000 milliseconds.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "muxer/mod_events"

#include "zf_test.h"

/* Non-zero timeout for @b zf_muxer_wait(), milliseconds. */
#define MUXER_TIMEOUT   1000

/*
 * Check that @b zf_muxer_wait() terminated successfully
 * returning no events.
 *
 * @param err_msg_      String to append to verdict in case of failure.
 */
#define CHECK_MUXER_WAIT_ZERO(err_msg_) \
    do {                                                              \
        if (rc < 0)                                                   \
            TEST_VERDICT("%s: zf_muxer_wait() failed with errno %r",  \
                         err_msg_, RPC_ERRNO(pco_iut));               \
        else if (rc > 0)                                              \
            TEST_VERDICT("%s: zf_muxer_wait() returned unexpected "   \
                         "events", err_msg_);                         \
    } while (0)

/*
 * Call @b zf_muxer_wait(), check that it blocks if timeout is not zero,
 * then check that it terminates successfully returning no events.
 *
 * @param err_msg_      String to append to verdict in case of failure.
 */
#define CALL_CHECK_MUXER_WAIT_ZERO(err_msg_) \
    do {                                                                  \
        if (!non_block)                                                   \
        {                                                                 \
            pco_iut->op = RCF_RPC_CALL;                                   \
            rpc_zf_muxer_wait(pco_iut, muxer_set,                         \
                              &event, 1, MUXER_TIMEOUT);                  \
                                                                          \
            CHECK_RC(rcf_rpc_server_is_op_done(pco_iut, &done));          \
            if (done)                                                     \
            {                                                             \
                ERROR_VERDICT("%s: zf_muxer_wait() call does not block "  \
                              "when it should", err_msg_);                \
                rpc_zf_muxer_wait(pco_iut, muxer_set,                     \
                                  &event, 1, MUXER_TIMEOUT);              \
                TEST_STOP;                                                \
            }                                                             \
                                                                          \
            RPC_AWAIT_ERROR(pco_iut);                                     \
            rc = rpc_zf_muxer_wait(pco_iut, muxer_set, &event, 1,         \
                                   MUXER_TIMEOUT);                        \
        }                                                                 \
        else                                                              \
        {                                                                 \
            RPC_AWAIT_ERROR(pco_iut);                                     \
            rc = rpc_zf_muxer_wait(pco_iut, muxer_set, &event, 1, 0);     \
        }                                                                 \
                                                                          \
        CHECK_MUXER_WAIT_ZERO(err_msg_);                                  \
    } while (0)

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    const char *zf_event = NULL;
    te_bool     non_block = FALSE;

    rpc_zf_muxer_set_p  muxer_set = RPC_NULL;

    struct rpc_epoll_event    event;
    te_bool                   done = FALSE;

    zfts_mzockets mzockets = {0};

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_STRING_PARAM(zf_event);
    TEST_GET_BOOL_PARAM(non_block);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Create a zocket in dependence on @p zf_event. */
    zfts_create_mzockets(zf_event, pco_iut, attr, stack, iut_addr,
                         pco_tst, tst_addr, &mzockets);

    /*- Allocate a muxer set and add the zocket to it with no events. */
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    zfts_mzockets_add_events(muxer_set, &mzockets, 0);

    /*- Provoke an event in dependence on @p zf_event. */
    zfts_mzockets_prepare_events(&mzockets, zf_event);
    zfts_mzockets_invoke_events(&mzockets);

    /*- Call @b zf_muxer_wait() with a timeout in dependence on @p non_block
     *  - no events should be returned;
     *  - if @p non_block is @c FALSE check that the call is blocked
     *    for a time. */
    RING("Calling zf_muxer_wait() the first time");
    CALL_CHECK_MUXER_WAIT_ZERO("The first call");

    /*- Release the event - read/send data or accept connection. */
    zfts_mzockets_process_events(&mzockets);

    /*- Add the appropriate event for the zocket using @c zf_muxer_mod(). */
    zfts_mzockets_prepare_events(&mzockets, zf_event);
    zfts_mzockets_mod_exp_events(&mzockets);

    /*- Call @c zf_muxer_wait() with zero timeout - no events. */
    RING("Calling zf_muxer_wait() the second time");
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zf_muxer_wait(pco_iut, muxer_set, &event, 1, 0);
    CHECK_MUXER_WAIT_ZERO("The second call");

    /*- Provoke the event and call the muxer in the following order
     *  - if @p non_block is @c FALSE
     *    - start @b zf_muxer_wait() with the timeout;
     *    - ensure it is waiting;
     *    - provoke the event;
     *    - check that @b zf_muxer_wait() is unblocked and returns
     *      the event;
     *  - else
     *    - provoke the event;
     *    - call @b zf_muxer_wait() with zero timeout. */

    if (!non_block)
    {
        pco_iut->op = RCF_RPC_CALL;
        rpc_zf_muxer_wait(pco_iut, muxer_set, &event, 1, MUXER_TIMEOUT);

        CHECK_RC(rcf_rpc_server_is_op_done(pco_iut, &done));
        if (done)
        {
            ERROR_VERDICT("The third call: zf_muxer_wait() call does "
                          "not block when it should");
            rpc_zf_muxer_wait(pco_iut, muxer_set, &event, 1, MUXER_TIMEOUT);
            TEST_STOP;
        }

        zfts_mzockets_invoke_events(&mzockets);
    }
    else
    {
        zfts_mzockets_invoke_events(&mzockets);
        /* When timeout is zero, zf_muxer_wait() may not be able
         * to process incoming event on its own, at least in the
         * case of incoming TCP connection. */
        ZFTS_WAIT_NETWORK(pco_iut, stack);
    }

    /*- The event is returned in the both cases, check it. */
    RING("Calling zf_muxer_wait() the third time");
    zfts_mzockets_check_events(pco_iut, muxer_set,
                               (non_block ? 0 : MUXER_TIMEOUT), FALSE,
                               &mzockets, "The third call");

    /*- Release the event - read/send data or accept connection. */
    zfts_mzockets_process_events(&mzockets);

    /*- Provoke the event. */
    zfts_mzockets_prepare_events(&mzockets, zf_event);
    zfts_mzockets_invoke_events(&mzockets);

    /*- Remove the event expectation for the zocket using
     * @b zf_muxer_mod(). */
    zfts_mzockets_mod_events(&mzockets, 0);

    /*- Call @c zf_muxer_wait() with a timeout in dependence on @p non_block
     *  - no events should be returned;
     *  - if @p non_block is @c FALSE check that the call is blocked
     *    for a time. */
    RING("Calling zf_muxer_wait() the last time");
    CALL_CHECK_MUXER_WAIT_ZERO("The last call");

    /*- Release the event - read/send data or accept connection. */
    zfts_mzockets_process_events(&mzockets);

    TEST_SUCCESS;

cleanup:

    zfts_destroy_mzockets(&mzockets);
    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

