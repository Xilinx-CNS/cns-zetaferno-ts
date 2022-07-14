/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP TX tests
 *
 * $Id:$
 */

/**
 * @page udp_tx-send_recv_flooding Send-receive flooding with multiple zockets.
 *
 * @objective Send and receive datagrams incessantly by a few zockets.
 *
 * @param func          Transmitting function.
 * @param large_buffer  Use large data buffer to send on @p utx_num_large
 *                      TX zockets.
 * @param few_iov       Use several iov vectors.
 * @param urx_num  UDP RX zockets number:
 *      - @c 5
 * @param utx_num  UDP TX zockets number:
 *      - @c 5
 * @param utx_num_large  Number of UDP TX zockets which send large data
 *                       buffer if @p large_buffer is @c TRUE:
 *      - @c 2
 * @param duration How long data transmission should last in seconds:
 *      - @c 10
 * @param thread   Create personal thread for each zocket if @c TRUE,
 *                 else - process.
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME "udp_tx/send_recv_flooding"

#include "zf_test.h"

/* Calculate TX zocket index. */
#define TX_IDX (urx_num + i)

/* How long read waiting for the end of data, seconds. */
#define WAIT_FOR_END_OF_DATA 1

/* Minimum number of successful transmitted datagrams, percentages. */
#define MIN_SUCCESS_RATE 1

/*
 * Size of datagram in case of @p large_buffer is TRUE.
 * ZFTS_DGRAM_LARGE is too large for this test because tester
 * cannot reassemble all fragments, so reduced value is used.
 */
#define DGRAM_LARGE (ZFTS_DGRAM_LARGE / 2)

/**
 * Network points pair.
 */
typedef struct point_type {
    rcf_rpc_server  *pco_iut;   /* IUT RPC server handle */
    rcf_rpc_server  *pco_tst;   /* Tester RPC server handle */
    rpc_zf_stack_p   stack;     /* ZF stack */
    rpc_zf_attr_p    attr;      /* ZF attributes */
    rpc_zfur_p       urx;       /* UDP RX zocket or @c RPC_NULL */
    rpc_zfut_p       utx;       /* UDP TX zocket or @c RPC_NULL */
    struct sockaddr *laddr;     /* Local address */
    struct sockaddr *raddr;     /* Remote address */
    int              tst_s;     /* Tester socket */
    uint64_t         stats;     /* Sent or received data amount */
    uint64_t         stats_tst; /* Sent or received by tester data amount */
    uint64_t         errors;    /* Errors number */
    te_bool          large_buffer; /* Use large data buffer to send */
} point_type;

/**
 * Start aux RPC servers for both IUT and tester, allocate ZF attributes and
 * stacks on new IUT RPC servers.
 *
 * @param pco_iut       IUT RPC server handle.
 * @param pco_tst       Tester RPC server handle,
 * @param points        Network points array.
 * @param points_num    The array length.
 * @param thread        Whether thread or process should be created for each
 *                      point.
 */
static void
start_aux_pcos(rcf_rpc_server *pco_iut, rcf_rpc_server *pco_tst,
               point_type *points, int points_num, te_bool thread)
{
    char pco_name_iut[30] = {0};
    char pco_name_tst[30] = {0};
    int i;

    for (i = 0; i < points_num; i++)
    {
        snprintf(pco_name_iut, sizeof(pco_name_iut), "child_pco_iut_%d", i);
        snprintf(pco_name_tst, sizeof(pco_name_tst), "child_pco_tst_%d", i);
        if (thread)
        {
            CHECK_RC(rcf_rpc_server_thread_create(pco_iut, pco_name_iut,
                                                  &points[i].pco_iut));
            CHECK_RC(rcf_rpc_server_thread_create(pco_tst, pco_name_tst,
                                                  &points[i].pco_tst));
        }
        else
        {
            CHECK_RC(rcf_rpc_server_create_process(pco_iut, pco_name_iut,
                                                   0, &points[i].pco_iut));
            rpc_zf_init(points[i].pco_iut);
            CHECK_RC(rcf_rpc_server_create_process(pco_tst, pco_name_tst,
                                                   0, &points[i].pco_tst));
        }
    }

    for (i = 0; i < points_num; i++)
    {
        rpc_zf_attr_alloc(points[i].pco_iut, &points[i].attr);
        rpc_zf_stack_alloc(points[i].pco_iut, points[i].attr,
                           &points[i].stack);
    }
}

/**
 * Run flooders execution.
 *
 * @param points        Network points array.
 * @param urx_num       RX zockets number.
 * @param utx_num       TX zockets number.
 * @param duration      Flooders execution duration,
 * @param send_f        ZF send function to send datagrams.
 * @param few_iov       Use several iov vectors.
 * @param call          Make non-blocking RPC call if @c TRUE.
 */
