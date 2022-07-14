/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions implementation
 *
 * Implementation of TAPI for Zetaferno Direct API remote calls.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
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

#include "rpc_zf.h"
#include "rpc_zf_internal.h"

#undef TE_LGR_USER
#define TE_LGR_USER "ZF TAPI RPC"

/* See description in rpc_zf.h */
int
rpc_zf_init(rcf_rpc_server *rpcs)
{
    tarpc_zf_init_in  in;
    tarpc_zf_init_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    rcf_rpc_call(rpcs, "zf_init", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_init, out.retval);
    TAPI_RPC_LOG(rpcs, zf_init, "", "%d", out.retval);

    RETVAL_ZERO_INT(zf_init, out.retval);
}

/* See description in rpc_zf.h */
int
rpc_zf_deinit(rcf_rpc_server *rpcs)
{
    tarpc_zf_deinit_in  in;
    tarpc_zf_deinit_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    rcf_rpc_call(rpcs, "zf_deinit", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_deinit, out.retval);
    TAPI_RPC_LOG(rpcs, zf_deinit, "", "%d", out.retval);

    RETVAL_ZERO_INT(zf_deinit, out.retval);
}


/* See description in rpc_zf.h */
int
rpc_zf_attr_alloc(rcf_rpc_server *rpcs, rpc_zf_attr_p *attr_out)
{
    tarpc_zf_attr_alloc_in  in;
    tarpc_zf_attr_alloc_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    rcf_rpc_call(rpcs, "zf_attr_alloc", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_attr_alloc, out.retval);
    TAPI_RPC_LOG(rpcs, zf_attr_alloc, RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(out.attr), out.retval);

    if (RPC_IS_CALL_OK(rpcs) && out.retval >= 0)
    {
        if (TAPI_RPC_NAMESPACE_CHECK(rpcs, out.attr, RPC_TYPE_NS_ZF_ATTR))
            RETVAL_INT(zf_attr_alloc, -1);
    }

    *attr_out = out.attr;
    RETVAL_ZERO_INT(zf_attr_alloc, out.retval);
}

/* See description in rpc_zf.h */
void
rpc_zf_attr_free(rcf_rpc_server *rpcs, rpc_zf_attr_p attr)
{
    tarpc_zf_attr_free_in  in;
    tarpc_zf_attr_free_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, attr, RPC_TYPE_NS_ZF_ATTR);
    in.attr = attr;

    rcf_rpc_call(rpcs, "zf_attr_free", &in, &out);

    TAPI_RPC_LOG(rpcs, zf_attr_free, RPC_PTR_FMT, "", RPC_PTR_VAL(attr));


    RETVAL_VOID(zf_attr_free);
}

/* See description in rpc_zf.h */
int
rpc_zf_attr_set_int(rcf_rpc_server *rpcs, rpc_zf_attr_p attr,
                    const char *name, int64_t val)
{
    tarpc_zf_attr_set_int_in  in;
    tarpc_zf_attr_set_int_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, attr, RPC_TYPE_NS_ZF_ATTR);

    in.attr = attr;
    in.name = (char *)name;
    in.val = val;

    rcf_rpc_call(rpcs, "zf_attr_set_int", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_attr_set_int, out.retval);
    TAPI_RPC_LOG(rpcs, zf_attr_set_int, RPC_PTR_FMT "name = %s, "
                 "val = %"TE_PRINTF_64"d", "%d", RPC_PTR_VAL(attr), name,
                 val, out.retval);

    RETVAL_INT(zf_attr_set_int, out.retval);
}


/* See description in rpc_zf.h */
int
rpc_zf_stack_alloc(rcf_rpc_server *rpcs, rpc_zf_attr_p attr,
                   rpc_zf_stack_p *stack_out)
{
    tarpc_zf_stack_alloc_in  in;
    tarpc_zf_stack_alloc_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, attr, RPC_TYPE_NS_ZF_ATTR);
    in.attr = attr;

    rcf_rpc_call(rpcs, "zf_stack_alloc", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_stack_alloc, out.retval);
    TAPI_RPC_LOG(rpcs, zf_stack_alloc, RPC_PTR_FMT ", " RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(attr), RPC_PTR_VAL(out.stack), out.retval);

    *stack_out = out.stack;
    RETVAL_ZERO_INT(zf_stack_alloc, out.retval);
}

/* See description in rpc_zf.h */
int
rpc_zf_stack_free(rcf_rpc_server *rpcs, rpc_zf_stack_p stack)
{
    tarpc_zf_stack_free_in  in;
    tarpc_zf_stack_free_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    rcf_rpc_call(rpcs, "zf_stack_free", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_stack_free, out.retval);
    TAPI_RPC_LOG(rpcs, zf_stack_free, RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(stack), out.retval);

    RETVAL_ZERO_INT(zf_stack_free, out.retval);
}

/* See description in rpc_zf.h */
int
rpc_zf_stack_is_quiescent(rcf_rpc_server *rpcs, rpc_zf_stack_p stack)
{
    tarpc_zf_stack_is_quiescent_in  in;
    tarpc_zf_stack_is_quiescent_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    rcf_rpc_call(rpcs, "zf_stack_is_quiescent", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zf_stack_is_quiescent, out.retval);
    TAPI_RPC_LOG(rpcs, zf_stack_is_quiescent, RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(stack), out.retval);

    RETVAL_INT(zf_stack_is_quiescent, out.retval);
}

