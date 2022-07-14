/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Common test API functions implementation
 *
 * Implementation of common functions.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

/* User name of the library which is used in logging. */
#define TE_LGR_USER     "ZF TAPI Library"

#include "zetaferno_ts.h"
#include "zfts_tcp.h"
#include "te_dbuf.h"
#include "te_param.h"

/** Shell command to get sockstat data. */
#define  ZFTS_SOCKSTAT_CMD  "cat /proc/net/sockstat"

/** Pathname template for temporary file with sockstat data. */
#define  ZFTS_SOCKSTAT_TMP_FMT  "/tmp/sockstat_%s%s"

/** Maximum allowed difference in sockets number before and after tests
 * execution. */
#define ZFTS_SOCKSTAT_MAX_DEVIATION 3

/** Maximum allowed difference in total TCP sockets number before and after
 * tests execution. */
#define ZFTS_SOCKSTAT_MAX_DEVIATION_TCP 10

/**
 * Statistics from "/proc/net/sockstat".
 */
typedef struct zfts_sockstat_stats {
    int tcp_inuse;
    int tcp_orphan;
    int tcp_alloc;
    int udp_inuse;
} zfts_sockstat_stats;

/* See description in zetaferno_ts.h */
void
zfts_create_stack(rcf_rpc_server *rpcs, rpc_zf_attr_p *attr,
                  rpc_zf_stack_p *stack)
{
    rpc_zf_init(rpcs);
    rpc_zf_attr_alloc(rpcs, attr);
    rpc_zf_stack_alloc(rpcs, *attr, stack);
}

/* See description in zetaferno_ts.h */
void
zfts_destroy_stack(rcf_rpc_server *rpcs, rpc_zf_attr_p attr,
                   rpc_zf_stack_p stack)
{
    if (stack != RPC_NULL)
        rpc_zf_stack_free(rpcs, stack);

    if (attr != RPC_NULL)
        rpc_zf_attr_free(rpcs, attr);

    rpc_zf_deinit(rpcs);
}

/* See description in zetaferno_ts.h */
void
zfts_sendto_iov(rcf_rpc_server *rpcs, int sock,
                rpc_iovec *iov, size_t iovcnt,
                const struct sockaddr *dst_addr)
{
    ssize_t rc;
    size_t  i;

    for (i = 0; i < iovcnt; i++)
    {
        rc = rpc_sendto(rpcs, sock, iov[i].iov_base, iov[i].iov_len, 0,
                        dst_addr);
        if ((size_t)rc != iov[i].iov_len)
            TEST_FAIL("Data was sent incompletely");
    }
}

/* See description in zetaferno_ts.h */
const char *
zfts_zocket_type2str(zfts_zocket_type zock_type)
{
    static struct param_map_entry map[] = {
        ZFTS_ZOCKET_TYPES,
        { NULL, 0}
    };

    int i;

    for (i = 0; map[i].str_val != NULL; i++)
    {
        /* Do not output generic ZFT type,
         * choose specific one. */
        if (strcmp(map[i].str_val, "zft") == 0)
            continue;

        if (map[i].num_val == (int)zock_type)
            return map[i].str_val;
    }

    return "<unknown>";
}

/* See description in zetaferno_ts.h */
zfts_zocket_type
zfts_str2zocket_type(const char *str)
{
    static struct param_map_entry map[] = {
        ZFTS_ZOCKET_TYPES,
        { NULL, 0}
    };

    int i;

    for (i = 0; map[i].str_val != NULL; i++)
    {
        if (strcmp(map[i].str_val, str) == 0)
            return map[i].num_val;
    }

    return ZFTS_ZOCKET_UNKNOWN;
}

