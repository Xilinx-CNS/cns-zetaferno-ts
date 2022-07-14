/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Bad Parameters and Boundary Values
 *
 * $Id$
 */

/**
 * @page bnbvalue-bad_address Invalid address processing in various ZF functions.
 *
 * @objective Check that passing an invalid address family or length is
 *            processed correctly in various ZF API functions.
 *
 * @param function    Tested function:
 *                    - zftl_listen
 *                    - zft_addr_bind
 *                    - zft_connect
 *                    - zfur_addr_bind
 *                    - zfur_addr_unbind
 *                    - zfut_alloc
 * @param family      Address family:
 *                    - AF_INET
 *                    - AF_INET6
 *                    - AF_PACKET
 *                    - AF_UNIX
 * @param length      Address length:
 *                    - sockaddr_in - 1
 *                    - sockaddr_in
 *                    - sockaddr_in6
 *                    - sockaddr_storage
 * @param mod_laddr   Whether to modify laddr
 *                    function parameter according
 *                    to @p family and @p length.
 * @param mod_raddr   Whether to modify raddr
 *                    function parameter according
 *                    to @p family and @p length.
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "bnbvalue/bad_address"

#include "zf_test.h"
#include "rpc_zf.h"
#include "tapi_rpc_params.h"

#include <ctype.h>

/** Enumeration of function types. */
typedef enum {
    ZFTS_ADDR_FUNC_ZFTL_LISTEN,       /**< zftl_listen(). */
    ZFTS_ADDR_FUNC_ZFT_ADDR_BIND,     /**< zft_addr_bind(). */
    ZFTS_ADDR_FUNC_ZFT_CONNECT,       /**< zft_connect(). */
    ZFTS_ADDR_FUNC_ZFUR_ADDR_BIND,    /**< zfur_addr_bind(). */
    ZFTS_ADDR_FUNC_ZFUR_ADDR_UNBIND,  /**< zfur_addr_unbind(). */
    ZFTS_ADDR_FUNC_ZFUT_ALLOC,        /**< zfut_alloc(). */
} zfts_addr_func;

/** List of function types to be used with TEST_GET_ENUM_PARAM(). */
#define ZFTS_ADDR_FUNCS \
    { "zftl_listen",      ZFTS_ADDR_FUNC_ZFTL_LISTEN },       \
    { "zft_addr_bind",    ZFTS_ADDR_FUNC_ZFT_ADDR_BIND },     \
    { "zft_connect",      ZFTS_ADDR_FUNC_ZFT_CONNECT },       \
    { "zfur_addr_bind",   ZFTS_ADDR_FUNC_ZFUR_ADDR_BIND },    \
    { "zfur_addr_unbind", ZFTS_ADDR_FUNC_ZFUR_ADDR_UNBIND },  \
    { "zfut_alloc",       ZFTS_ADDR_FUNC_ZFUT_ALLOC }

/**
 * Parse length test parameter value.
 *
 * @param rpcs      RPC server.
 * @param len_str   Length parameter value.
 *
 * @return Parsed length numeric value.
 */
static int
parse_length(rcf_rpc_server *rpcs, const char *len_str)
{
#define MAX_STR_LEN 256

    int i;
    int result = 0;

    char cur_str[MAX_STR_LEN];
    char cur_type[MAX_STR_LEN];
    int  j;
    int  cur_sign = 1;

    for (i = 0, j = 0; ; i++)
    {
        if (isalnum(len_str[i]) || len_str[i] == '_')
        {
            if (j == MAX_STR_LEN)
                TEST_FAIL("Too big substring in length value");

            cur_str[j] = len_str[i];
            j++;
        }
        else if (!isspace(len_str[i]) || len_str[i] == '\0')
        {
            cur_str[j] = '\0';
            if (isalpha(cur_str[0]))
            {
                snprintf(cur_type, MAX_STR_LEN, "struct %s", cur_str);
                result += cur_sign * rpc_get_sizeof(rpcs, cur_type);
            }
            else
            {
                result += cur_sign * atoi(cur_str);
            }
            j = 0;
        }

        if (len_str[i] == '-')
            cur_sign = -1;
        else if (len_str[i] == '+')
            cur_sign = 1;
        else if (len_str[i] == '\0')
            break;
    }

    return result;
}