/* See description in rpc_zf.h */
rpc_zf_waitable_p
rpc_zf_stack_to_waitable(rcf_rpc_server *rpcs, rpc_zf_stack_p stack)
{
    tarpc_zf_stack_to_waitable_in  in;
    tarpc_zf_stack_to_waitable_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    rcf_rpc_call(rpcs, "zf_stack_to_waitable", &in, &out);

    TAPI_RPC_LOG(rpcs, zf_stack_to_waitable, RPC_PTR_FMT, RPC_PTR_FMT,
                 RPC_PTR_VAL(stack), RPC_PTR_VAL(out.zf_waitable));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, out.zf_waitable,
                                  RPC_TYPE_NS_ZF_WAITABLE);
    CHECK_RETVAL_VAR(zf_stack_to_waitable, out.zf_waitable,
                     FALSE, RPC_NULL);

    RETVAL_RPC_PTR(zf_stack_to_waitable, out.zf_waitable);
}

/* See description in rpc_zf.h */
int
rpc_zf_stack_has_pending_work(rcf_rpc_server *rpcs, rpc_zf_stack_p stack)
{
    tarpc_zf_stack_has_pending_work_in  in;
    tarpc_zf_stack_has_pending_work_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    rcf_rpc_call(rpcs, "zf_stack_has_pending_work", &in, &out);

    CHECK_RETVAL_VAR(zf_stack_has_pending_work, out.retval,
                     out.retval < 0, -1);
    TAPI_RPC_LOG(rpcs, zf_stack_has_pending_work, RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(stack), out.retval);

    RETVAL_INT(zf_stack_has_pending_work, out.retval);
}

/* See description in rpc_zf.h */
int
rpc_zf_reactor_perform(rcf_rpc_server *rpcs, rpc_zf_stack_p stack)
{
    tarpc_zf_reactor_perform_in  in;
    tarpc_zf_reactor_perform_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    rcf_rpc_call(rpcs, "zf_reactor_perform", &in, &out);

    CHECK_RETVAL_VAR(zf_reactor_perform, out.retval,
                     out.retval < 0, -1);
    TAPI_RPC_LOG(rpcs, zf_reactor_perform, RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(stack), out.retval);

    RETVAL_INT(zf_reactor_perform, out.retval);
}

/* See description in rpc_zf.h */
int
rpc_zf_wait_for_event(rcf_rpc_server *rpcs, rpc_zf_stack_p stack)
{
    tarpc_zf_wait_for_event_in  in;
    tarpc_zf_wait_for_event_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    rcf_rpc_call(rpcs, "zf_wait_for_event", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_wait_for_event, out.retval);
    TAPI_RPC_LOG(rpcs, zf_wait_for_event, RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(stack), out.retval);

    RETVAL_ZERO_INT(zf_wait_for_event, out.retval);
}

/* See description in rpc_zf.h */
int
rpc_zf_process_events(rcf_rpc_server *rpcs, rpc_zf_stack_p stack)
{
    tarpc_zf_process_events_in  in;
    tarpc_zf_process_events_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    rcf_rpc_call(rpcs, "zf_process_events", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zf_process_events, out.retval);
    TAPI_RPC_LOG(rpcs, zf_process_events, RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(stack), out.retval);

    RETVAL_INT(zf_process_events, out.retval);
}

/* See description in rpc_zf.h */
int
rpc_zf_process_events_long(rcf_rpc_server *rpcs,
                           rpc_zf_stack_p stack,
                           int duration)
{
    tarpc_zf_process_events_long_in  in;
    tarpc_zf_process_events_long_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    in.duration = duration;

    rcf_rpc_call(rpcs, "zf_process_events_long", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zf_process_events_long, out.retval);
    TAPI_RPC_LOG(rpcs, zf_process_events_long, RPC_PTR_FMT ", %d", "%d",
                 RPC_PTR_VAL(stack), duration, out.retval);

    RETVAL_INT(zf_process_events_long, out.retval);
}


/* See description in rpc_zf.h */
int
rpc_zf_many_threads_alloc_free_stack(rcf_rpc_server *rpcs, rpc_zf_attr_p attr,
                                     int threads_num, int wait_after_alloc)
{
    tarpc_zf_many_threads_alloc_free_stack_in  in;
    tarpc_zf_many_threads_alloc_free_stack_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, attr, RPC_TYPE_NS_ZF_ATTR);
    in.attr = attr;
    in.threads_num = threads_num;
    in.wait_after_alloc = wait_after_alloc;

    rcf_rpc_call(rpcs, "zf_many_threads_alloc_free_stack", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_many_threads_alloc_free_stack,
                                          out.retval);
    TAPI_RPC_LOG(rpcs, zf_many_threads_alloc_free_stack,
                 RPC_PTR_FMT ", %d, %d", "%d",
                 RPC_PTR_VAL(attr), threads_num, wait_after_alloc,
                 out.retval);

    RETVAL_ZERO_INT(zf_many_threads_alloc_free_stack, out.retval);
}
