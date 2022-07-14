/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/**
 * @brief Auxiliary RPC API definition
 *
 * Definition of auxiliary API which can be used in RPC routines
 * implementation.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#ifndef __ZF_RPC_H__
#define __ZF_RPC_H__

#include <zf/zf.h>
#include <etherfabric/ef_vi.h>

static inline int
zf_zc_flags_rpc2h(int rpc_flags)
{
    int flags = 0;

#define ZF_ZC_FLAGS_RPC2H(_flag) \
    if ((rpc_flags & TARPC_##_flag) == TARPC_##_flag)  \
        flags |= _flag;

#ifdef ZF_EPOLLIN_OVERLAPPED
    ZF_ZC_FLAGS_RPC2H(ZF_OVERLAPPED_WAIT)
    ZF_ZC_FLAGS_RPC2H(ZF_OVERLAPPED_COMPLETE)
#endif

#undef ZF_ZC_FLAGS_RPC2H

    return flags;
}

/**
 * Convert clock synchronization flags from RPC to native values.
 *
 * @param rpc_flags       Value to convert.
 *
 * @return Converted value.
 */
static inline unsigned int
zf_sync_flags_rpc2h(unsigned int rpc_flags)
{
    unsigned int res = 0;

#ifdef EF_VI_SYNC_FLAG_CLOCK_SET
    if (rpc_flags & TARPC_ZF_SYNC_FLAG_CLOCK_SET)
        res = res | EF_VI_SYNC_FLAG_CLOCK_SET;
#endif
#ifdef EF_VI_SYNC_FLAG_CLOCK_IN_SYNC
    if (rpc_flags & TARPC_ZF_SYNC_FLAG_CLOCK_IN_SYNC)
        res = res | EF_VI_SYNC_FLAG_CLOCK_IN_SYNC;
#endif

    return res;
}

/**
 * Convert clock synchronization flags from native to RPC values.
 *
 * @param flags       Value to convert.
 *
 * @return Converted value.
 */
static inline unsigned int
zf_sync_flags_h2rpc(unsigned int flags)
{
    unsigned int res = 0;

#ifdef EF_VI_SYNC_FLAG_CLOCK_SET
    if (flags & EF_VI_SYNC_FLAG_CLOCK_SET)
        res = res | TARPC_ZF_SYNC_FLAG_CLOCK_SET;
#endif
#ifdef EF_VI_SYNC_FLAG_CLOCK_IN_SYNC
    if (flags & EF_VI_SYNC_FLAG_CLOCK_IN_SYNC)
        res = res | TARPC_ZF_SYNC_FLAG_CLOCK_IN_SYNC;
#endif

    return res;
}

/**
 * Convert ZF packet report flags from native to RPC values.
 *
 * @param flags       Value to convert
 *
 * @return Converted value.
 */
static inline unsigned int
zf_pkt_report_flags_h2rpc(unsigned int flags)
{
    unsigned int res = 0;

#define PKT_REPORT_FLAGS_H2RPC(_flag) \
    if (flags & ZF_PKT_REPORT_ ## _flag)  \
    {                                             \
        res |= TARPC_ZF_PKT_REPORT_ ## _flag;     \
        flags &= ~ZF_PKT_REPORT_ ## _flag;        \
    }

#ifdef ZF_PKT_REPORT_CLOCK_SET
    PKT_REPORT_FLAGS_H2RPC(CLOCK_SET);
#endif
#ifdef ZF_PKT_REPORT_IN_SYNC
    PKT_REPORT_FLAGS_H2RPC(IN_SYNC);
#endif
#ifdef ZF_PKT_REPORT_NO_TIMESTAMP
    PKT_REPORT_FLAGS_H2RPC(NO_TIMESTAMP);
#endif
#ifdef ZF_PKT_REPORT_DROPPED
    PKT_REPORT_FLAGS_H2RPC(DROPPED);
#endif
#ifdef ZF_PKT_REPORT_TCP_RETRANS
    PKT_REPORT_FLAGS_H2RPC(TCP_RETRANS);
#endif
#ifdef ZF_PKT_REPORT_TCP_SYN
    PKT_REPORT_FLAGS_H2RPC(TCP_SYN);
#endif
#ifdef ZF_PKT_REPORT_TCP_FIN
    PKT_REPORT_FLAGS_H2RPC(TCP_FIN);
#endif

    if (flags != 0)
        res |= TARPC_ZF_PKT_REPORT_UNKNOWN;

    return res;
#undef PKT_REPORT_FLAGS_H2RPC
}

/**
 *  Get Zetaferno UDP send function.
 *
 * @param send_func Send function index.
 * @param func      Poointer to the function.
 *
 * @return @c Zero on success or @c -1 on failure.
 */
extern int zfut_get_send_function(zfts_send_function send_func,
                                  api_func_ptr *func);

/**
 * Prepare array of ZF packet reports to be passed to ZF functions
 * like zfut_get_tx_timestamps() and zft_get_tx_timestamps().
 *
 * @param reps_out        Where to save pointer to array of report
 *                        structures.
 * @param reps_copy_out   Where to save pointer to copy of the
 *                        array (used for checking that array
 *                        is not changed beyond retrieved count).
 * @param tarpc_reports   TARPC report structures passed by the RPC caller.
 * @param reports_num     Number of structures in @p tarpc_reports.
 *
 * @return @c 0 on success, @c -1 on failure.
 */
extern int prepare_pkt_reports(struct zf_pkt_report **reps_out,
                               struct zf_pkt_report **reps_copy_out,
                               tarpc_zf_pkt_report *tarpc_reports,
                               size_t reports_num);

/**
 * Process array of ZF packet reports after calling ZF functions
 * like zfut_get_tx_timestamps() and zft_get_tx_timestamps().
 *
 * @param reports         Pointer to the array of report structures.
 * @param reports_copy    Copy of the array made before calling the
 *                        target function.
 * @param tarpc_reports   TARPC report structures to be filled with
 *                        data from @p reports.
 * @param reports_num     Number of structures in @p tarpc_reports.
 * @param count           Count of structures returned from the function.
 *
 * @return @c 0 on success, @c -1 on failure.
 */
extern int process_pkt_reports(struct zf_pkt_report *reports,
                               struct zf_pkt_report *reports_copy,
                               tarpc_zf_pkt_report *tarpc_reports,
                               size_t reports_num, int count);

#endif /* !__ZF_RPC_H__ */
