/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/**
 * @brief TCP RPC routines implementation
 *
 * Zetaferno Direct API TCP RPC routines implementation.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "SFC Zetaferno RPC TCP"
#include "te_config.h"
#include "config.h"

#include "logger_ta_lock.h"
#include "rpc_server.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "te_sockaddr.h"
#include "zf_talib_namespace.h"
#include "zf_talib_internal.h"
#include "te_alloc.h"
#include "zf_rpc.h"
#include "te_rpc_sys_socket.h"
#include "te_dbuf.h"
#include "te_tools.h"

#include <zf/zf.h>
#include <zf/zf_tcp.h>

/**
 * Structure to receive data with zft_zc_recv()-like calls.
 */
typedef struct rpc_zft_msg_iov {
    struct zft_msg   msg;       /**< Control data structure. */
    struct iovec     iov[0];    /**< Pointers to received data. */
} rpc_zft_msg_iov;

/** Allocate memory to receive a number of data vectors. */
#define ALLOC_TE_ZFT_MSG_IOV(_cnt) \
    TE_ALLOC(sizeof(rpc_zft_msg_iov) + (_cnt) * sizeof(struct iovec))

TARPC_FUNC(zftl_listen, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_attr = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zftl = RPC_PTR_ID_NS_INVALID;

    struct zf_attr      *attr = NULL;
    struct zf_stack     *stack = NULL;

    struct zftl *tl_out = NULL;

    PREPARE_ADDR(laddr, in->laddr, 0);

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_attr,
                                           RPC_TYPE_NS_ZF_ATTR,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zftl,
                                           RPC_TYPE_NS_ZFTL,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(attr, in->attr, ns_attr,);

    MAKE_CALL(out->retval = func_ptr(stack, laddr, ZFTS_ADDR_LEN(laddr),
                                     attr, &tl_out));
    out->tl = RCF_PCH_MEM_INDEX_ALLOC(tl_out, ns_zftl);
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zftl_getname,
{
    COPY_ARG_ADDR(laddr);
},
{
    static rpc_ptr_id_namespace ns_zftl = RPC_PTR_ID_NS_INVALID;

    struct zftl *tl = NULL;

    socklen_t laddr_len;

    PREPARE_ADDR(laddr, out->laddr, out->laddr.raw.raw_len);

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zftl, RPC_TYPE_NS_ZFTL,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(tl, in->tl, ns_zftl,);

    laddr_len = sizeof(*laddr);
    MAKE_CALL(func_ptr(tl, laddr, &laddr_len));

    sockaddr_output_h2rpc(laddr, laddrlen,
                          out->laddr.raw.raw_len, &out->laddr);
})

TARPC_FUNC(zftl_accept, {},
{
    static rpc_ptr_id_namespace ns_zftl = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zftl   *tl = NULL;
    struct zft    *ts_out = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zftl,
                                           RPC_TYPE_NS_ZFTL,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(tl, in->tl, ns_zftl,);

    MAKE_CALL(out->retval = func_ptr(tl, &ts_out));
    out->ts = RCF_PCH_MEM_INDEX_ALLOC(ts_out, ns_zft);
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zftl_to_waitable, {},
{
    static rpc_ptr_id_namespace ns_zftl = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zf_w = RPC_PTR_ID_NS_INVALID;

    struct zftl         *tl = NULL;
    struct zf_waitable  *waitable = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zftl,
                                           RPC_TYPE_NS_ZFTL,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zf_w,
                                           RPC_TYPE_NS_ZF_WAITABLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(tl, in->tl, ns_zftl,);

    MAKE_CALL(waitable = func_ptr_ret_ptr(tl));
    out->zf_waitable = RCF_PCH_MEM_INDEX_ALLOC(waitable, ns_zf_w);
})

TARPC_FUNC(zftl_free, {},
{
    static rpc_ptr_id_namespace ns_zftl = RPC_PTR_ID_NS_INVALID;

    struct zftl *tl = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zftl,
                                           RPC_TYPE_NS_ZFTL,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(tl, in->tl, ns_zftl,);

    MAKE_CALL(out->retval = func_ptr(tl));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    if (out->retval == 0)
        RCF_PCH_MEM_INDEX_FREE(in->tl, ns_zftl);
})

TARPC_FUNC(zft_to_waitable, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zf_w = RPC_PTR_ID_NS_INVALID;

    struct zft          *ts = NULL;
    struct zf_waitable  *waitable = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zf_w,
                                           RPC_TYPE_NS_ZF_WAITABLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(waitable = func_ptr_ret_ptr(ts));
    out->zf_waitable = RCF_PCH_MEM_INDEX_ALLOC(waitable, ns_zf_w);
})

