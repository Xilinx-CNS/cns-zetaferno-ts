/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Implementation of auxiliary test API for TCP zockets
 *
 * Implementation of auxiliary test API to work with TCP zockets.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

/* User name of the library which is used in logging. */
#define TE_LGR_USER     "ZF TCP TAPI"

#include "zetaferno_ts.h"
#include "zfts_tcp.h"
#include "tapi_sockets.h"

/** Default ZF TCP receive function. */
static zfts_tcp_recv_func_t def_tcp_recv_func = ZFTS_TCP_RECV_ZFT_ZC_RECV;

/**
 * Handler used for zft_zc_recv_done() or zft_zc_recv_done_some()
 * result processing.
 */
static zfts_zft_recv_done_rc_handler def_zft_recv_done_rc_handler = NULL;

/* See description in zfts_tcp.h */
void
zfts_set_zft_recv_done_rc_handler(zfts_zft_recv_done_rc_handler h)
{
    def_zft_recv_done_rc_handler = h;
}

/**
 * Wrapper for rpc_zft_zc_recv_done() and
 * rpc_zft_zc_recv_done_some().
 *
 * @param rpcs        RPC server.
 * @param zock        RPC pointer of TCP zocket.
 * @param msg         ZF TCP message structure.
 * @param done_some   Whether to call rpc_zft_zc_recv_done_some()
 *                    or rpc_zft_zc_recv_done().
 * @param len         If rpc_zft_zc_recv_done_some() is called,
 *                    pass this as length argument.
 *
 * @return Return value of RPC call.
 */
static int
zfts_zft_zc_recv_done_wrapper(rcf_rpc_server *rpcs,
                              rpc_zft_p zock,
                              rpc_zft_msg *msg,
                              te_bool done_some,
                              size_t len)
{
    int rc;

    if (def_zft_recv_done_rc_handler != NULL)
        RPC_AWAIT_ERROR(rpcs);

    if (done_some)
        rc = rpc_zft_zc_recv_done_some(rpcs, zock, msg, len);
    else
        rc = rpc_zft_zc_recv_done(rpcs, zock, msg);

    if (def_zft_recv_done_rc_handler != NULL)
        def_zft_recv_done_rc_handler(zock, rc, RPC_ERRNO(rpcs));

    return rc;
}

/* See description in zfts_tcp.h */
void
zfts_connect_to_zftl(rcf_rpc_server *pco_iut,
                     rpc_zf_stack_p stack,
                     rpc_zftl_p iut_zftl,
                     rpc_zft_p *iut_zft,
                     const struct sockaddr *iut_addr,
                     rcf_rpc_server *pco_tst,
                     int *tst_s,
                     const struct sockaddr *tst_addr)
{
    if (*tst_s < 0)
    {
        *tst_s = rpc_socket(pco_tst,
                            rpc_socket_domain_by_addr(tst_addr),
                            RPC_SOCK_STREAM, RPC_PROTO_DEF);
    }
    if (tst_addr != NULL)
        rpc_bind(pco_tst, *tst_s, tst_addr);

    rpc_zf_process_events(pco_iut, stack);

    pco_tst->op = RCF_RPC_CALL;
    rpc_connect(pco_tst, *tst_s, iut_addr);
    ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);
    pco_tst->op = RCF_RPC_WAIT;
    rpc_connect(pco_tst, *tst_s, iut_addr);

    rpc_zftl_accept(pco_iut, iut_zftl, iut_zft);
}

/* See description in zfts_tcp.h */
void
zfts_establish_tcp_conn_ext(te_bool active,
                            rcf_rpc_server *pco_iut,
                            rpc_zf_attr_p attr,
                            rpc_zf_stack_p stack,
                            rpc_zftl_p iut_zftl,
                            rpc_zft_p *iut_zft,
                            const struct sockaddr *iut_addr,
                            rcf_rpc_server *pco_tst,
                            int *tst_s,
                            const struct sockaddr *tst_addr,
                            int tst_sndbuf, int tst_rcvbuf)
{
    if (active)
    {
        rpc_zft_handle_p  iut_zft_handle = RPC_NULL;
        int               tst_s_listening = -1;

        tst_s_listening =
          rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                     RPC_PROTO_DEF, FALSE, FALSE,
                                     tst_addr);
        if (tst_sndbuf >= 0)
            rpc_setsockopt_int(pco_tst, tst_s_listening,
                               RPC_SO_SNDBUF, tst_sndbuf);
        if (tst_rcvbuf >= 0)
            rpc_setsockopt_int(pco_tst, tst_s_listening,
                               RPC_SO_RCVBUF, tst_rcvbuf);

        rpc_listen(pco_tst, tst_s_listening, 1);

        rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);
        rpc_zft_addr_bind(pco_iut, iut_zft_handle, iut_addr, 0);
        rpc_zft_connect(pco_iut, iut_zft_handle, tst_addr, iut_zft);
        ZFTS_WAIT_PROCESS_EVENTS(pco_iut, stack);
        *tst_s = rpc_accept(pco_tst, tst_s_listening, NULL, NULL);

        rpc_close(pco_tst, tst_s_listening);
    }
    else
    {
        te_bool free_zftl = FALSE;

        *tst_s = rpc_socket(pco_tst,
                            rpc_socket_domain_by_addr(tst_addr),
                            RPC_SOCK_STREAM, RPC_PROTO_DEF);
        if (tst_sndbuf >= 0)
            rpc_setsockopt_int(pco_tst, *tst_s,
                               RPC_SO_SNDBUF, tst_sndbuf);
        if (tst_rcvbuf >= 0)
            rpc_setsockopt_int(pco_tst, *tst_s,
                               RPC_SO_RCVBUF, tst_rcvbuf);

        if (iut_zftl == RPC_NULL)
        {
            free_zftl = TRUE;
            rpc_zftl_listen(pco_iut, stack, iut_addr, attr,
                            &iut_zftl);
        }

        zfts_connect_to_zftl(pco_iut, stack, iut_zftl,
                             iut_zft, iut_addr,
                             pco_tst, tst_s, tst_addr);

        if (free_zftl)
            rpc_zftl_free(pco_iut, iut_zftl);
    }
}

