/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions implementation
 *
 * Implementation of TAPI for Zetaferno Direct API remote
 * calls related to delegated sends.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#include "te_config.h"

#include "tapi_rpc_internal.h"
#include "zf_test.h"
#include "zf_talib_common.h"

#include "rpc_zf_internal.h"
#include "rpc_zf_ds.h"

#undef TE_LGR_USER
#define TE_LGR_USER "ZF TAPI DS RPC"

/**
 * Number of bytes enough to store string representation of struct zf_ds.
 */
#define ZFTS_ZF_DS_STR_LEN 1024

/* See description in rpc_zf_ds.h */
const char *
zf_delegated_send_rc_rpc2str(rpc_zf_delegated_send_rc rc)
{
#define CONVERT(_val) \
    case ZF_DELEGATED_SEND_RC_ ## _val: \
        return #_val;

    switch (rc)
    {
        CONVERT(OK);
        CONVERT(NOCWIN);
        CONVERT(NOWIN);
        CONVERT(BAD_SOCKET);
        CONVERT(SMALL_HEADER);
        CONVERT(SENDQ_BUSY);
        CONVERT(NOARP);

        default:
            return "<UNKNOWN>";
    }
#undef CONVERT
}

/* See description in rpc_zf_ds.h */
void
zf_ds_h2str_append(struct zf_ds *ds, te_string *str)
{
#define APPEND_FIELD(_field, _format) \
    do {                                                          \
        te_string_append(str, #_field "=" _format ", ",           \
                              ds->_field);                        \
    } while (0)

    te_string_append(str, "{ ");

    APPEND_FIELD(headers, "%p");
    APPEND_FIELD(headers_size, "%d");
    APPEND_FIELD(headers_len, "%d");
    APPEND_FIELD(mss, "%d");
    APPEND_FIELD(send_wnd, "%d");
    APPEND_FIELD(cong_wnd, "%d");
    APPEND_FIELD(delegated_wnd, "%d");
    APPEND_FIELD(tcp_seq_offset, "%d");
    APPEND_FIELD(ip_len_offset, "%d");
    APPEND_FIELD(ip_tcp_hdr_len, "%d");
    APPEND_FIELD(reserved, "%d");

    te_string_cut(str, 2);
    te_string_append(str, " }");
#undef APPEND_FIELD
}

/**
 * Convert struct zf_ds to tarpc_zf_ds before an RPC call.
 *
 * @param in        Pointer to struct zf_ds value.
 * @param out       Pointer to tarpc_zf_ds value.
 */
static void
zf_ds_h2tarpc(struct zf_ds *in, tarpc_zf_ds *out)
{
    ZFTS_COPY_ZF_DS_FIELDS(*out, *in);

    out->headers.headers_val = in->headers;
    out->headers.headers_len = in->headers_size;
}

/**
 * Convert tarpc_zf_ds to struct zf_ds after an RPC call.
 *
 * @param in        Pointer to tarpc_zf_ds value.
 * @param out       Pointer to struct zf_ds value.
 * @param rpcs      RPC server handle (so set errno in case of failure).
 * @param retval    Pointer to RPC return value (will be set to @c -1
 *                  in case of failure; may be @c NULL).
 */
static void
zf_ds_tarpc2h(tarpc_zf_ds *in, struct zf_ds *out,
              rcf_rpc_server *rpcs, int *retval)
{
    ZFTS_COPY_ZF_DS_FIELDS(*out, *in);

    if ((int)(in->headers.headers_len) > out->headers_size)
    {
        ERROR("%s(): returned headers are greater than available "
              "buffer", __FUNCTION__);
        if (retval != NULL)
            *retval = -1;
        rpcs->_errno = TE_RC(TE_TAPI, TE_ECORRUPTED);
    }
    else
    {
        memcpy(out->headers, in->headers.headers_val,
               in->headers.headers_len);
    }
}

/* See description in rpc_zf_ds.h */
rpc_zf_delegated_send_rc
rpc_zf_delegated_send_prepare(rcf_rpc_server *rpcs,
                              rpc_zft_p ts, int max_delegated_wnd,
                              int cong_wnd_override, unsigned int flags,
                              struct zf_ds *ds)
{
    te_string str = TE_STRING_INIT_STATIC(ZFTS_ZF_DS_STR_LEN);

    tarpc_zf_delegated_send_prepare_in in;
    tarpc_zf_delegated_send_prepare_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;
    in.max_delegated_wnd = max_delegated_wnd;
    in.cong_wnd_override = cong_wnd_override;
    in.flags = flags;
    zf_ds_h2tarpc(ds, &in.ds);

    rcf_rpc_call(rpcs, "zf_delegated_send_prepare", &in, &out);

    if (rpcs->op != RCF_RPC_WAIT)
        zf_ds_tarpc2h(&out.ds, ds, rpcs, &out.retval);

    CHECK_RETVAL_VAR(zf_delegated_send_prepare, out.retval,
                     (out.retval < 0), -1);

    zf_ds_h2str_append(ds, &str);
    TAPI_RPC_LOG(rpcs, zf_delegated_send_prepare,
                 RPC_PTR_FMT ", %d, %d, 0x%x, %p (%s)", "%d (%s)",
                 RPC_PTR_VAL(ts), max_delegated_wnd, cong_wnd_override,
                 flags, ds, str.ptr, out.retval,
                 zf_delegated_send_rc_rpc2str(out.retval));
    TAPI_RPC_OUT(zf_delegated_send_prepare,
                 (out.retval != ZF_DELEGATED_SEND_RC_OK));
    return out.retval;
}

