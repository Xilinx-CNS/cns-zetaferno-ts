/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Auxiliary test API for ZF multiplexer.
 *
 * Implementation of auxiliary test API to work with ZF multiplexer.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 *
 * $Id$
 */

#define TE_LGR_USER     "ZF Multiplexer TAPI"

#include "zetaferno_ts.h"
#include "zfts_muxer.h"
#include "zfts_tcp.h"
#include "zfts_zfur.h"

#include "te_rpc_sys_epoll.h"
#include "tapi_sockets.h"

/** Maximum length of string containing muxer events. */
#define MAX_EVT_STR_LEN 1000

/* See description in zfts_muxer.h */
void
zfts_check_muxer_events(rcf_rpc_server *rpcs,
                        rpc_zf_muxer_set_p ms,
                        const char *err_msg,
                        int timeout,
                        struct rpc_epoll_event *exp_evts,
                        int exp_evts_num,
                        zfts_zock_descr *zock_descrs)
{
    zfts_mzockets mzockets;

    int i;
    int j;


    mzockets.count = exp_evts_num;

    if (zock_descrs != NULL)
    {
        for (i = 0; zock_descrs[i].zocket != NULL; i++);
        if (mzockets.count < i)
            mzockets.count = i;
    }

    mzockets.mzocks = tapi_calloc(mzockets.count, sizeof(zfts_mzocket));

    if (zock_descrs != NULL)
    {
        for (j = 0; j < exp_evts_num; j++)
        {
            for (i = 0; zock_descrs[i].zocket != NULL; i++)
            {
                if (*(zock_descrs[i].zocket) == exp_evts[j].data.u32)
                    break;
            }

            if (zock_descrs[i].zocket == NULL)
                TEST_FAIL("For some zocket(s) description is missing");
        }

        for (i = 0; zock_descrs[i].zocket != NULL; i++)
        {
            mzockets.mzocks[i].zocket = *(zock_descrs[i].zocket);
            mzockets.mzocks[i].in_mset = TRUE;
            mzockets.mzocks[i].exp_events = 0;
            snprintf(mzockets.mzocks[i].descr, ZFTS_MZOCK_DESCR_LEN,
                     "%s", zock_descrs[i].descr);

            for (j = 0; j < exp_evts_num; j++)
            {
                if (exp_evts[j].data.u32 == *(zock_descrs[i].zocket))
                {
                    mzockets.mzocks[i].exp_events = exp_evts[j].events;
                    break;
                }
            }
        }
    }
    else
    {
        for (j = 0; j < exp_evts_num; j++)
        {
            mzockets.mzocks[j].zocket = exp_evts[j].data.u32;
            mzockets.mzocks[j].in_mset = TRUE;
            mzockets.mzocks[j].exp_events = exp_evts[j].events;
            snprintf(mzockets.mzocks[j].descr, ZFTS_MZOCK_DESCR_LEN,
                     "zocket %d", j + 1);
        }
    }

    zfts_mzockets_check_events(rpcs, ms, timeout, FALSE, &mzockets,
                               err_msg);

    free(mzockets.mzocks);
}

/* See description in zfts_muxer.h */
void
zfts_create_mzocket(zfts_zocket_type zock_type,
                    rcf_rpc_server *pco_iut,
                    rpc_zf_attr_p attr,
                    rpc_zf_stack_p stack,
                    const struct sockaddr *iut_addr,
                    rcf_rpc_server *pco_tst,
                    const struct sockaddr *tst_addr,
                    const char *descr,
                    zfts_mzocket *mzocket)
{
    memset(mzocket, 0, sizeof(*mzocket));

    mzocket->zock_type = zock_type;
    mzocket->pco_tst = pco_tst;
    mzocket->pco_iut = pco_iut;
    mzocket->attr = attr;
    mzocket->stack = stack;
    mzocket->zocket = RPC_NULL;
    mzocket->peer_sock = -1;
    mzocket->in_mset = FALSE;

    snprintf(mzocket->descr, ZFTS_MZOCK_DESCR_LEN, "%s", descr);

    tapi_sockaddr_clone_exact(iut_addr, &mzocket->iut_addr);
    tapi_sockaddr_clone_exact(tst_addr, &mzocket->tst_addr);

    zfts_create_zocket(zock_type, pco_iut, attr, stack,
                       SA(&mzocket->iut_addr),
                       pco_tst, SA(&mzocket->tst_addr), &mzocket->zocket,
                       &mzocket->waitable,
                       (zock_type == ZFTS_ZOCKET_ZFTL ?
                                        NULL : &mzocket->peer_sock));
}

