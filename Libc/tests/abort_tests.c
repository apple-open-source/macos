#include <darwintest.h>
#include <sys/types.h>
#include <pthread.h>
#include <mach/mach_types.h>
#include <unistd.h>
#include <stdlib.h>
#include <dispatch/dispatch.h>

#include <platform/string.h>

typedef enum { PTHREAD, WORKQUEUE } thread_type_t;

typedef enum {
	NO_CORRUPTION = 0x0,
	SIG_CORRUPTION = 0x1, // the pthread_t::sig
	FULL_CORRUPTION = 0x2, // the pthread_t::sig and also TSDs
	STACK_CORRUPTION = 0x4, // the stack protector cookie
} corrupt_type_t;

__attribute__((noinline))
static void
induce_stack_check_failure(void)
{
	char buf[20];
	// _platform_memset() sidesteps the -fstack-check rewrite of regular
	// memset() to __memset_chk(), which doesn't take the abort path we want to
	// exercise
	_platform_memset(buf, 'a', 28);
}

static void
body(void *ctx)
{
	corrupt_type_t corrupt_type = (corrupt_type_t)ctx;
	pthread_t self = pthread_self();

	T_LOG("Helper thread running: %d", corrupt_type);

	// The pthread_t is stored at the top of the stack and could be
	// corrupted because of a stack overflow. To make the test more
	// reliable, we will manually smash the pthread struct directly.
	if (corrupt_type & FULL_CORRUPTION) {
		memset(self, 0x41, 4096);
	} else if (corrupt_type & SIG_CORRUPTION) {
		memset(self, 0x41, 128);
	}

	if (corrupt_type & STACK_CORRUPTION) {
		induce_stack_check_failure();
		T_ASSERT_FAIL("Should have aborted");
	} else {
		// Expected behavior is that if a thread calls abort, the process should
		// abort promptly.
		abort();
		T_FAIL("Abort didn't?");
	}
}

static void *
body_thr(void *ctx)
{
	body(ctx);
	/* UNREACHABLE */
	return (NULL);
}

