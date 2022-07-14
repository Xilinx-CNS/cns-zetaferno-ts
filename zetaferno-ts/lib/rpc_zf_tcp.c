/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions implementation
 *
 * Implementation of TAPI for Zetaferno Direct API remote
 * calls related to TCP zockets.
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
#include "rpc_zf_tcp.h"

#undef TE_LGR_USER
#define TE_LGR_USER "ZF TAPI TCP RPC"

/* See description in rpc_zf_tcp.h */
int
rpc_zftl_listen_gen(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                    const struct sockaddr *laddr,
                    socklen_t laddrlen, te_bool fwd_laddrlen,
                    rpc_zf_attr_p attr, rpc_zftl_p *tl_out)
{
    tarpc_zftl_listen_in    in;
    tarpc_zftl_listen_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    sockaddr_input_h2rpc(laddr, &in.laddr);
    in.laddrlen = laddrlen;
    in.fwd_laddrlen = fwd_laddrlen;

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, attr, RPC_TYPE_NS_ZF_ATTR);
    in.attr = attr;

    rcf_rpc_call(rpcs, "zftl_listen", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zftl_listen, out.retval);
    TAPI_RPC_LOG(rpcs, zftl_listen,
                 RPC_PTR_FMT ", " ZFTS_ADDR_FMT ", " RPC_PTR_FMT ", "
                 RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(stack), ZFTS_ADDR_VAL(laddr),
                 RPC_PTR_VAL(attr), RPC_PTR_VAL(out.tl), out.retval);

    if (RPC_IS_CALL_OK(rpcs) && out.retval >= 0)
    {
        if (TAPI_RPC_NAMESPACE_CHECK(rpcs, out.tl, RPC_TYPE_NS_ZFTL))
            RETVAL_INT(zftl_listen, -1);
    }

    *tl_out = out.tl;
    RETVAL_ZERO_INT(zftl_listen, out.retval);
}

/* See description in rpc_zf_tcp.h */
void
rpc_zftl_getname(rcf_rpc_server *rpcs, rpc_zftl_p tl,
                 struct sockaddr_in *laddr)
{
    tarpc_zftl_getname_in    in;
    tarpc_zftl_getname_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, tl, RPC_TYPE_NS_ZFTL);
    in.tl = tl;

    if (laddr != NULL)
        sockaddr_raw2rpc(laddr, sizeof(*laddr), &in.laddr);

    rcf_rpc_call(rpcs, "zftl_getname", &in, &out);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
    {
        if (sockaddr_rpc2h(&out.laddr, SA(laddr),
                           sizeof(*laddr), NULL, NULL) != 0)
            rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
    }

    TAPI_RPC_LOG(rpcs, zftl_getname, RPC_PTR_FMT ", %s", "",
                 RPC_PTR_VAL(tl),
                 sockaddr_h2str(SA(laddr)));
    RETVAL_VOID(zftl_getname);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zftl_accept(rcf_rpc_server *rpcs, rpc_zftl_p tl, rpc_zft_p *ts_out)
{
    tarpc_zftl_accept_in    in;
    tarpc_zftl_accept_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, tl, RPC_TYPE_NS_ZFTL);
    in.tl = tl;

    rcf_rpc_call(rpcs, "zftl_accept", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zftl_accept, out.retval);
    TAPI_RPC_LOG(rpcs, zftl_accept,
                 RPC_PTR_FMT ", " RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(tl), RPC_PTR_VAL(out.ts), out.retval);

    if (RPC_IS_CALL_OK(rpcs) && out.retval >= 0)
    {
        if (TAPI_RPC_NAMESPACE_CHECK(rpcs, out.ts, RPC_TYPE_NS_ZFT))
            RETVAL_INT(zftl_accept, -1);
    }

    *ts_out = out.ts;
    RETVAL_ZERO_INT(zftl_accept, out.retval);
}

