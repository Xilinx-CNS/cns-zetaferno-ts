/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Auxiliary test API for TCP zockets
 *
 * Definition of auxiliary test API to work with TCP zockets.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef ___ZFTS_TCP_H__
#define ___ZFTS_TCP_H__

#include "te_dbuf.h"
#include "te_errno.h"

/** Default listen backlog value */
#define ZFTS_LISTEN_BACKLOG_DEF 5

/** Maximum number of ZF TCP endpoints. */
#define ZFTS_MAX_TCP_ENDPOINTS 64

/** Maximum number of ZF TCP listen endpoints. */
#define ZFTS_MAX_TCP_LISTEN_ENDPOINTS 16

/**
 * Maximum TCP test data length (to be used as
 * a typical data length in tests).
 */
#define ZFTS_TCP_DATA_MAX 1400

/** Maximum number of bytes TCP packet headers may require. */
#define ZFTS_TCP_HDRS_MAX 300

/**
 * Currently it is not supported to send from more
 * than one iov.
 */
#define ZFTS_TCP_SEND_IOVCNT 1

/**
 * Shutdown functions.
 */
typedef enum {
    ZFTS_TCP_SHUTDOWN_SHUTDOWN = 0, /**< Use rpc_zft_shutdown_tx() */
    ZFTS_TCP_SHUTDOWN_FREE,         /**< Use rpc_zftl_free() or
                                         rpc_zft_free() */
    ZFTS_TCP_SHUTDOWN_NONE,         /**< Do not shutdown */
} zfts_tcp_shutdown_func;

/**
 * Shutdown functions list, can be passed to macro @b TEST_GET_ENUM_PARAM.
 */
#define ZFTS_TCP_SHUTDOWN_FUNCS  \
    { "shutdown",    ZFTS_TCP_SHUTDOWN_SHUTDOWN }, \
    { "free",        ZFTS_TCP_SHUTDOWN_FREE }, \
    { "none",        ZFTS_TCP_SHUTDOWN_NONE }

/**
 * Release resources occupied by zft_handle object and
 * its RPC pointer if its RPC pointer is not NULL.
 *
 * @param rpcs_       RPC server.
 * @param handle_     zft_handle object RPC pointer.
 */
#define ZFT_HANDLE_FREE(rpcs_, handle_) \
    do {                                            \
        if (handle_ != RPC_NULL)                    \
            rpc_zft_handle_free(rpcs_, handle_);    \
        handle_ = RPC_NULL;                         \
    } while (0)

/**
 * Release resources occupied by zft_handle object and
 * its RPC pointer if its RPC pointer is not NULL.
 * The same as rpc_zft_handle_free(), but does not go to 'cleanup' label.
 *
 * @param rpcs_       RPC server.
 * @param handle_     zft_handle object RPC pointer.
 */
#define CLEANUP_RPC_ZFT_HANDLE_FREE(rpcs_, handle_) \
    do {                                                            \
        if (handle_ != RPC_NULL)                                    \
        {                                                           \
            int rc_;                                                \
                                                                    \
            RPC_AWAIT_IUT_ERROR(rpcs_);                             \
            if ((rc_ = rpc_zft_handle_free(rpcs_, handle_)) != 0)   \
                MACRO_TEST_ERROR;                                   \
            else                                                    \
                handle_ = RPC_NULL;                                 \
        }                                                           \
    } while (0)

/** Possible types of TCP connection establishment problem. */
typedef enum {
    ZFTS_CONN_REFUSED = 0,    /**< RST is received in response. */
    ZFTS_CONN_TIMEOUT,        /**< Failed due to timeout. */
    ZFTS_CONN_DELAYED,        /**< Connection is established successfully
                                   after some delay. */
} zfts_conn_problem;

/**
 * List of TCP connection establishment problems,
 * can be passed to macro @b TEST_GET_ENUM_PARAM.
 */
