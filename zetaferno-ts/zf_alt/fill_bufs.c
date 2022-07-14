/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-fill_bufs Queue data using alternatives API while send buffer is not empty.
 *
 * @objective Fill send buffer partly or completely and try to queue data
 *            using alternatives API.
 *
 * @param overfill      Fill send buffer completely if @c TRUE.
 * @param queue_first   Queue data to alternatives before buffers
 *                      overfilling if @c TRUE.
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

#define TE_TEST_NAME "zf_alt/fill_bufs"

#include "zf_test.h"
#include "rpc_zf.h"

/**
 * Maximum length of packet to be queued on ZF alernative.
 */
#define MAX_ALT_PKT_LEN 100

/**
 * How many times ioctl(FIONREAD) should return the same
 * value before we will consider receive buffer overfilled.
 */
#define SAME_FIONREAD_CNT 5

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zftl_p  iut_zftl = RPC_NULL;
    rpc_zft_p   iut_zft = RPC_NULL;
    int         tst_s = -1;

    rpc_zf_althandle *alts;
    int               n_alts;
    rpc_iovec        *iovs;
    int               i;
    int               alt_count_max;

    char        send_buf[ZFTS_TCP_DATA_MAX];
    rpc_iovec   send_iov = { .iov_base = send_buf,
                             .iov_len = ZFTS_TCP_DATA_MAX,
                             .iov_rlen = ZFTS_TCP_DATA_MAX };

    te_dbuf           iut_sent = TE_DBUF_INIT(0);
    te_dbuf           tst_received = TE_DBUF_INIT(0);

    int               tst_rcvbuf_prev = 0;
    int               tst_rcvbuf = 0;
    int               tst_rcvbuf_same_cnt = 0;

    te_bool overfill;
    te_bool queue_first;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(overfill);
    TEST_GET_BOOL_PARAM(queue_first);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    CHECK_RC(tapi_sh_env_get_int(pco_iut, "ZFTS_ALT_COUNT_DEF",
                                 &alt_count_max));

    alts = (rpc_zf_althandle *)tapi_calloc(sizeof(*alts), alt_count_max);
    iovs = (rpc_iovec *)tapi_calloc(sizeof(*iovs), alt_count_max);

    n_alts = rand_range(1, alt_count_max);

    for (i = 0; i < n_alts; i++)
    {
        rpc_make_iov(&iovs[i], 1, 1, MAX_ALT_PKT_LEN);
    }

    te_fill_buf(send_buf, ZFTS_TCP_DATA_MAX);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection to get zft zocket. */

    zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                 attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- If @p queue_first is @c TRUE, allocate random number [1; max]
     * of alternatives and queue data to them. */

    if (queue_first)
    {
        for (i = 0; i < n_alts; i++)
        {
            rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alts[i]);
            rpc_zft_alternatives_queue(pco_iut, iut_zft, alts[i],
                                       &iovs[i], 1, 0);
        }
    }

    /*- Send data using @b zft_send(). Send until send buffer is full
     * if @p overfill is @c TRUE; else send data enough to overfill
     * Tester receive buffer and leave some data in IUT send buffer,
     * but not overfill IUT buffer. */

    do {
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_send(pco_iut, iut_zft, &send_iov, 1, 0);
        if (rc < 0)
        {
            if (!overfill)
                TEST_VERDICT("zft_send() failed unexpectedly with errno %r "
                             "before required amount of data was sent",
                             RPC_ERRNO(pco_iut));
            else if (RPC_ERRNO(pco_iut) != RPC_EAGAIN)
                TEST_VERDICT("zft_send() failed with unexpected errno %r",
                             RPC_ERRNO(pco_iut));

            break;
        }
        else if (rc == 0)
        {
            TEST_VERDICT("zft_send() unexpectedly returned zero");
        }

        rpc_zf_process_events(pco_iut, stack);
        te_dbuf_append(&iut_sent, send_iov.iov_base, rc);

        rpc_ioctl(pco_tst, tst_s, RPC_FIONREAD, &tst_rcvbuf);
        if (tst_rcvbuf == tst_rcvbuf_prev && tst_rcvbuf_prev > 0)
            tst_rcvbuf_same_cnt++;
        else
            tst_rcvbuf_same_cnt = 0;

        if (tst_rcvbuf_same_cnt >= SAME_FIONREAD_CNT)
        {
            ZFTS_WAIT_NETWORK(pco_iut, stack);
            rpc_ioctl(pco_tst, tst_s, RPC_FIONREAD, &tst_rcvbuf);

            if (tst_rcvbuf == tst_rcvbuf_prev)
                break;
            else
                tst_rcvbuf_same_cnt = 0;
        }
        tst_rcvbuf_prev = tst_rcvbuf;
    } while (TRUE);

    RING("%u bytes were sent", (unsigned int)iut_sent.len);

    /*- If @p queue_first is @c FALSE:
     *  - allocate random number [1; max] of alternatives;
     *  - try to queue data using @b zft_alternatives_queue(),
     *    the call should fail because send buffer is not empty. */

    if (!queue_first)
    {
        for (i = 0; i < n_alts; i++)
        {
            rpc_zf_alternatives_alloc(pco_iut, stack, attr, &alts[i]);
        }

        for (i = 0; i < n_alts; i++)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zft_alternatives_queue(pco_iut, iut_zft, alts[i],
                                            &iovs[i], 1, 0);
            if (rc < 0)
            {
                if (RPC_ERRNO(pco_iut) != RPC_EAGAIN)
                    TEST_VERDICT("zft_alternatives_queue() failed "
                                 "unexpectedly with errno %r "
                                 "when send buffer is not empty",
                                 RPC_ERRNO(pco_iut));
            }
            else
            {
                TEST_VERDICT("zft_alternatives_queue() succeeds "
                             "when send buffer is not empty");
            }
        }
    }

    /*- If @p queue_first is @c TRUE, try to send data previously
     * queued on alternative queues. @b zf_alternatives_send()
     * should fail with @c EINVAL. */

    if (queue_first)
    {
        for (i = 0; i < n_alts; i++)
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zf_alternatives_send(pco_iut, stack, alts[i]);
            if (rc < 0)
            {
                if (RPC_ERRNO(pco_iut) != RPC_EINVAL)
                    TEST_VERDICT("zf_alternatives_send() unexpectedly "
                                 "failed with errno %r",
                                 RPC_ERRNO(pco_iut));
            }
            else
            {
                TEST_VERDICT("zf_alternatives_send() succeeded");
            }

            rpc_zf_process_events(pco_iut, stack);
        }
    }

    /*- Read and check data on tester. */

    zfts_zft_peer_read_all(pco_iut, stack, pco_tst, tst_s,
                           &tst_received);

    ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr, iut_sent.ptr,
                             tst_received.len, iut_sent.len,
                             " from IUT");

    TEST_SUCCESS;

cleanup:

    for (i = 0; i < n_alts; i++)
    {
        CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack, alts[i]);
        rpc_release_iov(&iovs[i], 1);
    }

    te_dbuf_free(&iut_sent);
    te_dbuf_free(&tst_received);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
