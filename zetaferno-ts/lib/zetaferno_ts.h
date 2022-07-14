/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Common test API definition
 *
 * Definition of common test API.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#ifndef ___ZETAFERNO_TS_H__
#define ___ZETAFERNO_TS_H__

#include "te_config.h"

#include "te_defs.h"
#include "te_rpc_defs.h"
#include "te_errno.h"
#include "te_bufs.h"
#include "te_dbuf.h"
#include "logger_api.h"
#include "te_sleep.h"
#include "tapi_jmp.h"
#include "tapi_sockaddr.h"
#include "tapi_rpc.h"
#include "tapi_rpcsock_macros.h"
#include "tapi_env.h"
#include "tapi_test_log.h"
#include "tapi_rpc_socket.h"
#include "tapi_rpc_client_server.h"
#include "tapi_cfg.h"
#include "tapi_cfg_base.h"
#include "tapi_cfg_sys.h"
#include "tapi_test.h"
#include "tapi_sh_env.h"
#include "tapi_mem.h"
#include "rpc_zf.h"

/** Maximum indivisible datagram size. */
#define ZFTS_DGRAM_MAX 1400

/** Large datagram size. */
#define ZFTS_DGRAM_LARGE 4000

/** Default iov vectors number. */
#define ZFTS_IOVCNT 3

/** How long to wait for ZF events (in milliseconds). */
#define ZFTS_WAIT_EVENTS_TIMEOUT 500

/** Length of VLAN tag in bytes. */
#define ZFTS_VLAN_TAG_LEN 4

/** Prologue suffix for temporary sockstat file. */
#define  ZFTS_SOCKSTAT_SUF_P  "_p"

/** Epilogue suffix for temporary sockstat file. */
#define  ZFTS_SOCKSTAT_SUF_E  "_e"

/** Buffer size to keep a file pathname. */
#define  ZFTS_FILE_NAME_LEN 256

/**
 * Wait for ZF events until ZFTS_WAIT_EVENTS_TIMEOUT is expired.
 *
 * @param rpcs_     RPC server
 * @param stack_    RPC pointer ID of ZF stack object
 * */
#define ZFTS_WAIT_NETWORK(rpcs_, stack_) \
    rpc_zf_process_events_long(rpcs_, stack_,\
                               ZFTS_WAIT_EVENTS_TIMEOUT)

/**
 * Wait for ZF events and process them.
 *
 * @param rpcs_     RPC server
 * @param stack_    RPC pointer ID of ZF stack object
 */
#define ZFTS_WAIT_PROCESS_EVENTS(rpcs_, stack_) \
    do {                                        \
        rpc_zf_wait_for_event(rpcs_, stack_);   \
        rpc_zf_process_events(rpcs_, stack_);   \
    } while (0)

/**
 * Set every element of a static array to the specified
 * value.
 *
 * @param arr_      Array variable name.
 * @oaram val_      Initializing value.
 */
#define ZFTS_INIT_STATIC_ARRAY(arr_, val_) \
    do {                                                            \
        unsigned int i_;                                            \
                                                                    \
        for (i_ = 0; i_ < sizeof(arr_) / sizeof(arr_[0]); i_++)     \
        {                                                           \
            arr_[i_] = val_;                                        \
        }                                                           \
    } while (0)

/**
 * Compare data received with data sent, print verdict and exit
 * in case of mismatch.
 *
 * @param received_data_      Buffer with received data.
 * @param sent_data_          Buffer with sent data.
 * @param received_len_       Length of received data.
 * @param sent_len            Length of sent data.
 * @param format_             String to add to the end of verdict
 *                            in case of failure.
 */
