/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief SFC Zetaferno Alternative Queues API Test Package prologue.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#ifndef DOXYGEN_TEST_SPEC

/** Logging subsystem entity name */
#define TE_TEST_NAME    "prologue"

#include "zf_test.h"

/** Length of ZF_ATTR environment variable value. */
#define ZF_ATTR_LEN 1024

/** Size of buffer len step */
#define ZFTS_ALT_BUF_LEN_STEP 10000

/**
 * Calculate available number of alternatives by the method of gradual increase
 * of number.
 *
 * @param rpcs         RPC server
 * @param zf_attr_val  ZF attributes
 *
 * @return Number of maximal available alternatives
 */
static int
zf_alternatives_get_avail_alts(rcf_rpc_server *rpcs, const char *zf_attr_val)
{
    char  zf_attr_new_val[ZF_ATTR_LEN] = "";

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    int alts_avail;
    int rc;

    for (alts_avail = 1; alts_avail <= ZFTS_ALT_COUNT_MAX; alts_avail++)
    {
        rc = snprintf(zf_attr_new_val, ZF_ATTR_LEN,
                      "%s;alt_count=%d", zf_attr_val, alts_avail);

        if (rc < 0 || rc >= ZF_ATTR_LEN)
            TEST_FAIL("Failed to extend ZF_ATTR variable value");

        CHECK_RC(tapi_sh_env_set(rpcs, "ZF_ATTR", zf_attr_new_val,
                                 TRUE, TRUE));

        rpc_zf_init(rpcs);
        rpc_zf_attr_alloc(rpcs, &attr);
        RPC_AWAIT_IUT_ERROR(rpcs);
        rc = rpc_zf_stack_alloc(rpcs, attr, &stack);
        if (rc != 0)
        {
            if (RPC_ERRNO(rpcs) == RPC_EBUSY)
                return alts_avail - 1;
            else
                break;
        }

        zfts_destroy_stack(rpcs, attr, stack);
    }

    return -1;
}

/**
 * Calculate available buffer size by the method of gradual increase
 * of size to a possible maximum with a certain number of alternatives.
 *
 * @param rpcs         RPC server
 * @param zf_attr_val  ZF attributes
 * @param alt_num      Number of alternatives
 *
 * @return Maximal available buffer
 */
static int
zf_alternatives_get_avail_buff(rcf_rpc_server *rpcs, const char *zf_attr_val,
                               int alts_num)
{
    char  zf_attr_new_val[ZF_ATTR_LEN] = "";

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    int buf_size_avail;
    int rc;

    for (buf_size_avail = ZFTS_ALT_BUF_SIZE_MIN;
         buf_size_avail <= ZFTS_ALT_BUF_SIZE_MAX;
         buf_size_avail += ZFTS_ALT_BUF_LEN_STEP)
    {
        rc = snprintf(zf_attr_new_val, ZF_ATTR_LEN,
                      "%s;alt_count=%d;alt_buf_size=%d",
                       zf_attr_val, alts_num, buf_size_avail);

        if (rc < 0 || rc >= ZF_ATTR_LEN)
            TEST_FAIL("Failed to extend ZF_ATTR variable value");

        CHECK_RC(tapi_sh_env_set(rpcs, "ZF_ATTR", zf_attr_new_val,
                                 TRUE, TRUE));

        rpc_zf_init(rpcs);
        rpc_zf_attr_alloc(rpcs, &attr);
        RPC_AWAIT_IUT_ERROR(rpcs);
        rc = rpc_zf_stack_alloc(rpcs, attr, &stack);
        if (rc != 0)
        {
            if (RPC_ERRNO(rpcs) == RPC_EBUSY)
                return buf_size_avail - ZFTS_ALT_BUF_LEN_STEP;
            else
                break;
        }

        zfts_destroy_stack(rpcs, attr, stack);
    }

    return -1;
}

int
main(int argc, char **argv)
{
    rcf_rpc_server        *pco_iut = NULL;

    char  zf_attr_new_val[ZF_ATTR_LEN] = "";
    char *zf_attr_cur_val = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    int alts_avail;
    int alts_buf_avail;

    TEST_START;
    TEST_GET_PCO(pco_iut);

    CHECK_RC(tapi_sh_env_get(pco_iut, "ZF_ATTR", &zf_attr_cur_val));

    alts_avail = zf_alternatives_get_avail_alts(pco_iut, zf_attr_cur_val);
    alts_buf_avail = zf_alternatives_get_avail_buff(pco_iut, zf_attr_cur_val,
                                                    alts_avail);

    rc = snprintf(zf_attr_new_val, ZF_ATTR_LEN,
                  "%s;alt_count=%d;alt_buf_size=%d", zf_attr_cur_val,
                  alts_avail, alts_buf_avail);
    free(zf_attr_cur_val);
    if (rc < 0 || rc >= ZF_ATTR_LEN)
        TEST_FAIL("Failed to extend ZF_ATTR variable value");

    CHECK_RC(tapi_sh_env_set_int(pco_iut, "ZFTS_ALT_COUNT_DEF",
                                 alts_avail, TRUE, FALSE));

    CHECK_RC(tapi_sh_env_set_int(pco_iut, "ZFTS_ALT_BUF_SIZE_DEF",
                                 alts_buf_avail, TRUE, FALSE));


    CHECK_RC(tapi_sh_env_set(pco_iut, "ZF_ATTR", zf_attr_new_val,
                             TRUE, TRUE));

    /* Check that stack can be allocated with such attributes. */
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

#endif /* !DOXYGEN_TEST_SPEC */