/* See description in zfts_muxer.h */
void
zfts_destroy_mzocket(zfts_mzocket *mzocket)
{
    int i;

    te_dbuf_free(&mzocket->iut_sent_data);
    te_dbuf_free(&mzocket->tst_sent_data);
    te_dbuf_free(&mzocket->tst_recv_data);

    if (mzocket->zocket != RPC_NULL)
    {
        zfts_destroy_zocket(mzocket->zock_type, mzocket->pco_iut,
                            mzocket->zocket);
        mzocket->zocket = RPC_NULL;
    }

    if (mzocket->waitable != RPC_NULL)
    {
        rpc_zf_waitable_free(mzocket->pco_iut, mzocket->waitable);
        mzocket->waitable = RPC_NULL;
    }

    if (mzocket->peer_sock >= 0)
    {
        rpc_close(mzocket->pco_tst, mzocket->peer_sock);
        mzocket->peer_sock = -1;
    }

    for (i = 0; i < mzocket->lpeers_count; i++)
    {
        if (mzocket->lpeers[i].lpeer_sock >= 0)
            rpc_close(mzocket->pco_tst, mzocket->lpeers[i].lpeer_sock);
    }

    free(mzocket->lpeers);
    mzocket->lpeers_count = 0;
    mzocket->lpeers_max = 0;
    mzocket->lpeers = NULL;
}

/* See description in zfts_muxer.h */
void
zfts_mzocket_prepare_events(zfts_mzocket *mzocket,
                            uint32_t events)
{
    mzocket->exp_events = events;

    if (events & RPC_EPOLLOUT)
    {
        if (zfts_zocket_type_zft(mzocket->zock_type))
        {
            rpc_zft_overfill_buffers(mzocket->pco_iut, mzocket->stack,
                                     mzocket->zocket, &mzocket->iut_sent_data);
        }
        else
        {
            TEST_FAIL("EPOLLOUT event can be invoked "
                      "only for connected TCP zocket");
        }
    }

    if (events & RPC_EPOLLIN)
    {
        if (mzocket->zock_type == ZFTS_ZOCKET_ZFTL)
        {
            if (mzocket->lpeers_count == mzocket->lpeers_max)
            {
                zfts_listen_peer *new_lpeers;
                int               new_max;

                new_max = (mzocket->lpeers_max + 1) * 2;

                new_lpeers = realloc(mzocket->lpeers,
                                     sizeof(*new_lpeers) * new_max);
                if (new_lpeers == NULL)
                      TEST_FAIL("Out of memory");

                mzocket->lpeers = new_lpeers;
                mzocket->lpeers_max = new_max;
            }

            mzocket->lpeers[mzocket->lpeers_count].lpeer_sock =
                    rpc_socket(mzocket->pco_tst,
                               rpc_socket_domain_by_addr(
                                                SA(&mzocket->tst_addr)),
                               RPC_SOCK_STREAM, RPC_PROTO_DEF);
            mzocket->lpeers_count++;
        }
    }
}

