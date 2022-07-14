/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief RPC entities definition
 *
 * Definition of RPC structures and functions for Zetaferno Test Suite.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

typedef uint64_t tarpc_zf_althandle;

/**
 * Transmitting functions.
 */
enum zfts_send_function {
    ZFTS_ZFUT_SEND = 0,     /**< UDP iov send. */
    ZFTS_ZFUT_SEND_SINGLE,  /**< Copy-based send of single packet. */
    ZFTS_ZFUT_SEND_INVALID  /**< Invalid value. */
};

struct tarpc_zfur_msg {
  tarpc_int             reserved<>;
  tarpc_int             dgrams_left;
  tarpc_int             flags;
  tarpc_int             pftf_len;
  tarpc_int             iovcnt;
  struct tarpc_iovec    iov<>;
  tarpc_ptr             ptr;
};

typedef struct tarpc_void_in tarpc_zf_init_in;
typedef struct tarpc_int_retval_out tarpc_zf_init_out;

typedef struct tarpc_void_in tarpc_zf_deinit_in;
typedef struct tarpc_int_retval_out tarpc_zf_deinit_out;

typedef struct tarpc_void_in tarpc_zf_attr_alloc_in;

struct tarpc_zf_attr_alloc_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               attr;
    tarpc_int               retval;
};

struct tarpc_zf_attr_free_in {
    struct tarpc_in_arg common;
    tarpc_ptr           attr;
};

typedef struct tarpc_void_out tarpc_zf_attr_free_out;


struct tarpc_zf_stack_alloc_in {
    struct tarpc_in_arg common;
    tarpc_ptr           attr;
};

struct tarpc_zf_stack_alloc_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               stack;
    tarpc_int               retval;
};

struct tarpc_zf_stack_free_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
};

typedef struct tarpc_int_retval_out tarpc_zf_stack_free_out;

struct tarpc_zf_stack_is_quiescent_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
};

typedef struct tarpc_int_retval_out tarpc_zf_stack_is_quiescent_out;

struct tarpc_zf_stack_has_pending_work_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
};

typedef struct tarpc_int_retval_out tarpc_zf_stack_has_pending_work_out;

struct tarpc_zf_stack_to_waitable_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
};

struct tarpc_zf_stack_to_waitable_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               zf_waitable;
};

struct tarpc_zf_reactor_perform_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
};

typedef struct tarpc_int_retval_out tarpc_zf_reactor_perform_out;

struct tarpc_zfur_alloc_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
    tarpc_ptr           attr;
};

struct tarpc_zfur_alloc_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               urx;
    tarpc_int               retval;
};

struct tarpc_zfur_free_in {
    struct tarpc_in_arg common;
    tarpc_ptr           urx;
};

typedef struct tarpc_int_retval_out tarpc_zfur_free_out;

struct tarpc_zfur_addr_bind_in {
    struct tarpc_in_arg common;
    tarpc_ptr           urx;
    struct tarpc_sa     laddr;
    tarpc_socklen_t     laddrlen;
    tarpc_bool          fwd_laddrlen;
    struct tarpc_sa     raddr;
    tarpc_socklen_t     raddrlen;
    tarpc_bool          fwd_raddrlen;
    tarpc_int           flags;
};

struct tarpc_zfur_addr_bind_out {
    struct tarpc_out_arg    common;
    struct tarpc_sa         laddr;
    tarpc_int               retval;
};

struct tarpc_zfur_addr_unbind_in {
    struct tarpc_in_arg common;
    tarpc_ptr           urx;
    struct tarpc_sa     laddr;
    tarpc_socklen_t     laddrlen;
    tarpc_bool          fwd_laddrlen;
    struct tarpc_sa     raddr;
    tarpc_socklen_t     raddrlen;
    tarpc_bool          fwd_raddrlen;
    tarpc_int           flags;
};

typedef struct tarpc_int_retval_out tarpc_zfur_addr_unbind_out;

enum tarpc_zf_zc_flags {
    TARPC_ZF_OVERLAPPED_WAIT = 0x10000,
    TARPC_ZF_OVERLAPPED_COMPLETE = 0x20000
};

enum tarpc_zf_epoll_events {
    TARPC_ZF_EPOLLIN_OVERLAPPED = 0x10000
};

struct tarpc_zfur_zc_recv_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               urx;
    struct tarpc_zfur_msg   msg;
    tarpc_int               flags;
};

