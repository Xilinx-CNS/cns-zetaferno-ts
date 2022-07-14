/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * TCP Tests
 */

/**
 * @page tcp-msg_more Send data using MSG_MORE flag.
 *
 * @objective Send some data with @c MSG_MORE flag using @b zft_send()
 *            or @b zft_send_single(). Check that data is sent only in
 *            full segments when @c MSG_MORE flag is specified.
 *
 * @param open_method   Connection establishment way:
 *                      - active (actively)
 *                      - passive (passively)
 *                      - passive_close (passively, close the listener
 *                        just after connection acceptance)
 * @param func          Testing function:
 *                      - send (use only function @b zft_send() to
 *                        send data)
 *                      - single (use only function @b zft_send_single()
 *                        to send data)
 *                      - send_single (use function @b zft_send() to send
 *                        data with @c MSG_MORE and @b zft_send_single() in
 *                        the final send call without @c MSG_MORE)
 *                      - single_send (use function @b zft_send_single() to
 *                        send data with @c MSG_MORE and @c zft_send() in
 *                        the final send call without @c MSG_MORE)
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "tcp/msg_more"

#include "zf_test.h"
#include "rpc_zf.h"

#include "tapi_tcp.h"
#include "tapi_cfg_if.h"
#include "te_string.h"

/**
 * Types of functions used in this test.
 */
typedef enum {
    ZFTS_MSG_MORE_SEND = 0,     /**< Use only zft_send(). */
    ZFTS_MSG_MORE_SINGLE,       /**< Use only zft_send_single(). */
    ZFTS_MSG_MORE_SEND_SINGLE,  /**< Call zft_send() with MSG_MORE,
                                     then zft_send_single() without
                                     it. */
    ZFTS_MSG_MORE_SINGLE_SEND,  /**< Call zft_send_single() with
                                     MSG_MORE, then zft_send()
                                     without it. */
} zfts_msg_more_func;

/**
 * List of types of functions used in this test, to be
 * passed to TEST_GET_ENUM_PARAM() macro.
 */
#define ZFTS_MSG_MORE_FUNCS \
    { "send",             ZFTS_MSG_MORE_SEND }, \
    { "single",           ZFTS_MSG_MORE_SINGLE }, \
    { "send_single",      ZFTS_MSG_MORE_SEND_SINGLE }, \
    { "single_send",      ZFTS_MSG_MORE_SINGLE_SEND }

/** How long to run test loop, in seconds. */
#define LOOP_TIME 3

/** Maximum number of buffers in IOV. */
#define MAX_IOVCNT 10

/** Minimum length of data in small IOV. */
#define MIN_IOV_SMALL 1

/** Maximum length of data in small IOV */
#define MAX_IOV_SMALL 1400

/** Minimum length of data in big IOV. */
#define MIN_IOV_BIG 1400

/** Maximum length of data in big IOV. */
#define MAX_IOV_BIG 4000

/** Minimum number of send calls in a bunch. */
#define MIN_BUNCH_SIZE 2

/** Maximum number of send calls in a bunch. */
#define MAX_BUNCH_SIZE 10

/**
 * Flags defining how IOV array is constructed.
 */
typedef enum {
    ZFTS_IOV_BIG = 0x1,               /**< Use big buffers. */
    ZFTS_IOV_MANY = 0x2,              /**< Use more than one
                                           buffer. */
    ZFTS_IOV_FIRST_SMALLER_MSS = 0x4, /**< First buffer should
                                           be smaller than MSS. */
    ZFTS_IOV_FIRST_LARGER_MSS = 0x8,  /**< First buffer should
                                           be larger than MSS. */
    ZFTS_IOV_LAST_SMALLER_MSS = 0x10, /**< Last buffer should
                                           be smaller than MSS. */
    ZFTS_IOV_LAST_LARGER_MSS = 0x20,  /**< Last buffer should
                                           be larger than MSS. */
} iov_param_flags;

/**
 * Get string representation of IOV flags.
 *
 * @param flags     Flags.
 *
 * @return String representation.
 */
