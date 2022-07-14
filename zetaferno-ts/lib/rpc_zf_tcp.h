/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions definition
 *
 * Definition of TAPI for Zetaferno Direct API remote calls
 * related to TCP zockets.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru
 *
 * $Id$
 */

#ifndef ___RPC_ZF_TCP_H__
#define ___RPC_ZF_TCP_H__

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
 * Reserved array length in @b zft_msg structure. The value should
 * correspond the value declared in @b  struct @b zft_msg in ZF sources
 * header @c zf_tcp.h.  */
#define ZFT_MSG_RESERVED 4

/**
 * Structure to receive datagrams using Zetaferno API.
 */
typedef struct rpc_zft_msg {
  int32_t         reserved[ZFT_MSG_RESERVED]; /**< Reserved. */
  int             pkts_left;  /**< Packets in the queue. */
  int             flags;      /**< Reception flags. */
  int             iovcnt;     /**< In/out vectors number. */
  rpc_iovec      *iov;        /**< Vectors for datagrams reception. */
  rpc_zft_msg_p   ptr;        /**< Pointer to the structure on the agent
                                   side. */
} rpc_zft_msg;

/**
 * Allocate Zetaferno listening TCP zocket.
 *
 * @param rpcs          RPC server handle.
 * @param stack         RPC pointer identifier of the stack object.
 * @param laddr         Local address on which to listen.
 * @param laddrlen      Address length.
 * @param fwd_laddrlen  Whether to use laddrlen value or not.
 * @param attr          RPC pointer identifier of the attribute object.
 * @param tl_out        RPC pointer identitifer of the Zetaferno
 *                      listening zocket
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
extern int rpc_zftl_listen_gen(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                               const struct sockaddr *laddr,
                               socklen_t laddrlen, te_bool fwd_laddrlen,
                               rpc_zf_attr_p attr, rpc_zftl_p *tl_out);

/**
 * Allocate Zetaferno listening TCP zocket.
 *
 * @param rpcs        RPC server handle.
 * @param stack       RPC pointer identifier of the stack object.
 * @param laddr       Local address on which to listen.
 * @param attr        RPC pointer identifier of the attribute object.
 * @param tl_out      RPC pointer identitifer of the Zetaferno
 *                    listening zocket
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
static inline int
rpc_zftl_listen(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                const struct sockaddr *laddr,
                rpc_zf_attr_p attr, rpc_zftl_p *tl_out)
{
    return rpc_zftl_listen_gen(rpcs, stack, laddr, 0, FALSE, attr, tl_out);
}

/**
 * Retrieve local address of ZF TCP listening zocket.
 *
 * @param rpcs      RPC server handle.
 * @param tl        RPC pointer to ZF TCP listening zocket.
 * @param laddr     Where to save local address.
 */
extern void rpc_zftl_getname(rcf_rpc_server *rpcs, rpc_zftl_p ts,
                             struct sockaddr_in *laddr);

/**
 * Accept incoming TCP connect on Zetaferno listening zocket.
 *
 * @param rpcs        RPC server handle.
 * @param tl          RPC pointer identifier of Zetaferno listening
 *                    zocket
 * @param ts_out      RPC pointer identifier of Zetaferno TCP
 *                    zocket
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
extern int rpc_zftl_accept(rcf_rpc_server *rpcs, rpc_zftl_p tl,
                           rpc_zft_p *ts_out);

/**
 * Get pointer to @b zf_waitatable structure of ZF TCP listening zocket.
 *
 * @note Zetaferno API does not propose any cleanup operations for the
 * returned pointer. The function just returns pointer to a structure, which
 * is one of the zocket fields. But TE RPC pointer is allocated to keep the
 * agent pointer. So function @a rpc_zf_waitable_free shall be used to
 * release the RPC pointer.
 *
 * @param rpcs      RPC server handle.
 * @param tl        RPC pointer identifier of ZF TCP listening zocket.
 *
 * @return RPC pointer identifier of the waitable zocket.
 */
