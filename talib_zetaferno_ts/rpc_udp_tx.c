/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/**
 * @brief UDP TX RPC routines implementation
 *
 * Zetaferno Direct API UDP TX RPC routines implementation.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#define TE_LGR_USER     "SFC Zetaferno RPC UDP TX"

#include "te_config.h"
#include "config.h"

#include "logger_ta_lock.h"
#include "rpc_server.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "te_sockaddr.h"
#include "zf_talib_namespace.h"
#include "zf_talib_common.h"
#include "zf_talib_internal.h"
#include "te_alloc.h"
#include "te_sleep.h"
#include "zf_rpc.h"

#include <zf/zf.h>
#include <zf/zf_udp.h>

/**
 * Convert RPC zfut_send() flags to native flags.
 *
 * @param flags     RPC flags.
 *
 * @return Native flags.
 */
static unsigned int zfut_flags_rpc2h(unsigned int flags)
{
    return
        (!!(flags & RPC_ZFUT_FLAG_DONT_FRAGMENT) *
         ZFUT_FLAG_DONT_FRAGMENT);
}

TARPC_FUNC(zfut_alloc, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_attr = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zfut = RPC_PTR_ID_NS_INVALID;
    struct zf_stack *stack;
    struct zf_attr  *attr;
    struct zfut* us_out = NULL;

    PREPARE_ADDR(laddr, in->laddr, 0);
    PREPARE_ADDR(raddr, in->raddr, 0);

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_attr, RPC_TYPE_NS_ZF_ATTR,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfut, RPC_TYPE_NS_ZFUT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(attr, in->attr, ns_attr,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    /* TODO: add flags conversion. */
    MAKE_CALL(out->retval = func_ptr(&us_out, stack,
                                     laddr, ZFTS_ADDR_LEN(laddr),
                                     raddr, ZFTS_ADDR_LEN(raddr),
                                     in->flags, attr));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    if (out->retval == 0)
        out->utx = RCF_PCH_MEM_INDEX_ALLOC(us_out, ns_zfut);
})

TARPC_FUNC(zfut_free, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zfut *us;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZFUT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->utx, ns,);

    MAKE_CALL(out->retval = func_ptr(us));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    if (out->retval == 0)
        RCF_PCH_MEM_INDEX_FREE(in->utx, ns);
})

TARPC_FUNC(zfut_send, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct iovec   *iov;
    size_t          iov_size = sizeof(*iov) * in->iovec.iovec_len;
    size_t          i;
    struct zfut    *us;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZFUT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->utx, ns,);

    iov = TE_ALLOC(iov_size);
    if (iov == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return;
    }

    for (i = 0; i < in->iovec.iovec_len; i++)
    {
        INIT_CHECKED_ARG(in->iovec.iovec_val[i].iov_base.iov_base_val,
                         in->iovec.iovec_val[i].iov_base.iov_base_len, 0);

        iov[i].iov_base = in->iovec.iovec_val[i].iov_base.iov_base_val;
        iov[i].iov_len = in->iovec.iovec_val[i].iov_len;
    }
    INIT_CHECKED_ARG((char *)iov, iov_size, 0);

    MAKE_CALL(out->retval = func_ptr(us, iov, in->iovcnt,
                                     zfut_flags_rpc2h(in->flags)));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    free(iov);
})

TARPC_FUNC(zfut_to_waitable, {},
{
    static rpc_ptr_id_namespace ns_zfut = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zf_w = RPC_PTR_ID_NS_INVALID;
    struct zf_waitable *waitable;
    struct zfut        *us;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfut, RPC_TYPE_NS_ZFUT,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zf_w,
                                           RPC_TYPE_NS_ZF_WAITABLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->utx, ns_zfut,);

    MAKE_CALL(waitable = func_ptr(us));

    out->zf_waitable = RCF_PCH_MEM_INDEX_ALLOC(waitable, ns_zf_w);
})

TARPC_FUNC(zfut_get_mss, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zfut *us;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZFUT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->utx, ns,);

    MAKE_CALL(out->retval = func_ptr(us));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

TARPC_FUNC(zfut_send_single, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zfut *us;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZFUT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->utx, ns,);

    INIT_CHECKED_ARG(in->buf.buf_val, in->buf.buf_len, 0);

    MAKE_CALL(out->retval = func_ptr(us, in->buf.buf_val, in->buf.buf_len));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

/* See description in zf_rpc.h. */
int
zfut_get_send_function(zfts_send_function send_func, api_func_ptr *func)
{
    int rc;

    switch (send_func)
    {
        case ZFTS_ZFUT_SEND_SINGLE:
            rc = tarpc_find_func(FALSE, "zfut_send_single",
                                 (api_func *)func);
            break;

        case ZFTS_ZFUT_SEND:
            rc = tarpc_find_func(FALSE, "zfut_send", (api_func *)func);
            break;

        default:
            te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_EINVAL),
                             "Invalid send function index: %d", send_func);
            return -1;
    }

    if (rc != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, rc),
                         "Failed to resolve send function");
        return -1;
    }

    return 0;
}

