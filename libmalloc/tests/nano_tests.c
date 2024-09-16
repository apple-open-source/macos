//
//  nano_tests.c
//  libmalloc
//
//  Tests that are specific to the implementation details of Nanov2.
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

#if CONFIG_NANOZONE

#pragma mark -
#pragma mark Enumerator access

static int range_count;					// Total number of allocated ranges
static int ptr_count;					// Total number of allocated pointers
static size_t total_ranges_size;		// Size of all allocated ranges
static size_t total_in_use_ptr_size;	// Size of all allocated pointers

static void
range_recorder(task_t task, void *context, unsigned type, vm_range_t *ranges,
			   unsigned count)
{
	for (int i = 0; i < count; i++) {
		total_ranges_size += ranges[i].size;
	}
	range_count += count;
}

static void
pointer_recorder(task_t task, void *context, unsigned type, vm_range_t *ranges,
			 unsigned count)
{
	for (int i = 0; i < count; i++) {
		total_in_use_ptr_size += ranges[i].size;
	}
	ptr_count += count;
}

static kern_return_t
memory_reader(task_t remote_task, vm_address_t remote_address, vm_size_t size,
			  void **local_memory)
{
	if (local_memory) {
		*local_memory = (void*)remote_address;
		return KERN_SUCCESS;
	}
	return KERN_FAILURE;
}

static void
run_enumerator()
{
	total_ranges_size = 0;
	total_in_use_ptr_size = 0;
	range_count = 0;
	ptr_count = 0;
	malloc_zone_t *zone = malloc_default_zone();
	zone->introspect->enumerator(mach_task_self(), NULL,
			MALLOC_PTR_REGION_RANGE_TYPE, (vm_address_t)zone, memory_reader,
			range_recorder);
	zone->introspect->enumerator(mach_task_self(), NULL,
			MALLOC_PTR_IN_USE_RANGE_TYPE, (vm_address_t)zone, memory_reader,
			pointer_recorder);
}

#endif // CONFIG_NANOZONE

#pragma mark -
#pragma mark Enumerator tests

#if TARGET_OS_WATCH
#define ALLOCATION_COUNT 10000
#else // TARGET_OS_WATCH
#define ALLOCATION_COUNT 100000
#endif // TARGET_OS_WATCH

static void *allocations[ALLOCATION_COUNT];

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_NOT_PREFERRED);

T_DECL(nano_active_test, "Test that Nano is activated",
		T_META_ENVVAR("MallocNanoZone=1"), T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG_XZONE, T_META_TAG_NANO_ON_XZONE)
{
#if CONFIG_NANOZONE
	bool nano_on_xzone = getenv("MallocNanoOnXzone");
	if (nano_on_xzone) {
		T_ASSERT_NE(malloc_engaged_secure_allocator(), 0,
				"Secure allocator engaged");
	}

	T_ASSERT_NE(malloc_engaged_nano(), 0, "Nano mode engaged");

	if (nano_on_xzone || !malloc_engaged_secure_allocator()) {
		void *ptr = malloc(16);
		T_LOG("Nano ptr is %p\n", ptr);
		T_ASSERT_EQ(NANOZONE_SIGNATURE, (uint64_t)((uintptr_t)ptr) >> SHIFT_NANO_SIGNATURE,
				"Nanozone is active");
		free(ptr);
	}
#else // CONFIG_NANOZONE
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}