extern rpc_zf_waitable_p rpc_zftl_to_waitable(rcf_rpc_server *rpcs,
                                              rpc_zftl_p tl);

/**
 * Release resources associated with a Zetaferno TCP listening zocket.
 *
 * @param rpcs        RPC server handle.
 * @param tl          RPC pointer identifier of Zetaferno listening
 *                    zocket
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
extern int rpc_zftl_free(rcf_rpc_server *rpcs, rpc_zftl_p tl);

/**
 * Get pointer to @b zf_waitatable structure of ZF TCP zocket.
 *
 * @note Zetaferno API does not propose any cleanup operations for the
 * returned pointer. The function just returns pointer to a structure, which
 * is one of the zocket fields. But TE RPC pointer is allocated to keep the
 * agent pointer. So function @a rpc_zf_waitable_free shall be used to
 * release the RPC pointer.
 *
 * @param rpcs      RPC server handle.
 * @param ts        RPC pointer identifier of ZF TCP zocket.
 *
 * @return RPC pointer identifier of the waitable zocket.
 */
extern rpc_zf_waitable_p rpc_zft_to_waitable(rcf_rpc_server *rpcs,
                                             rpc_zft_p ts);

/**
 * Allocate Zetaferno TCP zocket handle.
 *
 * @param rpcs        RPC server handle.
 * @param stack       RPC pointer identifier of the stack object.
 * @param attr        RPC pointer identifier of the attribute object.
 * @param handle_out  RPC pointer identitifer of the Zetaferno
 *                    TCP zocket handle.
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
extern int rpc_zft_alloc(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                         rpc_zf_attr_p attr, rpc_zft_handle_p *handle_out);

/**
 * Bind Zetaferno TCP zocket handle to network address.
 *
 * @param rpcs          RPC server handle.
 * @param handle        RPC pointer identifier of ZF TCP zocket handle.
 * @param laddr         Local address.
 * @param laddrlen      Address length.
 * @param fwd_laddrlen  Whether to use laddrlen value or not.
 * @param flags         Control flags.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zft_addr_bind_gen(rcf_rpc_server *rpcs,
                                 rpc_zft_handle_p handle,
                                 const struct sockaddr *laddr,
                                 socklen_t laddrlen, te_bool fwd_laddrlen,
                                 int flags);

/**
 * Bind Zetaferno TCP zocket handle to network address.
 *
 * @param rpcs      RPC server handle.
 * @param handle    RPC pointer identifier of ZF TCP zocket handle.
 * @param laddr     Local address.
 * @param flags     Control flags.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
static inline int
rpc_zft_addr_bind(rcf_rpc_server *rpcs, rpc_zft_handle_p handle,
                  const struct sockaddr *laddr, int flags)
{
    return rpc_zft_addr_bind_gen(rpcs, handle, laddr, 0, FALSE, flags);
}

/**
 * Connect a Zetaferno TCP zocket.
 *
 * @param rpcs          RPC server handle.
 * @param handle        RPC pointer identifier of ZF TCP zocket handle.
 * @param raddr         Remote address.
 * @param raddrlen      Address length.
 * @param fwd_raddrlen  Whether to use raddrlen value or not.
 * @param ts_out        RPC pointer identitifer of connected ZF TCP zocket.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zft_connect_gen(rcf_rpc_server *rpcs,
                               rpc_zft_handle_p handle,
                               const struct sockaddr *raddr,
                               socklen_t raddrlen, te_bool fwd_raddrlen,
                               rpc_zft_p *ts_out);

/**
 * Connect a Zetaferno TCP zocket.
 *
 * @param rpcs      RPC server handle.
 * @param handle    RPC pointer identifier of ZF TCP zocket handle.
 * @param raddr     Remote address.
 * @param ts_out    RPC pointer identitifer of connected ZF TCP zocket.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
static inline int
rpc_zft_connect(rcf_rpc_server *rpcs, rpc_zft_handle_p handle,
                const struct sockaddr *raddr, rpc_zft_p *ts_out)
{
    return rpc_zft_connect_gen(rpcs, handle, raddr, 0, FALSE, ts_out);
}

/**
 * Shutdown outgoing TCP connection on Zetaferno TCP zocket.
 *
 * @param rpcs      RPC server handle.
 * @param ts        RPC pointer identitifer of connected ZF TCP zocket.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zft_shutdown_tx(rcf_rpc_server *rpcs, rpc_zft_p ts);


/**
 * Shutdown outgoing TCP connection on Zetaferno TCP zocket.
 * The same as rpc_zft_shutdown_tx(), but does not go to 'cleanup' label.
 *
 * @param rpcs_     RPC server handle.
 * @param ts_       RPC pointer identitifer of connected ZF TCP zocket.
 */
