/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 *
 * $Id$
 */

/**
 * @page tcp-send_buffer_overfilling Examine send buffers overfilling.
 *
 * @objective Transmit data from TCP zockets until send buffers
 *            are overfilled, read and check all the data. Examine
 *            maximum buffer size of individual zocket and common
 *            stack buffer.
 *
 * @param open_method     How to open connection:
 *                        - @c active
 *                        - @c passive
 *                        - @c passive_close (close listener after
 *                          passively establishing connection)
 * @param zft_num         Connections number to be established:
 *                        - @c 10
 *                        - @c 20
 * @param gradual         If zockets buffers should be overfilled
 *                        successively or gradually all together.
 * @param small_packet    If @c TRUE, send packets of small size.
 * @param large_n_bufs    If @c TRUE, set @b n_bufs attribute to
 *                        a large value.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/send_buffer_overfilling"

#include "zf_test.h"
#include "rpc_zf.h"

#include "te_dbuf.h"

/** Maximum number of a packet. */
#define MAX_PKT_LEN         ZFTS_TCP_DATA_MAX

/** Maximum number of a small packet. */
#define MAX_SMALL_PKT_LEN   100

/** How many times try to send more data if send call fails. */
#define EXTRA_ATTEMPTS 5

/** How long wait and process events between attempts to send data after
 * send call fail, milliseconds. */
#define WAITING_TIME 100

/**
 * Value of @b n_bufs attribute to be set if
 * @p large_n_bufs is @c TRUE.
 */
