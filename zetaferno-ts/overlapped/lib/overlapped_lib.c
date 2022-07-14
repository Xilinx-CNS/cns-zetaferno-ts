/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Zetaferno API Test Suite
 *
 * Auxiliary functions for overlapped package.
 *
 * @author Artemii Morozov <Artemii.Morozov@oktetlabs.ru>
 *
 * $Id$
 */

/** User name of overlapped library */
#define TE_LGR_USER     "Overlapped lib"

#include "overlapped_lib.h"

/**
 * The maximum number of events that can be encountered.
 * If more than this number is encountered, report an error
 */
#define MAX_NUM_OF_EPOLLIN_EVENT 10

static void
process_epollin_event(rcf_rpc_server *pco_iut, rpc_ptr zock,
                      te_bool is_udp_zocket, void *msg, int *iovcnt,
                      int *pkts_left)
{
    int rc;

    do {
        if (is_udp_zocket)
            rpc_zfur_zc_recv(pco_iut, zock, (rpc_zfur_msg *)msg, 0);
        else
            rpc_zft_zc_recv(pco_iut, zock, (rpc_zft_msg *)msg, 0);

        if (*iovcnt != 1)
            TEST_VERDICT("Failed to received packet.");

        if (is_udp_zocket)
            rc = rpc_zfur_zc_recv_done(pco_iut, zock, (rpc_zfur_msg *)msg);
        else
            rc = rpc_zft_zc_recv_done(pco_iut, zock, (rpc_zft_msg *)msg);

        if (rc < 0)
        {
            TEST_VERDICT("Failed to complete pending operation with errno %r",
                         RPC_ERRNO(pco_iut));
        }
    } while (*pkts_left != 0);
}

static void
process_zf_epollin_overllaped_event(rcf_rpc_server *pco_iut, rpc_ptr zock,
                                    te_bool is_udp_zocket, void *msg,
                                    int *iovcnt, rpc_iovec *iov,
                                    rpc_iovec *sndiov,
                                    int total_len, int part_len)
{
    int rc;

    if (is_udp_zocket)
    {
        rpc_zfur_zc_recv(pco_iut, zock, (rpc_zfur_msg *)msg,
                         RPC_ZF_OVERLAPPED_WAIT);
    }
    else
    {
        rpc_zft_zc_recv(pco_iut, zock, (rpc_zft_msg *)msg,
                        RPC_ZF_OVERLAPPED_WAIT);
    }

    if (*iovcnt == 0)
        TEST_VERDICT("Overlapped wait has been abandoned.");

    if (memcmp(sndiov->iov_base,(iov)->iov_base,
            part_len <= total_len ? part_len : total_len) != 0)
    {
        RING_VERDICT("First part of data differs from data sent");
    }

    if (is_udp_zocket)
    {
        rpc_zfur_zc_recv(pco_iut, zock, (rpc_zfur_msg *)msg,
                         RPC_ZF_OVERLAPPED_COMPLETE);
    }
    else
    {
        rpc_zft_zc_recv(pco_iut, zock, (rpc_zft_msg *)msg,
                        RPC_ZF_OVERLAPPED_COMPLETE);
    }

    if (*iovcnt == 0)
    {
        TEST_VERDICT("Packet did not pass verification");
    }
    else
    {
        if (memcmp(sndiov->iov_base, (iov)->iov_base, total_len) != 0)
            TEST_VERDICT("Data received differs from data sent");
    }

    RPC_AWAIT_ERROR(pco_iut);
    if (is_udp_zocket)
        rc = rpc_zfur_zc_recv_done(pco_iut, zock, (rpc_zfur_msg *)msg);
    else
        rc = rpc_zft_zc_recv_done(pco_iut, zock, (rpc_zft_msg *)msg);

    if (rc < 0)
    {
        TEST_VERDICT("Final zfur_zc_recv_done() failed with errno %r",
                     RPC_ERRNO(pco_iut));
    }
}

void
prepare_send_recv_pftf(rcf_rpc_server *pco_iut, rcf_rpc_server *pco_tst,
                       rpc_zf_muxer_set_p set, rpc_ptr zock, int sock,
                       int total_len, int part_len,
                       zfts_zocket_type zocket_type)
{
    struct rpc_epoll_event event;

    rpc_iovec   sndiov;
    rpc_iovec   rcviov;
    rpc_iovec **iov;

    rpc_zfur_msg zfur_msg = {0};
    rpc_zft_msg  zft_msg = {0};

    int  rc;
    int *iovcnt;
    int *pkts_left;
    int  epollin_count = 0;

    te_bool is_udp_zocket;
    size_t *iov_part_len;

    switch (zocket_type)
    {
        case ZFTS_ZOCKET_URX:
            is_udp_zocket = TRUE;
            break;
        case ZFTS_ZOCKET_ZFT_ACT:
        case ZFTS_ZOCKET_ZFT_PAS:
            is_udp_zocket = FALSE;
            break;

        default:
            TEST_FAIL("%s(): Unsupported zocket type in this function",
                       __FUNCTION__);
            break;
    }

    rpc_make_iov(&sndiov, 1, total_len, total_len);
    rpc_make_iov(&rcviov, 1, total_len, total_len);

    iovcnt = is_udp_zocket ? &zfur_msg.iovcnt : &zft_msg.iovcnt;
    iov_part_len = &rcviov.iov_len;
    iov = is_udp_zocket ? &zfur_msg.iov : &zft_msg.iov;
    pkts_left = is_udp_zocket ? &zfur_msg.dgrams_left : &zft_msg.pkts_left;

    *iovcnt = 1;
    *iov = &rcviov;
    *iov_part_len = part_len;

    do {
        pco_iut->op = RCF_RPC_CALL;
        rpc_zf_muxer_wait(pco_iut, set, &event, 1, -1);

        rc = rpc_send(pco_tst, sock, sndiov.iov_base, sndiov.iov_len, 0);
        if (rc != total_len)
            TEST_FAIL("send() returned unexpected value %d", rc);

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_muxer_wait(pco_iut, set, &event, 1, -1);
        if (rc < 0)
        {
            TEST_VERDICT("zf_muxer_wait() unexpectedly failed with errno",
                         RPC_ERRNO(pco_iut));
        }

        if (epollin_count == MAX_NUM_OF_EPOLLIN_EVENT)
        {
            RING_VERDICT("A large number of @c EPOLLIN events were reported");
            break;
        }

        if (event.events == RPC_EPOLLIN)
        {
            RING("The @c EPOLLIN event was accepted");

            epollin_count++;
            process_epollin_event(pco_iut, zock, is_udp_zocket,
                                  is_udp_zocket ? &zfur_msg : &zft_msg, iovcnt,
                                  pkts_left);
        }
        else if (event.events == RPC_ZF_EPOLLIN_OVERLAPPED)
        {
            process_zf_epollin_overllaped_event(pco_iut, zock, is_udp_zocket,
                                                is_udp_zocket ? &zfur_msg
                                                : &zft_msg, iovcnt, *iov, &sndiov,
                                                total_len, part_len);
            break;
        }
        else
            TEST_VERDICT("Unexpected event was accepted");
    } while (1);


    /* The memory in the case of errors in this function
     * will be released at the end of test in any case.
     * To make the code more simple we do it explicit only
     * for the success case.
     */
    rpc_release_iov(&rcviov, 1);
    rpc_release_iov(&sndiov, 1);
}
