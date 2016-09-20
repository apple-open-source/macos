#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <dispatch/dispatch.h>
#include <sys/mman.h>

#include <darwintest.h>

#define T_LOG_VERBOSE(...)

#ifdef __LP64__
#define STACK_LOCATIONS 16
#else
#define STACK_LOCATIONS 8
#endif

static void*
thread_routine(void *loc)
{
	int foo;
	*(uintptr_t*)loc = (uintptr_t)&foo;
	return NULL;
}

static int
pointer_compare(const void *ap, const void *bp)
{
	uintptr_t a = *(const uintptr_t*)ap;
	uintptr_t b = *(const uintptr_t*)bp;
	return a > b ? 1 : a < b ? -1 : 0;
}

static void
test_stack_aslr(bool workqueue_thread)
{
	const int attempts = 128;
	int attempt_round = 0;

	uintptr_t *addr_array = mmap(NULL, sizeof(uintptr_t) * attempts,
			PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);
	T_QUIET; T_ASSERT_NOTNULL(addr_array, NULL);

again:
	bzero(addr_array, sizeof(uintptr_t) * attempts);

	for (int i = 0; i < attempts; i++) {
		pid_t pid = fork();
		T_QUIET; T_ASSERT_POSIX_SUCCESS(pid, "[%d] fork()", i);

		if (pid) { // parent
			pid = waitpid(pid, NULL, 0);
			T_QUIET; T_ASSERT_POSIX_SUCCESS(pid, "[%d] waitpid()", i);
		} else if (workqueue_thread) { // child
			dispatch_async(dispatch_get_global_queue(0,0), ^{
				int foo;
				addr_array[i] = (uintptr_t)&foo;
				exit(0);
			});
			while (true) sleep(1);
		} else { // child
			pthread_t th;
			int ret = pthread_create(&th, NULL, thread_routine, &addr_array[i]);
			assert(ret == 0);
			ret = pthread_join(th, NULL);
			assert(ret == 0);
			exit(0);
		}
	}

	qsort(addr_array, attempts, sizeof(uintptr_t), pointer_compare);

	T_LOG("Stack address range: %p - %p (+%lx)", (void*)addr_array[0], (void*)addr_array[attempts-1],
			addr_array[attempts-1] - addr_array[0]);

	int unique_values = 0;
	T_LOG_VERBOSE("[%p]", (void*)addr_array[0]);
	for (int i = 1; i < attempts; i++) {
		T_LOG_VERBOSE("[%p]", (void*)addr_array[i]);
		if (addr_array[i-1] != addr_array[i]) {
			unique_values++;
		}
	}

	if (attempt_round < 3) T_MAYFAIL;
	T_EXPECT_GE(unique_values, STACK_LOCATIONS, "Should have more than %d unique stack locations", STACK_LOCATIONS);
	if (attempt_round++ < 3 && unique_values < STACK_LOCATIONS) goto again;
}

T_DECL(pthread_stack_aslr, "Confirm that stacks are ASLRed", T_META_CHECK_LEAKS(NO),
		T_META_ALL_VALID_ARCHS(YES))
{
	test_stack_aslr(false);
}

T_DECL(wq_stack_aslr, "Confirm that workqueue stacks are ASLRed", T_META_CHECK_LEAKS(NO),
		T_META_ALL_VALID_ARCHS(YES))
{
	test_stack_aslr(true);
}
