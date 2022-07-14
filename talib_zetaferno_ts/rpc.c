/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/**
 * @brief RPC routines implementation
 *
 * Zetaferno Direct API RPC routines implementation.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#define TE_LGR_USER     "SFC Zetaferno RPC"

#include "te_config.h"
#include "config.h"

#include "logger_ta_lock.h"
#include "rpc_server.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif

#ifdef HAVE_NETPACKET_PACKET_H
#include <netpacket/packet.h>
#endif

#ifdef HAVE_NET_ETHERNET_H
#include <net/ethernet.h>
#endif

#ifdef HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#endif

#include "te_sockaddr.h"
#include "zf_talib_namespace.h"
#include "te_alloc.h"
#include "te_sleep.h"
#include "zf_rpc.h"
#include "iomux.h"

#include <zf/zf.h>
#include <zf/zf_udp.h>
#include <zf/zf_reactor.h>
#include <zf/attr.h>

/* See the function description in zf.h */
TARPC_FUNC(zf_init, {},
{
    MAKE_CALL(out->retval = func_void());
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

/* See the function description in zf.h */
TARPC_FUNC(zf_deinit, {},
{
    MAKE_CALL(out->retval = func_void());
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

/* See the function description in attr.h */
TARPC_FUNC(zf_attr_alloc, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zf_attr *attr;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_ATTR,);

    MAKE_CALL(out->retval = func_ptr(&attr));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    if (out->retval == 0)
        out->attr = RCF_PCH_MEM_INDEX_ALLOC(attr, ns);
})

/* See the function description in attr.h */
TARPC_FUNC(zf_attr_free, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zf_attr *attr;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_ATTR,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(attr, in->attr, ns,);

    MAKE_CALL(func_ptr(attr));
    RCF_PCH_MEM_INDEX_FREE(in->attr, ns);
})

/* See the function description in attr.h */
TARPC_FUNC(zf_attr_set_int, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zf_attr *attr;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_ATTR,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(attr, in->attr, ns,);

    MAKE_CALL(out->retval = func_ptr(attr, in->name, in->val));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
})

/**
 * Check that pthread_mutex_lock() or pthread_mutex_unlock()
 * returned @c 0; terminate with abort() otherwise.
 *
 * @param func_       Function call to check.
 */
#define CHECK_LOCK(func_) \
    do {                                            \
        int rc_;                                    \
                                                    \
        rc_ = func_;                                \
        if (rc_ != 0)                               \
        {                                           \
            ERROR("%s(): " #func_ " returned %d",   \
                  __FUNCTION__, rc_);               \
            abort();                                \
        }                                           \
    } while (0);

/**
 * Check whether a given environment variable is set
 * to @c "yes"/@c "true", return @c TRUE if it is and
 * @c FALSE otherwise.
 */
#define ENV_GET_ENABLED_STATE(var_) \
    do {                                                              \
        static te_bool enabled = FALSE;                               \
        static te_bool enabled_initialized = FALSE;                   \
                                                                      \
        static pthread_mutex_t   lock = PTHREAD_MUTEX_INITIALIZER;    \
                                                                      \
        CHECK_LOCK(pthread_mutex_lock(&lock));                        \
                                                                      \
        if (!enabled_initialized)                                     \
        {                                                             \
            char *enabled_env;                                        \
                                                                      \
            enabled_env = getenv(var_);                               \
                                                                      \
            if (enabled_env != NULL &&                                \
                (strcasecmp(enabled_env, "yes") == 0 ||               \
                 strcasecmp(enabled_env, "true") == 0))               \
                enabled = TRUE;                                       \
                                                                      \
            enabled_initialized = TRUE;                               \
        }                                                             \
                                                                      \
        CHECK_LOCK(pthread_mutex_unlock(&lock));                      \
        return enabled;                                               \
    } while (0)

/**
 * Check whether creating a thread performing zf_stack_has_pending_work()
 * for each stack is enabled or not.
 *
 * @return @c TRUE if enabled, @c FALSE otherwise.
 */
static te_bool
stack_threads_enabled(void)
{
    ENV_GET_ENABLED_STATE("TE_RPC_ZF_HAS_PENDING_THREADS_ENABLED");
}

/**
 * Check whether calling iomux on fd returned by zf_waitable_fd_get()
 * before calling zf_reactor_perform() is enabled or not.
 *
 * @return @c TRUE if enabled, @c FALSE otherwise.
 */
