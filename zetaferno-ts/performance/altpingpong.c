/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno performance tests
 */

/**
 * @page performance-altpingpong Checking performance of alternative sends
 *
 * @objective Run @b zfaltpingpong to get mean @b RTT when using
 *            Alternative Sends API.
 *
 * @param env                 Testing environment:
 *                            - @ref arg_types_env_peer2peer
 * @param tst_alt             If @c TRUE, use @b zfaltpingpong as server on
 *                            Tester; otherwise use @b zftcppingpong.
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "performance/altpingpong"

#include "zf_test.h"
#include "rpc_zf.h"
#include "performance_lib.h"
#include "tapi_job_factory_rpc.h"

/** How long to wait for zfaltpingpong or zftcppingpong termination, in ms */
#define PINGPONG_TIMEOUT 300000

/** Default iterations number */
#define DEF_ITERS 1000000
/** Default payload size */
#define DEF_SIZE 12

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    const struct if_nameindex *tst_if = NULL;

    zfts_perf_app *ping_clnt = NULL;
    zfts_perf_app *ping_srv = NULL;

    tapi_job_factory_t *ping_clnt_factory = NULL;
    tapi_job_factory_t *ping_srv_factory = NULL;

    zfts_perf_app_opts app_opts = zfts_perf_app_opts_def;
    te_bool tst_alt;

    te_bool tst_zf_attr_unset = FALSE;
    double mean_rtt = 0;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(tst_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(tst_alt);

    TEST_STEP("Set @b ZF_ATTR on Tester to specify Zetaferno interface.");
    zfts_try_set_zf_if(pco_tst, tst_if->if_name, &tst_zf_attr_unset);

    CHECK_RC(tapi_job_factory_rpc_create(pco_iut, &ping_clnt_factory));
    CHECK_RC(tapi_job_factory_rpc_create(pco_tst, &ping_srv_factory));

    app_opts.clnt_addr = iut_addr;
    app_opts.srv_addr = tst_addr;

    if (!tst_alt)
    {
        /*
         * In case of using zftcppingpong instead of zfaltpingpong on
         * Tester, set these values explicitly to make sure both
         * peers are compatible.
         */
        app_opts.payload_size = TAPI_JOB_OPT_UINT_VAL(DEF_SIZE);
        app_opts.iters = TAPI_JOB_OPT_UINT_VAL(DEF_ITERS);
    }

    TEST_STEP("Allocate @b ping_clnt structure for running "
              "@b zfaltpingpong on IUT (as client).");
    CHECK_RC(zfts_perf_create_alt_pingpong_clnt(ping_clnt_factory,
                                                &app_opts, &ping_clnt));

    TEST_STEP("Allocate @b ping_srv structure for running "
              "@b zfaltpingpong (if @p tst_alt is @c TRUE) or "
              "@b zftcppingpong (if @p tst_alt is @c FALSE) on Tester "
              "(as server).");
    if (tst_alt)
    {
        CHECK_RC(zfts_perf_create_alt_pingpong_srv(ping_srv_factory,
                                                   &app_opts,
                                                   &ping_srv));
    }
    else
    {
        CHECK_RC(zfts_perf_create_tcp_pingpong_srv(ping_srv_factory,
                                                   &app_opts,
                                                   &ping_srv));
    }

    TEST_STEP("Run @b zfaltpingpong or @b zftcppingpong as server "
              "on Tester.");
    CHECK_RC(zfts_perf_start_app(ping_srv));
    TAPI_WAIT_NETWORK;

    TEST_STEP("Run @b zfaltpingpong as client on IUT.");
    CHECK_RC(zfts_perf_start_app(ping_clnt));

    TEST_STEP("Wait for termination of the testing applications on IUT "
              "and Tester.");
    CHECK_RC(zfts_perf_wait_app(ping_srv, PINGPONG_TIMEOUT));
    CHECK_RC(zfts_perf_wait_app(ping_clnt, PINGPONG_TIMEOUT));

    TEST_STEP("Get mean @b RTT value printed out by @b zfaltpingpong "
              "on IUT.");
    CHECK_RC(zfts_perf_get_single_result(ping_clnt, &mean_rtt));
    TEST_ARTIFACT("Mean RTT is %f us", mean_rtt);

    CHECK_RC(zfts_perf_mean_rtt_to_mi("zfaltpingpong", mean_rtt));

    TEST_SUCCESS;

cleanup:

    CLEANUP_CHECK_RC(zfts_perf_destroy_app(ping_srv));
    CLEANUP_CHECK_RC(zfts_perf_destroy_app(ping_clnt));

    if (tst_zf_attr_unset)
        rpc_unsetenv(pco_tst, "ZF_ATTR");

    TEST_END;
}
