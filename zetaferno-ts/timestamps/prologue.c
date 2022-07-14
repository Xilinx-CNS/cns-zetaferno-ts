/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief SFC Zetaferno Timestamps API Test Package prologue.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#ifndef DOXYGEN_TEST_SPEC

/** Logging subsystem entity name */
#define TE_TEST_NAME    "timestamps/prologue"

#include "zf_test.h"
#include "lib-ts_timestamps.h"
#include "lib-ts_netns.h"
#include "tapi_sfptpd.h"

/**
 * Maximum waiting time to get NIC synchronized, seconds.
 * In theory synchronization can take up to 20 minutes.
 */
#define MAX_ATTEMPTS (20 * 60)

int
main(int argc, char **argv)
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;

    char *ta_sfc = NULL;
    te_bool start_sfptpd = !tapi_getenv_bool("ST_RUN_TS_NO_SFPTPD");

    rpc_zf_attr_p attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zfur_p iut_s = RPC_NULL;
    int tst_s = -1;

    rpc_zfur_msg msg;
    rpc_iovec iov;
    char send_buf[ZFTS_DGRAM_MAX];
    char recv_buf[ZFTS_DGRAM_MAX];

    tarpc_timespec ts;
    unsigned int flags;
    int attempts_num = 0;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);

    libts_timestamps_enable_sfptpd(pco_iut);
    CHECK_RC(libts_netns_get_sfc_ta(&ta_sfc));

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_attr_set_int(pco_iut, attr, "rx_timestamping", 1);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    rpc_zfur_alloc(pco_iut, &iut_s, stack, attr);
    rpc_zfur_addr_bind(pco_iut, iut_s, SA(iut_addr), tst_addr, 0);

    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);
    rpc_connect(pco_tst, tst_s, iut_addr);

    while (TRUE)
    {
        if (start_sfptpd && !tapi_sfptpd_status(ta_sfc))
            TEST_VERDICT("sfptpd is disabled");

        RPC_SEND(rc, pco_tst, tst_s, send_buf, sizeof(send_buf), 0);

        rpc_zf_wait_for_event(pco_iut, stack);

        memset(&iov, 0, sizeof(iov));
        iov.iov_base = recv_buf;
        iov.iov_len = iov.iov_rlen = sizeof(recv_buf);

        memset(&msg, 0, sizeof(msg));
        msg.iovcnt = 1;
        msg.iov = &iov;

        rpc_zfur_zc_recv(pco_iut, iut_s, &msg, 0);
        if (msg.iovcnt != 1)
        {
            TEST_VERDICT("zfur_zc_recv() set msg.iovcnt to an unexpected "
                         "value");
        }

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zfur_pkt_get_timestamp(pco_iut, iut_s, msg.ptr,
                                        &ts, 0, &flags);
        if (rc < 0)
        {
            TEST_VERDICT("zfur_pkt_get_timestamp() failed unexpectedly "
                         "with error " RPC_ERROR_FMT,
                         RPC_ERROR_ARGS(pco_iut));
        }

        rpc_zfur_zc_recv_done(pco_iut, iut_s, &msg);

        if ((flags & TARPC_ZF_SYNC_FLAG_CLOCK_SET) &&
            (flags & TARPC_ZF_SYNC_FLAG_CLOCK_IN_SYNC))
            break;

        attempts_num++;
        if (attempts_num > MAX_ATTEMPTS)
            TEST_VERDICT("Failed to wait until NIC clock is synchronized");

        SLEEP(1);
    }

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

#endif /* !DOXYGEN_TEST_SPEC */