/* See description in zetaferno_ts.h */
void
zfts_create_zocket(zfts_zocket_type zock_type,
                   rcf_rpc_server *pco_iut,
                   rpc_zf_attr_p attr,
                   rpc_zf_stack_p stack,
                   struct sockaddr *iut_addr,
                   rcf_rpc_server *pco_tst,
                   const struct sockaddr *tst_addr,
                   rpc_ptr *zocket,
                   rpc_zf_waitable_p *waitable,
                   int *tst_s)
{
    switch (zock_type)
    {
        case ZFTS_ZOCKET_URX:
        {
            rpc_zfur_p iut_zfur = RPC_NULL;

            rpc_zfur_alloc(pco_iut, &iut_zfur, stack, attr);
            rpc_zfur_addr_bind(pco_iut, iut_zfur, iut_addr,
                               tst_addr, 0);

            if (tst_s != NULL)
            {
                *tst_s = rpc_socket(pco_tst,
                                    rpc_socket_domain_by_addr(tst_addr),
                                    RPC_SOCK_DGRAM, RPC_PROTO_DEF);
                rpc_bind(pco_tst, *tst_s, tst_addr);
                rpc_connect(pco_tst, *tst_s, iut_addr);
            }

            *waitable = rpc_zfur_to_waitable(pco_iut, iut_zfur);
            *zocket = iut_zfur;
            break;
        }

        case ZFTS_ZOCKET_UTX:
        {
            rpc_zfut_p iut_zfut = RPC_NULL;

            rpc_zfut_alloc(pco_iut, &iut_zfut, stack,
                           iut_addr, tst_addr, 0, attr);

            if (tst_s != NULL)
            {
                *tst_s = rpc_socket(pco_tst,
                                    rpc_socket_domain_by_addr(tst_addr),
                                    RPC_SOCK_DGRAM, RPC_PROTO_DEF);
                rpc_bind(pco_tst, *tst_s, tst_addr);
                rpc_connect(pco_tst, *tst_s, iut_addr);
            }

            *waitable = rpc_zfut_to_waitable(pco_iut, iut_zfut);
            *zocket = iut_zfut;
            break;
        }

        case ZFTS_ZOCKET_ZFTL:
        {
            rpc_zftl_p iut_zftl = RPC_NULL;

            rpc_zftl_listen(pco_iut, stack, iut_addr,
                            attr, &iut_zftl);

            if (tst_s != NULL)
                *tst_s = rpc_socket(pco_tst,
                                    rpc_socket_domain_by_addr(tst_addr),
                                    RPC_SOCK_STREAM, RPC_PROTO_DEF);

            *waitable = rpc_zftl_to_waitable(pco_iut, iut_zftl);
            *zocket = iut_zftl;
            break;
        }

        case ZFTS_ZOCKET_ZFT_ACT:
        case ZFTS_ZOCKET_ZFT_PAS:
        {
            rpc_zft_p iut_zft = RPC_NULL;

            te_bool active = (zock_type == ZFTS_ZOCKET_ZFT_ACT);

            if (tst_s == NULL)
                TEST_FAIL("Peer socket is required "
                          "for creating ZFT zocket");

            zfts_establish_tcp_conn(active, pco_iut, attr, stack,
                                    &iut_zft, iut_addr,
                                    pco_tst, tst_s, tst_addr);
            *waitable = rpc_zft_to_waitable(pco_iut, iut_zft);
            *zocket = iut_zft;
            break;
        }

        default:
            TEST_FAIL("Incorrect zocket type");
    }
}

/* See description in zetaferno_ts.h */
void
zfts_destroy_zocket(zfts_zocket_type zock_type,
                    rcf_rpc_server *pco_iut,
                    rpc_ptr zocket)
{
    switch (zock_type)
    {
        case ZFTS_ZOCKET_URX:
            rpc_zfur_free(pco_iut, zocket);
            break;

        case ZFTS_ZOCKET_UTX:
            rpc_zfut_free(pco_iut, zocket);
            break;

        case ZFTS_ZOCKET_ZFTL:
            rpc_zftl_free(pco_iut, zocket);
            break;

        case ZFTS_ZOCKET_ZFT_ACT:
        case ZFTS_ZOCKET_ZFT_PAS:
            rpc_zft_free(pco_iut, zocket);
            break;

        default:
            TEST_FAIL("Incorrect zocket type");
    }
}