static te_bool
stack_iomux_enabled(void)
{
    ENV_GET_ENABLED_STATE("TE_RPC_ZF_STACK_FD_IOMUX_ENABLED");
}

/**
 * Start routine of a thread calling zf_stack_has_pending_work()
 * on a given stack.
 *
 * @param arg       Pointer to ZF stack.
 *
 * @return On success, never returns, should be canceled with
 *         pthread_cancel(). In case of failure terminates RPC server
 *         with abort().
 */
static void *
stack_thread_func(void *arg)
{
    struct zf_stack *stack = (struct zf_stack *)arg;

    api_func_ptr  has_pending_work_func;
    int           rc;

    if (tarpc_find_func(FALSE, "zf_stack_has_pending_work",
                        (api_func *)&has_pending_work_func) != 0)
    {
        ERROR("Failed to resolve zf_stack_has_pending_work() function");
        abort();
    }

    RING("Started a thread calling zf_stack_has_pending_work() "
         "for stack %p", stack);
    while (TRUE)
    {
        rc = has_pending_work_func(stack);
        if (rc < 0)
        {
            ERROR("zf_stack_has_pending_work returned negative value %d",
                  rc);
            abort();
        }
        pthread_testcancel();
    }

    return NULL;
}

/**
 * Structure associating ZF stack with auxiliary variables
 * (thread ID, iomux state, etc).
 */
typedef struct stack_ctx {
    struct zf_stack  *stack;            /**< Pointer to ZF stack. */

    pthread_t         thread_id;        /**< Thread ID. */

    int               fd;               /**< Stack fd returned by
                                             zf_waitable_fd_get(). */
    api_func_ptr      fd_prime_func;    /**< Pointer to
                                             zf_waitable_fd_prime(). */
    iomux_func        iomux;            /**< Iomux function type used
                                             to poll stack fd. */
    iomux_funcs       iomux_f;          /**< Pointers to iomux functions. */
    iomux_state       iomux_st;         /**< Iomux context. */
} stack_ctx;

/** Array of stack_ctx structures. */
static stack_ctx *stack_contexts = NULL;
/** Current number of elements in array. */
static int cur_stack_contexts_num = 0;
/** Maximum number of elements in array for which memory is allocated. */
static int max_stack_contexts_num = 0;

/** Mutex protecting stack_contexts array. */
static pthread_mutex_t   stack_contexts_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Default timeout to be used when calling iomux on stack fd,
 * milliseconds.
 */
#define STACK_FD_IOMUX_TIMEOUT 100

/**
 * Initialize thread-related fields of stack context.
 *
 * @param ctx   Pointer to stack context.
 *
 * @return @c 0 on success, or a negative value in case of failure.
 */
static int
init_thread_ctx(stack_ctx *ctx)
{
    if (pthread_create(&ctx->thread_id, NULL,
                       stack_thread_func, ctx->stack) != 0)
    {
        ERROR("Failed to create new stack thread");
        return -1;
    }

    return 0;
}

/**
 * Release resources allocated for thread-related fields of
 * stack context.
 *
 * @param ctx   Pointer to stack context.
 *
 * @return @c 0 on success, or a negative value in case of failure.
 */
static int
finish_thread_ctx(stack_ctx *ctx)
{
    void      *rt = NULL;

    if (pthread_cancel(ctx->thread_id) != 0 ||
        pthread_join(ctx->thread_id, &rt) != 0)
    {
        ERROR("Failed to cancel and join stack thread");
        return -1;
    }

    return 0;
}

/**
 * Initialize iomux-related fields of stack context.
 *
 * @param ctx   Pointer to stack context.
 *
 * @return @c 0 on success, or a negative value in case of failure.
 */