struct tarpc_zfur_zc_recv_out {
    struct tarpc_out_arg    common;
    struct tarpc_zfur_msg   msg;
};

struct tarpc_zfur_zc_recv_done_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               urx;
    struct tarpc_zfur_msg   msg;
};

struct tarpc_zfur_zc_recv_done_out {
    struct tarpc_out_arg    common;
    struct tarpc_zfur_msg   msg;
    tarpc_int               retval;
};

struct tarpc_zfur_pkt_get_header_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               urx;
    struct tarpc_zfur_msg   msg;
    tarpc_int               pktind;
};

struct tarpc_zfur_pkt_get_header_out {
    struct tarpc_out_arg    common;
    tarpc_uchar             iphdr<>;
    tarpc_uchar             udphdr<>;
    tarpc_int               retval;
};

struct tarpc_zfur_zc_recv_send_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               urx;
    struct tarpc_zfur_msg   msg;
    tarpc_ptr               utx;
    zfts_send_function      send_func;
};

struct tarpc_zfur_zc_recv_send_out {
    struct tarpc_out_arg    common;
    struct tarpc_zfur_msg   msg;
    tarpc_int               retval;
};

struct tarpc_zfur_flooder_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               stack;
    tarpc_ptr               urx;
    tarpc_int               duration;
};

struct tarpc_zfur_flooder_out {
    struct tarpc_out_arg    common;
    uint64_t                stats;
    tarpc_int               retval;
};

struct tarpc_zfur_to_waitable_in {
    struct tarpc_in_arg common;
    tarpc_ptr           urx;
};

struct tarpc_zfur_to_waitable_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               zf_waitable;
};

struct tarpc_zfut_alloc_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
    tarpc_ptr           attr;
    struct tarpc_sa     laddr;
    tarpc_socklen_t     laddrlen;
    tarpc_bool          fwd_laddrlen;
    struct tarpc_sa     raddr;
    tarpc_socklen_t     raddrlen;
    tarpc_bool          fwd_raddrlen;
    tarpc_int           flags;
};

struct tarpc_zfut_alloc_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               utx;
    tarpc_int               retval;
};

struct tarpc_zfut_free_in {
    struct tarpc_in_arg common;
    tarpc_ptr           utx;
};

typedef struct tarpc_int_retval_out tarpc_zfut_free_out;


struct tarpc_zfut_send_in {
    struct tarpc_in_arg common;
    tarpc_ptr           utx;
    tarpc_int           iovcnt;
    struct tarpc_iovec  iovec<>;
    tarpc_int           flags;
};

typedef struct tarpc_int_retval_out tarpc_zfut_send_out;

struct tarpc_zfut_get_mss_in {
    struct tarpc_in_arg common;
    tarpc_ptr           utx;
};

typedef struct tarpc_int_retval_out tarpc_zfut_get_mss_out;

struct tarpc_zfut_send_single_in {
    struct tarpc_in_arg common;
    tarpc_ptr           utx;
    tarpc_uchar         buf<>;
};

typedef struct tarpc_int_retval_out tarpc_zfut_send_single_out;

struct tarpc_zfut_flooder_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               stack;
    tarpc_ptr               utx;
    zfts_send_function      send_func;
    tarpc_int               dgram_size;
    tarpc_int               iovcnt;
    tarpc_int               duration;
};

struct tarpc_zfut_flooder_out {
    struct tarpc_out_arg    common;
    uint64_t                stats;
    uint64_t                errors;
    tarpc_int               retval;
};

struct tarpc_zfut_to_waitable_in {
    struct tarpc_in_arg common;
    tarpc_ptr           utx;
};

struct tarpc_zfut_to_waitable_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               zf_waitable;
};

struct tarpc_zfut_get_header_size_in {
    struct tarpc_in_arg common;
    tarpc_ptr           utx;
};

struct tarpc_zfut_get_header_size_out {
    struct tarpc_out_arg    common;
    tarpc_uint              retval;
};

struct tarpc_zf_wait_for_event_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
};

typedef struct tarpc_int_retval_out tarpc_zf_wait_for_event_out;

struct tarpc_zf_process_events_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
};

typedef struct tarpc_int_retval_out tarpc_zf_process_events_out;

struct tarpc_zf_process_events_long_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
    int                 duration;
};

typedef struct tarpc_int_retval_out tarpc_zf_process_events_long_out;

