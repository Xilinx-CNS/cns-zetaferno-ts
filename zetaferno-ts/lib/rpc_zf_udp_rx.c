/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions implementation
 *
 * Implementation of TAPI for Zetaferno Direct API remote
 * calls related to UDP RX zockets.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
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
#include "zf_test.h"

#include "rpc_zf_internal.h"
#include "rpc_zf_udp_rx.h"

#undef TE_LGR_USER
#define TE_LGR_USER "ZF TAPI UDP RX RPC"

/* See description in rpc_zf_udp_rx.h */
int
rpc_zfur_alloc(rcf_rpc_server *rpcs, rpc_zfur_p *us_out,
               rpc_zf_stack_p stack, rpc_zf_attr_p attr)
{
    tarpc_zfur_alloc_in  in;
    tarpc_zfur_alloc_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, attr, RPC_TYPE_NS_ZF_ATTR);
    in.attr = attr;

    rcf_rpc_call(rpcs, "zfur_alloc", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfur_alloc, out.retval);
    TAPI_RPC_LOG(rpcs, zfur_alloc, RPC_PTR_FMT ", " RPC_PTR_FMT ", "
                 RPC_PTR_FMT, "%d", RPC_PTR_VAL(out.urx),
                 RPC_PTR_VAL(attr), RPC_PTR_VAL(stack), out.retval);

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, out.urx, RPC_TYPE_NS_ZFUR);
    *us_out = out.urx;

    RETVAL_ZERO_INT(zfur_alloc, out.retval);
}

/* See description in rpc_zf_udp_rx.h */
int
rpc_zfur_free(rcf_rpc_server *rpcs, rpc_zfur_p urx)
{
    tarpc_zfur_free_in  in;
    tarpc_zfur_free_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, urx, RPC_TYPE_NS_ZFUR);
    in.urx = urx;

    rcf_rpc_call(rpcs, "zfur_free", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfur_free, out.retval);
    TAPI_RPC_LOG(rpcs, zfur_free, RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(urx), out.retval);

    RETVAL_ZERO_INT(zfur_free, out.retval);
}

/* See description in rpc_zf_udp_rx.h */
int
rpc_zfur_addr_bind_gen(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                       struct sockaddr *laddr,
                       socklen_t laddrlen, te_bool fwd_laddrlen,
                       const struct sockaddr *raddr,
                       socklen_t raddrlen, te_bool fwd_raddrlen,
                       int flags)
{
    tarpc_zfur_addr_bind_in  in;
    tarpc_zfur_addr_bind_out out;

    char loc_buf[TE_SOCKADDR_STR_LEN];
    char rem_buf[TE_SOCKADDR_STR_LEN];

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, urx, RPC_TYPE_NS_ZFUR);
    in.urx = urx;
    sockaddr_input_h2rpc(laddr, &in.laddr);
    in.laddrlen = laddrlen;
    in.fwd_laddrlen = fwd_laddrlen;
    sockaddr_input_h2rpc(raddr, &in.raddr);
    in.raddrlen = raddrlen;
    in.fwd_raddrlen = fwd_raddrlen;
    in.flags = flags;

    rcf_rpc_call(rpcs, "zfur_addr_bind", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfur_addr_bind, out.retval);
    /* TODO: print flags as a string. */
    TAPI_RPC_LOG(rpcs, zfur_addr_bind,
                 RPC_PTR_FMT ", " ZFTS_ADDR_FMT ", " ZFTS_ADDR_FMT ", %d",
                 "%d", RPC_PTR_VAL(urx),
                 ZFTS_ADDR_VAL_SBUF(laddr, loc_buf),
                 ZFTS_ADDR_VAL_SBUF(raddr, rem_buf),
                 flags, out.retval);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT && laddr != NULL)
    {
        /*
         * For other address families (in case we test that they
         * cannot be used) sockaddr_rpc2h() may fail with assertion
         * because of too small address length.
         */
        if (laddr->sa_family == AF_INET ||
            laddr->sa_family == AF_INET6)
            CHECK_RC(sockaddr_rpc2h(&out.laddr, laddr, sizeof(*laddr),
                                    NULL, NULL));
    }

    RETVAL_ZERO_INT(zfur_addr_bind, out.retval);
}