/* See description in zfts_muxer.h */
void
zfts_mzocket_invoke_events(zfts_mzocket *mzocket)
{
    char data[ZFTS_TCP_DATA_MAX];
    int  rc;

    /*
     * Avoid calling functions on pco_iut here:
     * this function may be called to invoke event
     * while on IUT @b zf_muxer_wait() is blocked.
     */

    te_fill_buf(data, ZFTS_TCP_DATA_MAX);

    if (mzocket->exp_events & RPC_EPOLLIN)
    {
        if (mzocket->zock_type == ZFTS_ZOCKET_URX ||
            zfts_zocket_type_zft(mzocket->zock_type))
        {
            rc = rpc_send(mzocket->pco_tst, mzocket->peer_sock,
                          data, ZFTS_TCP_DATA_MAX, 0);
            if (rc != ZFTS_TCP_DATA_MAX)
                TEST_FAIL("send() returned unexpected result %d", rc);

            te_dbuf_append(&mzocket->tst_sent_data,
                           data, ZFTS_TCP_DATA_MAX);
        }
        else if (mzocket->zock_type == ZFTS_ZOCKET_ZFTL)
        {
            zfts_listen_peer *lpeer;

            socklen_t namelen;

            if (mzocket->lpeers_count <= 0)
                TEST_FAIL("No peer socket present for "
                          "initiating connection");
            else
                lpeer = &mzocket->lpeers[mzocket->lpeers_count - 1];

            rpc_fcntl(mzocket->pco_tst, lpeer->lpeer_sock,
                      RPC_F_SETFL, RPC_O_NONBLOCK);
            RPC_AWAIT_ERROR(mzocket->pco_tst);
            rc = rpc_connect(mzocket->pco_tst, lpeer->lpeer_sock,
                             SA(&mzocket->iut_addr));
            if (rc < 0 && RPC_ERRNO(mzocket->pco_tst) != RPC_EINPROGRESS)
                  TEST_FAIL("connect() failed with unexpected errno %r",
                            RPC_ERRNO(mzocket->pco_tst));

            namelen = sizeof(lpeer->lpeer_addr);
            rpc_getsockname(mzocket->pco_tst, lpeer->lpeer_sock,
                            SA(&lpeer->lpeer_addr), &namelen);
        }
        else
        {
            TEST_FAIL("Cannot invoke EPOLLIN event for zocket type '%s'",
                      zfts_zocket_type2str(mzocket->zock_type));
        }
    }

    if (mzocket->exp_events & RPC_EPOLLOUT)
    {
        if (zfts_zocket_type_zft(mzocket->zock_type))
        {
            rpc_read_fd2te_dbuf_append(mzocket->pco_tst, mzocket->peer_sock,
                                       0, 0, &mzocket->tst_recv_data);
        }
        else
        {
            TEST_FAIL("Cannot invoke EPOLLOUT event for zocket type '%s'",
                      zfts_zocket_type2str(mzocket->zock_type));
        }
    }
}

/* See description in zfts_muxer.h */
void
zfts_mzocket_process_events(zfts_mzocket *mzocket)
{
    int rc;

    if (mzocket->zocket == RPC_NULL)
    {
        mzocket->exp_events = 0;
        return;
    }

    if (mzocket->exp_events & RPC_EPOLLIN)
    {
        if (mzocket->zock_type == ZFTS_ZOCKET_URX ||
            zfts_zocket_type_zft(mzocket->zock_type))
        {
            te_dbuf received_data = TE_DBUF_INIT(0);

            rpc_zf_process_events(mzocket->pco_iut, mzocket->stack);

            if (mzocket->zock_type == ZFTS_ZOCKET_URX)
            {
                zfts_zfur_read_data(mzocket->pco_iut,
                                    mzocket->stack,
                                    mzocket->zocket,
                                    &received_data);
            }
            else
            {
                if (mzocket->expect_econnreset)
                    RPC_AWAIT_ERROR(mzocket->pco_iut);

                rc = zfts_zft_read_data(mzocket->pco_iut,
                                        mzocket->zocket,
                                        &received_data,
                                        NULL);

                if (mzocket->expect_econnreset)
                {
                    if (rc < 0)
                    {
                        if (RPC_ERRNO(mzocket->pco_iut) != RPC_ECONNRESET)
                        {
                            TEST_VERDICT("zfts_zft_read_data() "
                                         "failed with unexpected "
                                         "error " RPC_ERROR_FMT,
                                         RPC_ERROR_ARGS(mzocket->pco_iut));
                        }
                    }
                    else
                    {
                        TEST_VERDICT("zfts_zft_read_data() should have "
                                     "failed with ECONNRESET but it "
                                     "succeeded");
                    }
                }
            }

            ZFTS_CHECK_RECEIVED_DATA(received_data.ptr,
                                     mzocket->tst_sent_data.ptr,
                                     received_data.len,
                                     mzocket->tst_sent_data.len,
                                     " from peer socket");
            te_dbuf_free(&mzocket->tst_sent_data);
            te_dbuf_free(&received_data);
        }
        else if (mzocket->zock_type == ZFTS_ZOCKET_ZFTL)
        {
            rpc_zft_p zft_accepted;
            int       i;
            int       j;
            int       peer_sock;

            struct sockaddr_storage   zft_raddr;

            ZFTS_WAIT_NETWORK(mzocket->pco_iut, mzocket->stack);
            for (i = 0; i < mzocket->lpeers_count; i++)
            {
                rpc_zftl_accept(mzocket->pco_iut, mzocket->zocket,
                                &zft_accepted);
                rpc_zft_getname(mzocket->pco_iut, zft_accepted,
                                NULL, SIN(&zft_raddr));

                for (j = 0; j < mzocket->lpeers_count; j++)
                {
                    if (mzocket->lpeers[j].lpeer_sock >= 0 &&
                        tapi_sockaddr_cmp(
                                  SA(&zft_raddr),
                                  SA(&mzocket->lpeers[j].lpeer_addr)) == 0)
                        break;
                }

                if (j >= mzocket->lpeers_count)
                    TEST_FAIL("Failed to find peer corresponding "
                              "to connected zocket");

                peer_sock = mzocket->lpeers[j].lpeer_sock;

                rpc_fcntl(mzocket->pco_tst, peer_sock,
                          RPC_F_SETFL, 0);
                zfts_zft_check_connection(mzocket->pco_iut,
                                          mzocket->stack,
                                          zft_accepted,
                                          mzocket->pco_tst,
                                          peer_sock);

                rpc_zft_free(mzocket->pco_iut, zft_accepted);
                rpc_close(mzocket->pco_tst, peer_sock);
                mzocket->lpeers[j].lpeer_sock = -1;
            }

            mzocket->lpeers_count = 0;
        }
        else
        {
            TEST_FAIL("Cannot process EPOLLIN event for zocket type '%s'",
                      zfts_zocket_type2str(mzocket->zock_type));
        }
    }

    if (mzocket->exp_events & RPC_EPOLLOUT)
    {
        if (zfts_zocket_type_zft(mzocket->zock_type) &&
            mzocket->peer_sock >= 0)
        {
            zfts_zft_peer_read_all(mzocket->pco_iut, mzocket->stack,
                                   mzocket->pco_tst, mzocket->peer_sock,
                                   &mzocket->tst_recv_data);

            ZFTS_CHECK_RECEIVED_DATA(mzocket->tst_recv_data.ptr,
                                     mzocket->iut_sent_data.ptr,
                                     mzocket->tst_recv_data.len,
                                     mzocket->iut_sent_data.len,
                                     " from zocket after overfilling "
                                     "its send buffer");
            te_dbuf_free(&mzocket->iut_sent_data);
            te_dbuf_free(&mzocket->tst_recv_data);
        }
    }

    mzocket->exp_events = 0;
}

