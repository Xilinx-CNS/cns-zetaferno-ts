/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Zetaferno API Test Suite
 *
 * Auxiliary functions for performance package.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_LGR_USER "Performance lib"

#include "performance_lib.h"
#include "tapi_job.h"
#include "tapi_job_opt.h"
#include "te_mi_log.h"

/** Number of output channels per application */
#define OUT_CHAN_NUM 2

/** Timeout for graceful termination of a job, in ms */
#define DESTROY_TIMEOUT 1000

/** Maximum length of ZF_ATTR value */
#define ZF_ATTR_LEN 1024

/** Performance measurement application */
struct zfts_perf_app {
    tapi_job_t *job;  /**< TE job */
    tapi_job_channel_t *out_chs[OUT_CHAN_NUM];  /**< Output channels */

    tapi_job_channel_t *out_filter; /**< Filter to get RTT value from
                                         stdout */
};

/* See description in performance_lib.h */
const zfts_perf_app_opts zfts_perf_app_opts_def = {
    .clnt_addr = NULL,
    .srv_addr = NULL,
    .payload_size = TAPI_JOB_OPT_UINT_UNDEF,
    .iters = TAPI_JOB_OPT_UINT_UNDEF,
    .print_ts = FALSE,
    .use_muxer = FALSE,
    .overlapped_delay = TAPI_JOB_OPT_UINT_UNDEF,
};

/** Common command-line options */
#define COMMON_OPTS \
    TAPI_JOB_OPT_UINT_T("-s", FALSE, NULL, zfts_perf_app_opts,  \
                        payload_size),                          \
    TAPI_JOB_OPT_UINT_T("-i", FALSE, NULL, zfts_perf_app_opts,  \
                        iters),                                 \
    TAPI_JOB_OPT_BOOL("-t", zfts_perf_app_opts, print_ts),      \
    TAPI_JOB_OPT_BOOL("-m", zfts_perf_app_opts, use_muxer),     \
    TAPI_JOB_OPT_UINT_T("-c", FALSE, NULL, zfts_perf_app_opts,  \
                        overlapped_delay)

/** Common filters to log stdout and stderr of an application */
#define COMMON_FILTERS \
    {                                     \
        .use_stdout = TRUE,               \
        .log_level = TE_LL_RING,          \
        .readable = FALSE,                \
        .filter_name = "out",             \
    },                                    \
    {                                     \
        .use_stderr = TRUE,               \
        .log_level = TE_LL_ERROR,         \
        .readable = FALSE,                \
        .filter_name = "err",             \
    }

/**
 * Create and initialize zfts_perf_app structure for performance
 * measurement application.
 *
 * @param factory     Factory that creates job instances.
 * @param app_path    Path to the application.
 * @param opt_binds   Bindings between application command line
 *                    options and zfts_perf_app_opts structure.
 * @param opts        Command line options.
 * @param app_out     Where to save pointer to allocated and
 *                    initialized zfts_perf_app structure.
 *
 * @return Status code.
 */
static te_errno
create_app(tapi_job_factory_t *factory,
           const char *app_path,
           const tapi_job_opt_bind *opt_binds,
           const zfts_perf_app_opts *opts,
           zfts_perf_app **app_out)
{
    te_errno rc;
    te_vec args = TE_VEC_INIT(char *);
    zfts_perf_app *app;

    tapi_job_simple_filter_t *filters = TAPI_JOB_SIMPLE_FILTERS(
                                              COMMON_FILTERS);

    app = tapi_calloc(1, sizeof(*app));

    rc = tapi_job_opt_build_args(app_path, opt_binds, opts, &args);
    if (rc != 0)
        goto cleanup;

    rc = tapi_job_simple_create(factory,
                                &(tapi_job_simple_desc_t){
                                    .program = app_path,
                                    .argv = (const char **)args.data.ptr,
                                    .job_loc = &app->job,
                                    .stdout_loc = &app->out_chs[0],
                                    .stderr_loc = &app->out_chs[1],
                                    .filters = filters,
                                });

    if (rc != 0)
        goto cleanup;

    *app_out = app;

cleanup:

    if (rc != 0)
        free(app);

    te_vec_deep_free(&args);

    return rc;
}