#define ZFTS_CONN_PROBLEMS \
    { "refused", ZFTS_CONN_REFUSED }, \
    { "timeout", ZFTS_CONN_TIMEOUT }, \
    { "delayed", ZFTS_CONN_DELAYED }

/** TCP connection establishment methods. */
typedef enum {
    ZFTS_CONN_OPEN_ACT = 0,     /**< Active connection opening. */
    ZFTS_CONN_OPEN_PAS,         /**< Passive connection opening. */
    ZFTS_CONN_OPEN_PAS_CLOSE,   /**< Passive connection opening,
                                     after which listener is closed. */
} zfts_conn_open_method;

/**
 * List of TCP connection establishment methods,
 * can be passed to macro @b TEST_GET_ENUM_PARAM.
 */
#define ZFTS_CONN_OPEN_METHODS \
    { "active",           ZFTS_CONN_OPEN_ACT }, \
    { "passive",          ZFTS_CONN_OPEN_PAS }, \
    { "passive_close",    ZFTS_CONN_OPEN_PAS_CLOSE }

/** Functions which can be used to receive data on TCP zocket. */
typedef enum {
    ZFTS_TCP_RECV_ZFT_ZC_RECV = 0,  /**< @b zft_zc_recv() */
    ZFTS_TCP_RECV_ZFT_RECV,         /**< @b zft_recv() */
} zfts_tcp_recv_func_t;

/**
 * List of functions which can be used to receive data on
 * TCP zocket, to be passed to macro @b TEST_GET_ENUM_PARAM.
 */
#define ZFTS_TCP_RECV_FUNCS \
    { "zft_zc_recv",  ZFTS_TCP_RECV_ZFT_ZC_RECV }, \
    { "zft_recv",     ZFTS_TCP_RECV_ZFT_RECV }

/** Functions which can be used to send data from TCP zocket. */
typedef enum {
    ZFTS_TCP_SEND_ZFT_SEND = 0,     /**< @b zft_send() */
    ZFTS_TCP_SEND_ZFT_SEND_SINGLE,  /**< @b zft_send_single() */
} zfts_tcp_send_func_t;

/**
 * List of functions which can be used to send data from
 * TCP zocket, to be passed to macro @b TEST_GET_ENUM_PARAM.
 */
#define ZFTS_TCP_SEND_FUNCS \
    { "zft_send",             ZFTS_TCP_SEND_ZFT_SEND }, \
    { "zft_send_single",      ZFTS_TCP_SEND_ZFT_SEND_SINGLE }

/**
 * Get value of parameter defining ZF TCP receive
 * function; set default ZF TCP receive function to it.
 *
 * @param param_    Parameter name.
 */
#define TEST_GET_SET_DEF_RECV_FUNC(param_)                  \
    do {                                                    \
        zfts_tcp_recv_func_t param_;                        \
                                                            \
        TEST_GET_ENUM_PARAM(param_, ZFTS_TCP_RECV_FUNCS);   \
        zfts_set_def_tcp_recv_func(param_);                 \
    } while (0)

/**
 * Structure describing TCP connection.
 */
typedef struct {
    rpc_zft_p   iut_zft;    /**< IUT zocket. */
    int         tst_s;      /**< TESTER socket. */

    te_dbuf   iut_sent;     /**< Data sent from IUT in the current bunch. */
    te_dbuf   tst_sent;     /**< Data sent from TESTER
                                 in the current bunch. */

    te_bool   recv_overfilled;   /**< Whether receive zocket buffers
                                      are overfilled already or not. */
    te_bool   send_overfilled;   /**< Whether send zocket buffers
                                      are overfilled already or not. */
    int       group_id;          /**< Connection group id (for example,
                                      connections my be grouped by listener
                                      zockets via which they were
                                      accepted). */
    int       fails;             /**< Send fails counter. */
} zfts_tcp_conn;

