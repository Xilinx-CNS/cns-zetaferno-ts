/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/**
 * @brief Multiplexer RPC routines implementation
 *
 * Zetaferno Direct API multiplexer RPC routines implementation.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "SFC Zetaferno RPC Multiplexer"
#include "te_config.h"
#include "config.h"

#include "logger_ta_lock.h"
#include "rpc_server.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "te_sockaddr.h"
#include "zf_talib_namespace.h"
#include "te_alloc.h"
#include "zf_rpc.h"
#include "te_rpc_sys_socket.h"

#include <zf/zf.h>
#include <zf/muxer.h>

/** All known ZF poll events */
#ifdef ZF_EPOLLIN_OVERLAPPED
#define ZF_EPOLL_ALL (ZF_EPOLLIN_OVERLAPPED)
#else
#define ZF_EPOLL_ALL 0
#endif

#define TARPC_ZF_EPOLL_ALL (TARPC_ZF_EPOLLIN_OVERLAPPED)

unsigned int
zf_epoll_event_rpc2h(unsigned int rpc_events)
{
    unsigned int res;

    res = epoll_event_rpc2h(rpc_events & ~TARPC_ZF_EPOLL_ALL);

    return (res
#ifdef ZF_EPOLLIN_OVERLAPPED
            | (!!(rpc_events &
                  TARPC_ZF_EPOLLIN_OVERLAPPED) * ZF_EPOLLIN_OVERLAPPED)
#endif
            );
}

unsigned int
zf_epoll_event_h2rpc(unsigned int events)
{
    unsigned int res;

    res = epoll_event_h2rpc(events & ~ZF_EPOLL_ALL);

    return (res
#ifdef ZF_EPOLLIN_OVERLAPPED
            | (!!(events &
                  ZF_EPOLLIN_OVERLAPPED) * TARPC_ZF_EPOLLIN_OVERLAPPED)
#endif
            );
}

TARPC_FUNC(zf_muxer_alloc, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_muxer_set = RPC_PTR_ID_NS_INVALID;

    struct zf_stack       *stack = NULL;

    struct zf_muxer_set   *muxer_set_out = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_muxer_set,
                                           RPC_TYPE_NS_ZF_MUXER_SET,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(out->retval = func_ptr(stack, &muxer_set_out));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    if (out->retval == 0)
        out->muxer_set = RCF_PCH_MEM_INDEX_ALLOC(muxer_set_out,
                                                 ns_muxer_set);
})

TARPC_FUNC(zf_muxer_free, {},
{
    static rpc_ptr_id_namespace ns_muxer_set = RPC_PTR_ID_NS_INVALID;

    struct zf_muxer_set *muxer_set = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_muxer_set,
                                           RPC_TYPE_NS_ZF_MUXER_SET,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(muxer_set, in->muxer_set, ns_muxer_set,);

    MAKE_CALL(func_ptr(muxer_set));
    RCF_PCH_MEM_INDEX_FREE(in->muxer_set, ns_muxer_set);
})

