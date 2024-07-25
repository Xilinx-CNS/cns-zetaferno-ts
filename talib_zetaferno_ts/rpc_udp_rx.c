/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/**
 * @brief UDP RX RPC routines implementation
 *
 * Zetaferno Direct API UDP RX RPC routines implementation.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#define TE_LGR_USER     "SFC Zetaferno RPC UDP RX"

#include "te_config.h"
#include "config.h"

#include "logger_ta_lock.h"
#include "rpc_server.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif

#ifdef HAVE_NETPACKET_PACKET_H
#include <netpacket/packet.h>
#endif

#ifdef HAVE_NET_ETHERNET_H
#include <net/ethernet.h>
#endif

#ifdef HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#endif

#include "te_sockaddr.h"
#include "zf_talib_namespace.h"
#include "zf_talib_internal.h"
#include "te_alloc.h"
#include "te_sleep.h"
#include "zf_rpc.h"

#include <zf/zf.h>
#include <zf/zf_udp.h>

/**
 * Structure to receive datagrams with zfur_zc_recv()-like calls.
 */
typedef struct rpc_zfur_msg_iov {
    struct zfur_msg  msg;       /**< Control data structure. */
    struct iovec     iov[0];    /**< Pointers to received data. */
} rpc_zfur_msg_iov;

/** Allocate memory to receive a number of data vectors. */
#define ALLOC_TE_ZFUR_MSG_IOV(_cnt) \
    TE_ALLOC(sizeof(rpc_zfur_msg_iov) + (_cnt) * sizeof(struct iovec))

TARPC_FUNC(zfur_alloc, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_attr = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zfur = RPC_PTR_ID_NS_INVALID;
    struct zf_stack *stack;
    struct zf_attr  *attr;
    struct zfur     *us_out = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_attr, RPC_TYPE_NS_ZF_ATTR,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur, RPC_TYPE_NS_ZFUR,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(attr, in->attr, ns_attr,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(out->retval = func_ptr(&us_out, stack, attr));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    if (out->retval == 0)
        out->urx = RCF_PCH_MEM_INDEX_ALLOC(us_out, ns_zfur);
})

TARPC_FUNC(zfur_free, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zfur *us;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZFUR,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->urx, ns,);

    MAKE_CALL(out->retval = func_ptr(us));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    if (out->retval == 0)
        RCF_PCH_MEM_INDEX_FREE(in->urx, ns);
})

TARPC_FUNC(zfur_addr_bind,
{
    COPY_ARG_ADDR(laddr);
},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zfur *us;

    PREPARE_ADDR_GEN(laddr, out->laddr, 0, true, false);
    PREPARE_ADDR(raddr, in->raddr, 0);

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZFUR,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->urx, ns,);

    /* TODO: add flags RPC->host conversion. */
    MAKE_CALL(out->retval = func_ptr(us, laddr, ZFTS_ADDR_LEN(laddr),
                                     raddr, ZFTS_ADDR_LEN(raddr),
                                     in->flags));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    sockaddr_output_h2rpc(laddr, laddrlen, laddrlen, &out->laddr);
})

TARPC_FUNC(zfur_addr_unbind, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zfur *us;

    PREPARE_ADDR(laddr, in->laddr, 0);
    PREPARE_ADDR(raddr, in->raddr, 0);

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZFUR,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->urx, ns,);

    /* TODO: add flags RPC->host conversion. */
    MAKE_CALL(out->retval = func_ptr(us, laddr, ZFTS_ADDR_LEN(laddr),
                                     raddr, ZFTS_ADDR_LEN(raddr),
                                     in->flags));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

/**
 * Convert RPC structure @c tarpc_zfur_msg to the host representation.
 *
 * @param rpc_msg   ZF UDP RX message structure in RPC representation.
 * @param msg_iov   The target structure to keep the coversion result.
 *
 * @return Status code.
 */
static te_errno
tarpc_zfur_msg_rpc2h(tarpc_zfur_msg *rpc_msg, rpc_zfur_msg_iov *msg_iov)
{
    size_t i;

    if (rpc_msg->reserved.reserved_len <
        TE_ARRAY_LEN(msg_iov->msg.reserved))
    {
        ERROR("'reserved' array length is different in RPC %u and ZF "
              "library %u",
              rpc_msg->reserved.reserved_len,
              TE_ARRAY_LEN(msg_iov->msg.reserved));
        return TE_RC(TE_TA_UNIX, TE_EINVAL);
    }

    for (i = 0; i < TE_ARRAY_LEN(msg_iov->msg.reserved); i++)
        msg_iov->msg.reserved[i] = rpc_msg->reserved.reserved_val[i];

    /* TODO: add flags conversion */
    msg_iov->msg.flags = rpc_msg->flags;
    msg_iov->msg.iovcnt = rpc_msg->iovcnt;
    msg_iov->msg.dgrams_left = rpc_msg->dgrams_left;

    if (msg_iov->msg.iov != NULL)
        msg_iov->msg.iov[0].iov_len = rpc_msg->pftf_len;

    return 0;
}