TARPC_FUNC(zft_alloc, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_attr = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zft_handle = RPC_PTR_ID_NS_INVALID;

    struct zf_attr      *attr = NULL;
    struct zf_stack     *stack = NULL;

    struct zft_handle *handle_out = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_attr,
                                           RPC_TYPE_NS_ZF_ATTR,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft_handle,
                                           RPC_TYPE_NS_ZFT_HANDLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(attr, in->attr, ns_attr,);

    MAKE_CALL(out->retval = func_ptr(stack, attr, &handle_out));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    out->handle = RCF_PCH_MEM_INDEX_ALLOC(handle_out, ns_zft_handle);
})

TARPC_FUNC(zft_addr_bind, {},
{
    static rpc_ptr_id_namespace ns_zft_handle = RPC_PTR_ID_NS_INVALID;

    struct zft_handle *handle = NULL;

    PREPARE_ADDR(laddr, in->laddr, 0);

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft_handle,
                                           RPC_TYPE_NS_ZFT_HANDLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(handle, in->handle, ns_zft_handle,);

    MAKE_CALL(out->retval = func_ptr(handle, laddr,
                                     ZFTS_ADDR_LEN(laddr), in->flags));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zft_connect, {},
{
    static rpc_ptr_id_namespace ns_zft_handle = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft_handle *handle = NULL;
    struct zft        *ts_out = NULL;

    PREPARE_ADDR(raddr, in->raddr, 0);

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft_handle,
                                           RPC_TYPE_NS_ZFT_HANDLE,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft, RPC_TYPE_NS_ZFT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(handle, in->handle, ns_zft_handle,);

    MAKE_CALL(out->retval = func_ptr(handle, raddr,
                                     ZFTS_ADDR_LEN(raddr), &ts_out));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    if (out->retval == 0)
    {
        out->ts = RCF_PCH_MEM_INDEX_ALLOC(ts_out, ns_zft);
        RCF_PCH_MEM_INDEX_FREE(in->handle, ns_zft_handle);
    }
})

TARPC_FUNC(zft_shutdown_tx, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft *ts = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft, RPC_TYPE_NS_ZFT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = func_ptr(ts));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zft_handle_free, {},
{
    static rpc_ptr_id_namespace ns_zft_handle = RPC_PTR_ID_NS_INVALID;

    struct zft_handle *handle = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft_handle,
                                           RPC_TYPE_NS_ZFT_HANDLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(handle, in->handle, ns_zft_handle,);

    MAKE_CALL(out->retval = func_ptr(handle));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    if (out->retval == 0)
        RCF_PCH_MEM_INDEX_FREE(in->handle, ns_zft_handle);
})

TARPC_FUNC(zft_free, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft *ts = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft, RPC_TYPE_NS_ZFT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = func_ptr(ts));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    if (out->retval == 0)
        RCF_PCH_MEM_INDEX_FREE(in->ts, ns_zft);
})

TARPC_FUNC(zft_state, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft *ts = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft, RPC_TYPE_NS_ZFT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = tcp_state_h2rpc(func_ptr(ts)));
})

TARPC_FUNC(zft_error, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft *ts = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft, RPC_TYPE_NS_ZFT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = errno_h2rpc(func_ptr(ts)));
})

TARPC_FUNC(zft_handle_getname,
{
    COPY_ARG_ADDR(laddr);
},
{
    static rpc_ptr_id_namespace ns_zft_handle = RPC_PTR_ID_NS_INVALID;

    struct zft_handle *handle = NULL;

    PREPARE_ADDR(laddr, out->laddr, out->laddr.raw.raw_len);


    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft_handle,
                                           RPC_TYPE_NS_ZFT_HANDLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(handle, in->handle, ns_zft_handle,);

    MAKE_CALL(func_ptr(handle, laddr, &laddrlen));

    sockaddr_output_h2rpc(laddr, laddrlen,
                          out->laddr.raw.raw_len, &out->laddr);
})

