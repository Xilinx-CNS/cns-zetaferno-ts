/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */

/** @file
 * @brief RPC pointer namespaces definition
 *
 * Common definitions for agent and test API libraries with RPC pointer
 * namespaces.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#ifndef ___ZF_TALIB_NAMESPACE_H__
#define ___ZF_TALIB_NAMESPACE_H__

#include "te_rpc_types.h"

/**
 * @name RPC pointer namespaces names.
 * @{
 */
#define RPC_TYPE_NS_ZF_ATTR         "zf_attr"
#define RPC_TYPE_NS_ZF_STACK        "zf_stack"
#define RPC_TYPE_NS_ZFUR            "zfur"
#define RPC_TYPE_NS_ZFUR_MSG        "zfur_msg"
#define RPC_TYPE_NS_ZFUT            "zfut"
#define RPC_TYPE_NS_ZF_WAITABLE     "zf_waitable"
#define RPC_TYPE_NS_ZFTL            "zftl"
#define RPC_TYPE_NS_ZFT             "zft"
#define RPC_TYPE_NS_ZFT_HANDLE      "zft_handle"
#define RPC_TYPE_NS_ZFT_MSG         "zft_msg"
#define RPC_TYPE_NS_ZF_MUXER_SET    "zf_muxer_set"

/*@}*/

/**
 * @name RPC pointer types definition.
 * @{
 */
typedef rpc_ptr  rpc_zf_attr_p;
typedef rpc_ptr  rpc_zf_stack_p;
typedef rpc_ptr  rpc_zfur_p;
typedef rpc_ptr  rpc_zfur_msg_p;
typedef rpc_ptr  rpc_zfut_p;
typedef rpc_ptr  rpc_zf_waitable_p;
typedef rpc_ptr  rpc_zftl_p;
typedef rpc_ptr  rpc_zft_p;
typedef rpc_ptr  rpc_zft_handle_p;
typedef rpc_ptr  rpc_zft_msg_p;
typedef rpc_ptr  rpc_zf_muxer_set_p;
/*@}*/

#endif /* !___ZF_TALIB_NAMESPACE_H__ */