static int
init_iomux_ctx(stack_ctx *ctx)
{
    char *default_iomux = getenv("TE_RPC_ZF_STACK_FD_IOMUX_DEF");

    api_func_ptr  fd_get_func = NULL;
    int           rc;

    if (tarpc_find_func(FALSE, "zf_waitable_fd_get",
                        (api_func *)&fd_get_func) != 0)
    {
        ERROR("Failed to resolve zf_waitable_fd_get()");
        return -1;
    }

    if (tarpc_find_func(FALSE, "zf_waitable_fd_prime",
                        (api_func *)&ctx->fd_prime_func) != 0)
    {
        ERROR("Failed to resolve zf_waitable_fd_prime()");
        return -1;
    }

    rc = fd_get_func(ctx->stack, &ctx->fd);
    if (rc < 0)
    {
        ERROR("zf_waitable_fd_get() returned %d", rc);
        return -1;
    }
    else if (ctx->fd < 0)
    {
        ERROR("zf_waitable_fd_get() returned negative fd");
        return -1;
    }

    if (default_iomux == NULL)
        ctx->iomux = FUNC_POLL;
    else
        ctx->iomux = str2iomux(default_iomux);

    if (iomux_find_func(FALSE, &ctx->iomux,
                        &ctx->iomux_f) != 0 ||
        iomux_create_state(ctx->iomux, &ctx->iomux_f,
                           &ctx->iomux_st) != 0)
    {
        ERROR("Failed to initialize iomux state");
        return -1;
    }

    if (iomux_add_fd(ctx->iomux, &ctx->iomux_f,
                     &ctx->iomux_st, ctx->fd, POLLIN) != 0)
    {
        iomux_close(ctx->iomux, &ctx->iomux_f, &ctx->iomux_st);
        return -1;
    }

    return 0;
}

/**
 * Release resources allocated for iomux-related fields of stack context.
 *
 * @param ctx   Pointer to stack context.
 *
 * @return @c 0 on success, or a negative value in case of failure.
 */
static int
finish_iomux_ctx(stack_ctx *ctx)
{
    if (iomux_close(ctx->iomux, &ctx->iomux_f, &ctx->iomux_st) < 0)
        return -1;

    return 0;
}

/**
 * Allocate context for a given stack, initialize those fields
 * which are enabled according to environment.
 *
 * @param stack       Pointer to ZF stack.
 *
 * @return @c 0 on success, or negative value in case of failure.
 */
static int
add_stack_ctx(struct zf_stack *stack)
{
    int i = 0;
    int rc = -1;

    CHECK_LOCK(pthread_mutex_lock(&stack_contexts_lock));

    for (i = 0; i < cur_stack_contexts_num; i++)
    {
        if (stack_contexts[i].stack == NULL)
            break;
    }

    if (i == cur_stack_contexts_num)
    {
        cur_stack_contexts_num++;

        if (cur_stack_contexts_num > max_stack_contexts_num)
        {
            int   new_max = max_stack_contexts_num * 2 + 1;
            void *p = NULL;

            p = realloc(stack_contexts, new_max * sizeof(*stack_contexts));
            if (p == NULL)
            {
                ERROR("Failed to allocate more memory for stack contexts");
                goto cleanup;
            }

            stack_contexts = (stack_ctx *)p;
            max_stack_contexts_num = new_max;
        }
    }

    stack_contexts[i].stack = stack;

    if (stack_threads_enabled())
    {
        if (init_thread_ctx(&stack_contexts[i]) != 0)
            goto cleanup;
    }

    if (stack_iomux_enabled())
    {
        if (init_iomux_ctx(&stack_contexts[i]) != 0)
            goto cleanup;
    }

    rc = 0;

cleanup:
    CHECK_LOCK(pthread_mutex_unlock(&stack_contexts_lock));
    return rc;
}

/**
 * Get stack context corresponding to a given stack.
 *
 * @param stack     Pointer to ZF stack.
 *
 * @return Stack context pointer on success, or @c NULL in case
 *         of failure.
 */
static stack_ctx *
get_stack_ctx(struct zf_stack *stack)
{
    int        i;

    for (i = 0; i < cur_stack_contexts_num; i++)
    {
        if (stack_contexts[i].stack == stack)
            break;
    }

    if (i >= cur_stack_contexts_num)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_ENOENT),
                         "failed to find stack context");
        return NULL;
    }

    return &stack_contexts[i];
}

/**
 * Remove context allocated for a given stack, release
 * resources allocated for its fields.
 *
 * @param stack     Pointer to ZF stack.
 *
 * @return @c 0 on success, or a negative value in case of failure.
 */
static int
del_stack_ctx(struct zf_stack *stack)
{
    stack_ctx *ctx = NULL;
    int        rc = -1;

    CHECK_LOCK(pthread_mutex_lock(&stack_contexts_lock));

    ctx = get_stack_ctx(stack);
    if (ctx == NULL)
        goto cleanup;

    if (stack_threads_enabled())
    {
        if (finish_thread_ctx(ctx) != 0)
            goto cleanup;
    }

    if (stack_iomux_enabled())
    {
        if (finish_iomux_ctx(ctx) != 0)
            goto cleanup;
    }

    ctx->stack = NULL;
    rc = 0;

cleanup:
    CHECK_LOCK(pthread_mutex_unlock(&stack_contexts_lock));
    return rc;
}