TARPC_FUNC(zft_getname,
{
    COPY_ARG_ADDR(laddr);
    COPY_ARG_ADDR(raddr);
},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft *ts = NULL;

    PREPARE_ADDR(laddr, out->laddr, out->laddr.raw.raw_len);
    PREPARE_ADDR(raddr, out->raddr, out->raddr.raw.raw_len);

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft, RPC_TYPE_NS_ZFT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(func_ptr(ts, laddr, &laddrlen, raddr, &raddrlen));

    sockaddr_output_h2rpc(laddr, laddrlen,
                          out->laddr.raw.raw_len, &out->laddr);
    sockaddr_output_h2rpc(raddr, raddrlen,
                          out->raddr.raw.raw_len, &out->raddr);
})

/**
 * Convert RPC structure @c tarpc_zft_msg to the host representation.
 *
 * @param rpc_msg   ZF TCP message structure in RPC representation.
 * @param msg_iov   The target structure to keep the coversion result.
 *
 * @return Status code.
 */
static int
tarpc_zft_msg_rpc2h(tarpc_zft_msg *rpc_msg,
                    rpc_zft_msg_iov *msg_iov)
{
    size_t i;

    if (rpc_msg->reserved.reserved_len <
        TE_ARRAY_LEN(msg_iov->msg.reserved))
    {
        ERROR("'reserved' array length is different in RPC %u and ZF "
              "library %u",
              rpc_msg->reserved.reserved_len,
              TE_ARRAY_LEN(msg_iov->msg.reserved));
        return -EINVAL;
    }

    for (i = 0; i < TE_ARRAY_LEN(msg_iov->msg.reserved); i++)
        msg_iov->msg.reserved[i] = rpc_msg->reserved.reserved_val[i];

    /* TODO: add flags conversion */
    msg_iov->msg.flags = rpc_msg->flags;
    msg_iov->msg.iovcnt = rpc_msg->iovcnt;
    msg_iov->msg.pkts_left = rpc_msg->pkts_left;

    if (msg_iov->msg.iov != NULL)
        msg_iov->msg.iov[0].iov_len = rpc_msg->pftf_len;

    return 0;
}

/**
 * Convert structure @c rpc_zft_msg_iov to the RPC representation.
 *
 * @param msg_iov   ZF TCP message structure.
 * @param rpc_msg   The target structure to keep the coversion result.
 * @param copy_data Copy vectors data if @c TRUE.
 *
 * @return Status code.
 */
static int
tarpc_zft_msg_h2rpc(rpc_zft_msg_iov *msg_iov,
                    struct tarpc_zft_msg *rpc_msg, te_bool copy_data)
{
    int i;

    if (rpc_msg->reserved.reserved_len <
        TE_ARRAY_LEN(msg_iov->msg.reserved))
    {
        ERROR("'reserved' array length is different in RPC %u and ZF "
              "library %u",
              rpc_msg->reserved.reserved_len,
              TE_ARRAY_LEN(msg_iov->msg.reserved));
        return -EINVAL;
    }

    for (i = 0; i < (int)TE_ARRAY_LEN(msg_iov->msg.reserved); i++)
        rpc_msg->reserved.reserved_val[i] = msg_iov->msg.reserved[i];

    /* TODO: add flags conversion */
    rpc_msg->flags = msg_iov->msg.flags;
    rpc_msg->pkts_left = msg_iov->msg.pkts_left;

    if ((int)rpc_msg->iovcnt < msg_iov->msg.iovcnt)
    {
        ERROR("Returned iovec buffers number is more than output RPC "
              "buffers number");
        return -EINVAL;
    }
    rpc_msg->iovcnt = msg_iov->msg.iovcnt;

    if (!copy_data)
        return 0;

    if (rpc_msg->iov.iov_len > 0 ||
        rpc_msg->iov.iov_val != NULL)
    {
        ERROR("It is expected that no iovec buffers are passed as input");
        return -EINVAL;
    }

    rpc_msg->iov.iov_len = msg_iov->msg.iovcnt;
    rpc_msg->iov.iov_val = TE_ALLOC(rpc_msg->iov.iov_len *
                                    sizeof(*(rpc_msg->iov.iov_val)));
    if (rpc_msg->iov.iov_val == NULL)
    {
        ERROR("Failed to allocate memory for iovec buffers array");
        return -ENOMEM;
    }