/* See description in zfts_muxer.h */
void
zfts_mzocket_add_events(rpc_zf_muxer_set_p ms,
                        zfts_mzocket *mzocket,
                        uint32_t events)
{
    rpc_zf_muxer_add_simple(mzocket->pco_iut, ms, mzocket->waitable,
                            mzocket->zocket, events);
    mzocket->in_mset = TRUE;
}

/* See description in zfts_muxer.h */
void
zfts_mzocket_add_exp_events(rpc_zf_muxer_set_p ms,
                            zfts_mzocket *mzocket)
{
    rpc_zf_muxer_add_simple(mzocket->pco_iut, ms, mzocket->waitable,
                            mzocket->zocket, mzocket->exp_events);
    mzocket->in_mset = TRUE;
}

/* See description in zfts_muxer.h */
void
zfts_mzocket_mod_events(zfts_mzocket *mzocket,
                        uint32_t events)
{
    rpc_zf_muxer_mod_simple(mzocket->pco_iut, mzocket->waitable,
                            mzocket->zocket, events);
}

/* See description in zfts_muxer.h */
void
zfts_mzocket_mod_exp_events(zfts_mzocket *mzocket)
{
    rpc_zf_muxer_mod_simple(mzocket->pco_iut, mzocket->waitable,
                            mzocket->zocket, mzocket->exp_events);
}

/* See description in zfts_muxer.h */
void
zfts_mzocket_rearm_events(zfts_mzocket *mzocket)
{
    rpc_zf_muxer_mod_rearm(mzocket->pco_iut, mzocket->waitable);
}

/* See description in zfts_muxer.h */
void
zfts_mzocket_del(zfts_mzocket *mzocket)
{
    rpc_zf_muxer_del(mzocket->pco_iut, mzocket->waitable);
    mzocket->in_mset = FALSE;
}

