/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-zft_send_space Check send buffer size using zft_send_space()
 *
 * @objective Check send buffer size using zft_send_space() while it is
 *            gradually filled.
 *
 * @param open_method     How to open connection:
 *                        - @c active
 *                        - @c passive
 *                        - @c passive_close (close listener just after
 *                          passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/zft_send_space"

#include "zf_test.h"
#include "rpc_zf.h"

#include "te_dbuf.h"

/** How long to wait after sending or receiving data (milliseconds). */
#define SEND_RECV_TIMEOUT 100

/** Size of Tester receive buffer. */
#define TST_RCVBUF_SIZE 50000

/** Size of data bunch to send by a one call. */
#define DATA_BUNCH_SEND 5000

/** Size of data bunch to read by a one call. */
#define DATA_BUNCH_RECV 15000

int
main(int argc, char *argv[])
{
    rcf_rpc_server          *pco_iut = NULL;
    rcf_rpc_server          *pco_tst = NULL;
    const struct sockaddr   *iut_addr = NULL;
    const struct sockaddr   *tst_addr = NULL;

    rpc_zf_attr_p   attr = RPC_NULL;
    rpc_zf_stack_p  stack = RPC_NULL;

    char buf[DATA_BUNCH_RECV];

    rpc_zft_p           iut_zft = RPC_NULL;
    rpc_zftl_p          iut_zftl = RPC_NULL;
    int                 tst_s = -1;
    int                 tst_rcvbuf = 0;
    int                 tst_rcvbuf_prev = 0;
    size_t              send_space = 0;
    size_t              send_space_prev = 0;
    size_t              send_space_init = 0;
    int                 send_rc;

    te_dbuf send_data = TE_DBUF_INIT(0);
    te_dbuf recv_data = TE_DBUF_INIT(0);

    te_bool not_decr_verdict = FALSE;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    te_fill_buf(buf, DATA_BUNCH_RECV);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection according to @p open_method.
     * Tune tester receive buffer size to avoid extra iterations. */
    zfts_establish_tcp_conn_ext2(open_method,
                                 pco_iut, attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, TST_RCVBUF_SIZE);

    /*- Get initial value returned by @b zft_send_space(). */
    rpc_zft_send_space(pco_iut, iut_zft, &send_space_init);

    /*- In a loop until send buffer is full: */

    while (TRUE)
    {
        /*-- Send a data packet; */

        RPC_AWAIT_ERROR(pco_iut);
        send_rc = zfts_zft_send(pco_iut, iut_zft, buf, DATA_BUNCH_SEND);
        if (send_rc < 0)
        {
            if (RPC_ERRNO(pco_iut) == RPC_EAGAIN)
                break;
            else
                TEST_VERDICT("zft_send() failed with unexpected errno %r",
                             RPC_ERRNO(pco_iut));
        }
        else if (send_rc == 0)
        {
            TEST_VERDICT("zft_send() returned zero");
        }

        /*-- Process events; */
        rpc_zf_process_events_long(pco_iut, stack, SEND_RECV_TIMEOUT);

        /*-- Get amount of data in receive buffer of the tester socket; */
        rpc_ioctl(pco_tst, tst_s, RPC_FIONREAD, &tst_rcvbuf);

        /*-- Get ZFT send buffer size with @b zft_send_space(); */
        rpc_zft_send_space(pco_iut, iut_zft, &send_space);

        /*-- Check that if receive buffer of the tester socket is full
         * (data amount in it did not change), IUT send buffer size
         * decreases on each iteration; otherwise it should remain
         * approximately the same as in the previous iteration. */
        if (send_data.len > 0)
        {
            if (send_space > send_space_prev)
                TEST_VERDICT("zft_send_space() returned increased "
                             "value after sending more data");

            if (tst_rcvbuf < tst_rcvbuf_prev)
            {
                TEST_FAIL("Less data is reported in Tester receive buffer "
                          "after sending more data");
            }
            else if (tst_rcvbuf == tst_rcvbuf_prev + DATA_BUNCH_SEND)
            {
                if (send_space < send_space_prev)
                    TEST_VERDICT("zft_send_space() returned decreased "
                                 "value while peer receive buffer is not "
                                 "overfilled yet");
            }
            else
            {
                if (send_space == send_space_prev &&
                    !not_decr_verdict)
                {
                    ERROR_VERDICT("zft_send_space() returned the same "
                                  "value after sending data while peer "
                                  "receive buffer is overfilled");
                    not_decr_verdict = TRUE;
                }
            }
        }

        send_space_prev = send_space;
        tst_rcvbuf_prev = tst_rcvbuf;
        te_dbuf_append(&send_data, buf, send_rc);
    }

    /*- In a loop read data from Tester socket until no data is left;
     * check that after reading some data value returned by
     * @b zft_send_space() always increases. */
    while (TRUE)
    {
        RPC_AWAIT_ERROR(pco_tst);
        rc = rpc_recv(pco_tst, tst_s, buf,
                      DATA_BUNCH_RECV, RPC_MSG_DONTWAIT);
        if (rc < 0)
        {
            if (RPC_ERRNO(pco_tst) == RPC_EAGAIN)
                break;
            else
                TEST_VERDICT("recv() failed with unexpected errno %r",
                             RPC_ERRNO(pco_tst));
        }
        else if (rc == 0)
        {
            TEST_VERDICT("recv() returned zero");
        }

        te_dbuf_append(&recv_data, buf, rc);
        rpc_zf_process_events_long(pco_iut, stack, SEND_RECV_TIMEOUT);

        rpc_zft_send_space(pco_iut, iut_zft, &send_space);

        if (send_space < send_space_prev)
            TEST_VERDICT("zft_send_space() returned decreased "
                         "value after receiving some data on peer");
        send_space_prev = send_space;
    }

    /*- Check that after reading all the data from Tester
     * @b zft_send_space() returns the same value as at the beginning. */

    if (send_space < send_space_init)
        TEST_VERDICT("After reading all data from peer value "
                     "returned by zft_send_space() has not "
                     "fully recovered");
    else if (send_space > send_space_init)
        TEST_VERDICT("After reading all data from peer value "
                     "returned by zft_send_space() is greater "
                     "than just after zocket creation");

    /*- Check data received on Tester. */
    ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, send_data.ptr,
                             recv_data.len, send_data.len,
                             " from IUT");

    if (not_decr_verdict)
        TEST_STOP;

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);

    te_dbuf_free(&send_data);
    te_dbuf_free(&recv_data);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