    for (i = 0; i < rpc_msg->iovcnt; i++)
    {
        rpc_msg->iov.iov_val[i].iov_base.iov_base_len =
            msg_iov->iov[i].iov_len;
        rpc_msg->iov.iov_val[i].iov_len = msg_iov->iov[i].iov_len;

        rpc_msg->iov.iov_val[i].iov_base.iov_base_val =
                                  TE_ALLOC(msg_iov->iov[i].iov_len);
        if (rpc_msg->iov.iov_val[i].iov_base.iov_base_val == NULL)
        {
            ERROR("Failed to allocate memory");
            return -ENOMEM;
        }

        /* Copying data, since zero-copy receive call sets pointers to
         * buffers with received data. */
        memcpy(rpc_msg->iov.iov_val[i].iov_base.iov_base_val,
               msg_iov->iov[i].iov_base,
               msg_iov->iov[i].iov_len);
    }

    return 0;
}

TARPC_FUNC(zft_zc_recv,
{
    COPY_ARG_ADDR(msg);
},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zft_msg = RPC_PTR_ID_NS_INVALID;

    rpc_zft_msg_iov  *msg_iov = NULL;
    struct zft       *ts = NULL;

    int rc;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft, RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft_msg,
                                           RPC_TYPE_NS_ZFT_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    msg_iov = ALLOC_TE_ZFT_MSG_IOV(out->msg.iovcnt);
    if (msg_iov == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return;
    }

    rc = tarpc_zft_msg_rpc2h(&out->msg, msg_iov);
    if (rc != 0)
    {
        free(msg_iov);
        out->common._errno = rc;
        return;
    }

    MAKE_CALL(func_ptr(ts, &msg_iov->msg, zf_zc_flags_rpc2h(in->flags)));

    rc = tarpc_zft_msg_h2rpc(msg_iov, &out->msg, TRUE);
    if (rc != 0 || out->msg.iovcnt <= 0)
    {
        free(msg_iov);
        out->common._errno = rc;
        return;
    }

    out->msg.ptr = RCF_PCH_MEM_INDEX_ALLOC(msg_iov, ns_zft_msg);
})

TARPC_FUNC(zft_zc_recv_done,
{
    COPY_ARG_ADDR(msg);
},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zft_msg = RPC_PTR_ID_NS_INVALID;

    rpc_zft_msg_iov  *msg_iov = NULL;
    struct zft       *ts = NULL;

    int rc;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft_msg,
                                           RPC_TYPE_NS_ZFT_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(msg_iov, out->msg.ptr, ns_zft_msg,);

    rc = tarpc_zft_msg_rpc2h(&out->msg, msg_iov);
    if (rc != 0)
    {
        free(msg_iov);
        out->common._errno = rc;
        return;
    }

    MAKE_CALL(out->retval = func_ptr(ts, &msg_iov->msg));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    RCF_PCH_MEM_INDEX_FREE(out->msg.ptr, ns_zft_msg);

    rc = tarpc_zft_msg_h2rpc(msg_iov, &out->msg, FALSE);
    if (rc != 0)
        out->common._errno = rc;
    free(msg_iov);
})

TARPC_FUNC(zft_zc_recv_done_some,
{
    COPY_ARG_ADDR(msg);
},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zft_msg = RPC_PTR_ID_NS_INVALID;

    rpc_zft_msg_iov  *msg_iov = NULL;
    struct zft       *ts = NULL;

    int rc;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft_msg,
                                           RPC_TYPE_NS_ZFT_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(msg_iov, out->msg.ptr, ns_zft_msg,);

    rc = tarpc_zft_msg_rpc2h(&out->msg, msg_iov);
    if (rc != 0)
    {
        free(msg_iov);
        out->common._errno = rc;
        return;
    }

    MAKE_CALL(out->retval = func_ptr(ts, &msg_iov->msg, in->len));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    RCF_PCH_MEM_INDEX_FREE(out->msg.ptr, ns_zft_msg);

    rc = tarpc_zft_msg_h2rpc(msg_iov, &out->msg, FALSE);
    if (rc != 0)
        out->common._errno = rc;
    free(msg_iov);
})