static void
abort_test(thread_type_t type, corrupt_type_t corrupt_type)
{
	pid_t child = fork();

	if (child == 0) {
		T_LOG("Child running");
		switch (type) {
		case PTHREAD: {
			pthread_t tid;
			T_QUIET;
			T_ASSERT_POSIX_ZERO(
					pthread_create(&tid, NULL, body_thr, (void *)corrupt_type), NULL);
			break;
		}
		case WORKQUEUE: {
			dispatch_async_f(dispatch_get_global_queue(
									 DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
					(void *)corrupt_type, body);
			break;
		}
		}
		sleep(5);
		T_FAIL("Child didn't abort");
		exit(-1);
	}

	// Wait and check the exit status of the child
	int status = 0;
	pid_t pid = wait(&status);
	T_QUIET;
	T_ASSERT_EQ(pid, child, NULL);
	T_QUIET;
	T_EXPECT_FALSE(WIFEXITED(status), "WIFEXITED Status: %x", status);
	T_QUIET;
	T_EXPECT_TRUE(WIFSIGNALED(status), "WIFSIGNALED Status: %x", status);
	T_QUIET;
	T_EXPECT_FALSE(WIFSTOPPED(status), "WIFSTOPPED Status: %x", status);

	// This test is successful if we trigger a SIGSEGV|SIGBUS or SIGABRT
	// since both will promptly terminate the program
	int signal = WTERMSIG(status);

#if defined(__i386__) || defined(__x86_64__)
	// on intel pthread_self() reads a TSD so FULL corruption results
	// in SIGSEGV/SIGBUS
	if (corrupt_type == FULL_CORRUPTION) {
		// any of these signals may happen depending on which libpthread
		// you're running on.
		if (signal == SIGBUS) {
			T_LOG("Converting %d to SIGSEGV", signal);
			signal = SIGSEGV;
		}
		T_EXPECT_EQ(signal, SIGSEGV, NULL);
		T_END;
	}
#endif
#if defined(__arm64e__)
	// on arm64e pthread_self() checks a ptrauth signature so it is
	// likely to die of either SIGTRAP or SIGBUS.
	//
	// On FPAC hardware with certain configurations, PAC exceptions
	// are fatal, which means the delivered signal is SIGKILL.
	if ((corrupt_type & (FULL_CORRUPTION | SIG_CORRUPTION))) {
		switch(signal) {
		case SIGBUS:
			T_EXPECT_EQ(signal, SIGBUS, NULL);
			break;
		case SIGTRAP:
			T_EXPECT_EQ(signal, SIGTRAP, NULL);
			break;
		default:
			// If the delivered signal is not SIGTRAP or SIGBUS,
			// it has to be because the exception is fatal,
			// which means the signal is SIGKILL.
			T_EXPECT_EQ(signal, SIGKILL, NULL);
			break;
		}
		T_END;
	}
#endif

	/* pthread calls abort_with_reason if only the signature is corrupt */
	T_EXPECT_EQ(signal, SIGABRT, NULL);
}

static void
signal_handler(int signo)
{
	// The user's signal handler should not be called during abort
	T_FAIL("Unexpected signal: %d\n", signo);
}

T_DECL(abort_pthread_corrupt_test_full, "Tests abort", T_META_IGNORECRASHES(".*abort_tests.*"))
{
	abort_test(PTHREAD, FULL_CORRUPTION);
}

T_DECL(abort_workqueue_corrupt_test_full, "Tests abort", T_META_IGNORECRASHES(".*abort_tests.*"))
{
	abort_test(WORKQUEUE, FULL_CORRUPTION);
}

T_DECL(abort_pthread_handler_test_full, "Tests abort", T_META_IGNORECRASHES(".*abort_tests.*"))
{
	// rdar://52892057
	T_SKIP("Abort hangs if the user registers their own SIGSEGV handler");
	signal(SIGSEGV, signal_handler);
	abort_test(PTHREAD, FULL_CORRUPTION);
}

T_DECL(abort_workqueue_handler_test_full, "Tests abort", T_META_IGNORECRASHES(".*abort_tests.*"))
{
	// rdar://52892057
	T_SKIP("Abort hangs if the user registers their own SIGSEGV handler");
	signal(SIGSEGV, signal_handler);
	abort_test(WORKQUEUE, FULL_CORRUPTION);
}

T_DECL(abort_pthread_corrupt_test_sig, "Tests abort", T_META_IGNORECRASHES(".*abort_tests.*"))
{
	abort_test(PTHREAD, SIG_CORRUPTION);
}

T_DECL(abort_workqueue_corrupt_test_sig, "Tests abort", T_META_IGNORECRASHES(".*abort_tests.*"))
{
	abort_test(WORKQUEUE, SIG_CORRUPTION);
}

T_DECL(abort_pthread_test, "Tests abort", T_META_IGNORECRASHES(".*abort_tests.*"))
{
	abort_test(PTHREAD, NO_CORRUPTION);
}

T_DECL(abort_workqueue_test, "Tests abort",
		T_META_IGNORECRASHES(".*abort_tests.*"))
{
	abort_test(WORKQUEUE, NO_CORRUPTION);
}

T_DECL(abort_pthread_stack_check_test, "Tests __stack_chk_fail() abort",
		T_META_IGNORECRASHES(".*abort_tests.*"))
{
	abort_test(PTHREAD, STACK_CORRUPTION);
}

T_DECL(abort_pthread_stack_check_corrupt_sig_test,
		"Tests __stack_chk_fail() abort with a corrupt pthread_t",
		T_META_IGNORECRASHES(".*abort_tests.*"))
{
	abort_test(PTHREAD, (corrupt_type_t)(STACK_CORRUPTION | SIG_CORRUPTION));
}
