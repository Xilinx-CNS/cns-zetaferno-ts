/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief SFC Zetaferno Timestamps API Test Package epilogue.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#ifndef DOXYGEN_TEST_SPEC

/** Logging subsystem entity name */
#define TE_TEST_NAME    "timestamps/epilogue"

#include "zf_test.h"

#include "lib-ts_timestamps.h"

int
main(int argc, char **argv)
{
    rcf_rpc_server *pco_iut = NULL;

    TEST_START;
    TEST_GET_PCO(pco_iut);

    libts_timestamps_disable_sfptpd(pco_iut);

    TEST_SUCCESS;

cleanup:

    TEST_END;
}

#endif /* !DOXYGEN_TEST_SPEC */