TARPC_FUNC_STANDALONE(zft_read_zft_msg, {},
{
    static rpc_ptr_id_namespace ns_zft_msg = RPC_PTR_ID_NS_INVALID;
    rpc_zft_msg_iov *msg_iov = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft_msg,
                                           RPC_TYPE_NS_ZFT_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(msg_iov, in->msg_ptr, ns_zft_msg,);

    out->msg.reserved.reserved_len = TE_ARRAY_LEN(msg_iov->msg.reserved);
    out->msg.reserved.reserved_val =
        TE_ALLOC(TE_ARRAY_LEN(msg_iov->msg.reserved) *
                 sizeof(*out->msg.reserved.reserved_val));

    out->msg.iovcnt = msg_iov->msg.iovcnt;
    out->msg.iov.iov_len = 0;
    out->msg.iov.iov_val = NULL;

    out->common._errno = tarpc_zft_msg_h2rpc(msg_iov, &out->msg, TRUE);
    out->msg.ptr = in->msg_ptr;
})

TARPC_FUNC(zft_send, {},
{
    struct iovec   *iov;
    size_t          iov_size = sizeof(*iov) * in->iovec.iovec_len;
    size_t          i;

    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft       *ts = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    iov = TE_ALLOC(iov_size);

    if (iov == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }
    else
    {
        for (i = 0; i < in->iovec.iovec_len; i++)
        {
            INIT_CHECKED_ARG(in->iovec.iovec_val[i].iov_base.iov_base_val,
                             in->iovec.iovec_val[i].iov_base.iov_base_len,
                             0);

            iov[i].iov_base = in->iovec.iovec_val[i].iov_base.iov_base_val;
            iov[i].iov_len = in->iovec.iovec_val[i].iov_len;
        }
        INIT_CHECKED_ARG((char *)iov, sizeof(iov_size), 0);

        MAKE_CALL(out->retval = func_ptr(ts, iov, in->iovcnt,
                                         send_recv_flags_rpc2h(in->flags)));
        TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
        free(iov);
    }
})

TARPC_FUNC(zft_send_single, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;

    struct zft *ts = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZFT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns,);

    INIT_CHECKED_ARG(in->buf.buf_val, in->buf.buf_len, 0);

    MAKE_CALL(out->retval = func_ptr(ts, in->buf.buf_val, in->buf.buf_len,
                                     send_recv_flags_rpc2h(in->flags)));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zft_send_space, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft   *ts = NULL;
    size_t        size;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = func_ptr(ts, &size));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    out->size = size;
})

TARPC_FUNC(zft_recv,
{
    COPY_ARG(iovec);
},
{
    struct iovec   *iov;
    size_t          iov_size = sizeof(*iov) * out->iovec.iovec_len;
    unsigned int    i;

    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft       *ts = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    iov = TE_ALLOC(iov_size);

    if (iov == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
    }
    else
    {
        for (i = 0; i < out->iovec.iovec_len; i++)
        {
            INIT_CHECKED_ARG(
                      out->iovec.iovec_val[i].iov_base.iov_base_val,
                      out->iovec.iovec_val[i].iov_base.iov_base_len,
                      out->iovec.iovec_val[i].iov_len);
            iov[i].iov_base = out->iovec.iovec_val[i].iov_base.iov_base_val;
            iov[i].iov_len = out->iovec.iovec_val[i].iov_len;
        }
        INIT_CHECKED_ARG((char *)iov, sizeof(iov_size), 0);

        MAKE_CALL(out->retval = func_ptr(ts, iov, in->iovcnt,
                                         in->flags));
        TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
        free(iov);
    }
})

TARPC_FUNC(zft_get_mss, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft          *ts = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = func_ptr(ts));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zft_get_header_size, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;

    struct zft          *ts = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = func_ptr(ts));
})

TARPC_FUNC(zft_pkt_get_timestamp, {},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zft_msg = RPC_PTR_ID_NS_INVALID;
    rpc_zft_msg_iov *msg_iov;
    struct zft *tcp_z;

    unsigned int flags;
    struct timespec ts;
    int i;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft, RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft_msg,
                                           RPC_TYPE_NS_ZFT_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(tcp_z, in->tcp_z, ns_zft,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(msg_iov, in->msg_ptr, ns_zft_msg,);

    INIT_CHECKED_ARG(msg_iov,
                     sizeof(*msg_iov) +
                          msg_iov->msg.iovcnt * sizeof(struct iovec),
                     0);
    for (i = 0; i < msg_iov->msg.iovcnt; i++)
    {
        INIT_CHECKED_ARG(msg_iov->iov[i].iov_base,
                         msg_iov->iov[i].iov_len, 0);
    }

    MAKE_CALL(out->retval = func_ptr(tcp_z, &msg_iov->msg, &ts,
                                     in->pktind, &flags));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    out->ts.tv_sec = ts.tv_sec;
    out->ts.tv_nsec = ts.tv_nsec;
    out->flags = zf_sync_flags_h2rpc(flags);
})