struct tarpc_zf_waitable_free_in {
    struct tarpc_in_arg common;
    tarpc_ptr           zf_waitable;
};

typedef struct tarpc_int_retval_out tarpc_zf_waitable_free_out;

struct tarpc_zf_attr_set_int_in {
    struct tarpc_in_arg common;
    tarpc_ptr           attr;
    string              name<>;
    int64_t             val;
};

typedef struct tarpc_int_retval_out tarpc_zf_attr_set_int_out;

struct tarpc_zftl_listen_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
    struct tarpc_sa     laddr;
    tarpc_socklen_t     laddrlen;
    tarpc_bool          fwd_laddrlen;
    tarpc_ptr           attr;
};

struct tarpc_zftl_listen_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               tl;
    tarpc_int               retval;
};

struct tarpc_zftl_getname_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               tl;
    struct tarpc_sa         laddr;
};

struct tarpc_zftl_getname_out {
    struct tarpc_out_arg    common;
    struct tarpc_sa         laddr;
};

struct tarpc_zftl_accept_in {
    struct tarpc_in_arg common;
    tarpc_ptr           tl;
};

struct tarpc_zftl_accept_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               ts;
    tarpc_int               retval;
};

struct tarpc_zftl_to_waitable_in {
    struct tarpc_in_arg common;
    tarpc_ptr           tl;
};

struct tarpc_zftl_to_waitable_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               zf_waitable;
};

struct tarpc_zftl_free_in {
    struct tarpc_in_arg common;
    tarpc_ptr           tl;
};

typedef struct tarpc_int_retval_out tarpc_zftl_free_out;

struct tarpc_zft_to_waitable_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
};

struct tarpc_zft_to_waitable_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               zf_waitable;
};

struct tarpc_zft_alloc_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
    tarpc_ptr           attr;
};

struct tarpc_zft_alloc_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               handle;
    tarpc_int               retval;
};

struct tarpc_zft_addr_bind_in {
    struct tarpc_in_arg common;
    struct tarpc_sa     laddr;
    tarpc_socklen_t     laddrlen;
    tarpc_bool          fwd_laddrlen;
    tarpc_ptr           handle;
    tarpc_int           flags;
};

typedef struct tarpc_int_retval_out tarpc_zft_addr_bind_out;

struct tarpc_zft_connect_in {
    struct tarpc_in_arg common;
    struct tarpc_sa     raddr;
    tarpc_socklen_t     raddrlen;
    tarpc_bool          fwd_raddrlen;
    tarpc_ptr           handle;
};

struct tarpc_zft_connect_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               ts;
    tarpc_int               retval;
};

struct tarpc_zft_shutdown_tx_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
};

typedef struct tarpc_int_retval_out tarpc_zft_shutdown_tx_out;

struct tarpc_zft_handle_free_in {
    struct tarpc_in_arg common;
    tarpc_ptr           handle;
};

typedef struct tarpc_int_retval_out tarpc_zft_handle_free_out;

struct tarpc_zft_free_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
};

typedef struct tarpc_int_retval_out tarpc_zft_free_out;

struct tarpc_zft_state_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
};

typedef struct tarpc_int_retval_out tarpc_zft_state_out;

struct tarpc_zft_error_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
};

typedef struct tarpc_int_retval_out tarpc_zft_error_out;

struct tarpc_zft_handle_getname_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               handle;
    struct tarpc_sa         laddr;
};

struct tarpc_zft_handle_getname_out {
    struct tarpc_out_arg    common;
    struct tarpc_sa         laddr;
};

struct tarpc_zft_getname_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               ts;
    struct tarpc_sa         laddr;
    struct tarpc_sa         raddr;
};

struct tarpc_zft_getname_out {
    struct tarpc_out_arg    common;
    struct tarpc_sa         laddr;
    struct tarpc_sa         raddr;
};

struct tarpc_zft_msg {
  tarpc_int             reserved<>;
  tarpc_int             pkts_left;
  tarpc_int             flags;
  tarpc_int             pftf_len;
  tarpc_int             iovcnt;
  struct tarpc_iovec    iov<>;
  tarpc_ptr             ptr;
};

struct tarpc_zft_zc_recv_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               ts;
    struct tarpc_zft_msg    msg;
    tarpc_int               flags;
};

struct tarpc_zft_zc_recv_out {
    struct tarpc_out_arg    common;
    struct tarpc_zft_msg    msg;
};

