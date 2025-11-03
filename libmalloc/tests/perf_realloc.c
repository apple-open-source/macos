#include <stdlib.h>
#include <../src/internal.h>
#include <darwintest.h>

#if !MALLOC_TARGET_EXCLAVES
#include <mach/vm_page_size.h>
#include <perfcheck_keys.h>
#else
#include <time.h>
#endif // !MALLOC_TARGET_EXCLAVES

#pragma mark -
#pragma mark Helper code

#define PAGE_SPREAD 8
#define MAX_NAME_SIZE 256
#define OUTLIER_TIME 20000

// This value is a guess that will be refined over time.
#define PERFCHECK_THRESHOLD_PCT	10.0

static char name[MAX_NAME_SIZE];
#define NUMBER_OF_SAMPLES_FOR_BATCH(s) _dt_stat_batch_size((dt_stat_t)s)

// Measures the time required to realloc a block of memory of given size
// by a specified amount.
static void
realloc_by_amount(const char *base_metric_name, size_t size, ssize_t amount,
		bool amount_is_random)
{
#if !MALLOC_TARGET_EXCLAVES
	if (amount_is_random) {
		snprintf(name, MAX_NAME_SIZE, amount >= 0 ?
			 "%s_from_%llu_up_random" : "%s_from_%llu_down_random",
			 base_metric_name, (uint64_t)size);
	} else {
		snprintf(name, MAX_NAME_SIZE, amount >= 0 ?
				 "%s_from_%llu_up_%ld" : "%s_from_%llu_down_%ld",
				 base_metric_name, (uint64_t)size, labs(amount));
	}
#endif // !MALLOC_TARGET_EXCLAVES

	uint64_t total_time = 0;
	int count = 0;

#if !MALLOC_TARGET_EXCLAVES
	dt_stat_time_t s = dt_stat_time_create(name);
	dt_stat_set_variable((dt_stat_t)s, "size (bytes)", (unsigned int)size);
	dt_stat_set_variable((dt_stat_t)s, "amount (bytes)", (int)amount);
	dt_stat_token now = dt_stat_time_begin(s);
#endif // !MALLOC_TARGET_EXCLAVES

	for (;;) {
		void *ptr = malloc(size);
		T_QUIET; T_ASSERT_NOTNULL(ptr, "malloc for size %llu failed", (uint64_t)size);

#if MALLOC_TARGET_EXCLAVES
		struct timespec start, end;
		int rc_start = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
		ptr = realloc(ptr, size + amount);
		int rc_end = clock_gettime(CLOCK_MONOTONIC_RAW, &end);

		const uint64_t _NSEC_PER_SEC = 1000000000ull;
		uint64_t start_nsec = start.tv_sec * _NSEC_PER_SEC + start.tv_nsec;
		uint64_t end_nsec = end.tv_sec * _NSEC_PER_SEC + end.tv_nsec;
		total_time += end_nsec - start_nsec;
#else
		dt_stat_token start = dt_stat_time_begin(s);
		ptr = realloc(ptr, size + amount);
		dt_stat_token end = dt_stat_time_begin(s);
		total_time += end - start;
#endif // MALLOC_TARGET_EXCLAVES

		T_QUIET; T_ASSERT_NOTNULL(ptr,
				"realloc from size %llu to size %llu failed", (uint64_t)size,
				(uint64_t)(size + amount));
		free(ptr);

#if MALLOC_TARGET_EXCLAVES
		// On exclaves, run this test for the very arbitrary 10ms, or at
		// least 5 loops
		if (++count >= 5 && total_time >= (_NSEC_PER_SEC / 100)) {
			T_LOG("Ran realloc test for %d iterations, average time %llu",
					count, total_time / count);
			break;
		}
#else
		if (++count >= NUMBER_OF_SAMPLES_FOR_BATCH(s)) {
			// Discard outliers or the test won't converge -- this should
			// be done in libdarwintest
			if (total_time/count < OUTLIER_TIME) {
				dt_stat_mach_time_add_batch(s, count, total_time);
			}
			total_time = 0;
			count = 0;
			if (dt_stat_stable(s)) {
				break;
			}
		}
#endif // MALLOC_TARGET_EXCLAVES
	}

#if !MALLOC_TARGET_EXCLAVES
	dt_stat_finalize(s);
#endif // !MALLOC_TARGET_EXCLAVES
}

