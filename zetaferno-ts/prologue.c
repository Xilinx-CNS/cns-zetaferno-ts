/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief SFC Zetaferno API Test Suite prologue.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#ifndef DOXYGEN_TEST_SPEC

/** Logging subsystem entity name */
#define TE_TEST_NAME    "prologue"

#include "zf_test.h"
#include "tapi_network.h"
#include "lib-ts.h"
#include "lib-ts_netns.h"
#include "lib-ts_timestamps.h"

#define CONSOLE_NAME      "serial_console"
#define AGENT_FOR_CONSOLE "LogListener"


/**
 * Save sockstat data.
 *
 * @param rpcs  RPC server handle.
 */
static void
prologue_save_sockstat(rcf_rpc_server *rpcs)
{
    char name[ZFTS_FILE_NAME_LEN];

    zfts_sockstat_file_name(rpcs, ZFTS_SOCKSTAT_SUF_P, name,
                            ZFTS_FILE_NAME_LEN);

    if (zfts_sockstat_save(rpcs, name) != 0)
        WARN("Failed to get and save sockstat output");
}

int
main(int argc, char **argv)
{
    char           *te_sniff_csconf = getenv("TE_SNIFF_CSCONF");
    char           *st_no_ip6 = getenv("ST_NO_IP6");
    rcf_rpc_server *pco_iut;
    rcf_rpc_server *pco_tst;

/* Redefine as empty to avoid environment processing here */
#undef TEST_START_SPECIFIC
#define TEST_START_SPECIFIC
    TEST_START;
    tapi_env_init(&env);

    libts_init_console_loglevel();

    libts_fix_tas_path_env();
    /* Restart existing RPC servers after the PATH is updated on Test Agents */
    CHECK_RC(rcf_rpc_servers_restart_all());

    /*
     * Now we use TA-TEN connectivity mode based on VETH intefaces
     * to avoid problems with the ST-1714 issue.
     * But it's necessary to remember that this mode may cause
     * Configurator failures in epilogue on old kernels (see ST-1000).
     * It's necessary to call @ref libts_set_zf_host_addr()
     * after network namespace setup to set a proper address for
     * Zetaferno implicit binds.
     */
    CHECK_RC(libts_setup_namespace(LIBTS_NETNS_CONN_VETH));

    tapi_network_setup(st_no_ip6 == NULL || *st_no_ip6 == '\0');
    libts_set_zf_host_addr();

    rc = tapi_cfg_env_local_to_agent();
    if (rc != 0)
    {
        TEST_FAIL("tapi_cfg_env_local_to_agent() failed: %r", rc);
    }

    if (te_sniff_csconf != NULL)
        CHECK_RC(cfg_process_history(te_sniff_csconf, NULL));

    if ((rc = libts_copy_socklibs()) != 0)
        TEST_FAIL("Processing of /local:*/socklib: failed: %r", rc);

    CFG_WAIT_CHANGES;
    CHECK_RC(rc = cfg_synchronize("/:", TRUE));

    TEST_START_ENV;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);

    CHECK_RC(tapi_neight_flush_ta(pco_tst));

    /*
     * Sometimes after rebooting the hos ipmi/conserver stops showing
     * anything. This problem is solved if you send "enter" to console.
     * For more information see Bug 9831.
     */
    libts_send_enter2serial_console(AGENT_FOR_CONSOLE, "rpcs_serial",
                                    CONSOLE_NAME);

    libts_timestamps_configure_sfptpd();

    CHECK_RC(rc = cfg_tree_print(NULL, TE_LL_RING, "/:"));

    prologue_save_sockstat(pco_iut);

    TEST_SUCCESS;
cleanup:

    TEST_END;
}

#endif /* !DOXYGEN_TEST_SPEC */
