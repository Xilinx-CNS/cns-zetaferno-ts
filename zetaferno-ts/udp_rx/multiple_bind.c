/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id:$
 */

/**
 * @page udp_rx-multiple_bind Binding to multiple local addresses
 *
 * @objective Bind to a few local IP addresses including @c INADDR_ANY,
 *            check that datagrams are received from the bound addresses and
 *            not received from others.
 *
 * @param pco_iut           PCO on IUT.
 * @param pco_tst           PCO on TST.
 * @param same_port         Use the same port in all @b bind() calls.
 * @param first_addr_type   The first binding address type:
 *      - @c specific.
 * @param second_addr_type  The second binding address type:
 *      - @c specific.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/multiple_bind"

#include "zf_test.h"

/* IUT IP addresses number. */
#define ADDRS_NUM 3

/* Total datagrams number to send. */
#define DGRAMS_NUM (ZFTS_IOVCNT * ADDRS_NUM)

int
main(int argc, char *argv[])
{
    rcf_rpc_server          *pco_iut;
    rcf_rpc_server          *pco_tst;
    const struct sockaddr   *iut_addr1 = NULL;
    const struct sockaddr   *iut_addr2 = NULL;
    const struct sockaddr   *iut_addr3 = NULL;
    const struct sockaddr   *iut_addr[ADDRS_NUM];
    const struct sockaddr   *tst_addr = NULL;
    te_bool                  same_port;
    tapi_address_type        first_addr_type;
    tapi_address_type        second_addr_type;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zfur_p     iut_s = RPC_NULL;
    rpc_iovec      rcviov[DGRAMS_NUM] = {};
    rpc_iovec      sndiov[DGRAMS_NUM] = {};
    te_bool        recv_dgrams[ADDRS_NUM];
    te_bool        equal;
    int tst_s = -1;
    int offt;
    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr1);
    TEST_GET_ADDR(pco_iut, iut_addr2);
    TEST_GET_ADDR(pco_iut, iut_addr3);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_BOOL_PARAM(same_port);
    TEST_GET_ENUM_PARAM(first_addr_type, TAPI_ADDRESS_TYPE);
    TEST_GET_ENUM_PARAM(second_addr_type, TAPI_ADDRESS_TYPE);

    equal = same_port && first_addr_type != TAPI_ADDRESS_SPECIFIC &&
            second_addr_type != TAPI_ADDRESS_SPECIFIC;

    rpc_make_iov(sndiov, DGRAMS_NUM, 1, ZFTS_DGRAM_MAX);
    rpc_make_iov(rcviov, DGRAMS_NUM, ZFTS_DGRAM_MAX, ZFTS_DGRAM_MAX);

    iut_addr[0] = iut_addr1;
    iut_addr[1] = iut_addr2;
    iut_addr[2] = iut_addr3;

    /* Make sure ARP table is resolved. */
    for (i = 0; i < ADDRS_NUM; i++)
    {
        tapi_rpc_provoke_arp_resolution(pco_tst, iut_addr[i]);
        recv_dgrams[i] = TRUE;
    }

    /*- Allocate Zetaferno attributes and stack. */
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Allocate Zetaferno UDP RX zocket. */
    rpc_zfur_alloc(pco_iut, &iut_s, stack, attr);

    if (same_port)
    {
        te_sockaddr_set_port(SA(iut_addr2),
                             *(te_sockaddr_get_port_ptr(SA(iut_addr1))));
        te_sockaddr_set_port(SA(iut_addr3),
                             *(te_sockaddr_get_port_ptr(SA(iut_addr1))));
    }

    /*- Bind the zocket to the two first IUT addresses or @c INADDR_ANY or
     * @c NULL in dependence on @p first_addr_type and
     * @p second_addr_type, @p raddr is @c NULL. */
    RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = zfts_zfur_bind(pco_iut, iut_s, first_addr_type, iut_addr1,
                        NULL, TAPI_ADDRESS_SPECIFIC, tst_addr, 0);
    if (rc != 0)
        TEST_VERDICT("UDP RX zocket binding failed with errno %r",
                     RPC_ERRNO(pco_iut));

    if (equal)
        RPC_AWAIT_IUT_ERROR(pco_iut);
    rc = zfts_zfur_bind(pco_iut, iut_s, second_addr_type, iut_addr2,
                        NULL, TAPI_ADDRESS_SPECIFIC, tst_addr, 0);
    if (equal)
    {
        /*- Double bind to the same address couple should fail with errno
         * @c EADDRINUSE. */
        if (rc < 0 && RPC_ERRNO(pco_iut) == RPC_EADDRINUSE)
            TEST_SUCCESS;

        if (rc == 0)
            TEST_VERDICT("The second bind of UDP RX zocket unexpectedly "
                         "passed");

        TEST_VERDICT("The second bind of UDP RX zocket failed with wrong "
                     "errno: %r instead of %r", RPC_ERRNO(pco_iut),
                     RPC_EEXIST);
    }
    else if (rc != 0)
        TEST_VERDICT("The second bind of UDP RX zocket failed with "
                     "errno %r", RPC_ERRNO(pco_iut));

    /*- If one of binding addresses is @c INADDR_ANY or @c NULL and @p
     * same_port is @c TRUE, then datagrams sent to the third IUT
     * address should also be received. */
    if (same_port == FALSE || (first_addr_type == TAPI_ADDRESS_SPECIFIC &&
                               second_addr_type == TAPI_ADDRESS_SPECIFIC))
        recv_dgrams[2] = FALSE;

    /*- Create UDP socket on tester. */
    tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                       RPC_SOCK_DGRAM, RPC_PROTO_DEF);
    rpc_bind(pco_tst, tst_s, tst_addr);

    /*- Send a few datagrams from the tester to all three IUT addresses. */
    for (i = 0; i < ADDRS_NUM; i++)
    {
        offt =  ZFTS_IOVCNT * i;
        zfts_sendto_iov(pco_tst, tst_s, sndiov + offt, ZFTS_IOVCNT,
                        iut_addr[i]);

        /*- Receive and check all datagrams which are sent to bound IUT
         * addresses. */
        if (recv_dgrams[i])
        {
            zfts_zfur_zc_recv_all(pco_iut, stack, iut_s, rcviov + offt,
                                  ZFTS_IOVCNT);
            rpc_iovec_cmp_strict(rcviov + offt, sndiov + offt, ZFTS_IOVCNT);
        }
        else
        {
            /* Timeout to make sure datagrams are delivered. */
            TAPI_WAIT_NETWORK;

            /*- Check that no extra datagrams are received. */
            zfts_zfur_check_not_readable(pco_iut, stack, iut_s);
        }
    }

    TEST_SUCCESS;

cleanup:
    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    rpc_release_iov(rcviov, ZFTS_IOVCNT);
    rpc_release_iov(sndiov, ZFTS_IOVCNT);

    TEST_END;
}
