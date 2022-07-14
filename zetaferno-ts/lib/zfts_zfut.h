/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Auxiliary test API for UDP TX zockets
 *
 * Definition of auxiliary test API to work with UDP TX zockets.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 *
 * $Id:$
 */

#ifndef ___ZFTS_ZFUT_H__
#define ___ZFTS_ZFUT_H__

/**
 * Transmitting functions list, can be passed to macro
 * @b TEST_GET_ENUM_PARAM.
 */
#define ZFTS_SEND_FUNCTIONS  \
    { "zfut_send", ZFTS_ZFUT_SEND }, \
    { "zfut_send_single", ZFTS_ZFUT_SEND_SINGLE }

/**
 * Get ZF UDP send function.
 */
#define ZFTS_TEST_GET_ZFUT_FUNCTION(var_name_) \
    TEST_GET_ENUM_PARAM(var_name_, ZFTS_SEND_FUNCTIONS)

/**
 * A data buffer and its representation in iov vectors.
 */
typedef struct zfts_zfut_buf {
    rpc_iovec *iov;     /**< Iov vectors array. */
    size_t     iovcnt;  /**< The array length. */
    void      *data;    /**< Pointer to the buffer. */
    size_t     len;     /**< The buffer length. */
} zfts_zfut_buf;

/**
 * Allocate a buffer with length in the range [ @p min; @p max ] and
 * intialize structure @a zfts_zfut_buf by it.
 *
 * @note Function @a zfts_zfut_buf_release() should be used to release
 * resources of @p buf.
 *
 * @param iovcnt    Vectors number.
 * @param min       Minimum buffer length.
 * @param max       Maximum buffer length.
 * @param buf       Initializing buffers structure.
 */
extern void zfts_zfut_buf_make(size_t iovcnt, size_t min, size_t max,
                               zfts_zfut_buf *buf);

/**
 * Intialize structure @a zfts_zfut_buf with appropriate data buffer length
 * and iov vector size.
 *
 * @note Function @a zfts_zfut_buf_release() should be used to release
 * resources of @p buf.
 *
 * @param large_buffer  Use large data buffer to send.
 * @param few_iov       Use several iov vectors.
 * @param buf           Initializing buffers structure.
 */
extern void zfts_zfut_buf_make_param(te_bool large_buffer, te_bool few_iov,
                                     zfts_zfut_buf *buf);

/**
 * Release allocated data in the data structure @a zfts_zfut_buf.
 *
 * @param buf   The data structure to be cleaned.
 */
extern void zfts_zfut_buf_release(zfts_zfut_buf *buf);

/**
 * Transmit a datagram from @p utx to tester, receive the datagram and
 * validate data.
 *
 * @param pco_iut       IUT RPC server.
 * @param stack         RPC pointer to Zetaferno stack object.
 * @param utx           RPC pointer to Zetaferno UDP TX zocket object.
 * @param pco_tst       Tester RPC server.
 * @param tst_s         Bound tester socket.
 * @param send_f        ZF send function to send datagrams.
 * @param large_buffer  Use large data buffer to send.
 * @param few_iov       Use several iov vectors.
 */
extern void zfts_zfut_check_send_func(rcf_rpc_server *pco_iut,
                                      rpc_zf_stack_p stack, rpc_zfut_p utx,
                                      rcf_rpc_server *pco_tst, int tst_s,
                                      zfts_send_function send_f,
                                      te_bool large_buffer,
                                      te_bool few_iov);

/**
 * Transmit a datagram from @p utx to tester, receive the datagram and
 * validate data. This function works like zfts_zfut_check_send_func()
 * but allows to print errors instead of using TEST_VERDICT().
 *
 * @param pco_iut       IUT RPC server.
 * @param stack         RPC pointer to Zetaferno stack object.
 * @param utx           RPC pointer to Zetaferno UDP TX zocket object.
 * @param pco_tst       Tester RPC server.
 * @param tst_s         Bound tester socket.
 * @param send_f        ZF send function to send datagrams.
 * @param large_buffer  Use large data buffer to send.
 * @param few_iov       Use several iov vectors.
 * @param print_verdict If @c TRUE, print verdict and stop test in case
 *                      of failure. Otherwise print error message and
 *                      return error code in case of failure.
 * @param len           If not @c NULL, length of sent datagram will
 *                      be saved here.
 *
 * @return Status code (@c 0 on success, @c TE_ENODATA in case Tester did
 *         not receive any data, @c TE_CORRUPTED in case received data did
 *         not match sent data).
 */
extern te_errno zfts_zfut_check_send_func_ext(rcf_rpc_server *pco_iut,
                                              rpc_zf_stack_p stack,
                                              rpc_zfut_p utx,
                                              rcf_rpc_server *pco_tst,
                                              int tst_s,
                                              zfts_send_function send_f,
                                              te_bool large_buffer,
                                              te_bool few_iov,
                                              te_bool print_verdict,
                                              size_t *len);

/**
 * Send a datagram using one of UDP TX functions.
 *
 * @note This function just calls RPC call correspoding to
 *       @p send_f, no checks are performed on what is returned.
 *
 * @param pco_iut       IUT RPC server.
 * @param utx           RPC pointer to Zetaferno UDP TX zocket object.
 * @param snd           Send buffers structure.
 * @param send_f        ZF send function to send datagrams.
 *
 * @return Return value of RPC call.
 */
int
zfts_zfut_send_no_check(rcf_rpc_server *rpcs, rpc_zfut_p utx,
                        zfts_zfut_buf *snd,
                        zfts_send_function send_f);

/**
 * Send a datagram using one of UDP TX functions.
 *
 * @param pco_iut       IUT RPC server.
 * @param utx           RPC pointer to Zetaferno UDP TX zocket object.
 * @param snd           Send buffers structure.
 * @param send_f        ZF send function to send datagrams.
 *
 * @return Sent data amount.
 */
extern int zfts_zfut_send(rcf_rpc_server *rpcs, rpc_zfut_p utx,
                          zfts_zfut_buf *snd, zfts_send_function send_f);

/**
 * Send a datagram using one of UDP TX functions. The same as zfts_zfut_send,
 * but in case of send function returns EAGAIN try to call zf_reactor_perform()
 * until it returns non-zero and try again by running zfts_zfut_send().
 *
 * @param pco_iut       IUT RPC server.
 * @param stack         RPC pointer to Zetaferno stack object.
 * @param utx           RPC pointer to Zetaferno UDP TX zocket object.
 * @param snd           Send buffers structure.
 * @param send_f        ZF send function to send datagrams.
 *
 * @return Sent data amount.
 */
extern int zfts_zfut_send_ext(rcf_rpc_server *rpcs, rpc_zf_stack_p stack,
                              rpc_zfut_p utx, zfts_zfut_buf *snd,
                              zfts_send_function send_f);

#endif /* !___ZFTS_ZFUT_H__ */
