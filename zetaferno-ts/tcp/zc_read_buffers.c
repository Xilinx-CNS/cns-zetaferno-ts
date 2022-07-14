/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-zc_read_buffers Use various buffers number (iovcnt) in random order in zero-copy read.
 *
 * @objective Read packets using various buffers number (iovcnt)
 *            in random order a lot of times, check for resources leakage.
 *
 * @param open_method     How to open connection:
 *                        - @c active
 *                        - @c passive
 *                        - @c passive_close (close listener after
 *                          passively establishing connection)
 * @param bunches_num     Total packets bunches number:
 *                        - @c 100
 * @param bunch_size      Packets number to be sent in a one bunch:
 *                        - @c 50
 * @param iovcnt          Iov vectors number to receive packets or @c -1
 *                        to use random value in range [1, bunch_size + 10]
 *                        for each bunch:
 *                        - @c 1
 *                        - @c 5
 *                        - @c 20
 *                        - @c 50
 *                        - @c -1 (range [1, @b bunch_size + 10])
 * @param recv_done_some  If @c TRUE, call @b zft_zc_recv_done_some()
 *                        instead of @b zft_zc_recv_done().
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/zc_read_buffers"

#include "zf_test.h"
#include "rpc_zf.h"

#include "te_dbuf.h"

/* Maximum length of packet. */
#define MAX_PKT_LEN  100

/* Maximum number of IO vectors in zft_msg. */
#define MAX_IOVCNT 100

/* Number that should be added to @p bunch_size
 * to get upper limit on number of IO vectors
 * if we choose it randomly. */
#define MAX_ADD_IOVCNT 10

int
main(int argc, char *argv[])
{
    rcf_rpc_server          *pco_iut = NULL;
    rcf_rpc_server          *pco_tst = NULL;
    const struct sockaddr   *iut_addr = NULL;
    const struct sockaddr   *tst_addr = NULL;

    rpc_zf_attr_p   attr = RPC_NULL;
    rpc_zf_stack_p  stack = RPC_NULL;

    int         tst_s = -1;
    rpc_zft_p   iut_zft = RPC_NULL;
    rpc_zft_p   iut_zftl = RPC_NULL;

    zfts_conn_open_method open_method;

    int       bunches_num = 0;
    int       bunch_size = 0;
    int       iovcnt = 0;
    te_bool   recv_done_some = FALSE;

    char    data[MAX_PKT_LEN];
    size_t  data_len;
    int     val = 0;

    long unsigned int total_sent = 0;

    te_dbuf sent_data = TE_DBUF_INIT(0);
    te_dbuf received_data = TE_DBUF_INIT(0);

    rpc_iovec     rcv_iov[MAX_IOVCNT] = {{0}};
    rpc_zft_msg   msg = { .iovcnt = MAX_IOVCNT, .iov = rcv_iov };

    te_bool       no_data;

    int i;
    int j;
    int k;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_INT_PARAM(bunches_num);
    TEST_GET_INT_PARAM(bunch_size);
    TEST_GET_INT_PARAM(iovcnt);
    TEST_GET_BOOL_PARAM(recv_done_some);

    if (iovcnt > MAX_IOVCNT ||
        (iovcnt < 0 && bunch_size + MAX_ADD_IOVCNT > MAX_IOVCNT))
        TEST_FAIL("Maximum number of IOV is too big");

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection according to @p open_method. */

    zfts_establish_tcp_conn_ext2(open_method,
                                 pco_iut, attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /* This is done to be sure that each @a send() call results
     * in sending new packet. */
    val = 1;
    rpc_setsockopt(pco_tst, tst_s, RPC_TCP_NODELAY, &val);

    /*- Repeat in the loop @p bunches_num times
     *  - send from tester @p bunch_size packets to the zocket;
     *  - process all events with @a rpc_zf_process_events();
     *  - read all packets:
     *    - use parameter value @p iovcnt as argument @b zft_msg.iovcnt;
     *    - or use random value in range [1, bunch_size + 10]
     *      if @p iovcnt is @c -1;
     *  - check data. */

    for (i = 0; i < bunches_num; i++)
    {
        te_dbuf_reset(&sent_data);

        RING("Sending bunch %d", i + 1);

        for (j = 0; j < bunch_size; j++)
        {
            data_len = rand_range(1, MAX_PKT_LEN);
            te_fill_buf(data, data_len);

            RPC_AWAIT_ERROR(pco_tst);
            rc = rpc_send(pco_tst, tst_s, data, data_len, 0);
            if (rc < 0)
            {
                RING("send() fails after sending "
                     "%"TE_PRINTF_SIZE_T"u bytes",
                     sent_data.len);
                TEST_VERDICT("send() failed with errno %r",
                             RPC_ERRNO(pco_tst));
            }
            else if (rc != (int)data_len)
            {
                TEST_FAIL("Failed to send %"TE_PRINTF_SIZE_T"u bytes",
                          data_len);
            }

            te_dbuf_append(&sent_data, data, data_len);
        }

        total_sent += sent_data.len;
        RING("%"TE_PRINTF_SIZE_T"u bytes were sent, "
             "total amount of data sent: %lu",
             sent_data.len, total_sent);

        ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);

        te_dbuf_reset(&received_data);
        no_data = FALSE;
        do {
            unsigned int recv_len;

            msg.iovcnt = (iovcnt > 0 ?
                            iovcnt :
                            rand_range(1, bunch_size + MAX_ADD_IOVCNT));

            rpc_zft_zc_recv(pco_iut, iut_zft, &msg, 0);
            if (!RPC_IS_CALL_OK(pco_iut) || msg.iovcnt < 0)
                TEST_VERDICT("zft_zc_recv() failed with errno %r",
                             RPC_ERRNO(pco_iut));

            if (msg.iovcnt <= 0)
            {
                if (no_data || received_data.len >= sent_data.len)
                    break;

                no_data = TRUE;
                ZFTS_WAIT_NETWORK(pco_iut, stack);
                continue;
            }
            no_data = FALSE;

            recv_len = rpc_iov_data_len(rcv_iov, msg.iovcnt);

            if (recv_done_some)
            {
                if (recv_len > 1)
                    recv_len -= rand_range(1, recv_len / 2);
                rpc_zft_zc_recv_done_some(pco_iut, iut_zft, &msg, recv_len);
            }
            else
            {
                rpc_zft_zc_recv_done(pco_iut, iut_zft, &msg);
            }

            for (k = 0; k < msg.iovcnt; k++)
            {
                te_dbuf_append(&received_data, rcv_iov[k].iov_base,
                               (rcv_iov[k].iov_len > recv_len ?
                                     recv_len : rcv_iov[k].iov_len));

                if (recv_len <= rcv_iov[k].iov_len)
                    break;
                recv_len -= rcv_iov[k].iov_len;
            }

            rpc_release_iov(rcv_iov, msg.iovcnt);
            rpc_zf_process_events(pco_iut, stack);
        } while (TRUE);

        RING("%"TE_PRINTF_SIZE_T"u bytes were received",
             received_data.len);

        ZFTS_CHECK_RECEIVED_DATA(received_data.ptr, sent_data.ptr,
                                 received_data.len, sent_data.len, "");
    }

    TEST_SUCCESS;

cleanup:

    te_dbuf_free(&sent_data);
    te_dbuf_free(&received_data);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

