//
//  introspect_tests.c
//  libmalloc
//
//  Tests malloc introspection features
//

#include <TargetConditionals.h>
#include <darwintest.h>
#include <darwintest_utils.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <malloc/malloc.h>
#include <../private/malloc_private.h>
#include <../src/internal.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_PREFERRED);

extern int32_t malloc_num_zones;
extern malloc_zone_t **malloc_zones;

void check_stats(malloc_zone_t *zone, malloc_statistics_t *stats)
{
	malloc_statistics_t stats2 = {};
	zone->introspect->statistics(zone, &stats2);
	T_QUIET; T_EXPECT_EQ(stats->blocks_in_use, stats2.blocks_in_use, "blocks_in_use matches");
	T_QUIET; T_EXPECT_EQ(stats->size_in_use, stats2.size_in_use, "size_in_use matches");
	T_QUIET; T_EXPECT_EQ(stats->blocks_in_use, stats2.blocks_in_use, "blocks_in_use matches");
	T_QUIET; T_EXPECT_EQ(stats->size_allocated, stats2.size_allocated, "size_allocated matches");
}


void test_stats(int limit)
{
	malloc_statistics_t stats = {};
	malloc_statistics_t old_stats = {};

	malloc_zone_t *zone = malloc_zones[0];

	T_ASSERT_GE(zone->version, 12, "zone version is at least 12");

	zone->introspect->task_statistics(mach_task_self(), (vm_address_t)zone, NULL, &old_stats);
	check_stats(zone, &old_stats);

	T_LOG("testing statistics for allocations from 1 to %d", limit);

	for (int i = 0; i < limit; i++) {
		void *ptr = malloc(i);
		T_QUIET; T_ASSERT_NOTNULL(ptr, "ptr isn't null");
		size_t size = malloc_size(ptr);
		zone->introspect->task_statistics(mach_task_self(), (vm_address_t)zone, NULL, &stats);
		check_stats(zone, &stats);
		if (stats.blocks_in_use == old_stats.blocks_in_use) {
			T_LOG("last-free cache hit at size %d", i);
			/* It's possible we hit the last-free cache.   Try one more allocation. */
			ptr = malloc(i);
			T_QUIET; T_ASSERT_NOTNULL(ptr, "ptr isn't null");
			size = malloc_size(ptr);
			zone->introspect->task_statistics(mach_task_self(), (vm_address_t)zone, NULL, &stats);
			check_stats(zone, &stats);
		}
		T_QUIET; T_EXPECT_EQ(stats.size_in_use, old_stats.size_in_use+size, "%ld more bytes in use", (long)size);
		T_QUIET; T_EXPECT_EQ(stats.blocks_in_use, old_stats.blocks_in_use+1, "one more block in use");
		old_stats = stats;
	}

	T_LOG("done");
}

T_DECL(nano_statistics_test, "Test that we can introspect nano zone statistics",
		T_META_ENVVAR("MallocNanoZone=V2"), T_META_ENVVAR("MallocProbGuard=0"),
		T_META_CHECK_LEAKS(false), T_META_TAG_ALL_ALLOCATORS)
{
#if CONFIG_NANOZONE
	(void)malloc(16);
	T_ASSERT_EQ(malloc_engaged_nano(), 2, "Nanozone v2 engaged");
	test_stats(256);
#else // CONFIG_NANOZONE
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}


T_DECL(szone_statistics_test, "Test that we can introspect szone zone statistics",
		T_META_TAG_ALL_ALLOCATORS, T_META_ENVVAR("MallocNanoZone=0"),
		T_META_ENVVAR("MallocProbGuard=0"), T_META_CHECK_LEAKS(false))
{
	(void)malloc(16);
	T_ASSERT_EQ(malloc_engaged_nano(), 0, "Nanozone not engaged");
	test_stats(5000);
}


void check_zones(vm_address_t *zones, unsigned zone_count)
{
	T_EXPECT_EQ(zone_count, malloc_num_zones, "zone count");
	for (unsigned i = 0; i < zone_count; i++) {
		T_EXPECT_EQ(zones[i], (vm_address_t)malloc_zones[i], "malloc_num_zones[%d]", i);
	}
}

T_DECL(malloc_get_all_zones,
		"Test that we can retrieve the zones of our own process with a NULL reader",
		T_META_TAG_ALL_ALLOCATORS)
{
	vm_address_t *zones;
	unsigned zone_count;
	kern_return_t kr;

	kr = malloc_get_all_zones(mach_task_self(), /*reader=*/NULL, &zones, &zone_count);
	T_QUIET; T_ASSERT_EQ(kr, KERN_SUCCESS, "malloc_get_all_zones(mach_task_self(), ...)");
	check_zones(zones, zone_count);

	kr = malloc_get_all_zones(TASK_NULL, /*reader=*/NULL, &zones, &zone_count);
	T_QUIET; T_ASSERT_EQ(kr, KERN_SUCCESS, "malloc_get_all_zones(TASK_NULL, ...)");
	check_zones(zones, zone_count);
}