/**
 * Establish TCP connection between ZF zocket and socket over
 * existing ZF listener TCP zocket.
 *
 * @param pco_iut     IUT RPC server.
 * @param stack       ZF stack object RPC pointer.
 * @param iut_zftl    IUT listener TCP zocket.
 * @param iut_zft     IUT TCP zocket.
 * @param iut_addr    IUT network address.
 * @param pco_tst     Tester RPC server.
 * @param tst_s       Tester socket.
 * @param tst_addr    Tester network address.
 *
 * @note tst_s is created only if it is less than zero initially.
 */
extern void zfts_connect_to_zftl(
                              rcf_rpc_server *pco_iut,
                              rpc_zf_stack_p stack,
                              rpc_zftl_p iut_zftl,
                              rpc_zft_p *iut_zft,
                              const struct sockaddr *iut_addr,
                              rcf_rpc_server *pco_tst,
                              int *tst_s,
                              const struct sockaddr *tst_addr);

/**
 * Establish TCP connection between ZF zocket and socket.
 *
 * @param active      Whether connection should be established
 *                    actively or passively in relation to the zocket.
 * @param pco_iut     IUT RPC server.
 * @param attr        ZF attribute object RPC pointer.
 * @param stack       ZF stack object RPC pointer.
 * @param iut_zft     IUT zocket.
 * @param iut_addr    IUT network address.
 * @param pco_tst     Tester RPC server.
 * @param tst_s       Tester socket.
 * @param tst_addr    Tester network address.
 */
extern void zfts_establish_tcp_conn(
                              te_bool active,
                              rcf_rpc_server *pco_iut,
                              rpc_zf_attr_p attr,
                              rpc_zf_stack_p stack,
                              rpc_zft_p *iut_zft,
                              const struct sockaddr *iut_addr,
                              rcf_rpc_server *pco_tst,
                              int *tst_s,
                              const struct sockaddr *tst_addr);

/**
 * Establish TCP connection between ZF zocket and socket,
 * with setting @c SO_SNDBUF and/or @c SO_RCVBUF for Tester socket
 * before connection establishment.
 *
 * @param active      Whether connection should be established
 *                    actively or passively in relation to the zocket.
 * @param pco_iut     IUT RPC server.
 * @param attr        ZF attribute object RPC pointer.
 * @param stack       ZF stack object RPC pointer.
 * @param iut_zftl    IUT TCP listening zocket (will be used if not
 *                    @c RPC_NULL).
 * @param iut_zft     IUT zocket.
 * @param iut_addr    IUT network address.
 * @param pco_tst     Tester RPC server.
 * @param tst_s       Tester socket.
 * @param tst_addr    Tester network address.
 * @param tst_sndbuf  @c SO_SNDBUF value for Tester socket,
 *                    or negative value if its setting is not required.
 * @param tst_rcvbuf  @c SO_RCVBUF value for Tester socket,
 *                    or negative value if its setting is not required.
 */
extern void zfts_establish_tcp_conn_ext(
                              te_bool active,
                              rcf_rpc_server *pco_iut,
                              rpc_zf_attr_p attr,
                              rpc_zf_stack_p stack,
                              rpc_zftl_p iut_zftl,
                              rpc_zft_p *iut_zft,
                              const struct sockaddr *iut_addr,
                              rcf_rpc_server *pco_tst,
                              int *tst_s,
                              const struct sockaddr *tst_addr,
                              int tst_sndbuf, int tst_rcvbuf);

/**
 * Establish TCP connection between ZF zocket and socket,
 * with setting @c SO_SNDBUF and/or @c SO_RCVBUF for Tester socket
 * before connection establishment.
 *
 * @param open_method How to establish connection: actively, passively
 *                    or passively with closing listening zocket.
 * @param pco_iut     IUT RPC server.
 * @param attr        ZF attribute object RPC pointer.
 * @param stack       ZF stack object RPC pointer.
 * @param iut_zftl    Where to save IUT TCP listening zocket handle.
 * @param iut_zft     IUT zocket.
 * @param iut_addr    IUT network address.
 * @param pco_tst     Tester RPC server.
 * @param tst_s       Tester socket.
 * @param tst_addr    Tester network address.
 * @param tst_sndbuf  @c SO_SNDBUF value for Tester socket,
 *                    or negative value if its setting is not required.
 * @param tst_rcvbuf  @c SO_RCVBUF value for Tester socket,
 *                    or negative value if its setting is not required.
 */