#define CLEANUP_RPC_ZFT_SHUTDOWN_TX(rpcs_, ts_) \
    do {                                                        \
        if (ts_ != RPC_NULL)                                    \
        {                                                       \
            int rc_;                                            \
            RPC_AWAIT_IUT_ERROR(rpcs_);                         \
            if ((rc_ = rpc_zft_shutdown_tx(rpcs_, ts_)) != 0)   \
                MACRO_TEST_ERROR;                               \
        }                                                       \
    } while (0)

/**
 * Release resources associated with a Zetaferno TCP zocket handle.
 *
 * @param rpcs      RPC server handle.
 * @param handle    RPC pointer identitifer of ZF TCP zocket handle.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zft_handle_free(rcf_rpc_server *rpcs,
                               rpc_zft_handle_p handle);

/**
 * Release resources associated with a Zetaferno TCP zocket.
 *
 * @param rpcs      RPC server handle.
 * @param ts        RPC pointer identitifer of ZF TCP zocket.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
extern int rpc_zft_free(rcf_rpc_server *rpcs, rpc_zft_p ts);

/**
 * Get TCP state of Zetaferno TCP zocket.
 *
 * @param rpcs      RPC server handle.
 * @param ts        RPC pointer identitifer of ZF TCP zocket.
 *
 * @return TCP state on success or a negative value in case of failure.
 */
extern int rpc_zft_state(rcf_rpc_server *rpcs, rpc_zft_p ts);

/**
 * Find out error happened on Zetaferno TCP zocket.
 *
 * @param rpcs      RPC server handle.
 * @param ts        RPC pointer identitifer of ZF TCP zocket.
 *
 * @return Errno value, similar to @c SO_ERROR value for sockets.
 */
extern rpc_errno rpc_zft_error(rcf_rpc_server *rpcs, rpc_zft_p ts);

/**
 * Retrieve local address of ZF TCP zocket handle.
 *
 * @param rpcs      RPC server handle.
 * @param handle    RPC pointer to ZF TCP zocket handle.
 * @param laddr     Where to save local address.
 */
extern void rpc_zft_handle_getname(rcf_rpc_server *rpcs,
                                   rpc_zft_handle_p handle,
                                   struct sockaddr_in *laddr);

/**
 * Retrieve local and/or remote addresses of ZF TCP zocket.
 *
 * @param rpcs      RPC server handle.
 * @param ts        Pointer to ZF TCP zocket.
 * @param laddr     Where to save local address.
 * @param raddr     Where to save remote address.
 */
extern void rpc_zft_getname(rcf_rpc_server *rpcs, rpc_zft_p ts,
                            struct sockaddr_in *laddr,
                            struct sockaddr_in *raddr);

/**
 * Zero-copy packets reception on ZF TCP zocket.
 *
 * @note It is required to keep the message structure on the agent once
 * successful reception is done. Then it is freed with call
 * @a rpc_zft_zc_recv_done.
 *
 * @param rpcs      RPC server handle.
 * @param ts        Pointer to ZF TCP zocket.
 * @param msg       Zetaferno RX message.
 * @param flags     Control flags.
 */
extern void rpc_zft_zc_recv(rcf_rpc_server *rpcs, rpc_zft_p ts,
                            rpc_zft_msg *msg, int flags);

