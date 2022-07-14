/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Auxiliary test API for ZF multiplexer.
 *
 * Definition of auxiliary test API to work with ZF multiplexer.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef ___ZFTS_MUXER_H__
#define ___ZFTS_MUXER_H__

#include "te_dbuf.h"

/** Default number of events to be passed to @b rpc_zf_muxer_wait. */
#define ZFTS_DEF_MUXER_EVTS_NUM   100

/**
 * Structure associating zocket with its
 * human-readable description.
 */
typedef struct zfts_zock_descr {
    const rpc_ptr *zocket;  /**< Pointer to variable containing
                                 RPC pointer of a zocket. */
    const char    *descr;   /**< Human-readable description. */
} zfts_zock_descr;

/**
 * Call @b rpc_zf_muxer_wait() and check that it returns expected events.
 *
 * @param rpcs          RPC server.
 * @param ms            RPC pointer of ZF muxer set.
 * @param err_msg       String to be added to verdict in case of failure.
 * @param timeout       How long to wait for events (in milliseconds).
 * @param exp_evts      Expected events.
 * @param exp_evts_num  Number of expected events.
 * @param zock_descrs   Descriptions of zockets to be printed in verdicts.
 */
extern void zfts_check_muxer_events(rcf_rpc_server *rpcs,
                                    rpc_zf_muxer_set_p ms,
                                    const char *err_msg,
                                    int timeout,
                                    struct rpc_epoll_event *exp_evts,
                                    int exp_evts_num,
                                    zfts_zock_descr *zock_descrs);

/**
 * Call @b rpc_zf_muxer_wait() and check that it returns expected events.
 *
 * @param rpcs_         RPC server.
 * @param ms_           RPC pointer of ZF muxer set.
 * @param err_msg_      String to be added to verdict in case of failure.
 * @param timeout_      How long to wait for events (in milliseconds).
 * @param zock_descrs_  Descriptions of zockets to be printed in verdicts.
 * @param exp_events_   Expected events.
 */
#define ZFTS_CHECK_MUXER_EVENTS(rpcs_, ms_, err_msg_, \
                                timeout_, \
                                zock_descrs_, \
                                exp_events_...) \
    do {                                                                  \
        struct rpc_epoll_event exp_evts_[] = {                            \
            exp_events_                                                   \
        };                                                                \
        int exp_evts_num_ = sizeof(exp_evts_) /                           \
                            sizeof(struct rpc_epoll_event);               \
                                                                          \
        zfts_check_muxer_events(rpcs_, ms_, err_msg_, timeout_,           \
                                exp_evts_,                                \
                                exp_evts_num_, zock_descrs_);             \
    } while (0)

/**
 * Structure describing peer for listening zocket.
 */
typedef struct zfts_listen_peer {
    int                       lpeer_sock; /**< Peer socket. */
    struct sockaddr_storage   lpeer_addr; /**< Address to which peer socket
                                               is bound. */
} zfts_listen_peer;

/**
 * Maximum length of zocket description.
 */
#define ZFTS_MZOCK_DESCR_LEN 256

/**
 * Structure describing zocket and it peer in multiplexer tests.
 */