/* See description in zfts_tcp.h */
void
zfts_establish_tcp_conn_ext2(zfts_conn_open_method open_method,
                             rcf_rpc_server *pco_iut,
                             rpc_zf_attr_p attr,
                             rpc_zf_stack_p stack,
                             rpc_zftl_p *iut_zftl,
                             rpc_zft_p *iut_zft,
                             const struct sockaddr *iut_addr,
                             rcf_rpc_server *pco_tst,
                             int *tst_s,
                             const struct sockaddr *tst_addr,
                             int tst_sndbuf, int tst_rcvbuf)
{
    rpc_zftl_p zftl_zock = RPC_NULL;

    if (open_method != ZFTS_CONN_OPEN_ACT)
        rpc_zftl_listen(pco_iut, stack, iut_addr,
                        attr, &zftl_zock);

    zfts_establish_tcp_conn_ext((open_method == ZFTS_CONN_OPEN_ACT),
                                pco_iut, attr, stack,
                                zftl_zock, iut_zft, iut_addr,
                                pco_tst, tst_s, tst_addr,
                                tst_sndbuf, tst_rcvbuf);

    if (open_method == ZFTS_CONN_OPEN_PAS_CLOSE)
        ZFTS_FREE(pco_iut, zftl, zftl_zock);

    if (iut_zftl != NULL)
        *iut_zftl = zftl_zock;
}

/* See description in zfts_tcp.h */
void
zfts_establish_tcp_conn(te_bool active,
                        rcf_rpc_server *pco_iut,
                        rpc_zf_attr_p attr,
                        rpc_zf_stack_p stack,
                        rpc_zft_p *iut_zft,
                        const struct sockaddr *iut_addr,
                        rcf_rpc_server *pco_tst,
                        int *tst_s,
                        const struct sockaddr *tst_addr)
{
    zfts_establish_tcp_conn_ext(active, pco_iut, attr, stack,
                                RPC_NULL, iut_zft, iut_addr,
                                pco_tst, tst_s, tst_addr, -1, -1);
}

/* See description in zfts_tcp.h */
int
zfts_zft_handle_bind(rcf_rpc_server *rpcs, rpc_zft_handle_p handle,
                     tapi_address_type local_addr_type,
                     const struct sockaddr *local_addr,
                     int flags)
{
    int rc;
    struct sockaddr *laddr = tapi_sockaddr_clone_typed(local_addr,
                                                       local_addr_type);

    /* It is possible to get memory leak if the next call fails. Do not care
     * about it because the call jumps to cleanup in this case. Then the
     * test application will be closed. */
    rc = rpc_zft_addr_bind(rpcs, handle, laddr, flags);

    free(laddr);

    return rc;
}

/* See description in zfts_tcp.h */
void
zfts_iovec_cmp_data(const rpc_iovec *iov, size_t iovcnt,
                    const char *data, ssize_t data_len)
{
    size_t  i;
    ssize_t data_pos = 0;

    if (data_len < 0)
        TEST_FAIL("Receiving function returned negative value");

    for (i = 0; i < iovcnt; i++)
    {
        if (data_len - data_pos < (ssize_t)iov[i].iov_len)
            TEST_FAIL("Data length is less than "
                      "total length of iovec buffers");
        if (memcmp(iov[i].iov_base, data + data_pos, iov[i].iov_len) != 0)
            TEST_FAIL("Data does not match iovec");

        data_pos += iov[i].iov_len;
    }

    if (data_len > data_pos)
        TEST_FAIL("Data length is greater than "
                  "total length of iovec buffers");
}

