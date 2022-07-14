/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/**
 * @brief ZF Alternatives RPC routines implementation
 *
 * Implementation of RPC routines for Zetaferno Direct API Alternatives.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "SFC Zetaferno RPC Alternatives"
#include "te_config.h"
#include "config.h"

#include "logger_ta_lock.h"
#include "rpc_server.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "te_sockaddr.h"
#include "zf_talib_namespace.h"
#include "te_alloc.h"
#include "zf_rpc.h"
#include "te_rpc_sys_socket.h"

#include <zf/zf.h>
#include <zf/zf_alts.h>
#include <zf/zf_tcp.h>

TARPC_FUNC(zf_alternatives_alloc, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_attr = RPC_PTR_ID_NS_INVALID;

    struct zf_stack     *stack = NULL;
    struct zf_attr      *attr = NULL;

    zf_althandle alt_out;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_attr,
                                           RPC_TYPE_NS_ZF_ATTR,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(attr, in->attr, ns_attr,);

    MAKE_CALL(out->retval = func_ptr(stack, attr, &alt_out));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    out->alt_out = alt_out;
})

TARPC_FUNC(zf_alternatives_release, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;

    struct zf_stack     *stack = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(out->retval = func_ptr(stack, in->alt));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zf_alternatives_send, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;

    struct zf_stack     *stack = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(out->retval = func_ptr(stack, in->alt));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zf_alternatives_cancel, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;

    struct zf_stack     *stack = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(out->retval = func_ptr(stack, in->alt));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zft_alternatives_queue, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft       *ts = NULL;

    struct iovec   *iov;
    size_t          iov_size = sizeof(*iov) * in->iovec.iovec_len;
    unsigned int    i;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    iov = TE_ALLOC(iov_size);

    if (iov == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return;
    }

    for (i = 0; i < in->iovec.iovec_len; i++)
    {
        INIT_CHECKED_ARG(
                  in->iovec.iovec_val[i].iov_base.iov_base_val,
                  in->iovec.iovec_val[i].iov_base.iov_base_len,
                  in->iovec.iovec_val[i].iov_len);
        iov[i].iov_base = in->iovec.iovec_val[i].iov_base.iov_base_val;
        iov[i].iov_len = in->iovec.iovec_val[i].iov_len;
    }
    INIT_CHECKED_ARG((char *)iov, sizeof(iov_size), 0);

    MAKE_CALL(out->retval = func_ptr(ts, in->alt, iov, in->iov_cnt,
                                     in->flags));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    free(iov);
})

TARPC_FUNC(zf_alternatives_free_space, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;

    struct zf_stack     *stack = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(out->retval = func_ptr(stack, in->alt));
})
