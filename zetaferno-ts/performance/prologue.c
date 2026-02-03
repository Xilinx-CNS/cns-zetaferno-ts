/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief SFC Zetaferno Performance Tests Package prologue.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME    "performance/prologue"

#include "zf_test.h"

/**
 * Copy performance measurement application to TA host.
 *
 * @param src_dir       Source directory.
 * @param dst_dir       Destination directory on remote host.
 * @param dst_ta        Destination TA name.
 * @param app_name      Application file name.
 */
static void
copy_app(const char *src_dir, const char *dst_dir, const char *dst_ta,
         const char *app_name)
{
    te_string src_path = TE_STRING_INIT_STATIC(PATH_MAX);
    te_string dst_path = TE_STRING_INIT_STATIC(PATH_MAX);

    te_string_append(&src_path, "%s/%s", src_dir, app_name);
    te_string_append(&dst_path, "%s/%s", dst_dir, app_name);

    RING("Copy %s to %s:%s", src_path.ptr, dst_ta, dst_path.ptr);
    CHECK_RC(rcf_ta_put_file(dst_ta, 0, src_path.ptr, dst_path.ptr));
}

int
main(int argc, char **argv)
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;

    const char *apps_dir = NULL;
    char *iut_agt_dir = NULL;
    char *tst_agt_dir = NULL;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);

    apps_dir = getenv("SFC_ZETAFERNO_APPS");
    if (te_str_is_null_or_empty(apps_dir))
        TEST_FAIL("SFC_ZETAFERNO_APPS is not set");

    CHECK_RC(cfg_get_instance_fmt(NULL, &iut_agt_dir, "/agent:%s/dir:",
                                  pco_iut->ta));
    CHECK_RC(cfg_get_instance_fmt(NULL, &tst_agt_dir, "/agent:%s/dir:",
                                  pco_tst->ta));

    copy_app(apps_dir, iut_agt_dir, pco_iut->ta, "zfudppingpong");
    copy_app(apps_dir, iut_agt_dir, pco_iut->ta, "zftcppingpong");
    copy_app(apps_dir, iut_agt_dir, pco_iut->ta, "zfaltpingpong");
    copy_app(apps_dir, tst_agt_dir, pco_tst->ta, "zfudppingpong");
    copy_app(apps_dir, tst_agt_dir, pco_tst->ta, "zftcppingpong");
    copy_app(apps_dir, tst_agt_dir, pco_tst->ta, "zfaltpingpong");

    TEST_SUCCESS;

cleanup:

    free(iut_agt_dir);
    free(tst_agt_dir);
    TEST_END;
}