/* See description in zfts_tcp.h */
size_t
zfts_zft_check_sending(rcf_rpc_server *pco_iut,
                       rpc_zf_stack_p stack, rpc_zft_p iut_zft,
                       rcf_rpc_server *pco_tst, int tst_s)
{
    ssize_t rc;

    char       *data;
    size_t      data_len;
    size_t      sent_len;

    rpc_iovec sndiov[ZFTS_TCP_SEND_IOVCNT];

    data_len = ZFTS_TCP_SEND_IOVCNT * ZFTS_TCP_DATA_MAX;
    data = tapi_calloc(data_len, 1);

    rpc_make_iov(sndiov, ZFTS_TCP_SEND_IOVCNT, 1, ZFTS_TCP_DATA_MAX);

    sent_len = rpc_zft_send(pco_iut, iut_zft, sndiov,
                            ZFTS_TCP_SEND_IOVCNT, 0);
    rpc_zf_process_events(pco_iut, stack);

    rc = rpc_recv(pco_tst, tst_s, data, ZFTS_TCP_DATA_MAX, 0);
    zfts_iovec_cmp_data(sndiov, ZFTS_TCP_SEND_IOVCNT, data, rc);
    rpc_release_iov(sndiov, ZFTS_TCP_SEND_IOVCNT);
    free(data);

    return sent_len;
}

/* See description in zfts_tcp.h */
void
zfts_zft_check_receiving(rcf_rpc_server *pco_iut,
                         rpc_zf_stack_p stack, rpc_zft_p iut_zft,
                         rcf_rpc_server *pco_tst, int tst_s)
{
    char       *data;
    size_t      data_len;
    ssize_t     rc;
    int         failed_recv_attempts = 0;
    te_dbuf     received_data = TE_DBUF_INIT(0);

    data = te_make_buf(1,
                       ZFTS_IOVCNT * ZFTS_TCP_DATA_MAX,
                       &data_len);

    RPC_AWAIT_ERROR(pco_tst);
    rc = rpc_send(pco_tst, tst_s, data, data_len, 0);
    if (rc < 0)
        TEST_VERDICT("send() failed on Tester with errno %r",
                     RPC_ERRNO(pco_tst));
    else if (rc != (ssize_t)data_len)
        TEST_FAIL("Failed to send %u bytes", (unsigned int)data_len);

    rpc_zf_wait_for_event(pco_iut, stack);

    /*
     * Here receive function is called repeatedly in a loop
     * because sometimes Tester does not send all the data
     * at once and waits ACK to the part of data it sent before
     * sending the rest.
     */
    do {
        rpc_zf_process_events(pco_iut, stack);
        RPC_AWAIT_ERROR(pco_iut);
        rc = zfts_zft_recv_dbuf(pco_iut, iut_zft, &received_data);

        if (received_data.len >= data_len)
        {
            break;
        }
        else if (rc > 0)
        {
            failed_recv_attempts = 0;
        }
        else
        {
            failed_recv_attempts++;
            if (failed_recv_attempts > 1)
                break;
        }

        if (rc <= 0)
            ZFTS_WAIT_NETWORK(pco_iut, stack);
    } while (TRUE);

    ZFTS_CHECK_RECEIVED_DATA(received_data.ptr, data,
                             received_data.len, data_len,
                             " from peer socket");
    te_dbuf_free(&received_data);
    free(data);
}

/* See description in zfts_tcp.h */
void
zfts_zft_check_partial_receiving(rcf_rpc_server *pco_iut,
                                 rpc_zf_stack_p stack, rpc_zft_p iut_zft,
                                 rcf_rpc_server *pco_tst, int tst_s)
{
    char       *data;
    size_t      data_len;
    te_dbuf     received_data = TE_DBUF_INIT(0);
    ssize_t     rc;

    data = te_make_buf(ZFTS_IOVCNT * ZFTS_TCP_DATA_MAX + 1,
                       ZFTS_IOVCNT * ZFTS_TCP_DATA_MAX * 2,
                       &data_len);

    rc = rpc_send(pco_tst, tst_s, data, data_len, 0);
    if (rc != (ssize_t)data_len)
        TEST_FAIL("Failed to send %u bytes", (unsigned int)data_len);

    rpc_zf_wait_for_event(pco_iut, stack);
    rpc_zf_process_events(pco_iut, stack);

    zfts_zft_recv_dbuf(pco_iut, iut_zft, &received_data);
    rpc_zf_process_events(pco_iut, stack);

    if (received_data.len == 0)
        TEST_FAIL("No data was received");
    else
        RING("%ld bytes was sent, %ld bytes was read",
             (long int)data_len, (long int)received_data.len);

    ZFTS_CHECK_RECEIVED_DATA(received_data.ptr, data,
                             received_data.len, received_data.len,
                             " from Tester");
    free(data);
    te_dbuf_free(&received_data);
}

/* See description in zfts_tcp.h */
void
zfts_zft_check_connection(rcf_rpc_server *pco_iut,
                          rpc_zf_stack_p stack, rpc_zft_p iut_zft,
                          rcf_rpc_server *pco_tst, int tst_s)
{
    zfts_zft_check_sending(pco_iut, stack, iut_zft, pco_tst, tst_s);
    zfts_zft_check_receiving(pco_iut, stack, iut_zft, pco_tst, tst_s);
}