/**
 * Convert structure @c rpc_zfur_msg_iov to the RPC representation.
 *
 * @param msg_iov   ZF UDP RX message structure.
 * @param rpc_msg   The target structure to keep the coversion result.
 * @param copy_data Copy vectors data if @c TRUE.
 *
 * @return Status code.
 */
static te_errno
tarpc_zfur_msg_h2rpc(rpc_zfur_msg_iov *msg_iov,
                     struct tarpc_zfur_msg *rpc_msg, te_bool copy_data)
{
    int i;

    if (rpc_msg->reserved.reserved_len <
        TE_ARRAY_LEN(msg_iov->msg.reserved))
    {
        ERROR("'reserved' array length is different in RPC %u and ZF "
              "library %u",
              rpc_msg->reserved.reserved_len,
              TE_ARRAY_LEN(msg_iov->msg.reserved));
        return TE_RC(TE_TA_UNIX, TE_EINVAL);
    }

    for (i = 0; i < (int)TE_ARRAY_LEN(msg_iov->msg.reserved); i++)
        rpc_msg->reserved.reserved_val[i] = msg_iov->msg.reserved[i];

    /* TODO: add flags conversion */
    rpc_msg->flags = msg_iov->msg.flags;
    rpc_msg->dgrams_left = msg_iov->msg.dgrams_left;

    if ((int)rpc_msg->iovcnt < msg_iov->msg.iovcnt)
    {
        ERROR("Returned iovec buffers number is more than output RPC "
              "buffers number");
        return TE_RC(TE_TA_UNIX, TE_EINVAL);
    }
    rpc_msg->iovcnt = msg_iov->msg.iovcnt;

    if (!copy_data)
        return 0;

    if (rpc_msg->iov.iov_len > 0 || rpc_msg->iov.iov_val != NULL)
    {
        ERROR("It is expected that no iovec buffers are passed as input");
        return TE_RC(TE_TA_UNIX, TE_EINVAL);
    }

    rpc_msg->iov.iov_len = msg_iov->msg.iovcnt;
    rpc_msg->iov.iov_val = TE_ALLOC(rpc_msg->iov.iov_len *
                                    sizeof(*(rpc_msg->iov.iov_val)));
    if (rpc_msg->iov.iov_val == NULL)
        return TE_RC(TE_TA_UNIX, TE_ENOMEM);

    for (i = 0; i < rpc_msg->iovcnt; i++)
    {
        rpc_msg->iov.iov_val[i].iov_base.iov_base_len =
            msg_iov->iov[i].iov_len;
        rpc_msg->iov.iov_val[i].iov_len = msg_iov->iov[i].iov_len;

        rpc_msg->iov.iov_val[i].iov_base.iov_base_val =
            TE_ALLOC(msg_iov->iov[i].iov_len);
        if (rpc_msg->iov.iov_val[i].iov_base.iov_base_val == NULL)
            return TE_RC(TE_TA_UNIX, TE_ENOMEM);

        /* Copying data, since zero-copy receive call sets pointers to
         * buffers with received data. */
        memcpy(rpc_msg->iov.iov_val[i].iov_base.iov_base_val,
               msg_iov->iov[i].iov_base,
               msg_iov->iov[i].iov_len);
    }

    return 0;
}

TARPC_FUNC(zfur_zc_recv,
{
    COPY_ARG_ADDR(msg);
},
{
    static rpc_ptr_id_namespace ns_zfur = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zfur_msg = RPC_PTR_ID_NS_INVALID;
    rpc_zfur_msg_iov *msg_iov = NULL;
    struct zfur *us;
    int rc;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur, RPC_TYPE_NS_ZFUR,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur_msg,
                                           RPC_TYPE_NS_ZFUR_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->urx, ns_zfur,);

    msg_iov = ALLOC_TE_ZFUR_MSG_IOV(out->msg.iovcnt);
    if (msg_iov == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        return;
    }

    rc = tarpc_zfur_msg_rpc2h(&out->msg, msg_iov);
    if (rc != 0)
    {
        out->common._errno = rc;
        free(msg_iov);
        return;
    }

    MAKE_CALL(func_ptr(us, &msg_iov->msg, zf_zc_flags_rpc2h(in->flags)));

    rc = tarpc_zfur_msg_h2rpc(msg_iov, &out->msg, TRUE);
    if (rc != 0 || out->msg.iovcnt <= 0)
    {
        free(msg_iov);
        out->common._errno = rc;
        return;
    }

    out->msg.ptr = RCF_PCH_MEM_INDEX_ALLOC(msg_iov, ns_zfur_msg);
})

