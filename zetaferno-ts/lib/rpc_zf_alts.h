/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions definition
 *
 * Definition of TAPI for Zetaferno Direct API remote calls
 * related to alternative queues.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru
 *
 * $Id$
 */

#ifndef ___RPC_ZF_ALTS_H__
#define ___RPC_ZF_ALTS_H__

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

/** ZF alternative handle type. */
typedef uint64_t rpc_zf_althandle;

/**
 * Acquire an ID for an alternative queue.
 *
 * @param rpcs        RPC server handle.
 * @param stack       RPC pointer to ZF stack object.
 * @param attr        RPC pointer to ZF attributes object.
 * @param alt_out     Where to save ZF Alternative queue handle.
 *
 * @return @c 0 on success, or negative value in case of failure.
 */
extern int rpc_zf_alternatives_alloc(rcf_rpc_server *rpcs,
                                     rpc_zf_stack_p stack,
                                     rpc_zf_attr_p attr,
                                     rpc_zf_althandle *alt_out);

/**
 * Release an ID for an alternative queue.
 *
 * @param rpcs        RPC server handle.
 * @param stack       RPC pointer to ZF stack object.
 * @param alt         Handle of ZF Alternative queue.
 *
 * @return @c 0 on success, or negative value in case of failure.
 */
extern int rpc_zf_alternatives_release(rcf_rpc_server *rpcs,
                                       rpc_zf_stack_p stack,
                                       rpc_zf_althandle alt);

/**
 * Release an ID for an alternative queue.
 * The same as rpc_zf_alternatives_release(), but does not go to 'cleanup' label
 *
 * @param rpcs_       RPC server handle.
 * @param stack_      RPC pointer to ZF stack object.
 * @param alt_        Handle of ZF Alternative queue.
 *
 */
#define CLEANUP_RPC_ZF_ALTERNATIVES_RELEASE(rpcs_, stack_, alt_) \
    do {                                                                       \
        if ((stack_ != RPC_NULL) && (alt_ != RPC_NULL))                        \
        {                                                                      \
            int rc_;                                                           \
                                                                               \
            RPC_AWAIT_IUT_ERROR(rpcs_);                                        \
            if ((rc_ = rpc_zf_alternatives_release(rpcs_, stack_, alt_)) != 0) \
                MACRO_TEST_ERROR;                                              \
            else                                                               \
                alt_ = RPC_NULL;                                               \
        }                                                                      \
    } while (0)

/**
 * Select an alternative queue and send messages from it.
 *
 * @param rpcs        RPC server handle.
 * @param stack       RPC pointer to ZF stack object.
 * @param alt         Handle of ZF Alternative queue.
 *
 * @return @c 0 on success, or negative value in case of failure.
 */
extern int rpc_zf_alternatives_send(rcf_rpc_server *rpcs,
                                    rpc_zf_stack_p stack,
                                    rpc_zf_althandle alt);

/**
 * Cancel an alternative queue.
 *
 * @param rpcs        RPC server handle.
 * @param stack       RPC pointer to ZF stack object.
 * @param alt         Handle of ZF Alternative queue.
 *
 * @return @c 0 on success, or negative value in case of failure.
 */
extern int rpc_zf_alternatives_cancel(rcf_rpc_server *rpcs,
                                      rpc_zf_stack_p stack,
                                      rpc_zf_althandle alt);

/**
 * Queue a TCP message for sending.
 *
 * @param rpcs        RPC server handle.
 * @param ts          RPC pointer identifier of ZF TCP zocket handle.
 * @param alt         Handle of ZF Alternative queue.
 * @param iov         Data vectors array.
 * @param iov_cnt     The array length.
 * @param flags       Flags.
 *
 * @return @c 0 on success, or negative value in case of failure.
 */
extern int rpc_zft_alternatives_queue(rcf_rpc_server *rpcs, rpc_zft_p ts,
                                      rpc_zf_althandle alt,
                                      rpc_iovec *iov,
                                      int iov_cnt, int flags);

/**
 * Query the payload size in butes of the largest packet which can
 * be sent into this alternative at this moment.
 *
 * @param rpcs        RPC server handle.
 * @param stack       RPC pointer to ZF stack object.
 * @param alt         Handle of ZF Alternative queue.
 *
 * @return Number of bytes.
 */
extern unsigned int rpc_zf_alternatives_free_space(rcf_rpc_server *rpcs,
                                                   rpc_zf_stack_p stack,
                                                   rpc_zf_althandle alt);

#endif /* !___RPC_ZF_ALTS_H__ */
