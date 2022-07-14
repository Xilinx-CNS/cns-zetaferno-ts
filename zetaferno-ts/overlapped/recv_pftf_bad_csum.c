/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Overlapped API tests
 */

/** @page overlapped-recv_pftf_bad_csum Receiving packets with bad or zero UDP layer checksum using PFTF API
 *
 * @objective Check that PFTF API receives packets with correct or zero
 *            checksum and drops packets with bad checksum.
 *
 * @param env           Network environment configuration:
 *                      - @ref arg_types_env_peer2peer_mcast
 * @param total_len     Length of datagram to send from TST
 * @param part_len      Length of data to check overlapped reception
 * @param mcast_bind    Whether to use multicast address:
 *                      - @c FALSE: Use unicast address to bind
 *                      - @c TRUE: Use multicast address to bind
 * @param raddr_bind    Whether to use remote address to bind zocket:
 *                      - @c FALSE: Traffic can be accepted from all
 *                                  remote addresses
 *                      - @c TRUE: Accept traffic from only one remote adress
 * @param checksum      UDP checksum description:
 *                      - @c bad: Bad UDP layer checksum
 *                      - @c zero: Zero UDP layer checksum
 *                      - @c correct: Correct UDP layer checksum
 * @par Scenario:
 *
 * @author Denis Pryazhennikov <Denis.Pryazhennikov@oktetlabs.ru>
 */

#define TE_TEST_NAME "overlapped/recv_pftf_bad_csum"

#include "zf_test.h"
#include "rpc_zf.h"

#ifdef HAVE_NET_ETHERNET_H
#include <net/ethernet.h>
#endif

#include "tapi_tad.h"
#include "tapi_ip4.h"
#include "tapi_udp.h"
#include "tapi_cfg_base.h"
#include "ndn.h"

/** Buffer size to hold string representation of pkt template */
#define BUF_SIZE 1024
/** Timeout for zf_muxer_wait(), in ms */
#define MUXER_TIMEOUT 3000
/** Maximum number of attempts to get ZF_EPOLLIN_OVERLAPPED event */
#define MAX_ATTEMPTS 10

