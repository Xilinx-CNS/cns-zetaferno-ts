/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id$
 */

/**
 * @page udp_rx-multiple_unbind Bind and unbind UDP RX zocket many times
 *
 * @objective Bind UDP RX zocket to a few address couples with different
 *            local or remote address or port, then repeatively unbind it
 *            from some of them and check that only packets sent to bound
 *            addresses are received.
 *
 * @param diff_port           If @c TRUE bind to addresses with different
 *                            port numbers, else different IP should be
 *                            used. Local or remote addresses can differ in
 *                            dependence on @p diff_laddr.
 * @param diff_laddr          If @c TRUE binding local addresses are
 *                            different, else - remote addresses.
 * @param zero_port           Value @c TRUE is applicable only when both
 *                            @p diff_port and @p diff_laddr are @c TRUE,
 *                            use zero port in @c laddr in this case.
 * @param binds_number        How many times zocket should be bound:
 *                            - @c 10
 * @param iterations_number   Main loop iterations number:
 *                            - @c 100
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "udp_rx/multiple_unbind"

#include "zf_test.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server            *pco_iut = NULL;
    rcf_rpc_server            *pco_tst = NULL;
    const struct sockaddr     *iut_addr = NULL;
    const struct sockaddr     *tst_addr = NULL;
    const struct if_nameindex *iut_if = NULL;
    const struct if_nameindex *tst_if = NULL;
    tapi_env_net              *net = NULL;

    te_bool diff_port = FALSE;
    te_bool diff_laddr = FALSE;
    te_bool zero_port = FALSE;
    int     binds_number = 0;
    int     iterations_number = 0;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    int i = 0;
    int j = 0;
    int k = 0;

    struct sockaddr **laddr_list = NULL;
    struct sockaddr **raddr_list = NULL;
    struct sockaddr **same_addrs = NULL;
    struct sockaddr **diff_addrs = NULL;

    te_bool *pkt_received = NULL;
    te_bool *addr_unbound = NULL;

    rpc_zfur_p       iut_zfur = RPC_NULL;
    int              tst_s = -1;

    int unbind_num = -1;

    char send_data[ZFTS_DGRAM_MAX];
    char recv_data[ZFTS_DGRAM_MAX];

    rpc_iovec     iov;
    rpc_zfur_msg  msg;

    TEST_START;
    TEST_GET_NET(net);
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_IF(iut_if);
    TEST_GET_IF(tst_if);
    TEST_GET_BOOL_PARAM(diff_port);
    TEST_GET_BOOL_PARAM(diff_laddr);
    TEST_GET_BOOL_PARAM(zero_port);
    TEST_GET_INT_PARAM(binds_number);
    TEST_GET_INT_PARAM(iterations_number);

    te_fill_buf(send_data, ZFTS_DGRAM_MAX);

    laddr_list = tapi_env_add_addresses(pco_iut, net, AF_INET,
                                        iut_if, binds_number);
    CHECK_NOT_NULL(laddr_list);
    raddr_list = tapi_env_add_addresses(pco_tst, net, AF_INET,
                                        tst_if, binds_number);
    CHECK_NOT_NULL(raddr_list);

    pkt_received = tapi_calloc(binds_number, sizeof(*pkt_received));
    addr_unbound = tapi_calloc(binds_number, sizeof(*addr_unbound));

    for (i = 0; i < binds_number; i++)
    {
        tapi_rpc_provoke_arp_resolution(pco_tst, laddr_list[i]);
    }

    if (diff_laddr)
    {
        same_addrs = raddr_list;
        diff_addrs = laddr_list;
    }
    else
    {
        same_addrs = laddr_list;
        diff_addrs = raddr_list;
    }

    for (i = 1; i < binds_number; i++)
    {
        tapi_sockaddr_clone_exact(same_addrs[0],
                                  (struct sockaddr_storage *)same_addrs[i]);
        if (diff_port)
        {
            CHECK_RC(te_sockaddr_set_netaddr(
                          diff_addrs[i],
                          te_sockaddr_get_netaddr(diff_addrs[0])));
            if (zero_port && diff_laddr)
                te_sockaddr_set_port(diff_addrs[i], 0);
        }
        else
            te_sockaddr_set_port(diff_addrs[i],
                                 te_sockaddr_get_port(diff_addrs[0]));
    }

    if (diff_port && zero_port && diff_laddr)
        te_sockaddr_set_port(laddr_list[0], 0);

    /*- Allocate ZF attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    /*- Allocate UDP RX zocket. */
    rpc_zfur_alloc(pco_iut, &iut_zfur, stack, attr);

    /*- Bind the zocket @p binds_number times:
     *  - use different local or remote address or port in dependence on
     *    parameters @p diff_laddr and @p diff_port;
     *  - use zero port in local address if @p zero_port is @c TRUE. */

    for (k = 0; k < binds_number; k++)
    {
        rpc_zfur_addr_bind(pco_iut, iut_zfur,
                           laddr_list[k], raddr_list[k], 0);
    }

    /*- Repeat in the loop @p iterations_number times:
     *  - unbind from random number (in the range [1; @p binds_number])
     *    of addresses;
     *  - send datagrams using all addresses;
     *  - check that datagrams corresponding to bound addresses are received
     *    and others are ignored;
     *  - bind back to addresses which were unbound. */

    for (i = 0; i < iterations_number; i++)
    {
        for (k = 0; k < binds_number; k++)
        {
            pkt_received[k] = FALSE;
            addr_unbound[k] = FALSE;
        }

        unbind_num = rand_range(1, binds_number);
        while (unbind_num > 0)
        {
            k = rand_range(0, binds_number - 1);
            if (addr_unbound[k])
                continue;

            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zfur_addr_unbind(pco_iut, iut_zfur,
                                      laddr_list[k],
                                      raddr_list[k], 0);
            if (rc < 0)
                TEST_VERDICT("zfur_addr_unbind() failed unexpectedly "
                             "with errno %r", RPC_ERRNO(pco_iut));

            addr_unbound[k] = TRUE;
            unbind_num--;
        }

        for (k = 0; k < binds_number; k++)
        {
            tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                               RPC_SOCK_DGRAM, RPC_PROTO_DEF);
            rpc_bind(pco_tst, tst_s, raddr_list[k]);
            *((int *)send_data) = k;
            rpc_sendto(pco_tst, tst_s, send_data, ZFTS_DGRAM_MAX, 0,
                       laddr_list[k]);
            RPC_CLOSE(pco_tst, tst_s);
        }

        ZFTS_WAIT_NETWORK(pco_iut, stack);

        while (TRUE)
        {
            iov.iov_base = recv_data;
            iov.iov_rlen = iov.iov_len = ZFTS_DGRAM_MAX;
            memset(&msg, 0, sizeof(msg));
            msg.iov = &iov;
            msg.iovcnt = 1;

            rpc_zfur_zc_recv(pco_iut, iut_zfur, &msg, 0);
            if (msg.iovcnt > 0)
            {
                j = *((int *)recv_data);
                if (j < 0 || j >= binds_number ||
                    msg.iov[0].iov_len != ZFTS_DGRAM_MAX ||
                    memcmp(send_data + sizeof(int),
                           recv_data + sizeof(int),
                           ZFTS_DGRAM_MAX - sizeof(int)) != 0)
                    TEST_FAIL("Invalid data was received");

                if (addr_unbound[j])
                    TEST_VERDICT("Packet sent to unbound address "
                                 "was received");
                pkt_received[j] = TRUE;

                rpc_zfur_zc_recv_done(pco_iut, iut_zfur, &msg);
                rpc_zf_process_events(pco_iut, stack);
            }
            else
                break;
        }

        for (k = 0; k < binds_number; k++)
        {
            if (!pkt_received[k] && !addr_unbound[k])
                TEST_VERDICT("Packet sent to bound address "
                             "was not received");

            if (addr_unbound[k])
            {
                RPC_AWAIT_ERROR(pco_iut);
                rc = rpc_zfur_addr_bind(pco_iut, iut_zfur,
                                        laddr_list[k],
                                        raddr_list[k], 0);
                if (rc < 0)
                    TEST_VERDICT("zfur_addr_bind() failed with errno %r "
                                 "while trying to rebind to unbound "
                                 "address",
                                 RPC_ERRNO(pco_iut));
            }
        }

    }
    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_zfur);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    free(pkt_received);
    free(addr_unbound);
    free(laddr_list);
    free(raddr_list);

    TEST_END;
}

