/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/**
 * @brief Delegated Sends RPC routines implementation
 *
 * Implementation of RPC routines for Zetaferno Direct API Delegated Sends.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_LGR_USER     "SFC Zetaferno RPC DS"

#include "te_config.h"
#include "config.h"

#include "logger_ta_lock.h"
#include "rpc_server.h"

#include "zf_talib_namespace.h"
#include "zf_talib_common.h"
#include "zf_talib_internal.h"
#include "te_alloc.h"
#include "zf_rpc.h"
#include "te_tools.h"

#include <zf/zf.h>

/**
 * Move memory allocated for headers from "in" to "out" argument.
 * Set fields of zf_ds structure to values from "in".
 *
 * @param _ds       zf_ds structure to fill.
 * @param _out      "out" parameter of RPC call.
 * @param _in       "in" parameter of RPC call.
 */
#define PREPARE_DS(_ds, _out, _in) \
    do {                                                                  \
        (_out)->ds.headers.headers_val = (_in)->ds.headers.headers_val;   \
        (_out)->ds.headers.headers_len = (_in)->ds.headers.headers_len;   \
        (_in)->ds.headers.headers_val = NULL;                             \
        (_in)->ds.headers.headers_len = 0;                                \
                                                                          \
        memset(&_ds, 0, sizeof(_ds));                                     \
        ZFTS_COPY_ZF_DS_FIELDS(_ds, (_in)->ds);                           \
        (_ds).headers = (_out)->ds.headers.headers_val;                   \
    } while (0)

TARPC_FUNC(zf_delegated_send_prepare, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    struct zft *ts = NULL;
    struct zf_ds ds;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    PREPARE_DS(ds, out, in);

    MAKE_CALL(out->retval = func_ptr(ts, in->max_delegated_wnd,
                                     in->cong_wnd_override, in->flags,
                                     &ds));
    ZFTS_COPY_ZF_DS_FIELDS(out->ds, ds);
})

TARPC_FUNC_STATIC(zf_delegated_send_tcp_update, {},
{
    struct zf_ds ds;

    PREPARE_DS(ds, out, in);
    MAKE_CALL(func_ptr(&ds, in->bytes, in->push));
    ZFTS_COPY_ZF_DS_FIELDS(out->ds, ds);
})

TARPC_FUNC_STATIC(zf_delegated_send_tcp_advance, {},
{
    struct zf_ds ds;

    PREPARE_DS(ds, out, in);
    MAKE_CALL(func_ptr(&ds, in->bytes));
    ZFTS_COPY_ZF_DS_FIELDS(out->ds, ds);
})

TARPC_FUNC(zf_delegated_send_complete, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    struct zft *ts = NULL;
    struct iovec *iov;
    int i;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    iov = TE_ALLOC(in->iov.iov_len * sizeof(*iov));
    if (iov == NULL)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_ENOMEM),
                         "Not enough memory for iovec array");
        out->retval = -1;
        return;
    }

    for (i = 0; i < in->iov.iov_len; i++)
    {
        INIT_CHECKED_ARG(in->iov.iov_val[i].iov_base.iov_base_val,
                         in->iov.iov_val[i].iov_base.iov_base_len, 0);
        iov[i].iov_base = in->iov.iov_val[i].iov_base.iov_base_val;
        iov[i].iov_len = in->iov.iov_val[i].iov_len;
    }
    INIT_CHECKED_ARG((char *)iov, sizeof(*iov) * in->iov.iov_len, 0);

    MAKE_CALL(out->retval = func(ts, iov, in->iovlen, in->flags));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    free(iov);
})

TARPC_FUNC(zf_delegated_send_cancel, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    struct zft *ts = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = func_ptr(ts));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})
