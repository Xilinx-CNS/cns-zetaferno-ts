/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions implementation
 *
 * Implementation of TAPI for Zetaferno Direct API remote
 * calls related to alternative queues.
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
#include "rpc_zf_alts.h"

#undef TE_LGR_USER
#define TE_LGR_USER "ZF TAPI ALTS RPC"

/* See description in rpc_zf.h */
int
rpc_zf_alternatives_alloc(rcf_rpc_server *rpcs,
                          rpc_zf_stack_p stack,
                          rpc_zf_attr_p attr,
                          rpc_zf_althandle *alt_out)
{
    tarpc_zf_alternatives_alloc_in  in;
    tarpc_zf_alternatives_alloc_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, attr, RPC_TYPE_NS_ZF_ATTR);
    in.attr = attr;

    rcf_rpc_call(rpcs, "zf_alternatives_alloc", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_alternatives_alloc,
                                          out.retval);
    TAPI_RPC_LOG(rpcs, zf_alternatives_alloc,
                 RPC_PTR_FMT ", " RPC_PTR_FMT ", %"TE_PRINTF_64"u", "%d",
                 RPC_PTR_VAL(stack), RPC_PTR_VAL(attr), out.alt_out,
                 out.retval);
    *alt_out = out.alt_out;

    RETVAL_INT(zf_alternatives_alloc, out.retval);
}

/* See description in rpc_zf_alts.h */
int
rpc_zf_alternatives_release(rcf_rpc_server *rpcs,
                            rpc_zf_stack_p stack,
                            rpc_zf_althandle alt)
{
    tarpc_zf_alternatives_release_in  in;
    tarpc_zf_alternatives_release_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    in.alt = alt;

    rcf_rpc_call(rpcs, "zf_alternatives_release", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_alternatives_release,
                                          out.retval);
    TAPI_RPC_LOG(rpcs, zf_alternatives_release,
                 RPC_PTR_FMT ", %"TE_PRINTF_64"u", "%d",
                 RPC_PTR_VAL(stack), in.alt,
                 out.retval);

    RETVAL_INT(zf_alternatives_release, out.retval);
}

/* See description in rpc_zf.h */
int
rpc_zf_alternatives_send(rcf_rpc_server *rpcs,
                         rpc_zf_stack_p stack,
                         rpc_zf_althandle alt)
{
    tarpc_zf_alternatives_send_in  in;
    tarpc_zf_alternatives_send_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    in.alt = alt;

    rcf_rpc_call(rpcs, "zf_alternatives_send", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_alternatives_send,
                                          out.retval);
    TAPI_RPC_LOG(rpcs, zf_alternatives_send,
                 RPC_PTR_FMT ", %"TE_PRINTF_64"u", "%d",
                 RPC_PTR_VAL(stack), in.alt,
                 out.retval);

    RETVAL_INT(zf_alternatives_send, out.retval);
}

/* See description in rpc_zf.h */
int
rpc_zf_alternatives_cancel(rcf_rpc_server *rpcs,
                           rpc_zf_stack_p stack,
                           rpc_zf_althandle alt)
{
    tarpc_zf_alternatives_cancel_in  in;
    tarpc_zf_alternatives_cancel_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    in.alt = alt;

    rcf_rpc_call(rpcs, "zf_alternatives_cancel", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_alternatives_cancel,
                                          out.retval);
    TAPI_RPC_LOG(rpcs, zf_alternatives_cancel,
                 RPC_PTR_FMT ", %"TE_PRINTF_64"u", "%d",
                 RPC_PTR_VAL(stack), in.alt,
                 out.retval);

    RETVAL_INT(zf_alternatives_cancel, out.retval);
}

/* See description in rpc_zf.h */
int
rpc_zft_alternatives_queue(rcf_rpc_server *rpcs, rpc_zft_p ts,
                           rpc_zf_althandle alt,
                           rpc_iovec *iov,
                           int iov_cnt, int flags)
{
    tarpc_zft_alternatives_queue_in  in;
    tarpc_zft_alternatives_queue_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;
    in.alt = alt;
    in.iov_cnt = in.iovec.iovec_len = iov_cnt;
    in.flags = flags;
    in.iovec.iovec_val = TE_ALLOC(sizeof(*in.iovec.iovec_val) *
                                  in.iovec.iovec_len);
    if (in.iovec.iovec_val == NULL && in.iovec.iovec_len > 0)
        TEST_FAIL("Out of memory while processing %s", __FUNCTION__);

    rpc_iovec2tarpc_iovec(iov, in.iovec.iovec_val, iov_cnt);

    rcf_rpc_call(rpcs, "zft_alternatives_queue", &in, &out);
    free(in.iovec.iovec_val);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_iovec2str(iov, iov_cnt, &log_strbuf);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_alternatives_queue,
                                          out.retval);
    /* FIXME: print flags as a string. */
    TAPI_RPC_LOG(rpcs, zft_alternatives_queue,
                 RPC_PTR_FMT ", alt = %"TE_PRINTF_64"u, iov = %s, "
                 "iovcnt = %d, flags = %d", "%d",
                 RPC_PTR_VAL(ts), alt, log_strbuf.ptr,
                 iov_cnt, flags, out.retval);

    RETVAL_INT(zft_alternatives_queue, out.retval);
}

/* See description in rpc_zf.h */
unsigned int
rpc_zf_alternatives_free_space(rcf_rpc_server *rpcs,
                               rpc_zf_stack_p stack,
                               rpc_zf_althandle alt)
{
    tarpc_zf_alternatives_free_space_in  in;
    tarpc_zf_alternatives_free_space_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    in.alt = alt;

    rcf_rpc_call(rpcs, "zf_alternatives_free_space", &in, &out);

    TAPI_RPC_LOG(rpcs, zf_alternatives_free_space,
                 RPC_PTR_FMT ", %"TE_PRINTF_64"u", "%u",
                 RPC_PTR_VAL(stack), in.alt,
                 out.retval);

    TAPI_RPC_OUT(zf_alternatives_free_space, FALSE);
    return out.retval;
}