T_DECL(nano_enumerator_test, "Test the Nanov2 enumerator",
		T_META_ENVVAR("MallocNanoZone=V2"), T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG_XZONE)
{
#if CONFIG_NANOZONE
	if (malloc_engaged_secure_allocator()) {
		T_ASSERT_NE(malloc_engaged_nano(), 0,
				"Secure allocator nano mode engaged");
	} else {
		T_ASSERT_EQ(malloc_engaged_nano(), 2, "Nanozone V2 engaged");
	}

	// This test is problematic because the allocator is used before the test
	// starts, so we can't start everything from zero.
	// Grab a baseline
	malloc_statistics_t stats;
	malloc_zone_statistics(malloc_default_zone(), &stats);
	const unsigned int initial_blocks_in_use = stats.blocks_in_use;
	const size_t initial_size_in_use = stats.size_in_use;
	const size_t initial_size_allocated = stats.size_allocated;

	run_enumerator();
	const int initial_ptrs = ptr_count;
	const size_t initial_ranges_size = total_ranges_size;
	const size_t initial_in_use_ptr_size = total_in_use_ptr_size;

	// Allocate memory of random sizes, all less than the max Nano size.
	size_t total_requested_size = 0;
	for (int i = 0; i < ALLOCATION_COUNT; i++) {
		size_t sz = malloc_good_size(arc4random_uniform(257));
		allocations[i] = malloc(sz);
		total_requested_size += sz;
	}

	// Get the stats and enumerator values again and check whether the result is consistent.
	malloc_zone_statistics(malloc_default_zone(), &stats);
	run_enumerator();

	T_ASSERT_EQ(stats.blocks_in_use, initial_blocks_in_use + ALLOCATION_COUNT,
			"Incorrect blocks_in_use");
	T_ASSERT_EQ(stats.size_in_use, initial_size_in_use + total_requested_size,
			"Incorrect size_in_use");
	T_ASSERT_GE(stats.size_allocated, total_requested_size,
			"Size allocated must be >= size requested");

	T_ASSERT_EQ(ptr_count, initial_ptrs + ALLOCATION_COUNT,
			"Incorrect number of pointers");
	T_ASSERT_EQ(total_in_use_ptr_size, initial_in_use_ptr_size + total_requested_size,
			"Incorrect in-use pointer size");

	// Free half of the memory and recheck the statistics
	size_t size_freed = 0;
	for (int i = 0; i < ALLOCATION_COUNT / 2; i++) {
		size_freed += malloc_size(allocations[i]);
		free(allocations[i]);
	}

	// Check the stats and enumerator values.
	malloc_zone_statistics(malloc_default_zone(), &stats);
	run_enumerator();
	T_ASSERT_EQ(stats.blocks_in_use, initial_blocks_in_use + ALLOCATION_COUNT/2,
			"Incorrect blocks_in_use after half free");
	T_ASSERT_EQ(stats.size_in_use,
			initial_size_in_use + total_requested_size - size_freed,
			"Incorrect size_in_use after half free");
	T_ASSERT_GE(stats.size_allocated, initial_size_allocated,
			"Size allocated must be >= size requested");

	T_ASSERT_EQ(ptr_count, initial_ptrs + ALLOCATION_COUNT / 2,
			"Incorrect number of pointers after half free");
	T_ASSERT_EQ(total_in_use_ptr_size,
			initial_in_use_ptr_size + total_requested_size - size_freed,
			"Incorrect in-use pointer size after half free");

	// Free the rest the memory and recheck the statistics
	for (int i = ALLOCATION_COUNT / 2; i < ALLOCATION_COUNT; i++) {
		free(allocations[i]);
	}
	
	// Check the stats and enumerator values one more time.
	malloc_zone_statistics(malloc_default_zone(), &stats);
	run_enumerator();

	T_ASSERT_EQ(stats.blocks_in_use, initial_blocks_in_use,
			"Incorrect blocks_in_use after full free");
	T_ASSERT_EQ(stats.size_in_use, initial_size_in_use,
			"Incorrect size_in_use after full free");
	T_ASSERT_GE(stats.size_allocated, initial_size_allocated,
			"Size allocated must be >= size requested");
	
	T_ASSERT_EQ(ptr_count, initial_ptrs, "Incorrect number of pointers after free");
	T_ASSERT_EQ(total_in_use_ptr_size, initial_in_use_ptr_size,
			"Incorrect in-use pointer size after free");
#else // CONFIG_NANOZONE
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}

#pragma mark -
#pragma mark Nano realloc tests

// These tests are specific to the Nano implementation of realloc(). They
// don't necessarily work with other allocators, since the behavior tested is
// not part of the documented behavior of realloc().

const char * const data = "abcdefghijklm";

T_DECL(realloc_nano_size_class_change, "realloc with size class change",
	   T_META_ENVVAR("MallocNanoZone=1"))
{
#if CONFIG_NANOZONE
	void *ptr = malloc(16);
	strcpy(ptr, data);
	void *new_ptr;

	// Each pass of the loop realloc's to the next size class up. We must
	// get a new pointer each time and the content must have been copied.
	for (int i = 32; i <= 256; i += 16) {
		new_ptr = realloc(ptr, i);
		T_QUIET; T_ASSERT_TRUE(ptr != new_ptr, "realloc pointer should change");
		T_QUIET; T_ASSERT_EQ(i, (int)malloc_size(new_ptr), "Check size for new allocation");
		T_QUIET; T_ASSERT_TRUE(!strncmp(new_ptr, data, strlen(data)), "Content must be copied");
		T_QUIET; T_ASSERT_EQ(0, (int)malloc_size(ptr), "Old allocation not freed");
		ptr = new_ptr;
	}
	free(new_ptr);
#else // CONFIG_NANOZONE
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}