TARPC_FUNC(zft_get_tx_timestamps,
{
    COPY_ARG(reports);
},
{
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    struct zft *tz = NULL;
    struct zf_pkt_report *reports = NULL;
    struct zf_pkt_report *reports_copy = NULL;
    int count;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft, RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(tz, in->tz, ns_zft,);

    if (prepare_pkt_reports(&reports, &reports_copy,
                            out->reports.reports_val,
                            out->reports.reports_len) < 0)
    {
        out->retval = -1;
        return;
    }

    count = in->count;
    MAKE_CALL(out->retval = func(tz, reports, &count));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    out->count = count;
    if (process_pkt_reports(reports, reports_copy,
                            out->reports.reports_val,
                            out->reports.reports_len,
                            count) < 0)
    {
        out->retval = -1;
    }
})

/**
 * Read all data on a TCP zocket using zft_recv() function.
 *
 * @param lib_flags How to resolve function name.
 * @param ts        TCP zocket.
 * @param buf       Location of the buffer to read the data in; may be @c NULL
 *                  to drop the read data.
 * @param read      Location for the amount of data read.
 *
 * @return @c -1 in the case of failure or @c 0 on success (data reading
 * was interrupted by EAGAIN error).
 */
static int
zft_read_all(tarpc_lib_flags lib_flags, struct zft* ts, uint8_t **buf,
             size_t *read)
{
    api_func_ptr zft_recv_f;
    te_dbuf dbuf = TE_DBUF_INIT(50);
    int rc;
    char tmp_buf[1400];
    struct iovec iov = {tmp_buf, sizeof(tmp_buf)};

    if (tarpc_find_func(lib_flags, "zft_recv", (api_func *)&zft_recv_f) != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_ENOENT),
                         "fail to resolve zft_recv function");
        return -1;
    }

    *read = 0;

    while (TRUE)
    {
        rc = zft_recv_f(ts, &iov, 1, 0);

        if (rc >= 0)
        {
            *read += rc;
            te_dbuf_append(&dbuf, tmp_buf, rc);
        }
        else if (rc == -EAGAIN)
        {
            break;
        }
        else
        {
            te_rpc_error_set(TE_OS_RC(TE_RPC, -rc),
                             "zft_recv() returned unexpected error");
            if (buf != NULL)
                *buf = dbuf.ptr;
            else
                te_dbuf_free(&dbuf);

            return -1;
        }
    }

    if (buf != NULL)
        *buf = dbuf.ptr;
    else
        te_dbuf_free(&dbuf);

    return 0;
}

TARPC_FUNC_STATIC(zft_read_all, {},
{
    size_t read;
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    struct zft *ts = NULL;

    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = func(in->common.lib_flags, ts,
                                 &out->buf.buf_val, &read));
    out->buf.buf_len = read;
})

/**
 * Read all data on a TCP zocket using zft_zc_recv() function.
 *
 * @param lib_flags How to resolve function name.
 * @param ts        TCP zocket.
 * @param buf       Location of the buffer to read the data in; may be @c NULL
 *                  to drop the read data.
 * @param read      Location for the amount of data read.
 *
 * @return @c -1 in the case of failure or @c 0 on success (data reading
 * was interrupted by EOF return code).
 */
static int
zft_read_all_zc(tarpc_lib_flags lib_flags, struct zft* ts, uint8_t **buf,
             size_t *read)
{
    struct rx_msg
    {
        /* The iovec used by zft_msg must be immediately afterwards */
        struct zft_msg msg;
        struct iovec iov;
    };

    api_func_ptr zft_zc_recv_f;
    api_func_ptr zft_zc_recv_done_f;
    te_dbuf dbuf = TE_DBUF_INIT(50);
    char tmp_buf[1400];
    struct rx_msg msg;

    msg.iov.iov_base = tmp_buf;
    msg.iov.iov_len = sizeof(tmp_buf);

    if (tarpc_find_func(lib_flags, "zft_zc_recv",
                        (api_func *)&zft_zc_recv_f) != 0 ||
        tarpc_find_func(lib_flags, "zft_zc_recv_done",
                        (api_func *)&zft_zc_recv_done_f) != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_ENOENT),
                         "fail to resolve functions");
        return -1;
    }

    *read = 0;

    while (TRUE)
    {
        msg.msg.iovcnt = 1;
        zft_zc_recv_f(ts, &msg.msg, 0);

        if (msg.msg.iovcnt > 0)
        {
            int rc;

            *read += msg.iov.iov_len;
            te_dbuf_append(&dbuf, msg.iov.iov_base, msg.iov.iov_len);

            rc = zft_zc_recv_done_f(ts, &msg);
            if (rc == 0)
                break;
            if (rc < 0)
            {
                te_rpc_error_set(TE_OS_RC(TE_RPC, -rc),
                                 "zft_zc_recv_done() returned "
                                 "unexpected error");
                if (buf != NULL)
                    *buf = dbuf.ptr;
                else
                    te_dbuf_free(&dbuf);

                return -1;
            }
        }
        else
        {
            break;
        }
    }

    if (buf != NULL)
        *buf = dbuf.ptr;
    else
        te_dbuf_free(&dbuf);

    return 0;
}