extern void zfts_establish_tcp_conn_ext2(
                              zfts_conn_open_method open_method,
                              rcf_rpc_server *pco_iut,
                              rpc_zf_attr_p attr,
                              rpc_zf_stack_p stack,
                              rpc_zftl_p *iut_zftl,
                              rpc_zft_p *iut_zft,
                              const struct sockaddr *iut_addr,
                              rcf_rpc_server *pco_tst,
                              int *tst_s,
                              const struct sockaddr *tst_addr,
                              int tst_sndbuf, int tst_rcvbuf);

/**
 * Bind TCP zocket handle to local address of a given type.
 *
 * @param rpcs              RPC server handle.
 * @param handle            ZF TCP zocket handle.
 * @param local_addr_type   Local address type.
 * @param local_addr        Local address to bind.
 * @param flags             Binding flags.
 *
 * @return Forwarding @a rpc_zft_addr_bind() return code.
 */
extern int zfts_zft_handle_bind(rcf_rpc_server *rpcs,
                                rpc_zft_handle_p handle,
                                tapi_address_type local_addr_type,
                                const struct sockaddr *local_addr,
                                int flags);

/**
 * Check that data stored in iovec buffers matches data stored
 * in a given buffer.
 *
 * @param iov       Array of iovec buffers.
 * @param iovcnt    Number of elements in the array.
 * @param data      Buffer with data to compare.
 * @param data_len  Length of the buffer.
 */
extern void zfts_iovec_cmp_data(const rpc_iovec *iov, size_t iovcnt,
                                const char *data, ssize_t data_len);

/**
 * Check that sending from connected TCP zocket to peer works OK.
 *
 * @param pco_iut       RPC server where TCP zocket is allocated.
 * @param stack         RPC pointer ID of ZF stack object.
 * @param iut_zft       RPC pointer ID of TCP zocket.
 * @param pco_tst       RPC server where peer socket is allocated.
 * @param tst_s         Peer socket.
 *
 * @return Number of bytes sent.
 */
extern size_t zfts_zft_check_sending(rcf_rpc_server *pco_iut,
                                     rpc_zf_stack_p stack,
                                     rpc_zft_p iut_zft,
                                     rcf_rpc_server *pco_tst, int tst_s);

/**
 * Check that receiving data on connected TCP zocket works OK.
 *
 * @param pco_iut       RPC server where TCP zocket is allocated.
 * @param stack         RPC pointer ID of ZF stack object.
 * @param iut_zft       RPC pointer ID of TCP zocket.
 * @param pco_tst       RPC server where peer socket is allocated.
 * @param tst_s         Peer socket.
 */
extern void zfts_zft_check_receiving(rcf_rpc_server *pco_iut,
                                     rpc_zf_stack_p stack,
                                     rpc_zft_p iut_zft,
                                     rcf_rpc_server *pco_tst, int tst_s);

/**
 * Check that receiving part of sent data on connected TCP zocket works OK.
 *
 * @param pco_iut       RPC server where TCP zocket is allocated.
 * @param stack         RPC pointer ID of ZF stack object.
 * @param iut_zft       RPC pointer ID of TCP zocket.
 * @param pco_tst       RPC server where peer socket is allocated.
 * @param tst_s         Peer socket.
 */
extern void zfts_zft_check_partial_receiving(rcf_rpc_server *pco_iut,
                                             rpc_zf_stack_p stack,
                                             rpc_zft_p iut_zft,
                                             rcf_rpc_server *pco_tst,
                                             int tst_s);