/* See description in zfts_tcp.h */
zfts_tcp_conn *
zfts_tcp_conns_alloc(int count)
{
    zfts_tcp_conn *conns = NULL;

    int i;

    conns = tapi_calloc(count, sizeof(*conns));
    for (i = 0; i < count; i++)
    {
        conns[i].iut_zft = RPC_NULL;
        conns[i].tst_s = -1;
    }

    return conns;
}

/* See description in zfts_tcp.h */
void
zfts_tcp_conns_establish(zfts_tcp_conn *conns,
                         int count,
                         te_bool active,
                         rcf_rpc_server *pco_iut,
                         rpc_zf_attr_p attr,
                         rpc_zf_stack_p stack,
                         rpc_zftl_p iut_zftl,
                         const struct sockaddr *iut_addr,
                         rcf_rpc_server *pco_tst,
                         const struct sockaddr *tst_addr,
                         int tst_sndbuf, int tst_rcvbuf,
                         te_bool tst_ndelay, te_bool tst_nonblock)
{
    struct sockaddr_storage iut_bind_addr;
    struct sockaddr_storage tst_bind_addr;
    te_bool silent_default_orig_iut = pco_iut->silent_default;
    te_bool silent_default_orig_tst = pco_tst->silent_default;
    int i;

    pco_iut->silent_pass_default = pco_iut->silent_pass = TRUE;
    pco_tst->silent_pass_default = pco_tst->silent_pass = TRUE;

    for (i = 0; i < count; i++)
    {
        if (active || iut_zftl == RPC_NULL)
            CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr, &iut_bind_addr));
        else
            tapi_sockaddr_clone_exact(iut_addr, &iut_bind_addr);

        CHECK_RC(tapi_sockaddr_clone(pco_tst, tst_addr, &tst_bind_addr));

        RING("Establishing connection %d: %s %s %s", i + 1,
             te_sockaddr2str(CONST_SA(&iut_bind_addr)),
             active ? "->" : "<-",
             te_sockaddr2str(CONST_SA(&tst_bind_addr)));

        zfts_establish_tcp_conn_ext(active, pco_iut, attr, stack,
                                    iut_zftl, &conns[i].iut_zft,
                                    SA(&iut_bind_addr),
                                    pco_tst, &conns[i].tst_s,
                                    SA(&tst_bind_addr),
                                    tst_sndbuf, tst_rcvbuf);

        if (tst_ndelay)
            rpc_setsockopt_int(pco_tst, conns[i].tst_s,
                               RPC_TCP_NODELAY, 1);
        if (tst_nonblock)
            rpc_fcntl(pco_tst, conns[i].tst_s, RPC_F_SETFL,
                      RPC_O_NONBLOCK);
    }

    pco_iut->silent_pass_default = silent_default_orig_iut;
    pco_iut->silent_pass = silent_default_orig_iut;
    pco_tst->silent_pass_default = silent_default_orig_tst;
    pco_tst->silent_pass = silent_default_orig_tst;
}

/* See description in zfts_tcp.h */
void
zfts_tcp_conns_establish2(zfts_tcp_conn *conns,
                          int count,
                          zfts_conn_open_method open_method,
                          rcf_rpc_server *pco_iut,
                          rpc_zf_attr_p attr,
                          rpc_zf_stack_p stack,
                          rpc_zftl_p *iut_zftl,
                          const struct sockaddr *iut_addr,
                          rcf_rpc_server *pco_tst,
                          const struct sockaddr *tst_addr,
                          int tst_sndbuf, int tst_rcvbuf,
                          te_bool tst_ndelay, te_bool tst_nonblock2)
{
    rpc_zftl_p zftl_zock = RPC_NULL;

    if (open_method != ZFTS_CONN_OPEN_ACT)
        rpc_zftl_listen(pco_iut, stack, iut_addr,
                        attr, &zftl_zock);

    zfts_tcp_conns_establish(conns, count,
                             (open_method == ZFTS_CONN_OPEN_ACT),
                             pco_iut, attr, stack, zftl_zock, iut_addr,
                             pco_tst, tst_addr, tst_sndbuf, tst_rcvbuf,
                             tst_ndelay, tst_nonblock2);

    if (open_method == ZFTS_CONN_OPEN_PAS_CLOSE)
        ZFTS_FREE(pco_iut, zftl, zftl_zock);

    if (iut_zftl != NULL)
        *iut_zftl = zftl_zock;
}

/* See description in zfts_tcp.h */
void
zfts_tcp_conns_destroy(rcf_rpc_server *pco_iut,
                       rcf_rpc_server *pco_tst,
                       zfts_tcp_conn *conns,
                       int count)
{
    int i;

    if (conns != NULL)
    {
        for (i = 0; i < count; i++)
        {
            if (conns[i].tst_s >= 0)
                rpc_close(pco_tst, conns[i].tst_s);

            if (conns[i].iut_zft != RPC_NULL)
                rpc_zft_free(pco_iut, conns[i].iut_zft);

            te_dbuf_free(&conns[i].iut_sent);
            te_dbuf_free(&conns[i].tst_sent);
        }

        free(conns);
    }
}

