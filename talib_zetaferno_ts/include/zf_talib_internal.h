/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Internal definitions for agent libraries.
 *
 * Internal definitions for agent libraries.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef ___ZF_TALIB_INTERNAL_H__
#define ___ZF_TALIB_INTERNAL_H__

/**
 * Address length to use in RPC call implementation.
 *
 * @param addr    Address field name in RPC input parameters structure.
 */
#define ZFTS_ADDR_LEN(addr_) \
    (in->fwd_ ## addr_ ## len ? in->addr_ ## len : addr_ ## len)

#endif /* !___ZF_TALIB_INTERNAL_H__ */
