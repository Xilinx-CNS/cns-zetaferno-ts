/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Auxiliary test API for alternative queues.
 *
 * Definition of auxiliary test API to work with ZF alternative queues.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef ___ZFTS_ALT_H__
#define ___ZFTS_ALT_H__

/** Minimal sensible value of @b alt_count attribute. */
#define ZFTS_ALT_COUNT_MIN 10

/** Maximal sensible value of @b alt_count attribute. */
#define ZFTS_ALT_COUNT_MAX 32

/** Minimal sensible value of @b alt_buf_size attribute. */
#define ZFTS_ALT_BUF_SIZE_MIN 40000

/** Maximum sensible value of @b alt_buf_size attribute. */
#define ZFTS_ALT_BUF_SIZE_MAX 120000

/** Reserved amount of NIC-side buffer space for an alternative in bytes. */
#define ZFTS_ALT_BUF_RESERVED 1024

/**
 * Maximum difference between initial value
 * reported by zf_alternatives_free_space()
 * and value reported by this function after filling
 * alternative and then sending or canceling all the data.
 * See bug 66507.
 *
 * The value is one "word" (32 bytes) less than one buffer (1024 bytes).
 */
#define ZFTS_FREE_ALT_SPACE_MAX_DIFF 992

#endif /* !___ZFTS_ALT_H__ */