/* See description in zfts_tcp.h */
void
zfts_zft_peer_read_all(rcf_rpc_server *pco_iut,
                       rpc_zf_stack_p stack,
                       rcf_rpc_server *pco_tst,
                       int tst_s,
                       te_dbuf *received_data)
{
    size_t buf_len_prev = received_data->len;

    do {
        ZFTS_WAIT_NETWORK(pco_iut, stack);
        rpc_read_fd2te_dbuf_append(pco_tst, tst_s, 0, 0, received_data);
        if (received_data->len - buf_len_prev == 0)
            break;
        else
            buf_len_prev = received_data->len;
    } while (TRUE);
}

/* See description in zfts_tcp.h */
void
zfts_tcp_conns_read_check_data_tst(rcf_rpc_server *pco_iut,
                                   rpc_zf_stack_p stack,
                                   rcf_rpc_server *pco_tst,
                                   zfts_tcp_conn *conns,
                                   int count)
{
    int i;

    te_dbuf *received_bufs = NULL;

    size_t  received_len = 0;
    te_dbuf tmp_dbuf = TE_DBUF_INIT(100);
    te_bool prev_failed = FALSE;

    received_bufs = tapi_calloc(count, sizeof(*received_bufs));

    do {
        received_len = 0;
        for (i = 0; i < count; i++)
        {
            size_t left_len = conns[i].iut_sent.len - received_bufs[i].len;

            if (left_len == 0)
                continue;

            rpc_read_fd2te_dbuf(pco_tst, conns[i].tst_s, 100, left_len,
                                &tmp_dbuf);
            received_len += tmp_dbuf.len;
            te_dbuf_append(&received_bufs[i], tmp_dbuf.ptr, tmp_dbuf.len);
        }

        if (received_len > 0)
        {
            prev_failed = FALSE;
        }
        else
        {
            if (prev_failed)
                break;
            prev_failed = TRUE;
        }

        ZFTS_WAIT_NETWORK(pco_iut, stack);
    } while (TRUE);

    for (i = 0; i < count; i++)
    {
        RING("Checking data received from IUT for connection %d",
             i + 1);
        ZFTS_CHECK_RECEIVED_DATA(received_bufs[i].ptr,
                                 conns[i].iut_sent.ptr,
                                 received_bufs[i].len,
                                 conns[i].iut_sent.len,
                                 " from IUT");
        te_dbuf_free(&received_bufs[i]);
    }

    free(received_bufs);
    te_dbuf_free(&tmp_dbuf);
}

/* See description in zfts_tcp.h */
int
zfts_zft_read_data(rcf_rpc_server *rpcs,
                   rpc_zft_p zocket,
                   te_dbuf *received_data,
                   size_t *received_len)
{
    if (def_tcp_recv_func == ZFTS_TCP_RECV_ZFT_RECV)
    {
        return rpc_zft_read_all(rpcs, zocket, received_data,
                                received_len);
    }
    else if (def_tcp_recv_func == ZFTS_TCP_RECV_ZFT_ZC_RECV)
    {
        return rpc_zft_read_all_zc(rpcs, zocket, received_data,
                                   received_len);
    }
    else
    {
        TEST_FAIL("%s(): unknown TCP receive function", __FUNCTION__);
    }

    /* It should never reach here */
    return -1;
}

/* See description in zfts_tcp.h */
void
zfts_zft_read_all(rcf_rpc_server *rpcs,
                  rpc_zf_stack_p stack,
                  rpc_zft_p zocket,
                  te_dbuf *received_data)
{
    size_t received_len;

    /*
     * FIXME: may be this function should return
     * number of bytes read.
     */
    do {
        zfts_zft_read_data(rpcs, zocket, received_data,
                           &received_len);
        if (received_len <= 0)
            break;
        ZFTS_WAIT_NETWORK(rpcs, stack);
    } while (TRUE);
}

/**
 * Read all the data from IUT zocket for a given TCP
 * connection.
 *
 * @param pco_iut         RPC server on IUT.
 * @param conn            TCP connection description.
 * @param received_data   Where to save read data.
 *
 * @return Amount of data read.
 */
static size_t
zfts_tcp_conn_read_data_iut(rcf_rpc_server *pco_iut,
                            zfts_tcp_conn *conn,
                            te_dbuf *received_data)
{
    size_t received_len = 0;

    if (conn->tst_sent.len == 0)
        return 0;

    zfts_zft_read_data(pco_iut, conn->iut_zft,
                       received_data, &received_len);

    return received_len;
}