/* See the function description in zf_udp.h */
TARPC_FUNC(zfur_zc_recv_done,
{
    COPY_ARG_ADDR(msg);
},
{
    static rpc_ptr_id_namespace ns_zfur = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zfur_msg = RPC_PTR_ID_NS_INVALID;
    rpc_zfur_msg_iov *msg_iov;
    struct zfur      *us;
    int rc;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur, RPC_TYPE_NS_ZFUR,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur_msg,
                                           RPC_TYPE_NS_ZFUR_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->urx, ns_zfur,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(msg_iov, out->msg.ptr, ns_zfur_msg,);

    rc = tarpc_zfur_msg_rpc2h(&out->msg, msg_iov);
    if (rc != 0)
    {
        out->common._errno = rc;
        out->retval = -1;
        return;
    }

    MAKE_CALL(func_ptr(us, &msg_iov->msg));

    RCF_PCH_MEM_INDEX_FREE(out->msg.ptr, ns_zfur_msg);

    rc = tarpc_zfur_msg_h2rpc(msg_iov, &out->msg, FALSE);
    if (rc != 0)
    {
        out->common._errno = rc;
        out->retval = -1;
    }
    free(msg_iov);
})

TARPC_FUNC(zfur_pkt_get_header, {},
{
    static rpc_ptr_id_namespace ns_zfur = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zfur_msg = RPC_PTR_ID_NS_INVALID;
    rpc_zfur_msg_iov *msg_iov;
    const struct iphdr  *iph;
    const struct udphdr *udph;
    struct zfur         *us;
    int ip_len;
    int udp_len;
    int rc;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur, RPC_TYPE_NS_ZFUR,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur_msg,
                                           RPC_TYPE_NS_ZFUR_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->urx, ns_zfur,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(msg_iov, in->msg.ptr, ns_zfur_msg,);

    rc = tarpc_zfur_msg_rpc2h(&in->msg, msg_iov);
    if (rc != 0)
    {
        out->common._errno = rc;
        out->retval = -1;
        return;
    }

    MAKE_CALL(out->retval = func_ptr(us, &msg_iov->msg, &iph, &udph,
                                     in->pktind));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    /* FIXME: Currently, ZF does not support IP options. However, in future
     * it is possible (though unlikely). To add IP options support, we need
     * calculate the length of IP header correctly here. */
    ip_len = sizeof(*iph);
    udp_len = sizeof(*udph);

    out->iphdr.iphdr_val = TE_ALLOC(ip_len);
    if (out->iphdr.iphdr_val == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        out->retval = -1;
        return;
    }
    memcpy(out->iphdr.iphdr_val, iph, ip_len);
    out->iphdr.iphdr_len = ip_len;

    out->udphdr.udphdr_val = TE_ALLOC(udp_len);
    if (out->udphdr.udphdr_val == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        out->retval = -1;
        return;
    }
    memcpy(out->udphdr.udphdr_val, udph, udp_len);
    out->udphdr.udphdr_len = udp_len;
})

