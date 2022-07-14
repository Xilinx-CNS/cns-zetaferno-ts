/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Delegated Sends tests
 *
 * Declaration of common API for Delegated Sends tests.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#ifndef __TS_ZF_DS_TEST_H__
#define __TS_ZF_DS_TEST_H__

#include "zf_test.h"
#include "te_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process format string and arguments for verdicts prefix.
 *
 * @param _fmt        Format argument.
 * @param _str        TE string to which to append
 *                    formatted string.
 */
#define ZFTS_PROCESS_PREFIX_FMT(_fmt, _str) \
    do {                                              \
        va_list _ap;                                  \
                                                      \
        va_start(_ap, _fmt);                          \
        te_string_append_va(&_str, _fmt, _ap);        \
        va_end(_ap);                                  \
    } while (0)

/**
 * Check what @b zf_delegated_send_prepare() returns.
 *
 * @param exp_rc      Expected return value.
 * @param len         Expected data length reserved for delegated sends.
 * @param rc          Obtained return value.
 * @param ds          Pointer to zf_ds structure.
 * @param fmt         Format string for verdict prefix.
 * @param ...         Format arguments.
 */
static inline void
zfts_check_ds_prepare(rpc_zf_delegated_send_rc exp_rc, int len,
                      rpc_zf_delegated_send_rc rc, const struct zf_ds *ds,
                      const char *fmt, ...)
{
    te_string str = TE_STRING_INIT_STATIC(1024);

    ZFTS_PROCESS_PREFIX_FMT(fmt, str);

    if (rc != exp_rc)
    {
        TEST_VERDICT("%szf_delegated_send_prepare() returned %s "
                     "instead of %s", str.ptr,
                     zf_delegated_send_rc_rpc2str(rc),
                     zf_delegated_send_rc_rpc2str(exp_rc));
    }
    else if (rc == ZF_DELEGATED_SEND_RC_OK && ds->delegated_wnd != len)
    {
        ERROR("%d bytes were requested but %d were reserved",
              len, ds->delegated_wnd);
        TEST_VERDICT("%szf_delegated_send_prepare() returned "
                     "unexpected delegated_wnd value", str.ptr);
    }
}

/**
 * Call @b zf_delegated_send_complete() for a single buffer.
 *
 * @param rpcs          RPC server handle.
 * @param ts            TCP zocket RPC pointer.
 * @param buf           Buffer with data to be completed.
 * @param len           Buffer length.
 * @param flags         Flags for @b zf_delegated_send_complete().
 *
 * @return Return value of @b zf_delegated_send_complete().
 */
static inline int
zfts_ds_complete_buf(rcf_rpc_server *rpcs, rpc_zft_p ts, char *buf,
                     int len, int flags)
{
    rpc_iovec iov;

    memset(&iov, 0, sizeof(iov));
    iov.iov_base = buf;
    iov.iov_len = iov.iov_rlen = len;

    return rpc_zf_delegated_send_complete(rpcs, ts, &iov, 1, flags);
}

/**
 * Check what @b zf_delegated_send_complete() returns.
 *
 * @param rpcs        RPC server on which the function was called.
 * @param rc          Return value.
 * @param len         Expected data length which should have been
 *                    completed.
 * @param fmt         Format string for verdict prefix.
 * @param ...         Format arguments.
 */
static inline void
zfts_check_ds_complete(rcf_rpc_server *rpcs, int rc, int len,
                       const char *fmt, ...)
{
    te_string str = TE_STRING_INIT_STATIC(1024);

    ZFTS_PROCESS_PREFIX_FMT(fmt, str);

    if (rc < 0)
    {
        TEST_VERDICT("%szf_delegated_send_complete() failed with error %r",
                     str.ptr, RPC_ERRNO(rpcs));
    }
    else if (rc != len)
    {
        TEST_VERDICT("%szf_delegated_send_complete() returned value "
                     "which is %s than expected", str.ptr,
                     (rc > len ? "greater" : "smaller"));
    }
}

/**
 * Send data over raw socket when performing delegated sends. Call
 * @b zf_delegated_send_tcp_update() before sending and
 * @b zf_delegated_send_tcp_advance() after it.
 *
 * @note This function can be used only for sending a single packet
 *       at a time.
 *
 * @param rpcs          RPC server handle.
 * @param if_index      Index of the interface over which to send.
 * @param raw_s         RAW socket FD.
 * @param ds            Pointer to zf_ds structure.
 * @param buf           Data to send.
 * @param len           Length of the data.
 * @param silent_pass   If @c TRUE, do not print logs about successful
 *                      RPC calls.
 */
extern void zfts_ds_raw_send(rcf_rpc_server *rpcs, int if_index, int raw_s,
                             struct zf_ds *ds, char *buf, size_t len,
                             te_bool silent_pass);

/**
 * Send buffers from array of rpc_iovec structures over RAW socket.
 *
 * @param rpcs          RPC server handle.
 * @param if_index      Index of the interface over which to send.
 * @param raw_s         RAW socket FD.
 * @param ds            Pointer to zf_ds structure.
 * @param iov           Array of rpc_iovec structures.
 * @param iov_num       Number of elements in the array.
 * @param silent_pass   If @c TRUE, do not print logs about successful
 *                      RPC calls.
 */
extern void zfts_ds_raw_send_iov(rcf_rpc_server *rpcs, int if_index,
                                 int raw_s, struct zf_ds *ds,
                                 rpc_iovec *iov, int iov_num,
                                 te_bool silent_pass);

/**
 * Perform delegated sends of the specified data via the RAW socket,
 * possibly splitting it into multiple packets. After that call
 * @b zf_delegated_send_complete().
 *
 * @param rpcs          RPC server handle.
 * @param if_index      Index of the interface over which to send.
 * @param raw_s         RAW socket FD (if negative, data is not actually
 *                      sent but @b zf_delegated_send_complete() is still
 *                      called).
 * @param zft           TCP zocket on which to complete delegated sends.
 * @param ds            Pointer to zf_ds structure.
 * @param buf           Data to send.
 * @param len           Length of the data.
 * @param min_pkt_len   Minimum length of payload in the single packet.
 * @param max_pkt_len   Maximum length of payload in the single packet.
 */
extern void zfts_ds_raw_send_complete(rcf_rpc_server *rpcs, int if_index,
                                      int raw_s, rpc_zft_p zft,
                                      struct zf_ds *ds, char *buf,
                                      size_t len, int min_pkt_len,
                                      int max_pkt_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __TS_ZF_DS_TEST_H */
