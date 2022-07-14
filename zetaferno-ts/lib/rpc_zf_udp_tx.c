/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions implementation
 *
 * Implementation of TAPI for Zetaferno Direct API remote
 * calls related to UDP TX zockets.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#include "te_config.h"

#if HAVE_STRING_H
#include <string.h>
#endif

#include "tapi_rpc_internal.h"
#include "tapi_rpc_unistd.h"
#include "te_rpc_sys_socket.h"
#include "te_rpc_sys_epoll.h"
#include "zf_test.h"

#include "rpc_zf_internal.h"
#include "rpc_zf_udp_tx.h"

#undef TE_LGR_USER
#define TE_LGR_USER "ZF TAPI UDP TX RPC"

/* See description in rpc_zf_udp_tx.h */
int
rpc_zfut_alloc_gen(rcf_rpc_server *rpcs, rpc_zfut_p *us_out,
                   rpc_zf_stack_p stack, const struct sockaddr *laddr,
                   socklen_t laddrlen, te_bool fwd_laddrlen,
                   const struct sockaddr *raddr, socklen_t raddrlen,
                   te_bool fwd_raddrlen, int flags, rpc_zf_attr_p attr)
{
    tarpc_zfut_alloc_in  in;
    tarpc_zfut_alloc_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, attr, RPC_TYPE_NS_ZF_ATTR);
    in.attr = attr;

    sockaddr_input_h2rpc(laddr, &in.laddr);
    in.laddrlen = laddrlen;
    in.fwd_laddrlen = fwd_laddrlen;
    sockaddr_input_h2rpc(raddr, &in.raddr);
    in.raddrlen = raddrlen;
    in.fwd_raddrlen = fwd_raddrlen;
    in.flags = flags;

    rcf_rpc_call(rpcs, "zfut_alloc", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfut_alloc, out.retval);
    /* TODO: print flags as a string. */
    TAPI_RPC_LOG(rpcs, zfut_alloc,
                 RPC_PTR_FMT ", " RPC_PTR_FMT ", "
                 "laddr = " ZFTS_ADDR_FMT ", "
                 "raddr = " ZFTS_ADDR_FMT ", flags = %d, "
                 RPC_PTR_FMT, "%d", RPC_PTR_VAL(out.utx),
                 RPC_PTR_VAL(stack), ZFTS_ADDR_VAL(laddr),
                 ZFTS_ADDR_VAL(raddr), flags, RPC_PTR_VAL(attr),
                 out.retval);

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, out.utx, RPC_TYPE_NS_ZFUT);
    *us_out = out.utx;

    RETVAL_ZERO_INT(zfut_alloc, out.retval);
}

/* See description in rpc_zf_udp_tx.h */
int
rpc_zfut_free(rcf_rpc_server *rpcs, rpc_zfut_p utx)
{
    tarpc_zfut_free_in  in;
    tarpc_zfut_free_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, utx, RPC_TYPE_NS_ZFUT);
    in.utx = utx;

    rcf_rpc_call(rpcs, "zfut_free", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfut_free, out.retval);
    TAPI_RPC_LOG(rpcs, zfut_free, RPC_PTR_FMT, "%d", RPC_PTR_VAL(utx),
                 out.retval);

    RETVAL_ZERO_INT(zfut_free, out.retval);
}

/* See description in rpc_zf_udp_tx.h */
int
rpc_zfut_send(rcf_rpc_server *rpcs, rpc_zfut_p utx, rpc_iovec *iov,
              size_t iovcnt, rpc_zfut_flags flags)
{
    tarpc_zfut_send_in  in;
    tarpc_zfut_send_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, utx, RPC_TYPE_NS_ZFUT);
    in.utx = utx;
    in.iovcnt = in.iovec.iovec_len = iovcnt;
    in.flags = flags;
    in.iovec.iovec_val = TE_ALLOC(sizeof(*in.iovec.iovec_val) *
                                  in.iovec.iovec_len);

    rpc_iovec2tarpc_iovec(iov, in.iovec.iovec_val, iovcnt);

    rcf_rpc_call(rpcs, "zfut_send", &in, &out);
    free(in.iovec.iovec_val);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_iovec2str(iov, iovcnt, &log_strbuf);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zfut_send, out.retval);
    TAPI_RPC_LOG(rpcs, zfut_send, RPC_PTR_FMT ", iov = %s, iovcnt = %d, "
                 "flags %s", "%d", RPC_PTR_VAL(utx), log_strbuf.ptr,
                 iovcnt, zfut_flags_rpc2str(flags), out.retval);

    RETVAL_INT(zfut_send, out.retval);
}

/* See description in rpc_zf_udp_tx.h */
int
rpc_zfut_get_mss(rcf_rpc_server *rpcs, rpc_zfut_p utx)
{
    tarpc_zfut_get_mss_in  in;
    tarpc_zfut_get_mss_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, utx, RPC_TYPE_NS_ZFUT);
    in.utx = utx;

    rcf_rpc_call(rpcs, "zfut_get_mss", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zfut_get_mss, out.retval);
    TAPI_RPC_LOG(rpcs, zfut_get_mss, RPC_PTR_FMT, "%d", RPC_PTR_VAL(utx),
                 out.retval);

    RETVAL_INT(zfut_get_mss, out.retval);
}