#define ZFTS_CHECK_RECEIVED_DATA(received_data_, sent_data_, \
                                 received_len_, sent_len_, format_...) \
    do {                                                               \
        RING("%ld bytes were sent, %ld bytes were received",           \
             (long int)sent_len_, (long int)received_len_);            \
                                                                       \
        if ((ssize_t)received_len_ < (ssize_t)sent_len_)               \
            TEST_VERDICT("Less data than expected "                    \
                         "was received" format_);                      \
        else if ((ssize_t)received_len_ > (ssize_t)sent_len_)          \
            TEST_VERDICT("More data than expected "                    \
                         "was received" format_);                      \
        else if (memcmp(received_data_, sent_data_,                    \
                        received_len_) != 0)                           \
            TEST_VERDICT("Data received differs "                      \
                         "from data sent" format_);                    \
    } while (0)

/**
 * Successively send all buffers from @p iov using @a rpc_sendto.
 *
 * @note The function jumps to @b cleanup in case of fail.
 *
 * @param rpcs      RPC server handle.
 * @param sock      BSD socket descriptor.
 * @param iov       Iov vectors array.
 * @param iovcnt    The array length.
 * @param dst_addr  Destination address.
 */
extern void zfts_sendto_iov(rcf_rpc_server *rpcs, int sock,
                            rpc_iovec *iov, size_t iovcnt,
                            const struct sockaddr *dst_addr);

/**
 * Initialize ZF library, allocate attributes and stack.
 *
 * @param rpcs      RPC server handle.
 * @param stack     Pointer to the stack object.
 * @param attr      Pointer to the attribute object.
 */
extern void zfts_create_stack(rcf_rpc_server *rpcs, rpc_zf_attr_p *attr,
                              rpc_zf_stack_p *stack);

/**
 * Free ZF attributes and stack and deinitialize ZF library. The function
 * can be safely used in cleanup code, it checks attribute and stack RPC
 * pointers against @c RPC_NULL.
 *
 * @param rpcs      RPC server handle.
 * @param stack     Pointer to the stack object.
 * @param attr      Pointer to the attribute object.
 */
extern void zfts_destroy_stack(rcf_rpc_server *rpcs, rpc_zf_attr_p attr,
                               rpc_zf_stack_p stack);

/**
 * Release resources allocated for Zetaferno object and its RPC pointer.
 *
 * @param rpcs_       RPC server.
 * @param type_       Object type.
 * @param ptr_        RPC pointer.
 */
#define ZFTS_FREE(rpcs_, type_, ptr_) \
    do {                                            \
        if (ptr_ != RPC_NULL)                       \
            rpc_ ## type_ ## _free(rpcs_, ptr_);    \
                                                    \
        ptr_ = RPC_NULL;                            \
    } while (0)

/**
 * Release resources allocated for Zetaferno object and its RPC pointer.
 * The same as ZFTS_FREE, but does not go to 'cleanup' label.
 *
 * @param rpcs_       RPC server.
 * @param type_       Object type.
 * @param ptr_        RPC pointer.
 */