/* See the function description in zf.h */
TARPC_FUNC(zf_stack_alloc, {},
{
    static rpc_ptr_id_namespace stack_ns = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace attr_ns = RPC_PTR_ID_NS_INVALID;
    struct zf_stack *stack;
    struct zf_attr *attr;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&stack_ns,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&attr_ns, RPC_TYPE_NS_ZF_ATTR,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(attr, in->attr, attr_ns,);

    MAKE_CALL(out->retval = func_ptr(attr, &stack));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);

    if (out->retval == 0)
    {
        out->stack = RCF_PCH_MEM_INDEX_ALLOC(stack, stack_ns);
        if (add_stack_ctx(stack) != 0)
            abort();
    }
})

/* See the function description in zf.h */
TARPC_FUNC(zf_stack_free, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zf_stack* stack;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns,);

    if (del_stack_ctx(stack) != 0)
        abort();

    MAKE_CALL(out->retval = func_ptr(stack));
    TE_RPC_CONVERT_NEGATIVE_ERR(out->retval);
    if (out->retval == 0)
        RCF_PCH_MEM_INDEX_FREE(in->stack, ns);
})

/* See the function description in zf_stack.h */
TARPC_FUNC(zf_stack_to_waitable, {},
{
    static rpc_ptr_id_namespace ns_stack = RPC_PTR_ID_NS_INVALID;
    static rpc_ptr_id_namespace ns_zf_w = RPC_PTR_ID_NS_INVALID;

    struct zf_stack     *stack = NULL;
    struct zf_waitable  *waitable = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_stack,
                                           RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns_zf_w,
                                           RPC_TYPE_NS_ZF_WAITABLE,);

    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns_stack,);

    MAKE_CALL(waitable = func_ptr_ret_ptr(stack));
    out->zf_waitable = RCF_PCH_MEM_INDEX_ALLOC(waitable, ns_zf_w);
})

/* See the function description in zf_stack.h */
TARPC_FUNC(zf_stack_is_quiescent, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zf_stack* stack;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns,);

    MAKE_CALL(out->retval = func_ptr(stack));

    /*
     * Make RPC call return negative value only in case of
     * some RPC failure. zf_stack_is_quiescent() returns either
     * zero (if stack is not quiescent) or nonzero (if stack is
     * quiescent). If nonzero is negative, it does not do any harm
     * to change its sign, it still would mean the same. So let's
     * reserve negative value for some RPC failure here, which may
     * happen if zf_stack_is_quiescent() results in segfault and
     * does not return any value at all.
     */
    if (out->retval < 0)
        out->retval = -out->retval;
})

/* See the function description in zf_reactor.h */
TARPC_FUNC(zf_stack_has_pending_work, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zf_stack* stack;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns,);

    MAKE_CALL(out->retval = func_ptr(stack));
})

/**
 * Check whether calling zf_stack_has_pending_work() before every
 * zf_reactor_perform() call is enabled or not.
 *
 * @return @c TRUE if enabled, @c FALSE otherwise.
 */
static te_bool
has_pending_reactor_enabled(void)
{
    ENV_GET_ENABLED_STATE("TE_RPC_ZF_HAS_PENDING_REACTOR_ENABLED");
}

/**
 * Call iomux on fd returned by zf_waitable_fd_get().
 *
 * @param stack         Pointer to ZF stack.
 * @param timeout       Iomux timeout.
 * @param log_success   If @c FALSE, log only errors,
 *                      otherwise log successful iomux call
 *                      too.
 *
 * @return @c 0 or @c 1 (iomux function return value) on success,
 *         or a negative value in case of failure.
 */