/* See description in zetaferno_ts.h */
int
zfts_sock_wait_data(rcf_rpc_server *rpcs,
                    int s, int timeout)
{
    struct rpc_pollfd poll_fd;

    poll_fd.fd = s;
    poll_fd.events = RPC_POLLIN;
    poll_fd.revents = 0;

    return rpc_poll(rpcs, &poll_fd, 1, timeout);
}

/* See the description in zetaferno_ts.h */
void
zfts_sockstat_file_name(rcf_rpc_server *rpcs, const char *suf,
                    char *name, size_t len)
{
    char  hname[ZFTS_FILE_NAME_LEN] = {};

    rpc_gethostname(rpcs, hname, sizeof(hname));
    snprintf(name, len, ZFTS_SOCKSTAT_TMP_FMT, hname,
             (suf == NULL) ? "" : suf);
}

/* See the description in zetaferno_ts.h */
int
zfts_sockstat_save(rcf_rpc_server *rpcs, const char *pathname)
{
    char *out_str = NULL;
    int   file;
    int   rc = -1;

    rpcs->use_libc_once = TRUE;
    rpc_shell_get_all(rpcs, &out_str, ZFTS_SOCKSTAT_CMD, -1);
    if (out_str == NULL)
    {
        ERROR("Could not get output of '%s' command", ZFTS_SOCKSTAT_CMD);
        goto zfts_sockstat_save_cleanup;
    }

    if ((file = open(pathname, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) == -1)
    {
        ERROR("Could not create file %s", pathname);
        goto zfts_sockstat_save_cleanup;
    }

    if (write(file, out_str, strlen(out_str)) != (ssize_t)strlen(out_str))
    {
        ERROR("Could not write %d bytes to file %s", strlen(out_str),
              pathname);
        goto zfts_sockstat_save_cleanup;
    }

    if (close(file) == -1)
    {
        ERROR("Could not close file %s", pathname);
        goto zfts_sockstat_save_cleanup;
    }

    rc = 0;

zfts_sockstat_save_cleanup:
    free(out_str);

    return rc;
}

/**
 * Extract stats from @p sockstat_file and save it in @p stats.
 */
static te_errno
zfts_sockstat_extract(const char *sockstat_file, zfts_sockstat_stats *stats)
{
    char buf[512] = {};
    char *line;
    char *p;
    int rc;
    int fd;

    if ((fd = open(sockstat_file, O_RDONLY)) == -1)
    {
        ERROR("Could not open file %s: %s",
              sockstat_file, strerror(errno));
        return TE_RC(TE_TAPI, TE_EFAIL);
    }

    rc = read(fd, buf, sizeof(buf));
    if (rc < 0)
    {
        ERROR("Failed to read file %s: %s", sockstat_file, strerror(errno));
        close(fd);
        return TE_RC(TE_TAPI, TE_EFAIL);
    }
    if (rc == sizeof(buf))
    {
        ERROR("Too small buffer is used to read sockstat data");
        close(fd);
        return TE_RC(TE_TAPI, TE_EOVERFLOW);
    }
    close(fd);

    RING("Dump sockstat (local file %s):\n%s", sockstat_file, buf);

#define CHECK_TOKEN(_p) \
    do {                                                                 \
        if (_p == NULL)                                                  \
        {                                                                \
            ERROR("%s:%d: Failed to extract stats from sockstat buffer", \
                  __FUNCTION__, __LINE__);                               \
            return TE_RC(TE_TAPI, TE_ENOENT);                            \
        }                                                                \
    } while (0)

#define GET_SOCKSTAT_VALUE(_begin, _field, _dst) \
    do {                                                                 \
        p = strstr(_begin, _field " ");                                  \
        CHECK_TOKEN(p);                                                  \
        p += strlen(_field " ");                                         \
        _dst = atoi(p);                                                  \
    } while (0)

    /*
     * Parsing data example:
     * $ cat /proc/net/sockstat
     * sockets: used 273
     * TCP: inuse 18 orphan 0 tw 5 alloc 26 mem 3
     * UDP: inuse 14 mem 4
     * UDPLITE: inuse 0
     * RAW: inuse 2
     * FRAG: inuse 0 memory 0
     */
    line = strstr(buf, "TCP: ");
    CHECK_TOKEN(line);
    GET_SOCKSTAT_VALUE(line, "inuse", stats->tcp_inuse);
    GET_SOCKSTAT_VALUE(line, "orphan", stats->tcp_orphan);
    GET_SOCKSTAT_VALUE(line, "alloc", stats->tcp_alloc);

    line = strstr(buf, "UDP: ");
    CHECK_TOKEN(line);
    GET_SOCKSTAT_VALUE(line, "inuse", stats->udp_inuse);

#undef CHECK_TOKEN
#undef GET_SOCKSTAT_VALUE

    return 0;
}

/**
 * Get TCP sockets number (bound or not) which are not connected and not
 * orphaned.
 */
static inline int
zfts_sockstat_get_hidden_tcp(const zfts_sockstat_stats *stats)
{
    return stats->tcp_alloc - stats->tcp_inuse - stats->tcp_orphan;
}

/* See the description in zetaferno_ts.h */
te_errno
zfts_sockstat_cmp(const char *sf1, const char *sf2)
{
    zfts_sockstat_stats st1;
    zfts_sockstat_stats st2;
    int rc;

    rc = zfts_sockstat_extract(sf1, &st1);
    if (rc != 0)
    {
        ERROR("Failed to get stats from the first sockstat file %s", sf1);
        return rc;
    }

    rc = zfts_sockstat_extract(sf2, &st2);
    if (rc != 0)
    {
        ERROR("Failed to get stats from the second sockstat file %s", sf2);
        return rc;
    }

#define CHECK_DIFF(_field, _v1, _v2, _max_diff) \
    do {                                                            \
        if (((_v1) > (_v2) + _max_diff) ||                          \
            ((_v1) < (_v2) - _max_diff))                            \
            {                                                       \
                ERROR_VERDICT("Too much difference in %s sockets "  \
                              "amount", _field);                    \
                ERROR("Before tests execution %d, after %d",        \
                        (_v1), (_v2));                              \
            }                                                       \
    } while (0)

    /* Checks to report possible resource leaks. */
    CHECK_DIFF("hidden TCP",
               zfts_sockstat_get_hidden_tcp(&st1),
               zfts_sockstat_get_hidden_tcp(&st2),
               ZFTS_SOCKSTAT_MAX_DEVIATION);
    CHECK_DIFF("total TCP", st1.tcp_alloc, st2.tcp_alloc,
               ZFTS_SOCKSTAT_MAX_DEVIATION_TCP);
    CHECK_DIFF("UDP", st1.udp_inuse, st2.udp_inuse,
               ZFTS_SOCKSTAT_MAX_DEVIATION);

#undef CHECK_DIFF

    return 0;
}

/* See the description in zetaferno_ts.h */
void
zfts_split_in_iovs(const char *buf, size_t len,
                   int min_iov_len, int max_iov_len,
                   rpc_iovec **iov_out, int *num_out)
{
    rpc_iovec *iov;
    int num;
    int i;
    size_t cur_len;
    size_t remained;

    num = 2 * len / (min_iov_len + max_iov_len) + 1;
    iov = tapi_calloc(num, sizeof(rpc_iovec));

    remained = len;
    i = 0;
    while (remained > 0)
    {
        cur_len = rand_range(min_iov_len, max_iov_len);
        if (cur_len > remained)
            cur_len = remained;

        if (i == num)
        {
            void *p;
            int new_num;

            new_num = num * 2;
            p = realloc(iov, new_num * sizeof(rpc_iovec));
            if (p == NULL)
            {
                free(iov);
                TEST_FAIL("%s(): not enough memory", __FUNCTION__);
            }
            iov = (rpc_iovec *)p;
            num = new_num;
        }

        iov[i].iov_base = buf + len - remained;
        iov[i].iov_len = iov[i].iov_rlen = cur_len;
        remained -= cur_len;
        i++;
    }

    *iov_out = iov;
    *num_out = i;
}
