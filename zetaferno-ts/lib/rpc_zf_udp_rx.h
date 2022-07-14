/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions definition
 *
 * Definition of TAPI for Zetaferno Direct API remote calls
 * related to UDP RX zockets.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru
 *
 * $Id$
 */

#ifndef ___RPC_ZF_UDP_RX_H__
#define ___RPC_ZF_UDP_RX_H__

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

/**
 * Reserved array length in @b zfur_msg structure. The value should
 * correspond the value declared in @b  struct @b zfur_msg in ZF sources
 * header @c zf_udp.h.  */
#define ZFUR_MSG_RESERVED 4

/**
 * Structure to receive datagrams using Zetaferno API.
 */
typedef struct rpc_zfur_msg {
  int32_t         reserved[ZFUR_MSG_RESERVED]; /**< Reserved. */
  int             dgrams_left; /**< Datagrams in the queue. */
  int             flags;       /**< Reception flags. */
  int             iovcnt;      /**< In/out vectors number. */
  rpc_iovec      *iov;         /**< Vectors for datagrams reception. */
  rpc_zfur_msg_p  ptr;         /**< Pointer to the structure on the agent
                                    side. */
} rpc_zfur_msg;

/**
 * Allocate Zetaferno UDP RX zocket.
 *
 * @param rpcs      RPC server handle.
 * @param us_out    Pointer location for new UDP RX zocket.
 * @param stack     Pointer to the stack object.
 * @param attr      Pointer to the attribute object.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfur_alloc(rcf_rpc_server *rpcs, rpc_zfur_p *us_out,
                          rpc_zf_stack_p stack, rpc_zf_attr_p attr);

/**
 * Release Zetaferno UDP RX zocket.
 *
 * @param rpcs      RPC server handle.
 * @param urx       Pointer to UDP RX zocket to be freed.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfur_free(rcf_rpc_server *rpcs, rpc_zfur_p urx);

/**
 * Bind Zetaferno UDP RX zocket.
 *
 * @param rpcs          RPC server handle.
 * @param urx           Pointer to UDP RX zocket.
 * @param laddr         Local address, if zero port is passed it is replaced
 *                      by a real port number and modified in the argument.
 * @param laddrlen      Local address length.
 * @param fwd_laddrlen  Whether to use laddrlen value or not.
 * @param raddr         Remote address.
 * @param raddrlen      Remote address length.
 * @param fwd_raddrlen  Whether to use raddrlen value or not.
 * @param flags         Control flags.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfur_addr_bind_gen(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                                  struct sockaddr *laddr,
                                  socklen_t laddrlen, te_bool fwd_laddrlen,
                                  const struct sockaddr *raddr,
                                  socklen_t raddrlen, te_bool fwd_raddrlen,
                                  int flags);

/**
 * Bind Zetaferno UDP RX zocket.
 *
 * @param rpcs      RPC server handle.
 * @param urx       Pointer to UDP RX zocket.
 * @param laddr     Local address, if zero port is passed it is replaced by
 *                  a real port number and modified in the argument.
 * @param raddr     Remote address.
 * @param flags     Control flags.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
static inline int
rpc_zfur_addr_bind(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                   struct sockaddr *laddr,
                   const struct sockaddr *raddr, int flags)
{
    return rpc_zfur_addr_bind_gen(rpcs, urx, laddr, 0, FALSE,
                                  raddr, 0, FALSE, flags);
}

/**
 * Unbind Zetaferno UDP RX zocket.
 *
 * @param rpcs          RPC server handle.
 * @param urx           Pointer to UDP RX zocket.
 * @param laddr         Local address.
 * @param laddrlen      Local address length.
 * @param fwd_laddrlen  Whether to use laddrlen value or not.
 * @param raddr         Remote address.
 * @param raddrlen      Remote address length.
 * @param fwd_raddrlen  Whether to use raddrlen value or not.
 * @param flags         Control flags.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfur_addr_unbind_gen(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                                    const struct sockaddr *laddr,
                                    socklen_t laddrlen,
                                    te_bool fwd_laddrlen,
                                    const struct sockaddr *raddr,
                                    socklen_t raddrlen,
                                    te_bool fwd_raddrlen,
                                    int flags);

/**
 * Unbind Zetaferno UDP RX zocket.
 *
 * @param rpcs      RPC server handle.
 * @param urx       Pointer to UDP RX zocket.
 * @param laddr     Local address.
 * @param raddr     Remote address.
 * @param flags     Control flags.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
static inline int
rpc_zfur_addr_unbind(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                     const struct sockaddr *laddr,
                     const struct sockaddr *raddr, int flags)
{
    return rpc_zfur_addr_unbind_gen(rpcs, urx, laddr, 0, FALSE,
                                    raddr, 0, FALSE, flags);
}

/**
 * Zero-copy datagrams reception on ZF RX zocket.
 *
 * @note It is required to keep the message structure on the agent once
 * successful reception is done. Then it is freed with call
 * @a rpc_zfur_zc_recv.
 *
 * @param rpcs      RPC server handle.
 * @param urx       Pointer to UDP RX zocket.
 * @param msg       Zetaferno RX message.
 * @param flags     Control flags.
 */