/**
 * Create a filter to get reported RTT value from measurement
 * application output.
 *
 * @param app     Pointer to application structure.
 *
 * @return Status code.
 */
static te_errno
create_out_filter(zfts_perf_app *app)
{
    tapi_job_channel_t *out_filter;
    te_errno rc;

    rc = tapi_job_attach_filter(TAPI_JOB_CHANNEL_SET(app->out_chs[0]),
                                "RTT filter", TRUE, 0, &out_filter);
    if (rc != 0)
        return rc;

    rc = tapi_job_filter_add_regexp(out_filter, "(\\d+\\.\\d+)", 1);
    if (rc != 0)
        return rc;

    app->out_filter = out_filter;
    return 0;
}

/* See description in performance_lib.h */
te_errno
zfts_perf_create_udp_pingpong_clnt(tapi_job_factory_t *factory,
                                   const zfts_perf_app_opts *opts,
                                   zfts_perf_app **app)
{
    const tapi_job_opt_bind opt_binds[] = TAPI_JOB_OPT_SET(
        COMMON_OPTS,
        TAPI_JOB_OPT_ADDR_PORT_PTR("ping", FALSE, zfts_perf_app_opts,
                                   clnt_addr),
        TAPI_JOB_OPT_ADDR_PORT_PTR(NULL, FALSE, zfts_perf_app_opts,
                                   srv_addr)
    );

    te_errno rc;

    rc = create_app(factory, "zfudppingpong", opt_binds, opts, app);
    if (rc != 0)
        return rc;

    return create_out_filter(*app);
}

/* See description in performance_lib.h */
te_errno
zfts_perf_create_udp_pingpong_srv(tapi_job_factory_t *factory,
                                  const zfts_perf_app_opts *opts,
                                  zfts_perf_app **app)
{
    const tapi_job_opt_bind opt_binds[] = TAPI_JOB_OPT_SET(
          COMMON_OPTS,
          TAPI_JOB_OPT_ADDR_PORT_PTR("pong", FALSE, zfts_perf_app_opts,
                                     srv_addr),
          TAPI_JOB_OPT_ADDR_PORT_PTR(NULL, FALSE, zfts_perf_app_opts,
                                     clnt_addr)
    );

    return create_app(factory, "zfudppingpong", opt_binds, opts, app);
}

/* See description in performance_lib.h */
te_errno
zfts_perf_create_tcp_pingpong_clnt(tapi_job_factory_t *factory,
                                   const zfts_perf_app_opts *opts,
                                   zfts_perf_app **app)
{
    te_errno rc;

    const tapi_job_opt_bind opt_binds[] = TAPI_JOB_OPT_SET(
          COMMON_OPTS,
          TAPI_JOB_OPT_ADDR_PORT_PTR("ping", FALSE, zfts_perf_app_opts,
                                     srv_addr)
    );

    rc = create_app(factory, "zftcppingpong", opt_binds, opts, app);
    if (rc != 0)
        return rc;

    return create_out_filter(*app);
}

/* See description in performance_lib.h */
te_errno
zfts_perf_create_tcp_pingpong_srv(tapi_job_factory_t *factory,
                                  const zfts_perf_app_opts *opts,
                                  zfts_perf_app **app)
{
    const tapi_job_opt_bind opt_binds[] = TAPI_JOB_OPT_SET(
          COMMON_OPTS,
          TAPI_JOB_OPT_ADDR_PORT_PTR("pong", FALSE, zfts_perf_app_opts,
                                     srv_addr)
    );

    return create_app(factory, "zftcppingpong", opt_binds, opts, app);
}

/* See description in performance_lib.h */
te_errno
zfts_perf_create_alt_pingpong_clnt(tapi_job_factory_t *factory,
                                   const zfts_perf_app_opts *opts,
                                   zfts_perf_app **app)
{
    te_errno rc;

    const tapi_job_opt_bind opt_binds[] = TAPI_JOB_OPT_SET(
          COMMON_OPTS,
          TAPI_JOB_OPT_ADDR_PORT_PTR("ping", FALSE, zfts_perf_app_opts,
                                     srv_addr)
    );

    rc = create_app(factory, "zfaltpingpong", opt_binds, opts, app);
    if (rc != 0)
        return rc;

    return create_out_filter(*app);
}

