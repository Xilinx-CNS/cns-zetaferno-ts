/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Zetaferno API Test Suite
 *
 * Auxiliary functions for performance package.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#ifndef __TS_PERFORMANCE_LIB_H__
#define __TS_PERFORMANCE_LIB_H__

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_job.h"
#include "tapi_job_opt.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Command line options for performance application */
typedef struct zfts_perf_app_opts {
    const struct sockaddr *clnt_addr;   /**< Client address */
    const struct sockaddr *srv_addr;    /**< Server address */

    tapi_job_opt_uint_t payload_size;       /**< Payload size for
                                                 a packet */
    tapi_job_opt_uint_t iters;              /**< Number of iterations */
    te_bool print_ts;                       /**< If @c TRUE, print
                                                 timestamps */
    te_bool use_muxer;                      /**< If @c TRUE, use muxer */
    tapi_job_opt_uint_t overlapped_delay;   /**< Delay between overlapped
                                                 reads */
} zfts_perf_app_opts;

/** Default values of command line options */
extern const zfts_perf_app_opts zfts_perf_app_opts_def;

/** Opaque type for performance application data */
typedef struct zfts_perf_app zfts_perf_app;

/**
 * Allocate zfts_perf_app for client application of zfudppingpong.
 *
 * @param factory     Factory that creates job instances.
 * @param opts        Command line options.
 * @param app         Where to save pointer to allocated zfts_perf_app.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_create_udp_pingpong_clnt(
                                            tapi_job_factory_t *factory,
                                            const zfts_perf_app_opts *opts,
                                            zfts_perf_app **app);

/**
 * Allocate zfts_perf_app for server application of zfudppingpong.
 *
 * @param factory     Factory that creates job instances.
 * @param opts        Command line options.
 * @param app         Where to save pointer to allocated zfts_perf_app.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_create_udp_pingpong_srv(
                                            tapi_job_factory_t *factory,
                                            const zfts_perf_app_opts *opts,
                                            zfts_perf_app **app);

/**
 * Allocate zfts_perf_app for client application of zftcppingpong.
 *
 * @param factory     Factory that creates job instances.
 * @param opts        Command line options.
 * @param app         Where to save pointer to allocated zfts_perf_app.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_create_tcp_pingpong_clnt(
                                            tapi_job_factory_t *factory,
                                            const zfts_perf_app_opts *opts,
                                            zfts_perf_app **app);

/**
 * Allocate zfts_perf_app for server application of zftcppingpong.
 *
 * @param factory     Factory that creates job instances.
 * @param opts        Command line options.
 * @param app         Where to save pointer to allocated zfts_perf_app.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_create_tcp_pingpong_srv(
                                            tapi_job_factory_t *factory,
                                            const zfts_perf_app_opts *opts,
                                            zfts_perf_app **app);

/**
 * Allocate zfts_perf_app for client application of zfaltpingpong.
 *
 * @param factory     Factory that creates job instances.
 * @param opts        Command line options.
 * @param app         Where to save pointer to allocated zfts_perf_app.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_create_alt_pingpong_clnt(
                                            tapi_job_factory_t *factory,
                                            const zfts_perf_app_opts *opts,
                                            zfts_perf_app **app);

/**
 * Allocate zfts_perf_app for server application of zfaltpingpong.
 *
 * @param factory     Factory that creates job instances.
 * @param opts        Command line options.
 * @param app         Where to save pointer to allocated zfts_perf_app.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_create_alt_pingpong_srv(
                                            tapi_job_factory_t *factory,
                                            const zfts_perf_app_opts *opts,
                                            zfts_perf_app **app);

/**
 * Start performance measurement application.
 *
 * @param app       Pointer to zfts_perf_app structure.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_start_app(zfts_perf_app *app);

/**
 * Wait for termination of performance measurement application.
 *
 * @param app           Pointer to zfts_perf_app structure.
 * @param timeout_ms    Timeout in milliseconds.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_wait_app(zfts_perf_app *app, int timeout_ms);

/**
 * Release resources allocated for zfts_perf_app (terminating the
 * application if it is not finished yet).
 *
 * @param app           Pointer to zfts_perf_app structure.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_destroy_app(zfts_perf_app *app);

/**
 * Check whether ZF_ATTR environment variable is already set.
 * If it is not, set it to "interface=<if_name>".
 *
 * @param rpcs              RPC server.
 * @param if_name           Interface name.
 * @param unset             Will be set to @c TRUE if "ZF_ATTR" should be
 *                          unset in cleanup after calling this function.
 */
extern void zfts_try_set_zf_if(rcf_rpc_server *rpcs, const char *if_name,
                               te_bool *unset);

/**
 * Get measurement result as a double number.
 *
 * @param app         Measurement application.
 * @param val         Where to save parsed result.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_get_single_result(zfts_perf_app *app,
                                            double *val);

/**
 * Report mean RTT in a MI artefact.
 *
 * @param app_name        Name of the measurement application.
 * @param rtt             RTT value.
 *
 * @return Status code.
 */
extern te_errno zfts_perf_mean_rtt_to_mi(const char *app_name,
                                         double rtt);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !__TS_PERFORMANCE_LIB_H__ */
