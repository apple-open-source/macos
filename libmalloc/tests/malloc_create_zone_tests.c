//
//  malloc_create_xzone_tests.c
//  libmalloc
//
//  Tests that we can create additional xzone malloc zones through
//  malloc_create_zone, and that these zones behave properly
//

#include <darwintest.h>
#include <stdlib.h>
#include <malloc/malloc.h>
#include <../src/internal.h>

T_DECL(malloc_create_many_zones, "Register lots of zones",
		T_META_CHECK_LEAKS(false), T_META_TAG_ALL_ALLOCATORS,
		T_META_TAG_VM_NOT_PREFERRED, T_META_ENABLED(MALLOC_TARGET_64BIT))
{
	const int num_zones = 4097;

	for (int i = 0; i < num_zones; i++) {
		malloc_zone_t *zone = malloc_create_zone(0, 0);
		T_QUIET; T_ASSERT_NOTNULL(zone, "create zone %d", i);

		void *p = malloc_zone_malloc(zone, 16);
		T_QUIET; T_ASSERT_NOTNULL(p, "malloc_zone_malloc %d", i);
		T_QUIET; T_ASSERT_GE(malloc_size(p), 16ul, "malloc_size %d", i);
		free(p);
	}
}

#if CONFIG_XZONE_MALLOC

#define PTR_EQUALS(a, b) (((uintptr_t)a & (XZM_LIMIT_ADDRESS - 1)) == \
			((uintptr_t)b & (XZM_LIMIT_ADDRESS - 1)))

T_DECL(malloc_xzone_create_zone, "Test malloc_create_zone with xzones enabled",
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_PREFERRED)
{
	// Check that we can create a new zone, and that it is an xzone
	malloc_zone_t *new_zone = malloc_create_zone(0, 0);
	T_ASSERT_NOTNULL(new_zone, "Create new zone");

	T_ASSERT_GE(new_zone->version, 14, "New zone supports zone_type");
	T_ASSERT_EQ(new_zone->introspect->zone_type, MALLOC_ZONE_TYPE_XZONE,
			"New zone is of type XZONE");

	void *ptr = malloc_zone_malloc(new_zone, 65536);
	T_ASSERT_NOTNULL(ptr, "allocate from new zone");
	T_ASSERT_GE(malloc_size(ptr), 65536ul, "pointer size works");

	malloc_destroy_zone(new_zone);

	// Once again, but make a small enough allocation to be served from the
	// early allocator
	new_zone = malloc_create_zone(0, 0);
	T_ASSERT_NOTNULL(new_zone, "Create new zone");
	T_ASSERT_GE(new_zone->version, 14, "New zone supports zone_type");
	T_ASSERT_EQ(new_zone->introspect->zone_type, 1,
			"New zone is of type XZONE");
	ptr = malloc_zone_malloc(new_zone, 64);
	T_ASSERT_NOTNULL(ptr, "allocate from new zone");
	T_ASSERT_GE(malloc_size(ptr), 64ul, "pointer size works");
	malloc_destroy_zone(new_zone);
	T_ASSERT_EQ(malloc_size(ptr), 0ul, "pointer is freed by destroying zone");
	T_PASS("success");
}

// This test would exhaust the 64GB embedded address space, so only run on OSX
#if TARGET_OS_OSX
T_DECL(malloc_xzone_too_many_zones, "Register more zones than xzm supports",
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_PREFERRED)
{
	// XZM uses a uint16_t for the mzone unique ID, so we can only register
	// (1 << 16) - 2 new zones (ID 1 is reserved for the main zone, and ID 0 is
	// a magic invalid value). After running out of IDs, check that
	// malloc_create_zone falls back to a scalable zone
	const size_t num_xzone_zones = (1 << 16) - 2;
	malloc_zone_t *xzm_zones[num_xzone_zones];
	for (int i = 0; i < num_xzone_zones; i++) {
		xzm_zones[i] = malloc_create_zone(0, 0);
		T_QUIET; T_ASSERT_NOTNULL(xzm_zones[i], "Create new zone %d", i);
		T_QUIET;
		T_ASSERT_GE(xzm_zones[i]->version, 14, "New zone supports zone_type");
		T_QUIET;
		T_ASSERT_EQ(xzm_zones[i]->introspect->zone_type, MALLOC_ZONE_TYPE_XZONE,
				"New zone is of type XZONE");
	}

	malloc_zone_t *szone = malloc_create_zone(0, 0);
	T_ASSERT_NOTNULL(szone, "Fallback to new scalable zone");
	if (szone->version >= 14) {
		T_ASSERT_NE(szone->introspect->zone_type, MALLOC_ZONE_TYPE_XZONE,
				"Fallback shouldn't be xzone malloc zone");
	} else {
		T_LOG("Fallback malloc zone doesn't support zone_type");
	}

	malloc_destroy_zone(szone);
	for (int i = 0; i < num_xzone_zones; i++) {
		malloc_destroy_zone(xzm_zones[i]);
	}
}
#endif // TARGET_OS_OSX

