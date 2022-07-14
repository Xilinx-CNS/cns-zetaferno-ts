/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2016 - 2022 Xilinx, Inc. All rights reserved. */
/*
 * Zetaferno Direct API Test Suite
 * Bad Parameters and Boundary Values
 */

/**
 * @page bnbvalue-too_many_stacks_kill Receiving a signal when many threads create/destroy ZF stack
 *
 * @objective Check what happens when many threads create and destroy
 *            ZF stacks in infinite loop in parallel (and some of them
 *            fail because there are too many stacks), and then they are
 *            all killed with @c SIGKILL.
 *
 * @note This test is intended to reproduce kernel panic from
 *       ST-2017/ON-11907.
 *
 * @param threads_num           Number of threads:
 *                              - @c 512
 * @param nr_hugepages          Value to set for /proc/sys/vm/nr_hugepages:
 *                              - @c 3000
 *
 * @par Scenario:
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_TEST_NAME "bnbvalue/too_many_stacks_kill"

#include "zf_test.h"
#include "rpc_zf.h"

/**
 * How long to wait after ZF stack allocation before freeing
 * it, in microseconds.
 */
#define WAIT_AFTER_ALLOC 100000

/**
 * How long to wait before killing auxiliary process, in
 * milliseconds.
 */
#define WAIT_BEFORE_KILL 2000

/**
 * How long to wait after killing auxiliary process, in
 * milliseconds.
 */
#define WAIT_AFTER_KILL 500

int
main(int argc, char *argv[])
{
    rcf_rpc_server *pco_iut = NULL;
    rcf_rpc_server *pco_iut_aux = NULL;
    pid_t killed_pid;
    te_bool done;
    rpc_wait_status wait_status;
    rpc_zf_attr_p attr = RPC_NULL;

    int threads_num;
    int nr_hugepages;
    int old_nr_hugepages = -1;

    TEST_START;
    TEST_GET_PCO(pco_iut);
    TEST_GET_INT_PARAM(threads_num);
    TEST_GET_INT_PARAM(nr_hugepages);

    TEST_STEP("Set /proc/sys/vm/nr_hugepages to @p nr_hugepages.");
    CHECK_RC(tapi_cfg_sys_set_int(pco_iut->ta, nr_hugepages,
                                  &old_nr_hugepages,
                                  "vm/nr_hugepages"));

    TEST_STEP("Fork @b pco_iut_aux from @p pco_iut.");
    CHECK_RC(rcf_rpc_server_fork(pco_iut, "pco_iut_aux", &pco_iut_aux));
    killed_pid = rpc_getpid(pco_iut_aux);

    TEST_STEP("Initialize Zetaferno library for @b pco_iut_aux, allocate "
              "Zetaferno attributes object.");
    rpc_zf_init(pco_iut_aux);
    rpc_zf_attr_alloc(pco_iut_aux, &attr);

    TEST_STEP("Call rpc_zf_many_threads_alloc_free_stack() on "
              "@b pco_iut_aux with @c RCF_RPC_CALL; it should "
              "create @p threads_num threads, each allocating "
              "and freeing ZF stack in an infinite loop.");
    pco_iut_aux->op = RCF_RPC_CALL;
    rpc_zf_many_threads_alloc_free_stack(pco_iut_aux, attr, threads_num,
                                         WAIT_AFTER_ALLOC);

    TEST_STEP("Wait for a while and check that "
              "rpc_zf_many_threads_alloc_free_stack() still hangs.");
    MSLEEP(WAIT_BEFORE_KILL);
    CHECK_RC(rcf_rpc_server_is_op_done(pco_iut_aux, &done));
    if (done)
    {
        RPC_AWAIT_ERROR(pco_iut_aux);
        rc = rpc_zf_many_threads_alloc_free_stack(pco_iut_aux, attr,
                                                  threads_num,
                                                  WAIT_AFTER_ALLOC);

        if (rc == 0)
        {
            TEST_VERDICT("rpc_zf_many_threads_alloc_free_stack() succeeded "
                         "unexpectedly");
        }
        else
        {
            TEST_VERDICT("rpc_zf_many_threads_alloc_free_stack() failed "
                         "unexpectedly with error " RPC_ERROR_FMT,
                         RPC_ERROR_ARGS(pco_iut_aux));
        }
    }

    TEST_STEP("From @p pco_iut send @c SIGKILL signal to @b pco_iut_aux.");
    rpc_kill(pco_iut, killed_pid, RPC_SIGKILL);

    TEST_STEP("Wait for a while and check, that @b pco_iut is still "
              "accessible.");
    MSLEEP(WAIT_AFTER_KILL);
    if (!rcf_rpc_server_is_alive(pco_iut))
    {
        TEST_VERDICT("RPC server on IUT is no longer accessible after "
                     "killing its child");
    }

    TEST_STEP("Check with waitpid() that @b pco_iut_aux was indeed killed "
              "with @c SIGKILL, not terminated for another reason.");
    RPC_AWAIT_ERROR(pco_iut);
    rc = rpc_waitpid(pco_iut, killed_pid, &wait_status, 0);
    if (rc < 0)
    {
        TEST_VERDICT("waitpid() failed unexpectedly with error "
                     RPC_ERROR_FMT, RPC_ERROR_ARGS(pco_iut));
    }
    else if (wait_status.flag != RPC_WAIT_STATUS_SIGNALED)
    {
        TEST_VERDICT("Child process terminated with unexpected status %s",
                     wait_status_flag_rpc2str(wait_status.flag));
    }
    else if (wait_status.value != RPC_SIGKILL)
    {
        TEST_VERDICT("Child process was killed by unexpected signal %s",
                     signum_rpc2str(wait_status.value));
    }

    TEST_SUCCESS;

cleanup:

    if (pco_iut_aux != NULL)
        CLEANUP_CHECK_RC(rcf_rpc_server_destroy(pco_iut_aux));

    if (old_nr_hugepages >= 0)
    {
        CLEANUP_CHECK_RC(tapi_cfg_sys_set_int(pco_iut->ta, old_nr_hugepages,
                                              NULL, "vm/nr_hugepages"));
    }

    TEST_END;
}