T_DECL(realloc_nano_ptr_change, "realloc with pointer change",
	   T_META_ENVVAR("MallocNanoZone=1"))
{
#if CONFIG_NANOZONE
	void *ptr = malloc(32);
	strcpy(ptr, data);

	void *new_ptr = realloc(ptr, 128);
	T_ASSERT_TRUE(ptr != new_ptr, "realloc pointer should change");
	T_ASSERT_EQ(128, (int)malloc_size(new_ptr), "Wrong size for new allocation");
	T_ASSERT_TRUE(!strncmp(new_ptr, data, strlen(data)), "Content must be copied");
	T_ASSERT_EQ(0, (int)malloc_size(ptr), "Old allocation not freed");
	
	free(new_ptr);
#else // CONFIG_NANOZONE
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}

T_DECL(realloc_nano_to_other, "realloc with allocator change (nano)",
	   T_META_ENVVAR("MallocNanoZone=1"), T_META_TAG_NANO_ON_XZONE)
{
#if CONFIG_NANOZONE
	void *ptr = malloc(32);					// From Nano
	strcpy(ptr, data);

	void *new_ptr = realloc(ptr, 1024);		// Cannot be Nano.

	T_ASSERT_TRUE(ptr != new_ptr, "realloc pointer should change");
	T_ASSERT_EQ(1024, (int)malloc_size(new_ptr), "Wrong size for new allocation");
	T_ASSERT_TRUE(!strncmp(new_ptr, data, strlen(data)), "Content must be copied");
	T_ASSERT_EQ(0, (int)malloc_size(ptr), "Old allocation not freed");

	free(new_ptr);
#else // CONFIG_NANOZONE
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}

T_DECL(realloc_nano_to_zero_size, "realloc with target size zero",
	   T_META_ENVVAR("MallocNanoZone=1"))
{
#if CONFIG_NANOZONE
	void *ptr = malloc(16);

	// Realloc to 0 frees the old memory and returns a valid pointer.
	void *new_ptr = realloc(ptr, 0);
	T_ASSERT_EQ(0, (int)malloc_size(ptr), "Old allocation not freed");
	T_ASSERT_NOTNULL(new_ptr, "New allocation must be non-NULL");
	T_ASSERT_TRUE(malloc_size(new_ptr) > 0, "New allocation not known");

	free(new_ptr);
#else // CONFIG_NANOZONE
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}

T_DECL(realloc_nano_shrink, "realloc to smaller size",
	   T_META_ENVVAR("MallocNanoZone=1"))
{
#if CONFIG_NANOZONE
	void *ptr = malloc(64);
	strcpy(ptr, data);

	// Reallocate to greater than half the current size - should remain
	// in-place.
	void *new_ptr = realloc(ptr, 40);
	T_ASSERT_TRUE(ptr == new_ptr, "realloc pointer should not change");
	T_ASSERT_TRUE(!strncmp(new_ptr, data, strlen(data)), "Content changed");

	// Reallocate to less than half the current size - should get a new pointer
	// Realloc to 0 frees the old memory and returns a valid pointer.
	ptr = new_ptr;
	new_ptr = realloc(ptr, 16);
	T_ASSERT_TRUE(ptr != new_ptr, "realloc pointer should change");
	T_ASSERT_TRUE(!strncmp(new_ptr, data, strlen(data)), "Content must be copied");

	free(new_ptr);
#else // CONFIG_NANOZONE
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}

T_DECL(nano_memalign_trivial, "Test that nano serves trivial memalign allocations",
	   T_META_ENVVAR("MallocNanoZone=1"))
{
#if CONFIG_NANOZONE
	size_t size = 16;
	void *ptr8 = aligned_alloc(8, size);
	void *ptr16 = aligned_alloc(16, size);
	T_LOG("Nano ptrs are %p, %p\n", ptr8, ptr16);
	T_ASSERT_EQ(NANOZONE_SIGNATURE, (uint64_t)((uintptr_t)ptr8) >> SHIFT_NANO_SIGNATURE,
			"8-byte-aligned allocation served from nano");
	T_ASSERT_EQ(NANOZONE_SIGNATURE, (uint64_t)((uintptr_t)ptr16) >> SHIFT_NANO_SIGNATURE,
			"16-byte-aligned allocation served from nano");
	free(ptr8);
	free(ptr16);
#else // CONFIG_NANOZONE
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}