/**
 * Check that both sending and receiving data on connected
 * TCP zocket works OK.
 *
 * @param pco_iut       RPC server where TCP zocket is allocated.
 * @param stack         RPC pointer ID of ZF stack object.
 * @param iut_zft       RPC pointer ID of TCP zocket.
 * @param pco_tst       RPC server where peer socket is allocated.
 * @param tst_s         Peer socket.
 */
extern void zfts_zft_check_connection(rcf_rpc_server *pco_iut,
                                      rpc_zf_stack_p stack,
                                      rpc_zft_p iut_zft,
                                      rcf_rpc_server *pco_tst, int tst_s);

/**
 * Allocate array of TCP connection structures.
 *
 * @param count     Number of elements in array.
 *
 * @return Pointer to allocated array.
 */
extern zfts_tcp_conn *zfts_tcp_conns_alloc(int count);

/**
 * Establish TCP connection for every TCP connection structure
 * in array.
 *
 * @param conns         TCP connection structures array.
 * @param count         Number of elements in the array.
 * @param active        Whether connection should be established
 *                      actively or passively in relation to the zocket.
 * @param pco_iut       IUT RPC server.
 * @param attr          ZF attribute object RPC pointer.
 * @param stack         ZF stack object RPC pointer.
 * @param iut_zftl      IUT listener zocket (to be used if not @c RPC_NULL).
 * @param iut_addr      IUT network address.
 * @param pco_tst       Tester RPC server.
 * @param tst_addr      Tester network address.
 * @param tst_sndbuf    If not negative, set @c SO_SNDBUF to this value
 *                      for tester socket before establishing connection.
 * @param tst_rcvbuf    If not negative, set @c SO_RCVBUF to this value
 *                      for tester socket before establishing connection.
 * @param tst_ndelay    If @c TRUE, set @c TCP_NODELAY socket option for
 *                      tester socket.
 * @param tst_nonblock  If @c TRUE, make tester socket non-blocking.
 */
extern void zfts_tcp_conns_establish(
                         zfts_tcp_conn *conns,
                         int count,
                         te_bool active,
                         rcf_rpc_server *pco_iut,
                         rpc_zf_attr_p attr,
                         rpc_zf_stack_p stack,
                         rpc_zftl_p iut_zftl,
                         const struct sockaddr *iut_addr,
                         rcf_rpc_server *pco_tst,
                         const struct sockaddr *tst_addr,
                         int tst_sndbuf, int tst_rcvbuf,
                         te_bool tst_ndelay, te_bool tst_nonblock);

/**
 * Establish TCP connection for every TCP connection structure
 * in array.
 *
 * @param conns         TCP connection structures array.
 * @param count         Number of elements in the array.
 * @param open_method   How to establish connection: actively, passively
 *                      or passively with closing listening zocket.
 * @param pco_iut       IUT RPC server.
 * @param attr          ZF attribute object RPC pointer.
 * @param stack         ZF stack object RPC pointer.
 * @param iut_zftl      Where to save IUT listener zocket handle.
 * @param iut_addr      IUT network address.
 * @param pco_tst       Tester RPC server.
 * @param tst_addr      Tester network address.
 * @param tst_sndbuf    If not negative, set @c SO_SNDBUF to this value
 *                      for tester socket before establishing connection.
 * @param tst_rcvbuf    If not negative, set @c SO_RCVBUF to this value
 *                      for tester socket before establishing connection.
 * @param tst_ndelay    If @c TRUE, set @c TCP_NODELAY socket option for
 *                      tester socket.
 * @param tst_nonblock  If @c TRUE, make tester socket non-blocking.
 */
