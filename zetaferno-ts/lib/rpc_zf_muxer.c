/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions implementation
 *
 * Implementation of TAPI for Zetaferno Direct API remote
 * calls related to muxer.
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
#include "rpc_zf_muxer.h"

#undef TE_LGR_USER
#define TE_LGR_USER "ZF TAPI MUXER RPC"

/* See description in rpc_zf_muxer.h */
int
rpc_zf_waitable_free(rcf_rpc_server *rpcs, rpc_zf_waitable_p zf_w_ptr)
{
    tarpc_zf_waitable_free_in  in;
    tarpc_zf_waitable_free_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, zf_w_ptr, RPC_TYPE_NS_ZF_WAITABLE);
    in.zf_waitable = zf_w_ptr;

    rcf_rpc_call(rpcs, "zf_waitable_free", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_waitable_free, out.retval);
    TAPI_RPC_LOG(rpcs, zf_waitable_free, RPC_PTR_FMT, "%r",
                 RPC_PTR_VAL(zf_w_ptr), out.retval);

    RETVAL_ZERO_INT(zf_waitable_free, out.retval);
}

/* See description in rpc_zf_muxer.h */
int
rpc_zf_muxer_alloc(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                   rpc_zf_muxer_set_p *ms_out)
{
    tarpc_zf_muxer_alloc_in    in;
    tarpc_zf_muxer_alloc_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    rcf_rpc_call(rpcs, "zf_muxer_alloc", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_muxer_alloc, out.retval);
    TAPI_RPC_LOG(rpcs, zf_muxer_alloc,
                 RPC_PTR_FMT ", " RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(stack), RPC_PTR_VAL(out.muxer_set),
                 out.retval);

    if (RPC_IS_CALL_OK(rpcs) && out.retval >= 0)
    {
        if (TAPI_RPC_NAMESPACE_CHECK(rpcs, out.muxer_set,
                                     RPC_TYPE_NS_ZF_MUXER_SET))
            RETVAL_INT(zf_muxer_alloc, -1);
    }

    *ms_out = out.muxer_set;
    RETVAL_ZERO_INT(zf_muxer_alloc, out.retval);
}

/* See description in rpc_zf_muxer.h */
void
rpc_zf_muxer_free(rcf_rpc_server *rpcs, rpc_zf_muxer_set_p ms)
{
    tarpc_zf_muxer_free_in  in;
    tarpc_zf_muxer_free_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ms, RPC_TYPE_NS_ZF_MUXER_SET);
    in.muxer_set = ms;

    rcf_rpc_call(rpcs, "zf_muxer_free", &in, &out);

    TAPI_RPC_LOG(rpcs, zf_muxer_free, RPC_PTR_FMT, "", RPC_PTR_VAL(ms));

    RETVAL_VOID(zf_muxer_free);
}

/**
 * Convert array of epoll events to human-readable string.
 *
 * @param rpcs      RPC server.
 * @param evts      Array of events.
 * @param n_evts    Number of events.
 * @param str       Where to put string.
 */
static void
epoll_events_rpc2str(rcf_rpc_server *rpcs,
                     struct rpc_epoll_event *evts,
                     unsigned int n_evts,
                     te_string *str)
{
    unsigned int i;

    if (evts == NULL)
        return;

    te_string_append(str, "{");

    for (i = 0; i < n_evts; ++i)
    {
        /* TODO: Correct union field should be chosen. */
        te_string_append(str, "{" RPC_PTR_FMT ", ",
                         RPC_PTR_VAL(evts[i].data.u32));

        if ((evts[i].events & RPC_ZF_EPOLL_ALL) != 0)
        {
            te_string_append(str, "%s", zf_epoll_event_rpc2str(
                                 evts[i].events & RPC_ZF_EPOLL_ALL));

            if ((evts[i].events & RPC_EPOLL_ALL) != 0)
                te_string_append(str, " | ");
        }

        if ((evts[i].events & ~RPC_ZF_EPOLL_ALL) != 0)
        {
            te_string_append(str, "%s", epoll_event_rpc2str(
                                 evts[i].events & ~RPC_ZF_EPOLL_ALL));
        }

        te_string_append(str, "}");

        if (i < n_evts - 1)
            te_string_append(str, ", ");
    }

    te_string_append(str, "}");
}

/**
 * Convert array of rpc_epoll_event structures to
 * array of tarpc_epoll_event structures.
 *
 * @param rpc_evts    Array of rpc_epoll_event structures.
 * @param tarpc_evts  Array of tarpc_epoll_event structures.
 * @param evts_num    Number of elements in arrays.
 */