/* See description in rpc_zf_udp_rx.h */
int
rpc_zfur_addr_unbind_gen(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                         const struct sockaddr *laddr,
                         socklen_t laddrlen, te_bool fwd_laddrlen,
                         const struct sockaddr *raddr,
                         socklen_t raddrlen, te_bool fwd_raddrlen,
                         int flags)
{
    tarpc_zfur_addr_unbind_in  in;
    tarpc_zfur_addr_unbind_out out;

    char loc_buf[TE_SOCKADDR_STR_LEN];
    char rem_buf[TE_SOCKADDR_STR_LEN];

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, urx, RPC_TYPE_NS_ZFUR);
    in.urx = urx;
    sockaddr_input_h2rpc(laddr, &in.laddr);
    in.laddrlen = laddrlen;
    in.fwd_laddrlen = fwd_laddrlen;
    sockaddr_input_h2rpc(raddr, &in.raddr);
    in.raddrlen = raddrlen;
    in.fwd_raddrlen = fwd_raddrlen;
    in.flags = flags;

    rcf_rpc_call(rpcs, "zfur_addr_unbind", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfur_addr_unbind, out.retval);
    /* TODO: print flags as a string. */
    TAPI_RPC_LOG(rpcs, zfur_addr_unbind,
                 RPC_PTR_FMT ", " ZFTS_ADDR_FMT ", " ZFTS_ADDR_FMT ", %d",
                 "%d", RPC_PTR_VAL(urx),
                 ZFTS_ADDR_VAL_SBUF(laddr, loc_buf),
                 ZFTS_ADDR_VAL_SBUF(raddr, rem_buf),
                 flags, out.retval);

    RETVAL_ZERO_INT(zfur_addr_unbind, out.retval);
}

/**
 * Convert @c struct @c rpc_zfur_msg to string representation and save it in
 * TE string buffer @p str.
 *
 * @param rpcs  RPC server handle.
 * @param msg   ZF RX message structure.
 * @param str   TE string to keep the line.
 */
static void
rpc_zfur_msg2str(rcf_rpc_server *rpcs, rpc_zfur_msg *msg, te_string *str)
{
    int i;

    te_string_append(str, "{reserved[");

    for (i = 0; i < ZFUR_MSG_RESERVED; i++)
        te_string_append(str, "%s%d", (i == 0) ? "" : ", ",
                         msg->reserved[i]);

    te_string_append(str, "], dgrams_left = %d, flags = %d, iovcnt = %d, "
                     "iov = ", msg->dgrams_left, msg->flags, msg->iovcnt);

    rpc_iovec2str(msg->iov, msg->iovcnt, str);

    te_string_append(str, ", "RPC_PTR_FMT"}", RPC_PTR_VAL(msg->ptr));
}

/**
 * Convert @c struct @c rpc_zfur_msg to @c struct @c tarpc_zfur_msg.
 *
 * @param msg       The message structure to be converted.
 * @param tarp_msg  The result structure.
 */
static void
rpc_zfur_msg_rpc2tarpc(rpc_zfur_msg *msg, tarpc_zfur_msg *tarp_msg)
{
    tarp_msg->dgrams_left = msg->dgrams_left;
    tarp_msg->flags = msg->flags;
    tarp_msg->iovcnt = msg->iovcnt;
    tarp_msg->ptr = msg->ptr;
    tarp_msg->iov.iov_len = 0;
    tarp_msg->iov.iov_val = NULL;

    /* In the case of overlapped receive a lenght of the
     * first iovec is used to ensure arrival of requested bytes */
    if (msg->iov != NULL)
        tarp_msg->pftf_len = msg->iov[0].iov_len;

    tarp_msg->reserved.reserved_len = ZFUR_MSG_RESERVED;
    tarp_msg->reserved.reserved_val = msg->reserved;
}

/**
 * Convert @c struct @c tarpc_zfur_msg to @c struct @c rpc_zfur_msg.
 *
 * @param tarp_msg  The message structure to be converted.
 * @param msg       The result structure.
 * @param copy_data Copy vectors data if @c TRUE.
 */
