/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-many_zft Send data from a few TCP zockets using TCP alternatives.
 *
 * @objective Queue data to a few alternatives in a few TCP zockets,
 *            send data from random zockets and alternatives.
 *
 * @param iterations_num    The main loop iterations number:
 *                          - @c 100
 * @param zft_num           TCP connections number:
 *                          - @c 5
 * @param iovcnt            IOV vectors number:
 *                          - @c 1
 * @param data_size         Data buffer size:
 *                          - @c 1
 *                          - @c 1400
 *                          - @c 4000
 * @param send_all          Send all alternatives if @c TRUE.
 * @param open_method       How to open connection:
 *                          - @c active
 *                          - @c passive
 *                          - @c passive_close (close listener after
 *                            passively establishing connection)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "zf_alt/many_zft"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_sockets.h"
#include "te_string.h"

/** Number of ZF alternatives allocated per zocket. */
#define ALTS_PER_ZOCK 3

/** How long to wait for a sent packet on a peer (milliseconds). */
#define DATA_WAIT_TIMEOUT 500

/** Structure storing TCP connection peers and associated variables. */
typedef struct zfts_alt_conn {
    rpc_zft_p   iut_zftl;                         /**< IUT TCP listener
                                                       zocket.*/
    rpc_zft_p   iut_zft;                          /**< IUT TCP zocket.*/
    int         tst_s;                            /**< Tester TCP socket. */

    rpc_zf_althandle  alts[ALTS_PER_ZOCK];        /**< Alternatives
                                                       allocated for a given
                                                       zocket. */
    te_bool           alt_filled[ALTS_PER_ZOCK];  /**< Array whose elements
                                                       determine whether we
                                                       have data queued on
                                                       a given alternative.
                                                       */
    rpc_iovec        *iovs[ALTS_PER_ZOCK];        /**< IOVs used to queue
                                                       data on alternatives.
                                                       */

    te_dbuf     iut_sent;                         /**< Data sent from IUT.
                                                       */
} zfts_alt_conn;

/** Array of connections. */
zfts_alt_conn   *conns = NULL;
/** Number of elements in the array. */
int              zft_num = 0;

/**
 * Check whether in given IOV data is encoded so that it
 * contains the same zocket and alternative IDs in all bytes.
 *
 * @param iov       IOV array to check.
 * @param iovcnt    Number of elements in array.
 * @param zock_id   Zocket ID.
 * @param alt_id    Alternative ID.
 */
static void
check_zock_alt_iov(const rpc_iovec *iov, int iovcnt,
                   int zock_id, int alt_id)
{
    char b;
    int  i;
    int  j;

    b = (zock_id << 4) + alt_id;

    for (i = 0; i < iovcnt; i++)
    {
        for (j = 0; j < (int)iov[i].iov_len; j++)
        {
            if (((char *)iov[i].iov_base)[j] != b)
                TEST_FAIL("Wrong data found in IOV of [zock %d, alt %d]",
                          zock_id, alt_id);
        }
    }
}

/**
 * Fill buffer with bytes in which zocket ID and alternative ID will
 * be encoded.
 *
 * @param buf       Buffer to fill.
 * @param len       Length of buffer.
 * @param zock_id   Zocket ID.
 * @param alt_id    Alternative ID.
 */
static void
fill_zock_alt_buf(char *buf, int len, int zock_id, int alt_id)
{
    char b;
    int  i;

    b = (zock_id << 4) + alt_id;

    for (i = 0; i < len; i++)
    {
        buf[i] = b;
    }
}

/**
 * Analyze a buffer and get string representation saying
 * from which alternatives of which zockets its bytes may have
 * come from (in form [zock 1, alt 2], [zock 1, alt 3], ...).
 *
 * @param buf   Buffer to analyze.
 * @param len   Length of buffer.
 * @param str   String where to save string representation
 *              of buffer contents.
 */
