/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Multicasts tests
 *
 * $Id$
 */

/**
 * @page multicast-check_addresses Test source and destination IP and MAC addresses
 *
 * @objective Transmit multicast datagrams from few UDP zockets, check that
 *            source and destination IP and MAC addresses are correct.
 *
 * @param one_stack     Create zockets in single stack if @c TRUE,
 *                      else - create ZF stack for each zocket.
 * @param zockets_num   Zockets number:
 *                      - 3.
 * @param func          Transmitting function.
 * @param large_buffer  Use large data buffer to send.
 * @param few_iov       Use several iov vectors.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "multicast/check_addresses"

#include "zf_test.h"
#include "tapi_igmp.h"

/** The first available IP multicast address. */
#define IP_MULTICAST_FIRST    0xe0000100

/** The last available IP multicast address. */
#define IP_MULTICAST_LAST     0xefffffff

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;

    const struct sockaddr     *iut_lladdr = NULL;
    const struct if_nameindex *tst_if = NULL;

    te_bool                 one_stack = FALSE;
    int                     zockets_num = -1;
    zfts_send_function      func;
    te_bool                 large_buffer = FALSE;
    te_bool                 few_iov = FALSE;

    rpc_zf_attr_p     attr = RPC_NULL;
    rpc_zf_stack_p   *stacks = NULL;

    struct sockaddr_storage  *mcast_ip_addrs = NULL;
    struct sockaddr_storage  *mcast_mac_addrs = NULL;
    struct sockaddr_storage  *iut_addrs = NULL;

    rpc_zfut_p  *iut_zocks = NULL;
    int         *tst_socks = NULL;

    csap_handle_t   *tst_csaps = NULL;

    int session_id = -1;

    int i = 0;
    int j = 0;

    unsigned int pkts_num;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_LINK_ADDR(iut_lladdr);
    TEST_GET_IF(tst_if);
    TEST_GET_BOOL_PARAM(one_stack);
    TEST_GET_INT_PARAM(zockets_num);
    TEST_GET_BOOL_PARAM(large_buffer);
    TEST_GET_BOOL_PARAM(few_iov);
    ZFTS_TEST_GET_ZFUT_FUNCTION(func);

    stacks = tapi_calloc(zockets_num, sizeof(*stacks));
    mcast_ip_addrs = tapi_calloc(zockets_num, sizeof(*mcast_ip_addrs));
    mcast_mac_addrs = tapi_calloc(zockets_num, sizeof(*mcast_mac_addrs));
    iut_addrs = tapi_calloc(zockets_num, sizeof(*iut_addrs));
    iut_zocks = tapi_calloc(zockets_num, sizeof(*iut_zocks));
    tst_socks = tapi_calloc(zockets_num, sizeof(*tst_socks));
    tst_csaps = tapi_calloc(zockets_num, sizeof(*tst_csaps));

    for (i = 0; i < zockets_num; i++)
    {
        stacks[i] = RPC_NULL;
        iut_zocks[i] = RPC_NULL;
        tst_socks[i] = -1;
        tst_csaps[i] = CSAP_INVALID_HANDLE;
    }

    /*- Allocate ZF attributes and ZF stack one or for each zocket
     * in dependence on @p one_stack. */

    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    for (i = 0; i < zockets_num; i++)
    {
        if (i == 0 || !one_stack)
            rpc_zf_stack_alloc(pco_iut, attr, &stacks[i]);
        else
            stacks[i] = stacks[0];
    }

    /*- Allocate and bind @p zockets_num UDP TX zockets so
     * that each zocket will send packets to different
     * multicast group. */
    /*- Create UDP sockets on Tester and subscribe to appropriate
     * multicast groups. */
    /*- Create CSAPs on Tester to count received packets filtering
     * by source and destination IP and MAC addresses. */

    rcf_ta_create_session(pco_tst->ta, &session_id);

    for (i = 0; i < zockets_num; i++)
    {
        SA(&mcast_ip_addrs[i])->sa_family = AF_INET;
        while (TRUE)
        {
            SIN(&mcast_ip_addrs[i])->sin_addr.s_addr =
                      htonl(rand_range(IP_MULTICAST_FIRST,
                                       IP_MULTICAST_LAST));
            for (j = 0; j < i; j++)
            {
                if (SIN(&mcast_ip_addrs[i])->sin_addr.s_addr ==
                        SIN(&mcast_ip_addrs[j])->sin_addr.s_addr)
                    break;
            }
            if (j >= i)
                break;
        }

        tapi_allocate_set_port(pco_tst, SA(&mcast_ip_addrs[i]));

        CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr, &iut_addrs[i]));

        rpc_zfut_alloc(pco_iut, &iut_zocks[i], stacks[i],
                       SA(&iut_addrs[i]), SA(&mcast_ip_addrs[i]), 0, attr);

        tapi_ip4_to_mcast_mac(SIN(&mcast_ip_addrs[i])->sin_addr.s_addr,
                              (uint8_t *)SA(&mcast_mac_addrs[i])->sa_data);

        CHECK_RC(tapi_ip4_eth_csap_create(
                      pco_tst->ta, session_id, tst_if->if_name,
                      TAD_ETH_RECV_DEF | TAD_ETH_RECV_NO_PROMISC,
                      (uint8_t *)SA(&mcast_mac_addrs[i])->sa_data,
                      (uint8_t *)iut_lladdr->sa_data,
                      SIN(&mcast_ip_addrs[i])->sin_addr.s_addr,
                      SIN(&iut_addrs[i])->sin_addr.s_addr,
                      IPPROTO_UDP, &tst_csaps[i]));
        CHECK_RC(tapi_tad_trrecv_start(pco_tst->ta, session_id,
                                       tst_csaps[i], NULL,
                                       TAD_TIMEOUT_INF, zockets_num,
                                       RCF_TRRECV_PACKETS));

        tst_socks[i] = rpc_socket(pco_tst, RPC_PF_INET, RPC_SOCK_DGRAM,
                                  RPC_IPPROTO_UDP);
        CHECK_RC(rpc_mcast_join(pco_tst, tst_socks[i],
                                SA(&mcast_ip_addrs[i]),
                                tst_if->if_index,
                                TARPC_MCAST_JOIN_LEAVE));
        rpc_bind(pco_tst, tst_socks[i], SA(&mcast_ip_addrs[i]));
    }

    /*- Send data from each zocket (choosing send function,
     * length of data and number of IOVs according to
     * @p func, @p large_buffer and @p few_iov). */
    for (i = 0; i < zockets_num; i++)
    {
        zfts_zfut_check_send_func(pco_iut, stacks[i], iut_zocks[i],
                                  pco_tst, tst_socks[i],
                                  func, large_buffer, few_iov);
    }

    /*- Check datagrams number received on CSAPs. */
    for (i = 0; i < zockets_num; i++)
    {
        pkts_num = 0;
        CHECK_RC(tapi_tad_trrecv_stop(pco_tst->ta, session_id,
                                      tst_csaps[i], NULL, &pkts_num));
        if (pkts_num < 1)
            TEST_VERDICT("CSAP has not seen multicast packet on Tester");
    }

    TEST_SUCCESS;

cleanup:

    for (i = 0; i < zockets_num; i++)
    {
        if (tst_socks != NULL)
            CLEANUP_RPC_CLOSE(pco_tst, tst_socks[i]);

        if (tst_csaps != NULL)
            CLEANUP_CHECK_RC(tapi_tad_csap_destroy(pco_tst->ta,
                                                   session_id,
                                                   tst_csaps[i]));

        if (iut_zocks != NULL)
            CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_zocks[i]);
    }

    if (stacks != NULL)
    {
        for (i = 0; i < zockets_num; i++)
        {
            if (i == 0 || !one_stack)
                CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_stack, stacks[i]);
        }
    }

    CLEANUP_RPC_ZF_ATTR_FREE(pco_iut, attr);

    CLEANUP_RPC_ZF_DEINIT(pco_iut);

    free(stacks);
    free(mcast_ip_addrs);
    free(mcast_mac_addrs);
    free(iut_addrs);
    free(iut_zocks);
    free(tst_socks);
    free(tst_csaps);

    TEST_END;
}