static int
zfts_call_stack_fd_iomux(struct zf_stack *stack, int timeout,
                         te_bool log_success)
{
    stack_ctx *ctx = NULL;

    int           rc = 0;
    int           iomux_rc = 0;
    iomux_return  iomux_ret;
    te_bool       iomux_failed = FALSE;
    int           events = 0;
    int           fd = -1;

    iomux_return_iterator it;

    CHECK_LOCK(pthread_mutex_lock(&stack_contexts_lock));

    ctx = get_stack_ctx(stack);
    if (ctx == NULL)
    {
        CHECK_LOCK(pthread_mutex_unlock(&stack_contexts_lock));
        return -1;
    }

    if ((rc = ctx->fd_prime_func(ctx->stack)) < 0)
    {
        te_rpc_error_set(TE_OS_RC(TE_TA_UNIX, -rc),
                         "zf_waitable_fd_prime() failed");
        CHECK_LOCK(pthread_mutex_unlock(&stack_contexts_lock));
        return -1;
    }

    iomux_rc = iomux_wait(ctx->iomux, &ctx->iomux_f, &ctx->iomux_st,
                          &iomux_ret, timeout);

    iomux_failed = TRUE;
    if (iomux_rc < 0)
    {
        te_rpc_error_set(TE_OS_RC(TE_TA_UNIX, errno),
                         "%s iomux failed", iomux2str(ctx->iomux));
    }
    else if (iomux_rc > 1)
    {
        ERROR("%s(): %s iomux returned strange result %d",
              __FUNCTION__, iomux2str(ctx->iomux), iomux_rc);
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_EFAIL),
                         "%s iomux returned unexpected value",
                         iomux2str(ctx->iomux));
    }
    else if (iomux_rc == 0)
    {
        if (timeout < 0)
        {
            te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_EFAIL),
                             "%s iomux returned zero with negative timeout",
                             iomux2str(ctx->iomux));
        }
        else
        {
            if (log_success)
                RING("%s(): %s iomux returned zero",
                     __FUNCTION__, iomux2str(ctx->iomux));
            iomux_failed = FALSE;
        }
    }
    else
    {
        it = iomux_return_iterate(ctx->iomux, &ctx->iomux_st, &iomux_ret,
                                  IOMUX_RETURN_ITERATOR_START,
                                  &fd, &events);
        if (it == IOMUX_RETURN_ITERATOR_END)
        {
            te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_EFAIL),
                             "%s iomux failed to obtain event with "
                             "iterator", iomux2str(ctx->iomux));
        }
        else if (fd != ctx->fd)
        {
            ERROR("%s(): %s iomux returned event for unknown fd %d",
                  __FUNCTION__, iomux2str(ctx->iomux), fd);

            te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_EFAIL),
                             "%s iomux obtained an event for unknown fd");
        }
        else if (events != POLLIN)
        {
            te_rpc_error_set(
                  TE_RC(TE_TA_UNIX, TE_EFAIL),
                  "%s iomux returned unexpected events %s",
                  iomux2str(ctx->iomux),
                  poll_event_rpc2str(poll_event_h2rpc(events)));
        }
        else
        {
            if (log_success)
                RING("%s(): %s iomux returned POLLIN for stack fd",
                     __FUNCTION__, iomux2str(ctx->iomux));
            iomux_failed = FALSE;
        }
    }

    CHECK_LOCK(pthread_mutex_unlock(&stack_contexts_lock));
    if (iomux_failed)
        return -1;
    return iomux_rc;
}

/**
 * Call zf_reactor_perform(); call zf_stack_has_pending_work() before
 * that if required.
 *
 * @param stack             Pointer to ZF stack.
 * @param reactor_ptr       Pointer to zf_reactor_perform() function
 *                          pointer (if it points to NULL value, it will
 *                          be updated by this function).
 * @param has_pending_ptr   Pointer to zf_stack_has_pending_work() function
 *                          pointer (if it points to NULL value, it will
 *                          be updated by this function).
 *
 * @return Return value of zf_reactor_perform() on success,
 *         negative value in case of failure.
 */
static int
zfts_call_zf_reactor_perform(struct zf_stack *stack,
                             api_func_ptr *reactor_ptr,
                             api_func_ptr *has_pending_ptr)
{
    api_func_ptr  reactor_func = NULL;
    api_func_ptr  has_pending_func = NULL;
    int           rc;

    if (reactor_ptr != NULL)
        reactor_func = *reactor_ptr;
    if (has_pending_ptr != NULL)
        has_pending_func = *has_pending_ptr;

    if (reactor_func == NULL)
    {
        rc = tarpc_find_func(FALSE, "zf_reactor_perform",
                             (api_func *)&reactor_func);
        if (rc != 0)
        {
            te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_ENOENT),
                             "failed to resolve zf_reactor_perform()");
            return -1;
        }
    }

    if (has_pending_reactor_enabled())
    {
        if (has_pending_func == NULL)
        {
            rc = tarpc_find_func(FALSE, "zf_stack_has_pending_work",
                                 (api_func *)&has_pending_func);
            if (rc != 0)
            {
                te_rpc_error_set(
                         TE_RC(TE_TA_UNIX, TE_ENOENT),
                         "failed to resolve zf_stack_has_pending_work()");
                return -1;
            }
        }

        rc = has_pending_func(stack);
        if (rc < 0)
        {
            te_rpc_error_set(
                     TE_OS_RC(TE_TA_UNIX, -rc),
                     "zf_stack_has_pending_work() returned negative value");
            return -1;
        }
    }

    if (reactor_ptr != NULL)
        *reactor_ptr = reactor_func;
    if (has_pending_ptr != NULL)
        *has_pending_ptr = has_pending_func;

    return reactor_func(stack);
}