/* See description in rpc_zf_ds.h */
void
rpc_zf_delegated_send_tcp_update(rcf_rpc_server *rpcs, struct zf_ds *ds,
                                 int bytes, int push)
{
    te_string str = TE_STRING_INIT_STATIC(ZFTS_ZF_DS_STR_LEN);

    tarpc_zf_delegated_send_tcp_update_in in;
    tarpc_zf_delegated_send_tcp_update_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.bytes = bytes;
    in.push = push;
    zf_ds_h2tarpc(ds, &in.ds);

    rcf_rpc_call(rpcs, "zf_delegated_send_tcp_update", &in, &out);

    if (rpcs->op != RCF_RPC_WAIT)
        zf_ds_tarpc2h(&out.ds, ds, rpcs, NULL);

    zf_ds_h2str_append(ds, &str);
    TAPI_RPC_LOG(rpcs, zf_delegated_send_tcp_update,
                 "%p (%s), %d, %d", "", ds, str.ptr, bytes, push);
    RETVAL_VOID(zf_delegated_send_tcp_update);
}

/* See description in rpc_zf_ds.h */
void
rpc_zf_delegated_send_tcp_advance(rcf_rpc_server *rpcs, struct zf_ds *ds,
                                  int bytes)
{
    te_string str = TE_STRING_INIT_STATIC(ZFTS_ZF_DS_STR_LEN);

    tarpc_zf_delegated_send_tcp_advance_in in;
    tarpc_zf_delegated_send_tcp_advance_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.bytes = bytes;
    zf_ds_h2tarpc(ds, &in.ds);

    rcf_rpc_call(rpcs, "zf_delegated_send_tcp_advance", &in, &out);

    if (rpcs->op != RCF_RPC_WAIT)
        zf_ds_tarpc2h(&out.ds, ds, rpcs, NULL);

    zf_ds_h2str_append(ds, &str);
    TAPI_RPC_LOG(rpcs, zf_delegated_send_tcp_advance,
                 "%p (%s), %d", "", ds, str.ptr, bytes);
    RETVAL_VOID(zf_delegated_send_tcp_advance);
}

/* See description in rpc_zf_ds.h */
int
rpc_zf_delegated_send_complete(rcf_rpc_server *rpcs, rpc_zft_p ts,
                               rpc_iovec *iov, int iovlen, int flags)
{
    te_string str = TE_STRING_INIT_STATIC(4096);

    struct tarpc_iovec *iovec_arr = NULL;
    int i;

    tarpc_zf_delegated_send_complete_in in;
    tarpc_zf_delegated_send_complete_out out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;
    in.flags = flags;
    in.iovlen = iovlen;

    if (iovlen > 0 && iov != NULL)
    {
        iovec_arr = tapi_calloc(iovlen, sizeof(*iovec_arr));
        for (i = 0; i < iovlen; i++)
        {
            iovec_arr[i].iov_base.iov_base_val = iov[i].iov_base;
            iovec_arr[i].iov_base.iov_base_len = iov[i].iov_rlen;
            iovec_arr[i].iov_len = iov[i].iov_len;

            if (!rpcs->silent)
            {
                if (str.len > 0)
                    te_string_append(&str, ", ");

                te_string_append(&str, "{ .iov_base=%p, .iov_len=%lu",
                                 iov[i].iov_base,
                                 (long unsigned int)(iov[i].iov_len));
                if (iov[i].iov_rlen != iov[i].iov_len)
                {
                    te_string_append(&str, ", .iov_rlen=%lu }",
                                     (long unsigned int)(iov[i].iov_rlen));
                }
                else
                {
                    te_string_append(&str, " }");
                }
            }
        }

        in.iov.iov_val = iovec_arr;
        in.iov.iov_len = iovlen;
    }

    rcf_rpc_call(rpcs, "zf_delegated_send_complete", &in, &out);
    free(iovec_arr);

    CHECK_RETVAL_VAR_IS_GTE_MINUS_ONE(zf_delegated_send_complete,
                                      out.retval);

    TAPI_RPC_LOG(rpcs, zf_delegated_send_complete,
                 RPC_PTR_FMT ", %p (%s), %d, 0x%x", "%d",
                 RPC_PTR_VAL(ts), iov, str.ptr, iovlen,
                 flags, out.retval);
    RETVAL_INT(zf_delegated_send_complete, out.retval);
}

/* See description in rpc_zf_ds.h */
int
rpc_zf_delegated_send_cancel(rcf_rpc_server *rpcs, rpc_zft_p ts)
{
    tarpc_zf_delegated_send_cancel_in    in;
    tarpc_zf_delegated_send_cancel_out   out;

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    TAPI_RPC_NAMESPACE_CHECK_JUMP(rpcs, ts, RPC_TYPE_NS_ZFT);
    in.ts = ts;

    rcf_rpc_call(rpcs, "zf_delegated_send_cancel", &in, &out);

    CHECK_RETVAL_VAR_IS_ZERO_OR_MINUS_ONE(zf_delegated_send_cancel,
                                          out.retval);
    TAPI_RPC_LOG(rpcs, zf_delegated_send_cancel,
                 RPC_PTR_FMT, "%d", RPC_PTR_VAL(ts), out.retval);

    RETVAL_ZERO_INT(zf_delegated_send_cancel, out.retval);
}