static void
zock_alt_buf2str(uint8_t *buf, int len, te_string *str)
{
    int  i;
    char prev_c;

    int  zock_id;
    int  alt_id;

    te_string_free(str);

    for (i = 0; i < len; i++)
    {
        if (i == 0 || prev_c != buf[i])
        {
            zock_id = (buf[i] >> 4) & 0x0f;
            alt_id = buf[i] & 0x0f;

            if (i > 0)
                te_string_append(str, ", ");

            te_string_append(str,
                             "[zock %u (%#x), "
                             "alt %u (%"TE_PRINTF_64"u)]",
                             zock_id,
                             (zock_id < zft_num ?
                                      conns[zock_id].iut_zft :
                                      (unsigned)-1),
                             alt_id,
                             (zock_id < zft_num && alt_id < ALTS_PER_ZOCK ?
                                  conns[zock_id].alts[alt_id] :
                                  (unsigned)-1));
        }

        prev_c = buf[i];
    }
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    struct sockaddr_storage cur_iut_addr;
    struct sockaddr_storage cur_tst_addr;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    int        i;
    int        j;
    int        k;
    int        iter;

    int       iterations_num;
    int       iovcnt;
    int       data_size;
    te_bool   send_all;

    int              send_num;

    te_dbuf recv_data = TE_DBUF_INIT(0);

    te_string recv_log = TE_STRING_INIT;
    te_string exp_log = TE_STRING_INIT;

    zfts_conn_open_method open_method;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_INT_PARAM(iterations_num);
    TEST_GET_INT_PARAM(zft_num);
    TEST_GET_INT_PARAM(iovcnt);
    TEST_GET_INT_PARAM(data_size);
    TEST_GET_BOOL_PARAM(send_all);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);

    /*- Allocate ZF attributes and stack. */
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Establish TCP connections to get @p zft_num zft zockets. */

    conns = tapi_calloc(zft_num, sizeof(zfts_alt_conn));

    for (i = 0; i < zft_num; i++)
    {
        CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr, &cur_iut_addr));
        CHECK_RC(tapi_sockaddr_clone(pco_tst, tst_addr, &cur_tst_addr));
        zfts_establish_tcp_conn_ext2(open_method, pco_iut,
                                     attr, stack,
                                     &conns[i].iut_zftl, &conns[i].iut_zft,
                                     SA(&cur_iut_addr),
                                     pco_tst, &conns[i].tst_s,
                                     SA(&cur_tst_addr),
                                     -1, -1);

        /*- Allocate @c ALTS_PER_ZOCK alternatives for each zocket. */
        for (j = 0; j < ALTS_PER_ZOCK; j++)
        {
            ZFTS_CHECK_RPC(
                rpc_zf_alternatives_alloc(pco_iut, stack, attr,
                                          &conns[i].alts[j]),
                pco_iut, "zf_alternatives_alloc()");

            rpc_alloc_iov(&conns[i].iovs[j], iovcnt, data_size, data_size);
            conns[i].alt_filled[j] = FALSE;

            for (k = 0; k < iovcnt; k++)
            {
                fill_zock_alt_buf(conns[i].iovs[j][k].iov_base,
                                  conns[i].iovs[j][k].iov_len,
                                  i, j);
            }
        }
    }

    /*- In the loop @p iterations_num times: */
    for (iter = 0; iter < iterations_num; iter++)
    {
        RING("Iteration %d", iter + 1);

        /*- Queue packets for sending:
         *  - fill all alternatives on all zockets;
         *  - use @p iovcnt and @p data_size as packets parameters. */
        for (i = 0; i < zft_num; i++)
        {
            for (j = 0; j < ALTS_PER_ZOCK; j++)
            {
                if (!conns[i].alt_filled[j])
                {
                    check_zock_alt_iov(conns[i].iovs[j], iovcnt,
                                       i, j);

                    ZFTS_CHECK_RPC(
                      rpc_zft_alternatives_queue(
                                    pco_iut, conns[i].iut_zft,
                                    conns[i].alts[j],
                                    conns[i].iovs[j], iovcnt, 0),
                      pco_iut, "zft_alternatives_queue()");
                    conns[i].alt_filled[j] = TRUE;
                }
            }

            te_dbuf_free(&conns[i].iut_sent);
        }

        if (send_all)
        {

           /*- If @p send_all is @c TRUE, send one
            * alternative for each zocket. */
            for (i = 0; i < zft_num; i++)
            {
                j = rand_range(0, ALTS_PER_ZOCK - 1);

                ZFTS_CHECK_RPC(
                        rpc_zf_alternatives_send(pco_iut, stack,
                                                 conns[i].alts[j]),
                        pco_iut, "zf_alternatives_send()");
                conns[i].alt_filled[j] = FALSE;
                rpc_iov_append2dbuf(conns[i].iovs[j], iovcnt,
                                    &conns[i].iut_sent);
                rpc_zf_process_events(pco_iut, stack);
            }
        }
        else
        {

           /*- Else send random alternative from random number
            * of zft zockets. Use random order of zokets on each
            * iteration. */
            send_num = rand_range(1, zft_num);

            k = send_num;
            while (k > 0)
            {
                i = rand_range(0, zft_num - 1);
                if (conns[i].iut_sent.len > 0)
                    continue;

                j = rand_range(0, ALTS_PER_ZOCK - 1);
                ZFTS_CHECK_RPC(
                        rpc_zf_alternatives_send(pco_iut, stack,
                                                 conns[i].alts[j]),
                        pco_iut, "zf_alternatives_send()");
                conns[i].alt_filled[j] = FALSE;
                rpc_iov_append2dbuf(conns[i].iovs[j], iovcnt,
                                    &conns[i].iut_sent);

                k--;
                rpc_zf_process_events(pco_iut, stack);
            }
        }

        ZFTS_CHECK_RPC(rpc_zf_process_events(pco_iut, stack),
                       pco_iut, "zf_process_events()");

        /*- Read and check data on tester. */

        for (i = 0; i < zft_num; i++)
        {
            if (conns[i].iut_sent.len <= 0)
                continue;

            rc = zfts_sock_wait_data(pco_tst, conns[i].tst_s,
                                     DATA_WAIT_TIMEOUT);
            if (rc <= 0)
                TEST_VERDICT("Failed to wait for data sent from IUT");

            te_dbuf_free(&recv_data);
            if (tapi_sock_read_data(pco_tst, conns[i].tst_s,
                                    &recv_data) < 0)
                TEST_VERDICT("Failed to read data from peer");

            zock_alt_buf2str(conns[i].iut_sent.ptr, conns[i].iut_sent.len,
                             &exp_log);
            zock_alt_buf2str(recv_data.ptr, recv_data.len, &recv_log);

            RING("Expected to receive: %s", exp_log.ptr);
            RING("Actually received: %s", recv_log.ptr);

            ZFTS_CHECK_RECEIVED_DATA(recv_data.ptr, conns[i].iut_sent.ptr,
                                     recv_data.len, conns[i].iut_sent.len,
                                     " from IUT");
        }

        /*- For every zocket from each data was sent, cancel all the
         * alternatives on which data is still queued for it. */
        for (i = 0; i < zft_num; i++)
        {
            if (conns[i].iut_sent.len > 0)
            {
                for (j = 0; j < ALTS_PER_ZOCK; j++)
                {
                    if (conns[i].alt_filled[j])
                    {
                        ZFTS_CHECK_RPC(
                                rpc_zf_alternatives_cancel(
                                                      pco_iut, stack,
                                                      conns[i].alts[j]),
                                pco_iut, "zf_alternatives_cancel()");
                        conns[i].alt_filled[j] = FALSE;

                        rpc_zf_process_events(pco_iut, stack);
                    }
                }
            }
        }
    }

    TEST_SUCCESS;

cleanup:

    for (i = 0; i < zft_num; i++)
    {
        CLEANUP_RPC_CLOSE(pco_tst, conns[i].tst_s);
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, conns[i].iut_zftl);
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, conns[i].iut_zft);
        te_dbuf_free(&conns[i].iut_sent);

        for (j = 0; j < ALTS_PER_ZOCK; j++)
        {
            CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(pco_iut, stack,
                                        conns[i].alts[j]);

            rpc_free_iov(conns[i].iovs[j], iovcnt);
        }

    }

    free(conns);
    te_dbuf_free(&recv_data);

    te_string_free(&exp_log);
    te_string_free(&recv_log);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}
