/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test API - Zetaferno Direct API RPC functions definition
 *
 * Internal definitions used by TAPI for ZF API remote calls.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru
 *
 * $Id$
 */

#ifndef ___RPC_ZF_INTERNAL_H__
#define ___RPC_ZF_INTERNAL_H__

#include "te_config.h"

#if HAVE_STRING_H
#include <string.h>
#endif

#include "tapi_rpc_internal.h"
#include "tapi_rpc_unistd.h"
#include "te_rpc_sys_socket.h"
#include "te_rpc_sys_epoll.h"
#include "te_string.h"
#include "zf_test.h"

/** Auxiliary buffer for logging structures or other data converted to
 * string representation. */
extern te_string log_strbuf;

/**
 * Network address format string for logging.
 */
#define ZFTS_ADDR_FMT "%s [len=%d %s]"

/**
 * Description of network address according to ZFTS_ADDR_FMT.
 */
#define ZFTS_ADDR_VAL(addr) \
    sockaddr_h2str(addr), (int) addr ## len, \
    (fwd_ ## addr ## len ? "passed" : "not passed")

/**
 * Description of network address according to ZFTS_ADDR_FMT
 * (address string is printed in a statically allocated buffer).
 *
 * @param addr        Address.
 * @param sbuf        Where to print string representation of an address.
 */
#define ZFTS_ADDR_VAL_SBUF(addr, sbuf) \
    SOCKADDR_H2STR_SBUF(addr, sbuf), (int) addr ## len, \
    (fwd_ ## addr ## len ? "passed" : "not passed")

/**
 * Convert @p iov vectors array to TE string representation.
 *
 * @param iov       Iov vectors array.
 * @param iovcnt    The array length.
 * @param str       TE string to keep the line.
 */
extern void rpc_iovec2str(rpc_iovec *iov, size_t iovcnt,
                          te_string *str);

/**
 * Convert @c struct @c rpc_iovec to @c tarpc_iovec.
 *
 * @param iov           RPC Vectors array.
 * @param tarpc_iov     tarpc vectors array.
 * @param iovcnt        Arrays length.
 */
extern void rpc_iovec2tarpc_iovec(rpc_iovec *iov, tarpc_iovec *tarpc_iov,
                                  size_t iovcnt);

/**
 * Convert @c tarpc_iovec to @c rpc_iovec.
 *
 * @param tarpc_iov     TARPC vectors array.
 * @param iov           RPC vectors array.
 * @param iovcnt        Arrays length.
 */
extern void tarpc_iovec2rpc_iovec(tarpc_iovec *tarpc_iov,
                                  rpc_iovec *iov, int iovcnt);

/**
 * Get string representation of a single tarpc_zf_pkt_report value.
 *
 * @param report        Pointer to tarpc_zf_pkt_report value.
 * @param str           String where to append string representation.
 *
 * @return Status code.
 */
extern te_errno zf_pkt_report_rpc2str(tarpc_zf_pkt_report *report,
                                      te_string *str);

/**
 * Get string representation of an array of tarpc_zf_pkt_report values.
 *
 * @param reports       Pointer to the array.
 * @param count         Number of elements in the array.
 * @param str           String where to append string representation.
 *
 * @return Status code.
 */
extern te_errno zf_pkt_reports_rpc2str(tarpc_zf_pkt_report *reports,
                                       int count, te_string *str);

#endif /* !___RPC_ZF_INTERNAL_H__ */
