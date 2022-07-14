/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Implementation of auxiliary test API for UDP TX zockets
 *
 * Implementation of auxiliary test API to work with UDP TX zockets.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

/* User name of the library which is used in logging. */
#define TE_LGR_USER     "ZF UDP TX TAPI"

#include "zetaferno_ts.h"
#include "zfts_zfut.h"

/**
 * Split a buffer to iov vector buffers.
 *
 * @param buf       Buffer to split.
 * @param length    The buffer length.
 * @param iov       Iov vectors array.
 * @param iovcnt    Vectors number.
 */
static void
zfts_zfut_cut_buffer_iov(void *buf, size_t length, rpc_iovec *iov,
                         size_t iovcnt)
{
    size_t i;
    int    offt = 0;

    for (i = 0; i < iovcnt; i++)
    {
        if (i == iovcnt - 1)
            iov[i].iov_len = iov[i].iov_rlen = length - offt;
        else
            iov[i].iov_len = iov[i].iov_rlen = rand_range(0, length - offt);
        iov[i].iov_base = iov[i].iov_len == 0 ? NULL : buf + offt;
        offt += iov[i].iov_len;
    }
}

/* See description in zfts_zfut.h */
void
zfts_zfut_buf_make(size_t iovcnt, size_t min, size_t max,
                   zfts_zfut_buf *buf)
{
    buf->data = te_make_buf(min, max, &buf->len);

    buf->iov = tapi_calloc(sizeof(*buf->iov), iovcnt);
    zfts_zfut_cut_buffer_iov(buf->data, buf->len, buf->iov, iovcnt);
    buf->iovcnt = iovcnt;
}

/* See description in zfts_zfut.h */
void
zfts_zfut_buf_make_param(te_bool large_buffer, te_bool few_iov,
                         zfts_zfut_buf *buf)
{
    int iovcnt = few_iov ? ZFTS_IOVCNT : 1;

    if (large_buffer)
        zfts_zfut_buf_make(iovcnt, ZFTS_DGRAM_MAX, ZFTS_DGRAM_LARGE, buf);
    else
        zfts_zfut_buf_make(iovcnt, 1, ZFTS_DGRAM_MAX, buf);
}

/* See description in zfts_zfut.h */
void
zfts_zfut_buf_release(zfts_zfut_buf *buf)
{
    free(buf->iov);
    free(buf->data);
}

/* See description in zfts_zfut.h */
int
zfts_zfut_send_no_check(rcf_rpc_server *rpcs, rpc_zfut_p utx, zfts_zfut_buf *snd,
                        zfts_send_function send_f)
{
    int rc;

    switch (send_f)
    {
        case ZFTS_ZFUT_SEND:
            rc = rpc_zfut_send(rpcs, utx, snd->iov, snd->iovcnt, 0);
            break;

        case ZFTS_ZFUT_SEND_SINGLE:
            rc = rpc_zfut_send_single(rpcs, utx, snd->data, snd->len);
            break;

        default:
            TEST_FAIL("Invalid send function index: %d", send_f);
    }

    return rc;
}

/* See description in zfts_zfut.h */
int
zfts_zfut_send(rcf_rpc_server *rpcs, rpc_zfut_p utx, zfts_zfut_buf *snd,
               zfts_send_function send_f)
{
    int rc;

    rc = zfts_zfut_send_no_check(rpcs, utx, snd, send_f);

    if (rc != (int)snd->len)
        TEST_VERDICT("Data was sent incompletely");

    return rc;
}