int
main(int argc, char *argv[])
{
    rcf_rpc_server            *pco_iut = NULL;
    rcf_rpc_server            *pco_tst = NULL;
    const struct sockaddr     *iut_addr = NULL;
    const struct sockaddr     *mcast_addr = NULL;
    const struct sockaddr     *tst_addr = NULL;
    const struct if_nameindex *iut_if;
    const struct if_nameindex *tst_if;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;
    rpc_zfur_p     iut_s = RPC_NULL;

    uint8_t mac_iut[ETHER_ADDR_LEN];
    uint8_t mac_tst[ETHER_ADDR_LEN];

    rpc_zf_muxer_set_p     muxer_set = RPC_NULL;
    rpc_zf_waitable_p      iut_zfur_waitable = RPC_NULL;
    struct rpc_epoll_event event;

    int          part_len;
    int          total_len;
    int          exp_len;
    te_bool      mcast_bind;
    te_bool      raddr_bind;
    const char  *checksum;
    int          csum_val;

    rpc_iovec      sndiov;
    rpc_iovec      rcviov;
    rpc_zfur_msg   msg;

    csap_handle_t csap = CSAP_INVALID_HANDLE;
    asn_value    *pkt = NULL;
    char          buf[BUF_SIZE];
    char          oid[RCF_MAX_ID];
    int           sid;
    int           num;

    int i;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_IF(iut_if);
    TEST_GET_IF(tst_if);
    TEST_GET_INT_PARAM(part_len);
    TEST_GET_INT_PARAM(total_len);
    TEST_GET_BOOL_PARAM(raddr_bind);
    TEST_GET_BOOL_PARAM(mcast_bind);
    TEST_GET_STRING_PARAM(checksum);

    if (mcast_bind)
        TEST_GET_ADDR(pco_iut, mcast_addr);

    if (strcmp(checksum, "correct") == 0)
        csum_val = TE_IP4_UPPER_LAYER_CSUM_CORRECT;
    else if (strcmp(checksum, "bad") == 0)
        csum_val = TE_IP4_UPPER_LAYER_CSUM_BAD;
    else if (strcmp(checksum, "zero") == 0)
        csum_val = TE_IP4_UPPER_LAYER_CSUM_ZERO;
    else
        TEST_FAIL("Incorrect value of 'checksum' parameter");

    rpc_make_iov(&sndiov, 1, total_len, total_len);
    rpc_make_iov(&rcviov, 1, total_len, total_len);

    snprintf(oid, RCF_MAX_ID, "/agent:%s/interface:%s", pco_iut->ta,
             iut_if->if_name);
    if (tapi_cfg_base_if_get_mac(oid, mac_iut) != 0)
        TEST_FAIL("Failed to get MAC address of %s", iut_if->if_name);

    snprintf(oid, RCF_MAX_ID, "/agent:%s/interface:%s", pco_tst->ta,
             tst_if->if_name);
    if (tapi_cfg_base_if_get_mac(oid, mac_tst) != 0)
        TEST_FAIL("Failed to get MAC address of %s", tst_if->if_name);

    TEST_STEP("Allocate Zetaferno attributes and stack.");
    zfts_create_stack(pco_iut, &attr, &stack);

    TEST_STEP("Allocate Zetaferno UDP RX zocket.");
    rpc_zfur_alloc(pco_iut, &iut_s, stack, attr);

    TEST_STEP("Bind the zocket using local and remote addresses. "
              "Local address may be unicast or multicast. "
              "Remote address may be @c NULL or unicast. ");
    rpc_zfur_addr_bind(pco_iut, iut_s,
                       mcast_bind ? SA(mcast_addr) : SA(iut_addr),
                       raddr_bind ? tst_addr : NULL, 0);

    TEST_STEP("Allocate a muxer set and add the zocket to it with "
              "@c ZF_EPOLLIN_OVERLAPPED | @c EPOLLIN events.");
    rpc_zf_muxer_alloc(pco_iut, stack, &muxer_set);
    iut_zfur_waitable = rpc_zfur_to_waitable(pco_iut, iut_s);
    rpc_zf_muxer_add_simple(pco_iut, muxer_set, iut_zfur_waitable, iut_s,
                            RPC_ZF_EPOLLIN_OVERLAPPED | RPC_EPOLLIN);

    TEST_STEP("Create a CSAP on Tester to send UDP packets to IUT.");

    if (rcf_ta_create_session(pco_tst->ta, &sid) != 0)
        TEST_FAIL("Failed to allocate RCF session");

    CHECK_RC(tapi_udp_ip4_eth_csap_create(
                 pco_tst->ta, sid, tst_if->if_name,
                 (TAD_ETH_RECV_DEF & ~TAD_ETH_RECV_OTHER) |
                 TAD_ETH_RECV_NO_PROMISC, mac_tst, mac_iut,
                 SIN(tst_addr)->sin_addr.s_addr,
                 SIN(mcast_bind ? mcast_addr : iut_addr)->sin_addr.s_addr,
                 SIN(tst_addr)->sin_port,
                 SIN(mcast_bind ? mcast_addr : iut_addr)->sin_port, &csap));


    snprintf(buf, BUF_SIZE, " { pdus { udp: { checksum plain: %d}, "
            "ip4:{}, eth:{}}}", csum_val);

    CHECK_RC(asn_parse_value_text(buf, ndn_traffic_template, &pkt, &num));
    CHECK_RC(asn_write_value_field(pkt, sndiov.iov_base, total_len,
                                   "payload.#bytes"));

    memset(&msg, 0, sizeof(msg));
    msg.iovcnt = 1;
    msg.iov = &rcviov;

    TEST_STEP("In a loop do until @c ZF_EPOLLIN_OVERLAPPED is reported "
              "or too many attempts are made:");

    for (i = 0; i < MAX_ATTEMPTS; i++)
    {
        TEST_SUBSTEP("Call @b zf_muxer_wait() with @c RCF_RPC_CALL on IUT.");
        pco_iut->op = RCF_RPC_CALL;
        rpc_zf_muxer_wait(pco_iut, muxer_set, &event, 1, MUXER_TIMEOUT);

        TEST_SUBSTEP("With help of CSAP send a packet from Tester with UDP "
                     "checksum set according to @p checksum.");
        CHECK_RC(tapi_tad_trsend_start(pco_tst->ta, sid, csap, pkt,
                                       RCF_MODE_BLOCKING));

        TEST_SUBSTEP("Wait for @b zf_muxer_wait() termination. If it reports "
                     "no events and @p checksum is @c bad, move to the next "
                     "loop iteration. If it reports @c EPOLLIN and "
                     "@p checksum is not @c bad, read the packet on IUT and "
                     "move to the next loop iteration. If it reports "
                     "@c ZF_EPOLLIN_OVERLAPPED event, break the loop. "
                     "In all the other cases fail the test.");

        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zf_muxer_wait(pco_iut, muxer_set, &event, 1,
                               MUXER_TIMEOUT);
        if (rc < 0)
        {
            TEST_VERDICT("zf_muxer_wait() unexpectedly failed with error "
                         RPC_ERROR_FMT, RPC_ERROR_ARGS(pco_iut));
        }
        else if (rc == 0)
        {
            if (csum_val == TE_IP4_UPPER_LAYER_CSUM_BAD)
            {
                WARN("A packet with bad checksum was dropped without "
                     "reporting overlapped event, try to send "
                     "again from Tester.");
            }
            else
            {
                TEST_VERDICT("No event was reported for a packet with "
                             "correct or no checksum");
            }
        }
        else if (rc > 1)
        {
            TEST_VERDICT("zf_muxer_wait() returned too big value");
        }
        else if (event.data.u32 != iut_s)
        {
            TEST_VERDICT("zf_muxer_wait() returned event for unknown "
                         "descriptor");
        }
        else if (event.events == RPC_EPOLLIN)
        {
            if (csum_val == TE_IP4_UPPER_LAYER_CSUM_BAD)
            {
                TEST_VERDICT("EPOLLIN is reported for UDP packet with "
                             "bad checksum");
            }
            else
            {
                WARN("EPOLLIN is reported instead of ZF_EPOLLIN_OVERLAPPED. "
                     "Read the packet and try again.");

                msg.iov->iov_len = total_len;

                rpc_zfur_zc_recv(pco_iut, iut_s, &msg, 0);
                if (msg.iovcnt == 0)
                {
                    TEST_VERDICT("zfur_zc_recv() retrieved no data "
                                 "after EPOLLIN event was reported");
                }
                else
                {
                    if (msg.iov->iov_len != (size_t)total_len ||
                        memcmp(msg.iov->iov_base, sndiov.iov_base,
                               total_len) != 0)
                    {
                        TEST_VERDICT("zfur_zc_recv() retrieved unexpected "
                                     "data after EPOLLIN event was "
                                     "reported");
                    }
                }

                rpc_zfur_zc_recv_done(pco_iut, iut_s, &msg);
            }
        }
        else if (event.events != RPC_ZF_EPOLLIN_OVERLAPPED)
        {
            TEST_VERDICT("zf_muxer_wait() reported unexpected events '%s'",
                         epoll_event_rpc2str(event.events));
        }
        else
        {
            break;
        }
    }

    if (i >= MAX_ATTEMPTS)
        TEST_VERDICT("ZF_EPOLLIN_OVERLAPPED is not reported");

    TEST_STEP("Try to read @p part_len bytes with @b zfur_zc_recv() "
              "using @c ZF_OVERLAPPED_WAIT flag, check obtained "
              "data.");

    msg.iov[0].iov_len = part_len;
    msg.iovcnt = 1;

    exp_len = MIN(part_len, total_len);

    rpc_zfur_zc_recv(pco_iut, iut_s, &msg, RPC_ZF_OVERLAPPED_WAIT);
    if (msg.iovcnt == 0)
        TEST_VERDICT("Overlapped wait has been abandoned.");

    if (msg.iov->iov_len != (size_t)exp_len)
    {
        TEST_VERDICT("zfur_zc_recv() returned unexpected number "
                     "of bytes when called with ZF_OVERLAPPED_WAIT "
                     "flag");
    }

    if (memcmp(sndiov.iov_base, msg.iov->iov_base, exp_len) != 0)
    {
        RING_VERDICT("First part of data differs from data sent");
    }

    TEST_STEP("Call @b zfur_zc_recv() with @c ZF_OVERLAPPED_COMPLETE"
              " to wait for completion of reception.");
    rpc_zfur_zc_recv(pco_iut, iut_s, &msg, RPC_ZF_OVERLAPPED_COMPLETE);

    TEST_STEP("Check that received packet was either dropped or "
              "completely matches the sent one according to @p "
              "checksum parameter.");
    if (msg.iovcnt != 0)
    {
        if (strcmp(checksum, "bad") == 0)
        {
            RING_VERDICT("Packet passed verification unexpectedly"
                         " with %s checksum", checksum);
        }
        else
        {
            if (msg.iov->iov_len != (size_t)total_len)
                TEST_VERDICT("Unexpected number of bytes was retrieved");
            if (memcmp(sndiov.iov_base, msg.iov->iov_base, total_len) != 0)
                TEST_VERDICT("Data received differs from data sent");
        }
    }
    else
    {
        if (strcmp(checksum, "bad") != 0)
        {
            TEST_VERDICT("Packet did not pass verification unexpectedly"
                         " with %s checksum", checksum);
        }
    }

    TEST_STEP("Call @b zfur_zc_recv_done() to conclude pending zero-copy "
              "receive operation if at least one packet is received.");
    if (msg.iovcnt != 0)
    {
        RPC_AWAIT_ERROR(pco_iut);
        rc = rpc_zfur_zc_recv_done(pco_iut, iut_s, &msg);
        if (rc < 0)
        {
            TEST_VERDICT("Final zft_zc_recv_done() failed with errno %r",
                         RPC_ERRNO(pco_iut));
        }
    }

    TEST_SUCCESS;

cleanup:

    if (csap != CSAP_INVALID_HANDLE)
        CLEANUP_CHECK_RC(tapi_tad_csap_destroy(pco_tst->ta, 0, csap));

    ZFTS_FREE(pco_iut, zf_muxer, muxer_set);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zf_waitable, iut_zfur_waitable);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_s);

    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    rpc_release_iov(&rcviov, 1);
    rpc_release_iov(&sndiov, 1);
    asn_free_value(pkt);

    TEST_END;
}