/* See the function description in zf_reactor.h */
TARPC_FUNC(zf_reactor_perform, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zf_stack* stack;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns,);

    MAKE_CALL(out->retval =
                  zfts_call_zf_reactor_perform(stack,
                                               (api_func_ptr *)&func_ptr,
                                               NULL));
})

/**
 * Run in the loop calling @a zf_reactor_perform until an event is observed.
 *
 * @param stack Pointer ZF stack object.
 *
 * @return Zero on success or a negative value in case of fail.
 */
int
zf_wait_for_event(struct zf_stack *stack)
{
    int     rc;
    te_bool first_iomux_call = TRUE;

    api_func_ptr reactor_func = NULL;
    api_func_ptr has_pending_func = NULL;

    do {
        if (stack_iomux_enabled())
        {
            if (first_iomux_call)
                rc = zfts_call_stack_fd_iomux(stack, -1, TRUE);
            else
                rc = zfts_call_stack_fd_iomux(stack,
                                              STACK_FD_IOMUX_TIMEOUT,
                                              FALSE);
            first_iomux_call = FALSE;

            if (rc < 0)
                break;
            else if (rc == 0) /* No event, do not call reactor. */
                continue;
        }

        rc = zfts_call_zf_reactor_perform(stack,
                                          &reactor_func,
                                          &has_pending_func);
    } while (rc == 0);

    if (rc < 0)
        return rc;
    return 0;
}

TARPC_FUNC_STATIC(zf_wait_for_event, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zf_stack* stack;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns,);

    MAKE_CALL(out->retval = func_ptr(stack));
})

/**
 * Run in the loop calling @a zf_reactor_perform until all events are
 * processed.
 *
 * @param stack Pointer ZF stack object.
 *
 * @return Events number or a negative value in case of fail.
 */
int
zf_process_events(struct zf_stack *stack)
{
    int rc = 0;
    int count = 0;

    api_func_ptr reactor_func = NULL;
    api_func_ptr has_pending_func = NULL;

    while ((rc = zfts_call_zf_reactor_perform(stack,
                                              &reactor_func,
                                              &has_pending_func)) > 0)
        count += rc;

    if (rc < 0)
        return rc;
    return count;
}

TARPC_FUNC_STATIC(zf_process_events, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    struct zf_stack* stack;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns,);

    MAKE_CALL(out->retval = func_ptr(stack));
})

/**
 * Call @a zf_reactor_perform repeatedly until timeout is expired.
 *
 * @param st          Pointer to ZF stack object
 * @param duration    Timeout (in milliseconds)
 *
 * @return Number of events encountered in case of success,
 *         negative value in case of failure.
 */
int
zf_process_events_long(struct zf_stack *st,
                       int duration)
{
    int rc = 0;
    int events_count = 0;

    te_bool first_iomux_call = TRUE;

    struct timeval tv_start;
    struct timeval tv_now;

    api_func_ptr reactor_func = NULL;
    api_func_ptr has_pending_func = NULL;

    gettimeofday(&tv_start, NULL);

    while (1)
    {
        if (stack_iomux_enabled())
        {
            if (first_iomux_call)
                rc = zfts_call_stack_fd_iomux(st, -1, TRUE);
            else
                rc = zfts_call_stack_fd_iomux(st, STACK_FD_IOMUX_TIMEOUT,
                                              FALSE);

            first_iomux_call = FALSE;

            if (rc < 0)
                return rc;
            else if (rc == 0) /* No event, do not call reactor. */
                continue;
        }

        rc = zfts_call_zf_reactor_perform(st,
                                          &reactor_func,
                                          &has_pending_func);
        if (rc >= 0)
            events_count += rc;
        else
            return rc;

        gettimeofday(&tv_now, NULL);
        if (TIMEVAL_SUB(tv_now, tv_start) / 1000L > duration)
            break;
    }

    return events_count;
}