/* See description in rpc_zf_udp_tx.h */
int
rpc_zfut_send_single(rcf_rpc_server *rpcs, rpc_zfut_p utx, const void *buf,
                     size_t buflen)
{
    tarpc_zfut_send_single_in  in;
    tarpc_zfut_send_single_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, utx, RPC_TYPE_NS_ZFUT);
    in.utx = utx;
    in.buf.buf_val = (uint8_t *)buf;
    in.buf.buf_len = buflen;

    rcf_rpc_call(rpcs, "zfut_send_single", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zfut_send_single, out.retval);
    TAPI_RPC_LOG(rpcs, zfut_send_single, RPC_PTR_FMT "buf = %p, buflen = %"
                 TE_PRINTF_SIZE_T"u", "%d", RPC_PTR_VAL(utx), buf, buflen,
                 out.retval);

    RETVAL_INT(zfut_send_single, out.retval);
}

/* See description in rpc_zf_udp_tx.h */
int
rpc_zfut_flooder(rcf_rpc_server *rpcs, rpc_zf_stack_p stack, rpc_zfut_p utx,
                 zfts_send_function send_func, int dgram_size,
                 int iovcnt, int duration, uint64_t *stats,
                 uint64_t *errors)
{
    tarpc_zfut_flooder_in  in;
    tarpc_zfut_flooder_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, utx, RPC_TYPE_NS_ZFUT);
    in.utx = utx;
    in.send_func = send_func;
    in.dgram_size = dgram_size;
    in.iovcnt = iovcnt;
    in.duration = duration;

    rcf_rpc_call(rpcs, "zfut_flooder", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfut_flooder, out.retval);

    TAPI_RPC_LOG(rpcs, zfut_flooder, RPC_PTR_FMT", "RPC_PTR_FMT
                 ", dgram_size = %d, duration = %d, stats = %llu, "
                 "errors = %llu", "%d", RPC_PTR_VAL(stack),
                 RPC_PTR_VAL(utx), dgram_size, duration, out.stats,
                 out.errors, out.retval);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
    {
        if (stats != NULL)
            *stats = out.stats;
        if (errors != NULL)
            *errors = out.errors;
    }

    RETVAL_ZERO_INT(zfut_flooder, out.retval);
}

/* See description in rpc_zf_udp_tx.h */
rpc_zf_waitable_p
rpc_zfut_to_waitable(rcf_rpc_server *rpcs, rpc_zfut_p utx)
{
    tarpc_zfut_to_waitable_in  in;
    tarpc_zfut_to_waitable_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, utx, RPC_TYPE_NS_ZFUT);
    in.utx = utx;

    rcf_rpc_call(rpcs, "zfut_to_waitable", &in, &out);

    CHECK_RETVAL_VAR(zfut_to_waitable, out.zf_waitable, FALSE, 0);
    TAPI_RPC_LOG(rpcs, zfut_to_waitable, RPC_PTR_FMT, RPC_PTR_FMT,
                 RPC_PTR_VAL(utx), RPC_PTR_VAL(out.zf_waitable));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, out.zf_waitable,
                                  RPC_TYPE_NS_ZF_WAITABLE);

    RETVAL_RPC_PTR(zfut_to_waitable, out.zf_waitable);
}

/* See description in rpc_zf_udp_tx.h */
unsigned int
rpc_zfut_get_header_size(rcf_rpc_server *rpcs, rpc_zfut_p utx)
{
    tarpc_zfut_get_header_size_in  in;
    tarpc_zfut_get_header_size_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, utx, RPC_TYPE_NS_ZFUT);
    in.utx = utx;

    rcf_rpc_call(rpcs, "zfut_get_header_size", &in, &out);

    TAPI_RPC_LOG(rpcs, zfut_get_header_size,
                 RPC_PTR_FMT, "%u",
                 RPC_PTR_VAL(utx), out.retval);

    TAPI_RPC_OUT(zfut_get_header_size, FALSE);
    return out.retval;
}

/* See description in rpc_zf_udp_tx.h */
int
rpc_zfut_get_tx_timestamps(rcf_rpc_server *rpcs, rpc_zfut_p utx,
                           tarpc_zf_pkt_report *reports, int *count)
{
    tarpc_zfut_get_tx_timestamps_in in;
    tarpc_zfut_get_tx_timestamps_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, utx, RPC_TYPE_NS_ZFUT);
    in.utx = utx;
    in.reports.reports_val = reports;
    in.reports.reports_len = *count;
    in.count = *count;

    rcf_rpc_call(rpcs, "zfut_get_tx_timestamps", &in, &out);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
    {
        memcpy(reports, out.reports.reports_val,
               sizeof(tarpc_zf_pkt_report) * out.reports.reports_len);
        *count = out.count;
    }

    te_string_cut(&log_strbuf, log_strbuf.len);
    zf_pkt_reports_rpc2str(reports, *count, &log_strbuf);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfut_get_tx_timestamps,
                                          out.retval);

    TAPI_RPC_LOG(rpcs, zfut_get_tx_timestamps, RPC_PTR_FMT ", %s, %d", "%d",
                 RPC_PTR_VAL(utx), log_strbuf.ptr, *count,
                 out.retval);

    RETVAL_ZERO_INT(zfut_get_tx_timestamps, out.retval);
}