/* See description in zfts_tcp.h */
void
zfts_tcp_conns_read_check_data_iut(rcf_rpc_server *pco_iut,
                                   rpc_zf_stack_p stack,
                                   zfts_tcp_conn *conns,
                                   int count)
{
    int i;

    te_dbuf *received_bufs = NULL;
    size_t   received_len;

    received_bufs = tapi_calloc(count, sizeof(*received_bufs));

    do {
        received_len = 0;
        for (i = 0; i < count; i++)
        {
            received_len += zfts_tcp_conn_read_data_iut(pco_iut, &conns[i],
                                                        &received_bufs[i]);
        }

        if (received_len == 0)
            break;

        ZFTS_WAIT_NETWORK(pco_iut, stack);
    } while (TRUE);

    for (i = 0; i < count; i++)
    {
        RING("Checking data received from Tester for connection %d",
             i + 1);
        ZFTS_CHECK_RECEIVED_DATA(received_bufs[i].ptr,
                                 conns[i].tst_sent.ptr,
                                 received_bufs[i].len,
                                 conns[i].tst_sent.len,
                                 " from Tester");
        te_dbuf_free(&received_bufs[i]);
    }

    free(received_bufs);
}

/* See description in zfts_tcp.h */
ssize_t
zfts_zft_recv_func(rcf_rpc_server *rpcs, rpc_zft_p zft_zocket,
                   zfts_tcp_recv_func_t func,
                   char *buf, size_t len)
{
    rpc_iovec    *iovs = NULL;
    int           iovcnt;
    int           rc;

    iovcnt = len / ZFTS_TCP_DATA_MAX;
    if (len % ZFTS_TCP_DATA_MAX != 0 || len == 0)
        iovcnt++;

    iovs = tapi_calloc(iovcnt, sizeof(*iovs));

    switch (func)
    {
        case ZFTS_TCP_RECV_ZFT_ZC_RECV:
        {
            rpc_zft_msg   msg;
            te_dbuf       buf_aux = TE_DBUF_INIT(0);
            int           copy_len;

            memset(&msg, 0, sizeof(msg));
            msg.iovcnt = iovcnt;
            msg.iov = iovs;

            rpc_zft_zc_recv(rpcs, zft_zocket, &msg, 0);
            if (!RPC_IS_CALL_OK(rpcs))
            {
                free(iovs);
                return -1;
            }

            if (msg.iovcnt == 0)
            {
                free(iovs);
                return 0;
            }

            if (msg.iovcnt < 0 || msg.iovcnt > iovcnt)
                TEST_FAIL("zft_zc_recv() returned strange iovcnt value %d",
                          msg.iovcnt);

            rpc_iov2dbuf(iovs, msg.iovcnt, &buf_aux);
            if (len >= buf_aux.len)
            {
                copy_len = buf_aux.len;

                zfts_zft_zc_recv_done_wrapper(rpcs, zft_zocket, &msg,
                                              FALSE, 0);
            }
            else
            {
                copy_len = len;

                zfts_zft_zc_recv_done_wrapper(rpcs, zft_zocket, &msg,
                                              TRUE, copy_len);
            }

            memcpy(buf, buf_aux.ptr, copy_len);
            rpc_release_iov(iovs, msg.iovcnt);
            free(iovs);
            te_dbuf_free(&buf_aux);

            return copy_len;
        }

        case ZFTS_TCP_RECV_ZFT_RECV:
        {
            int     i;
            size_t  pos = 0;
            size_t  iov_len = 0;

            for (i = 0; i < iovcnt; i++)
            {
                iov_len = len - pos;
                if (iov_len > ZFTS_TCP_DATA_MAX)
                    iov_len = ZFTS_TCP_DATA_MAX;

                iovs[i].iov_base = buf + pos;
                iovs[i].iov_rlen = iovs[i].iov_len = iov_len;

                pos = pos + iov_len;

                if (pos == len)
                {
                    iovcnt = i + 1;
                    break;
                }
            }

            rc = rpc_zft_recv(rpcs, zft_zocket, iovs, iovcnt, 0);
            free(iovs);
            return rc;
        }

        default:

            TEST_FAIL("%s(): unknown TCP receive function", __FUNCTION__);
    }

    free(iovs);
    return -1;
}

/* See description in zfts_tcp.h */
ssize_t
zfts_zft_recv_func_dbuf(rcf_rpc_server *rpcs, rpc_zft_p zft_zocket,
                        zfts_tcp_recv_func_t func,
                        te_dbuf *dbuf)
{
    rpc_iovec iovs[ZFTS_IOVCNT];
    int       iovcnt;
    ssize_t   rc = 0;
    ssize_t   len;

    memset(iovs, 0, sizeof(iovs));
    iovcnt = rand_range(1, ZFTS_IOVCNT);

    switch (func)
    {
        case ZFTS_TCP_RECV_ZFT_ZC_RECV:
        {
            rpc_zft_msg   msg;

            memset(&msg, 0, sizeof(msg));
            msg.iovcnt = iovcnt;
            msg.iov = iovs;

            rpc_zft_zc_recv(rpcs, zft_zocket, &msg, 0);
            if (!RPC_IS_CALL_OK(rpcs))
            {
                rpc_release_iov(iovs, iovcnt);
                return -1;
            }

            len = 0;
            if (msg.iovcnt > 0)
            {
                len = rpc_iov_data_len(iovs, msg.iovcnt);
                rpc_iov_append2dbuf(iovs, msg.iovcnt, dbuf);

                zfts_zft_zc_recv_done_wrapper(rpcs, zft_zocket, &msg,
                                              FALSE, 0);

                rpc_release_iov(iovs, iovcnt);
            }

            return len;
        }

        case ZFTS_TCP_RECV_ZFT_RECV:
        {
            char buf[ZFTS_IOVCNT * ZFTS_TCP_DATA_MAX];
            int  i;
            int  pos;

            pos = 0;
            for (i = 0; i < iovcnt; i++)
            {
                iovs[i].iov_len = iovs[i].iov_rlen =
                            rand_range(1, ZFTS_TCP_DATA_MAX);
                iovs[i].iov_base = buf + pos;

                pos += iovs[i].iov_len;
            }

            rc = rpc_zft_recv(rpcs, zft_zocket, iovs, iovcnt, 0);
            if (rc < 0)
                return rc;

            te_dbuf_append(dbuf, buf, rc);
            return rc;
        }

        default:
            TEST_FAIL("%s(): unknown TCP receive function", __FUNCTION__);
    }

    return -1;
}