static const char *
iov_param_flags2str(int flags)
{
    static te_string str = TE_STRING_INIT;

    te_string_cut(&str, str.len);

    if (flags & ZFTS_IOV_BIG)
        te_string_append(&str, "big-");
    else
        te_string_append(&str, "small-");

    if (flags & ZFTS_IOV_MANY)
        te_string_append(&str, "many");
    else
        te_string_append(&str, "one");

    if (flags & ZFTS_IOV_FIRST_SMALLER_MSS)
        te_string_append(&str, ", first buffer is smaller than MSS");
    if (flags & ZFTS_IOV_FIRST_LARGER_MSS)
        te_string_append(&str, ", first buffer is larger than MSS");
    if (flags & ZFTS_IOV_LAST_SMALLER_MSS)
        te_string_append(&str, ", last buffer is smaller than MSS");
    if (flags & ZFTS_IOV_LAST_LARGER_MSS)
        te_string_append(&str, ", last buffer is larger than MSS");

    return str.ptr;
}

/**
 * This variable will be set to TRUE if CSAP discovered
 * packets which size is not equal to MSS.
 */
static te_bool pkt_check_failed = FALSE;

/**
 * Handler for checking packets obtained by CSAP.
 *
 * @param pkt         TCP packet.
 * @param user_data   Pointer to MSS value.
 */
static void
user_pkt_handler(const tcp4_message *pkt, void *user_data)
{
    int mss = *((int *)user_data);

    if (pkt->payload_len != mss)
    {
        ERROR("TCP payload length is %d, MSS is %d",
              (int)pkt->payload_len, mss);

        if (!pkt_check_failed)
            ERROR_VERDICT("TCP payload length differs from MSS for "
                          " data sent with MSG_MORE flag");

        pkt_check_failed = TRUE;
    }
}

/**
 * Function to create IOV array or single data buffer and send
 * it with help of zft_send() or zft_send_single().
 *
 * @param rpcs          RPC server.
 * @param zft_zocket    RPC pointer to TCP zocket.
 * @param send_func     Which function to use for sending.
 * @param flags         Flags to pass to send function.
 * @param mss           MSS value.
 * @param dbuf          Dynamic buffer to which to append sent
 *                      data.
 */