/**
 * Finalize zero-copy packets reception on ZF TCP zocket and release
 * resources. Must be called after each successfull (@b iovcnt > @c 0)
 * @a zft_zc_recv operation.
 *
 * @param rpcs      RPC server handle.
 * @param ts        Pointer to ZF TCP zocket.
 * @param msg       Zetaferno RX message.
 *
 * @return >= @c 0 on success or a negative value in case of fail.
 */
extern int rpc_zft_zc_recv_done(rcf_rpc_server *rpcs, rpc_zft_p ts,
                                rpc_zft_msg *msg);

/**
 * Finalize zero-copy packets reception on ZF TCP zocket and release
 * resources, acknowledging that all or some of the data was processed.
 * Must be called after each successfull (@b iovcnt > @c 0)
 * @b zft_zc_recv operation. If @p len is less than total data length
 * previously returned by @b zft_zc_recv(), remaining data will
 * be returned again by the next @b zft_zc_recv() call.
 *
 * @param rpcs      RPC server handle.
 * @param ts        Pointer to ZF TCP zocket.
 * @param msg       Zetaferno RX message.
 * @param len       Total number of bytes read by the client.
 *
 * @return >= @c 0 on success or a negative value in case of fail.
 */
extern int rpc_zft_zc_recv_done_some(rcf_rpc_server *rpcs, rpc_zft_p ts,
                                     rpc_zft_msg *msg, size_t len);

/**
 * Copy-based receive on Zetaferno TCP zocket.
 *
 * @param rpcs      RPC server handle.
 * @param ts        Pointer to ZF TCP zocket.
 * @param iov       Data vectors array.
 * @param iovcnt    The array length.
 * @param flags     Send flags.
 *
 * @return Number of bytes received on success or
 *         a negative value in case of fail.
 */
extern int rpc_zft_recv(rcf_rpc_server *rpcs, rpc_zft_p ts,
                        rpc_iovec *iov, int iovcnt, int flags);

/**
 * Extract zetaferno TCP RX message from RPC pointer.
 *
 * @param rpcs      RPC server handle.
 * @param msg_ptr   Pointer to zetaferno TCP RX message.
 * @param msg       Extracted message.
 */
extern void rpc_zft_read_zft_msg(rcf_rpc_server *rpcs,
                                 rpc_zft_msg_p msg_ptr,
                                 rpc_zft_msg *msg);

/**
 * Send packets using Zetaferno TCP zocket.
 *
 * @param rpcs      RPC server handle.
 * @param ts        Pointer to ZF TCP zocket.
 * @param iov       Data vectors array.
 * @param iovcnt    The array length.
 * @param flags     Send flags.
 *
 * @return Number of bytes sent on success or
 *         a negative value in case of fail.
 */
extern ssize_t rpc_zft_send(rcf_rpc_server *rpcs, rpc_zft_p ts,
                            rpc_iovec *iov, size_t iovcnt, int flags);

/**
 * Copy-based send of single non-fragmented packet.
 *
 * @param rpcs      RPC server handle.
 * @param ts        RPC pointer to TCP zocket.
 * @param buf       Data buffer.
 * @param buflen    The buffer length.
 * @param flags     Flags.
 *
 * @return Sent data amount or a negative value in case of fail.
 */
extern ssize_t rpc_zft_send_single(rcf_rpc_server *rpcs, rpc_zft_p ts,
                                   const void *buf, size_t buflen,
                                   int flags);

/**
 * Query available space in the send queue.
 *
 * @param rpcs      RPC server handle.
 * @param ts        Pointer to ZF TCP zocket.
 * @param size      Where to save number of bytes available.
 *
 * @return @c 0 on success or a negative value in case of fail.
 */
extern int rpc_zft_send_space(rcf_rpc_server *rpcs, rpc_zft_p ts,
                              size_t *size);

/**
 * Retrieve maximum segment size for connected TCP zocket.
 *
 * @param rpcs      RPC server handle.
 * @param ts        RPC pointer to ZF TCP zocket.
 *
 * @return Negative value in case of failure or MSS size in case of
 *         success.
 */