static void
rpc_zfur_msg_tarpc2rpc(tarpc_zfur_msg *tarp_msg, rpc_zfur_msg *msg,
                       te_bool copy_data)
{
    int i;

    if (copy_data)
    {
        for (i = 0; i < tarp_msg->iovcnt && i < msg->iovcnt; i++)
        {
            msg->iov[i].iov_len = tarp_msg->iov.iov_val[i].iov_len;
            if (msg->iov[i].iov_len > msg->iov[i].iov_rlen)
                TEST_FAIL("One of returned iov buffers is larger then its "
                          "buffer size: %d/%d", msg->iov[i].iov_len,
                          msg->iov[i].iov_rlen);

            memcpy(msg->iov[i].iov_base,
                   tarp_msg->iov.iov_val[i].iov_base.iov_base_val,
                   msg->iov[i].iov_len);
        }
    }

    msg->dgrams_left = tarp_msg->dgrams_left;
    msg->flags = tarp_msg->flags;
    msg->iovcnt = tarp_msg->iovcnt;
    msg->ptr = tarp_msg->ptr;

    if (tarp_msg->reserved.reserved_len != ZFUR_MSG_RESERVED)
        TEST_FAIL("Incorrect length of the reserved array in structure "
                 "tarpc_zfur_msg");
    memcpy(msg->reserved, tarp_msg->reserved.reserved_val,
           ZFUR_MSG_RESERVED);
}

/* See description in rpc_zf_udp_rx.h */
void
rpc_zfur_zc_recv(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                 rpc_zfur_msg *msg, int flags)
{
    tarpc_zfur_zc_recv_in  in;
    tarpc_zfur_zc_recv_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, urx, RPC_TYPE_NS_ZFUR);
    in.urx = urx;
    in.flags = flags;

    rpc_zfur_msg_rpc2tarpc(msg, &in.msg);

    rcf_rpc_call(rpcs, "zfur_zc_recv", &in, &out);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
        rpc_zfur_msg_tarpc2rpc(&out.msg, msg, TRUE);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_zfur_msg2str(rpcs, msg, &log_strbuf);

    TAPI_RPC_LOG(rpcs, zfur_zc_recv, RPC_PTR_FMT ", %s, %s", "",
                 RPC_PTR_VAL(urx), log_strbuf.ptr,
                 zf_zc_flags_rpc2str(flags));


    RETVAL_VOID(zfur_zc_recv);
}

/* See description in rpc_zf_udp_rx.h */
int
rpc_zfur_zc_recv_done(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                      rpc_zfur_msg *msg)
{
    tarpc_zfur_zc_recv_done_in  in;
    tarpc_zfur_zc_recv_done_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, urx, RPC_TYPE_NS_ZFUR);
    in.urx = urx;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, msg->ptr, RPC_TYPE_NS_ZFUR_MSG);
    rpc_zfur_msg_rpc2tarpc(msg, &in.msg);

    rcf_rpc_call(rpcs, "zfur_zc_recv_done", &in, &out);
    free(in.msg.iov.iov_val);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfur_zc_recv_done, out.retval);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
        rpc_zfur_msg_tarpc2rpc(&out.msg, msg, FALSE);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_zfur_msg2str(rpcs, msg, &log_strbuf);
    TAPI_RPC_LOG(rpcs, zfur_zc_recv_done, RPC_PTR_FMT ", %s", "%d",
                 RPC_PTR_VAL(urx), log_strbuf.ptr, out.retval);

    RETVAL_ZERO_INT(zfur_zc_recv_done, out.retval);
}

/* See description in rpc_zf_udp_rx.h */
void
rpc_zfur_read_zfur_msg(rcf_rpc_server *rpcs, rpc_zfur_msg_p msg_ptr,
                       rpc_zfur_msg *msg)
{
    tarpc_zfur_read_zfur_msg_in  in;
    tarpc_zfur_read_zfur_msg_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, msg_ptr, RPC_TYPE_NS_ZFUR_MSG);
    in.msg_ptr = msg_ptr;

    rcf_rpc_call(rpcs, "zfur_read_zfur_msg", &in, &out);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
        rpc_zfur_msg_tarpc2rpc(&out.msg, msg, TRUE);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_zfur_msg2str(rpcs, msg, &log_strbuf);

    TAPI_RPC_LOG(rpcs, zfur_read_zfur_msg, RPC_PTR_FMT ", %s", "",
                 RPC_PTR_VAL(msg_ptr), log_strbuf.ptr);

    RETVAL_VOID(zfur_read_zfur_msg);
}


