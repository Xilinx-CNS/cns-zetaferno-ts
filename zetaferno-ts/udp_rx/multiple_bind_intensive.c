/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * UDP RX tests
 *
 * $Id:$
 */

/**
 * @page udp_rx-multiple_bind_intensive Bind zocket a few times, check it receives only expected datagrams
 *
 * @objective  Bind a few times with different combinations of @b laddr and
 *             @b raddr adrress and port values. Send a number of datagrams
 *             from tester with various source and destination addresses -
 *             as expected by IUT and unexpected (wrong port or IP of
 *             source or destination). Check that IUT zocket receives
 *             only expected datagrams.
 *
 * @param pco_iut           PCO on IUT.
 * @param pco_tst           PCO on TST.
 * @param iut_addr          IUT address.
 * @param tst_addr          Tester address.
 * @param binds_number      How many times to bind.
 * @param datagrams_number  Datagrams number to be sent.
 * @param laddr_diff_port   Local address for the next binding has different
 *                          port number if @c TRUE.
 * @param laddr_diff_addr   Local address for the next binding has different
 *                          IP address if @c TRUE.
 * @param raddr_diff_port   Remote address for the next binding has different
 *                          port number if @c TRUE.
 * @param raddr_diff_addr   Remote address for the next binding has different
 *                          IP address if @c TRUE.
 *
 * @type Conformance.
 *
 * @reference @ref ZF_DOC
 *
 * @par Scenario:
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_TEST_NAME  "udp_rx/multiple_bind_intensive"

#include "zf_test.h"
#include "rpc_zf.h"

/* Datagrams bunch_size size to be sent at a time. */
#define BUNCH_SIZE 30

/**
 * Generate random address based on possible IP and port ranges in
 * @p addr_list array. New address is allocated from the heap.
 *
 * @param addr_list     Addresses array.
 * @param len           The array length.
 *
 * @return Pointer to the generated address.
 */
static struct sockaddr *
get_rand_addr(struct sockaddr **addr_list, int len)
{
    struct sockaddr *addr;
    int i = rand_range(0, len - 1);

    addr = tapi_sockaddr_clone_typed(addr_list[i], TAPI_ADDRESS_SPECIFIC);

    i = rand_range(0, len - 1);
    te_sockaddr_set_port(addr, te_sockaddr_get_port(addr_list[i]));

    return addr;
}

/**
 * Determine if sent datagram should be received or not. The criteria is
 * there is addresses couple in arrays @p laddr_list and @p raddr_list which
 * match to the couple @p tst_laddr and @p tst_raddr.
 *
 * @param laddr_list    Local addresses array.
 * @param raddr_list    Remote addresses array.
 * @param len           The arrays length.
 * @param tst_laddr     Tester local address.
 * @param tst_laddr     Tester remote address.
 * @param raddr_null    Index of IUT address which is bound to wildcard.
 *
 * @return @c TRUE if datagram should be received.
 */
