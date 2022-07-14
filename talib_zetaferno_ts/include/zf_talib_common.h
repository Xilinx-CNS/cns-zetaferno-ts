/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Common definitions for agent and test API libraries.
 *
 * Common definitions for agent and test API libraries.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#ifndef ___ZF_TALIB_COMMON_H__
#define ___ZF_TALIB_COMMON_H__

#include "te_rpc_types.h"

typedef enum rpc_zfut_flags {
    RPC_ZFUT_FLAG_DONT_FRAGMENT = (1 << 0),
} rpc_zfut_flags;

#define ZFUT_FLAGS_MAPPING_LIST \
    RPC_BIT_MAP_ENTRY(ZFUT_FLAG_DONT_FRAGMENT)

/**
 * zfut_flags_rpc2str().
 */
RPCBITMAP2STR(zfut_flags, ZFUT_FLAGS_MAPPING_LIST)

/**
 * Copy field value from one structure to another one.
 *
 * @param _dst        Destination structure.
 * @param _src        Source structure.
 * @param _field      Field name.
 */
#define ZFTS_COPY_FIELD(_dst, _src, _field) \
    (_dst)._field = (_src)._field

/**
 * Copy fields from one zf_ds or tarpc_zf_ds structure to another one.
 * (except for headers field which has different types in zf_ds
 * and tarpc_zf_ds).
 *
 * @param _dst      Destination structure.
 * @param _src      Source structure.
 */
#define ZFTS_COPY_ZF_DS_FIELDS(_dst, _src) \
    do {                                                    \
        ZFTS_COPY_FIELD(_dst, _src, headers_size);          \
        ZFTS_COPY_FIELD(_dst, _src, headers_len);           \
        ZFTS_COPY_FIELD(_dst, _src, mss);                   \
        ZFTS_COPY_FIELD(_dst, _src, send_wnd);              \
        ZFTS_COPY_FIELD(_dst, _src, cong_wnd);              \
        ZFTS_COPY_FIELD(_dst, _src, delegated_wnd);         \
        ZFTS_COPY_FIELD(_dst, _src, tcp_seq_offset);        \
        ZFTS_COPY_FIELD(_dst, _src, ip_len_offset);         \
        ZFTS_COPY_FIELD(_dst, _src, ip_tcp_hdr_len);        \
        ZFTS_COPY_FIELD(_dst, _src, reserved);              \
    } while (0)

#endif /* !___ZF_TALIB_COMMON_H__ */
