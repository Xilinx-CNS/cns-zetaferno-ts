/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions definition
 *
 * Definition of TAPI for Zetaferno Direct API remote calls
 * related to muxer.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru
 *
 * $Id$
 */

#ifndef ___RPC_ZF_MUXER_H__
#define ___RPC_ZF_MUXER_H__

#if HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#if HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif

#include "rcf_rpc.h"
#include "tapi_rpc_unistd.h"
#include "te_rpc_sys_socket.h"
#include "zf_talib_namespace.h"

/** RPC definition of ZF epoll events */
typedef enum rpc_zf_epoll_events {
    RPC_ZF_EPOLLIN_OVERLAPPED = 0x10000,
} rpc_zf_epoll_events;


#define RPC_ZF_EPOLL_ALL (RPC_ZF_EPOLLIN_OVERLAPPED)

#define ZF_EPOLL_EVENT_MAPPING_LIST \
            RPC_BIT_MAP_ENTRY(ZF_EPOLLIN_OVERLAPPED)

/**
 * zf_epoll_event_rpc2str()
 */
RPCBITMAP2STR(zf_epoll_event, ZF_EPOLL_EVENT_MAPPING_LIST)

/**
 * Release RPC pointer of a @b zf_waitatable object.
 *
 * @param rpcs      RPC server handle.
 * @param zf_w_ptr  RPC pointer of the Zetaferno waitable structure.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zf_waitable_free(rcf_rpc_server *rpcs,
                                rpc_zf_waitable_p zf_w_ptr);

/**
 * Allocate Zetaferno muxer set.
 *
 * @param rpcs      RPC server handle.
 * @param stack     RPC pointer to ZF stack.
 * @param ms_out    RPC pointer to allocated ZF muxer set.
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
extern int rpc_zf_muxer_alloc(rcf_rpc_server *rpcs,
                              rpc_zf_stack_p stack,
                              rpc_zf_muxer_set_p *ms_out);

/**
 * Release resources allocated for ZF muxer set.
 *
 * @param rpcs      RPC server handle.
 * @param ms        RPC pointer to ZF muxer set.
 */
extern void rpc_zf_muxer_free(rcf_rpc_server *rpcs,
                              rpc_zf_muxer_set_p ms);

/**
 * Add an endpoint to a ZF muxer set.
 *
 * @param rpcs      RPC server handle.
 * @param ms        RPC pointer to ZF muxer set.
 * @param waitable  RPC pointer to endpoint.
 * @param event     Descriptor specifying the events to
 *                  be polled on endpoint and the data to be
 *                  returned when those events are detected.
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
extern int rpc_zf_muxer_add(rcf_rpc_server *rpcs,
                            rpc_zf_muxer_set_p ms,
                            rpc_zf_waitable_p waitable,
                            struct rpc_epoll_event *event);

/**
 * Add an endpoint to a ZF muxer set.
 *
 * @param rpcs      RPC server handle.
 * @param ms        RPC pointer to ZF muxer set.
 * @param waitable  RPC pointer to endpoint.
 * @param zocket    RPC pointer to zocket to be saved
 *                  in the data associated with endpoint.
 * @param events    Events to be polled on endpoint.
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
static inline int
rpc_zf_muxer_add_simple(rcf_rpc_server *rpcs,
                        rpc_zf_muxer_set_p ms,
                        rpc_zf_waitable_p waitable,
                        rpc_ptr zocket, uint32_t events)
{
    struct rpc_epoll_event event;

    event.events = events;
    event.data.u32 = zocket;

    return rpc_zf_muxer_add(rpcs, ms, waitable, &event);
}

/**
 * Modify event data for an endpoint in a ZF muxer set.
 *
 * @param rpcs      RPC server handle.
 * @param waitable  RPC pointer to endpoint.
 * @param event     Descriptor specifying the events to
 *                  be polled on endpoint and the data to be
 *                  returned when those events are detected.
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
extern int rpc_zf_muxer_mod(rcf_rpc_server *rpcs,
                            rpc_zf_waitable_p waitable,
                            struct rpc_epoll_event *event);

/**
 * Modify event data for an endpoint in a ZF muxer set.
 *
 * @param rpcs      RPC server handle.
 * @param waitable  RPC pointer to endpoint.
 * @param zocket    RPC pointer to zocket to be saved
 *                  in the data associated with endpoint.
 * @param events    Events to be polled on endpoint.
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
static inline int
rpc_zf_muxer_mod_simple(rcf_rpc_server *rpcs,
                        rpc_zf_waitable_p waitable,
                        rpc_ptr zocket,
                        uint32_t events)
{
    struct rpc_epoll_event event;

    event.events = events;
    event.data.u32 = zocket;

    return rpc_zf_muxer_mod(rpcs, waitable, &event);
}


/**
 * Remove endpoint from ZF muxer set.
 *
 * @param rpcs      RPC server handle.
 * @param waitable  RPC pointer to endpoint.
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
extern int rpc_zf_muxer_del(rcf_rpc_server *rpcs,
                            rpc_zf_waitable_p waitable);

/**
 * Poll ZF muxer set.
 *
 * @param rpcs        RPC server handle.
 * @param ms          RPC pointer to ZF muxer set.
 * @param events      Array into which to return event descriptors.
 * @param rmaxev      Real number of elements in array.
 * @param maxevents   Number of events to be passed to @a zf_muxer_wait.
 * @param timeout     How long to wait for events (nanoseconds).
 *
 * @return Number of events on success, or a negative value.
 */