static void
run_flooders(point_type *points, int urx_num, int utx_num, int duration,
             zfts_send_function send_f,
             te_bool few_iov, te_bool call)
{
    tarpc_timeval tv = {0, 0};
    int points_num = urx_num + utx_num;
    int i;

    if (call)
    {
        rpc_gettimeofday(points->pco_iut, &tv, NULL);
        for (i = 0; i < points_num; i++)
            points[i].pco_iut->start = points[i].pco_tst->start =
                (tv.tv_sec + 1) * 1000 + tv.tv_usec / 1000;
    }

    for (i = 0; i < urx_num; i++)
    {
        if (call)
            points[i].pco_iut->op = RCF_RPC_CALL;
        rpc_zfur_flooder(points[i].pco_iut, points[i].stack, points[i].urx,
                         duration + WAIT_FOR_END_OF_DATA * 1000,
                         &points[i].stats);

        if (call)
            points[i].pco_tst->op = RCF_RPC_CALL;
        rpc_iomux_flooder(points[i].pco_tst, &points[i].tst_s, 1, NULL, 0,
                          ZFTS_DGRAM_MAX, duration / 1000, 0,
                          FUNC_DEFAULT_IOMUX, &points[i].stats_tst, 0);
    }

    for (i = 0; i < utx_num; i++)
    {
        if (call)
            points[TX_IDX].pco_iut->op = RCF_RPC_CALL;
        rpc_zfut_flooder(points[TX_IDX].pco_iut, points[TX_IDX].stack,
                         points[TX_IDX].utx, send_f,
                         points[TX_IDX].large_buffer ? DGRAM_LARGE :
                                                       ZFTS_DGRAM_MAX,
                         few_iov ? ZFTS_IOVCNT : 1, duration,
                         &points[TX_IDX].stats, &points[TX_IDX].errors);

        if (call)
            points[TX_IDX].pco_tst->op = RCF_RPC_CALL;
        rpc_iomux_flooder(points[TX_IDX].pco_tst, NULL, 0,
                          &points[TX_IDX].tst_s, 1,
                          ZFTS_DGRAM_MAX, duration / 1000,
                          WAIT_FOR_END_OF_DATA, FUNC_DEFAULT_IOMUX,
                          0, &points[TX_IDX].stats_tst);
    }
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut;
    rcf_rpc_server        *pco_tst;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    int                    urx_num;
    int                    utx_num;
    int                    utx_num_large;
    int                    duration;
    te_bool                thread;
    zfts_send_function     func;
    te_bool                large_buffer;
    te_bool                few_iov;
    te_bool                test_failed = FALSE;

    point_type *points;
    int points_num;
    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);
    TEST_GET_INT_PARAM(urx_num);
    TEST_GET_INT_PARAM(utx_num);
    TEST_GET_INT_PARAM(utx_num_large);
    TEST_GET_INT_PARAM(duration);
    TEST_GET_BOOL_PARAM(thread);

    TEST_STEP("Make tester able to receive a huge amount of fragmented "
              "IPv4 packets.");
    CHECK_RC(tapi_cfg_sys_set_int(pco_tst->ta, 8392, NULL,
                                  "net/ipv4/ipfrag_max_dist"));
    CHECK_RC(tapi_cfg_sys_set_int(pco_tst->ta, 52428800, NULL,
                                  "net/ipv4/ipfrag_high_thresh"));

    /* Convert to milliseconds. */
    duration *= 1000;

    TEST_STEP("Create @p urx_num + @p utx_num threads (or processes) and "
              "stacks, according to @p thread.");
    points_num = urx_num + utx_num;
    points = tapi_calloc(points_num, sizeof(*points));

    if (thread)
        rpc_zf_init(pco_iut);

    start_aux_pcos(pco_iut, pco_tst, points, points_num, thread);

    for (i = 0; i < points_num; i++)
    {
        points[i].laddr = tapi_sockaddr_clone_typed(iut_addr,
                                                    TAPI_ADDRESS_SPECIFIC);
        CHECK_RC(tapi_allocate_set_port(pco_iut, points[i].laddr));
        points[i].raddr = tapi_sockaddr_clone_typed(tst_addr,
                                                    TAPI_ADDRESS_SPECIFIC);
        CHECK_RC(tapi_allocate_set_port(pco_tst, points[i].raddr));

        points[i].tst_s = rpc_socket(points[i].pco_tst,
                                     rpc_socket_domain_by_addr(tst_addr),
                                     RPC_SOCK_DGRAM, RPC_PROTO_DEF);
        rpc_bind(points[i].pco_tst, points[i].tst_s, points[i].raddr);
        rpc_connect(points[i].pco_tst, points[i].tst_s, points[i].laddr);
    }

    TEST_STEP("Create UDP RX and TX zockets each in its personal "
              "thread/poccess and stack.");
    for (i = 0; i < urx_num; i++)
    {
        rpc_zfur_alloc(points[i].pco_iut, &points[i].urx, points[i].stack,
                       points[i].attr);
        rpc_zfur_addr_bind(points[i].pco_iut, points[i].urx,
                           points[i].laddr, points[i].raddr, 0);
    }

    for (i = 0; i < utx_num; i++)
    {
        rpc_zfut_alloc(points[TX_IDX].pco_iut, &points[TX_IDX].utx,
                       points[TX_IDX].stack, points[TX_IDX].laddr,
                       points[TX_IDX].raddr, 0, points[TX_IDX].attr);
        if (large_buffer && i < utx_num_large)
            points[TX_IDX].large_buffer = TRUE;
        else
            points[TX_IDX].large_buffer = FALSE;
    }

    TEST_STEP("Send/receive datagrams during @p duration seconds on each "
              "zocket and check that all zockets sent or received at least "
              "by on datagram and the percentage of passed datagrams is "
              "greater than @c MIN_SUCCESS_RATE.");
    run_flooders(points, urx_num, utx_num, duration, func, few_iov, TRUE);
    MSLEEP(duration);
    run_flooders(points, urx_num, utx_num, duration, func, few_iov, FALSE);

    for (i = 0; i < points_num; i++)
    {
        int percent;
        int errors;

        if (i < urx_num)
        {
            percent = points[i].stats_tst == 0 ? 0 :
                      points[i].stats * 100 / points[i].stats_tst;
        }
        else
        {
            percent = points[i].stats == 0 ? 0 :
                      points[i].stats_tst * 100 / points[i].stats;
        }

        errors = points[i].stats / ZFTS_DGRAM_MAX + points[i].errors;
        if (errors != 0)
            errors = points[i].errors * 100 / errors;

        rpc_zf_process_events(points[i].pco_iut, points[i].stack);
        RING("Datagrams %llu/%llu, success rate %d%%, errors %llu%%",
             points[i].stats / ZFTS_DGRAM_MAX,
             points[i].stats_tst / ZFTS_DGRAM_MAX, percent, errors);

        if (points[i].stats == 0 || points[i].stats_tst == 0)
        {
            ERROR("Zocket number %d did not transmit (or receive) data", i);
            if (!test_failed)
            {
                ERROR_VERDICT("One of zockets did not %s data",
                              i < urx_num ? "receive" : "send");
                test_failed = TRUE;
            }
        }

        if (percent < MIN_SUCCESS_RATE)
        {
            ERROR("Zocket #%d has too low success rate", i);
            if (!test_failed)
            {
                ERROR_VERDICT("Too few datagrams number were successful "
                              "transmitted");
                test_failed = TRUE;
            }
        }
    }
    if (test_failed)
        TEST_STOP;

    TEST_STEP("Check that all zockets can receive datagrams after all.");
    for (i = 0; i < urx_num; i++)
        zfts_zfur_check_reception(points[i].pco_iut, points[i].stack,
                                  points[i].urx, points[i].pco_tst,
                                  points[i].tst_s, NULL);

    TEST_STEP("Check that all zockets can send datagrams after all.");
    for (i = 0; i < utx_num; i++)
        zfts_zfut_check_send_func(points[TX_IDX].pco_iut,
                                  points[TX_IDX].stack, points[TX_IDX].utx,
                                  points[TX_IDX].pco_tst,
                                  points[TX_IDX].tst_s, func,
                                  points[TX_IDX].large_buffer,
                                  few_iov);

    TEST_SUCCESS;

cleanup:
    for (i = 0; i < urx_num; i++)
        CLEANUP_RPC_ZFTS_FREE(points[i].pco_iut, zfur, points[i].urx);
    for (i = 0; i < utx_num; i++)
        CLEANUP_RPC_ZFTS_FREE(points[TX_IDX].pco_iut, zfut, points[TX_IDX].utx);

    for (i = 0; i < points_num; i++)
    {
        CLEANUP_RPC_ZFTS_FREE(points[i].pco_iut, zf_stack, points[i].stack);
        CLEANUP_RPC_ZF_ATTR_FREE(points[i].pco_iut, points[i].attr);
        CLEANUP_RPC_CLOSE(points[i].pco_tst, points[i].tst_s);
        if (!thread)
            CLEANUP_RPC_ZF_DEINIT(points[i].pco_iut);
    }

    if (thread)
        CLEANUP_RPC_ZF_DEINIT(pco_iut);

    for (i = 0; i < points_num; i++)
    {
        rcf_rpc_server_destroy(points[i].pco_iut);
        rcf_rpc_server_destroy(points[i].pco_tst);
    }

    TEST_END;
}