/**
 * Repeatedly send datagrams during a period of time.
 *
 * @param stack         Zetaferno stack.
 * @param utx           UDP TX zocket.
 * @param send_func     Transmitting function.
 * @param dgram_size    Datagrams size, bytes.
 * @param iovcnt        Iov vectors number.
 * @param duration      How long transmit datagrams, milliseconds.
 * @param stats         Sent data amount, bytes.
 * @param errors        @c EAGAIN errors counter.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
int
zfut_flooder(struct zf_stack *stack, struct zfut *utx,
             zfts_send_function send_func, int dgram_size, int iovcnt,
             int duration, uint64_t *stats, uint64_t *errors)
{
    api_func_ptr func_send;
    api_func_ptr process_events_func;
    struct timeval tv_start;
    struct timeval tv_now;
    struct iovec  *iov;
    void          *buf;
    uint64_t i;
    int offt = 0;
    int rc;

    rc = zfut_get_send_function(send_func, &func_send);
    if (rc != 0)
        return rc;

    rc = tarpc_find_func(FALSE, "zf_process_events",
                         (api_func *)&process_events_func);
    if (rc != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, rc),
                         "Failed to resolve zf_process_events()");
        return -1;
    }

    *stats = 0;
    *errors = 0;

    if ((rc = gettimeofday(&tv_start, NULL)) != 0)
    {
        te_rpc_error_set(TE_OS_RC(TE_TA_UNIX, errno),
                         "gettimeofday() failed");
        return -1;
    }

    if (dgram_size < iovcnt)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_EINVAL),
                         "drgam_size should not be less than iovcnt");
        return -1;
    }

    buf = TE_ALLOC(dgram_size);
    iov = TE_ALLOC(iovcnt * sizeof(*iov));

    for (i = 0; i < (unsigned int)iovcnt; i++)
    {
        if (i == (unsigned int)iovcnt - 1)
            iov[i].iov_len = dgram_size - offt;
        else
            iov[i].iov_len = dgram_size / iovcnt;
        iov[i].iov_base = buf + offt;
        offt += iov[i].iov_len;
    }

    for (i = 0;; i++)
    {
        rc = process_events_func(stack);
        if (rc < 0)
        {
            ERROR("zf_process_events() #%llu failed, rc = %d (%r)", i, rc,
                  te_rc_os2te(-rc));
            te_rpc_error_set(TE_OS_RC(TE_TA_UNIX, -rc),
                             "zf_process_events() failed");
            rc = -1;
            break;
        }

        if (send_func == ZFTS_ZFUT_SEND)
            rc = func_send(utx, iov, iovcnt, 0);
        else
            rc = func_send(utx, buf, dgram_size);
        if (rc == - EAGAIN)
        {
            (*errors)++;
        }
        else if (rc != dgram_size)
        {
            ERROR("zfut_send() #%llu returned unexpected value, rc = %d "
                  "(%r)", i, rc, te_rc_os2te(-rc));
            te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_EFAIL),
                             "zfut_send() returned unexpected value");
            rc = -1;
            break;
        }
        else
        {
            *stats += rc;
        }

        rc = process_events_func(stack);
        if (rc < 0)
        {
            ERROR("zf_process_events() #%llu failed, rc = %d (%r)", i, rc,
                  te_rc_os2te(-rc));
            te_rpc_error_set(TE_OS_RC(TE_TA_UNIX, -rc),
                             "zf_process_events() failed");
            rc = -1;
            break;
        }

        if ((rc = gettimeofday(&tv_now, NULL)) != 0)
        {
            te_rpc_error_set(TE_OS_RC(TE_TA_UNIX, errno),
                             "gettimeofday() failed");
            rc = -1;
            break;
        }

        if (TIMEVAL_SUB(tv_now, tv_start) / 1000L > duration)
            break;
    }

    free(buf);
    free(iov);
    return rc;
}

TARPC_FUNC_STATIC(zfut_flooder, {},
{
    static rpc_ptr_id_namespace ns_zfut = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;
    struct zf_stack *stack;
    struct zfut *utx;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfut, RPC_TYPE_NS_ZFUT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(utx, in->utx, ns_zfut,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(out->retval = func_ptr(stack, utx, in->send_func,
                                     in->dgram_size, in->iovcnt,
                                     in->duration, &out->stats,
                                     &out->errors));
})

TARPC_FUNC(zfut_get_header_size, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;

    struct zfut *utx = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZFUT,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(utx, in->utx, ns,);

    MAKE_CALL(out->retval = func_ptr(utx));
})

TARPC_FUNC(zfut_get_tx_timestamps,
{
    COPY_ARG(reports);
},
{
    static rpc_ptr_id_namespace ns_zfut = RPC_PTR_ID_NS_INVALID;
    struct zfut *utx = NULL;
    struct zf_pkt_report *reports = NULL;
    struct zf_pkt_report *reports_copy = NULL;
    int count;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfut, RPC_TYPE_NS_ZFUT,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(utx, in->utx, ns_zfut,);

    if (prepare_pkt_reports(&reports, &reports_copy,
                            out->reports.reports_val,
                            out->reports.reports_len) < 0)
    {
        out->retval = -1;
        return;
    }

    count = in->count;
    MAKE_CALL(out->retval = func(utx, reports, &count));
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