TARPC_FUNC_STATIC(zft_read_all_zc, {},
{
    size_t read;
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    struct zft *ts = NULL;

    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = func(in->common.lib_flags, ts,
                                 &out->buf.buf_val, &read));
    out->buf.buf_len = read;
})


static int
zft_overfill_buffers(tarpc_lib_flags lib_flags, struct zf_stack *stack,
                     struct zft* ts, uint8_t **buf,
                     size_t *written)
{
    api_func_ptr zft_send_f;
    api_func_ptr zf_process_events_f;
    api_func_ptr zf_process_events_long_f;
    te_dbuf dbuf = TE_DBUF_INIT(50);
    char tmp_buf[1400];
    struct iovec iov;
    te_bool data_sent = FALSE;
    int rc = 0;

    iov.iov_base = tmp_buf;
    iov.iov_len = sizeof(tmp_buf);

    if (tarpc_find_func(lib_flags, "zft_send",
                        (api_func *)&zft_send_f) != 0 ||
        tarpc_find_func(lib_flags, "zf_process_events",
                        (api_func *)&zf_process_events_f) != 0 ||
        tarpc_find_func(lib_flags, "zf_process_events_long",
                        (api_func *)&zf_process_events_long_f) != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_ENOENT),
                         "fail to resolve functions");
        return -1;
    }

    *written = 0;

    do {
        data_sent = FALSE;
        do {
            te_fill_buf(tmp_buf, sizeof(tmp_buf));
            rc = zft_send_f(ts, &iov, 1, 0);
            if (rc < 0)
            {
                if (rc == -EAGAIN)
                {
                    rc = 0;
                    break;
                }
                else
                {
                    te_rpc_error_set(TE_OS_RC(TE_RPC, -rc),
                                     "zft_send() returned unexpected error");
                    rc = -1;
                    goto exit;
                }
            }
            else
            {
                te_dbuf_append(&dbuf, tmp_buf, rc);
                *written += rc;
                data_sent = TRUE;
            }

            rc = zf_process_events_f(stack);
            if (rc < 0)
            {
                te_rpc_error_set(rc == -1 ? TE_RC(TE_TA_UNIX, TE_EFAIL) :
                                            TE_OS_RC(TE_RPC, -rc),
                                 "zf_process_events() failed");
                rc = -1;
                goto exit;
            }
        } while (TRUE);

        if (data_sent)
        {
            rc = zf_process_events_long_f(stack, 500);
            if (rc < 0)
            {
                te_rpc_error_set(rc == -1 ? TE_RC(TE_TA_UNIX, TE_EFAIL) :
                                            TE_OS_RC(TE_RPC, -rc),
                                 "zf_process_events_long() failed");
                rc = -1;
                goto exit;
            }
        }
    } while (data_sent);

exit:
    if (buf != NULL)
        *buf = dbuf.ptr;
    else
        te_dbuf_free(&dbuf);

    return rc;
}

TARPC_FUNC_STATIC(zft_overfill_buffers, {},
{
    size_t written;
    static rpc_ptr_id_namespace ns_zft = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;
    struct zf_stack *stack = NULL;
    struct zft *ts = NULL;

    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zft,
                                           RPC_TYPE_NS_ZFT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(ts, in->ts, ns_zft,);

    MAKE_CALL(out->retval = func(in->common.lib_flags, stack, ts,
                                 &out->buf.buf_val, &written));
    out->buf.buf_len = written;
})