TARPC_FUNC(zf_muxer_add, {},
{
    static rpc_ptr_id_namespace ns_muxer_set = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_waitable = RPC_PTR_ID_NS_INVALID;

    struct zf_muxer_set *muxer_set = NULL;
    struct zf_waitable  *waitable = NULL;

    struct epoll_event  event;
    struct epoll_event *event_ptr;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_muxer_set,
                                           RPC_TYPE_NS_ZF_MUXER_SET,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_waitable,
                                           RPC_TYPE_NS_ZF_WAITABLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(muxer_set, in->muxer_set, ns_muxer_set,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(waitable, in->waitable, ns_waitable,);

    if (in->events.events_len > 0)
    {
        event_ptr = &event;
        event.events = zf_epoll_event_rpc2h(in->events.events_val[0].events);
        /* TODO: Should be substituted by correct handling of union */
        event.data.u32 =
                in->events.events_val[0].data.tarpc_epoll_data_u.u32;
    }
    else
        event_ptr = NULL;

    MAKE_CALL(out->retval = func_ptr(muxer_set, waitable, event_ptr));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zf_muxer_mod, {},
{
    static rpc_ptr_id_namespace ns_waitable = RPC_PTR_ID_NS_INVALID;

    struct zf_waitable  *waitable = NULL;

    struct epoll_event  event;
    struct epoll_event *event_ptr;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_waitable,
                                           RPC_TYPE_NS_ZF_WAITABLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(waitable, in->waitable, ns_waitable,);

    if (in->events.events_len > 0)
    {
        event_ptr = &event;
        event.events = zf_epoll_event_rpc2h(in->events.events_val[0].events);
        /* TODO: Should be substituted by correct handling of union */
        event.data.u32 =
                  in->events.events_val[0].data.tarpc_epoll_data_u.u32;
    }
    else
        event_ptr = NULL;

    MAKE_CALL(out->retval = func_ptr(waitable, event_ptr));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zf_muxer_del, {},
{
    static rpc_ptr_id_namespace ns_waitable = RPC_PTR_ID_NS_INVALID;

    struct zf_waitable  *waitable = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_waitable,
                                           RPC_TYPE_NS_ZF_WAITABLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(waitable, in->waitable, ns_waitable,);

    MAKE_CALL(out->retval = func_ptr(waitable));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zf_muxer_wait,
{
    COPY_ARG(events);
},
{
    static rpc_ptr_id_namespace ns_muxer_set = RPC_PTR_ID_NS_INVALID;

    struct zf_muxer_set *muxer_set = NULL;

    struct epoll_event  *events = NULL;
    unsigned int         len = out->events.events_len;
    unsigned int         i;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_muxer_set,
                                           RPC_TYPE_NS_ZF_MUXER_SET,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(muxer_set, in->muxer_set, ns_muxer_set,);

    if (len > 0)
    {
        events = calloc(len, sizeof(struct epoll_event));

        for (i = 0; i < len; i++)
        {
            events[i].events = zf_epoll_event_rpc2h(
                                    out->events.events_val[i].events);
            events[i].data.u32 =
                out->events.events_val[i].data.tarpc_epoll_data_u.u32;
        }
    }

    MAKE_CALL(out->retval = func(muxer_set, events, in->maxevents,
                                 in->timeout));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    for (i = 0; i < len; i++)
    {
        out->events.events_val[i].events =
            zf_epoll_event_h2rpc(events[i].events);
        /* TODO: should be substituted by correct handling of union */
        out->events.events_val[i].data.type = TARPC_ED_U32;
        out->events.events_val[i].data.tarpc_epoll_data_u.u32 =
            events[i].data.u32;
    }
    free(events);
})

TARPC_FUNC(zf_waitable_event, {},
{
    static rpc_ptr_id_namespace ns_zf_w = RPC_PTR_ID_NS_INVALID;

    struct zf_waitable        *waitable = NULL;
    const struct epoll_event  *event = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zf_w,
                                           RPC_TYPE_NS_ZF_WAITABLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(waitable, in->waitable, ns_zf_w,);

    MAKE_CALL(event = func_ptr_ret_ptr(waitable));

    if (event != NULL)
    {
        out->event.events = zf_epoll_event_h2rpc(event->events);
        /* TODO: should be substituted by correct handling of union */
        out->event.data.type = TARPC_ED_U32;
        out->event.data.tarpc_epoll_data_u.u32 = event->data.u32;
    }
    else
    {
        ERROR("zf_waitable_event() returned NULL");
    }
})

TARPC_FUNC(zf_waitable_fd_get, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;

    struct zf_stack *stack = NULL;

    int fd = -1;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(out->retval = func_ptr(stack, &fd));
    out->fd = fd;
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zf_waitable_fd_prime, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;

    struct zf_stack *stack = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(out->retval = func_ptr(stack));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

/**
 * Call @a zf_muxer_mod(waitable, @a zf_waitable_event(waitable)).
 *
 * @param w         Pointer to zf_waitable structure.
 *
 * @return @c 0 on success, or negative value in case of failure.
 */
int
zf_muxer_mod_rearm(struct zf_waitable *w)
{
    api_func_ptr_ret_ptr  waitable_event_func;
    api_func_ptr          muxer_mod_func;
    te_errno              te_rc;
    int                   rc;

    te_rc = tarpc_find_func(FALSE, "zf_waitable_event",
                            (api_func *)&waitable_event_func);
    if (te_rc != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, te_rc),
                         "failed to resolve zf_waitable_event");
        return -1;
    }

    te_rc = tarpc_find_func(FALSE, "zf_muxer_mod",
                            (api_func *)&muxer_mod_func);
    if (te_rc != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, te_rc),
                         "failed to resolve zf_muxer_mod");
        return -1;
    }

    rc = muxer_mod_func(w, waitable_event_func(w));
    TE_RPC_CONVERT_NEGATIVE_ERR(rc);
    return rc;
}

TARPC_FUNC(zf_muxer_mod_rearm, {},
{
    static rpc_ptr_id_namespace ns_waitable = RPC_PTR_ID_NS_INVALID;

    struct zf_waitable  *waitable = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_waitable,
                                           RPC_TYPE_NS_ZF_WAITABLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(waitable, in->waitable, ns_waitable,);

    MAKE_CALL(out->retval = func_ptr(waitable));
})