static te_bool
dgram_is_expected(struct sockaddr **laddr_list,
                  struct sockaddr **raddr_list, int len,
                  struct sockaddr *tst_laddr, struct sockaddr *tst_raddr,
                  int raddr_null)
{
    int i;

    for (i = 0; i < len; i++)
    {
        if (tapi_sockaddr_cmp(laddr_list[i], tst_raddr) == 0 &&
            (tapi_sockaddr_cmp(raddr_list[i], tst_laddr) == 0 ||
             i == raddr_null))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Check that @p iovs and @p iov_ptr_arr include identical vectors
 * (possibly in different order).
 * The function jumps to cleanup with verdict, if at least one vector
 * from @p iovs is not equal to some vector in @p iov_ptr_arr.
 *
 * @param iov_ptr_arr   Array of pointers to @b rpc_iovec elements.
 * @param iovs          Array of vectors which are to be compared.
 * @param len           Number of elements in @p iov_ptr_arr and @p iovs.
 */
static void
iovec_cmp(rpc_iovec **iov_ptr_arr, rpc_iovec *iovs, int len)
{
    rpc_iovec *iov_cmp;
    rpc_iovec *iov;
    te_bool    matched;
    int        i;
    int        j;
    te_bool   *matched_flag;

    matched_flag = tapi_calloc(len, sizeof(te_bool));

    for (i = 0; i < len; ++i)
    {
        iov = &iovs[i];
        matched = FALSE;
        for (j = 0; j < len; ++j)
        {
            iov_cmp = iov_ptr_arr[j];
            if (!matched_flag[j] &&
                iov->iov_len == iov_cmp->iov_len &&
                memcmp(iov->iov_base, iov_cmp->iov_base, iov->iov_len) == 0)
            {
                matched = TRUE;
                matched_flag[j] = TRUE;
                break;
            }
        }
        if (!matched)
        {
            free(matched_flag);
            TEST_VERDICT("One of buffers is corrupted "
                         "or has incorrect length");
        }
    }
    free(matched_flag);
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server            *pco_iut;
    rcf_rpc_server            *pco_tst;
    const struct sockaddr     *iut_addr = NULL;
    const struct sockaddr     *tst_addr = NULL;
    const struct if_nameindex *tst_if = NULL;
    const struct if_nameindex *iut_if = NULL;
    tapi_env_net              *net = NULL;
    int     binds_number;
    int     datagrams_number;
    te_bool laddr_diff_port;
    te_bool laddr_diff_addr;
    te_bool raddr_diff_port;
    te_bool raddr_diff_addr;

    struct sockaddr **laddr_list = NULL;
    struct sockaddr **raddr_list = NULL;
    struct sockaddr  *tst_laddr[BUNCH_SIZE];
    struct sockaddr  *tst_raddr[BUNCH_SIZE];
    rpc_zf_attr_p    attr = RPC_NULL;
    rpc_zf_stack_p   stack = RPC_NULL;
    rpc_zfur_p       iut_s = RPC_NULL;
    rpc_iovec        sndiov[BUNCH_SIZE];
    rpc_iovec        rcviov[BUNCH_SIZE];
    rpc_iovec       *exp_sndiov[BUNCH_SIZE];
    te_bool          exp;

    int raddr_null = -1;
    int count = 0;
    int ip_num;
    int tst_s = -1;
    int bunch_size;
    int i;
    int j;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_IF(tst_if);
    TEST_GET_IF(iut_if);
    TEST_GET_NET(net);
    TEST_GET_INT_PARAM(binds_number);
    TEST_GET_INT_PARAM(datagrams_number);
    TEST_GET_BOOL_PARAM(laddr_diff_port);
    TEST_GET_BOOL_PARAM(laddr_diff_addr);
    TEST_GET_BOOL_PARAM(raddr_diff_port);
    TEST_GET_BOOL_PARAM(raddr_diff_addr);

    /* Invalid case - multiple bind to the exactly same addresses couple. */
    if (!laddr_diff_port && !laddr_diff_addr && !raddr_diff_port &&
        !raddr_diff_addr)
    {
        RING("Meaningless iteration, skipping. See bug 83502 for details.");
        TEST_SUCCESS;
    }

    /*- If @p laddr_diff_port or @p laddr_diff_addr is @c TRUE - @c NULL is
     * used as @b raddr argument in one of calls @a rpc_zfur_addr_bind(). */
    if (laddr_diff_port || laddr_diff_addr)
        raddr_null = rand_range(0, binds_number - 1);

    ip_num = binds_number + 1;

    /*- Allocate and add @p binds_number IP addresses on IUT and Tester. */
    laddr_list = tapi_env_add_addresses(pco_iut, net, AF_INET, iut_if, ip_num);
    CHECK_NOT_NULL(laddr_list);
    raddr_list = tapi_env_add_addresses(pco_tst, net, AF_INET, tst_if, ip_num);
    CHECK_NOT_NULL(raddr_list);

    /* Resolve ARP for new added IPs. */
    for (i = 0; i < ip_num; i++)
    {
        tapi_rpc_provoke_arp_resolution(pco_tst, laddr_list[i]);
    }

    /*- Allocate Zetaferno attributes and stack. */
    rpc_zf_init(pco_iut);
    rpc_zf_attr_alloc(pco_iut, &attr);
    rpc_zf_stack_alloc(pco_iut, attr, &stack);

    /*- Allocate Zetaferno UDP RX zocket. */
    rpc_zfur_alloc(pco_iut, &iut_s, stack, attr);

    /*- Both local and remote addresses for binding can have different IP
     * and/or port for each next @a rpc_zfur_addr_bind() call. It is
     * dependence on arguments @p laddr_diff_port, @p laddr_diff_addr,
     * @p raddr_diff_port, @p raddr_diff_addr. */
    for (i = 1; i < binds_number; i++)
    {
        if (!laddr_diff_port)
            te_sockaddr_set_port(laddr_list[i],
                                 te_sockaddr_get_port(laddr_list[0]));

        if (!laddr_diff_addr)
            CHECK_RC(te_sockaddr_set_netaddr(laddr_list[i],
                te_sockaddr_get_netaddr(laddr_list[0])));

        if (!raddr_diff_port)
            te_sockaddr_set_port(raddr_list[i],
                                 te_sockaddr_get_port(raddr_list[0]));

        if (!raddr_diff_addr)
            CHECK_RC(te_sockaddr_set_netaddr(raddr_list[i],
                te_sockaddr_get_netaddr(raddr_list[0])));
    }

    /*- Bind the zocket number @p binds_number times. */
    for (i = 0; i < binds_number; i++)
        rpc_zfur_addr_bind(pco_iut, iut_s, laddr_list[i],
                           raddr_null == i ? NULL : raddr_list[i], 0);

    /*- Send @p datagrams_number datagrams from tester:
     *   * source and destination address:port are chosen randomly from the
     * range:
     *      * source and IP as well as port can be one of bound IUT
     *        addresses + @c 1 extra (unbound) address/port;
     *   * datagrams are sent in bunches in the loop.
     */
     /*- Receive datagrams bunches:
     *   * read datagrams on IUT
     *   * check that only expected datagrams are received;
     *   * acceptance creiteria is:
     *      * datagram destination address:port is equal to @b laddr;
     *      * datagram source address:port is equal to @b raddr.
     *      * @b laddr and @b raddr are bound addresses couple.
     */
    for (i = 0; i < datagrams_number; i++)
    {
        count = 0;
        bunch_size = datagrams_number - i;
        if (bunch_size > BUNCH_SIZE)
            bunch_size = BUNCH_SIZE;

        rpc_make_iov(sndiov, bunch_size, 1, ZFTS_DGRAM_MAX);
        rpc_make_iov(rcviov, bunch_size, ZFTS_DGRAM_MAX, ZFTS_DGRAM_MAX);

        for (j = 0; j < bunch_size; i++, j++)
        {
            tst_laddr[j] = get_rand_addr(raddr_list, ip_num);
            tst_raddr[j] = get_rand_addr(laddr_list, ip_num);

            tst_s = rpc_socket(pco_tst, rpc_socket_domain_by_addr(tst_addr),
                               RPC_SOCK_DGRAM, RPC_PROTO_DEF);
            rpc_bind(pco_tst, tst_s, tst_laddr[j]);
            zfts_sendto_iov(pco_tst, tst_s, sndiov + j, 1, tst_raddr[j]);

            RPC_CLOSE(pco_tst, tst_s);

            exp = dgram_is_expected(laddr_list, raddr_list, binds_number,
                                    tst_laddr[j], tst_raddr[j], raddr_null);
            if (exp)
                exp_sndiov[count++] = sndiov + j;

            free(tst_laddr[j]);
            free(tst_raddr[j]);
        }

        zfts_zfur_zc_recv_all(pco_iut, stack, iut_s, rcviov, count);

        iovec_cmp(exp_sndiov, rcviov, count);

        zfts_zfur_check_not_readable(pco_iut, stack, iut_s);

        rpc_release_iov(rcviov, bunch_size);
        rpc_release_iov(sndiov, bunch_size);
        RING("Received datagrams number %d", count);
    }

    TEST_SUCCESS;

cleanup:
    if (stack != RPC_NULL)
    {
        CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s);
        CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);
    }

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    TEST_END;
}