/**
 * Construct address according to specification.
 *
 * @param rpcs        RPC server.
 * @param dst         Where to save constructed address.
 * @param addr_ipv4   IPv4 network address.
 * @param addr_ipv6   IPv6 network address.
 * @param family      Address family to use.
 */
static void
gen_addr(rcf_rpc_server *rpcs,
         struct sockaddr_storage *dst,
         const struct sockaddr *addr_ipv4,
         const struct sockaddr *addr_ipv6,
         rpc_socket_addr_family family)
{
    static int i = 0;

    switch (family)
    {
        case RPC_AF_INET:
        {
            tapi_sockaddr_clone_exact(addr_ipv4, dst);
            break;
        }

        case RPC_AF_INET6:
        {
            tapi_sockaddr_clone_exact(addr_ipv6, dst);
            break;
        }

        case RPC_AF_UNIX:
        {
            pid_t rpcs_pid;

            SA(dst)->sa_family = AF_UNIX;
            rpcs_pid = rpc_getpid(rpcs);
            sprintf(((struct sockaddr_un *)dst)->sun_path,
                    "/tmp/unix_addr_for_%d_%d", (int)rpcs_pid, i++);

            break;
        }

        default:
            TEST_FAIL("Unknown address family");
    }
}

int
main(int argc, char *argv[])
{
    rcf_rpc_server        *pco_iut = NULL;
    rcf_rpc_server        *pco_tst = NULL;
    const struct sockaddr *iut_addr = NULL;
    const struct sockaddr *tst_addr = NULL;
    const struct sockaddr *iut_addr6 = NULL;
    const struct sockaddr *tst_addr6 = NULL;

    struct sockaddr_storage iut_addr_aux;
    struct sockaddr_storage tst_addr_aux;

    rpc_zf_attr_p  attr = RPC_NULL;
    rpc_zf_stack_p stack = RPC_NULL;

    rpc_zftl_p        iut_zftl = RPC_NULL;
    rpc_zft_handle_p  iut_zft_handle = RPC_NULL;
    rpc_zft_p         iut_zft = RPC_NULL;
    rpc_zfur_p        iut_zfur = RPC_NULL;
    rpc_zfut_p        iut_zfut = RPC_NULL;
    int               tst_s = -1;

    zfts_addr_func           function;
    rpc_socket_addr_family   family;
    const char              *length;

    te_bool mod_laddr;
    te_bool mod_raddr;

    int parsed_len;
    int sockaddr_in_len;

    int     laddr_len = 0;
    te_bool laddr_fwd = FALSE;
    int     raddr_len = 0;
    te_bool raddr_fwd = FALSE;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_PCO(pco_tst);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_ADDR(pco_iut, iut_addr6);
    TEST_GET_ADDR(pco_tst, tst_addr6);
    TEST_GET_ENUM_PARAM(function, ZFTS_ADDR_FUNCS);
    TEST_GET_ADDR_FAMILY(family);
    TEST_GET_STRING_PARAM(length);
    TEST_GET_BOOL_PARAM(mod_laddr);
    TEST_GET_BOOL_PARAM(mod_raddr);

    /*- Allocate Zetaferno attributes and stack. */
    zfts_create_stack(pco_iut, &attr, &stack);

    parsed_len = parse_length(pco_iut, length);
    sockaddr_in_len = rpc_get_sizeof(pco_iut, "struct sockaddr_in");

    if (mod_laddr)
    {
        laddr_len = parsed_len;
        laddr_fwd = TRUE;
        gen_addr(pco_iut, &iut_addr_aux, iut_addr, iut_addr6, family);
    }
    else
    {
        tapi_sockaddr_clone_exact(iut_addr, &iut_addr_aux);
    }

    if (mod_raddr)
    {
        raddr_len = parsed_len;
        raddr_fwd = TRUE;
        gen_addr(pco_tst, &tst_addr_aux, tst_addr, tst_addr6, family);
    }
    else
    {
        tapi_sockaddr_clone_exact(tst_addr, &tst_addr_aux);
    }

    /*- Allocate a zocket and call function according to @p function. */

    switch (function)
    {
        case ZFTS_ADDR_FUNC_ZFTL_LISTEN:
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zftl_listen_gen(pco_iut, stack, SA(&iut_addr_aux),
                                     laddr_len, laddr_fwd, attr, &iut_zftl);

            break;
        }

        case ZFTS_ADDR_FUNC_ZFT_ADDR_BIND:
        {
            rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);

            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zft_addr_bind_gen(pco_iut, iut_zft_handle,
                                       SA(&iut_addr_aux),
                                       laddr_len, laddr_fwd, 0);
            break;
        }

        case ZFTS_ADDR_FUNC_ZFT_CONNECT:
        {
            rpc_zft_alloc(pco_iut, stack, attr, &iut_zft_handle);
            rpc_zft_addr_bind(pco_iut, iut_zft_handle,
                              iut_addr, 0);

            tst_s =
              rpc_create_and_bind_socket(pco_tst, RPC_SOCK_STREAM,
                                         RPC_PROTO_DEF, FALSE, FALSE,
                                         tst_addr);
            rpc_listen(pco_tst, tst_s, 1);

            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zft_connect_gen(pco_iut, iut_zft_handle,
                                     SA(&tst_addr_aux),
                                     raddr_len, raddr_fwd, &iut_zft);
            if (rc >= 0)
                iut_zft_handle = RPC_NULL;

            break;
        }

        case ZFTS_ADDR_FUNC_ZFUR_ADDR_BIND:
        {
            rpc_zfur_alloc(pco_iut, &iut_zfur, stack, attr);

            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zfur_addr_bind_gen(pco_iut, iut_zfur,
                                        SA(&iut_addr_aux),
                                        laddr_len, laddr_fwd,
                                        SA(&tst_addr_aux),
                                        raddr_len, raddr_fwd,
                                        0);

            break;
        }

        case ZFTS_ADDR_FUNC_ZFUR_ADDR_UNBIND:
        {
            struct sockaddr_storage iut_addr_dup;

            tapi_sockaddr_clone_exact(iut_addr, &iut_addr_dup);

            rpc_zfur_alloc(pco_iut, &iut_zfur, stack, attr);
            rpc_zfur_addr_bind(pco_iut, iut_zfur,
                               SA(&iut_addr_dup), tst_addr, 0);

            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zfur_addr_unbind_gen(pco_iut, iut_zfur,
                                          SA(&iut_addr_aux),
                                          laddr_len, laddr_fwd,
                                          SA(&tst_addr_aux),
                                          raddr_len, raddr_fwd,
                                          0);

            break;
        }

        case ZFTS_ADDR_FUNC_ZFUT_ALLOC:
        {
            RPC_AWAIT_ERROR(pco_iut);
            rc = rpc_zfut_alloc_gen(pco_iut, &iut_zfut, stack,
                                    SA(&iut_addr_aux),
                                    laddr_len, laddr_fwd,
                                    SA(&tst_addr_aux),
                                    raddr_len, raddr_fwd,
                                    0, attr);

            break;
        }

        default:
            TEST_FAIL("Unknown function");
    }

    /*- Check that the call fails in all cases except when @p family is
     * @c AF_INET and @p length >= @c sockaddr_in. */

    if (parsed_len < sockaddr_in_len ||
        family != RPC_AF_INET)
    {
        if (rc >= 0)
        {
            TEST_VERDICT("Function succeeded with incorrect address");
        }
        else if (parsed_len < sockaddr_in_len)
        {
            if (RPC_ERRNO(pco_iut) != RPC_EINVAL)
                TEST_VERDICT("Function failed with unexpected errno %r "
                             "instead of EINVAL", RPC_ERRNO(pco_iut));
        }
        else if (family != RPC_AF_INET)
        {
            if (RPC_ERRNO(pco_iut) != RPC_EAFNOSUPPORT)
                TEST_VERDICT("Function failed with unexpected errno %r "
                             "instead of EAFNOSUPPORT", RPC_ERRNO(pco_iut));
        }
    }
    else
    {
        if (rc < 0)
            TEST_VERDICT("Function failed unexpectedly with errno %r",
                         RPC_ERRNO(pco_iut));
    }

    TEST_SUCCESS;

cleanup:

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft_handle, iut_zft_handle);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zft, iut_zft);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zftl, iut_zftl);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfur, iut_zfur);
    CLEANUP_RPC_ZFTS_FREE(pco_iut, zfut, iut_zfut);
    CLEANUP_RPC_ZFTS_DESTROY_STACK(pco_iut, attr, stack);

    TEST_END;
}

