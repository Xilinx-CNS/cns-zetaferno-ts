/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-share_events_queue Events queue sharing RX/TX.
 *
 * @objective Establish a few TCP connections using single stack, send
 *            data to and from zockets in random order, check that data
 *            is delivered to proper IUT zockets and tester sockets and
 *            no data is lost.
 *
 * @param open_method     How to open connection:
 *                        - @c active
 *                        - @c passive
 *                        - @c passive_close (close listener after
 *                          passively establishing connection)
 * @param zft_num         Connections number to be established:
 *                        - @c 10
 * @param data_size_min   Minimum data size to be sent at a time to
 *                        a zocket. Use in couples with
 *                        @p data_size_max:
 *                        - @c 1
 *                        - @c 500
 *                        - @c 1000
 * @param data_size_max   Maximum data size to be sent at a time to a zocket:
 *                        - @c 500
 *                        - @c 1000
 *                        - @c 2000
 * @param bunches_num     Total packets bunches number:
 *                        - @c 100
 * @param bunch_size      Packets number to be sent in one bunch:
 *                        - @c 50 (can be tuned in dependence on
 *                        sent data volume)
 * @param send_from       Who sends data:
 *                        - IUT
 *                        - TESTER
 *                        - RANDOM
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/share_events_queue"

#include "zf_test.h"
#include "rpc_zf.h"

#include "te_dbuf.h"

/**
 * Maximum length of data to be sent through connection
 * (used to avoid buffers overfilling).
 */
#define MAX_BYTES_PER_CONN  4000

/**
 * Maximum number of attempts to select connection for which
 * sent data size limit is not reached yet.
 */
#define MAX_CONN_SELECT_ATTEMPTS 1000000

/** Disable/enable RPC logging. */
#define VERBOSE_LOGGING FALSE

/** Sender type. */
typedef enum {
    ZFTS_TCP_SENDER_IUT = 0,  /**< Send data from IUT. */
    ZFTS_TCP_SENDER_TESTER,   /**< Send data from TESTER. */
    ZFTS_TCP_SENDER_RANDOM,   /**< Choose randomly whether
                                   to send from IUT or from TESTER. */
} zfts_tcp_sender_rpcs;

/** Correspondence between sender value and string representation
 * used for getting test parameter value. */
#define ZFTS_TCP_SENDER_RPCS \
    { "IUT",        ZFTS_TCP_SENDER_IUT }, \
    { "TESTER",     ZFTS_TCP_SENDER_TESTER }, \
    { "RANDOM",     ZFTS_TCP_SENDER_RANDOM }

/**
 * Get string representation of sender parameter value
 * for logging purposes.
 *
 * @param sender      Sender
 *
 * @return String representation of value or @c "UNKNOWN".
 */
const char *
sender2str(zfts_tcp_sender_rpcs sender)
{
    if (sender == ZFTS_TCP_SENDER_IUT)
        return "IUT";
    else if (sender == ZFTS_TCP_SENDER_TESTER)
        return "TESTER";
    else
        return "UNKNOWN";
}

/**
 * Send data through a given TCP connection.
 *
 * @param conn        TCP connection description.
 * @param pco_iut     IUT RPC server.
 * @param pco_tst     TESTER RPC server.
 * @param send_from   From which peer to send data.
 * @param data        Data to send.
 * @param data_len    Length of data.
 * @param silent      Disable RPC logging.
 */
static void
tcp_conn_send(zfts_tcp_conn *conn,
              rcf_rpc_server *pco_iut,
              rcf_rpc_server *pco_tst,
              zfts_tcp_sender_rpcs send_from,
              char *data, size_t data_len,
              te_bool silent)
{
    int rc;

    if (send_from == ZFTS_TCP_SENDER_RANDOM)
    {
        if (rand() % 2 == 0)
            send_from = ZFTS_TCP_SENDER_IUT;
        else
            send_from = ZFTS_TCP_SENDER_TESTER;
    }

    if (send_from == ZFTS_TCP_SENDER_IUT)
    {
        rpc_iovec sndiov;

        sndiov.iov_base = data;
        sndiov.iov_len = sndiov.iov_rlen = data_len;

        RPC_AWAIT_ERROR(pco_iut);
        pco_iut->silent_pass = silent;
        rc = rpc_zft_send(pco_iut, conn->iut_zft, &sndiov, 1, 0);
        if (rc < 0)
            TEST_VERDICT("zft_send() failed with errno %r",
                         RPC_ERRNO(pco_iut));
        te_dbuf_append(&conn->iut_sent, data, rc);
    }
    else
    {
        RPC_AWAIT_ERROR(pco_tst);
        pco_tst->silent_pass = silent;
        rc = rpc_send(pco_tst, conn->tst_s, data, data_len, 0);
        if (rc < 0)
            TEST_VERDICT("send() failed with errno %r", RPC_ERRNO(pco_tst));
        else if (rc != (int)data_len)
            TEST_FAIL("send() returned %d instead of %d",
                      rc, (int)data_len);

        te_dbuf_append(&conn->tst_sent, data, data_len);
    }
}

/**
 * Receive all the data through a given
 * TCP connection sent from specified sender and
 * check that received data matches data which was sent.
 *
 * @param conn        TCP connection description.
 * @param pco_iut     IUT RPC server.
 * @param pco_tst     TESTER RPC server.
 * @param sender      Whether data was sent from IUT or from TESTER.
 * @param sent_buf    Data which was sent, to be compared with that
 *                    which was received.
 */