extern void zfts_tcp_conns_establish2(
                         zfts_tcp_conn *conns,
                         int count,
                         zfts_conn_open_method open_method,
                         rcf_rpc_server *pco_iut,
                         rpc_zf_attr_p attr,
                         rpc_zf_stack_p stack,
                         rpc_zftl_p *iut_zftl,
                         const struct sockaddr *iut_addr,
                         rcf_rpc_server *pco_tst,
                         const struct sockaddr *tst_addr,
                         int tst_sndbuf, int tst_rcvbuf,
                         te_bool tst_ndelay, te_bool tst_nonblock2);

/**
 * Release resources allocated for array of TCP connections.
 *
 * @param pco_iut     RPC server on IUT.
 * @param pco_tst     RPC server on Tester.
 * @param conns       Array of TCP connection structures.
 * @param count       Number of elements in the array.
 */
extern void zfts_tcp_conns_destroy(rcf_rpc_server *pco_iut,
                                   rcf_rpc_server *pco_tst,
                                   zfts_tcp_conn *conns,
                                   int count);

/**
 * Read and check data on Tester for all connections from array.
 *
 * @param pco_iut         RPC server on IUT.
 * @param stack           Zetaferno stack RPC pointer.
 * @param pco_tst         RPC server on Tester.
 * @param conns           Array of TCP connection descriptions.
 * @param count           Number of elements in the array.
 */
extern void
zfts_tcp_conns_read_check_data_tst(rcf_rpc_server *pco_iut,
                                   rpc_zf_stack_p stack,
                                   rcf_rpc_server *pco_tst,
                                   zfts_tcp_conn *conns,
                                   int count);

/**
 * Read and check data on IUT for all connections from array.
 *
 * @param pco_iut         RPC server on IUT.
 * @param stack           Zetaferno stack RPC pointer.
 * @param conns           Array of TCP connection descriptions.
 * @param count           Number of elements in the array.
 */
extern void
zfts_tcp_conns_read_check_data_iut(rcf_rpc_server *pco_iut,
                                   rpc_zf_stack_p stack,
                                   zfts_tcp_conn *conns,
                                   int count);


/**
 * Set ZF TCP receive function used in ZFTS auxiliary functions for which
 * it is not passed as a parameter.
 *
 * @param recv_func       Receive function to set.
 */
extern void zfts_set_def_tcp_recv_func(zfts_tcp_recv_func_t recv_func);

/**
 * Read data using @b zft_zc_recv() or zft_recv() (default function can
 * be changed with @b zfts_set_def_tcp_recv_func()).
 *
 * @note The function jumps to @b cleanup in case of fail, i.e. if any of
 * Zetaferno functions return a negative value or any internal problem in
 * used RPCs.
 *
 * @param rpcs        RPC server handle.
 * @param zft_zocket  ZF TCP zocket.
 * @param buf         Buffer where to save data.
 * @param len         Length of buffer.
 *
 * @return Length of received data.
 */
extern size_t zfts_zft_recv(rcf_rpc_server *rpcs, rpc_zft_p zft_zocket,
                            char *buf, size_t len);

/**
 * Read data using @b zft_zc_recv() or zft_recv() (default function can
 * be changed with @b zfts_set_def_tcp_recv_func()). This function reads
 * arbitrary amount of data, not necessary all the data which can be read
 * at the moment.
 *
 * @note The function jumps to @b cleanup in case of fail, i.e. if any of
 *       Zetaferno functions return a negative value or any internal
 *       problem in used RPCs.
 *
 * @param rpcs        RPC server handle.
 * @param zft_zocket  ZF TCP zocket.
 * @param dbuf        te_dbuf to which to append read data.
 *
 * @return Length of received data.
 */
extern size_t zfts_zft_recv_dbuf(rcf_rpc_server *rpcs, rpc_zft_p zft_zocket,
                                 te_dbuf *dbuf);