struct tarpc_zft_zc_recv_done_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               ts;
    struct tarpc_zft_msg    msg;
};

struct tarpc_zft_zc_recv_done_out {
    struct tarpc_out_arg    common;
    struct tarpc_zft_msg    msg;
    tarpc_int               retval;
};

struct tarpc_zft_zc_recv_done_some_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               ts;
    struct tarpc_zft_msg    msg;
    tarpc_size_t            len;
};

struct tarpc_zft_zc_recv_done_some_out {
    struct tarpc_out_arg    common;
    struct tarpc_zft_msg    msg;
    tarpc_int               retval;
};

struct tarpc_zft_recv_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
    tarpc_int           iovcnt;
    struct tarpc_iovec  iovec<>;
    tarpc_int           flags;
};

struct tarpc_zft_recv_out {
    struct tarpc_out_arg    common;
    tarpc_int               retval;
    struct tarpc_iovec      iovec<>;
};

struct tarpc_zft_read_zft_msg_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               msg_ptr;
};

struct tarpc_zft_read_zft_msg_out {
    struct tarpc_out_arg    common;
    struct tarpc_zft_msg     msg;
};

struct tarpc_zft_send_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
    tarpc_int           iovcnt;
    struct tarpc_iovec  iovec<>;
    tarpc_int           flags;
};

struct tarpc_zft_send_out {
    struct tarpc_out_arg    common;
    tarpc_ssize_t           retval;
};

struct tarpc_zft_send_single_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
    tarpc_uchar         buf<>;
    tarpc_int           flags;
};

struct tarpc_zft_send_single_out {
    struct tarpc_out_arg    common;
    tarpc_ssize_t           retval;
};

struct tarpc_zft_send_space_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
};

struct tarpc_zft_send_space_out {
    struct tarpc_out_arg    common;
    tarpc_size_t            size;
    tarpc_int               retval;
};

struct tarpc_zft_get_mss_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
};

typedef struct tarpc_int_retval_out tarpc_zft_get_mss_out;

struct tarpc_zft_get_header_size_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
};

struct tarpc_zft_get_header_size_out {
    struct tarpc_out_arg    common;
    tarpc_uint              retval;
};

struct tarpc_zfur_read_zfur_msg_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               msg_ptr;
};

struct tarpc_zfur_read_zfur_msg_out {
    struct tarpc_out_arg    common;
    struct tarpc_zfur_msg   msg;
};

struct tarpc_zf_muxer_alloc_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
};

struct tarpc_zf_muxer_alloc_out {
    struct tarpc_out_arg    common;
    tarpc_ptr               muxer_set;
    tarpc_int               retval;
};

struct tarpc_zf_muxer_free_in {
    struct tarpc_in_arg common;
    tarpc_ptr           muxer_set;
};

typedef struct tarpc_void_out tarpc_zf_muxer_free_out;

struct tarpc_zf_muxer_add_in {
    struct tarpc_in_arg       common;
    tarpc_ptr                 muxer_set;
    tarpc_ptr                 waitable;
    struct tarpc_epoll_event  events<>;
};

typedef struct tarpc_int_retval_out tarpc_zf_muxer_add_out;

struct tarpc_zf_muxer_mod_in {
    struct tarpc_in_arg       common;
    tarpc_ptr                 waitable;
    struct tarpc_epoll_event  events<>;
};

typedef struct tarpc_int_retval_out tarpc_zf_muxer_mod_out;

struct tarpc_zf_muxer_del_in {
    struct tarpc_in_arg       common;
    tarpc_ptr                 waitable;
};

typedef struct tarpc_int_retval_out tarpc_zf_muxer_del_out;

struct tarpc_zf_muxer_wait_in {
    struct tarpc_in_arg common;

    tarpc_ptr                muxer_set;
    struct tarpc_epoll_event events<>;
    tarpc_int                maxevents;
    int64_t                  timeout;
};

struct tarpc_zf_muxer_wait_out {
    struct tarpc_out_arg    common;

    tarpc_int               retval;

    struct tarpc_epoll_event events<>;
};

struct tarpc_zf_waitable_event_in {
    struct tarpc_in_arg       common;
    tarpc_ptr                 waitable;
};

struct tarpc_zf_waitable_event_out {
    struct tarpc_out_arg      common;
    struct tarpc_epoll_event  event;
};