TARPC_FUNC_STATIC(zf_process_events_long, {},
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;

    struct zf_stack    *stack = NULL;

    out->common._errno = TE_RC(TE_RCF_PCH, TE_EFAIL);
    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_STACK,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(stack, in->stack, ns,);

    MAKE_CALL(out->retval = func_ptr(stack, in->duration));
})

/**
 * Release RPC pointer of a @b zf_waitable object.
 *
 * @param ptr  RPC pointer identifier of the ZF waitable object.
 *
 * @return Status code.
 */
int
zf_waitable_free(rpc_zf_waitable_p ptr)
{
    static rpc_ptr_id_namespace ns = RPC_PTR_ID_NS_INVALID;
    te_errno rc;

    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&ns, RPC_TYPE_NS_ZF_WAITABLE,
                                           -EINVAL);
    rc = RCF_PCH_MEM_INDEX_FREE(ptr, ns);

    if (rc != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, rc),
                         "failed to release zf_waitable RPC pointer");
        return -1;
    }

    return 0;
}

TARPC_FUNC_STATIC(zf_waitable_free, {},
{
    MAKE_CALL(out->retval = zf_waitable_free(in->zf_waitable));
})

/**
 * If this variable is set to @c TRUE, all alloc_free_stack_thread()
 * calls should terminate.
 */
static volatile te_bool stop_alloc_free_stack_threads = FALSE;

/** Arguments passed to alloc_free_stack_thread() */
typedef struct alloc_free_stack_args {
    api_func_ptr    stack_alloc;    /**< Pointer to zf_stack_alloc() */
    api_func_ptr    stack_free;     /**< Pointer to zf_stack_free() */
    struct zf_attr *attr;           /**< Pointer to ZF attributes */
    int wait_after_alloc;           /**< How long to wait after
                                         allocating ZF stack,
                                         in microseconds */
} alloc_free_stack_args;

/**
 * Main function of a thread creating and destroying ZF stack
 * in an infinite loop.
 *
 * @param arg     Pointer to alloc_free_stack_args structure.
 *
 * @return @c NULL.
 */
static void *
alloc_free_stack_thread(void *arg)
{
    alloc_free_stack_args *args = (alloc_free_stack_args *)arg;
    struct zf_stack *stack;
    int rc;

    while (!stop_alloc_free_stack_threads)
    {
        rc = args->stack_alloc(args->attr, &stack);
        if (rc < 0)
        {
            if (rc != -ENOMEM)
            {
                ERROR("zf_stack_alloc() returned unexpected error "
                      "%d (-%r)", rc, te_rc_os2te(-rc));
            }
        }

        usleep(args->wait_after_alloc);
        if (rc == 0)
        {
            rc = args->stack_free(stack);
            if (rc != 0)
            {
                ERROR("zf_stack_free() returned %d (-%r)", rc,
                      te_rc_os2te(-rc));
            }
        }
    }

    return NULL;
}

/**
 * Create multiple threads of alloc_free_stack_thread(). Wait for
 * their termination (which normally should never happen).
 *
 * @param attr              ZF attributes.
 * @param threads_num       Number of threads to create.
 * @param wait_after_alloc  How long to wait after stack allocation
 *                          (in microseconds).
 *
 * @return @c 0 on success, @c -1 on failure.
 */
static int
zf_many_threads_alloc_free_stack(struct zf_attr *attr, int threads_num,
                                 int wait_after_alloc)
{
    int threads_created = 0;
    int i;
    int rc;
    int result = 0;

    pthread_t *threads = NULL;
    alloc_free_stack_args thread_args;

    stop_alloc_free_stack_threads = FALSE;

    memset(&thread_args, 0, sizeof(thread_args));
    thread_args.attr = attr;
    thread_args.wait_after_alloc = wait_after_alloc;

    rc = tarpc_find_func(FALSE, "zf_stack_alloc",
                         (api_func *)&thread_args.stack_alloc);
    if (rc != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, rc),
                         "Failed to resolve zf_stack_alloc()");
        return -1;
    }

    rc = tarpc_find_func(FALSE, "zf_stack_free",
                         (api_func *)&thread_args.stack_free);
    if (rc != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, rc),
                         "Failed to resolve zf_stack_free()");
        return -1;
    }

    threads = calloc(threads_num, sizeof(*threads));
    if (threads == NULL)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_ENOMEM),
                         "Not enough memory to create an array of threads");
        result = -1;
        goto cleanup;
    }

    for (i = 0; i < threads_num; i++)
    {
         rc = pthread_create(&threads[i], NULL,
                             &alloc_free_stack_thread, &thread_args);
         if (rc != 0)
         {
            ERROR("%s(): failed to create thread %d", __FUNCTION__, i);
            te_rpc_error_set(TE_OS_RC(TE_TA_UNIX, rc),
                             "pthread_create() failed");
            result = -1;
            stop_alloc_free_stack_threads = TRUE;
            goto cleanup;
        }

        threads_created++;
    }

