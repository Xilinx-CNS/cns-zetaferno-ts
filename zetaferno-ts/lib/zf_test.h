/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Genreic Test Suite API
 *
 * Macros to be used in tests. The header must be included from test
 * sources only. It is allowed to use the macros only from @b main()
 * function of the test.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#ifndef __TS_ZF_TEST_H__
#define __TS_ZF_TEST_H__

#include "te_config.h"


#ifndef TEST_START_VARS
/**
 * Test suite specific variables of the test @b main() function.
 */
#define TEST_START_VARS TEST_START_ENV_VARS
#endif

#ifndef TEST_START_SPECIFIC
/**
 * Test suite specific the first actions of the test.
 */
#define TEST_START_SPECIFIC TEST_START_ENV
#endif

#ifndef TEST_END_SPECIFIC
/**
 * Test suite specific part of the last action of the test @b main()
 * function.
 */
#define TEST_END_SPECIFIC TEST_END_ENV
#endif

#include "tapi_test.h"
#include "zetaferno_ts.h"
#include "zfts_zfur.h"
#include "zfts_zfut.h"
#include "zfts_tcp.h"
#include "zfts_muxer.h"
#include "zfts_alt.h"

#endif /* !__TS_ZF_TEST_H__ */
