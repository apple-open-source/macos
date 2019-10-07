#include <darwintest.h>
#include <sys/types.h>
#include <pthread.h>
#include <mach/mach_types.h>
#include <unistd.h>
#include <stdlib.h>
#include <dispatch/dispatch.h>

static void *
body(void *corrupt)
{
	T_LOG("Helper thread running: %d", (bool)corrupt);
	if (corrupt) {
		// The pthread_t is stored at the top of the stack and could be
		// corrupted because of a stack overflow. To make the test more
		// reliable, we will manually smash the pthread struct directly.
		pthread_t self = pthread_self();
		memset(self, 0x41, 4096);
	}
	// Expected behavior is that if a thread calls abort, the process should
	// abort promptly.
	abort();
	T_FAIL("Abort didn't?");
}

typedef enum { PTHREAD, WORKQUEUE } thread_type_t;

static void
abort_test(thread_type_t type, int expected_signal)
{
	pid_t child = fork();
	bool corrupt = expected_signal == SIGSEGV;

	if (child == 0) {
		T_LOG("Child running");
		switch (type) {
		case PTHREAD: {
			pthread_t tid;
			T_QUIET;
			T_ASSERT_POSIX_ZERO(
					pthread_create(&tid, NULL, body, (void *)corrupt), NULL);
			break;
		}
		case WORKQUEUE: {
			dispatch_async_f(dispatch_get_global_queue(
									 DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
					(void *)corrupt, &body);
			break;
		}
		}
		sleep(5);
		T_FAIL("Child didn't abort");
		exit(-1);
	} else {
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
		if (signal == SIGBUS) {
			// rdar://53269061
			T_LOG("Converting %d to SIGSEGV", signal);
			signal = SIGSEGV;
		}
		T_EXPECT_EQ(signal, expected_signal, NULL);
	}
}

static void
signal_handler(int signo)
{
	// The user's signal handler should not be called during abort
	T_FAIL("Unexpected signal: %d\n", signo);
}

T_DECL(abort_pthread_corrupt_test, "Tests abort")
{
	abort_test(PTHREAD, SIGSEGV);
}

T_DECL(abort_workqueue_corrupt_test, "Tests abort")
{
	abort_test(WORKQUEUE, SIGSEGV);
}

T_DECL(abort_pthread_handler_test, "Tests abort")
{
	// rdar://52892057
	T_SKIP("Abort hangs if the user registers their own SIGSEGV handler");
	signal(SIGSEGV, signal_handler);
	abort_test(PTHREAD, SIGSEGV);
}

T_DECL(abort_workqueue_handler_test, "Tests abort")
{
	// rdar://52892057
	T_SKIP("Abort hangs if the user registers their own SIGSEGV handler");
	signal(SIGSEGV, signal_handler);
	abort_test(WORKQUEUE, SIGSEGV);
}

T_DECL(abort_pthread_test, "Tests abort")
{
	abort_test(PTHREAD, SIGABRT);
}

T_DECL(abort_workqueue_test, "Tests abort")
{
	abort_test(WORKQUEUE, SIGABRT);
}