/**
 * Do zero-copy receive using RX zocket and then send that data using TX
 * zocket.
 *
 * @param urx       UDP RX zocket.
 * @param utx       UDP TX zocket.
 * @param msg       UDP RX message to receive datagrams.
 * @param send_func Transmitting function.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
int
zfur_zc_recv_send(struct zfur *urx, struct zfut *utx,
                  zfts_send_function send_func, struct zfur_msg *msg)
{
    api_func_ptr rx_func;
    api_func_ptr func_send;
    api_func_ptr rx_done_func;
    int i = 0;
    int rc;

    if ((unsigned int)send_func >= ZFTS_ZFUT_SEND_INVALID)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_EINVAL),
                         "Invalid send function index: %d", send_func);
        return -1;
    }

    TARPC_FIND_FUNC_RETURN(FALSE, "zfur_zc_recv", (api_func *)&rx_func);

    rc = zfut_get_send_function(send_func, &func_send);
    if (rc != 0)
        return rc;

    TARPC_FIND_FUNC_RETURN(FALSE, "zfur_zc_recv_done",
                           (api_func *)&rx_done_func);

    rx_func(urx, msg, 0);

    if (send_func == ZFTS_ZFUT_SEND_SINGLE)
    {
        for (i = 0; i < msg->iovcnt; i++)
        {
            rc = func_send(utx, msg->iov[i].iov_base, msg->iov[i].iov_len);
            if (rc < 0)
            {
                ERROR("zfut_send_single() #%d failed, rc = %d (%r)", i, rc,
                      te_rc_os2te(-rc));

                te_rpc_error_set(TE_OS_RC(TE_RPC, -rc),
                                 "zfut_send_single() failed");
                rc = -1;
                break;
            }
        }
    }
    else
    {
        rc = func_send(utx, msg->iov, msg->iovcnt, 0);
        if (rc < 0)
        {
            te_rpc_error_set(TE_OS_RC(TE_RPC, -rc),
                             "zfut_send() failed");
            rc = -1;
        }
    }

    if (rc < 0)
    {
        WARN("Perform zfur_zc_recv_done() to release resources");
        rx_done_func(urx, msg);
        return rc;
    }

    return 0;
}

TARPC_FUNC_STATIC(zfur_zc_recv_send,
{
    COPY_ARG_ADDR(msg);
},
{
    static rpc_ptr_id_namespace ns_zfur = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zfut = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zfur_msg = RPC_PTR_ID_NS_INVALID;
    rpc_zfur_msg_iov *msg_iov = NULL;
    struct zfur *urx;
    struct zfut *utx;
    int rc;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur, RPC_TYPE_NS_ZFUR,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfut, RPC_TYPE_NS_ZFUT,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur_msg,
                                           RPC_TYPE_NS_ZFUR_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(urx, in->urx, ns_zfur,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(utx, in->utx, ns_zfut,);

    msg_iov = ALLOC_TE_ZFUR_MSG_IOV(out->msg.iovcnt);
    if (msg_iov == NULL)
    {
        out->common._errno = TE_RC(TE_TA_UNIX, TE_ENOMEM);
        out->retval = -1;
        return;
    }

    rc = tarpc_zfur_msg_rpc2h(&out->msg, msg_iov);
    if (rc != 0)
    {
        out->common._errno = rc;
        out->retval = -1;
        free(msg_iov);
        return;
    }

    MAKE_CALL(out->retval = func_ptr(urx, utx, in->send_func,
                                     &msg_iov->msg));

    if (out->retval != 0)
    {
        free(msg_iov);
        return;
    }

    rc = tarpc_zfur_msg_h2rpc(msg_iov, &out->msg, TRUE);
    if (rc != 0 || out->msg.iovcnt <= 0)
    {
        free(msg_iov);
        out->common._errno = rc;
        out->retval = -1;
        return;
    }

    out->msg.ptr = RCF_PCH_MEM_INDEX_ALLOC(msg_iov, ns_zfur_msg);
})

#define ZFUR_FLOODER_IOVCNT 5

/**
 * Repeatedly receive datagrams during a period of time.
 *
 * @param stack     Zetaferno stack.
 * @param urx       UDP RX zocket.
 * @param duration  How long receive datagrams, milliseconds.
 * @param stats     Sent data amount, bytes.
 *
 * @return @c Zero on success or a negative value in case of fail.
 */