static void
check_receive(zfts_tcp_conn *conn,
              rcf_rpc_server *pco_iut,
              rcf_rpc_server *pco_tst,
              zfts_tcp_sender_rpcs sender,
              te_dbuf *sent_buf)
{
    te_dbuf received_buf = TE_DBUF_INIT(0);

    if (sender == ZFTS_TCP_SENDER_TESTER)
    {
        zfts_zft_read_data(pco_iut, conn->iut_zft, &received_buf,
                           NULL);
    }
    else
    {
        rpc_read_fd2te_dbuf(pco_tst, conn->tst_s, 0, sent_buf->len,
                            &received_buf);
    }

    ZFTS_CHECK_RECEIVED_DATA(received_buf.ptr, sent_buf->ptr,
                             received_buf.len, sent_buf->len,
                             " from %s", sender2str(sender));

    te_dbuf_free(&received_buf);
}

/**
 * Receive data previously sent through a given TCP
 * connection and check that received data matches
 * data which was sent.
 *
 * @param conn        TCP connection description.
 * @param pco_iut     IUT RPC server.
 * @param pco_tst     TESTER RPC server.
 */
static void
tcp_conn_receive(zfts_tcp_conn *conn,
                 rcf_rpc_server *pco_iut,
                 rcf_rpc_server *pco_tst)
{
    if (conn->iut_sent.len > 0)
        check_receive(conn, pco_iut, pco_tst,
                      ZFTS_TCP_SENDER_IUT,
                      &conn->iut_sent);

    if (conn->tst_sent.len > 0)
        check_receive(conn, pco_iut, pco_tst,
                      ZFTS_TCP_SENDER_TESTER,
                      &conn->tst_sent);
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    zfts_tcp_conn    *conns = NULL;
    rpc_zftl_p        iut_zftl = RPC_NULL;

    int       zft_num = 0;
    int       i;
    int       j;
    int       k;
    int       l;

    int   data_size_min = 0;
    int   data_size_max = 0;
    int   bunches_num = 0;
    int   bunch_size = 0;

    ssize_t bunch_bytes_limit = 0;

    zfts_conn_open_method   open_method;
    zfts_tcp_sender_rpcs    send_from;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_INT_PARAM(zft_num);
    TEST_GET_INT_PARAM(data_size_min);
    TEST_GET_INT_PARAM(data_size_max);
    TEST_GET_INT_PARAM(bunches_num);
    TEST_GET_INT_PARAM(bunch_size);
    TEST_GET_ENUM_PARAM(send_from, ZFTS_TCP_SENDER_RPCS);
    TEST_GET_SET_DEF_RECV_FUNC(recv_func);

    bunch_bytes_limit = ((int)(MAX_BYTES_PER_CONN / data_size_max)) *
                        data_size_max * zft_num;
    if (bunch_size * data_size_max > bunch_bytes_limit)
        TEST_FAIL("Too much data may be transmitted in bunch");

    conns = zfts_tcp_conns_alloc(zft_num);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish @p zft_num TCP connections actively or passively
     * according to @p open_method. */

    zfts_tcp_conns_establish2(conns, zft_num, open_method,
                              pco_iut, attr, stack, &iut_zftl, iut_addr,
                              pco_tst, tst_addr, -1, -1,
                              TRUE, FALSE);

    /*- Repeat in the loop @p bunches_num times
     *  - send packets bunch
     *    - from tester or IUT socket/zocket in dependence on @p send_from;
     *    - packets number is equal to @p bunch_size;
     *    - each send() call is done using random connection;
     *  - call @a rpc_zf_process_events();
     *  - read all data on zockets or sockets, check received data is equal
     *    to sent. */

    for (j = 0; j < bunches_num; j++)
    {
        size_t total_sent = 0;

        RING("Sending bunch %d", j + 1);

        for (k = 0; k < bunch_size; k++)
        {
            char   *data;
            size_t  data_len;

            data = te_make_buf(data_size_min, data_size_max, &data_len);

            l = 0;
            while (TRUE)
            {
                i = rand_range(0, zft_num - 1);
                if (conns[i].iut_sent.len + conns[i].tst_sent.len <=
                    MAX_BYTES_PER_CONN - data_len)
                    break;
                l++;

                if (l >= MAX_CONN_SELECT_ATTEMPTS)
                    TEST_FAIL("Failed to choose connection for "
                              "which bytes per connection limit is not "
                              "reached yet");
            }

            total_sent += data_len;
            tcp_conn_send(&conns[i], pco_iut, pco_tst, send_from,
                          data, data_len, !VERBOSE_LOGGING);
            free(data);
        }
        RING("%"TE_PRINTF_SIZE_T"u bytes were sent via all connections",
             total_sent);

        ZFTS_WAIT_NETWORK(pco_iut, stack);

        for (i = 0; i < zft_num; i++)
        {
            tcp_conn_receive(&conns[i], pco_iut, pco_tst);

            te_dbuf_reset(&conns[i].iut_sent);
            te_dbuf_reset(&conns[i].tst_sent);
        }
    }

    TEST_SUCCESS;

cleanup:

    zfts_tcp_conns_destroy(pco_iut, pco_tst, conns, zft_num);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