extern int
rpc_zf_muxer_wait_gen(rcf_rpc_server *rpcs, rpc_zf_muxer_set_p ms,
                      struct rpc_epoll_event *events, int rmaxev,
                      int maxevents, int64_t timeout);

/**
 * Poll ZF muxer set.
 *
 * @param rpcs        RPC server handle.
 * @param ms          RPC pointer to ZF muxer set.
 * @param events      Array into which to return event descriptors.
 * @param maxevents   Number of events to be passed to @a zf_muxer_wait.
 * @param timeout     How long to wait for events (milliseconds).
 *
 * @return Number of events on success, or a negative value.
 */
static inline int
rpc_zf_muxer_wait(rcf_rpc_server *rpcs, rpc_zf_muxer_set_p ms,
                  struct rpc_epoll_event *events,
                  int maxevents, int timeout)
{
    return rpc_zf_muxer_wait_gen(rpcs, ms, events, maxevents, maxevents,
                                 timeout < 0 ? -1 : TE_MS2NS(timeout));
}

/**
 * Find out epoll event data associated with a given waitable in a muxer
 * set.
 *
 * @param rpcs          RPC server handle.
 * @param waitable      RPC pointer to zf_waitable object.
 * @param event         Where to save event data.
 */
extern void rpc_zf_waitable_event(rcf_rpc_server *rpcs,
                                  rpc_zf_waitable_p waitable,
                                  struct rpc_epoll_event *event);

/**
 * Create a file descriptor that can be used with epoll()
 * or other standard multiplexer.
 *
 * @param rpcs          RPC server handle.
 * @param stack         RPC pointer to ZF stack object.
 * @param fd            Where to save file descriptor.
 *
 * @return 0 on success, negative value in case of failure.
 */
extern int rpc_zf_waitable_fd_get(rcf_rpc_server *rpcs,
                                  rpc_zf_stack_p stack,
                                  int *fd);

/**
 * Prime fd returned by zf_waitable_fd_get() before
 * calling iomux function.
 *
 * @param rpcs          RPC server handle.
 * @param stack         RPC pointer to ZF stack object.
 *
 * @return 0 on success, negative value in case of failure.
 */
extern int rpc_zf_waitable_fd_prime(rcf_rpc_server *rpcs,
                                    rpc_zf_stack_p stack);

/**
 * Call @b zf_muxer_mod(w, @b zf_waitable_event(w)) to reset event
 * to the same value so that muxer will be able to see it again.
 *
 * @param rpcs          RPC server handle.
 * @param waitable      RPC pointer to zf_waitable object.
 *
 * @return @c 0 on success, or negative value in case of failure.
 */
extern int rpc_zf_muxer_mod_rearm(rcf_rpc_server *rpcs,
                                  rpc_zf_waitable_p waitable);

#endif /* !___RPC_ZF_MUXER_H__ */