/* See description in zfts_muxer.h */
void
zfts_mzocket_check_events(rcf_rpc_server *pco_iut,
                          rpc_zf_muxer_set_p ms,
                          int timeout,
                          zfts_mzocket *mzocket,
                          const char *err_msg)
{
    zfts_mzockets mzockets = {0};

    mzockets.mzocks = mzocket;
    mzockets.count = 1;
    zfts_mzockets_check_events(pco_iut, ms, timeout, FALSE,
                               &mzockets, err_msg);
}

/* See description in zfts_muxer.h */
void
zfts_create_mzockets(const char *spec,
                     rcf_rpc_server *pco_iut,
                     rpc_zf_attr_p attr,
                     rpc_zf_stack_p stack,
                     const struct sockaddr *iut_addr,
                     rcf_rpc_server *pco_tst,
                     const struct sockaddr *tst_addr,
                     zfts_mzockets *mzockets)
{
#define MAX_SUBSTR_LEN 100
    int count = 0;
    int i = 0;
    int j = 0;
    int k = 0;

    char              type_str[MAX_SUBSTR_LEN];
    zfts_zocket_type  zock_type;

    struct sockaddr_storage iut_addr2;
    struct sockaddr_storage tst_addr2;

    if (spec[0] != '\0')
        count++;
    for (i = 0; spec[i] != '\0'; i++)
    {
        if (spec[i] == ',')
            count++;
    }

    if (count == 0)
    {
        mzockets->mzocks = NULL;
        mzockets->count = 0;
        return;
    }

    mzockets->mzocks = tapi_calloc(count, sizeof(zfts_mzocket));
    mzockets->count = count;

    for (k = 0, j = 0, i = 0; ; i++, j++)
    {
        if (spec[i] == ',' || spec[i] == '\0')
        {
            type_str[j] = '\0';
            zock_type = zfts_str2zocket_type(type_str);
            j = -1;

            CHECK_RC(tapi_sockaddr_clone(pco_iut, iut_addr, &iut_addr2));
            CHECK_RC(tapi_sockaddr_clone(pco_tst, tst_addr, &tst_addr2));
            zfts_create_mzocket(zock_type, pco_iut, attr, stack,
                                SA(&iut_addr2), pco_tst, SA(&tst_addr2),
                                "", &mzockets->mzocks[k]);
            snprintf(mzockets->mzocks[k].descr, ZFTS_MZOCK_DESCR_LEN,
                     "zocket %i", k + 1);
            k++;
        }
        else
        {
            if (spec[i] == '.')
                type_str[j] = '\0';
            else
                type_str[j] = spec[i];
        }

        if (spec[i] == '\0')
            break;
    }
}

/* See description in zfts_muxer.h */
void
zfts_destroy_mzockets(zfts_mzockets *mzockets)
{
    int i;

    for (i = 0; i < mzockets->count; i++)
    {
        zfts_destroy_mzocket(&mzockets->mzocks[i]);
    }
    free(mzockets->mzocks);
    mzockets->mzocks = NULL;
    mzockets->count = 0;
}

/* See description in zfts_muxer.h */
void
zfts_mzockets_prepare_events(zfts_mzockets *mzockets,
                             const char *events_spec)
{
    char evt_str[MAX_EVT_STR_LEN];
    int  i;
    int  j;
    int  k;

    uint32_t  events;

    if (mzockets->count <= 0)
        return;

    for (i = 0, j = 0, k = 0; ; i++, j++)
    {
        if (events_spec[i] != ',' && events_spec[i] != '\0')
        {
            if (events_spec[i] == '.')
            {
                evt_str[0] = '\0';
                j = -1;
            }
            else
                evt_str[j] = events_spec[i];
        }
        else
        {
            evt_str[j] = '\0';
            j = -1;

            events = zfts_parse_events(evt_str);

            zfts_mzocket_prepare_events(&mzockets->mzocks[k], events);
            k++;
            if (k >= mzockets->count)
                break;
        }

        if (events_spec[i] == '\0')
            break;
    }
}

/* See description in zfts_muxer.h */
void
zfts_mzockets_invoke_events(zfts_mzockets *mzockets)
{
    int i;

    /*
     * This function may be called to invoke events
     * while on IUT @b zf_muxer_wait() is blocked,
     * so it should not use RPC calls on IUT.
     */

    for (i = 0; i < mzockets->count; i++)
    {
        zfts_mzocket_invoke_events(&mzockets->mzocks[i]);
    }

    /*
     * Wait to make sure that packets sent to invoke events
     * have arrived to their destinations.
     */
    TAPI_WAIT_NETWORK;
}

