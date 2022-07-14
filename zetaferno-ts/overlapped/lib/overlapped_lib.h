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

#ifndef __TS_OVERLAPPED_H__
#define __TS_OVERLAPPED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "zf_test.h"
#include "rpc_zf.h"

/**
 * Prepare everything for pftf reception,
 * send a packet from Tester and receive it on IUT
 *
 * @note In the case of UDP, socket on Tester should be
 *       connected.
 *
 * @param pco_iut         RPC server on IUT.
 * @param pco_tst         RPC serever on Tester.
 * @param set             Muxer set for zf_muxer_wait() call
 * @param zock_descrs     Description for each zocket
 * @param iut_zfur        Zocket to receive pftf
 * @param tst_s           Socket to send data
 * @param total_len       Total packet length
 * @param part_len        Length to check part data
 * @param zocket_type     Zocket type
 * @param extected_events Expected events on muxer
 */
void prepare_send_recv_pftf(rcf_rpc_server *pco_iut, rcf_rpc_server *pco_tst,
                            rpc_zf_muxer_set_p set, rpc_ptr zock, int sock,
                            int total_len, int part_len,
                            zfts_zocket_type zocket_type);


#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TS_OVERLAPPED_H__ */
