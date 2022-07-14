/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions definition
 *
 * Definition of TAPI for Zetaferno Direct API remote calls
 * related to UDP TX zockets.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru
 *
 * $Id$
 */

#ifndef ___RPC_ZF_UDP_TX_H__
#define ___RPC_ZF_UDP_TX_H__

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
#include "zf_talib_common.h"

/**
 * Create Zetaferno UDP TX zocket.
 *
 * @param rpcs          RPC server handle.
 * @param us_out        Pointer to UDP TX zocket.
 * @param stack         Pointer to the stack object.
 * @param laddr         Local address.
 * @param laddrlen      Local address length.
 * @param fwd_laddrlen  Whether to use laddrlen value or not.
 * @param raddr         Remote address.
 * @param raddrlen      Remote address length.
 * @param fwd_raddrlen  Whether to use raddrlen value or not.
 * @param flags         Control flags.
 * @param attr          Pointer to the attribute object.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfut_alloc_gen(rcf_rpc_server *rpcs,
                              rpc_zfut_p *us_out,
                              rpc_zf_stack_p stack,
                              const struct sockaddr *laddr,
                              socklen_t laddrlen, te_bool fwd_laddrlen,
                              const struct sockaddr *raddr,
                              socklen_t raddrlen, te_bool fwd_raddrlen,
                              int flags,
                              rpc_zf_attr_p attr);

/**
 * Create Zetaferno UDP TX zocket.
 *
 * @param rpcs      RPC server handle.
 * @param us_out    Pointer to UDP TX zocket.
 * @param stack     Pointer to the stack object.
 * @param laddr     Local address.
 * @param raddr     Remote address.
 * @param flags     Control flags.
 * @param attr      Pointer to the attribute object.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
static inline int
rpc_zfut_alloc(rcf_rpc_server *rpcs, rpc_zfut_p *us_out,
               rpc_zf_stack_p stack,
               const struct sockaddr *laddr,
               const struct sockaddr *raddr, int flags,
               rpc_zf_attr_p attr)
{
    return rpc_zfut_alloc_gen(rpcs, us_out, stack, laddr, 0, FALSE,
                              raddr, 0, FALSE, flags, attr);
}

/**
 * Free Zetaferno UDP TX zocket.
 *
 * @param rpcs      RPC server handle.
 * @param utx       Pointer to UDP TX zocket.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfut_free(rcf_rpc_server *rpcs, rpc_zfut_p utx);


/**
 * Send datagrams using Zetaferno UDP TX zocket.
 *
 * @param rpcs      RPC server handle.
 * @param utx       Pointer to UDP TX zocket.
 * @param iov       Data vectors array.
 * @param iovcnt    The array length.
 * @param flags     Send flags.
 *
 * @return Sent data amount or a negative value in case of fail.
 */
extern int rpc_zfut_send(rcf_rpc_server *rpcs, rpc_zfut_p utx,
                         rpc_iovec *iov, size_t iovcnt,
                         rpc_zfut_flags flags);

/**
 * Get the maximum segment size which can be transmitted.
 *
 * @param rpcs      RPC server handle.
 * @param utx       Pointer to UDP TX zocket.
 *
 * @return The maximum segment size or a negative value in case of fail.
 */
extern int rpc_zfut_get_mss(rcf_rpc_server *rpcs, rpc_zfut_p utx);

/**
 * Copy-based send of single non-fragmented packet.
 *
 * @param rpcs      RPC server handle.
 * @param utx       Pointer to UDP TX zocket.
 * @param buf       Data buffer.
 * @param buflen    The buffer length.
 *
 * @return Sent data amount or a negative value in case of fail.
 */
extern int rpc_zfut_send_single(rcf_rpc_server *rpcs, rpc_zfut_p utx,
                                const void *buf, size_t buflen);

/**
 * Send datagrams during a period of time.
 *
 * @param rpcs          RPC server handle.
 * @param stack         Pointer to the stack object.
 * @param utx           Pointer to UDP TX zocket.
 * @param send_func     Transmitting function.
 * @param dgram_size    Datagrams size, bytes.
 * @param iovcnt        Iov vectors number.
 * @param duration      How long transmit datagrams, milliseconds.
 * @param stats         Sent data amount, bytes.
 * @param errors        @c EAGAIN errors counter.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zfut_flooder(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                            rpc_zfut_p utx, zfts_send_function send_func,
                            int dgram_size, int iovcnt, int duration,
                            uint64_t *stats, uint64_t *errors);

/**
 * Get pointer to @b zf_waitatable structure of ZF UDP TX zocket.
 *
 * @note Zetaferno API does not propose any cleanup operations for the
 * returned pointer. The function just returns pointer to a structure, which
 * is one of the zocket fields. But TE RPC pointer is allocated to keep the
 * agent pointer. So function @a rpc_zf_waitable_free shall be used to
 * release the RPC pointer.
 *
 * @param rpcs      RPC server handle.
 * @param utx       Pointer to UDP TX zocket.
 *
 * @return Pointer to the waitable zocket.
 */
extern rpc_zf_waitable_p rpc_zfut_to_waitable(rcf_rpc_server *rpcs,
                                              rpc_zfut_p utx);

/**
 * Return protocol header size for UDP TX zocket.
 *
 * @param rpcs      RPC server handle.
 * @param utx       Pointer to ZF UDP TX zocket.
 *
 * @return Protocol header size (total size of all protocol headers
 *         in bytes).
 */
extern unsigned int rpc_zfut_get_header_size(rcf_rpc_server *rpcs,
                                             rpc_zfut_p utx);

/**
 * Obtain TX timestamps for sent UDP packets.
 *
 * @param rpcs            RPC server handle.
 * @param utx             UDP TX zocket.
 * @param reports         Where to save "packet reports" with timestamps.
 * @param count           Total number of entries in @p reports on input,
 *                        number of retrieved entries on output.
 *
 * @return @c 0 on success, @c -1 on failure.
 */
extern int rpc_zfut_get_tx_timestamps(rcf_rpc_server *rpcs, rpc_zfut_p utx,
                                      tarpc_zf_pkt_report *reports,
                                      int *count);

#endif /* !___RPC_ZF_UDP_TX_H__ */
