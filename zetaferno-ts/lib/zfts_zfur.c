/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Implementation of auxiliary test API for UDP RX zockets
 *
 * Implementation of auxiliary test API to work with UDP RX zockets.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

/* User name of the library which is used in logging. */
#define TE_LGR_USER     "ZF UDP RX TAPI"

#include "zetaferno_ts.h"
#include "te_dbuf.h"
#include "zfts_zfur.h"

/* Maximum attempts multiplier reading datagrams. It is used to handle
 * possible occasion appearing of an event on the ZF stack, which is not
 * receiving event really. */
#define MAX_ATTEMPTS_MULT 10

/* See description in zfts_zfur.h */
size_t
zfts_zfur_zc_recv(rcf_rpc_server *rpcs, rpc_zfur_p urx, rpc_iovec *iov,
                  size_t iovcnt)
{
    rpc_zfur_msg msg = {.iovcnt = iovcnt, .iov = iov};

    rpc_zfur_zc_recv(rpcs, urx, &msg, 0);
    if (msg.iovcnt > 0)
        rpc_zfur_zc_recv_done(rpcs, urx, &msg);

    return msg.iovcnt;
}

/* See description in zfts_zfur.h */
size_t
zfts_zfur_recv(rcf_rpc_server *rpcs, rpc_zfur_p urx,
               char *buf, size_t len)
{
    size_t  rc;

    rpc_iovec iov = {.iov_base = buf, .iov_len = len};

    iov.iov_rlen = iov.iov_len;

    rc = zfts_zfur_zc_recv(rpcs, urx, &iov, 1);

    return (rc > 0 ? iov.iov_len : 0);
}

/* See description in zfts_zfur.h */
size_t
zfts_zfur_read_data(rcf_rpc_server *rpcs,
                    rpc_zf_stack_p stack,
                    rpc_zfur_p zocket,
                    te_dbuf *received_data)
{
    char buf[ZFTS_DGRAM_MAX];

    size_t received_len;
    size_t total_read = 0;

    do {
        received_len = zfts_zfur_recv(rpcs, zocket,
                                      buf, ZFTS_DGRAM_MAX);
        if (received_len <= 0)
            break;
        rpc_zf_process_events(rpcs, stack);

        te_dbuf_append(received_data, buf, received_len);
        total_read += received_len;
    } while (TRUE);

    return total_read;
}

/* See description in zfts_zfur.h */
void
zfts_zfur_zc_recv_all(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                      rpc_zfur_p urx, rpc_iovec *iov, size_t iovcnt)
{
    size_t rcnt;
    size_t rc = 0;
    size_t i;

    for (i = 0, rcnt = 0; rcnt < iovcnt; i++)
    {
        if (i == iovcnt * MAX_ATTEMPTS_MULT)
        {
            ERROR("Maximum attempts (%d) to receive datagrams has "
                  "been reached.", MAX_ATTEMPTS_MULT);
            ERROR("Received datagrams number %"TE_PRINTF_SIZE_T"u "
                  "instead of %"TE_PRINTF_SIZE_T"u", rcnt, iovcnt);
            TEST_VERDICT("There is a datagram loss");
        }

        if (rc == 0)
            rpc_zf_wait_for_event(rpcs, stack);
        rc = zfts_zfur_zc_recv(rpcs, urx, iov + rcnt, iovcnt - rcnt);
        rcnt += rc;
        rpc_zf_process_events(rpcs, stack);
    }
}

/* See description in zfts_zfur.h */
void
zfts_zfur_bind_to_wildcard(rcf_rpc_server *rpcs, rpc_zfur_p urx,
                           const struct sockaddr *src_addr,
                           const struct sockaddr *dst_addr)
{
    struct sockaddr_storage  wildcard_addr;

    tapi_sockaddr_clone_exact(src_addr, &wildcard_addr);
    te_sockaddr_set_wildcard(SA(&wildcard_addr));
    rpc_zfur_addr_bind(rpcs, urx, SA(&wildcard_addr), dst_addr, 0);
}

/* See description in zfts_zfur.h */
void
zfts_zfur_check_not_readable(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                             rpc_zfur_p urx)
{
    size_t rc;
    char buf[ZFTS_DGRAM_MAX];
    rpc_iovec iov = {.iov_base = buf, .iov_len = sizeof(buf)};

    iov.iov_rlen = iov.iov_len;

    rpc_zf_process_events(rpcs, stack);
    rc = zfts_zfur_zc_recv(rpcs, urx, &iov, 1);
    if (rc > 0)
        TEST_VERDICT("There are unexpectedly received datagrams");
}

/* See description in zfts_zfur.h */
void
zfts_zfur_check_reception(rcf_rpc_server *pco_iut, rpc_zf_stack_p stack,
                          rpc_zfur_p urx, rcf_rpc_server *pco_tst,
                          int tst_s, const struct sockaddr *iut_addr)
{
    rpc_iovec rcviov[ZFTS_IOVCNT];
    rpc_iovec sndiov[ZFTS_IOVCNT];

    rpc_make_iov(sndiov, ZFTS_IOVCNT, 1, ZFTS_DGRAM_MAX);
    rpc_make_iov(rcviov, ZFTS_IOVCNT, ZFTS_DGRAM_MAX, ZFTS_DGRAM_MAX);

    zfts_sendto_iov(pco_tst, tst_s, sndiov, ZFTS_IOVCNT, iut_addr);
    zfts_zfur_zc_recv_all(pco_iut, stack, urx, rcviov, ZFTS_IOVCNT);

    rpc_iovec_cmp_strict(rcviov, sndiov, ZFTS_IOVCNT);

    rpc_release_iov(rcviov, ZFTS_IOVCNT);
    rpc_release_iov(sndiov, ZFTS_IOVCNT);
}

/* See description in zfts_zfur.h */
int
zfts_zfur_bind(rcf_rpc_server *rpcs, rpc_zfur_p urx,
               tapi_address_type local_addr_type,
               const struct sockaddr *local_addr,
               struct sockaddr **local_addr_out,
               tapi_address_type remote_addr_type,
               const struct sockaddr *remote_addr, int flags)
{
    int rc;
    struct sockaddr *laddr = tapi_sockaddr_clone_typed(local_addr,
                                                       local_addr_type);
    /* It is possible to get memory leak if the next call fails. Do not care
     * about it because the call jumps to cleanup in this case. Then the
     * test application will be closed. */
    struct sockaddr *raddr = tapi_sockaddr_clone_typed(remote_addr,
                                                       remote_addr_type);

    rc = rpc_zfur_addr_bind(rpcs, urx, laddr, raddr, flags);

    if (local_addr_out != NULL)
        *local_addr_out = laddr;
    else
        free(laddr);

    free(raddr);

    return rc;
}