/* See description in performance_lib.h */
te_errno
zfts_perf_create_alt_pingpong_srv(tapi_job_factory_t *factory,
                                  const zfts_perf_app_opts *opts,
                                  zfts_perf_app **app)
{
    const tapi_job_opt_bind opt_binds[] = TAPI_JOB_OPT_SET(
          COMMON_OPTS,
          TAPI_JOB_OPT_ADDR_PORT_PTR("pong", FALSE, zfts_perf_app_opts,
                                     srv_addr)
    );

    return create_app(factory, "zfaltpingpong", opt_binds, opts, app);
}

/* See description in performance_lib.h */
te_errno
zfts_perf_start_app(zfts_perf_app *app)
{
    return tapi_job_start(app->job);
}

/* See description in performance_lib.h */
te_errno
zfts_perf_wait_app(zfts_perf_app *app, int timeout_ms)
{
    te_errno rc;
    tapi_job_status_t status;

    rc = tapi_job_wait(app->job, timeout_ms, &status);
    if (rc != 0)
        return rc;

    TAPI_JOB_CHECK_STATUS(status);
    return 0;
}

/* See description in performance_lib.h */
te_errno
zfts_perf_destroy_app(zfts_perf_app *app)
{
    te_errno rc;

    if (app == NULL)
        return 0;

    rc = tapi_job_destroy(app->job, DESTROY_TIMEOUT);
    free(app);

    return rc;
}

/* See description in performance_lib.h */
void
zfts_try_set_zf_if(rcf_rpc_server *rpcs, const char *if_name,
                   te_bool *unset)
{
    te_string zf_attr = TE_STRING_INIT_STATIC(ZF_ATTR_LEN);
    char *cur_val = NULL;

    cur_val = rpc_getenv(rpcs, "ZF_ATTR");
    if (cur_val != NULL)
    {
        free(cur_val);
        *unset = FALSE;
        return;
    }

    *unset = TRUE;
    te_string_append(&zf_attr, "interface=%s", if_name);
    rpc_setenv(rpcs, "ZF_ATTR", zf_attr.ptr, 1);
}

/* See description in performance_lib.h */
te_errno
zfts_perf_get_single_result(zfts_perf_app *app, double *val)
{
    te_errno rc;
    tapi_job_buffer_t buf = TAPI_JOB_BUFFER_INIT;
    char *endptr = NULL;
    int saved_errno = errno;

    rc = tapi_job_receive(TAPI_JOB_CHANNEL_SET(app->out_filter),
                          0, &buf);
    if (rc != 0)
        return rc;

    errno = 0;
    *val = strtod(buf.data.ptr, &endptr);
    if (endptr == buf.data.ptr || *endptr != '\0')
    {
        ERROR("%s(): double value '%s' is malformed", __FUNCTION__,
              buf.data.ptr);
        return TE_EINVAL;
    }
    else if ((fabs(*val) == HUGE_VAL || *val == 0) && errno == ERANGE)
    {
        ERROR("%s(): double value '%s' is out of range", __FUNCTION__,
              buf.data.ptr);
        return TE_EINVAL;
    }

    errno = saved_errno;
    te_string_free(&buf.data);
    return 0;
}

/* See description in performance_lib.h */
te_errno
zfts_perf_mean_rtt_to_mi(const char *app_name, double mean_rtt)
{
    te_mi_logger *logger;
    te_errno rc;

    rc = te_mi_logger_meas_create(app_name, &logger);
    if (rc != 0)
        return rc;

    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_RTT, "RTT",
                          TE_MI_MEAS_AGGR_MEAN, mean_rtt,
                          TE_MI_MEAS_MULTIPLIER_MICRO);

    te_mi_logger_destroy(logger);
    return 0;
}
