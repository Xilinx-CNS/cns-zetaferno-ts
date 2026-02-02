/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions definition
 *
 * Implementation of auxiliary functions used by TAPI for
 * ZF API remote calls.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru
 *
 * $Id$
 */

#include "te_config.h"

#if HAVE_STRING_H
#include <string.h>
#endif

#include "tapi_rpc_internal.h"
#include "tapi_rpc_unistd.h"
#include "te_rpc_sys_socket.h"
#include "te_rpc_sys_epoll.h"
#include "te_string.h"
#include "zf_test.h"

#undef TE_LGR_USER
#define TE_LGR_USER "ZF TAPI RPC INTERNAL"

/* See description in rpc_zf_internal.h */
te_string log_strbuf = TE_STRING_INIT;

/* See description in rpc_zf_internal.h */
void
rpc_iovec2str(rpc_iovec *iov, size_t iovcnt, te_string *str)
{
    size_t i;

    te_string_append(str, "{");
    for (i = 0; i < iovcnt; i++)
        te_string_append(str,"%s{%"TE_PRINTF_SIZE_T"u, "
                         "%p[%"TE_PRINTF_SIZE_T"u]}", (i == 0) ? "" : ", ",
                         iov[i].iov_len, iov[i].iov_base,
                         iov[i].iov_rlen);
    te_string_append(str, "}");
}

/* See description in rpc_zf_internal.h */
void
rpc_iovec2tarpc_iovec(rpc_iovec *iov, tarpc_iovec *tarpc_iov, size_t iovcnt)
{
    size_t i;

    for (i = 0; i < iovcnt; i++)
    {
        tarpc_iov[i].iov_len = iov[i].iov_len;
        tarpc_iov[i].iov_base.iov_base_len = iov[i].iov_rlen;
        tarpc_iov[i].iov_base.iov_base_val = iov[i].iov_base;
    }
}

/* See description in rpc_zf_internal.h */
void
tarpc_iovec2rpc_iovec(tarpc_iovec *tarpc_iov,
                      rpc_iovec *iov, int iovcnt)
{
    int i;

    for (i = 0; i < iovcnt; i++)
    {
        if (tarpc_iov[i].iov_base.iov_base_len > iov[i].iov_len)
            TEST_FAIL("One of returned iov buffers is larger "
                      "than its buffer size");

        memcpy(iov[i].iov_base, tarpc_iov[i].iov_base.iov_base_val,
               tarpc_iov[i].iov_base.iov_base_len);
        iov[i].iov_len = tarpc_iov[i].iov_base.iov_base_len;
    }
}

/* See description in rpc_zf_internal.h */
void
zf_pkt_report_rpc2str(tarpc_zf_pkt_report *report, te_string *str)
{
    te_string_append(str, "{ timestamp=" TE_PRINTF_TS_FMT ", "
                     "start=%u, bytes=%u, flags=0x%x (%s) }",
                     TE_PRINTF_TS_VAL(report->timestamp),
                     report->start, report->bytes,
                     report->flags,
                     zf_pkt_report_flags_rpc2str(report->flags));
}

/* See description in rpc_zf_internal.h */
void
zf_pkt_reports_rpc2str(tarpc_zf_pkt_report *reports, int count,
                       te_string *str)
{
    int i;

    te_string_append(str, "[ ");

    for (i = 0; i < count; i++)
    {
        zf_pkt_report_rpc2str(&reports[i], str);

        te_string_append(str, (i < count - 1 ? ", " : " "));
    }

    te_string_append(str, "]");
}