/* See description in rpc_zf_tcp.h */
rpc_zf_waitable_p
rpc_zftl_to_waitable(rcf_rpc_server *rpcs, rpc_zftl_p tl)
{
    tarpc_zftl_to_waitable_in  in;
    tarpc_zftl_to_waitable_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, tl, RPC_TYPE_NS_ZFTL);
    in.tl = tl;

    rcf_rpc_call(rpcs, "zftl_to_waitable", &in, &out);

    CHECK_RETVAL_VAR(zftl_to_waitable, out.zf_waitable, FALSE, 0);
    TAPI_RPC_LOG(rpcs, zftl_to_waitable, RPC_PTR_FMT, RPC_PTR_FMT,
                 RPC_PTR_VAL(tl), RPC_PTR_VAL(out.zf_waitable));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, out.zf_waitable,
                                  RPC_TYPE_NS_ZF_WAITABLE);

    RETVAL_RPC_PTR(zftl_to_waitable, out.zf_waitable);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zftl_free(rcf_rpc_server *rpcs, rpc_zftl_p tl)
{
    tarpc_zftl_free_in    in;
    tarpc_zftl_free_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, tl, RPC_TYPE_NS_ZFTL);
    in.tl = tl;

    rcf_rpc_call(rpcs, "zftl_free", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zftl_free, out.retval);
    TAPI_RPC_LOG(rpcs, zftl_free,
                 RPC_PTR_FMT, "%d", RPC_PTR_VAL(tl), out.retval);

    RETVAL_ZERO_INT(zftl_free, out.retval);
}