#define CLEANUP_RPC_ZFTS_FREE(rpcs_, type_, ptr_) \
    do {                                                            \
        int rc_;                                                    \
                                                                    \
        if (ptr_ != RPC_NULL)                                       \
        {                                                           \
            RPC_AWAIT_IUT_ERROR(rpcs_);                             \
            if ((rc_ = (rpc_ ## type_ ## _free(rpcs_, ptr_))) != 0) \
                MACRO_TEST_ERROR;                                   \
            else                                                    \
                ptr_ = RPC_NULL;                                    \
        }                                                           \
    } while (0)

/**
 * Deinitialize Zetaferno library.
 *
 * @param rpcs_       RPC server.
 */
#define CLEANUP_RPC_ZF_DEINIT(rpcs_) \
    do {                                        \
        int rc_;                                \
                                                \
        RPC_AWAIT_IUT_ERROR(rpcs_);             \
        if ((rc_ = rpc_zf_deinit(rpcs_)) != 0)  \
            MACRO_TEST_ERROR;                   \
    } while (0)

/**
 * Free a Zetaferno attribute object.
 * The same as rpc_zf_attr_free, but does not go to 'cleanup' label.
 *
 * @param rpcs_     RPC server handle.
 * @param attr_     Pointer to the attribute object.
 */
#define CLEANUP_RPC_ZF_ATTR_FREE(rpcs_, attr_)  \
    do {                                        \
        if (attr_ != RPC_NULL)                  \
        {                                       \
            RPC_AWAIT_IUT_ERROR(rpcs_);         \
            rpc_zf_attr_free(rpcs_, attr_);     \
            if (!RPC_IS_CALL_OK(rpcs_))         \
                MACRO_TEST_ERROR;               \
            attr_ = RPC_NULL;                   \
        }                                       \
    } while (0)

/**
 * Release resources allocated for Zetaferno objects and its RPC pointer,
 * deinitialize Zetaferno library.
 *
 * @param rpcs_       RPC server.
 * @param attr_       Pointer to the attribute object.
 * @param stack_      Pointer to the stack object.
 */
#define CLEANUP_RPC_ZFTS_DESTROY_STACK(rpcs_, attr_, stack_)\
    do {                                                    \
        CLEANUP_RPC_ZFTS_FREE(rpcs_, zf_stack, stack_);     \
        CLEANUP_RPC_ZF_ATTR_FREE(rpcs_, attr_);             \
        CLEANUP_RPC_ZF_DEINIT(rpcs_);                       \
    } while (0)

/**
 * Types of zockets.
 */
typedef enum {
    ZFTS_ZOCKET_URX = 0,    /**< UDP RX zocket. */
    ZFTS_ZOCKET_UTX,        /**< UDP TX zocket. */
    ZFTS_ZOCKET_ZFTL,       /**< TCP listening zocket. */
    ZFTS_ZOCKET_ZFT_ACT,    /**< TCP connected zocket (actively opened). */
    ZFTS_ZOCKET_ZFT_PAS,    /**< TCP connected zocket (passively opened). */
    ZFTS_ZOCKET_UNKNOWN,    /**< Unknown zocket type. */
} zfts_zocket_type;

/** Default ZFT zocket type. */
#define ZFTS_ZOCKET_ZFT ZFTS_ZOCKET_ZFT_ACT

/**
 * Check whether zocket type defines ZFT zocket.
 *
 * @param type    Zocket type
 *
 * @return @c TRUE if zocket type defines ZFT zocket, @c FALSE otherwise.
 */
static inline te_bool
zfts_zocket_type_zft(zfts_zocket_type type)
{
    return (type == ZFTS_ZOCKET_ZFT_ACT ||
            type == ZFTS_ZOCKET_ZFT_PAS);
}

/**
 * List of zocket types, can be passed to macro @b TEST_GET_ENUM_PARAM.
 */
#define ZFTS_ZOCKET_TYPES  \
    { "urx",      ZFTS_ZOCKET_URX }, \
    { "utx",      ZFTS_ZOCKET_UTX }, \
    { "zftl",     ZFTS_ZOCKET_ZFTL }, \
    { "zft",      ZFTS_ZOCKET_ZFT }, \
    { "zft-act",  ZFTS_ZOCKET_ZFT_ACT }, \
    { "zft-pas",  ZFTS_ZOCKET_ZFT_PAS }

/**
 * Convert zocket type to string representation.
 *
 * @param zock_type     Zocket type.
 *
 * @return String representation or "<unknown>".
 */
extern const char *zfts_zocket_type2str(zfts_zocket_type zock_type);

/**
 * Convert string to zocket type.
 *
 * @param str     String.
 *
 * @return Zocket type.
 */

extern zfts_zocket_type zfts_str2zocket_type(const char *str);

/**
 * Create and prepare zocket of given type.
 * Peer on Tester side is necessary for some types of zockets
 * to establish connection (actively or passively opened TCP connection).
 * In other cases, passing the peer to this function is optional.
 *
 * @param zock_type       Zocket type.
 * @param pco_iut         RPC server on IUT.
 * @param attr            RPC pointer of ZF attributes object.
 * @param stack           RPC pointer of ZF stack object.
 * @param iut_addr        Network address on IUT.
 * @param pco_tst         RPC server on Tester.
 * @param tst_addr        Network address on Tester.
 * @param zocket          Where to save RPC pointer of zocket.
 * @param waitable        Where to save RPC pointer of ZF waitable object.
 * @oaram tst_s           Where to save peer socket.
 */
extern void zfts_create_zocket(zfts_zocket_type zock_type,
                               rcf_rpc_server *pco_iut,
                               rpc_zf_attr_p attr,
                               rpc_zf_stack_p stack,
                               struct sockaddr *iut_addr,
                               rcf_rpc_server *pco_tst,
                               const struct sockaddr *tst_addr,
                               rpc_ptr *zocket,
                               rpc_zf_waitable_p *waitable,
                               int *tst_s);

/**
 * Destroy zocket of a given type.
 *
 * @param zock_type       Zocket type.
 * @param pco_iut         RPC server on IUT.
 * @param zocket          RPC pointer of ZF zocket.
 */
extern void zfts_destroy_zocket(zfts_zocket_type zock_type,
                                rcf_rpc_server *pco_iut,
                                rpc_ptr zocket);

/**
 * Configure routes and other options to use a gateway host to control
 * traffic. The macro can be used with environments like
 * @c env.peer2peer_gw and @c env.peer2peer_tst_gw .
 */
#define ZFTS_CONFIGURE_GATEWAY \
    do {                                                                \
        rpc_socket_addr_family family;                                  \
                                                                        \
        family = iut_addr->sa_family;                                   \
                                                                        \
        CHECK_RC(tapi_cfg_add_route_via_gw(pco_iut->ta,                 \
                 family,                                                \
                 te_sockaddr_get_netaddr(tst_addr),                     \
                 te_netaddr_get_size(family) * 8,                       \
                 te_sockaddr_get_netaddr(gw_iut_addr)));                \
                                                                        \
        CHECK_RC(tapi_cfg_add_route_via_gw(pco_tst->ta,                 \
                 family,                                                \
                 te_sockaddr_get_netaddr(iut_addr),                     \
                 te_netaddr_get_size(family) * 8,                       \
                 te_sockaddr_get_netaddr(gw_tst_addr)));                \
                                                                        \
    } while (0)

/**
 * Enable or disable IPv4 forwarding on a previously configured gateway.
 * The macro can be used with environments like
 * @c env.peer2peer_gw and @c env.peer2peer_tst_gw.
 */
#define ZFTS_GATEWAY_SET_FORWARDING(enabled_) \
    do {                                                                \
        CHECK_RC(tapi_cfg_sys_set_int(pco_gw->ta, enabled_ ? 1 : 0,     \
                                      NULL, "net/ipv4/ip_forward"));    \
    } while (0)

/**
 * Call RPC and exit with verdict if it returned a negative value.
 *
 * @param call_       RPC call.
 * @param rpcs_       RPC server handle.
 * @param rpc_name_   Call name to be used in verdict in case of failure.
 */
#define ZFTS_CHECK_RPC(call_, rpcs_, rpc_name_) \
    do {                                                    \
        te_errno rc_;                                       \
                                                            \
        RPC_AWAIT_ERROR(rpcs_);                             \
        rc_ = call_;                                        \
        if (rc_ < 0)                                        \
            TEST_VERDICT("%s failed with errno %r",         \
                         rpc_name_, RPC_ERRNO(rpcs_));      \
    } while (0)

/**
 * Call @b poll() to wait until data arrives on a socket.
 *
 * @param rpcs        RPC server.
 * @param s           Socket.
 * @param tineout     Timeout (in milliseconds).
 *
 * @return Result of @b rpc_poll().
 */
extern int zfts_sock_wait_data(rcf_rpc_server *rpcs,
                               int s, int timeout);

/**
 * Generate pathname for temporary sockstat file.
 *
 * @param rpcs  RPC server handle.
 * @param suf   File name suffix.
 * @param name  Buffer to save file name.
 * @param len   The buffer length.
 */
extern void zfts_sockstat_file_name(rcf_rpc_server *rpcs, const char *suf,
                                    char *name, size_t len);

/**
 * Save sockstat data in the local @p file.
 *
 * @param rpcs      RPC server handle.
 * @param pathname  Temporary sockstat file pathname.
 *
 * @return Status code.
 */
extern te_errno zfts_sockstat_save(rcf_rpc_server *rpcs,
                                   const char *pathname);

/**
 * Compare two files with sockstat data.
 *
 * @param sf1   Pathname of the first file.
 * @param sf2   Pathname of the second file.
 *
 * @return Status code.
 */
extern te_errno zfts_sockstat_cmp(const char *sf1, const char *sf2);

/**
 * Get string representation of clock synchronization flags.
 *
 * @param rpc_flags       Flags.
 *
 * @return Pointer to string constant.
 */
static inline const char *
zf_sync_flags_rpc2str(unsigned int rpc_flags)
{
    if ((rpc_flags & TARPC_ZF_SYNC_FLAG_CLOCK_SET) &&
        (rpc_flags & TARPC_ZF_SYNC_FLAG_CLOCK_IN_SYNC))
        return "CLOCK_SET | CLOCK_IN_SYNC";
    else if (rpc_flags & TARPC_ZF_SYNC_FLAG_CLOCK_SET)
        return "CLOCK_SET";
    else if (rpc_flags & TARPC_ZF_SYNC_FLAG_CLOCK_IN_SYNC)
        return "CLOCK_IN_SYNC";

    return "";
}

/**
 * Get string representation of ZF packet report flags.
 *
 * @param rpc_flags       Flags (see tarpc_zf_pkt_report_flags).
 *
 * @return Pointer to statically allocated string.
 */
static inline const char *
zf_pkt_report_flags_rpc2str(unsigned int rpc_flags)
{
#define PKT_REPORT_FLAG_MAP_ENTRY(_val) \
    { #_val, (unsigned int)TARPC_ZF_PKT_REPORT_ ## _val }

    struct rpc_bit_map_entry map[] = {
        PKT_REPORT_FLAG_MAP_ENTRY(CLOCK_SET),
        PKT_REPORT_FLAG_MAP_ENTRY(IN_SYNC),
        PKT_REPORT_FLAG_MAP_ENTRY(NO_TIMESTAMP),
        PKT_REPORT_FLAG_MAP_ENTRY(DROPPED),
        PKT_REPORT_FLAG_MAP_ENTRY(TCP_RETRANS),
        PKT_REPORT_FLAG_MAP_ENTRY(TCP_SYN),
        PKT_REPORT_FLAG_MAP_ENTRY(TCP_FIN),
        PKT_REPORT_FLAG_MAP_ENTRY(UNKNOWN),
        { NULL, 0 }
    };

#undef PKT_REPORT_FLAG_MAP_ENTRY

    return bitmask2str(map, rpc_flags);
}

/**
 * Split data from a buffer between a few rpc_iovec structures.
 *
 * @param buf           Pointer to the buffer.
 * @param len           Length of the data.
 * @param min_iov_len   Minimum length of data in single rpc_iovec
 *                      (less may be assigned to the last one).
 * @param max_iov_len   Maximum length of data in single rpc_iovec.
 * @param iov_out       Where to save pointer to allocated array
 *                      of rpc_iovec.
 * @param num_out       Where to save number of elements in the array.
 */
extern void zfts_split_in_iovs(const char *buf, size_t len,
                               int min_iov_len, int max_iov_len,
                               rpc_iovec **iov_out, int *num_out);

#endif /* !___ZETAFERNO_TS_H__ */