/* See description in zfts_tcp.h */
void
zfts_set_def_tcp_recv_func(zfts_tcp_recv_func_t recv_func)
{
    def_tcp_recv_func = recv_func;
}

/* See description in zfts_tcp.h */
size_t
zfts_zft_recv(rcf_rpc_server *rpcs, rpc_zft_p zft_zocket,
              char *buf, size_t len)
{
    return zfts_zft_recv_func(rpcs, zft_zocket, def_tcp_recv_func,
                              buf, len);
}

/* See description in zfts_tcp.h */
size_t
zfts_zft_recv_dbuf(rcf_rpc_server *rpcs, rpc_zft_p zft_zocket,
                   te_dbuf *dbuf)
{
    return zfts_zft_recv_func_dbuf(rpcs, zft_zocket,
                                   def_tcp_recv_func, dbuf);
}

/* See description in zfts_tcp.h */
ssize_t
zfts_zft_send_func(rcf_rpc_server *rpcs,
                   rpc_zft_p zft_zocket,
                   zfts_tcp_send_func_t func,
                   char *buf, size_t len)
{
    switch (func)
    {
        case ZFTS_TCP_SEND_ZFT_SEND_SINGLE:

            return rpc_zft_send_single(rpcs, zft_zocket, buf, len, 0);

        case ZFTS_TCP_SEND_ZFT_SEND:
        {
            rpc_iovec *iovs = NULL;
            size_t     iovcnt;
            size_t     i;
            size_t     pos;
            size_t     iov_len;
            ssize_t    rc;

            iovcnt = len / ZFTS_TCP_DATA_MAX;
            if (len % ZFTS_TCP_DATA_MAX != 0 || len == 0)
                iovcnt++;

            iovs = tapi_calloc(iovcnt, sizeof(*iovs));

            pos = 0;
            for (i = 0; i < iovcnt; i++)
            {
                iov_len = len - pos;
                if (iov_len > ZFTS_TCP_DATA_MAX)
                    iov_len = ZFTS_TCP_DATA_MAX;

                iovs[i].iov_base = buf + pos;
                iovs[i].iov_rlen = iovs[i].iov_len = iov_len;
                pos += iov_len;
            }

            rc = rpc_zft_send(rpcs, zft_zocket, iovs, iovcnt, 0);

            free(iovs);
            return rc;
        }

        default:

            TEST_FAIL("%s(): unknown TCP send function", __FUNCTION__);
    }

    return -1;
}

/* See description in zfts_tcp.h */
int
zfts_zft_send(rcf_rpc_server *rpcs,
              rpc_zft_p zft_zocket,
              char *buf, size_t len)
{
    return zfts_zft_send_func(rpcs, zft_zocket,
                              ZFTS_TCP_SEND_ZFT_SEND,
                              buf, len);
}

/* See description in zfts_tcp.h */
void
zfts_wait_for_final_tcp_state_ext(rcf_rpc_server *pco_iut,
                                  rpc_zf_stack_p stack,
                                  rpc_zft_p zocket,
                                  te_bool wait_established,
                                  te_bool wait_closed)
{
#define MAX_CHECK_STATE_ITERS 140
    int i;
    int tcp_state;

    if (!wait_established && !wait_closed)
        return;

    for (i = 0; i < MAX_CHECK_STATE_ITERS; i++)
    {
        ZFTS_WAIT_NETWORK(pco_iut, stack);

        tcp_state = rpc_zft_state(pco_iut, zocket);
        if ((tcp_state == RPC_TCP_CLOSE && wait_closed) ||
            (tcp_state == RPC_TCP_ESTABLISHED && wait_established))
            return;
    }

    TEST_VERDICT("Failed to wait until zocket is in connected or "
                 "closed state");
}

/* See description in zfts_tcp.h */
void
zfts_wait_for_final_tcp_state(rcf_rpc_server *pco_iut,
                              rpc_zf_stack_p stack,
                              rpc_zft_p zocket)
{
    return zfts_wait_for_final_tcp_state_ext(pco_iut, stack, zocket,
                                             TRUE, TRUE);
}