static void
epoll_events_rpc2tarpc(struct rpc_epoll_event *rpc_evts,
                       tarpc_epoll_event *tarpc_evts,
                       unsigned int evts_num)
{
    unsigned int i;

    for (i = 0; i < evts_num; i++)
    {
        tarpc_evts[i].events = rpc_evts[i].events;
        tarpc_evts[i].data.type = TARPC_ED_U32;
        tarpc_evts[i].data.tarpc_epoll_data_u.u32 = rpc_evts[i].data.u32;
    }
}

/**
 * Convert array of rpc_epoll_event structures to
 * array of tarpc_epoll_event structures and set a field
 * in RPC call argument containing it.
 *
 * @param rpc_events_    Array of rpc_epoll_event structures.
 * @param tarpc_events_  Array of tarpc_epoll_event structures.
 * @param events_num_    Number of elements in arrays.
 */
#define EPOLL_EVENTS_RPC2TARPC(rpc_events_, tarpc_events_, events_num_) \
    do {                                                        \
        if (rpc_events_ != NULL)                                \
        {                                                       \
            epoll_events_rpc2tarpc(rpc_events_, tarpc_events_,  \
                                   events_num_);                \
            in.events.events_len = events_num_;                 \
            in.events.events_val = tarpc_events_;               \
        }                                                       \
        else                                                    \
        {                                                       \
            in.events.events_len = 0;                           \
            in.events.events_val = NULL;                        \
        }                                                       \
    } while (0)

/* See description in rpc_zf_muxer.h */
int
rpc_zf_muxer_add(rcf_rpc_server *rpcs, rpc_zf_muxer_set_p ms,
                 rpc_zf_waitable_p waitable, struct rpc_epoll_event *event)
{
    tarpc_zf_muxer_add_in  in;
    tarpc_zf_muxer_add_out out;

    tarpc_epoll_event evt;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ms, RPC_TYPE_NS_ZF_MUXER_SET);
    in.muxer_set = ms;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, waitable,
                                  RPC_TYPE_NS_ZF_WAITABLE);
    in.waitable = waitable;

    EPOLL_EVENTS_RPC2TARPC(event, &evt, 1);

    rcf_rpc_call(rpcs, "zf_muxer_add", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_muxer_add, out.retval);

    te_string_cut(&log_strbuf, log_strbuf.len);
    epoll_events_rpc2str(rpcs, event, 1, &log_strbuf);
    TAPI_RPC_LOG(rpcs, zf_muxer_add,
                 RPC_PTR_FMT ", " RPC_PTR_FMT ", %p%s", "%d",
                 RPC_PTR_VAL(ms), RPC_PTR_VAL(waitable),
                 event, log_strbuf.ptr, out.retval);

    RETVAL_INT(zf_muxer_add, out.retval);
}

/* See description in rpc_zf_muxer.h */
int
rpc_zf_muxer_mod(rcf_rpc_server *rpcs,
                 rpc_zf_waitable_p waitable,
                 struct rpc_epoll_event *event)
{
    tarpc_zf_muxer_mod_in  in;
    tarpc_zf_muxer_mod_out out;

    tarpc_epoll_event evt;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, waitable,
                                  RPC_TYPE_NS_ZF_WAITABLE);
    in.waitable = waitable;

    EPOLL_EVENTS_RPC2TARPC(event, &evt, 1);

    rcf_rpc_call(rpcs, "zf_muxer_mod", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_muxer_mod, out.retval);

    te_string_cut(&log_strbuf, log_strbuf.len);
    epoll_events_rpc2str(rpcs, event, 1, &log_strbuf);
    TAPI_RPC_LOG(rpcs, zf_muxer_mod,
                 RPC_PTR_FMT ", %p%s", "%d",
                 RPC_PTR_VAL(waitable),
                 event, log_strbuf.ptr, out.retval);

    RETVAL_INT(zf_muxer_mod, out.retval);
}

/* See description in rpc_zf_muxer.h */
int
rpc_zf_muxer_del(rcf_rpc_server *rpcs,
                 rpc_zf_waitable_p waitable)
{
    tarpc_zf_muxer_del_in  in;
    tarpc_zf_muxer_del_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, waitable,
                                  RPC_TYPE_NS_ZF_WAITABLE);
    in.waitable = waitable;

    rcf_rpc_call(rpcs, "zf_muxer_del", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_muxer_del, out.retval);
    TAPI_RPC_LOG(rpcs, zf_muxer_del,
                 RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(waitable), out.retval);

    RETVAL_INT(zf_muxer_del, out.retval);
}

