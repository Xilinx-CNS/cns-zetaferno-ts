/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions definition
 *
 * Definition of TAPI for Zetaferno Direct API remote calls.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#ifndef ___RPC_ZF_H__
#define ___RPC_ZF_H__

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

#include "rpc_zf_tcp.h"
#include "rpc_zf_udp_rx.h"
#include "rpc_zf_udp_tx.h"
#include "rpc_zf_muxer.h"
#include "rpc_zf_alts.h"
#include "rpc_zf_ds.h"

/** Event indicating stack quiescence. */
#define RPC_EPOLLSTACKHUP RPC_EPOLLRDHUP

/** RPC definition of flags for handling overlapped receives */
typedef enum rpc_zf_zc_flags {
    RPC_ZF_OVERLAPPED_WAIT = 0x10000,
    RPC_ZF_OVERLAPPED_COMPLETE = 0x20000,
} rpc_zf_zc_flags;

/** List of mapping numerical value to string for 'rpc_zf_zc_flags' */
#define ZF_ZC_FLAGS_MAPPING_LIST \
            RPC_BIT_MAP_ENTRY(ZF_OVERLAPPED_WAIT),    \
            RPC_BIT_MAP_ENTRY(ZF_OVERLAPPED_COMPLETE)

/**
 * zf_zc_flags_rpc2str()
 */
RPCBITMAP2STR(zf_zc_flags, ZF_ZC_FLAGS_MAPPING_LIST)

/**
 * Initialize Zetaferno library.
 *
 * @param rpcs      RPC server handle.
 *
 * @return @c Zero on success and a negative value in case of fail.
 */
extern int rpc_zf_init(rcf_rpc_server *rpcs);

/**
 * Deinitialize Zetaferno library.
 *
 * @param rpcs      RPC server handle.
 *
 * @return @c Zero on success and a negative value in case of fail.
 */
extern int rpc_zf_deinit(rcf_rpc_server *rpcs);

/**
 * Allocate a Zetaferno attribute object.
 *
 * @param rpcs      RPC server handle.
 * @param attr_out  Pointer to the attribute object.
 *
 * @return @c Zero on success and a negative value in case of fail.
 */
extern int rpc_zf_attr_alloc(rcf_rpc_server *rpcs, rpc_zf_attr_p *attr_out);

/**
 * Free a Zetaferno attribute object.
 *
 * @param rpcs      RPC server handle.
 * @param attr      Pointer to the attribute object.
 */
extern void rpc_zf_attr_free(rcf_rpc_server *rpcs, rpc_zf_attr_p attr);

/**
 * Set integer value for an attribute.
 *
 * @param rpcs      RPC server handle.
 * @param attr      Pointer to the attribute object.
 * @param name      Attribute name.
 * @param val       Value to be set.
 */
extern int rpc_zf_attr_set_int(rcf_rpc_server *rpcs, rpc_zf_attr_p attr,
                               const char *name, int64_t val);

/**
 * Allocate a Zetaferno stack.
 *
 * @param rpcs      RPC server handle.
 * @param attr      Pointer to the attribute object.
 * @param stack_out Pointer to the stack object.
 *
 * @return @c Zero on success and a negative value in case of fail.
 */
extern int rpc_zf_stack_alloc(rcf_rpc_server *rpcs, rpc_zf_attr_p attr,
                              rpc_zf_stack_p *stack_out);

/**
 * Release a Zetaferno stack.
 *
 * @param rpcs      RPC server handle.
 * @param stack     Pointer to the stack object.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zf_stack_free(rcf_rpc_server *rpcs, rpc_ptr stack);

/**
 * Check whether ZF stack is quiescent.
 *
 * @param rpcs    RPC server handle
 * @param stack   RPC pointer to the stack object.
 *
 * @return Greater than zero if the stack is quiescent,
 *         @c zero if it is not, negative value in case
 *         of RPC call failure.
 */
extern int rpc_zf_stack_is_quiescent(rcf_rpc_server *rpcs,
                                     rpc_zf_stack_p stack);

/**
 * Get pointer to @b zf_waitable structure of ZF stack.
 *
 * @note Zetaferno API does not propose any cleanup operation for the
 *       returned pointer. But TE RPC pointer is allocated to keep the
 *       agent pointer. So function @b rpc_zf_waitable_free() shall be
 *       used to release the RPC pointer.
 *
 * @param rpcs      RPC server handle.
 * @param stack     RPC pointer identifier of ZF stack.
 *
 * @return RPC pointer identifier of the waitable object.
 */
extern rpc_zf_waitable_p rpc_zf_stack_to_waitable(rcf_rpc_server *rpcs,
                                                  rpc_zf_stack_p stack);

/**
 * Check whether ZF stack has pending work.
 *
 * @param rpcs    RPC server handle
 * @param stack   RPC pointer to the stack object.
 *
 * @return Greater than zero if the stack has pending work,
 *         or @c zero otherwise; negative value in case
 *         of failure.
 */
extern int rpc_zf_stack_has_pending_work(rcf_rpc_server *rpcs,
                                         rpc_zf_stack_p stack);

/**
 * Poll a Zetaferno stack for events.
 *
 * @param rpcs      RPC server handle.
 * @param stack     Pointer to the stack object.
 *
 * @return @c 1 if there were some events processed, or @c 0 otherwise.
 */
extern int rpc_zf_reactor_perform(rcf_rpc_server *rpcs, rpc_ptr stack);

/**
 * Run in the loop calling @a zf_reactor_perform until an event is observed.
 *
 * @param rpcs    RPC server handle.
 * @param stack   RPC pointer identifier of ZF stack object.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zf_wait_for_event(rcf_rpc_server *rpcs,
                                 rpc_zf_stack_p stack);

/**
 * Run in the loop calling @a zf_reactor_perform until all events are
 * processed.
 *
 * @param rpcs    RPC server handle.
 * @param stack   RPC pointer identifier of ZF stack object.
 *
 * @return Events number or a negative value in case of fail.
 */
extern int rpc_zf_process_events(rcf_rpc_server *rpcs,
                                 rpc_zf_stack_p stack);

/**
 * Call @a zf_reactor_perform repeatedly until timeout is expired.
 *
 * @param rpcs      RPC server handle.
 * @param stack     RPC pointer identifier of ZF stack object.
 * @param duration  Time to run, in milliseconds.
 *
 * @return Number of events encountered or
 *         a negative value in case of failure.
 */
extern int rpc_zf_process_events_long(rcf_rpc_server *rpcs,
                                      rpc_zf_stack_p stack,
                                      int duration);

/**
 * Create requested number of threads, each allocating ZF stack
 * and freeing it after a while in an infinite loop.
 *
 * @note This function should hang indefinitely, it is intended
 *       for checking what happens when process calling it
 *       is killed.
 *
 * @param rpcs              RPC server.
 * @param attr              RPC pointer to the ZF attributes object.
 * @param threads_num       Number of threads to create.
 * @param wait_after_alloc  How long to wait after stack allocation
 *                          (in microseconds).
 *
 * @return @c 0 on success, @c -1 on failure.
 */
extern int rpc_zf_many_threads_alloc_free_stack(rcf_rpc_server *rpcs,
                                                rpc_zf_attr_p attr,
                                                int threads_num,
                                                int wait_after_alloc);
#endif /* !___RPC_ZF_H__ */