extern void rpc_zfur_zc_recv(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                             rpc_zfur_msg *msg, int flags);

/**
 * Finalize zero-copy datagrams reception on ZF RX zocket and release
 * resources. Must be called after each successfull (@b iovcnt > @c 0)
 * @a zfur_zc_recv operation.
 *
 * @param rpcs      RPC server handle.
 * @param urx       Pointer to UDP RX zocket.
 * @param msg       Zetaferno RX message.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfur_zc_recv_done(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                                 rpc_zfur_msg *msg);

/**
 * Extract zetaferno UDP RX message from RPC pointer.
 *
 * @param rpcs      RPC server handle.
 * @param msg_ptr   Pointer to zetaferno UDP RX message.
 * @param msg       Extracted message.
 */
extern void rpc_zfur_read_zfur_msg(rcf_rpc_server *rpcs,
                                   rpc_zfur_msg_p msg_ptr,
                                   rpc_zfur_msg *msg);

/**
 * Get a datagram headers.
 *
 * @param[in] rpcs      RPC server handle.
 * @param[in] urx       Pointer to UDP RX zocket.
 * @param[in] msg       Zetaferno RX message.
 * @param[out] iph      Location for IPv4 header or @c NULL.
 * @param[in] iph_len   @p iph buffer size.
 * @param[out] udph     Location for UDP header or @c NULL.
 * @param[in] pktind    Datagram index.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfur_pkt_get_header(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                                   rpc_zfur_msg *msg, struct iphdr *iph,
                                   size_t iph_len, struct udphdr *udph,
                                   int pktind);

/**
 * Receive datagrams using @a zfur_zc_recv() by RX zocket and send them by
 * TX zocket.
 *
 * @param rpcs      RPC server handle.
 * @param urx       Pointer to UDP RX zocket.
 * @param msg       Zetaferno RX message.
 * @param utx       Pointer to UDP TX zocket.
 * @param send_func Transmitting function.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfur_zc_recv_send(rcf_rpc_server *rpcs,rpc_zfur_p urx,
                                 rpc_zfur_msg *msg, rpc_zfut_p utx,
                                 zfts_send_function send_func);

/**
 * Receive datagrams during a period of time.
 *
 * @param rpcs      RPC server handle.
 * @param stack     Pointer to the stack object.
 * @param urx       Pointer to UDP RX zocket.
 * @param duration  How long receive datagrams, milliseconds.
 * @param stats     Received data amount, bytes.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfur_flooder(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                            rpc_zfur_p urx, int duration, uint64_t *stats);

/**
 * Get pointer to @b zf_waitatable structure of ZF UDP RX zocket.
 *
 * @note Zetaferno API does not propose any cleanup operations for the
 * returned pointer. The function just returns pointer to a structure, which
 * is one of the zocket fields. But TE RPC pointer is allocated to keep the
 * agent pointer. So function @a rpc_zf_waitable_free shall be used to
 * release the RPC pointer.
 *
 * @param rpcs      RPC server handle.
 * @param urx       Pointer to UDP RX zocket.
 *
 * @return Pointer to the waitable zocket.
 */
extern rpc_zf_waitable_p rpc_zfur_to_waitable(rcf_rpc_server *rpcs,
                                              rpc_zfur_p urx);

/**
 * Call zfur_pkt_get_timestamp() to obtain RX timestamp for an UDP packet
 * received with zfur_zc_recv().
 *
 * @param rpcs      RPC server handle.
 * @param urx       Pointer to UDP RX zocket.
 * @param msg_ptr   RPC pointer to zfur_msg structure on TA which was
 *                  passed to zfur_zc_recv() before.
 * @param ts        Where to save retrieved RX timestamp.
 * @param pktind    Index of packet within msg_ptr->iov.
 * @param flags     Where to save sync flags for the packet
 *                  (see @ref tarpc_zf_sync_flags).
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
extern int rpc_zfur_pkt_get_timestamp(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                                      rpc_zfur_msg_p msg_ptr,
                                      tarpc_timespec *ts,
                                      int pktind, unsigned int *flags);

#endif /* !___RPC_ZF_UDP_RX_H__ */