/* See description in zfts_tcp.h */
void
zfts_zft_sanity_checks(rcf_rpc_server *pco_iut,
                       rpc_zf_stack_p stack,
                       rpc_zft_p iut_zft,
                       const struct if_nameindex *iut_if,
                       const struct sockaddr *iut_addr,
                       rcf_rpc_server *pco_tst,
                       int *tst_s,
                       const struct sockaddr *tst_addr,
                       zfts_tcp_recv_func_t recv_func,
                       zfts_tcp_send_func_t send_func)
{
#define EXP_TCP_HEADER_SIZE (14 /* Ethernet */ + 20 /* IP */ + 20 /* TCP */)

    te_errno  rc;
    char      send_data[ZFTS_TCP_DATA_MAX];
    char      recv_data[ZFTS_TCP_DATA_MAX];
    char      recv_data_tst[ZFTS_TCP_DATA_MAX];

    rpc_iovec iov;

    struct sockaddr_in laddr;
    struct sockaddr_in raddr;

    rpc_zf_waitable_p   iut_zft_waitable = RPC_NULL;
    size_t              send_space = 0;
    unsigned int        header_size = 0;
    unsigned int        vlan_header_add = 0;

    te_fill_buf(send_data, ZFTS_TCP_DATA_MAX);

    /*
     * Check that data may be sent and received via
     * TCP connection.
     */

    rpc_send(pco_tst, *tst_s, send_data, ZFTS_TCP_DATA_MAX, 0);
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    rc = zfts_zft_recv_func(pco_iut, iut_zft, recv_func,
                            recv_data_tst, ZFTS_TCP_DATA_MAX);

    ZFTS_CHECK_RECEIVED_DATA(recv_data_tst, send_data,
                             rc, ZFTS_TCP_DATA_MAX,
                             " from IUT");

    zfts_zft_send_func(pco_iut, iut_zft, send_func,
                       recv_data_tst, rc);
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    rc = rpc_recv(pco_tst, *tst_s, recv_data, ZFTS_TCP_DATA_MAX, 0);

    ZFTS_CHECK_RECEIVED_DATA(recv_data, recv_data_tst,
                             rc, ZFTS_TCP_DATA_MAX,
                             " from Tester");

    /*
     * Check all possible function of TCP zocket.
     */

    rc = rpc_zft_state(pco_iut, iut_zft);
    if (rc != RPC_TCP_ESTABLISHED)
        ERROR_VERDICT("zft_state() returned unexpected state %s",
                      tcp_state_rpc2str(rc));

    rc = rpc_zft_error(pco_iut, iut_zft);
    if (rc != RPC_EOK)
        ERROR_VERDICT("zft_error() returned unexpected errno %r", rc);

    if (tapi_interface_is_vlan(pco_iut, iut_if))
        vlan_header_add = ZFTS_VLAN_TAG_LEN;

    header_size = rpc_zft_get_header_size(pco_iut, iut_zft);
    if (header_size != EXP_TCP_HEADER_SIZE + vlan_header_add)
        ERROR_VERDICT("zft_get_header_size() returned unexpected value");

    rpc_zft_getname(pco_iut, iut_zft, &laddr, &raddr);
    if (tapi_sockaddr_cmp(iut_addr, SA(&laddr)) != 0)
        ERROR_VERDICT("zft_getname() returned incorrect local address");
    if (tapi_sockaddr_cmp(tst_addr, SA(&raddr)) != 0)
        ERROR_VERDICT("zft_getname() returned incorrect remote address");

    iut_zft_waitable = rpc_zft_to_waitable(pco_iut, iut_zft);

    rpc_zft_send_space(pco_iut, iut_zft, &send_space);
    if (send_space == 0)
        ERROR_VERDICT("zft_send_space() returned space = 0");

    rpc_zft_shutdown_tx(pco_iut, iut_zft);
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    RPC_AWAIT_ERROR(pco_tst);
    rc = rpc_recv(pco_tst, *tst_s, recv_data,
                  ZFTS_TCP_DATA_MAX, RPC_MSG_DONTWAIT);
    if (rc != 0)
        ERROR_VERDICT("recv() did not return zero on Tester after "
                      "zft_shutdown_tx() on IUT");

    rpc_close(pco_tst, *tst_s);
    *tst_s = -1;
    ZFTS_WAIT_NETWORK(pco_iut, stack);

    iov.iov_base = recv_data;
    iov.iov_len = iov.iov_rlen = ZFTS_TCP_DATA_MAX;

    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_zft_recv(pco_iut, iut_zft, &iov, 1, 0);
    if (rc != 0)
        ERROR_VERDICT("zft_recv() did not return zero on IUT after "
                      "closing Tester socket");

    rc = rpc_zft_error(pco_iut, iut_zft);
    if (rc != RPC_EOK)
        ERROR_VERDICT("zft_error() returned unexpected errno %r "
                      "after closing peer socket", RPC_ERRNO(pco_iut));

    rpc_zf_waitable_free(pco_iut, iut_zft_waitable);
}
