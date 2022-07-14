/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-no_ack ACK does not come in time.
 *
 * @objective TCP alternatives API if ACK does not come in reply to
 *            data packets in time.
 *
 * @param status        Connection status:
 *                      - refused (RST is sent from tester)
 *                      - timeout (data transmission is aborted by
 *                        retransmits)
 *                      - delayed (data is retransmitted when connection
 *                        is fixed)
 * @param one_packet    Send one packet if @c TRUE, else send a few packets.
 * @param delay         Delay between send operations in milliseconds,
 *                      iterate only if @p one_packet is @c FALSE:
 *                      - @c 0
 *                      - @c 100
 * @param open_method   How to open connection:
 *                      - @c active
 *                      - @c passive
 *                      - @c passive_close (close listener after
 *                        passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "zf_alt/no_ack"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_route_gw.h"
#include "tapi_proc.h"

/** Number of iterations after which to stop infinite loop. */
#define MAX_LOOP_ITERS 1000000L

/** Retransmits number. */
#define RETRIES_NUM 3

int
main(int argc, char *argv[])
{
    TAPI_DECLARE_ROUTE_GATEWAY_PARAMS;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p     iut_zftl = RPC_NULL;
    rpc_zft_p     iut_zft = RPC_NULL;
    int           tst_s = -1;

    zfts_conn_problem status;
    te_bool           one_packet;
    int               delay;

    rpc_iovec         iov;
    rpc_zf_althandle  alt;
    te_dbuf           iut_sent = TE_DBUF_INIT(0);
    te_dbuf           tst_received = TE_DBUF_INIT(0);
    long int          i;

    tarpc_linger      optval;

    tapi_route_gateway gateway;

    zfts_conn_open_method open_method;

    TEST_START;
    TAPI_GET_ROUTE_GATEWAY_PARAMS;
    TEST_GET_ENUM_PARAM(status, ZFTS_CONN_PROBLEMS);
    TEST_GET_BOOL_PARAM(one_packet);
    TEST_GET_INT_PARAM(delay);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    if (status == ZFTS_CONN_TIMEOUT)
        CHECK_RC(tapi_cfg_sys_ns_set_int(pco_iut->ta, RETRIES_NUM, NULL,
                                         "net/ipv4/tcp_retries2"));

    rpc_make_iov(&iov, 1, ZFTS_TCP_DATA_MAX, ZFTS_TCP_DATA_MAX);

    /*- Configure gateway connecting IUT and Tester. */

    TAPI_INIT_ROUTE_GATEWAY(gateway);
    tapi_route_gateway_configure(&gateway);
    CFG_WAIT_CHANGES;

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate ZF alternative queue. */
    rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alt);

    /*- Establish TCP connection to get zft zocket. */
    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- Break connection between IUT and tester using gateway. */
    tapi_route_gateway_break_tst_gw(&gateway);
    TAPI_WAIT_NETWORK;

    /*- Send data using alternatives API:
     *  - send single packet if @p one_packet is @c TRUE;
     *  - else send packes until send operation is failed
     *    (it should fail because ACKs do not come);
     *  - use delay @p delay between calls. */

    i = 0;
    do {
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alt,
                                        &iov, 1, 0);
        if (rc < 0)
        {
            if (one_packet || RPC_ERRNO(pco_iut) != RPC_EAGAIN)
                TEST_VERDICT("zft_alternatives_queue() unexpectedly "
                             "failed with errno %r", RPC_ERRNO(pco_iut));

            break;
        }

        rpc_zf_alternatives_send(pco_iut, stack, alt);
        rpc_zf_process_events(pco_iut, stack);
        te_dbuf_append(&iut_sent, iov.iov_base, iov.iov_len);

        if (one_packet)
            break;

        if (delay > 0)
            MSLEEP(delay);

        i++;
        if (i > MAX_LOOP_ITERS)
            TEST_FAIL("Too many iterations in loop");
    } while (TRUE);

    switch (status)
    {
        case ZFTS_CONN_REFUSED:

            /*- If @p status is @c refused, abort connection on
             * Tester using zero linger. */

            optval.l_onoff = 1;
            optval.l_linger = 0;
            rpc_setsockopt(pco_tst, tst_s, RPC_SO_LINGER, &optval);

            RPC_CLOSE(pco_tst, tst_s);

            break;

        case ZFTS_CONN_TIMEOUT:

            /*- If @p status is @c timeout, wait until connection
             * is dropped by timeout. */
            zfts_wait_for_final_tcp_state_ext(pco_iut, stack, iut_zft,
                                              FALSE, TRUE);

            break;

        case ZFTS_CONN_DELAYED:

            /*- If @status is @c delayed, sleep @c 0.5 second. */
            TAPI_WAIT_NETWORK;
            break;
    }

    /*- Repair connection between IUT and tester. */
    tapi_route_gateway_repair_tst_gw(&gateway);

    /*- If @status is not @c timeout, process events for @c 1 second. */
    if (status != ZFTS_CONN_TIMEOUT)
        rpc_zf_process_events_long(pco_iut, stack, 1000);

    /*- Try to send data using alternatives API
     *  - if @status is @c timeout or @c refused, function
     *    @c zft_alternatives_queue() should fail
     *    - check returned error code;
     *    - get and check error value @c zft_error().
     *  - Else send should succeed. */

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alt,
                                    &iov, 1, 0);
    if (rc < 0)
    {
        if (status == ZFTS_CONN_DELAYED ||
            RPC_ERRNO(pco_iut) != RPC_ENOTCONN)
            TEST_VERDICT("Final zft_alternatives_queue() call unexpectedly "
                         "failed with errno %r", RPC_ERRNO(pco_iut));

        rc = rpc_zft_error(pco_iut, iut_zft);
        if ((status == ZFTS_CONN_REFUSED && rc != RPC_ECONNRESET) ||
            (status == ZFTS_CONN_TIMEOUT && rc != RPC_ETIMEDOUT))
            TEST_VERDICT("zft_error() returned unexpected error %r", rc);
    }
    else
    {
        if (status != ZFTS_CONN_DELAYED)
            TEST_VERDICT("Finall zft_alternatives_queue() call succeeded "
                         "for closed connection");

        rpc_zf_alternatives_send(pco_iut, stack, alt);
        te_dbuf_append(&iut_sent, iov.iov_base, iov.iov_len);
    }

    /*- If @status is @c delayed, read and check data on tester. */

    if (status == ZFTS_CONN_DELAYED)
    {
        zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s,
                               &tst_received);

        ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr, iut_sent.ptr,
                                 tst_received.len, iut_sent.len,
                                 " from IUT");
    }

    TEST_SUCCESS;

cleanup:

    te_dbuf_free(&iut_sent);
    te_dbuf_free(&tst_received);
    rpc_release_iov(&iov, 1);

    CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alt);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