/**
 * Convert @b struct @b iphdr to string representation.
 *
 * @param iph   IP header structure.
 * @param str   TE string to keep the line.
 */
static void
tarpc_iphdr2str(const struct iphdr *iph, te_string *str)
{
    char addr_buf[INET_ADDRSTRLEN];

    te_string_append(str, "{ihl = %#x, version = %#x, tos = %#x, "
                     "tot_len = %d, id = %#x, frag_off = %#x, "
                     "ttl = %#x, protocol = %#x, check = %#x, ",
                     iph->ihl, iph->version, iph->tos, iph->tot_len,
                     iph->id, iph->frag_off,
                     iph->ttl, iph->protocol, iph->check,
                     iph->saddr, iph->daddr);

    inet_ntop(AF_INET, &iph->saddr, addr_buf, sizeof(addr_buf));
    te_string_append(str, "saddr = %s, ", addr_buf);
    inet_ntop(AF_INET, &iph->daddr, addr_buf, sizeof(addr_buf));
    te_string_append(str, "daddr = %s}", addr_buf);
}

/**
 * Convert @b struct @b udphdr to string representation.
 *
 * @param udph  UDP header structure.
 * @param str   TE string to keep the line.
 */
static void
tarpc_udphdr2str(const struct udphdr *udph, te_string *str)
{
    te_string_append(str, "{source = %#x, dest = %#x, len = %d, "
                     "check = %#x}", udph->source, udph->dest,
                     udph->len, udph->check);
}

/* See description in rpc_zf_udp_rx.h */
int
rpc_zfur_pkt_get_header(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                        rpc_zfur_msg *msg, struct iphdr *iph,
                        size_t iph_len, struct udphdr *udph, int pktind)
{
    tarpc_zfur_pkt_get_header_in  in;
    tarpc_zfur_pkt_get_header_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, urx, RPC_TYPE_NS_ZFUR);
    in.urx = urx;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, msg->ptr, RPC_TYPE_NS_ZFUR_MSG);
    rpc_zfur_msg_rpc2tarpc(msg, &in.msg);
    in.pktind = pktind;

    rcf_rpc_call(rpcs, "zfur_pkt_get_header", &in, &out);
    free(in.msg.iov.iov_val);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfur_pkt_get_header, out.retval);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_zfur_msg2str(rpcs, msg, &log_strbuf);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
    {
        if (iph != NULL)
        {
            if (iph_len < out.iphdr.iphdr_len)
                TEST_FAIL("IP header buffer length is too small");
            memcpy(iph, out.iphdr.iphdr_val, out.iphdr.iphdr_len);
        }

        if (udph != NULL)
            memcpy(udph, out.udphdr.udphdr_val, out.udphdr.udphdr_len);

        te_string_append(&log_strbuf, ", ");
        tarpc_iphdr2str((struct iphdr *)out.iphdr.iphdr_val, &log_strbuf);
        te_string_append(&log_strbuf, ", ");
        tarpc_udphdr2str((struct udphdr *)out.udphdr.udphdr_val, &log_strbuf);
    }

    TAPI_RPC_LOG(rpcs, zfur_pkt_get_header, RPC_PTR_FMT ", %s", "%d",
                 RPC_PTR_VAL(urx), log_strbuf.ptr, out.retval);

    RETVAL_ZERO_INT(zfur_pkt_get_header, out.retval);
}