/* See description in zfts_zfut.h */
int
zfts_zfut_send_ext(rcf_rpc_server *rpcs, rpc_zf_stack_p stack, rpc_zfut_p utx,
                   zfts_zfut_buf *snd, zfts_send_function send_f)
{
    int rc;
    int i;

/* ST-2632: experimental value: usually two attempts are enough. */
#define ZFUT_SEND_EXT_NUM_ATTEMPTS 4
    for (i = 0; i < ZFUT_SEND_EXT_NUM_ATTEMPTS; i++)
    {
        RPC_AWAIT_IUT_ERROR(rpcs);
        rc = zfts_zfut_send_no_check(rpcs, utx, snd, send_f);

        if (rc < 0)
        {
            if (RPC_ERRNO(rpcs) != RPC_EAGAIN)
            {
                TEST_FAIL("zfts_zfut_send() returned unexpected"
                          " error: %r", RPC_ERRNO(rpcs));
            }
            else
            {
                RING("zfts_zfut_send() returned %r: call zf_reactor_perform() "
                "until it returns non-zero", RPC_ERRNO(rpcs));
                /*
                 * Always enable logging for the next two RPC calls: this will
                 * help to understand what happens after getting error EAGAIN
                 * if the test eventually fails.
                 */
                rpcs->silent = rpcs->silent_pass = FALSE;
                rpc_zf_wait_for_event(rpcs, stack);

                rpcs->silent = rpcs->silent_pass = FALSE;
            }
        }
        else if (rc != (int)snd->len)
        {
            TEST_VERDICT("Data was sent incompletely");
        }
        else
        {
            return rc;
        }
    }

    return zfts_zfut_send(rpcs, utx, snd, send_f);
#undef ZFUT_SEND_EXT_NUM_ATTEMPTS
}

/* See description in zfts_zfut.h */
te_errno
zfts_zfut_check_send_func_ext(rcf_rpc_server *pco_iut, rpc_zf_stack_p stack,
                              rpc_zfut_p utx, rcf_rpc_server *pco_tst,
                              int tst_s, zfts_send_function send_f,
                              te_bool large_buffer, te_bool few_iov,
                              te_bool print_verdict, size_t *len)
{
#define REPORT_ERROR(err_, msg_...) \
    do {                            \
        if (print_verdict)          \
        {                           \
            TEST_VERDICT(msg_);     \
        }                           \
        else                        \
        {                           \
            ERROR(msg_);            \
            ret = err_;             \
            goto cleanup;           \
        }                           \
    } while (0)

    void            *rcvbuf = NULL;
    zfts_zfut_buf    snd;
    size_t           rc;
    te_bool          readable = FALSE;
    te_errno         ret = 0;

    zfts_zfut_buf_make_param(large_buffer, few_iov, &snd);
    rcvbuf = tapi_calloc(snd.len, 1);

    zfts_zfut_send(pco_iut, utx, &snd, send_f);

    if (len != NULL)
        *len = snd.len;

    rpc_zf_process_events(pco_iut, stack);

    RPC_GET_READABILITY(readable, pco_tst, tst_s, TAPI_WAIT_NETWORK_DELAY);
    if (!readable)
        REPORT_ERROR(TE_ENODATA, "Tester socket is not readable");

    rc = rpc_recv(pco_tst, tst_s, rcvbuf, snd.len, 0);
    if (rc != snd.len)
        REPORT_ERROR(TE_ECORRUPTED, "Incomplete datagram was received");

    if (memcmp(rcvbuf, snd.data, snd.len) != 0)
        REPORT_ERROR(TE_ECORRUPTED, "Received and sent data are different");

cleanup:

    zfts_zfut_buf_release(&snd);
    free(rcvbuf);
    return ret;
#undef REPORT_ERROR
}

/* See description in zfts_zfut.h */
void
zfts_zfut_check_send_func(rcf_rpc_server *pco_iut, rpc_zf_stack_p stack,
                          rpc_zfut_p utx, rcf_rpc_server *pco_tst,
                          int tst_s, zfts_send_function send_f,
                          te_bool large_buffer, te_bool few_iov)
{
    zfts_zfut_check_send_func_ext(pco_iut, stack, utx, pco_tst, tst_s,
                                  send_f, large_buffer, few_iov, TRUE,
                                  NULL);
}