struct tarpc_zf_waitable_fd_get_in {
    struct tarpc_in_arg       common;
    tarpc_ptr                 stack;
};

struct tarpc_zf_waitable_fd_get_out {
    struct tarpc_out_arg      common;
    tarpc_int                 fd;
    tarpc_int                 retval;
};

struct tarpc_zf_waitable_fd_prime_in {
    struct tarpc_in_arg       common;
    tarpc_ptr                 stack;
};

typedef struct tarpc_int_retval_out tarpc_zf_waitable_fd_prime_out;

struct tarpc_zf_muxer_mod_rearm_in {
    struct tarpc_in_arg       common;
    tarpc_ptr                 waitable;
};

typedef struct tarpc_int_retval_out tarpc_zf_muxer_mod_rearm_out;

struct tarpc_zf_alternatives_alloc_in {
    struct tarpc_in_arg common;

    tarpc_ptr           stack;
    tarpc_ptr           attr;
};

struct tarpc_zf_alternatives_alloc_out {
    struct tarpc_out_arg    common;

    tarpc_int               retval;
    tarpc_zf_althandle      alt_out;
};

struct tarpc_zf_alternatives_release_in {
    struct tarpc_in_arg common;

    tarpc_ptr           stack;
    tarpc_zf_althandle  alt;
};

typedef struct tarpc_int_retval_out tarpc_zf_alternatives_release_out;

struct tarpc_zf_alternatives_send_in {
    struct tarpc_in_arg common;

    tarpc_ptr           stack;
    tarpc_zf_althandle  alt;
};

typedef struct tarpc_int_retval_out tarpc_zf_alternatives_send_out;

struct tarpc_zf_alternatives_cancel_in {
    struct tarpc_in_arg common;

    tarpc_ptr           stack;
    tarpc_zf_althandle  alt;
};

typedef struct tarpc_int_retval_out tarpc_zf_alternatives_cancel_out;

struct tarpc_zft_alternatives_queue_in {
    struct tarpc_in_arg common;

    tarpc_ptr           ts;
    tarpc_zf_althandle  alt;
    tarpc_int           iov_cnt;
    struct tarpc_iovec  iovec<>;
    tarpc_int           flags;
};

typedef struct tarpc_int_retval_out tarpc_zft_alternatives_queue_out;

struct tarpc_zf_alternatives_free_space_in {
    struct tarpc_in_arg     common;
    tarpc_ptr               stack;
    tarpc_zf_althandle      alt;
};

struct tarpc_zf_alternatives_free_space_out {
    struct tarpc_out_arg    common;
    tarpc_uint              retval;
};

struct tarpc_zf_many_threads_alloc_free_stack_in {
    struct tarpc_in_arg common;
    tarpc_ptr           attr;
    tarpc_int           threads_num;
    tarpc_int           wait_after_alloc;
};

struct tarpc_zf_many_threads_alloc_free_stack_out {
    struct tarpc_out_arg    common;
    tarpc_int               retval;
};

enum tarpc_zf_sync_flags {
    TARPC_ZF_SYNC_FLAG_CLOCK_SET = 0x1,
    TARPC_ZF_SYNC_FLAG_CLOCK_IN_SYNC = 0x2
};

enum tarpc_zf_pkt_report_flags {
    TARPC_ZF_PKT_REPORT_CLOCK_SET = 0x1,
    TARPC_ZF_PKT_REPORT_IN_SYNC = 0x2,
    TARPC_ZF_PKT_REPORT_NO_TIMESTAMP = 0x4,
    TARPC_ZF_PKT_REPORT_DROPPED = 0x8,
    TARPC_ZF_PKT_REPORT_TCP_RETRANS = 0x2000,
    TARPC_ZF_PKT_REPORT_TCP_SYN = 0x4000,
    TARPC_ZF_PKT_REPORT_TCP_FIN = 0x8000,
    TARPC_ZF_PKT_REPORT_UNKNOWN = 0x80000000
};

struct tarpc_zf_pkt_report {
    tarpc_timespec timestamp;
    tarpc_uint     start;
    tarpc_uint     bytes;
    tarpc_uint     flags;
};

struct tarpc_zfur_pkt_get_timestamp_in {
    struct tarpc_in_arg     common;

    tarpc_ptr               urx;
    tarpc_ptr               msg_ptr;
    tarpc_int               pktind;
};