#pragma mark -
#pragma mark Nanov2 tests

// These tests are specific to the implementation of the Nanov2 allocator.

// Guaranteed number of 256-byte allocations to be sure we fill an arena.
#define ALLOCS_PER_ARENA ((NANOV2_ARENA_SIZE)/256)

T_DECL(overspill_arena, "force overspill of an arena",
	   T_META_ENVVAR("MallocNanoZone=V2"),
	   T_META_ENVVAR("MallocGuardEdges=all"))
{
#if CONFIG_NANOZONE
	void **ptrs = calloc(ALLOCS_PER_ARENA, sizeof(void *));
	T_QUIET; T_ASSERT_NOTNULL(ptrs, "Unable to allocate pointers");
	int index;

	nanov2_addr_t first_ptr;
	ptrs[0] = malloc(256);
	T_QUIET; T_ASSERT_NOTNULL(ptrs[index], "Failed to allocate");
	first_ptr.addr = ptrs[0];

	for (index = 1; index < ALLOCS_PER_ARENA; index++) {
		ptrs[index] = malloc(256);
		T_QUIET; T_ASSERT_NOTNULL(ptrs[index], "Failed to allocate");

		// Stop allocating once we have crossed into a new arena.
		nanov2_addr_t current_ptr;
		current_ptr.addr = ptrs[index];
		if (current_ptr.fields.nano_arena != first_ptr.fields.nano_arena) {
			break;
		}

		// Write to the pointer to ensure the containing block is not
		// a guard block.
		*(int *)ptrs[index] = 0;
	}

	// Free everything, which is a check that the book-keeping works across
	// arenas.
	for (int i = 0; i <= index; i++) {
		free(ptrs[i]);
	}
	free(ptrs);
#else // CONFIG_NANOZONE
	T_SKIP("Nano allocator not configured");
#endif // CONFIG_NANOZONE
}

#if CONFIG_NANOZONE
// Guaranteed number of 256-byte allocations to be sure we fill a region.
#define ALLOCS_PER_REGION ((NANOV2_REGION_SIZE)/256)

#if NANOV2_MULTIPLE_REGIONS
T_DECL(overspill_region, "force overspill of a region",
	   T_META_ENVVAR("MallocNanoZone=V2"))
{
	void **ptrs = calloc(ALLOCS_PER_REGION, sizeof(void *));
	T_QUIET; T_ASSERT_NOTNULL(ptrs, "Unable to allocate pointers");
	int index;

	nanov2_addr_t first_ptr;
	ptrs[0] = malloc(256);
	T_QUIET; T_ASSERT_NOTNULL(ptrs[index], "Failed to allocate");
	first_ptr.addr = ptrs[0];

	for (index = 1; index < ALLOCS_PER_REGION; index++) {
		ptrs[index] = malloc(256);
		T_QUIET; T_ASSERT_NOTNULL(ptrs[index], "Failed to allocate");

		// Stop allocating once we have crossed into a new region.
		nanov2_addr_t current_ptr;
		current_ptr.addr = ptrs[index];
		if (current_ptr.fields.nano_region != first_ptr.fields.nano_region) {
			break;
		}
	}

	// Free everything, which is a check that the book-keeping works across
	// regions.
	for (int i = 0; i <= index; i++) {
		free(ptrs[i]);
	}
	free(ptrs);
}
#endif // NANOV2_MULTIPLE_REGIONS

extern malloc_zone_t **malloc_zones;

