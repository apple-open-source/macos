//
//  magazine_rack.c
//  libmalloc
//
//  Created by Matt Wright on 8/29/16.
//
//

#include <darwintest.h>
#include "magazine_testing.h"

// Size of allocation to fall into each rack
#define TINY_ALLOCATION_SZ 128
#define SMALL_ALLOCATION_SZ 1024
#define MEDIUM_ALLOCATION_SZ 65536

#define N_HELPER_THREADS 8

/*
 * Lower the size of the allocation pool on embedded devices to prevent test timeouts
 * / limit kills. These devices do not have a medium rack, so we should still fill
 * multiple regions for tiny/small (~1/8 MiB per region respectively).
 */
#if TARGET_OS_IPHONE
#define MAX_ALLOCATIONS_PER_THREAD (uint32_t)20000
#else
#define MAX_ALLOCATIONS_PER_THREAD (uint32_t)100000
#endif

static_assert((MAX_ALLOCATIONS_PER_THREAD * TINY_ALLOCATION_SZ) > (2 * TINY_REGION_SIZE), "");
static_assert((MAX_ALLOCATIONS_PER_THREAD * SMALL_ALLOCATION_SZ) > (2 * SMALL_REGION_SIZE), "");
#if CONFIG_MEDIUM_ALLOCATOR
static_assert((MAX_ALLOCATIONS_PER_THREAD * MEDIUM_ALLOCATION_SZ) > (2 * MEDIUM_REGION_SIZE), "");
#endif

// Stubs
malloc_zero_policy_t malloc_zero_policy = MALLOC_ZERO_POLICY_DEFAULT;

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(false), T_META_TIMEOUT(1200));

T_DECL(basic_magazine_init, "allocate magazine counts")
{
	struct rack_s rack;

	for (int i=1; i < 64; i++) {
		memset(&rack, 'a', sizeof(rack));
		rack_init(&rack, RACK_TYPE_NONE, i, 0);
		T_ASSERT_NOTNULL(rack.magazines, "%d magazine initialisation", i);
	}
}

T_DECL(basic_magazine_deinit, "allocate deallocate magazines")
{
	struct rack_s rack;
	memset(&rack, 'a', sizeof(rack));

	rack_init(&rack, RACK_TYPE_NONE, 1, 0);
	T_ASSERT_NOTNULL(rack.magazines, "magazine init");

	rack_destroy(&rack);
	T_ASSERT_NULL(rack.magazines, "magazine deinit");
}

void *
pressure_thread(void *arg)
{
	T_LOG("pressure thread started\n");
	while (1) {
		malloc_zone_pressure_relief(0, 0);
	}
}

void *
thread(void *arg)
{
	uintptr_t sz = (uintptr_t)arg;
	uint32_t max_allocations = MAX_ALLOCATIONS_PER_THREAD;

	T_LOG("thread started (allocation size: %lu bytes)\n", sz);
	void *temp = malloc(sz);

	uint64_t c = 100;
	while (c-- > 0) {
		uint32_t num = arc4random_uniform(max_allocations);
		void **allocs = malloc(sizeof(void *) * num);
		T_QUIET; T_ASSERT_NOTNULL(allocs, "Failed to allocate initial array with size %lu\n", num * sizeof(void *));

		for (int i=0; i<num; i++) {
			allocs[i] = malloc(sz);
			T_QUIET; T_ASSERT_NOTNULL(allocs[i], "Failed to make allocation with size %lu\n", sz);
		}
		for (int i=0; i<num; i++) {
			free(allocs[num - 1 - i]);
		}
		free((void *)allocs);
	}
	free(temp);
	return NULL;
}

void
test_region_remove(size_t allocation_size)
{
	T_LOG("Will allocate up to %u B per thread over %d threads\n", MAX_ALLOCATIONS_PER_THREAD, N_HELPER_THREADS);
	pthread_t p1;
	pthread_create(&p1, NULL, pressure_thread, NULL);

	pthread_t p[N_HELPER_THREADS];

	for (int i=0; i<N_HELPER_THREADS; i++) {
		pthread_create(&p[i], NULL, thread, (void *) allocation_size);
	}
	for (int i=0; i<N_HELPER_THREADS; i++) {
		pthread_join(p[i], NULL);
	}
}

T_DECL(rack_tiny_region_remove, "exercise region deallocation race (rdar://66713029)")
{
	test_region_remove(TINY_ALLOCATION_SZ);
	T_PASS("finished without crashing");
}

T_DECL(rack_small_region_remove, "exercise region deallocation race (rdar://66713029)")
{
	test_region_remove(SMALL_ALLOCATION_SZ);
	T_PASS("finished without crashing");
}

T_DECL(rack_medium_region_remove, "exercise region deallocation race (rdar://66713029)",
	   T_META_ENVVAR("MallocMediumZone=1"),
	   T_META_ENVVAR("MallocMediumActivationThreshold=1"),
	   T_META_ENABLED(CONFIG_MEDIUM_ALLOCATOR))
{
	test_region_remove(MEDIUM_ALLOCATION_SZ);
	T_PASS("finished without crashing");
}