/* See description in zfts_muxer.h */
void
zfts_mzockets_process_events(zfts_mzockets *mzockets)
{
    int i;

    for (i = 0; i < mzockets->count; i++)
    {
        zfts_mzocket_process_events(&mzockets->mzocks[i]);
    }
}

/* See description in zfts_muxer.h */
void
zfts_mzockets_add_events(rpc_zf_muxer_set_p ms,
                         zfts_mzockets *mzockets,
                         uint32_t events)
{
    int i;

    for (i = 0; i < mzockets->count; i++)
    {
        zfts_mzocket_add_events(ms, &mzockets->mzocks[i], events);
    }
}

/* See description in zfts_muxer.h */
void
zfts_mzockets_add_exp_events(rpc_zf_muxer_set_p ms,
                             zfts_mzockets *mzockets)
{
    int i;

    for (i = 0; i < mzockets->count; i++)
    {
        zfts_mzocket_add_exp_events(ms, &mzockets->mzocks[i]);
    }
}

/* See description in zfts_muxer.h */
void
zfts_mzockets_mod_events(zfts_mzockets *mzockets,
                         uint32_t events)
{
    int i;

    for (i = 0; i < mzockets->count; i++)
    {
        zfts_mzocket_mod_events(&mzockets->mzocks[i], events);
    }
}

/* See description in zfts_muxer.h */
void
zfts_mzockets_mod_exp_events(zfts_mzockets *mzockets)
{
    int i;

    for (i = 0; i < mzockets->count; i++)
    {
        zfts_mzocket_mod_exp_events(&mzockets->mzocks[i]);
    }
}

/* See description in zfts_muxer.h */
void
zfts_mzockets_rearm_events(zfts_mzockets *mzockets)
{
    int i;

    for (i = 0; i < mzockets->count; i++)
    {
        zfts_mzocket_rearm_events(&mzockets->mzocks[i]);
    }
}

/* See description in zfts_muxer.h */
void
zfts_mzockets_del(zfts_mzockets *mzockets)
{
    int i;

    for (i = 0; i < mzockets->count; i++)
    {
        zfts_mzocket_del(&mzockets->mzocks[i]);
    }
}

