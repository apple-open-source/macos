#include <darwintest.h>
#include <darwintest_perf.h>

#include "pthread_jit_write_test_inline.h"

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

static void *
expect_write_fail_thread(__unused void * arg)
{
	fault_state_create();

	if (does_access_fault(ACCESS_WRITE, rwx_addr)) {
		pthread_exit((void *)0);
	} else {
		pthread_exit((void *)1);
	}
}

T_DECL(pthread_jit_write_protect,
    "Verify that the pthread_jit_write_protect interfaces work correctly")
{
	void * join_value = NULL;
	pthread_t pthread;
	bool expect_fault = pthread_jit_write_protect_supported_np();

	pthread_jit_test_setup();

	/*
	 * Validate that we fault when we should, and that we do not fault when
	 * we should not fault.
	 */
	pthread_jit_write_protect_np(FALSE);

	T_EXPECT_EQ(does_access_fault(ACCESS_WRITE, rwx_addr), 0, "Write with RWX->RW");

	pthread_jit_write_protect_np(TRUE);

	T_EXPECT_EQ(does_access_fault(ACCESS_EXECUTE, rwx_addr), 0, "Execute with RWX->RX");

	pthread_jit_write_protect_np(TRUE);

	T_EXPECT_EQ(does_access_fault(ACCESS_WRITE, rwx_addr), expect_fault, "Write with RWX->RX");

	pthread_jit_write_protect_np(FALSE);

	T_EXPECT_EQ(does_access_fault(ACCESS_EXECUTE, rwx_addr), expect_fault, "Execute with RWX->RW");

	pthread_jit_write_protect_np(FALSE);

	if (expect_fault) {
		/*
		 * Create another thread for testing multithreading; mark this as setup
		 * as this test is not targeted towards the pthread create/join APIs.
		 */
		T_SETUPBEGIN;

		T_ASSERT_POSIX_ZERO(pthread_create(&pthread, NULL, expect_write_fail_thread, NULL), "pthread_create expect_write_fail_thread");

		T_ASSERT_POSIX_ZERO(pthread_join(pthread, &join_value), "pthread_join expect_write_fail_thread");

		T_SETUPEND;

		/*
		 * Validate that the other thread was unable to write to the JIT region
		 * without independently using the pthread_jit_write_protect code.
		 */
		T_ASSERT_NULL((join_value), "Write on other thread with RWX->RX, "
		    "RWX->RW on parent thread");
	}

	/* We're done with the test; tear down our extra state. */
	pthread_jit_test_teardown();
}

T_DECL(thread_self_restrict_rwx_perf,
    "Test the performance of the thread_self_restrict_rwx interfaces",
    T_META_TAG_PERF, T_META_CHECK_LEAKS(false))
{
	dt_stat_time_t dt_stat_time;

	dt_stat_time = dt_stat_time_create("rx->rw->rx time");

	T_STAT_MEASURE_LOOP(dt_stat_time) {
		pthread_jit_write_protect_np(FALSE);
		pthread_jit_write_protect_np(TRUE);
	}

	dt_stat_finalize(dt_stat_time);
}
