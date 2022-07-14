/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Zetaferno alternatives tests
 *
 * $Id$
 */

/**
 * @page zf_alt-invalid_alt_attr_bufs Try invalid combinations of alt_count and alt_buf_size.
 *
 * @objective Try to set invalid combinations of attributes @c alt_count
 *            and @c alt_buf_size, ZF stack allocation should gracefully
 *            fail.
 *
 * @param huge_alt_buf  Set huge @c alt_buf_size (> 122880) if @c TRUE,
 *                      otherwise set a value which is less then required
 *                      for the requested alternatives number.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "zf_alt/invalid_alt_attr_bufs"

#include "zf_test.h"
#include "rpc_zf.h"

/** Maximum value of alt_count attribute to be tested. */
#define MAX_ALT_COUNT 18

/** Maximum value of alt_count attribute which is supported by NIC
 * (Medford). */
#define MAX_ALT_SUPP 15

/** Value of alt_buf_size attribute if @p huge_alt_buf is @c TRUE. */
#define HUGE_ALT_BUF 122881

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    int       alt_count;
    int       alt_buf_size;
    te_errno  exp_errno;
    te_bool   failed = FALSE;

    te_bool   huge_alt_buf = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_BOOL_PARAM(huge_alt_buf);

    /*- Allocate ZF attributes. */
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);

    /*- In a loop iterate alt_count=[-1; MAX_ALT_COUNT]: */
    for (alt_count = -1; alt_count <= MAX_ALT_COUNT; alt_count++)
    {
        /*-- Set attribute @c alt_count value according to
         * the loop iteration. */
        rpc_zf_attr_set_int(pco_iut, attr, "alt_count", alt_count);

        if (huge_alt_buf)
            alt_buf_size = HUGE_ALT_BUF;
        else
            alt_buf_size = (alt_count * 1024 - 1);

        /*-- If @p huge_alt_buf is @c TRUE, set @b alt_buf_size attribute
         * to @c HUGE_ALT_BUF; else set it to alt_count * 1024 - 1. */
        rpc_zf_attr_set_int(pco_iut, attr, "alt_buf_size", alt_buf_size);

        if (alt_count < 0 || alt_buf_size < 0)
            exp_errno = RPC_EINVAL;
        else
            exp_errno = RPC_ENOSPC;

        /*-- Try to allocate ZF stack, the call should gracefully fail. */
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_stack_alloc(pco_iut, attr, &stack);
        if (rc >= 0)
        {
            rpc_zf_stack_free(pco_iut, stack);

            /* It is treated as correct values combination now, see
             * bug 66508#c4 for details.
             * alt_count=0 is the correct value, which means ZF alts are
             * disabled. */
            if ((!huge_alt_buf && alt_count > 0 && alt_buf_size > 0 &&
                alt_count <= MAX_ALT_SUPP) || alt_count == 0)
                continue;

            ERROR_VERDICT("Stack allocation succeeded with "
                          "attributes alt_count=%d, alt_buf_size=%d",
                          alt_count, alt_buf_size);
            failed = TRUE;
        }
        /* See SF bug 73896 for details. */
        else if (RPC_ERRNO(pco_iut) != exp_errno &&
                 RPC_ERRNO(pco_iut) != RPC_EBUSY)
        {
            ERROR_VERDICT("Stack allocation failed with unexpected errno "
                          "%r for attributes alt_count=%d, alt_buf_size=%d",
                          RPC_ERRNO(pco_iut), alt_count, alt_buf_size);
            failed = TRUE;
        }
    }

    if (failed)
        TEST_STOP;
    TEST_SUCCESS;

cleanup:

    TEST_END;
}