/* See description in rpc_zf_muxer.h */
int
rpc_zf_muxer_wait_gen(rcf_rpc_server *rpcs, rpc_zf_muxer_set_p ms,
                      struct rpc_epoll_event *events, int rmaxev,
                      int maxevents, int64_t timeout)
{
    tarpc_zf_muxer_wait_in  in;
    tarpc_zf_muxer_wait_out out;

    int                i;
    tarpc_epoll_event *evts = NULL;

    if (rmaxev > 0)
        evts = calloc(rmaxev, sizeof(tarpc_epoll_event));

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ms, RPC_TYPE_NS_ZF_MUXER_SET);
    in.muxer_set = ms;

    in.timeout = timeout;
    in.maxevents = maxevents;

    EPOLL_EVENTS_RPC2TARPC(events, evts, rmaxev);

    if ((timeout > 0) && (rpcs->timeout == RCF_RPC_UNSPEC_TIMEOUT))
    {
        rpcs->timeout = TE_SEC2MS(TAPI_RPC_TIMEOUT_EXTRA_SEC) + timeout;
    }

    rcf_rpc_call(rpcs, "zf_muxer_wait", &in, &out);

    te_string_cut(&log_strbuf, log_strbuf.len);
    if (RPC_IS_CALL_OK(rpcs))
    {
        if (events != NULL && out.events.events_val != NULL)
        {
            for (i = 0; i < rmaxev; i++)
            {
                events[i].events = out.events.events_val[i].events;
                events[i].data.u32 =
                    out.events.events_val[i].data.tarpc_epoll_data_u.u32;
            }
        }
        epoll_events_rpc2str(rpcs, events, MIN(rmaxev, MAX(out.retval, 0)),
                             &log_strbuf);
    }

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zf_muxer_wait, out.retval);

    free(evts);
    TAPI_RPC_LOG(rpcs, zf_muxer_wait,
                 RPC_PTR_FMT ", %p, %d, %"TE_PRINTF_64"d", "%d %s",
                 RPC_PTR_VAL(ms), events, maxevents, timeout, out.retval,
                 log_strbuf.ptr);
    RETVAL_INT(zf_muxer_wait, out.retval);
}

/* See description in rpc_zf_muxer.h */
void
rpc_zf_waitable_event(rcf_rpc_server *rpcs,
                      rpc_zf_waitable_p waitable,
                      struct rpc_epoll_event *event)
{
    tarpc_zf_waitable_event_in  in;
    tarpc_zf_waitable_event_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, waitable, RPC_TYPE_NS_ZF_WAITABLE);
    in.waitable = waitable;

    rcf_rpc_call(rpcs, "zf_waitable_event", &in, &out);

    te_string_cut(&log_strbuf, log_strbuf.len);
    if (RPC_IS_CALL_OK(rpcs))
    {
        event->events = out.event.events;
        event->data.u32 = out.event.data.tarpc_epoll_data_u.u32;
        epoll_events_rpc2str(rpcs, event, 1, &log_strbuf);
    }

    TAPI_RPC_LOG(rpcs, zf_waitable_event, RPC_PTR_FMT, "%s",
                 RPC_PTR_VAL(waitable), log_strbuf.ptr);
}

/* See description in rpc_zf_muxer.h */
int
rpc_zf_waitable_fd_get(rcf_rpc_server *rpcs,
                       rpc_zf_stack_p stack,
                       int *fd)
{
    tarpc_zf_waitable_fd_get_in    in;
    tarpc_zf_waitable_fd_get_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    rcf_rpc_call(rpcs, "zf_waitable_fd_get", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_waitable_fd_get, out.retval);
    TAPI_RPC_LOG(rpcs, zf_waitable_fd_get,
                 RPC_PTR_FMT ", %p (%d)", "%d",
                 RPC_PTR_VAL(stack), fd, out.fd,
                 out.retval);

    *fd = out.fd;
    RETVAL_ZERO_INT(zf_waitable_fd_get, out.retval);
}

/* See description in rpc_zf_muxer.h */
int
rpc_zf_waitable_fd_prime(rcf_rpc_server *rpcs,
                         rpc_zf_stack_p stack)
{
    tarpc_zf_waitable_fd_prime_in    in;
    tarpc_zf_waitable_fd_prime_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    rcf_rpc_call(rpcs, "zf_waitable_fd_prime", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_waitable_fd_prime, out.retval);
    TAPI_RPC_LOG(rpcs, zf_waitable_fd_prime,
                 RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(stack), out.retval);

    RETVAL_ZERO_INT(zf_waitable_fd_prime, out.retval);
}

/* See description in rpc_zf_muxer.h */
int
rpc_zf_muxer_mod_rearm(rcf_rpc_server *rpcs,
                       rpc_zf_waitable_p waitable)
{
    tarpc_zf_muxer_mod_rearm_in  in;
    tarpc_zf_muxer_mod_rearm_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, waitable,
                                  RPC_TYPE_NS_ZF_WAITABLE);
    in.waitable = waitable;

    rcf_rpc_call(rpcs, "zf_muxer_mod_rearm", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_muxer_mod_rearm,
                                          out.retval);
    TAPI_RPC_LOG(rpcs, zf_muxer_mod_rearm,
                 RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(waitable), out.retval);

    RETVAL_INT(zf_muxer_mod_rearm, out.retval);
}
