/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Auxiliary test API for UDP RX zockets
 *
 * Definition of auxiliary test API to work with UDP RX zockets.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#ifndef ___ZFTS_ZFUR_H__
#define ___ZFTS_ZFUR_H__

#include "te_dbuf.h"

/** Maximum number of ZF UDP endpoints. */
#define ZFTS_MAX_UDP_ENDPOINTS 64

/** Theoretical maximum datagrams number, which fit to the ZF UDP receive
 * buffer. */
#define ZFTS_MAX_DGRAMS_NUM 64

/**
 * Receiving functions.
 */
typedef enum {
    ZFTS_ZFUR_ZC_RECV = 0, /**< UDP zero-copy receive. */
} zfts_recv_function;

/**
 * Receiving functions list, can be passed to macro @b TEST_GET_ENUM_PARAM.
 */
#define ZFTS_RECV_FUNCTIONS  \
    { "zfur_zc_recv", ZFTS_ZFUR_ZC_RECV }

/**
 * Read datagrams using @b rpc_zfur_zc_recv. Do all required routines to
 * perform the call.
 *
 * @note The function jumps to @b cleanup in case of fail, i.e. if any of
 * Zetaferno functions returns a negative value or any internal problem in
 * used RPCs.
 *
 * @param rpcs      RPC server handle.
 * @param urx       ZF UDP RX zocket.
 * @param iov       Iov vectors array.
 * @param iovcnt    The array length.
 *
 * @return Received datagrams number.
 */
extern size_t zfts_zfur_zc_recv(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                                rpc_iovec *iov, size_t iovcnt);

/**
 * Read datagram using @b rpc_zfur_zc_recv.
 *
 * @note The function jumps to @b cleanup in case of fail, i.e. if any of
 * Zetaferno functions returns a negative value or any internal problem in
 * used RPCs.
 *
 * @param rpcs      RPC server handle.
 * @param urx       ZF UDP RX zocket.
 * @param buf       Buffer where to save data.
 * @param len       Length of buffer.
 *
 * @return Length of received data.
 */
extern size_t zfts_zfur_recv(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                             char *buf, size_t len);


/**
 * Read all available data for UDP RX zocket.
 *
 * @note The function jumps to @b cleanup in case of fail, i.e. if any of
 * Zetaferno functions returns a negative value or any internal problem in
 * used RPCs.
 *
 * @param rpcs                RPC server handle.
 * @param stack               ZF stack.
 * @param zocket              ZF UDP RX zocket.
 * @param received_data       Buffer where to save data.
 *
 * @return Length of received data.
 */
extern size_t zfts_zfur_read_data(rcf_rpc_server *rpcs,
                                  rpc_zf_stack_p stack,
                                  rpc_zfur_p zocket,
                                  te_dbuf *received_data);

/**
 * Receive datagrams using @b rpc_zfur_zc_recv until all requested
 * (@p iovcnt) datagrams are received.
 *
 * @note The function jumps to @b cleanup in case of fail.
 *
 * @param rpcs      RPC server handle.
 * @param stack     RPC pointer identifier of ZF stack object.
 * @param urx       ZF UDP RX zocket.
 * @param iov       Iov vectors array.
 * @param iovcnt    The array length.
 */
extern void zfts_zfur_zc_recv_all(rcf_rpc_server *rpcs,
                                  rpc_zf_stack_p stack, rpc_zfur_p urx,
                                  rpc_iovec *iov, size_t iovcnt);

/**
 * Bind ZF UDP RX zocket using @p rpc_zfur_addr_bind, but use @c INADDR_ANY
 * instead of specific address @p src_addr.
 *
 * @note The function jumps to @b cleanup in case of fail.
 *
 * @param rpcs      RPC server handle.
 * @param urx       ZF UDP RX zocket.
 * @param src_addr  Source address.
 * @param dst_addr  Destination address.
 */
extern void zfts_zfur_bind_to_wildcard(rcf_rpc_server *rpcs,
                                       rpc_zfur_p urx,
                                       const struct sockaddr *src_addr,
                                       const struct sockaddr *dst_addr);

/**
 * Check there is no datagrams to read on the zocket.
 *
 * @param rpcs      RPC server handle.
 * @param stack     RPC pointer identifier of ZF stack object.
 * @param urx       ZF UDP RX zocket.
 */
extern void zfts_zfur_check_not_readable(rcf_rpc_server *rpcs,
                                         rpc_zf_stack_p stack,
                                         rpc_zfur_p urx);

/**
 * Transmit a few datagrams from @p tst_s to IUT, receive datagrams and
 * validate data.
 *
 * @param pco_iut   IUT RPC server.
 * @param stack     RPC pointer to Zetaferno stack object.
 * @param urx       RPC pointer to Zetaferno UDP RX zocket object.
 * @param pco_tst   Tester RPC server.
 * @param tst_s     Tester socket.
 * @param iut_addr  IUT address, can be @c NULL.
 */
extern void zfts_zfur_check_reception(rcf_rpc_server *pco_iut,
                                      rpc_zf_stack_p stack,
                                      rpc_zfur_p urx,
                                      rcf_rpc_server *pco_tst,
                                      int tst_s,
                                      const struct sockaddr *iut_addr);

/**
 * Bind UDP RX zocket specifying local and remote addresses types.
 *
 * @param rpcs              RPC server handle.
 * @param urx               ZF UDP RX zocket.
 * @param local_addr_type   Local address type.
 * @param local_addr        Local address to bind.
 * @param local_addr_out    Updated local address or @c NULL, should be
 *                          freed. In the most cases user gets an address
 *                          using @b tapi_env, which returns @c const
 *                          address and declares it as unchangeable. From
 *                          the other hand the address (port number) can be
 *                          changed due to zetaferno API, so the new address
 *                          is allocated to return changed address if it is
 *                          required.
 * @param remote_addr_type  Remote address type.
 * @param remote_addr       Remote address to bind.
 * @param flags             Binding flags.
 *
 * @return Frowarding @a rpc_zfur_addr_bind() return code.
 */
extern int zfts_zfur_bind(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                          tapi_address_type local_addr_type,
                          const struct sockaddr *local_addr,
                          struct sockaddr **local_addr_out,
                          tapi_address_type remote_addr_type,
                          const struct sockaddr *remote_addr, int flags);

#endif /* !___ZFTS_ZFUR_H__ */