typedef struct zfts_mzocket {
    zfts_zocket_type          zock_type;      /**< Zocket type. */
    rpc_ptr                   zocket;         /**< RPC pointer to zocket. */
    rpc_zf_waitable_p         waitable;       /**< RPC pointer to ZF
                                                   waitable object. */
    te_bool                   in_mset;        /**< Zocket is in
                                                   a muxer set. */
    char    descr[ZFTS_MZOCK_DESCR_LEN];      /**< Human-readable
                                                   description (for
                                                   logging). */

    rcf_rpc_server           *pco_tst;        /**< RPC server on Tester. */
    int                       peer_sock;      /**< Peer socket. */

    zfts_listen_peer         *lpeers;         /**< Peers connecting to
                                                   listening zocket. */
    int                       lpeers_count;   /**< Current number of
                                                   elements in lpeers. */
    int                       lpeers_max;     /**< Maximum number of
                                                   elements in lpeers. */

    rcf_rpc_server           *pco_iut;        /**< RPC server on IUT. */
    rpc_zf_attr_p             attr;           /**< RPC pointer to ZF
                                                   attributes object. */
    rpc_zf_stack_p            stack;          /**< RPC pointer to ZF
                                                   stack object. */

    struct sockaddr_storage   iut_addr;       /**< Network address
                                                   on IUT. */
    struct sockaddr_storage   tst_addr;       /**< Network address
                                                   on Tester. */

    uint32_t                  exp_events;     /**< Currently expected
                                                   events. */
    te_dbuf                   iut_sent_data;  /**< Data sent from IUT and
                                                   not received yet. */
    te_dbuf                   tst_sent_data;  /**< Data sent from Tester
                                                   and not received yet. */
    te_dbuf                   tst_recv_data;  /**< Data received on Tester
                                                   and not checked yet. */

    te_bool expect_econnreset; /**< If @c TRUE, expect @c ECONNRESET when
                                    receiving data on IUT */
} zfts_mzocket;

/**
 * Array of zfts_mzocket structures.
 */
typedef struct zfts_mzockets {
    zfts_mzocket    *mzocks;    /**< Pointer to array. */
    int              count;     /**< Number of elements in array. */
} zfts_mzockets;

/**
 * Create zocket of a given type for multiplexer testing.
 *
 * @param zock_type     Zocket type.
 * @param pco_iut       RPC server on IUT.
 * @param attr          RPC pointer to ZF attributes object.
 * @param stack         RPC pointer to ZF stack object.
 * @param iut_addr      Network address on IUT.
 * @param pco_tst       RPC server on Tester.
 * @param tst_addr      Network address on Tester.
 * @param descr         Human-readable description.
 * @param zocket        Pointer to zfts_mzocket structure to be
 *                      initialized.
 */
extern void zfts_create_mzocket(zfts_zocket_type zock_type,
                                rcf_rpc_server *pco_iut,
                                rpc_zf_attr_p attr,
                                rpc_zf_stack_p stack,
                                const struct sockaddr *iut_addr,
                                rcf_rpc_server *pco_tst,
                                const struct sockaddr *tst_addr,
                                const char *descr,
                                zfts_mzocket *mzocket);

/**
 * Destroy zocket of a given type for multiplexer testing.
 *
 * @param zocket        Pointer to zfts_mzocket structure.
 */
extern void zfts_destroy_mzocket(zfts_mzocket *mzocket);


/**
 * Prepare generation of required events on zocket: update
 * exp_events, and overfill send buffer for @c EPOLLOUT event.
 *
 * @param zocket        Pointer to zfts_mzocket structure.
 * @param events        Epoll events.
 */
extern void zfts_mzocket_prepare_events(zfts_mzocket *mzocket,
                                        uint32_t events);

/**
 * Invoke events passed to @b zfts_mzocket_prepare_events().
 *
 * @note This function may be called to invoke events
 *       while on IUT @b zf_muxer_wait() is blocked,
 *       so it should not use RPC calls on IUT.
 *
 * @param zocket        Pointer to zfts_mzocket structure.
 */
extern void zfts_mzocket_invoke_events(zfts_mzocket *mzocket);

/**
 * Process previously invoked events (receive data,
 * get accepted zocket, etc).
 *
 * @param zocket        Pointer to zfts_mzocket structure.
 */
extern void zfts_mzocket_process_events(zfts_mzocket *mzocket);

/**
 * Add events for zocket to muxer set (using @b zf_muxer_add()).
 *
 * @param ms            RPC pointer to ZF muxer set.
 * @param mzocket       Pointer to zfts_mzocket structure.
 * @param events        Which events to wait for.
 */
extern void zfts_mzocket_add_events(rpc_zf_muxer_set_p ms,
                                    zfts_mzocket *mzocket,
                                    uint32_t events);

