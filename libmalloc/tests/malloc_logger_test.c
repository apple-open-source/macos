#include <darwintest.h>
#include <os/atomic_private.h>
#include <pthread.h>
#include <stack_logging.h> // Not available for exclaves
#include <stdlib.h>

T_GLOBAL_META(T_META_TAG_NO_ALLOCATOR_OVERRIDE);

static void
my_malloc_logger(uint32_t type,
		uintptr_t arg1,
		uintptr_t arg2,
		uintptr_t arg3,
		uintptr_t result,
		uint32_t num_hot_frames_to_skip)
{
	// Empty logger
}

static double
get_time(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	return now.tv_sec + (now.tv_nsec / 1e9);
}

static void *
do_allocations_thread(void *arg)
{
	bool *done = arg;
	while (!os_atomic_load(done, relaxed)) {
		volatile uint8_t *ptr = malloc(16);
		*ptr = 7;
		free((void *)ptr);
	}
	return NULL;
}

/// This tests creates the condition where the `malloc_logger` is set to NULL
/// between the check and the call in this code pattern (TOCTOU).  It reliably
/// triggers a crash without the fix.
/// ```
///   if (malloc_logger)
///     malloc_logger(...);
/// ```
T_DECL(malloc_logger_racy_unregister,
		"Test for malloc_logger time-of-check to time-of-use race",
		T_META_TAG_VM_NOT_PREFERRED)
{
	const size_t num_threads = 4;
	const double run_time_seconds = 1.0;
	struct timespec sleep_time = {0, 1000}; // 1 microsecond (1,000 nanoseconds)

	bool done = false;
	pthread_t threads[num_threads];

	T_LOG("Creating allocation threads");
	for (size_t i = 0; i < num_threads; i++) {
		int rc = pthread_create(&threads[i], NULL, do_allocations_thread, &done);
		T_QUIET; T_ASSERT_POSIX_ZERO(rc, "pthread_create");
	}

	T_LOG("Setting and unsetting malloc_logger for %.2f seconds", run_time_seconds);
	double end_time = get_time() + run_time_seconds;
	do {
		malloc_logger = my_malloc_logger;
		nanosleep(&sleep_time, NULL);
		malloc_logger = NULL;
	} while (get_time() < end_time);

	T_LOG("Waiting for allocation threads to finish");
	os_atomic_store(&done, true, relaxed);
	for (size_t i = 0; i < num_threads; i++) {
		int rc = pthread_join(threads[i], NULL);
		T_QUIET; T_ASSERT_POSIX_ZERO(rc, "pthread_join");
	}

	T_PASS("Survived racy malloc_logger set to NULL");
}
