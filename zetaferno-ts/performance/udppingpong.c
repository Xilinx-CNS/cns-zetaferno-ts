/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno performance tests
 */

/**
 * @page performance-udppingpong Checking UDP performance
 *
 * @objective Run @b zfudppingpong to get mean @b RTT for @b UDP.
 *
 * @param env             Testing environment:
 *                        - @ref arg_types_env_peer2peer
 *
 * @type Conformance.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "performance/udppingpong"

#include "zf_test.h"
#include "rpc_zf.h"
#include "performance_lib.h"
#include "tapi_job_factory_rpc.h"

/** How long to wait for zfudppingpong termination, in ms */
#define PINGPONG_TIMEOUT 60000

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

    te_bool tst_zf_attr_unset = FALSE;
    double mean_rtt = 0;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_IF(tst_if);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);

    TEST_STEP("Set @b ZF_ATTR on Tester to specify Zetaferno interface.");
    zfts_try_set_zf_if(pco_tst, tst_if->if_name, &tst_zf_attr_unset);

    CHECK_RC(tapi_job_factory_rpc_create(pco_iut, &ping_clnt_factory));
    CHECK_RC(tapi_job_factory_rpc_create(pco_tst, &ping_srv_factory));

    TEST_STEP("Allocate structures for running @b zfudppingpong on "
              "IUT (as client) and on Tester (as server).");
    app_opts.clnt_addr = iut_addr;
    app_opts.srv_addr = tst_addr;
    CHECK_RC(zfts_perf_create_udp_pingpong_clnt(ping_clnt_factory,
                                                &app_opts, &ping_clnt));
    CHECK_RC(zfts_perf_create_udp_pingpong_srv(ping_srv_factory, &app_opts,
                                               &ping_srv));

    TEST_STEP("Run @b zfudppingpong as server on Tester.");
    CHECK_RC(zfts_perf_start_app(ping_srv));
    TAPI_WAIT_NETWORK;

    TEST_STEP("Run @b zfudppingpong as client on IUT.");
    CHECK_RC(zfts_perf_start_app(ping_clnt));

    TEST_STEP("Wait for termination of @b zfudppingpong on IUT "
              "and Tester.");
    CHECK_RC(zfts_perf_wait_app(ping_srv, PINGPONG_TIMEOUT));
    CHECK_RC(zfts_perf_wait_app(ping_clnt, PINGPONG_TIMEOUT));

    TEST_STEP("Get mean @b RTT value printed out by @b zfudppingpong "
              "on IUT.");
    CHECK_RC(zfts_perf_get_single_result(ping_clnt, &mean_rtt));
    TEST_ARTIFACT("Mean RTT is %f us", mean_rtt);

    CHECK_RC(zfts_perf_mean_rtt_to_mi("zfudppingpong", mean_rtt));

    TEST_SUCCESS;

cleanup:

    CLEANUP_CHECK_RC(zfts_perf_destroy_app(ping_srv));
    CLEANUP_CHECK_RC(zfts_perf_destroy_app(ping_clnt));

    if (tst_zf_attr_unset)
        rpc_unsetenv(pco_tst, "ZF_ATTR");

    TEST_END;
}