int
zfur_flooder(struct zf_stack *stack, struct zfur *urx, int duration,
             uint64_t *stats)
{
    rpc_zfur_msg_iov *msg_iov = NULL;
    api_func_ptr func_zfur_zc_recv;
    api_func_ptr func_zfur_zc_recv_done;
    api_func_ptr process_events_func;
    struct timeval tv_start;
    struct timeval tv_now;
    uint64_t i;
    int iv;
    int rc;

    TARPC_FIND_FUNC_RETURN(FALSE, "zfur_zc_recv",
                           (api_func *)&func_zfur_zc_recv);
    TARPC_FIND_FUNC_RETURN(FALSE, "zfur_zc_recv_done",
                           (api_func *)&func_zfur_zc_recv_done);
    TARPC_FIND_FUNC_RETURN(FALSE, "zf_process_events",
                           (api_func *)&process_events_func);

    msg_iov = ALLOC_TE_ZFUR_MSG_IOV(ZFUR_FLOODER_IOVCNT);
    if (msg_iov == NULL)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_ENOMEM),
                         "Not enough memory for msg_iov");
        return -1;
    }

    *stats = 0;

    if ((rc = gettimeofday(&tv_start, NULL)) != 0)
    {
        te_rpc_error_set(TE_OS_RC(TE_TA_UNIX, errno),
                         "gettimeofday() failed");
        free(msg_iov);
        return -1;
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
            free(msg_iov);
            return -1;
        }

        msg_iov->msg.iovcnt = ZFUR_FLOODER_IOVCNT;
        func_zfur_zc_recv(urx, &msg_iov->msg, 0);

        if (msg_iov->msg.iovcnt > 0)
        {
            for (iv = 0; iv < msg_iov->msg.iovcnt; iv++)
                *stats += msg_iov->msg.iov[iv].iov_len;

            func_zfur_zc_recv_done(urx, &msg_iov->msg);
        }

        if ((rc = gettimeofday(&tv_now, NULL)) != 0)
        {
            te_rpc_error_set(TE_OS_RC(TE_TA_UNIX, errno),
                             "gettimeofday() failed");
            free(msg_iov);
            return -1;
        }

        if (TIMEVAL_SUB(tv_now, tv_start) / 1000L > duration)
            break;
    }

    free(msg_iov);
    return 0;
}

TARPC_FUNC_STATIC(zfur_flooder, {},
{
    static rpc_ptr_id_namespace ns_zfur = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;
    struct zf_stack *stack;
    struct zfur     *urx;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur, RPC_TYPE_NS_ZFUR,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(urx, in->urx, ns_zfur,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(out->retval = func_ptr(stack, urx, in->duration,
                                     &out->stats));
})

TARPC_FUNC(zfur_to_waitable, {},
{
    static rpc_ptr_id_namespace ns_zfur = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zf_w = RPC_PTR_ID_NS_INVALID;
    struct zf_waitable *waitable;
    struct zfur        *us;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur, RPC_TYPE_NS_ZFUR,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zf_w,
                                           RPC_TYPE_NS_ZF_WAITABLE,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->urx, ns_zfur,);

    MAKE_CALL(waitable = func_ptr(us));

    out->zf_waitable = RCF_PCH_MEM_INDEX_ALLOC(waitable, ns_zf_w);
})

TARPC_FUNC_STANDALONE(zfur_read_zfur_msg, {},
{
    static rpc_ptr_id_namespace ns_zfur_msg = RPC_PTR_ID_NS_INVALID;
    rpc_zfur_msg_iov *msg_iov = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur_msg,
                                           RPC_TYPE_NS_ZFUR_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(msg_iov, in->msg_ptr, ns_zfur_msg,);

    out->msg.reserved.reserved_len = TE_ARRAY_LEN(msg_iov->msg.reserved);
    out->msg.reserved.reserved_val =
        TE_ALLOC(TE_ARRAY_LEN(msg_iov->msg.reserved) *
                 sizeof(*out->msg.reserved.reserved_val));

    out->msg.iovcnt = msg_iov->msg.iovcnt;
    out->msg.iov.iov_len = 0;
    out->msg.iov.iov_val = NULL;

    out->common._errno = tarpc_zfur_msg_h2rpc(msg_iov, &out->msg, TRUE);
    out->msg.ptr = in->msg_ptr;
})

TARPC_FUNC(zfur_pkt_get_timestamp, {},
{
    static rpc_ptr_id_namespace ns_zfur = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zfur_msg = RPC_PTR_ID_NS_INVALID;
    rpc_zfur_msg_iov *msg_iov;
    struct zfur *us;

    unsigned int flags;
    struct timespec ts;
    int i;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur, RPC_TYPE_NS_ZFUR,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zfur_msg,
                                           RPC_TYPE_NS_ZFUR_MSG,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(us, in->urx, ns_zfur,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(msg_iov, in->msg_ptr, ns_zfur_msg,);

    INIT_CHECKED_ARG(msg_iov,
                     sizeof(*msg_iov) +
                          msg_iov->msg.iovcnt * sizeof(struct iovec),
                     0);
    for (i = 0; i < msg_iov->msg.iovcnt; i++)
    {
        INIT_CHECKED_ARG(msg_iov->iov[i].iov_base,
                         msg_iov->iov[i].iov_len, 0);
    }

    MAKE_CALL(out->retval = func_ptr(us, &msg_iov->msg, &ts,
                                     in->pktind, &flags));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    out->ts.tv_sec = ts.tv_sec;
    out->ts.tv_nsec = ts.tv_nsec;
    out->flags = zf_sync_flags_h2rpc(flags);
})