/**
 * Add events for zocket to muxer set
 * (events specified to @b zfts_mzocket_prepare_events()
 * will be added).
 *
 * @param ms            RPC pointer to ZF muxer set.
 * @param mzocket       Pointer to zfts_mzocket structure.
 */
extern void zfts_mzocket_add_exp_events(rpc_zf_muxer_set_p ms,
                                        zfts_mzocket *mzocket);

/**
 * Change events for zocket in muxer set (using @b zf_muxer_mod()).
 *
 * @param mzocket       Pointer to zfts_mzocket structure.
 * @param events        Which events to wait for.
 */
extern void zfts_mzocket_mod_events(zfts_mzocket *mzocket,
                                    uint32_t events);

/**
 * Change events for zocket in muxer set (using @b zf_muxer_mod())
 * to currently invoked events.
 *
 * @param mzocket       Pointer to zfts_mzocket structure.
 */
extern void zfts_mzocket_mod_exp_events(zfts_mzocket *mzocket);

/**
 * Rearm events for zocket in muxer set (using @b
 * rpc_zf_muxer_mod_rearm()).
 *
 * @param mzocket       Pointer to zfts_mzocket structure.
 */
extern void zfts_mzocket_rearm_events(zfts_mzocket *mzocket);

/**
 * Delete a zocket from muxer set.
 *
 * @param mzocket       Pointer to zfts_mzocket structure.
 */
extern void zfts_mzocket_del(zfts_mzocket *mzocket);

/**
 * Call @b zf_muxer_wait() and check that expected events are returned
 * for a given zocket
 *
 * @param pco_iut       RPC server on IUT.
 * @param ms            RPC pointer to ZF muxer set.
 * @param timeout       Timeout for @b zf_muxer_wait().
 * @param mzocket       Pointer to zfts_mzocket structure.
 * @param err_msg       String to be added to verdict in case
 *                      of failure.
 */
extern void zfts_mzocket_check_events(rcf_rpc_server *pco_iut,
                                      rpc_zf_muxer_set_p ms,
                                      int timeout,
                                      zfts_mzocket *mzocket,
                                      const char *err_msg);

/**
 * Create an array of zockets for multiplexer testing.
 *
 * @param spec          Zockets types to be placed in array
 *                      (examples: "zftl,zft,urx",
 *                      "zftl.in,zft.out,urx.in" - events are
 *                      ignored in the second form).
 * @param pco_iut       RPC server on IUT.
 * @param attr          RPC pointer to ZF attributes object.
 * @param stack         RPC pointer to ZF stack object.
 * @param iut_addr      Network address on IUT.
 * @param pco_tst       RPC server on Tester.
 * @param tst_addr      Network address on Tester.
 * @param mzockets      Where to save pointer to array and number
 *                      of elements.
 */
extern void zfts_create_mzockets(const char *spec,
                                 rcf_rpc_server *pco_iut,
                                 rpc_zf_attr_p attr,
                                 rpc_zf_stack_p stack,
                                 const struct sockaddr *iut_addr,
                                 rcf_rpc_server *pco_tst,
                                 const struct sockaddr *tst_addr,
                                 zfts_mzockets *mzockets);

/**
 * Destroy array of zockets.
 *
 * @param zockets       Pointer to array.
 */
extern void zfts_destroy_mzockets(zfts_mzockets *mzockets);

/**
 * Prepare events for array of zockets.
 *
 * @param mzockets      Zockets array.
 * @param events_spec   Events (examples: "in,out,in",
 *                      "zft.in,zft.out,zftl.in" - zocket types
 *                      are ignored in the second form).
 */
extern void zfts_mzockets_prepare_events(zfts_mzockets *mzockets,
                                         const char *events_spec);

/**
 * Invoke events previously passed to @b zfts_mzockets_prepare_events().
 *
 * @note This function may be called to invoke events
 *       while on IUT @b zf_muxer_wait() is blocked,
 *       so it should not use RPC calls on IUT.
 *
 * @param mzockets       Zockets array.
 */
extern void zfts_mzockets_invoke_events(zfts_mzockets *mzockets);