/* See description in zfts_muxer.h */
te_bool
zfts_mzockets_check_events(rcf_rpc_server *pco_iut,
                           rpc_zf_muxer_set_p ms,
                           int timeout,
                           te_bool allow_no_evts,
                           zfts_mzockets *mzockets,
                           const char *err_msg)
{
    struct rpc_epoll_event real_evts[ZFTS_DEF_MUXER_EVTS_NUM] = {{0}};

    int evts_num = 0;
    int exp_evts_num = 0;
    int i;
    int j;
    int total_duration = 0;

    te_bool failed = FALSE;

    for (i = 0; i < mzockets->count; i++)
    {
        exp_evts_num += (mzockets->mzocks[i].exp_events != 0 &&
                         mzockets->mzocks[i].in_mset);
    }

    /* Break loop if all events are received or if timeout is elapsed. */
    do {
        int evts_num_tmp = 0;
        RPC_AWAIT_ERROR(pco_iut);
        evts_num_tmp = rpc_zf_muxer_wait(pco_iut, ms, &real_evts[evts_num],
                                         ZFTS_DEF_MUXER_EVTS_NUM - evts_num,
                                         timeout < 0 ?
                                             -1 : timeout - total_duration);
        if (evts_num_tmp < 0)
        {
            TEST_VERDICT("%s: zf_muxer_wait() unexpectedly failed "
                         "with errno %r", err_msg, RPC_ERRNO(pco_iut));
        }
        else if (evts_num_tmp > ZFTS_DEF_MUXER_EVTS_NUM)
        {
            TEST_VERDICT("%s: zf_muxer_wait() returned too big value",
                         err_msg);
        }
        else if (evts_num_tmp == 0 && exp_evts_num != 0)
        {
            if (allow_no_evts && timeout >= 0 && evts_num == 0)
                return FALSE;

            TEST_VERDICT("%s: zf_muxer_wait() unexpectedly returned zero",
                         err_msg);
        }
        else
        {
            evts_num += evts_num_tmp;
            total_duration += (int)TE_US2MS(pco_iut->duration);
            if (total_duration >= timeout && timeout > 0)
                break;
        }
    } while (evts_num < exp_evts_num);

    for (i = 0; i < evts_num; i++)
    {
        for (j = 0; j < mzockets->count; j++)
        {
            if (mzockets->mzocks[j].zocket == real_evts[i].data.u32)
                break;
        }

        if (j >= mzockets->count)
        {
            ERROR_VERDICT("%s: zf_muxer_wait() returned "
                          "events for unknown zocket", err_msg);
            failed = TRUE;
        }
        else if (real_evts[i].events != mzockets->mzocks[j].exp_events ||
                 !mzockets->mzocks[j].in_mset)
        {
            char exp_evts_str[MAX_EVT_STR_LEN] = "";

            if (mzockets->mzocks[j].in_mset)
                snprintf(exp_evts_str, MAX_EVT_STR_LEN, "%s",
                         epoll_event_rpc2str(
                                    mzockets->mzocks[j].exp_events));

            ERROR_VERDICT("%s: zf_muxer_wait() returned "
                          "unexpected events '%s' instead of '%s' for %s",
                          err_msg,
                          epoll_event_rpc2str(real_evts[i].events),
                          exp_evts_str,
                          mzockets->mzocks[j].descr);
            failed = TRUE;
        }
    }

    for (j = 0; j < mzockets->count; j++)
    {
        if (mzockets->mzocks[j].exp_events != 0 &&
            mzockets->mzocks[j].in_mset)
        {
            for (i = 0; i < evts_num; i++)
            {
                if (mzockets->mzocks[j].zocket == real_evts[i].data.u32)
                    break;
            }

            if (i >= evts_num)
            {
                ERROR_VERDICT("%s: zf_muxer_wait() did not return "
                              "events for %s",
                              err_msg, mzockets->mzocks[j].descr);
                failed = TRUE;
            }
        }
    }

    for (i = 0; i < evts_num; i++)
    {
        for (j = 0; j < evts_num; j++)
        {
            if (real_evts[i].data.u32 == real_evts[j].data.u32 &&
                i != j)
            {
                TEST_VERDICT("%s: zf_muxer_wait() returned "
                             "event more than once for zome zocket",
                             err_msg);
            }
        }
    }

    if (failed)
        TEST_STOP;

    return TRUE;
}

/* See description in zfts_muxer.h */
uint32_t
zfts_parse_events(const char *events)
{
    char  evt_str[MAX_EVT_STR_LEN];
    int   i;
    int   j;

    uint32_t result = 0;

    if (events == NULL)
        return 0;

    for (i = 0, j = 0; ; i++, j++)
    {
        if (events[i] == '.')
        {
            j = -1;
        }
        else if (events[i] == '_' || events[i] == '\0')
        {
            evt_str[j] = '\0';
            j = -1;

            if (strcasecmp(evt_str, "in") == 0)
                result |= RPC_EPOLLIN;
            else if (strcasecmp(evt_str, "out") == 0)
                result |= RPC_EPOLLOUT;
            else if (strcasecmp(evt_str, "err") == 0)
                result |= RPC_EPOLLERR;
            else if (strcasecmp(evt_str, "hup") == 0)
                result |= RPC_EPOLLHUP;
            else if (strcasecmp(evt_str, "all") == 0)
                result |= RPC_EPOLLIN | RPC_EPOLLOUT |
                          RPC_EPOLLERR | RPC_EPOLLHUP;
            else
                TEST_FAIL("Cannot parse event '%s'", evt_str);
        }
        else
        {
            evt_str[j] = events[i];
        }

        if (events[i] == '\0')
            break;
    }

    return result;
}

/* See description in zfts_muxer.h */
void
zfts_muxer_wait_check_no_evts(rcf_rpc_server *rpcs,
                              rpc_zf_muxer_set_p muxer_set,
                              int timeout, const char *err_msg)
{
    struct rpc_epoll_event event;
    int                    rc;

    RPC_AWAIT_ERROR(rpcs);
    rc = rpc_zf_muxer_wait(rpcs, muxer_set,
                           &event, 1, timeout);

    if (rc < 0)
        TEST_VERDICT("%s: zf_muxer_wait() failed with errno %r",
                     err_msg, RPC_ERRNO(rpcs));
    else if (rc > 0)
        TEST_VERDICT("%s: zf_muxer_wait() returned unexpected "
                     "events", err_msg);
}
