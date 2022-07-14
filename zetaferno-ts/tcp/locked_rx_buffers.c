/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-locked_rx_buffers Buffers are locked until @b zft_zc_recv_done is performed.
 *
 * @objective Receive some data on TCP zocket using @b zft_zc_recv(), but
 *            do not call @b zft_zc_recv_done(). Overfill RX/TX buffers of
 *            the second TCP zocket, send/receive some data using the second
 *            zocket. Check that read buffers of the first zocket are left
 *            untouched after all.
 *
 * @param open_method     How to open connection:
 *                        - @c active
 *                        - @c passive
 *                        - @c passive_close (close listener after
 *                          passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/locked_rx_buffers"

#include "zf_test.h"
#include "rpc_zf.h"

#include "te_dbuf.h"

/* Number of TCP connections used by this test. */
#define CONNS_NUM   2

int
main(int argc, char *argv[])
{
    rcf_rpc_server          *pco_iut = NULL;
    rcf_rpc_server          *pco_tst = NULL;
    const struct sockaddr   *iut_addr = NULL;
    const struct sockaddr   *tst_addr = NULL;

    rpc_zf_attr_p   attr = RPC_NULL;
    rpc_zf_stack_p  stack = RPC_NULL;

    zfts_tcp_conn    *conns = NULL;
    rpc_zftl_p        iut_zftl = RPC_NULL;

    char    data[ZFTS_TCP_DATA_MAX];
    size_t  data_len = 0;
    int     i;

    rpc_iovec      sndiov[ZFTS_IOVCNT] = {{0}};

    rpc_iovec      rcviov_read[ZFTS_IOVCNT] = {{0}};
    rpc_iovec      rcviov_check[ZFTS_IOVCNT] = {{0}};

    rpc_iovec      sndiov2;

    rpc_zft_msg msg_read = {.iovcnt = ZFTS_IOVCNT, .iov = rcviov_read};
    rpc_zft_msg msg_check = {.iovcnt = ZFTS_IOVCNT, .iov = rcviov_check};

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_SET_DEF_RECV_FUNC(recv_func);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    conns = zfts_tcp_conns_alloc(CONNS_NUM);

    /*- Establish two TCP connections according to @p open_method. */
    zfts_tcp_conns_establish2(conns, CONNS_NUM, open_method,
                              pco_iut, attr, stack, &iut_zftl, iut_addr,
                              pco_tst, tst_addr, -1, -1,
                              TRUE, FALSE);

    rpc_make_iov(sndiov, ZFTS_IOVCNT, 1, ZFTS_TCP_DATA_MAX);

    /*- Send from tester a few packets to the first zocket. */
    for (i = 0; i < ZFTS_IOVCNT; i++)
    {
        rc = rpc_send(pco_tst, conns[0].tst_s,
                      sndiov[i].iov_base, sndiov[i].iov_len, 0);
        if (rc != (int)sndiov[i].iov_len)
            TEST_FAIL("send() returned %d instead of %d",
                      rc, sndiov[i].iov_len);
    }

    /*- Process the stack events and receive packets on the first
     * zocket using @b zft_zc_recv(). */
    ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);
    rpc_zft_zc_recv(pco_iut, conns[0].iut_zft, &msg_read, 0);
    if (msg_read.iovcnt != ZFTS_IOVCNT)
        TEST_FAIL("Number of sent packets is different "
                  "from number of received packets");
    rpc_iovec_cmp_strict(sndiov, rcviov_read, msg_read.iovcnt);

    /*- Overfill RX and TX buffers of the second zocket. */

    do {
        data_len = rand_range(1, ZFTS_TCP_DATA_MAX);
        te_fill_buf(data, data_len);
        RPC_AWAIT_IUT_ERROR(pco_tst);
        rc = rpc_send(pco_tst, conns[1].tst_s,
                      data, data_len, RPC_MSG_DONTWAIT);
        if (rc < 0)
        {
            if (RPC_ERRNO(pco_tst) == RPC_EAGAIN)
                break;
            else
                TEST_VERDICT("send() failed with unexpected errno %r "
                             "while trying to overfill zocket receive "
                             "buffer", RPC_ERRNO(pco_tst));
        }

        te_dbuf_append(&conns[1].tst_sent, data, rc);

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_process_events(pco_iut, stack);
        if (rc < 0)
            TEST_VERDICT("rpc_zf_process_events() fails with errno %r "
                         "when test tries to overfill zocket receive "
                         "buffer", RPC_ERRNO(pco_iut));
    } while (TRUE);


    do {
        data_len = rand_range(1, ZFTS_TCP_DATA_MAX);
        te_fill_buf(data, data_len);

        sndiov2.iov_base = data;
        sndiov2.iov_len = sndiov2.iov_rlen = data_len;

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zft_send(pco_iut, conns[1].iut_zft, &sndiov2,
                          1, 0);
        if (rc < 0)
        {
            if (RPC_ERRNO(pco_iut) == RPC_ENOMEM)
                ERROR_VERDICT("zft_send() returned unexpected errno %r "
                              "when trying to overfill zocket send buffer",
                              RPC_ERRNO(pco_iut));
            else if (RPC_ERRNO(pco_iut) != RPC_EAGAIN)
                TEST_VERDICT("zft_send() returned unexpected errno %r "
                             "when trying to overfill zocket send buffer",
                             RPC_ERRNO(pco_iut));

            break;
        }
        te_dbuf_append(&conns[1].iut_sent, data, rc);
    } while (TRUE);

    /*- Completely read all data on the second connection. */
    zfts_tcp_conns_read_check_data_tst(pco_iut, stack,
                                       pco_tst, conns, CONNS_NUM);
    zfts_tcp_conns_read_check_data_iut(pco_iut, stack,
                                       conns, CONNS_NUM);

    /*- Send/receive some more packets using the second connection. */
    zfts_zft_check_connection(pco_iut, stack, conns[1].iut_zft,
                              pco_tst, conns[1].tst_s);

    /*- Check that data which was read on the first zocket stays
     * unchanged. */
    rpc_zft_read_zft_msg(pco_iut, msg_read.ptr, &msg_check);
    if (msg_check.iovcnt != msg_read.iovcnt)
        TEST_VERDICT("Number of IO vectors changed in stored zft_msg");
    rpc_iovec_cmp_strict(sndiov, rcviov_check, msg_check.iovcnt);

    /*- Perform @b rpc_zft_zc_recv_done() to release buffers. */
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zft_zc_recv_done(pco_iut, conns[0].iut_zft, &msg_read);
    if (rc < 0)
        TEST_VERDICT("Final zft_zc_recv_done() failed with errno %r",
                     RPC_ERRNO(pco_iut));

    TEST_SUCCESS;

cleanup:

    rpc_release_iov(sndiov, ZFTS_IOVCNT);
    rpc_release_iov(rcviov_read, ZFTS_IOVCNT);
    rpc_release_iov(rcviov_check, ZFTS_IOVCNT);

    zfts_tcp_conns_destroy(pco_iut, pco_tst, conns, CONNS_NUM);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
