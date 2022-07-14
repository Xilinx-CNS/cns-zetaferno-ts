/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Delegated Sends tests
 *
 * Implementation of common API for Delegated Sends tests.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#include "zf_ds_test.h"
#include "tapi_sockets.h"

#include "te_vector.h"

/* See description in zf_ds_test.h */
void
zfts_ds_raw_send(rcf_rpc_server *rpcs, int if_index, int raw_s,
                 struct zf_ds *ds, char *buf, size_t len,
                 te_bool silent_pass)
{
    rpc_iovec raw_iovs[2];

    rpcs->silent_pass = silent_pass;
    rpc_zf_delegated_send_tcp_update(rpcs, ds, len, 1);

    memset(&raw_iovs, 0, sizeof(raw_iovs));
    raw_iovs[0].iov_base = ds->headers;
    raw_iovs[0].iov_len = raw_iovs[0].iov_rlen = ds->headers_len;
    raw_iovs[1].iov_base = buf;
    raw_iovs[1].iov_len = raw_iovs[1].iov_rlen = len;

    rpcs->silent_pass = silent_pass;
    CHECK_RC(tapi_sock_raw_tcpv4_send(rpcs, raw_iovs, 2,
                                      if_index, raw_s, TRUE));

    rpcs->silent_pass = silent_pass;
    rpc_zf_delegated_send_tcp_advance(rpcs, ds, len);
}

/* See description in zf_ds_test.h */
void
zfts_ds_raw_send_iov(rcf_rpc_server *rpcs, int if_index,
                     int raw_s, struct zf_ds *ds,
                     rpc_iovec *iov, int iov_num,
                     te_bool silent_pass)
{
    int i;

    for (i = 0; i < iov_num; i++)
    {
        zfts_ds_raw_send(rpcs, if_index, raw_s, ds, iov[i].iov_base,
                         iov[i].iov_len, silent_pass);
    }
}

/* See description in zf_ds_test.h */
void
zfts_ds_raw_send_complete(rcf_rpc_server *rpcs, int if_index, int raw_s,
                          rpc_zft_p zft, struct zf_ds *ds, char *buf,
                          size_t len, int min_pkt_len,
                          int max_pkt_len)
{
    rpc_iovec *iov;
    int iov_num;
    int rc;

    zfts_split_in_iovs(buf, len, min_pkt_len, max_pkt_len,
                       &iov, &iov_num);

    if (raw_s >= 0)
    {
        zfts_ds_raw_send_iov(rpcs, if_index, raw_s, ds, iov, iov_num,
                             FALSE);
    }

    RPC_AWAIT_ERROR(rpcs);
    rc = rpc_zf_delegated_send_complete(rpcs, zft, iov, iov_num, 0);
    free(iov);
    zfts_check_ds_complete(rpcs, rc, len, "");
}