/* See description in rpc_zf_udp_rx.h */
int
rpc_zfur_zc_recv_send(rcf_rpc_server *rpcs,rpc_zfur_p urx,
                      rpc_zfur_msg *msg, rpc_zfut_p utx,
                      zfts_send_function send_func)
{
    tarpc_zfur_zc_recv_send_in  in;
    tarpc_zfur_zc_recv_send_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, urx, RPC_TYPE_NS_ZFUR);
    in.urx = urx;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, utx, RPC_TYPE_NS_ZFUT);
    in.utx = utx;
    in.send_func = send_func;

    rpc_zfur_msg_rpc2tarpc(msg, &in.msg);

    rcf_rpc_call(rpcs, "zfur_zc_recv_send", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfur_zc_recv_send, out.retval);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
        rpc_zfur_msg_tarpc2rpc(&out.msg, msg, TRUE);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_zfur_msg2str(rpcs, msg, &log_strbuf);
    TAPI_RPC_LOG(rpcs, zfur_zc_recv_send, RPC_PTR_FMT ", %s, "
                 RPC_PTR_FMT, "%d", RPC_PTR_VAL(urx), log_strbuf.ptr,
                 RPC_PTR_VAL(utx), out.retval);

    RETVAL_ZERO_INT(zfur_zc_recv_send, out.retval);
}

/* See description in rpc_zf_udp_rx.h */
int
rpc_zfur_flooder(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                 rpc_zfur_p urx, int duration, uint64_t *stats)
{
    tarpc_zfur_flooder_in  in;
    tarpc_zfur_flooder_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, urx, RPC_TYPE_NS_ZFUR);
    in.urx = urx;
    in.duration = duration;

    rcf_rpc_call(rpcs, "zfur_flooder", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfur_flooder, out.retval);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT && stats != NULL)
        *stats = out.stats;

    TAPI_RPC_LOG(rpcs, zfur_flooder, RPC_PTR_FMT", "RPC_PTR_FMT
                 ",  %d, %llu", "%d", RPC_PTR_VAL(stack), RPC_PTR_VAL(urx),
                 duration, out.stats, out.retval);

    RETVAL_ZERO_INT(zfur_flooder, out.retval);
}

/* See description in rpc_zf_udp_rx.h */
rpc_zf_waitable_p
rpc_zfur_to_waitable(rcf_rpc_server *rpcs, rpc_zfur_p urx)
{
    tarpc_zfur_to_waitable_in  in;
    tarpc_zfur_to_waitable_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, urx, RPC_TYPE_NS_ZFUR);
    in.urx = urx;

    rcf_rpc_call(rpcs, "zfur_to_waitable", &in, &out);

    TAPI_RPC_LOG(rpcs, zfur_to_waitable, RPC_PTR_FMT, RPC_PTR_FMT,
                 RPC_PTR_VAL(urx), RPC_PTR_VAL(out.zf_waitable));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, out.zf_waitable,
                                  RPC_TYPE_NS_ZF_WAITABLE);
    CHECK_RETVAL_VAR(zfur_to_waitable, out.zf_waitable, FALSE, 0);

    RETVAL_RPC_PTR(zfur_to_waitable, out.zf_waitable);
}

/* See description in rpc_zf_udp_rx.h */
int
rpc_zfur_pkt_get_timestamp(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                           rpc_zfur_msg_p msg_ptr, tarpc_timespec *ts,
                           int pktind, unsigned int *flags)
{
    tarpc_zfur_pkt_get_timestamp_in in;
    tarpc_zfur_pkt_get_timestamp_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, urx, RPC_TYPE_NS_ZFUR);
    in.urx = urx;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, msg_ptr, RPC_TYPE_NS_ZFUR_MSG);
    in.msg_ptr = msg_ptr;

    in.pktind = pktind;

    rcf_rpc_call(rpcs, "zfur_pkt_get_timestamp", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zfur_pkt_get_timestamp,
                                          out.retval);

    if (ts != NULL)
        memcpy(ts, &out.ts, sizeof(*ts));
    if (flags != NULL)
        *flags = out.flags;

    TAPI_RPC_LOG(rpcs, zfur_pkt_get_timestamp,
                 RPC_PTR_FMT ", " RPC_PTR_FMT ", {%llu, %llu}, "
                 "%d, 0x%x (%s)", "%d",
                 RPC_PTR_VAL(urx), RPC_PTR_VAL(msg_ptr),
                 (long long unsigned)(out.ts.tv_sec),
                 (long long unsigned)(out.ts.tv_nsec),
                 pktind, out.flags, zf_sync_flags_rpc2str(out.flags),
                 out.retval);

    RETVAL_ZERO_INT(zfur_pkt_get_timestamp, out.retval);
}