struct tarpc_zfur_pkt_get_timestamp_out {
    struct tarpc_out_arg    common;

    tarpc_timespec          ts;
    tarpc_uint              flags;
    tarpc_int               retval;
};

struct tarpc_zft_pkt_get_timestamp_in {
    struct tarpc_in_arg     common;

    tarpc_ptr               tcp_z;
    tarpc_ptr               msg_ptr;
    tarpc_int               pktind;
};

struct tarpc_zft_pkt_get_timestamp_out {
    struct tarpc_out_arg    common;

    tarpc_timespec          ts;
    tarpc_uint              flags;
    tarpc_int               retval;
};

struct tarpc_zfut_get_tx_timestamps_in {
    struct tarpc_in_arg     common;

    tarpc_ptr               utx;
    tarpc_zf_pkt_report     reports<>;
    tarpc_int               count;
};

struct tarpc_zfut_get_tx_timestamps_out {
    struct tarpc_out_arg    common;

    tarpc_zf_pkt_report     reports<>;
    tarpc_int               count;
    tarpc_int               retval;
};

struct tarpc_zft_get_tx_timestamps_in {
    struct tarpc_in_arg     common;

    tarpc_ptr               tz;
    tarpc_zf_pkt_report     reports<>;
    tarpc_int               count;
};

struct tarpc_zft_get_tx_timestamps_out {
    struct tarpc_out_arg    common;

    tarpc_zf_pkt_report     reports<>;
    tarpc_int               count;
    tarpc_int               retval;
};

struct tarpc_zft_read_all_in {
    struct tarpc_in_arg common;
    tarpc_ptr    ts;
};

struct tarpc_zft_read_all_out {
    struct tarpc_out_arg common;
    tarpc_int   retval;
    uint8_t     buf<>;
};

typedef struct tarpc_zft_read_all_in tarpc_zft_read_all_zc_in;
typedef struct tarpc_zft_read_all_out tarpc_zft_read_all_zc_out;

struct tarpc_zft_overfill_buffers_in {
    struct tarpc_in_arg common;
    tarpc_ptr           stack;
    tarpc_ptr           ts;
};

struct tarpc_zft_overfill_buffers_out {
    struct tarpc_out_arg common;
    tarpc_int   retval;
    uint8_t     buf<>;
};

struct tarpc_zf_ds {
    uint8_t   headers<>;
    tarpc_int headers_size;
    tarpc_int headers_len;
    tarpc_int mss;
    tarpc_int send_wnd;
    tarpc_int cong_wnd;
    tarpc_int delegated_wnd;

    tarpc_int tcp_seq_offset;
    tarpc_int ip_len_offset;
    tarpc_int ip_tcp_hdr_len;
    tarpc_int reserved;
};

struct tarpc_zf_delegated_send_prepare_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
    tarpc_int           max_delegated_wnd;
    tarpc_int           cong_wnd_override;
    tarpc_uint          flags;
    tarpc_zf_ds         ds;
};

struct tarpc_zf_delegated_send_prepare_out {
    struct tarpc_out_arg    common;
    tarpc_int               retval;
    tarpc_zf_ds             ds;
};

struct tarpc_zf_delegated_send_tcp_update_in {
    struct tarpc_in_arg common;
    tarpc_zf_ds         ds;
    tarpc_int           bytes;
    tarpc_int           push;
};

struct tarpc_zf_delegated_send_tcp_update_out {
    struct tarpc_out_arg    common;
    tarpc_zf_ds             ds;
};

struct tarpc_zf_delegated_send_tcp_advance_in {
    struct tarpc_in_arg common;
    tarpc_zf_ds         ds;
    tarpc_int           bytes;
};

struct tarpc_zf_delegated_send_tcp_advance_out {
    struct tarpc_out_arg    common;
    tarpc_zf_ds             ds;
};

struct tarpc_zf_delegated_send_complete_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
    struct tarpc_iovec  iov<>;
    tarpc_int           iovlen;
    tarpc_int           flags;
};

typedef struct tarpc_int_retval_out
    tarpc_zf_delegated_send_complete_out;

struct tarpc_zf_delegated_send_cancel_in {
    struct tarpc_in_arg common;
    tarpc_ptr           ts;
};

typedef struct tarpc_int_retval_out
    tarpc_zf_delegated_send_cancel_out;

