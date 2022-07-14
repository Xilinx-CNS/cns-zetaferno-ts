/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions definition
 *
 * Definition of TAPI for Zetaferno Direct API remote calls
 * related to delegated sends.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru
 */

#include "rcf_rpc.h"
#include "zf_talib_namespace.h"

#include "zf/zf.h"

#ifndef ___RPC_ZF_DS_H__
#define ___RPC_ZF_DS_H__

/** Return code of @b rpc_zf_delegated_send_prepare(). */
typedef enum zf_delegated_send_rc rpc_zf_delegated_send_rc;

/**
 * Get string representation of @ref rpc_zf_delegated_send_rc.
 *
 * @param rc  Value.
 *
 * @return String representation.
 */
extern const char *zf_delegated_send_rc_rpc2str(
                                      rpc_zf_delegated_send_rc rc);

/**
 * Append string representation of zf_ds structure to TE string.
 *
 * @param ds      Pointer to zf_ds structure.
 * @param str     Pointer to TE string.
 *
 * @return Status code.
 */
extern te_errno zf_ds_h2str_append(struct zf_ds *ds, te_string *str);

/**
 * Call zf_delegated_send_prepare() before doing delegated sends.
 *
 * @param rpcs                RPC server handle.
 * @param ts                  RPC pointer to TCP zocket.
 * @param max_delegated_wnd   Bytes to reserve for future delegated sends.
 * @param cong_wnd_override   If not zero, this call acts as if congestion
 *                            window is the maximum of this value and actual
 *                            congestion window.
 * @param flags               Flags (reserved for future use).
 * @param ds                  Pointer to zf_ds structure.
 *
 * @return One of the values from @b zf_delegated_send_rc enum.
 */
extern rpc_zf_delegated_send_rc rpc_zf_delegated_send_prepare(
                                    rcf_rpc_server *rpcs,
                                    rpc_zft_p ts, int max_delegated_wnd,
                                    int cong_wnd_override,
                                    unsigned int flags,
                                    struct zf_ds *ds);

/**
 * Update packet headers for the next delegated send.
 *
 * @param rpcs        RPC server.
 * @param ds          Pointer to zf_ds structure with headers.
 * @param bytes       Payload length of the next delegated send.
 * @param push        Zero to clear PUSH flag, non-zero to set it.
 */
extern void rpc_zf_delegated_send_tcp_update(rcf_rpc_server *rpcs,
                                             struct zf_ds *ds,
                                             int bytes, int push);

/**
 * Update packet headers after delegated send of a packet.
 *
 * @param rpcs        RPC server.
 * @param ds          Pointer to zf_ds structure with headers.
 * @param bytes       Payload length of the last delegated send.
 */
extern void rpc_zf_delegated_send_tcp_advance(rcf_rpc_server *rpcs,
                                              struct zf_ds *ds,
                                              int bytes);

/**
 * Notify TCPDirect that some data have been sent via delegated sends.
 *
 * @param rpcs                RPC server handle.
 * @param ts                  RPC pointer to TCP zocket.
 * @param iov                 Pointer to the iovec array with packet
 *                            buffers.
 * @param iovlen              Number of elements in the iovec array.
 * @param flags               Flags (reserved for future use).
 *
 * @return Number of bytes completed on success (may be less than
 *         requested) or @c -1 on failure.
 */
extern int rpc_zf_delegated_send_complete(
                                  rcf_rpc_server *rpcs,
                                  rpc_zft_p ts, rpc_iovec *iov, int iovlen,
                                  int flags);

/**
 * Notify TCPDirect that remaining bytes reserved for delegated sends are
 * no longer required.
 *
 * @param rpcs            RPC server handle.
 * @param ts              RPC pointer to TCP zocket.
 *
 * @return @c 0 on success, @c -1 on failure.
 */
extern int rpc_zf_delegated_send_cancel(rcf_rpc_server *rpcs, rpc_zft_p ts);

#endif /* !___RPC_ZF_DS_H__ */