/**
 * Read data using @b rpc_zft_zc_recv() or @b rpc_zft_recv().
 *
 * @note If you use @b RPC_AWAIT_ERROR() before this functions, it will
 *       return negative value if failure is returned by RPC call for
 *       receive function. If you do not use RPC_AWAIT_ERROR() or failure
 *       occurs in some auxiliary step or RPC call (such as
 *       @b rpc_zft_zc_recv_done()), this function will jump to cleanup.
 *
 * @param rpcs        RPC server handle.
 * @param zft_zocket  ZF TCP zocket.
 * @param func        TCP receive function.
 * @param buf         Buffer where to save data.
 * @param len         Length of buffer.
 *
 * @return Length of received data on success or negative value in case of
 *         failure.
 */
extern ssize_t zfts_zft_recv_func(rcf_rpc_server *rpcs,
                                  rpc_zft_p zft_zocket,
                                  zfts_tcp_recv_func_t func, char *buf,
                                  size_t len);

/**
 * Read data using @b rpc_zft_zc_recv() or @b rpc_zft_recv().
 *
 * @note If you use @b RPC_AWAIT_ERROR() before this functions, it will
 *       return negative value if failure is returned by RPC call for
 *       receive function. If you do not use RPC_AWAIT_ERROR() or failure
 *       occurs in some auxiliary step or RPC call (such as
 *       @b rpc_zft_zc_recv_done()), this function will jump to cleanup.
 *
 * @param rpcs        RPC server handle.
 * @param zft_zocket  ZF TCP zocket.
 * @param func        TCP receive function.
 * @param dbuf        te_dbuf to which to append read data.
 *
 * @return Length of received data on success or negative value in case of
 *         failure.
 */
extern ssize_t zfts_zft_recv_func_dbuf(rcf_rpc_server *rpcs,
                                       rpc_zft_p zft_zocket,
                                       zfts_tcp_recv_func_t func,
                                       te_dbuf *dbuf);

/**
 * Send data using @b rpc_zft_send.
 *
 * @note The function jumps to @b cleanup in case of fail, i.e. if any of
 * Zetaferno functions returns a negative value or any internal problem in
 * used RPCs.
 *
 * @param rpcs        RPC server handle.
 * @param zft_zocket  ZF TCP zocket.
 * @param buf         Buffer with data to send.
 * @param len         Length of buffer.
 *
 * @return Result of @b rpc_zft_send() call.
 */
extern int zfts_zft_send(rcf_rpc_server *rpcs,
                         rpc_zft_p zft_zocket,
                         char *buf, size_t len);

/**
 * Send data using @b rpc_zft_send() or @b rpc_zft_send_single().
 *
 * @note The only RPC call this function uses is TCP send function,
 *       so you can use RPC_AWAIT_ERROR() with it. Otherwise
 *       (or in case of error in auxiliary step like memory allocation)
 *       it will jump to cleanup.
 *
 * @param rpcs        RPC server handle.
 * @param zft_zocket  RPC pointer to ZF TCP zocket.
 * @param func        TCP send function.
 * @param buf         Buffer with data to send.
 * @param len         Length of buffer.
 *
 * @return Length of sent data on success or negative value in case of
 *         failure.
 */
extern ssize_t zfts_zft_send_func(rcf_rpc_server *rpcs,
                                  rpc_zft_p zft_zocket,
                                  zfts_tcp_send_func_t func,
                                  char *buf, size_t len);

/**
 * Read data from ZF TCP zocket.
 *
 * @param rpcs            RPC server.
 * @param zocket          RPC pointer to zocket.
 * @param received_data   Where to save received data.
 * @param received_len    Where to save received data length,
 *                        if not @c NULL.
 *
 * @return Return value of used RPC call.
 */
extern int zfts_zft_read_data(rcf_rpc_server *rpcs,
                              rpc_zft_p zocket,
                              te_dbuf *received_data,
                              size_t *received_len);

/**
 * Read all the data from ZF TCP zocket (including
 * data which may be in overfilled send buffer
 * of peer).
 *
 * @param rpcs            RPC server.
 * @param stack           RPC pointer to ZF stack object.
 * @param zocket          RPC pointer to zocket.
 * @param received_data   Where to save received data.
 */