struct pointers_expected {
	size_t num_pointers;
	vm_range_t ranges[0];
};

// Once we find an enumerated pointer, set the expected size of it to the
// maximum value so that re-enumerating it causes an error
const vm_size_t already_seen = (vm_size_t)-1;

static void
pointer_recorder(task_t task, void *context, unsigned type, vm_range_t *ranges,
		unsigned count)
{
	T_QUIET; T_ASSERT_NOTNULL(context, "Received expected pointer ranges");
	struct pointers_expected *expected = context;

	for (unsigned i = 0; i < count; i++) {
		if (!(type & MALLOC_PTR_IN_USE_RANGE_TYPE)) {
			continue;
		}
		// Make sure that this pointer is in the expected list
		bool found_ptr = false;
		for (int j = 0; j < expected->num_pointers; j++) {
			if (PTR_EQUALS(expected->ranges[j].address, ranges[i].address)) {
				T_QUIET; T_ASSERT_NE(already_seen, expected->ranges[j].size,
						"enumerated %p twice", (void *)ranges[i].address);
				T_ASSERT_LE(expected->ranges[j].size, ranges[i].size,
						"Allocation %i has correct size", j);
				// Set the size to the max value so that enumerating it twice
				expected->ranges[j].size = already_seen;
				found_ptr = true;
			}
		}
		T_QUIET;
		T_ASSERT_TRUE(found_ptr, "Enumerated unexpected_ptr  %p (size 0x%lx)",
			(void *)ranges[i].address, ranges[i].size);
	}
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

T_DECL(malloc_new_xzone_enumerate, "Test non-default xzone enumerator",
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_PREFERRED)
{
	malloc_zone_t *new_zone = malloc_create_zone(0, 0);
	T_ASSERT_GE(new_zone->version, 14, "New zone isn't an xzone");
	T_ASSERT_EQ(new_zone->introspect->zone_type, MALLOC_ZONE_TYPE_XZONE,
		"New zone isn't an xzone");

	// Make 4 tiny allocations, 2 small allocations, 1 large and 1 huge
	// allocation from the new zone, and assert that the enumerator sees them
	const size_t num_pointers = 8;
	struct pointers_expected *pointers = malloc(sizeof(*pointers) +
			num_pointers * sizeof(vm_range_t));
	T_ASSERT_NOTNULL(pointers, "Allocated space for expected ranges");
	pointers->num_pointers = num_pointers;
	pointers->ranges[0].size = 64;
	pointers->ranges[1].size = 256;
	pointers->ranges[2].size = 1024;
	pointers->ranges[3].size = 2048; // TINY
	pointers->ranges[4].size = 10000;
	pointers->ranges[5].size = 30000; // SMALL
	pointers->ranges[6].size = 65536; // LARGE
	pointers->ranges[7].size = 4000000; // HUGE

	for (int i = 0; i < num_pointers; i++) {
		size_t size = pointers->ranges[i].size;
		void *addr = malloc_zone_malloc(new_zone, size);
		T_ASSERT_NOTNULL(addr, "Malloc %zu bytes from new zone", size);
		pointers->ranges[i].address = (mach_vm_address_t)addr;
	}

	// Run the enumerator
	kern_return_t kr = new_zone->introspect->enumerator(mach_task_self(),
			pointers, MALLOC_PTR_IN_USE_RANGE_TYPE, (vm_address_t)new_zone,
			memory_reader, pointer_recorder);
	T_ASSERT_EQ(KERN_SUCCESS, kr, "Enumerator returned success");

	// Check that all pointers were seen
	for (int i = 0; i < pointers->num_pointers; i++) {
		// The enumerator sets the size in this structure for every address
		// it sees to already_seen
		T_ASSERT_EQ(already_seen, pointers->ranges[i].size, "Found pointer %i",
				i);
	}

	// Free all pointers to pass the leaks check
	for (int i = 0; i < num_pointers; i++) {
		free((void*)pointers->ranges[i].address);
	}
	free(pointers);
}

struct worker_thread_data {
	bool done;
	malloc_zone_t *zone;
};
static void *
worker_thread(void *arg)
{
	struct worker_thread_data *data = (struct worker_thread_data*)arg;
	bool *done = &data->done;
	malloc_zone_t *zone = data->zone;
	while (!os_atomic_load(done, relaxed)) {
		size_t size = 1 << (rand() % 16);
		void *ptr = malloc_zone_malloc(zone, size);
		T_QUIET; T_ASSERT_NOTNULL(ptr, "malloc failed");
		free(ptr);
	}
	return NULL;
}

T_DECL(malloc_fork_with_xzone, "Test that we can fork with a non-default xzone",
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_NOT_PREFERRED)
{
	malloc_zone_t *new_zone = malloc_create_zone(0, 0);
	T_ASSERT_GE(new_zone->version, 14, "New zone isn't an xzone");
	T_ASSERT_EQ(new_zone->introspect->zone_type, MALLOC_ZONE_TYPE_XZONE,
		"New zone isn't an xzone");

	// Create some worker threads to do constant allocations and frees, to try
	// to expose any multi-threading bugs
	struct worker_thread_data data = {
		.done = false,
		.zone = new_zone,
	};
	const int num_threads = 8;
	pthread_t worker_threads[8] = { NULL };
	for (int i = 0; i < num_threads; i++) {
		pthread_create(&worker_threads[i], NULL, &worker_thread, &data);
	}

	// Wait a few microseconds so that the worker threads have had a chance to
	// run
	usleep(100);

	// Make 2 allocations, fork, and then ensure that both can see the
	// allocations and free them
	void *ptr1 = malloc_zone_malloc(new_zone, 64);
	void *ptr2 = malloc_zone_malloc(new_zone, 65536);
	T_ASSERT_NOTNULL(ptr1, "TINY allocation");
	T_ASSERT_NOTNULL(ptr2, "LARGE allocation");

	pid_t pid;
	if ((pid = fork()) == 0) {
		// Child - check that the pointers exist and can be freed, and exit
		// nonzero if we run into any problems
		if (malloc_size(ptr1) < 64) {
			fprintf(stderr, "TINY ptr %p has unexpected size %zu after fork",
					ptr1, malloc_size(ptr1));
			exit(1);
		} else if (malloc_size(ptr2) < 65536) {
			fprintf(stderr, "LARGE ptr %p has unexpected size %zu after fork",
					ptr2, malloc_size(ptr2));
			exit(2);
		}
		free(ptr1);
		free(ptr2);
		// Xzone Malloc can mistakenly report tiny freed chunks as allocated if
		// the chunk is madvised, treat a malloc_size mismatch as non-fatal
		if (malloc_size(ptr1) != 0 && !malloc_engaged_secure_allocator()) {
			fprintf(stderr, "TINY ptr %p has unexpected size %zu after free",
					ptr1, malloc_size(ptr1));
			exit(3);
		} else if (malloc_size(ptr2) != 0) {
			fprintf(stderr, "LARGE ptr %p has unexpected size %zu after free",
					ptr2, malloc_size(ptr2));
			exit(4);
		} else {
			// Success
			exit(0);
		}
	} else if (pid == -1) {
		T_ASSERT_FAIL("Fork failed");
	}

	// Parent
	int child_status = -1;
	pid_t child_pid = wait(&child_status);
	T_ASSERT_EQ(pid, child_pid, "Expected pid from child");
	T_ASSERT_TRUE(WIFEXITED(child_status), "Child called exit");
	T_ASSERT_EQ(WEXITSTATUS(child_status), 0, "Child called exit(0)");

	T_ASSERT_LE(malloc_size(ptr1), 64ul, "tiny pointer still seen post fork");
	T_ASSERT_LE(malloc_size(ptr2), 65536ul, "large pointer still seen post fork");

	free(ptr1);
	free(ptr2);

	// Join all the worker threads so that there are no leaks when the test ends
	os_atomic_store(&data.done, true, relaxed);
	for (int i = 0; i < num_threads; i++) {
		pthread_join(worker_threads[i], NULL);
	}
}

T_DECL(malloc_statistics, "Make sure the main and new zone support statistics",
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG_ALL_ALLOCATORS, T_META_TAG_VM_PREFERRED)
{
	malloc_zone_t *default_zone = malloc_default_zone();
	malloc_zone_t *new_zone = malloc_create_zone(0, 0);

	malloc_statistics_t default_stats = { 0 };
	malloc_statistics_t new_stats = { 0 };

	malloc_zone_statistics(default_zone, &default_stats);
	malloc_zone_statistics(new_zone, &new_stats);

	T_ASSERT_EQ(new_stats.size_in_use, 0ul,
			"Accurate stats before any allocations");
	T_ASSERT_EQ(new_stats.blocks_in_use, 0u,
			"Accurate allocation count before any allocations");
	void *ptr1 = malloc_zone_malloc(new_zone, 1024);
	void *ptr2 = malloc_zone_malloc(new_zone, 8192);
	malloc_zone_statistics(new_zone, &new_stats);
	free(ptr1);
	free(ptr2);

	T_ASSERT_EQ(new_stats.size_in_use, 1024ul + 8192ul,
			"Accurate stats after allocations");
	T_ASSERT_EQ(new_stats.blocks_in_use, 2u,
			"Accurate allocation count after allocations");
}

T_DECL(malloc_free_pointers_on_destroy,
		"Make many allocations, make sure they're freed on zone destroy",
		T_META_TAG_ALL_ALLOCATORS, T_META_TAG_VM_PREFERRED)
{
	malloc_zone_t *new_zone = malloc_create_zone(0, 0);

	// Allocate 1 segment of tiny chunks (4k * 1024 == 4M)
	void *tiny_ptrs[1024];

	// Allocate 3 segments worth of small chunks (32k * 384 == 4M * 3)
	void *small_ptrs[384];

	// Allocate 5 segments worth of large chunks
	void *large_ptrs[256];

	// Allocate 2 huge segments
	void *huge_ptrs[2];

	for (int i = 0; i < countof(tiny_ptrs); i++) {
		tiny_ptrs[i] = malloc_zone_malloc(new_zone, 1024);
		T_QUIET; T_ASSERT_NOTNULL(tiny_ptrs[i], "TINY allocation succeeded");
		T_QUIET; T_ASSERT_GE(malloc_size(tiny_ptrs[i]), 1024ul,
				"TINY allocation is of requested size");
	}

	for (int i = 0; i < countof(small_ptrs); i++) {
		small_ptrs[i] = malloc_zone_malloc(new_zone, 32768);
		T_QUIET; T_ASSERT_NOTNULL(small_ptrs[i], "SMALL allocation succeeded");
		T_QUIET; T_ASSERT_GE(malloc_size(small_ptrs[i]), 32768ul,
				"SMALL allocation is of requested size");
	}

	for (int i = 0; i < countof(large_ptrs); i++) {
		large_ptrs[i] = malloc_zone_malloc(new_zone, 65536);
		T_QUIET; T_ASSERT_NOTNULL(large_ptrs[i], "LARGE allocation succeeded");
		T_QUIET; T_ASSERT_GE(malloc_size(large_ptrs[i]), 65536ul,
				"LARGE allocation is of requested size");
	}

	for (int i = 0; i < countof(huge_ptrs); i++) {
		huge_ptrs[i] = malloc_zone_malloc(new_zone, MiB(32));
		T_QUIET; T_ASSERT_NOTNULL(huge_ptrs[i], "HUGE allocation succeeded");
		T_QUIET; T_ASSERT_GE(malloc_size(huge_ptrs[i]), (size_t)MiB(32),
				"HUGE allocation is of requested size");
	}

	malloc_destroy_zone(new_zone);

	for (int i = 0; i < countof(tiny_ptrs); i++) {
		T_QUIET; T_ASSERT_EQ(malloc_size(tiny_ptrs[i]), 0ul,
				"TINY allocation is freed by destroying zone");
	}

	for (int i = 0; i < countof(small_ptrs); i++) {
		T_QUIET; T_ASSERT_EQ(malloc_size(small_ptrs[i]), 0ul,
				"SMALL allocation is freed by destroying zone");
	}

	for (int i = 0; i < countof(large_ptrs); i++) {
		T_QUIET; T_ASSERT_EQ(malloc_size(large_ptrs[i]), 0ul,
				"LARGE allocation is freed by destroying zone");
	}

	for (int i = 0; i < countof(huge_ptrs); i++) {
		T_QUIET; T_ASSERT_EQ(malloc_size(huge_ptrs[i]), 0ul,
				"HUGE allocation is freed by destroying zone");
	}
}

static void zone_pressure(bool *finished, malloc_zone_t *zone, bool free)
{
	const int num_ptrs = 1024;
	void *ptrs[num_ptrs] = { NULL };

	malloc_type_id_t pure_data = (malloc_type_descriptor_t){
		.summary = (malloc_type_summary_t){
			.layout_semantics = (malloc_type_layout_semantics_t) {
				.generic_data = true,
			}
		}
	}.type_id;

	while (!os_atomic_load(finished, relaxed)) {
		bool data = rand() & 0x1;
		int index = rand() % num_ptrs;
		int new_sz = (rand() & 0xff) + 1;
		new_sz <<= (rand() % 10);
		if (ptrs[index] && ((rand() % 8) == 0)) {
			if (data) {
				ptrs[index] = malloc_type_zone_realloc(zone, ptrs[index],
						new_sz, pure_data);
			} else {
				ptrs[index] = malloc_zone_realloc(zone, ptrs[index], new_sz);
			}
		} else {
			malloc_zone_free(zone, ptrs[index]);
			if (data) {
				ptrs[index] = malloc_type_zone_malloc(zone, new_sz, pure_data);
			} else {
				ptrs[index] = malloc_zone_malloc(zone, new_sz);
			}
		}
	}

	if (free) {
		for (int i = 0; i < num_ptrs; i++) {
			malloc_zone_free(zone, ptrs[i]);
		}
	}
}

extern malloc_zone_t **malloc_zones;
static void *
main_zone_thread(void *arg)
{
	bool *finished = (bool *)arg;
	malloc_zone_t *zone = malloc_zones[0];
	zone_pressure(finished, zone, true);
	return NULL;
}

static void *
create_zone_thread(void *arg)
{
	bool *finished = (bool *)arg;
	malloc_zone_t *zone = malloc_create_zone(0, 0);
	zone_pressure(finished, zone, false);
	malloc_destroy_zone(zone);
	return NULL;
}

static void
create_zone_stress(void)
{
	const int num_main_threads = 3;
	const int concurrent_new_zones = 3;
	const int iterations = 100;

	bool main_finished = false;
	pthread_t main_zone_threads[num_main_threads];
	for (int i = 0; i < num_main_threads; i++) {
		int rc = pthread_create(&main_zone_threads[i], NULL, main_zone_thread,
				&main_finished);
		T_ASSERT_POSIX_ZERO(rc, "Create main zone pressure thread");
	}

	sleep(1);

	for (int i = 0; i < iterations; i++) {
		bool finished = false;
		pthread_t new_zone_threads[concurrent_new_zones];
		for (int j = 0; j < concurrent_new_zones; j++) {
			int rc = pthread_create(&new_zone_threads[j], NULL,
					create_zone_thread, &finished);
			T_ASSERT_POSIX_ZERO(rc, "Create new zone thread");
			usleep(500);
		}

		usleep(50000);
		os_atomic_store(&finished, true, relaxed);

		for (int j = 0; j < concurrent_new_zones; j++) {
			int rc = pthread_join(new_zone_threads[j], NULL);
			T_ASSERT_POSIX_ZERO(rc, "Join new zone thread");
		}

		usleep(50000);
	}

	os_atomic_store(&main_finished, true, relaxed);
	for (int i = 0; i < num_main_threads; i++) {
		int rc = pthread_join(main_zone_threads[i], NULL);
		T_ASSERT_POSIX_ZERO(rc, "Join main thread");
	}

	// Create new zones so that their mzone indices are enumerated by leaks
	// after the test runs
	for (int i = 0; i < concurrent_new_zones; i++) {
		malloc_create_zone(0, 0);
	}
}

T_DECL(malloc_create_zone_stress,
		"Create and destroy zones while stressing main zone",
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG_ALL_ALLOCATORS, T_META_TAG_VM_NOT_PREFERRED)
{
	create_zone_stress();
}

T_DECL(malloc_create_zone_stress_guarded,
		"Create and destroy zones while stressing main zone, guard pages enabled",
		T_META_ENVVAR("MallocXzoneGuarded=1"),
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_NOT_PREFERRED)
{
	create_zone_stress();
}

T_DECL(free_default_with_scribble,
		"Test freeing from a non-default zone while MALLOC_SCRIBBLE is enabled",
		T_META_TAG_ALL_ALLOCATORS,
		T_META_ENVVAR("MallocScribble=1"), T_META_TAG_VM_NOT_PREFERRED)
{
	malloc_zone_t *zone = malloc_create_zone(0, 0);
	void *ptr = malloc_zone_malloc(zone, 16384);
	T_ASSERT_NOTNULL(ptr, "Allocate from non-default zone");
	free(ptr);
	T_ASSERT_EQ(malloc_size(ptr), 0ul, "Pointer is freed");
}

#else // CONFIG_XZONE_MALLOC

T_DECL(skip_create_zone, "Do nothing test for !CONFIG_XZONE_MALLOC",
	T_META_ENABLED(false), T_META_TAG_VM_PREFERRED,
	T_META_TAG_NO_ALLOCATOR_OVERRIDE)
{
	T_SKIP("Nothing to test under !CONFIG_XZONE_MALLOC");

}

#endif // CONFIG_XZONE_MALLOC