// Times a set of size adjustments from a given base.
static void
realloc_test_set(const char *base_name, size_t start_size)
{
	ssize_t adj = 1;
	realloc_by_amount(base_name, start_size, adj, false);
	realloc_by_amount(base_name, start_size + adj, -adj, false);

	adj = 8;
	realloc_by_amount(base_name, start_size, adj, false);
	realloc_by_amount(base_name, start_size + adj, -adj, false);

	adj = 16;
	realloc_by_amount(base_name, start_size, adj, false);
	realloc_by_amount(base_name, start_size + adj, -adj, false);

#if INCLUDE_RANDOM_SIZE_TESTS
	adj = 1 + arc4random_uniform(vm_page_size - 1);
	realloc_by_amount(base_name, start_size, adj, true);
	realloc_by_amount(base_name, start_size + adj, -adj, true);

	uint32_t pages = 1 + arc4random_uniform(PAGE_SPREAD);
	adj = pages * vm_page_size;
	realloc_by_amount(base_name, start_size, adj, true);
	realloc_by_amount(base_name, start_size + adj, -adj, true);
#else // INCLUDE_RANDOM_SIZE_TESTS
	T_LOG("Skipping random size tests");
#endif // INCLUDE_RANDOM_SIZE_TESTS

	adj = vm_page_size;
	realloc_by_amount(base_name, start_size, adj, false);
	realloc_by_amount(base_name, start_size + adj, -adj, false);

	adj = 4 * vm_page_size;
	realloc_by_amount(base_name, start_size, adj, false);
	realloc_by_amount(base_name, start_size + adj, -adj, false);

	// Adjust up and then down by over half the size of the allocation --
	// this skips the fast path in some allocators.
	adj = 4 * vm_page_size;
	realloc_by_amount(base_name, start_size, adj, false);
	realloc_by_amount(base_name, start_size + adj, -(((ssize_t)start_size) + adj)/2 - 1, false);

	adj = 32;
	realloc_by_amount(base_name, start_size, adj, false);
	realloc_by_amount(base_name, start_size + adj, -(((ssize_t)start_size) + adj)/2 - 1, false);
}

static void
realloc_tests(const char *base_name, boolean_t using_nano)
{
	// tiny or nano
	realloc_test_set(using_nano ? base_name : "Tiny", 8);

	if (!using_nano) {
		// No point in running these tests three times.

		// tiny
		realloc_test_set("Tiny", 512);

		// small
		realloc_test_set("Small", 2048);

		// large
		realloc_test_set("Large", 128 * 1024);
	}

	T_END;
}

#pragma mark -
#pragma mark Tests for realloc()

T_DECL(realloc_perf_base, "realloc without nano",
	   T_META_TAG_PERF, T_META_TAG_ALL_ALLOCATORS, T_META_TAG_VM_NOT_PREFERRED,
	   T_META_ENVVAR("MallocNanoZone=0"))
{
	realloc_tests("NoNano", false);
}

#if !MALLOC_TARGET_EXCLAVES
T_DECL(realloc_perf_nanov2, "realloc with nanoV2",
	   T_META_TAG_PERF, T_META_TAG_ALL_ALLOCATORS, T_META_TAG_VM_NOT_PREFERRED,
	   T_META_ENVVAR("MallocNanoZone=V2"))
{
#if CONFIG_NANOZONE
	realloc_tests("Nanov2", true);
#else
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}
#endif // !MALLOC_TARGET_EXCLAVES