static void
send_data(rcf_rpc_server *rpcs, rpc_zft_p zft_zocket,
          zfts_tcp_send_func_t send_func, int flags, int mss,
          te_dbuf *dbuf)
{
    int param_sets[] = { 0,
                         ZFTS_IOV_MANY,
                         ZFTS_IOV_BIG,
                         ZFTS_IOV_BIG | ZFTS_IOV_MANY,
                         ZFTS_IOV_BIG | ZFTS_IOV_MANY |
                         ZFTS_IOV_FIRST_SMALLER_MSS,
                         ZFTS_IOV_BIG | ZFTS_IOV_MANY |
                         ZFTS_IOV_FIRST_LARGER_MSS,
                         ZFTS_IOV_BIG | ZFTS_IOV_MANY |
                         ZFTS_IOV_LAST_SMALLER_MSS,
                         ZFTS_IOV_BIG | ZFTS_IOV_MANY |
                         ZFTS_IOV_LAST_LARGER_MSS };

    int sets_num = TE_ARRAY_LEN(param_sets);
    int param_set;

    rpc_iovec iovs[MAX_IOVCNT];
    int       iovcnt;

    unsigned int       size_min;
    unsigned int       size_max;
    unsigned int       size_first_min;
    unsigned int       size_first_max;
    unsigned int       size_last_min;
    unsigned int       size_last_max;
    unsigned int       size;
    unsigned int       cur_size;

    int rc;
    int i;

    /*
     * The bunch unit is IOV vector which have the following parameters:
     * - size (total data length of buffers in IOV)
     *   - small [@c MIN_IOV_SMALL; @c MAX_IOV_SMALL]
     *   - big [@c MIN_IOV_BIG; @c MAX_IOV_BIG]
     * - iovcnt
     *   - one
     *   - many ([@c 2; @c MAX_IOVCNT]) - not applicable for
     *     @b zft_send_single().
     *
     * Each IOV vector has one of the following parameters sets
     * (size-iovcnt) chosen randomly:
     * - small-one
     * - small-many
     * - big-one
     * - big-many
     * - big-many, first buffer is smaller than MSS
     * - big-many, first buffer is larger than MSS
     * - big-many, last buffer is smaller than MSS
     * - big-many, last buffer is larger than MSS
     */

    while (TRUE)
    {
        param_set = param_sets[rand_range(0, sets_num - 1)];

        if (!((param_set & ZFTS_IOV_MANY) &&
              send_func == ZFTS_TCP_SEND_ZFT_SEND_SINGLE))
            break;
    }

    RING("Sending IOV with parameters '%s'",
         iov_param_flags2str(param_set));

    if (param_set & ZFTS_IOV_MANY)
        iovcnt = rand_range(2, MAX_IOVCNT);
    else
        iovcnt = 1;

    size_first_min = 1;
    size_first_max = MAX_IOV_BIG;
    size_last_min = 1;
    size_last_max = MAX_IOV_BIG;

    if (param_set & ZFTS_IOV_BIG)
    {
        if (param_set & ZFTS_IOV_FIRST_LARGER_MSS)
            size_first_min = mss * 3 / 2;
        else if (param_set & ZFTS_IOV_FIRST_SMALLER_MSS)
            size_first_max = mss / 2;

        if (param_set & ZFTS_IOV_LAST_LARGER_MSS)
            size_last_min = mss * 3 / 2;
        else if (param_set & ZFTS_IOV_LAST_SMALLER_MSS)
            size_last_max = mss / 2;

        size_min = MAX(MIN_IOV_BIG,
                       size_first_min + size_last_min +
                       (iovcnt - 2));
        size_max = MIN(MAX_IOV_BIG,
                       size_first_max + size_last_max +
                       MAX_IOV_BIG * (iovcnt - 2));
    }
    else
    {
        size_min = MAX(MIN_IOV_SMALL, iovcnt);
        size_max = MAX_IOV_SMALL;
    }

    if (size_min > size_max)
        TEST_FAIL("Minimum data sise is greater than maximum");

    if (iovcnt == 1)
    {
        rpc_make_iov(iovs, iovcnt, size_min, size_max);
        size = iovs[0].iov_len;
    }
    else
    {
        size = rand_range(size_min, size_max);
        cur_size = 0;

        rpc_make_iov(iovs, iovcnt, MAX_IOV_BIG, MAX_IOV_BIG);

        iovs[0].iov_len = size_first_min;
        cur_size += size_first_min;
        iovs[iovcnt - 1].iov_len = size_last_min;
        cur_size += size_last_min;
        for (i = 1; i < iovcnt - 1; i++)
        {
            iovs[i].iov_len = 1;
            cur_size++;
        }

        while (cur_size < size)
        {
            te_bool has_space = FALSE;

            for (i = 0; i < iovcnt; i++)
            {
                int incr = 0;

                if ((i == 0 && iovs[i].iov_len == size_first_max))
                    continue;

                if (i == iovcnt - 1 &&
                    iovs[i].iov_len == size_last_max)
                    continue;

                if (iovs[i].iov_len == MAX_IOV_BIG)
                    continue;

                has_space = TRUE;

                incr = rand_range(0, 1);
                iovs[i].iov_len += incr;
                cur_size += incr;

                if (cur_size == size)
                    break;
            }

            if (cur_size < size && !has_space)
                TEST_FAIL("Failed to construct IOV with specified "
                          "total data size");
        }

        if (cur_size > size)
            TEST_FAIL("Too big IOV was constructed");
    }

    RING("IOV was constructed with total data length of %u bytes", size);

    if (send_func == ZFTS_TCP_SEND_ZFT_SEND)
        rc = rpc_zft_send(rpcs, zft_zocket, iovs, iovcnt, flags);
    else
        rc = rpc_zft_send_single(rpcs, zft_zocket,
                                 iovs[0].iov_base, iovs[0].iov_len, flags);

    if (rc != (int)size)
        TEST_FAIL("%d bytes was sent but %u bytes was required to send",
                  rc, size);

    rpc_iov_append2dbuf(iovs, iovcnt, dbuf);
    rpc_release_iov(iovs, iovcnt);
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    const struct if_nameindex *tst_if = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zft_p       iut_zft = RPC_NULL;
    rpc_zftl_p      iut_zftl = RPC_NULL;
    int             tst_s = -1;

    struct timeval tv_start;
    struct timeval tv_cur;

    int bunch_size;
    int i;
    int mss;

    zfts_tcp_send_func_t first_send_func;
    zfts_tcp_send_func_t second_send_func;

    te_dbuf iut_sent = TE_DBUF_INIT(0);
    te_dbuf tst_received = TE_DBUF_INIT(0);

    csap_handle_t   csap = CSAP_INVALID_HANDLE;
    int             sid;

    zfts_conn_open_method   open_method;
    zfts_msg_more_func      func;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(tst_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ENUM_PARAM(open_method, ZFTS_CONN_OPEN_METHODS);
    TEST_GET_ENUM_PARAM(func, ZFTS_MSG_MORE_FUNCS);

    CHECK_RC(tapi_cfg_if_feature_set_all_parents(pco_tst->ta,
                                                 tst_if->if_name,
                                                 "rx-gro", 0));

    /*- Choose @b first_send_func (to send data with @c MSG_MORE)
     * and @b second_send_func (to send data without @c MSG_MORE)
     * according to @p func. */

    if (func == ZFTS_MSG_MORE_SEND || func == ZFTS_MSG_MORE_SEND_SINGLE)
        first_send_func = ZFTS_TCP_SEND_ZFT_SEND;
    else
        first_send_func = ZFTS_TCP_SEND_ZFT_SEND_SINGLE;

    if (func == ZFTS_MSG_MORE_SEND || func == ZFTS_MSG_MORE_SINGLE_SEND)
        second_send_func = ZFTS_TCP_SEND_ZFT_SEND;
    else
        second_send_func = ZFTS_TCP_SEND_ZFT_SEND_SINGLE;

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Establish TCP connection according to @p open_method. */
    zfts_establish_tcp_conn_ext2(open_method,
                                 pco_iut, attr, stack,
                                 &iut_zftl, &iut_zft, iut_addr,
                                 pco_tst, &tst_s, tst_addr,
                                 -1, -1);

    /*- Obtain MSS value with @b zft_get_mss(). */
    mss = rpc_zft_get_mss(pco_iut, iut_zft);

    /*- Create CSAP on Tester. */

    rcf_ta_create_session(pco_tst->ta, &sid);

    CHECK_RC(tapi_tcp_ip4_eth_csap_create(pco_tst->ta, sid,
                                          tst_if->if_name,
                                          TAD_ETH_RECV_DEF |
                                          TAD_ETH_RECV_NO_PROMISC,
                                          NULL, NULL,
                                          SIN(tst_addr)->sin_addr.s_addr,
                                          SIN(iut_addr)->sin_addr.s_addr,
                                          SIN(tst_addr)->sin_port,
                                          SIN(iut_addr)->sin_port,
                                          &csap));

    gettimeofday(&tv_start, NULL);

    /*- In a loop during @c LOOP_TIME seconds: */
    while (TRUE)
    {
        gettimeofday(&tv_cur, NULL);
        if (TIMEVAL_SUB(tv_cur, tv_start) > TE_SEC2US(LOOP_TIME))
            break;

        /*-- Choose @b bunch_size randomly from
         * [@c MIN_BUNCH_SIZE, @c MAX_BUNCH_SIZE]. */
        bunch_size = rand_range(MIN_BUNCH_SIZE, MAX_BUNCH_SIZE);

        te_dbuf_free(&iut_sent);

        CHECK_RC(tapi_tad_trrecv_start(pco_tst->ta, sid, csap,
                                       NULL, TAD_TIMEOUT_INF, 0,
                                       RCF_TRRECV_PACKETS));

        /*-- Send @b bunch_size - 1 IOVs with @c MSG_MORE flag
         * set, using @b first_send_func function. */
        for (i = 0; i < bunch_size - 1; i++)
        {
            send_data(pco_iut, iut_zft, first_send_func,
                      RPC_MSG_MORE, mss, &iut_sent);
            rpc_zf_process_events(pco_iut, stack);
        }

        /*-- Check with help of CSAP that only packets of MSS size
         * were received on Tester. */
        ZFTS_WAIT_NETWORK(pco_iut, stack);
        CHECK_RC(tapi_tad_trrecv_stop(
                    pco_tst->ta, sid, csap,
                    tapi_tcp_ip4_eth_trrecv_cb_data(user_pkt_handler, &mss),
                    NULL));

        if (pkt_check_failed)
            TEST_STOP;

        /*-- Send last IOV in bunch without @c MSG_MORE flag set. */
        send_data(pco_iut, iut_zft, second_send_func, 0, mss, &iut_sent);
        rpc_zf_process_events(pco_iut, stack);

        /*-- Receive all data on Tester, check that it matches
         * data sent from IUT. */
        te_dbuf_free(&tst_received);
        zfts_zft_peer_read_all(pco_iut, stack,
                               pco_tst, tst_s, &tst_received);
        ZFTS_CHECK_RECEIVED_DATA(tst_received.ptr, iut_sent.ptr,
                                 tst_received.len, iut_sent.len,
                                 " from IUT");
    }

    TEST_SUCCESS;

cleanup:

    CLEANUP_CHECK_RC(tapi_tad_csap_destroy(pco_tst->ta, sid,
                                           csap));

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    te_dbuf_free(&iut_sent);
    te_dbuf_free(&tst_received);
    TEST_END;
}