extern void zfts_zft_read_all(rcf_rpc_server *rpcs,
                              rpc_zf_stack_p stack,
                              rpc_zft_p zocket,
                              te_dbuf *received_data);

/**
 * Read all the data from peer of ZF TCP zocket (including
 * data which may be in overfilled send buffer).
 *
 * @param pco_iut         RPC server on IUT.
 * @param stack           RPC pointer to ZF stack object.
 * @param pco_tst         RPC server on Tester.
 * @param tst_s           Socket on Tester.
 * @param received_data   Where to save received data.
 */
extern void zfts_zft_peer_read_all(rcf_rpc_server *pco_iut,
                                   rpc_zf_stack_p stack,
                                   rcf_rpc_server *pco_tst,
                                   int tst_s,
                                   te_dbuf *received_data);

/**
 * Call @p zf_process_events() repeatedly until TCP zocket
 * is in either @c TCP_CLOSE or @c TCP_ESTABLISHED state.
 *
 * @param pco_iut           RPC server on IUT.
 * @param stack             RPC pointer to ZF stack object.
 * @param zocket            RPC pointer to ZF TCP zocket.
 */
extern void zfts_wait_for_final_tcp_state(rcf_rpc_server *pco_iut,
                                          rpc_zf_stack_p stack,
                                          rpc_zft_p zocket);

/**
 * Call @p zf_process_events() repeatedly until TCP zocket
 * is in either @c TCP_CLOSE or @c TCP_ESTABLISHED state.
 *
 * @param pco_iut           RPC server on IUT.
 * @param stack             RPC pointer to ZF stack object.
 * @param zocket            RPC pointer to ZF TCP zocket.
 * @param wait_established  If @c TRUE, wait for @c TCP_ESTABLISHED
 *                          state.
 * @param wait_closed       If @c TRUE, wait for @c TCP_CLOSE
 *                          state.
 */
extern void zfts_wait_for_final_tcp_state_ext(rcf_rpc_server *pco_iut,
                                              rpc_zf_stack_p stack,
                                              rpc_zft_p zocket,
                                              te_bool wait_established,
                                              te_bool wait_closed);


/**
 * Call all possible ZF API function on TCP zocket, check
 * results.
 *
 * @param pco_iut       RPC server on IUT.
 * @param stack         RPC pointer of ZF stack.
 * @param iut_zft       RPC pointer of TCP zocket.
 * @param iut_if        Network interface on IUT.
 * @param iut_addr      Network address on IUT.
 * @param pco_tst       RPC server on Tester.
 * @param tst_s         Socket on Tester (will be closed
 *                      and set to negative value).
 * @param tst_addr      Network address on Tester.
 * @param recv_func     TCP receive function to use when
 *                      checking data transmission.
 * @param send_func     TCP send function to use when
 *                      checking data transmission.
 *
 */
void zfts_zft_sanity_checks(rcf_rpc_server *pco_iut,
                            rpc_zf_stack_p stack,
                            rpc_zft_p iut_zft,
                            const struct if_nameindex *iut_if,
                            const struct sockaddr *iut_addr,
                            rcf_rpc_server *pco_tst,
                            int *tst_s,
                            const struct sockaddr *tst_addr,
                            zfts_tcp_recv_func_t recv_func,
                            zfts_tcp_send_func_t send_func);


/**
 * Type of handler used for zft_zc_recv_done()
 * or zft_zc_recv_done_some() result processing.
 */
typedef void (*zfts_zft_recv_done_rc_handler)(rpc_zft_p zocket, int rc,
                                              te_errno err);

/**
 * Set handler used to process return value of zft_zc_recv_done()
 * or zft_zc_recv_done_some() called from generic traffic receiving
 * functions.
 *
 * @param h       Handler function pointer.
 */
extern void zfts_set_zft_recv_done_rc_handler(
                                        zfts_zft_recv_done_rc_handler h);

#endif /* !___ZFTS_TCP_H__ */