/**
 * Process events previously invoked.
 *
 * @param mzockets       Zockets array.
 */
extern void zfts_mzockets_process_events(zfts_mzockets *mzockets);

/**
 * Add events for zockets from zockets array to muxer set
 * (using @b zf_muxer_add()).
 *
 * @param ms            RPC pointer to ZF muxer set.
 * @param mzockets      Zockets array.
 * @param events        Which events to wait for.
 */
extern void zfts_mzockets_add_events(rpc_zf_muxer_set_p ms,
                                     zfts_mzockets *mzockets,
                                     uint32_t events);

/**
 * Add events for zockets from zockets array to muxer set
 * (events previously specified with @b zfts_mzockets_prepare_events()
 * will be added for each zocket).
 *
 * @param ms            RPC pointer to ZF muxer set.
 * @param mzockets      Zockets array.
 */
extern void zfts_mzockets_add_exp_events(rpc_zf_muxer_set_p ms,
                                         zfts_mzockets *mzockets);

/**
 * Change events for zockets from zockets array in muxer set
 * (using @b zf_muxer_mod()).
 *
 * @param mzockets      Zockets array.
 * @param events        Which events to wait for.
 */
extern void zfts_mzockets_mod_events(zfts_mzockets *mzockets,
                                     uint32_t events);

/**
 * Change events for zockets from zockets array in muxer set
 * (using @b zf_muxer_mod()) to currently invoked events.
 *
 * @param mzockets      Zockets array.
 */
extern void zfts_mzockets_mod_exp_events(zfts_mzockets *mzockets);

/**
 * Rearm events for zockets from zockets array in muxer set
 * (using @b rpc_zf_muxer_mod_rearm()).
 *
 * @param mzockets      Zockets array.
 */
extern void zfts_mzockets_rearm_events(zfts_mzockets *mzockets);

/**
 * Delete array of zockets from muxer set.
 *
 * @param mzockets      Zockets array.
 */
extern void zfts_mzockets_del(zfts_mzockets *mzockets);

/**
 * Call @b zf_muxer_wait() and check that expected events are returned
 * for array of zockets. If @b zf_muxer_wait() exits before timeout
 * is expired, it is called again multiple times until all events are
 * received or timeout is over.
 *
 * @param pco_iut         RPC server on IUT.
 * @param ms              RPC pointer to ZF muxer set.
 * @param timeout         Timeout for @b zf_muxer_wait(), milliseconds.
 * @param allow_no_evts   If @c TRUE, return @c FALSE instead of failing
 *                        if no events are reported instead of expected
 *                        ones.
 * @param mzockets        Zockets array.
 * @param err_msg         String to be added to verdict in case
 *                        of failure.
 *
 * @return @c TRUE if all is fine, @c FALSE if no events were retrieved
 *         and @p allow_no_evts is @c TRUE. In all the other cases this
 *         function prints verdict and stops testing.
 */
extern te_bool zfts_mzockets_check_events(rcf_rpc_server *pco_iut,
                                          rpc_zf_muxer_set_p ms,
                                          int timeout,
                                          te_bool allow_no_evts,
                                          zfts_mzockets *mzockets,
                                          const char *err_msg);

/**
 * Get muxer events from test parameter string.
 *
 * @param events      Test parameter string
 *
 * @return Muxer events.
 */
extern uint32_t zfts_parse_events(const char *events);

/**
 * Check that @b zf_muxer_wait() terminates successfully
 * returning no events.
 *
 * @param rpcs         RPC server handle.
 * @param muxer_set    RPC pointer to muxer set object.
 * @param timeout      Timeout for @b zf_muxer_wait(), milliseconds.
 * @param err_msg      String to append to verdict in case of failure.
 */
extern void zfts_muxer_wait_check_no_evts(rcf_rpc_server *rpcs,
                                          rpc_zf_muxer_set_p muxer_set,
                                          int timeout, const char *err_msg);

#endif /* !___ZFTS_MUXER_H__ */