extern int rpc_zft_get_mss(rcf_rpc_server *rpcs,
                           rpc_zft_p ts);

/**
 * Return protocol header size for connected TCP zocket.
 *
 * @param rpcs      RPC server handle.
 * @param ts        Pointer to ZF TCP zocket.
 *
 * @return Protocol header size (total size of all protocol headers
 *         in bytes).
 */
extern unsigned int rpc_zft_get_header_size(rcf_rpc_server *rpcs,
                                            rpc_zft_p ts);

/**
 * Call zft_pkt_get_timestamp() to obtain RX timestamp for a TCP packet
 * received with zft_zc_recv().
 *
 * @param rpcs      RPC server handle.
 * @param tcp_z     Pointer to TCP zocket.
 * @param msg_ptr   RPC pointer to zft_msg structure on TA which was
 *                  passed to zft_zc_recv() before.
 * @param ts        Where to save retrieved RX timestamp.
 * @param pktind    Index of packet within msg_ptr->iov.
 * @param flags     Where to save sync flags for the packet
 *                  (see @ref tarpc_zf_sync_flags).
 *
 * @return @c Zero on success or a negative value in case of failure.
 */
extern int rpc_zft_pkt_get_timestamp(rcf_rpc_server *rpcs, rpc_zft_p tcp_z,
                                     rpc_zft_msg_p msg_ptr,
                                     tarpc_timespec *ts,
                                     int pktind, unsigned int *flags);

/**
 * Obtain TX timestamps for sent TCP packets.
 *
 * @param rpcs            RPC server handle.
 * @param tz              TCP zocket.
 * @param reports         Where to save "packet reports" with timestamps.
 * @param count           Total number of entries in @p reports on input,
 *                        number of retrieved entries on output.
 *
 * @return @c 0 on success, @c -1 on failure.
 */
extern int rpc_zft_get_tx_timestamps(rcf_rpc_server *rpcs, rpc_zft_p tz,
                                     tarpc_zf_pkt_report *reports,
                                     int *count);

/**
 * Read all data using @ref zft_recv on TCP zocket and append
 * it to @p dbuf.
 * Data is being read until @c EAGAIN error happens.
 *
 * @param rpcs      RPC server handle.
 * @param ts        Pointer to ZF TCP zocket.
 * @param dbuf      Buffer to append read data to.
 * @param read      Amount of read data (or @c NULL).
 *
 * @return @c -1 in the case of failure or @c 0 on success (data reading
 * was interrupted by @c EAGAIN error).
 */
extern int rpc_zft_read_all(rcf_rpc_server *rpcs, rpc_zft_p ts, te_dbuf *dbuf,
                            size_t *read);

/**
 * Read all data using @ref zft_zc_recv on TCP zocket and append
 * it to @p dbuf.
 * Data is being read until @c EOF is got.
 *
 * @param rpcs      RPC server handle.
 * @param ts        Pointer to ZF TCP zocket.
 * @param dbuf      Buffer to append read data to.
 * @param read      Amount of read data (or @c NULL).
 *
 * @return @c -1 in the case of failure or @c 0 on success (data reading
 * was interrupted by @c EOF return code).
 */
extern int rpc_zft_read_all_zc(rcf_rpc_server *rpcs, rpc_zft_p ts,
                               te_dbuf *dbuf, size_t *read);


/**
 * Overfill the buffers on receive and send sides of TCP connection.
 *
 * @param rpcs  RPC server.
 * @param stack ZF stack object.
 * @param ts    ZF TCP zocket.
 * @param dbuf  Output dynamic buffer sent data will be appended to.
 *
 * @return @c -1 in the case of failure or @c 0 on success (@ref zft_send
 * returned -EAGAIN).
 */
extern int rpc_zft_overfill_buffers(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                                    rpc_zft_p ts, te_dbuf *dbuf);

#endif /* !___RPC_ZF_TCP_H__ */