program zfrpc
{
    version ver0
    {
        RPC_DEF(zf_init)
        RPC_DEF(zf_deinit)
        RPC_DEF(zf_attr_alloc)
        RPC_DEF(zf_attr_free)
        RPC_DEF(zf_attr_set_int)
        RPC_DEF(zf_stack_alloc)
        RPC_DEF(zf_stack_free)
        RPC_DEF(zf_stack_is_quiescent)
        RPC_DEF(zf_stack_has_pending_work)
        RPC_DEF(zf_stack_to_waitable)
        RPC_DEF(zf_reactor_perform)
        RPC_DEF(zfur_alloc)
        RPC_DEF(zfur_free)
        RPC_DEF(zfur_addr_bind)
        RPC_DEF(zfur_addr_unbind)
        RPC_DEF(zfur_zc_recv)
        RPC_DEF(zfur_zc_recv_done)
        RPC_DEF(zfur_pkt_get_header)
        RPC_DEF(zfur_read_zfur_msg)
        RPC_DEF(zfur_zc_recv_send)
        RPC_DEF(zfur_flooder)
        RPC_DEF(zfut_alloc)
        RPC_DEF(zfut_free)
        RPC_DEF(zfut_send)
        RPC_DEF(zfut_get_mss)
        RPC_DEF(zfut_send_single)
        RPC_DEF(zfut_flooder)
        RPC_DEF(zfut_get_header_size)
        RPC_DEF(zf_wait_for_event)
        RPC_DEF(zf_process_events)
        RPC_DEF(zf_process_events_long)
        RPC_DEF(zfur_to_waitable)
        RPC_DEF(zfut_to_waitable)
        RPC_DEF(zf_waitable_free)
        RPC_DEF(zftl_listen)
        RPC_DEF(zftl_getname)
        RPC_DEF(zftl_accept)
        RPC_DEF(zftl_to_waitable)
        RPC_DEF(zftl_free)
        RPC_DEF(zft_to_waitable)
        RPC_DEF(zft_alloc)
        RPC_DEF(zft_addr_bind)
        RPC_DEF(zft_connect)
        RPC_DEF(zft_shutdown_tx)
        RPC_DEF(zft_handle_free)
        RPC_DEF(zft_free)
        RPC_DEF(zft_state)
        RPC_DEF(zft_error)
        RPC_DEF(zft_handle_getname)
        RPC_DEF(zft_getname)
        RPC_DEF(zft_zc_recv)
        RPC_DEF(zft_zc_recv_done)
        RPC_DEF(zft_zc_recv_done_some)
        RPC_DEF(zft_recv)
        RPC_DEF(zft_read_zft_msg)
        RPC_DEF(zft_send)
        RPC_DEF(zft_send_single)
        RPC_DEF(zft_send_space)
        RPC_DEF(zft_get_mss)
        RPC_DEF(zft_get_header_size)
        RPC_DEF(zf_muxer_alloc)
        RPC_DEF(zf_muxer_free)
        RPC_DEF(zf_muxer_add)
        RPC_DEF(zf_muxer_mod)
        RPC_DEF(zf_muxer_del)
        RPC_DEF(zf_muxer_wait)
        RPC_DEF(zf_waitable_event)
        RPC_DEF(zf_waitable_fd_get)
        RPC_DEF(zf_waitable_fd_prime)
        RPC_DEF(zf_muxer_mod_rearm)
        RPC_DEF(zf_alternatives_alloc)
        RPC_DEF(zf_alternatives_release)
        RPC_DEF(zf_alternatives_send)
        RPC_DEF(zf_alternatives_cancel)
        RPC_DEF(zft_alternatives_queue)
        RPC_DEF(zf_alternatives_free_space)
        RPC_DEF(zf_many_threads_alloc_free_stack)
        RPC_DEF(zfur_pkt_get_timestamp)
        RPC_DEF(zft_pkt_get_timestamp)
        RPC_DEF(zfut_get_tx_timestamps)
        RPC_DEF(zft_get_tx_timestamps)
        RPC_DEF(zft_read_all)
        RPC_DEF(zft_read_all_zc)
        RPC_DEF(zft_overfill_buffers)
        RPC_DEF(zf_delegated_send_prepare)
        RPC_DEF(zf_delegated_send_tcp_update)
        RPC_DEF(zf_delegated_send_tcp_advance)
        RPC_DEF(zf_delegated_send_complete)
        RPC_DEF(zf_delegated_send_cancel)
    } = 1;
} = 2;