T_DECL(overspill_nanozone, "force overspill of nano zone",
		T_META_ENVVAR("MallocNanoZone=V2"),
		T_META_ENVVAR("MallocNanoMaxRegion=1"),
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG_NANO_ON_XZONE)
{
	int index;
	bool spilled_to_tiny = false;
	void **ptrs;
	malloc_zone_t *nano_zone = malloc_zones[0];
	malloc_zone_t *helper_zone = malloc_zones[1];

	// Max number of 256B allocations that will fit in the nano zone (+1 to overspill)
	const unsigned int nano_max_allocations = 2 * ALLOCS_PER_REGION + 1;

	T_LOG("Allocating %d pointers for allocations", nano_max_allocations);
	ptrs = calloc(nano_max_allocations, sizeof(void *));
	T_QUIET; T_ASSERT_NOTNULL(ptrs, "Unable to allocate pointers");

	ptrs[0] = malloc(256);
	T_QUIET; T_ASSERT_NOTNULL(ptrs[0], "Failed to make initial allocation");
	T_QUIET; T_ASSERT_TRUE(malloc_zone_claimed_address(nano_zone, ptrs[0]), 
			"Initial allocation did not come from nano zone");
	T_QUIET; T_ASSERT_FALSE(malloc_zone_claimed_address(helper_zone, ptrs[0]), 
			"Initial allocation came from helper zone");

	for (index = 1; index < (nano_max_allocations); index++) {
		ptrs[index] = malloc(256);
		if (malloc_zone_claimed_address(helper_zone, ptrs[index])) {
			T_LOG("Spilled to helper zone");
			spilled_to_tiny = true;
			break;
		}
	}
	T_EXPECT_TRUE(spilled_to_tiny, "Allocation falls through to helper zone");

	T_LOG("Freeing %d pointers", index);
	for (int i = 0; i < MIN(index + 1, nano_max_allocations); i++) {
		free(ptrs[i]);
	}
	free(ptrs);
}

#if NANOV2_MULTIPLE_REGIONS
void *
punch_holes_thread(void *arg)
{
	T_LOG("Starting holes thread");
	bool *done = arg;

	bool holes[1024] = { false };
	while (!os_atomic_load(done, relaxed)) {
		bool allocate = random() % 2;
		int which = random() % 1024;
		uintptr_t base = NANOZONE_BASE_REGION_ADDRESS;
		size_t len = 128ull * 1024ull * 1024ull;
		size_t stride = 512ull * 1024ull * 1024ull;
		uint64_t offset = stride * which;
		void *addr = (void *)(base + offset);

		if (allocate && !holes[which]) {
			void *hole = mmap(addr, len, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
			if (hole == addr) {
				holes[which] = true;
				T_LOG("punched hole %d (%p, %p)", which, hole, addr);
			} else if (hole != MAP_FAILED) {
				T_LOG("failed to punch hole %d (wanted %p, got %p), unmapping", which,
						addr, hole);
				munmap(hole, len);
			}
		} else if (!allocate && holes[which]) {
			munmap(addr, len);
			holes[which] = false;
			T_LOG("unmapped hole %d", which);
		}

		usleep(300);
	}

	return NULL;
}

void *
do_allocations_thread(void *arg)
{
	bool *done = arg;

	size_t n = (256ull << 20) / 128;
	void **allocations = malloc(n * sizeof(void *));
	T_ASSERT_NOTNULL(allocations, "allocations");

	while (!os_atomic_load(done, relaxed)) {
		for (int i = 0; i < n; i++) {
			allocations[i] = malloc(128);
		}

		usleep(100);

		for (int i = 0; i < n; i++) {
			free(allocations[i]);
		}

		usleep(50000 * ((random() % 8) + 1));
	}

	free(allocations);

	return NULL;
}

T_DECL(region_holes, "ensure correct handling of holes between regions",
		T_META_ENVVAR("MallocNanoZone=V2"),
		// Region reservation does not allow for holes between regions
		T_META_ENABLED(!CONFIG_NANO_RESERVE_REGIONS))
{
	srandom(time(NULL));

	bool done = false;

	pthread_t holes_thread;
	int rc = pthread_create(&holes_thread, NULL, punch_holes_thread, &done);
	T_ASSERT_POSIX_ZERO(rc, "pthread_create");

	int nthreads = 4;
	pthread_t allocation_threads[nthreads];
	for (int i = 0; i < nthreads; i++) {
		rc = pthread_create(&allocation_threads[i], NULL, do_allocations_thread,
				&done);
		T_ASSERT_POSIX_ZERO(rc, "pthread_create");
	}

	sleep(4); // arbitrary time to try to hit the race

	os_atomic_store(&done, true, relaxed);

	rc = pthread_join(holes_thread, NULL);
	T_ASSERT_POSIX_ZERO(rc, "pthread_join");

	for (int i = 0; i < nthreads; i++) {
		rc = pthread_join(allocation_threads[i], NULL);
		T_ASSERT_POSIX_ZERO(rc, "pthread_join");
	}

	T_PASS("Didn't crash");
	T_END;
}
#endif // NANOV2_MULTIPLE_REGIONS
#endif // CONFIG_NANOZONE

