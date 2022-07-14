/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief SFC Zetaferno API Test Suite epilogue.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#ifndef DOXYGEN_TEST_SPEC

/** Logging subsystem entity name */
#define TE_TEST_NAME    "epilogue"

#include "zf_test.h"
#include "lib-ts_netns.h"

/**
 * Compare sockstat stats before and after tests execution.
 *
 * @param rpcs  RPC server handle.
 */
static void
epilogue_check_sockstat(rcf_rpc_server *rpcs)
{
    char name_p[ZFTS_FILE_NAME_LEN];
    char name_e[ZFTS_FILE_NAME_LEN];

    zfts_sockstat_file_name(rpcs, ZFTS_SOCKSTAT_SUF_E, name_e,
                            ZFTS_FILE_NAME_LEN);

    if (zfts_sockstat_save(rpcs, name_e) != 0)
    {
        ERROR("Failed to get and save sockstat output");
        return;
    }

    zfts_sockstat_file_name(rpcs, ZFTS_SOCKSTAT_SUF_P, name_p,
                            ZFTS_FILE_NAME_LEN);

    zfts_sockstat_cmp(name_p, name_e);
}

int
main(int argc, char **argv)
{
    rcf_rpc_server *pco_iut;

    TEST_START;
    TEST_GET_PCO(pco_iut);

    epilogue_check_sockstat(pco_iut);

    TEST_SUCCESS;
cleanup:
    CLEANUP_CHECK_RC(libts_cleanup_netns());

    TEST_END;
}

#endif /* !DOXYGEN_TEST_SPEC */