#define LARGE_N_BUFS 10000

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
    size_t            send_space1 = 0;
    size_t            send_space2 = 0;
    te_bool           init_space_verdict = FALSE;
    te_bool           final_space_verdict = FALSE;
    rpc_zftl_p        iut_zftl = RPC_NULL;

    char    data[MAX_PKT_LEN];
    size_t  data_len = 0;
    size_t  max_len = 0;
    int     i = 0;
    int     j = 0;
    int     next_i = 0;

    rpc_iovec   sndiov;

    te_bool enomem_verdict = FALSE;

    zfts_conn_open_method open_method;

    int     zft_num = 0;
    te_bool gradual = FALSE;
    te_bool small_packet = FALSE;
    te_bool large_n_bufs = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_INT_PARAM(zft_num);
    TEST_GET_BOOL_PARAM(gradual);
    TEST_GET_BOOL_PARAM(small_packet);
    TEST_GET_BOOL_PARAM(large_n_bufs);

    if (small_packet)
        max_len = MAX_SMALL_PKT_LEN;
    else
        max_len = MAX_PKT_LEN;

    /*- Allocate Zetaferno attributes and stack.
     * Set @b n_bufs attribute to @c LARGE_N_BUFS
     * if @p large_n_bufs is TRUE. */

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    if (large_n_bufs)
        rpc_zf_attr_set_int(pco_iut, attr, "n_bufs", LARGE_N_BUFS);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    conns = zfts_tcp_conns_alloc(zft_num);

    /*- Establish @p zft_num TCP connections according to @p open_method. */
    /*- If @p small_packet is @c TRUE - tune tester receive
     * buffers to a small value. */

    zfts_tcp_conns_establish2(conns, zft_num, open_method,
                              pco_iut, attr, stack, &iut_zftl, iut_addr,
                              pco_tst, tst_addr, -1,
                              (small_packet ? 1 : -1),
                              TRUE, FALSE);

    /*- Check that @b zft_send_space() returns the same value for
     * any ZFT zocket just after creation. */
    rpc_zft_send_space(pco_iut, conns[0].iut_zft, &send_space1);
    for (i = 1; i < zft_num; i++)
    {
        rpc_zft_send_space(pco_iut, conns[i].iut_zft, &send_space2);
        if (send_space2 != send_space1 && !init_space_verdict)
        {
            ERROR_VERDICT("zft_send_space() returns different values "
                          "for different ZFT zockets just after creation");
            init_space_verdict = TRUE;
        }
    }

    /*- Send packets from all zockets until buffers are overfilled
     *  - call @a rpc_zf_process_events() periodically or after each send;
     *  - in dependence on @b gradual
     *    - send by one packet from each zocket;
     *    - or send packets from a zocket until its buffer is full and
     *    then go to the next zocket; */
    RING("Overfilling IUT tx buffers");
    do {
        data_len = rand_range(1, max_len);
        te_fill_buf(data, data_len);

        /*
         * If @p gradual is @c TRUE, each time we try to select the next
         * not yet overfilled conection after the one we processed in the
         * previous iteration; otherwise we simply find the first not
         * yet overfilled connection in array.
         */
        if (!gradual)
            next_i = 0;

        for (i = next_i, j = 0; j < zft_num;
             i = (i + 1) % (zft_num), j++)
        {
            if (!conns[i].send_overfilled)
                break;
        }
        if (j >= zft_num)
            break;

        next_i = (i + 1) % zft_num;

        sndiov.iov_base = data;
        sndiov.iov_len = sndiov.iov_rlen = data_len;

        RPC_AWAIT_ERROR(pco_iut);
        pco_iut->silent_pass = TRUE;
        rc = rpc_zft_send(pco_iut, conns[i].iut_zft, &sndiov, 1, 0);
        if (rc < 0)
        {
            conns[i].fails++;
            if (conns[i].fails == EXTRA_ATTEMPTS)
                conns[i].send_overfilled = TRUE;

            if (RPC_ERRNO(pco_iut) != RPC_EAGAIN &&
                RPC_ERRNO(pco_iut) != RPC_ENOMEM)
            {
                TEST_VERDICT("zft_send() failed with "
                             "unexpected errno %r",
                             RPC_ERRNO(pco_iut));
            }
            else if (RPC_ERRNO(pco_iut) == RPC_ENOMEM)
            {
                if (large_n_bufs && !enomem_verdict)
                {
                    ERROR_VERDICT("zft_send() failed with ENOMEM "
                                  "after n_bufs attribute was set "
                                  "to a large value");
                    enomem_verdict = TRUE;
                }
            }

            if (conns[i].send_overfilled == FALSE)
            {
                pco_iut->silent_pass = TRUE;
                rpc_zf_process_events_long(pco_iut, stack, WAITING_TIME);
            }
        }
        else
        {
            te_dbuf_append(&conns[i].iut_sent, data, rc);
        }

        RPC_AWAIT_ERROR(pco_iut);
        pco_iut->silent_pass = TRUE;
        rc = rpc_zf_process_events(pco_iut, stack);
        if (rc < 0)
            TEST_VERDICT("rpc_zf_process_events() failed with errno %r",
                         RPC_ERRNO(pco_iut));
    } while (TRUE);

    /*- Check that @b zft_send_space() returns zero or small value
     * for every ZFT zocket after overfilling its send buffer. */
    for (i = 0; i < zft_num; i++)
    {
        rpc_zft_send_space(pco_iut, conns[i].iut_zft, &send_space2);
        if (send_space2 >= max_len * 2 && !final_space_verdict)
        {
            ERROR_VERDICT("zft_send_space() returns too much free space "
                          "for some ZFT zockets after overfilling "
                          "send buffer");
            final_space_verdict = TRUE;
        }
    }

    /*- Send some data from tester to each zocket. */
    for (i = 0; i < zft_num; i++)
    {
        RING("Sending data from Tester through %d connection", i + 1);

        data_len = rand_range(1, max_len);
        te_fill_buf(data, data_len);

        RPC_AWAIT_ERROR(pco_tst);
        rc = rpc_send(pco_tst, conns[i].tst_s, data, data_len, 0);
        if (rc < 0)
            TEST_VERDICT("rpc_send() failed with errno %r",
                         RPC_ERRNO(pco_tst));
        te_dbuf_append(&conns[i].tst_sent, data, data_len);
    }
    ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);

    /*- Read data on all zockets, check it. */
    zfts_tcp_conns_read_check_data_iut(pco_iut, stack, conns, zft_num);

    /*- Read data on all connections on Tester and check it. */
    zfts_tcp_conns_read_check_data_tst(pco_iut, stack,
                                       pco_tst, conns, zft_num);

    if (enomem_verdict || init_space_verdict || final_space_verdict)
        TEST_STOP;
    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);

    zfts_tcp_conns_destroy(pco_iut, pco_tst, conns, zft_num);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