cleanup:

    for (i = 0; i < threads_created; i++)
    {
        rc = pthread_join(threads[i], NULL);
        if (rc != 0)
        {
            ERROR("%s(): failed to join thread %d", __FUNCTION__, i);
            te_rpc_error_set(TE_OS_RC(TE_TA_UNIX, rc),
                             "pthread_join() failed");
            result = -1;
        }

    }

    free(threads);

    return result;
}

TARPC_FUNC_STATIC(zf_many_threads_alloc_free_stack, {},
{
    struct zf_attr *attr;
    static rpc_ptr_id_namespace attr_ns = RPC_PTR_ID_NS_INVALID;

    RCF_PCH_MEM_NS_CREATE_IF_NEEDED_RETURN(&attr_ns, RPC_TYPE_NS_ZF_ATTR,);
    RCF_PCH_MEM_INDEX_TO_PTR_RPC(attr, in->attr, attr_ns,);

    MAKE_CALL(out->retval = func_ptr(attr, in->threads_num,
                                     in->wait_after_alloc));
})

/* See description in zf_rpc.h */
int
prepare_pkt_reports(struct zf_pkt_report **reps_out,
                    struct zf_pkt_report **reps_copy_out,
                    tarpc_zf_pkt_report *tarpc_reports,
                    size_t reports_num)
{
    struct zf_pkt_report *reports;
    struct zf_pkt_report *reports_copy;
    tarpc_zf_pkt_report *tarpc_report;
    size_t i;
    size_t total_size;

    total_size = sizeof(*reports) * reports_num;
    reports = TE_ALLOC(total_size);
    reports_copy = TE_ALLOC(total_size);
    if (reports == NULL || reports_copy == NULL)
    {
        free(reports);
        free(reports_copy);

        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_ENOMEM),
                         "failed to allocate memory for packet reports");
        return -1;
    }

    for (i = 0; i < reports_num; i++)
    {
        tarpc_report = &tarpc_reports[i];
        reports[i].timestamp.tv_sec = tarpc_report->timestamp.tv_sec;
        reports[i].timestamp.tv_nsec = tarpc_report->timestamp.tv_nsec;
        reports[i].start = tarpc_report->start;
        reports[i].bytes = tarpc_report->bytes;
        reports[i].flags = tarpc_report->flags;
    }
    memcpy(reports_copy, reports, total_size);

    *reps_out = reports;
    *reps_copy_out = reports_copy;
    return 0;
}

/* See description in zf_rpc.h */
int
process_pkt_reports(struct zf_pkt_report *reports,
                    struct zf_pkt_report *reports_copy,
                    tarpc_zf_pkt_report *tarpc_reports,
                    size_t reports_num, int count)
{
    tarpc_zf_pkt_report *tarpc_report;
    int res = 0;
    int i;

    if (count < 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_EINVAL),
                         "TX timestamps function returned negative number "
                         "or packet reports");
        res = -1;
    }
    else if (count < (int)reports_num &&
             memcmp(reports + count, reports_copy + count,
                    (reports_num - count) * sizeof(*reports)) != 0)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_ECORRUPTED),
                         "TX timestamps function changes array of packet "
                         "reports beyond count returned by it");
        res = -1;
    }

    if (count > (int)reports_num)
    {
        te_rpc_error_set(TE_RC(TE_TA_UNIX, TE_EINVAL),
                         "TX timestamps function returned too big "
                         "count");
        res = -1;
    }

    for (i = 0; i < MIN(count, (int)reports_num); i++)
    {
        tarpc_report = &tarpc_reports[i];
        tarpc_report->timestamp.tv_sec = reports[i].timestamp.tv_sec;
        tarpc_report->timestamp.tv_nsec = reports[i].timestamp.tv_nsec;
        tarpc_report->start = reports[i].start;
        tarpc_report->bytes = reports[i].bytes;
        tarpc_report->flags = zf_pkt_report_flags_h2rpc(reports[i].flags);
    }

    free(reports);
    free(reports_copy);

    return res;
}