/* See description in rpc_zf_tcp.h */
rpc_zf_waitable_p
rpc_zft_to_waitable(rcf_rpc_server *rpcs, rpc_zft_p ts)
{
    tarpc_zft_to_waitable_in  in;
    tarpc_zft_to_waitable_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;

    rcf_rpc_call(rpcs, "zft_to_waitable", &in, &out);

    CHECK_RETVAL_VAR(zft_to_waitable, out.zf_waitable, FALSE, 0);
    TAPI_RPC_LOG(rpcs, zft_to_waitable, RPC_PTR_FMT, RPC_PTR_FMT,
                 RPC_PTR_VAL(ts), RPC_PTR_VAL(out.zf_waitable));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, out.zf_waitable,
                                  RPC_TYPE_NS_ZF_WAITABLE);

    RETVAL_RPC_PTR(zft_to_waitable, out.zf_waitable);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_alloc(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
              rpc_zf_attr_p attr, rpc_zft_handle_p *handle_out)
{
    tarpc_zft_alloc_in    in;
    tarpc_zft_alloc_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, attr, RPC_TYPE_NS_ZF_ATTR);
    in.attr = attr;

    rcf_rpc_call(rpcs, "zft_alloc", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_alloc, out.retval);
    TAPI_RPC_LOG(rpcs, zft_alloc,
                 RPC_PTR_FMT ", " RPC_PTR_FMT ", " RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(stack), RPC_PTR_VAL(attr),
                 RPC_PTR_VAL(out.handle),
                 out.retval);

    if (RPC_IS_CALL_OK(rpcs) && out.retval >= 0)
    {
        if (TAPI_RPC_NAMESPACE_CHECK(rpcs, out.handle,
                                     RPC_TYPE_NS_ZFT_HANDLE))
            RETVAL_INT(zft_alloc, -1);
    }

    *handle_out = out.handle;
    RETVAL_ZERO_INT(zft_alloc, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_addr_bind_gen(rcf_rpc_server *rpcs, rpc_zft_handle_p handle,
                      const struct sockaddr *laddr,
                      socklen_t laddrlen, te_bool fwd_laddrlen, int flags)
{
    tarpc_zft_addr_bind_in  in;
    tarpc_zft_addr_bind_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, handle, RPC_TYPE_NS_ZFT_HANDLE);
    in.handle = handle;
    sockaddr_input_h2rpc(laddr, &in.laddr);
    in.laddrlen = laddrlen;
    in.fwd_laddrlen = fwd_laddrlen;
    in.flags = flags;

    rcf_rpc_call(rpcs, "zft_addr_bind", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_addr_bind, out.retval);
    /* TODO: print flags as a string. */
    TAPI_RPC_LOG(rpcs, zft_addr_bind, RPC_PTR_FMT ", "
                 ZFTS_ADDR_FMT ", %d", "%d",
                 RPC_PTR_VAL(handle), ZFTS_ADDR_VAL(laddr),
                 flags, out.retval);

    RETVAL_ZERO_INT(zft_addr_bind, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_connect_gen(rcf_rpc_server *rpcs, rpc_zft_handle_p handle,
                    const struct sockaddr *raddr, socklen_t raddrlen,
                    te_bool fwd_raddrlen, rpc_zft_p *ts_out)
{
    tarpc_zft_connect_in  in;
    tarpc_zft_connect_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, handle, RPC_TYPE_NS_ZFT_HANDLE);
    in.handle = handle;
    sockaddr_input_h2rpc(raddr, &in.raddr);
    in.raddrlen = raddrlen;
    in.fwd_raddrlen = fwd_raddrlen;

    rcf_rpc_call(rpcs, "zft_connect", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_connect, out.retval);
    /* TODO: print flags as a string. */
    TAPI_RPC_LOG(rpcs, zft_connect,
                 RPC_PTR_FMT ", " ZFTS_ADDR_FMT ", " RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(handle), ZFTS_ADDR_VAL(raddr),
                 RPC_PTR_VAL(out.ts),
                 out.retval);

    if (RPC_IS_CALL_OK(rpcs) && out.retval >= 0)
    {
        if (TAPI_RPC_NAMESPACE_CHECK(rpcs, out.ts, RPC_TYPE_NS_ZFT))
            RETVAL_INT(zft_connect, -1);
    }

    *ts_out = out.ts;
    RETVAL_ZERO_INT(zft_connect, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_shutdown_tx(rcf_rpc_server *rpcs, rpc_zft_p ts)
{
    tarpc_zft_shutdown_tx_in    in;
    tarpc_zft_shutdown_tx_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;

    rcf_rpc_call(rpcs, "zft_shutdown_tx", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_shutdown_tx, out.retval);
    TAPI_RPC_LOG(rpcs, zft_shutdown_tx,
                 RPC_PTR_FMT, "%d", RPC_PTR_VAL(ts), out.retval);

    RETVAL_ZERO_INT(zft_shutdown_tx, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_handle_free(rcf_rpc_server *rpcs, rpc_zft_handle_p handle)
{
    tarpc_zft_handle_free_in    in;
    tarpc_zft_handle_free_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, handle, RPC_TYPE_NS_ZFT_HANDLE);
    in.handle = handle;

    rcf_rpc_call(rpcs, "zft_handle_free", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_handle_free, out.retval);
    TAPI_RPC_LOG(rpcs, zft_handle_free,
                 RPC_PTR_FMT, "%d", RPC_PTR_VAL(handle), out.retval);

    RETVAL_ZERO_INT(zft_handle_free, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_free(rcf_rpc_server *rpcs, rpc_zft_p ts)
{
    tarpc_zft_free_in    in;
    tarpc_zft_free_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;

    rcf_rpc_call(rpcs, "zft_free", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_free, out.retval);
    TAPI_RPC_LOG(rpcs, zft_free,
                 RPC_PTR_FMT, "%d", RPC_PTR_VAL(ts), out.retval);

    RETVAL_ZERO_INT(zft_free, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_state(rcf_rpc_server *rpcs, rpc_zft_p ts)
{
    tarpc_zft_state_in    in;
    tarpc_zft_state_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;

    rcf_rpc_call(rpcs, "zft_state", &in, &out);

    CHECK_RETVAL_VAR(zft_state, out.retval, out.retval < 0, -1);
    TAPI_RPC_LOG(rpcs, zft_state, RPC_PTR_FMT, "%s",
                 RPC_PTR_VAL(ts), tcp_state_rpc2str(out.retval));

    RETVAL_INT(zft_state, out.retval);
}

/* See description in rpc_zf_tcp.h */
rpc_errno
rpc_zft_error(rcf_rpc_server *rpcs, rpc_zft_p ts)
{
    tarpc_zft_error_in    in;
    tarpc_zft_error_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;

    rcf_rpc_call(rpcs, "zft_error", &in, &out);

    CHECK_RETVAL_VAR(zft_error, out.retval, out.retval < 0, -1);
    TAPI_RPC_LOG(rpcs, zft_error, RPC_PTR_FMT, "%s",
                 RPC_PTR_VAL(ts), errno_rpc2str(out.retval));

    RETVAL_INT(zft_error, out.retval);
}

/* See description in rpc_zf_tcp.h */
void
rpc_zft_handle_getname(rcf_rpc_server *rpcs, rpc_zft_handle_p handle,
                       struct sockaddr_in *laddr)
{
    tarpc_zft_handle_getname_in    in;
    tarpc_zft_handle_getname_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, handle, RPC_TYPE_NS_ZFT_HANDLE);
    in.handle = handle;

    if (laddr != NULL)
        sockaddr_raw2rpc(laddr, sizeof(*laddr), &in.laddr);

    rcf_rpc_call(rpcs, "zft_handle_getname", &in, &out);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
    {
        if (sockaddr_rpc2h(&out.laddr, SA(laddr),
                           sizeof(*laddr), NULL, NULL) != 0)
            rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
    }

    TAPI_RPC_LOG(rpcs, zft_handle_getname, RPC_PTR_FMT ", %s", "",
                 RPC_PTR_VAL(handle),
                 sockaddr_h2str(SA(laddr)));
    RETVAL_VOID(zft_handle_getname);
}

/* See description in rpc_zf_tcp.h */
void
rpc_zft_getname(rcf_rpc_server *rpcs, rpc_zft_p ts,
                struct sockaddr_in *laddr, struct sockaddr_in *raddr)
{
    tarpc_zft_getname_in    in;
    tarpc_zft_getname_out   out;

    char loc_buf[TE_SOCKADDR_STR_LEN];
    char rem_buf[TE_SOCKADDR_STR_LEN];

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;

    if (laddr != NULL)
        sockaddr_raw2rpc(laddr, sizeof(*laddr), &in.laddr);
    if (raddr != NULL)
        sockaddr_raw2rpc(raddr, sizeof(*raddr), &in.raddr);

    rcf_rpc_call(rpcs, "zft_getname", &in, &out);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
    {
        if (sockaddr_rpc2h(&out.laddr, SA(laddr),
                           sizeof(*laddr), NULL, NULL) != 0 ||
            sockaddr_rpc2h(&out.raddr, SA(raddr),
                           sizeof(*raddr), NULL, NULL) != 0)
            rpcs->_errno = TE_RC(TE_RCF, TE_EINVAL);
    }

    TAPI_RPC_LOG(rpcs, zft_getname, RPC_PTR_FMT ", %s, %s", "",
                 RPC_PTR_VAL(ts),
                 SOCKADDR_H2STR_SBUF(SA(laddr), loc_buf),
                 SOCKADDR_H2STR_SBUF(SA(raddr), rem_buf));
    RETVAL_VOID(zft_getname);
}

/**
 * Convert @c struct @c rpc_zft_msg to @c struct @c tarpc_zft_msg.
 *
 * @param msg       The message structure to be converted.
 * @param tarp_msg  The result structure.
 */
static void
rpc_zft_msg_rpc2tarpc(rpc_zft_msg *msg, tarpc_zft_msg *tarp_msg)
{
    tarp_msg->pkts_left = msg->pkts_left;
    tarp_msg->flags = msg->flags;
    tarp_msg->iovcnt = msg->iovcnt;
    tarp_msg->ptr = msg->ptr;

    tarp_msg->iov.iov_val = NULL;
    tarp_msg->iov.iov_len = 0;

    /* In the case of overlapped receive a lenght of the
     * first iovec is used to ensure arrival of requested bytes */
    if (msg->iov != NULL)
        tarp_msg->pftf_len = msg->iov[0].iov_len;

    tarp_msg->reserved.reserved_len = ZFT_MSG_RESERVED;
    tarp_msg->reserved.reserved_val = msg->reserved;
}

/**
 * Convert @c struct @c tarpc_zft_msg to @c struct @c rpc_zft_msg.
 *
 * @param tarp_msg   The message structure to be converted.
 * @param msg        The result structure.
 * @param copy_data  Copy vectors data if @c TRUE.
 */
static void
rpc_zft_msg_tarpc2rpc(tarpc_zft_msg *tarp_msg, rpc_zft_msg *msg,
                      te_bool copy_data)
{
    int i;

    if (copy_data)
    {
        for (i = 0; i < tarp_msg->iovcnt && i < msg->iovcnt; i++)
        {
            msg->iov[i].iov_len = tarp_msg->iov.iov_val[i].iov_len;
            msg->iov[i].iov_rlen = tarp_msg->iov.iov_val[i].iov_len;

            free(msg->iov[i].iov_base);
            msg->iov[i].iov_base = TE_ALLOC(msg->iov[i].iov_len);
            if (msg->iov[i].iov_base == NULL)
                TEST_FAIL("Out of memory");

            memcpy(msg->iov[i].iov_base,
                   tarp_msg->iov.iov_val[i].iov_base.iov_base_val,
                   msg->iov[i].iov_len);
        }
    }

    msg->pkts_left = tarp_msg->pkts_left;
    msg->flags = tarp_msg->flags;
    msg->iovcnt = tarp_msg->iovcnt;
    msg->ptr = tarp_msg->ptr;

    if (tarp_msg->reserved.reserved_len != ZFT_MSG_RESERVED)
        TEST_FAIL("Incorrect length of the reserved array in structure "
                  "tarpc_zft_msg");
    memcpy(msg->reserved, tarp_msg->reserved.reserved_val,
           ZFT_MSG_RESERVED);
}

/**
 * Convert @c struct @c rpc_zft_msg to string representation and save it in
 * TE string buffer @p str.
 *
 * @param rpcs  RPC server handle.
 * @param msg   ZF TCP message structure.
 * @param str   TE string to keep the line.
 */
static void
rpc_zft_msg2str(rcf_rpc_server *rpcs, rpc_zft_msg *msg, te_string *str)
{
    int i;

    te_string_append(str, "{reserved[");

    for (i = 0; i < ZFT_MSG_RESERVED; i++)
        te_string_append(str, "%s%d", (i == 0) ? "" : ", ",
                         msg->reserved[i]);

    te_string_append(str, "], pkts_left = %d, flags = %d, iovcnt = %d, "
                     "iov = ", msg->pkts_left, msg->flags, msg->iovcnt);

    rpc_iovec2str(msg->iov, msg->iovcnt, str);

    te_string_append(str, ", "RPC_PTR_FMT"}", RPC_PTR_VAL(msg->ptr));
}

/* See description in rpc_zf_tcp.h */
void
rpc_zft_zc_recv(rcf_rpc_server *rpcs, rpc_zft_p ts,
                rpc_zft_msg *msg, int flags)
{
    tarpc_zft_zc_recv_in  in;
    tarpc_zft_zc_recv_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;
    in.flags = flags;

    rpc_zft_msg_rpc2tarpc(msg, &in.msg);

    rcf_rpc_call(rpcs, "zft_zc_recv", &in, &out);
    free(in.msg.iov.iov_val);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
        rpc_zft_msg_tarpc2rpc(&out.msg, msg, TRUE);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_zft_msg2str(rpcs, msg, &log_strbuf);

    TAPI_RPC_LOG(rpcs, zft_zc_recv, RPC_PTR_FMT ", %s, %s", "",
                 RPC_PTR_VAL(ts), log_strbuf.ptr,
                 zf_zc_flags_rpc2str(flags));


    RETVAL_VOID(zft_zc_recv);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_zc_recv_done(rcf_rpc_server *rpcs, rpc_zft_p ts,
                     rpc_zft_msg *msg)
{
    tarpc_zft_zc_recv_done_in  in;
    tarpc_zft_zc_recv_done_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, msg->ptr, RPC_TYPE_NS_ZFT_MSG);
    rpc_zft_msg_rpc2tarpc(msg, &in.msg);

    rcf_rpc_call(rpcs, "zft_zc_recv_done", &in, &out);
    free(in.msg.iov.iov_val);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zft_zc_recv_done, out.retval);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
        rpc_zft_msg_tarpc2rpc(&out.msg, msg, FALSE);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_zft_msg2str(rpcs, msg, &log_strbuf);
    TAPI_RPC_LOG(rpcs, zft_zc_recv_done, RPC_PTR_FMT ", %s", "%d",
                 RPC_PTR_VAL(ts), log_strbuf.ptr, out.retval);

    RETVAL_INT(zft_zc_recv_done, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_zc_recv_done_some(rcf_rpc_server *rpcs, rpc_zft_p ts,
                          rpc_zft_msg *msg, size_t len)
{
    tarpc_zft_zc_recv_done_some_in  in;
    tarpc_zft_zc_recv_done_some_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, msg->ptr, RPC_TYPE_NS_ZFT_MSG);
    rpc_zft_msg_rpc2tarpc(msg, &in.msg);
    in.len = len;

    rcf_rpc_call(rpcs, "zft_zc_recv_done_some", &in, &out);
    free(in.msg.iov.iov_val);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zft_zc_recv_done_some, out.retval);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
        rpc_zft_msg_tarpc2rpc(&out.msg, msg, FALSE);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_zft_msg2str(rpcs, msg, &log_strbuf);
    TAPI_RPC_LOG(rpcs, zft_zc_recv_done_some,
                 RPC_PTR_FMT ", %s, %"TE_PRINTF_SIZE_T"u", "%d",
                 RPC_PTR_VAL(ts), log_strbuf.ptr, len, out.retval);

    RETVAL_INT(zft_zc_recv_done_some, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_recv(rcf_rpc_server *rpcs, rpc_zft_p ts, rpc_iovec *iov,
             int iovcnt, int flags)
{
    tarpc_zft_recv_in  in;
    tarpc_zft_recv_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;
    in.iovcnt = in.iovec.iovec_len = iovcnt;
    in.flags = flags;
    in.iovec.iovec_val = TE_ALLOC(sizeof(*in.iovec.iovec_val) *
                                  in.iovec.iovec_len);
    if (in.iovec.iovec_len > 0 && in.iovec.iovec_val == NULL)
    {
        rpcs->_errno = TE_RC(TE_RCF, TE_ENOMEM);
        RETVAL_INT(zft_recv, -1);
    }

    rpc_iovec2tarpc_iovec(iov, in.iovec.iovec_val, iovcnt);

    rcf_rpc_call(rpcs, "zft_recv", &in, &out);
    free(in.iovec.iovec_val);

    tarpc_iovec2rpc_iovec(out.iovec.iovec_val, iov, iovcnt);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_iovec2str(iov, iovcnt, &log_strbuf);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zft_recv, out.retval);
    TAPI_RPC_LOG(rpcs, zft_recv, RPC_PTR_FMT ", iov = %s, iovcnt = %d, "
                 "flags %d", "%d", RPC_PTR_VAL(ts), log_strbuf.ptr,
                 iovcnt, flags, out.retval);

    RETVAL_INT(zft_recv, out.retval);
}

/* See description in rpc_zf_tcp.h */
void
rpc_zft_read_zft_msg(rcf_rpc_server *rpcs, rpc_zft_msg_p msg_ptr,
                     rpc_zft_msg *msg)
{
    tarpc_zft_read_zft_msg_in  in;
    tarpc_zft_read_zft_msg_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, msg_ptr, RPC_TYPE_NS_ZFT_MSG);
    in.msg_ptr = msg_ptr;

    rcf_rpc_call(rpcs, "zft_read_zft_msg", &in, &out);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
        rpc_zft_msg_tarpc2rpc(&out.msg, msg, TRUE);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_zft_msg2str(rpcs, msg, &log_strbuf);

    TAPI_RPC_LOG(rpcs, zft_read_zft_msg, RPC_PTR_FMT ", %s", "",
                 RPC_PTR_VAL(msg_ptr), log_strbuf.ptr);

    RETVAL_VOID(zft_read_zft_msg);
}

/* See description in rpc_zf_tcp.h */
ssize_t
rpc_zft_send(rcf_rpc_server *rpcs, rpc_zft_p ts, rpc_iovec *iov,
             size_t iovcnt, int flags)
{
    tarpc_zft_send_in  in;
    tarpc_zft_send_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;
    in.iovcnt = in.iovec.iovec_len = iovcnt;
    in.flags = flags;
    in.iovec.iovec_val = TE_ALLOC(sizeof(*in.iovec.iovec_val) *
                                  in.iovec.iovec_len);

    rpc_iovec2tarpc_iovec(iov, in.iovec.iovec_val, iovcnt);

    rcf_rpc_call(rpcs, "zft_send", &in, &out);
    free(in.iovec.iovec_val);

    te_string_cut(&log_strbuf, log_strbuf.len);
    rpc_iovec2str(iov, iovcnt, &log_strbuf);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zft_send, out.retval);
    TAPI_RPC_LOG(rpcs, zft_send, RPC_PTR_FMT ", iov = %s, iovcnt = %d, "
                 "flags = %s", "%"TE_PRINTF_SIZE_T"d",
                 RPC_PTR_VAL(ts), log_strbuf.ptr,
                 iovcnt, send_recv_flags_rpc2str(flags),
                 (ssize_t)out.retval);

    RETVAL_INT64(zft_send, out.retval);
}

/* See description in rpc_zf_tcp.h */
ssize_t
rpc_zft_send_single(rcf_rpc_server *rpcs, rpc_zft_p ts,
                    const void *buf, size_t buflen,
                    int flags)
{
    tarpc_zft_send_single_in  in;
    tarpc_zft_send_single_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;
    in.buf.buf_val = (uint8_t *)buf;
    in.buf.buf_len = buflen;
    in.flags = flags;

    rcf_rpc_call(rpcs, "zft_send_single", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zft_send_single, out.retval);
    TAPI_RPC_LOG(rpcs, zft_send_single,
                 RPC_PTR_FMT ", buf = %p, buflen = %"
                 TE_PRINTF_SIZE_T"u, flags = %s",
                 "%"TE_PRINTF_SIZE_T"d",
                 RPC_PTR_VAL(ts), buf, buflen,
                 send_recv_flags_rpc2str(flags), out.retval);

    RETVAL_INT64(zft_send_single, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_send_space(rcf_rpc_server *rpcs, rpc_zft_p ts, size_t *size)
{
    tarpc_zft_send_space_in  in;
    tarpc_zft_send_space_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;

    rcf_rpc_call(rpcs, "zft_send_space", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_send_space, out.retval);
    TAPI_RPC_LOG(rpcs, zft_send_space,
                 RPC_PTR_FMT ", %" TE_PRINTF_SIZE_T"u", "%d",
                 RPC_PTR_VAL(ts), (size_t)out.size, out.retval);

    if (size != NULL)
        *size = out.size;

    RETVAL_INT(zft_send_space, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_get_mss(rcf_rpc_server *rpcs, rpc_zft_p ts)
{
    tarpc_zft_get_mss_in  in;
    tarpc_zft_get_mss_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;

    rcf_rpc_call(rpcs, "zft_get_mss", &in, &out);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zft_get_mss, out.retval);
    TAPI_RPC_LOG(rpcs, zft_get_mss,
                 RPC_PTR_FMT, "%d",
                 RPC_PTR_VAL(ts), out.retval);

    RETVAL_INT(zft_get_mss, out.retval);
}

/* See description in rpc_zf_tcp.h */
unsigned int
rpc_zft_get_header_size(rcf_rpc_server *rpcs, rpc_zft_p ts)
{
    tarpc_zft_get_header_size_in  in;
    tarpc_zft_get_header_size_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;

    rcf_rpc_call(rpcs, "zft_get_header_size", &in, &out);

    TAPI_RPC_LOG(rpcs, zft_get_header_size,
                 RPC_PTR_FMT, "%u",
                 RPC_PTR_VAL(ts), out.retval);

    TAPI_RPC_OUT(zft_get_header_size, FALSE);
    return out.retval;
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_pkt_get_timestamp(rcf_rpc_server *rpcs, rpc_zft_p tcp_z,
                          rpc_zft_msg_p msg_ptr, tarpc_timespec *ts,
                          int pktind, unsigned int *flags)
{
    tarpc_zft_pkt_get_timestamp_in in;
    tarpc_zft_pkt_get_timestamp_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, tcp_z, RPC_TYPE_NS_ZFT);
    in.tcp_z = tcp_z;
    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, msg_ptr, RPC_TYPE_NS_ZFT_MSG);
    in.msg_ptr = msg_ptr;

    in.pktind = pktind;

    rcf_rpc_call(rpcs, "zft_pkt_get_timestamp", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_pkt_get_timestamp,
                                          out.retval);

    if (ts != NULL)
        memcpy(ts, &out.ts, sizeof(*ts));
    if (flags != NULL)
        *flags = out.flags;

    TAPI_RPC_LOG(rpcs, zft_pkt_get_timestamp,
                 RPC_PTR_FMT ", " RPC_PTR_FMT ", " TE_PRINTF_TS_FMT
                 ", %d, 0x%x (%s)", "%d",
                 RPC_PTR_VAL(tcp_z), RPC_PTR_VAL(msg_ptr),
                 TE_PRINTF_TS_VAL(out.ts),
                 pktind, out.flags, zf_sync_flags_rpc2str(out.flags),
                 out.retval);

    RETVAL_ZERO_INT(zft_pkt_get_timestamp, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_get_tx_timestamps(rcf_rpc_server *rpcs, rpc_zft_p tz,
                          tarpc_zf_pkt_report *reports, int *count)
{
    tarpc_zft_get_tx_timestamps_in in;
    tarpc_zft_get_tx_timestamps_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, tz, RPC_TYPE_NS_ZFT);
    in.tz = tz;
    in.reports.reports_val = reports;
    in.reports.reports_len = *count;
    in.count = *count;

    rcf_rpc_call(rpcs, "zft_get_tx_timestamps", &in, &out);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
    {
        memcpy(reports, out.reports.reports_val,
               sizeof(tarpc_zf_pkt_report) * out.reports.reports_len);
        *count = out.count;
    }

    te_string_cut(&log_strbuf, log_strbuf.len);
    zf_pkt_reports_rpc2str(reports, *count, &log_strbuf);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_get_tx_timestamps,
                                          out.retval);
    TAPI_RPC_LOG(rpcs, zft_get_tx_timestamps, RPC_PTR_FMT ", %s, %d", "%d",
                 RPC_PTR_VAL(tz), log_strbuf.ptr, *count,
                 out.retval);

    RETVAL_ZERO_INT(zft_get_tx_timestamps, out.retval);
}

/* See the description in tapi_rpc_misc.h */
int
rpc_zft_read_all(rcf_rpc_server *rpcs, rpc_zft_p ts, te_dbuf *dbuf,
                 size_t *read)
{
    tarpc_zft_read_all_in  in;
    tarpc_zft_read_all_out out;
    te_errno rc;

    size_t read_len = 0;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.ts = ts;

    rcf_rpc_call(rpcs, "zft_read_all", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_read_all, out.retval);

    TAPI_RPC_LOG(rpcs, zft_read_all,
                 "ts = "RPC_PTR_FMT", buf = %p, "
                 "read = %"TE_PRINTF_SIZE_T"u", "%d",
                 RPC_PTR_VAL(ts), out.buf.buf_val, out.buf.buf_len,
                 out.retval);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
    {
        if (out.buf.buf_val != NULL && out.buf.buf_len != 0)
        {
            rc = te_dbuf_append(dbuf, out.buf.buf_val, out.buf.buf_len);
            if (rc != 0)
            {
                ERROR("Failed to save read data");
                RETVAL_INT(zft_read_all, -1);
            }

            read_len = out.buf.buf_len;
        }
    }

    if (read != NULL)
        *read = read_len;

    RETVAL_ZERO_INT(zft_read_all, out.retval);
}

/* See the description in tapi_rpc_misc.h */
int
rpc_zft_read_all_zc(rcf_rpc_server *rpcs, rpc_zft_p ts, te_dbuf *dbuf,
                    size_t *read)
{
    tarpc_zft_read_all_zc_in  in;
    tarpc_zft_read_all_zc_out out;
    te_errno rc;

    size_t read_len = 0;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.ts = ts;

    rcf_rpc_call(rpcs, "zft_read_all_zc", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_read_all_zc, out.retval);

    TAPI_RPC_LOG(rpcs, zft_read_all_zc,
                 "ts = "RPC_PTR_FMT", buf = %p, "
                 "read = %"TE_PRINTF_SIZE_T"u", "%d",
                 RPC_PTR_VAL(ts), out.buf.buf_val, out.buf.buf_len,
                 out.retval);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
    {
        if (out.buf.buf_val != NULL && out.buf.buf_len != 0)
        {
            rc = te_dbuf_append(dbuf, out.buf.buf_val, out.buf.buf_len);
            if (rc != 0)
            {
                ERROR("Failed to save read data");
                RETVAL_INT(zft_read_all_zc, -1);
            }

            read_len = out.buf.buf_len;
        }
    }

    if (read != NULL)
        *read = read_len;

    RETVAL_ZERO_INT(zft_read_all_zc, out.retval);
}

/* See description in rpc_zf_tcp.h */
int
rpc_zft_overfill_buffers(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                         rpc_zft_p ts, te_dbuf *dbuf)
{
    tarpc_zft_overfill_buffers_in  in;
    tarpc_zft_overfill_buffers_out out;
    te_errno rc;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, stack, RPC_TYPE_NS_ZF_STACK);
    in.stack = stack;
    in.ts = ts;

    if (rpcs->timeout == RCF_RPC_UNSPEC_TIMEOUT)
        rpcs->timeout = RCF_RPC_DEFAULT_TIMEOUT * 4;

    rcf_rpc_call(rpcs, "zft_overfill_buffers", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zft_overfill_buffers, out.retval);

    TAPI_RPC_LOG(rpcs, zft_overfill_buffers,
                 "stack = "RPC_PTR_FMT", ts = "RPC_PTR_FMT", buf = %p, "
                 "written = %"TE_PRINTF_SIZE_T"u", "%d",
                 RPC_PTR_VAL(stack), RPC_PTR_VAL(ts),
                 out.buf.buf_val, out.buf.buf_len, out.retval);

    if (RPC_IS_CALL_OK(rpcs) && rpcs->op != RCF_RPC_WAIT)
    {
        if (out.buf.buf_val != NULL && out.buf.buf_len != 0)
        {
            rc = te_dbuf_append(dbuf, out.buf.buf_val, out.buf.buf_len);
            if (rc != 0)
            {
                ERROR("Failed to save read data");
                RETVAL_INT(zft_overfill_buffers, -1);
            }
        }
    }

    RETVAL_ZERO_INT(zft_overfill_buffers, out.retval);
}
