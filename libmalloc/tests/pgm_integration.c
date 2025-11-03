//
//  pgm_integration.c
//  libmalloc
//
//  End-to-end integration tests for ProbGuard.
//

#include <darwintest.h>
#include <MallocStackLogging/MallocStackLogging.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(TRUE), T_META_NAMESPACE("pgm"),
		T_META_TAG_ALL_ALLOCATORS);

#include <mach/mach_vm.h>  // mach_vm_map()
#include <mach/mach.h>  // mach_task_self()
#include <mach/vm_page_size.h>
#include <malloc_private.h>
#include <malloc/malloc.h>
#include <stdlib.h>
#include <unistd.h>

T_GLOBAL_META(
	T_META_ENVVAR("MallocProbGuard=1"),
	T_META_ENVVAR("MallocProbGuardSampleRate=1"),
	// Make sure ProbGuard zone doesn't run out of space before actual tests.
	T_META_ENVVAR("MallocProbGuardAllocations=300")
);

static malloc_zone_t *
get_wrapped_zone(malloc_zone_t *zone)
{
	malloc_zone_t *wrapped_zone;
	kern_return_t kr = malloc_get_wrapped_zone(mach_task_self(),
			/*memory_reader=*/NULL, (vm_address_t)zone, (vm_address_t *)&wrapped_zone);
	T_QUIET; T_ASSERT_EQ(kr, KERN_SUCCESS, "malloc_get_wrapped_zone() failed");
	T_EXPECT_NOTNULL(wrapped_zone, "Wrapped zone");
	return wrapped_zone;
}

extern int32_t malloc_num_zones;
extern malloc_zone_t **malloc_zones;
T_DECL(zone_setup, "ProbGuard zone is default zone and not full", T_META_TAG_VM_PREFERRED)
{
	// malloc_default_zone() returns virtual zone, which delegates to zone 0.
	malloc_zone_t *zone0 = malloc_zones[0];
	T_EXPECT_EQ_STR(malloc_get_zone_name(zone0), "ProbGuardMallocZone",
			"ProbGuard zone is default zone");
	T_EXPECT_EQ(zone0->introspect->zone_type, 2, "MALLOC_ZONE_TYPE_PGM");
	T_EXPECT_EQ(get_wrapped_zone(zone0), malloc_zones[1],
			"Wrapped zone is registered");

	void *ptr = malloc(5);
	malloc_zone_t *zone = malloc_zone_from_ptr(ptr);
	free(ptr);
	T_EXPECT_EQ(zone, malloc_default_zone(), "ProbGuard zone not full");
}

static void
assert_crash(void (*test_func)(void))
{
	pid_t child_pid = fork();
	T_QUIET; T_ASSERT_NE(child_pid, -1, "Fork failed");

	if (!child_pid) {
		T_PASS("Triggering crash");
		test_func();
		T_FAIL("Expected crash");
	} else {
		int status;
		pid_t wait_pid = waitpid(child_pid, &status, 0);
		T_QUIET; T_EXPECT_EQ(wait_pid, child_pid, "Child status");
		T_QUIET; T_EXPECT_TRUE(WIFSIGNALED(status), "Child terminated by signal");
		T_EXPECT_EQ(WTERMSIG(status), SIGBUS, "Child terminated due to page fault");
	}
}

static void
touch_memory(uint8_t *ptr)
{
	*(volatile uint8_t *)ptr = 7;
}

static void
use_after_free(void)
{
	void *ptr = malloc(1);
	free(ptr);
	touch_memory(ptr);
}

static void
out_of_bounds(void)
{
	uint8_t *ptr = malloc(16);
	touch_memory(ptr - 1);
	touch_memory(ptr + 17);
}

T_DECL(uaf_detection, "Use-after-free detection",
		T_META_IGNORECRASHES("pgm_integration"),
		T_META_TAG_VM_PREFERRED)
{
	assert_crash(use_after_free);
}

T_DECL(oob_detection, "Out-of-bounds detection",
		T_META_IGNORECRASHES("pgm_integration"),
		T_META_TAG_VM_PREFERRED)
{
	assert_crash(out_of_bounds);
}

static void
access_in_bogus_pgm_region(void)
{
	mach_vm_address_t vm_addr = 0;
	kern_return_t kr = mach_vm_map(mach_task_self(), &vm_addr, PAGE_SIZE, 0,
			VM_FLAGS_ANYWHERE | VM_MAKE_TAG(VM_MEMORY_MALLOC_PROB_GUARD),
			MEMORY_OBJECT_NULL, 0, FALSE, VM_PROT_NONE, VM_PROT_NONE,
			VM_INHERIT_DEFAULT);
	T_ASSERT_MACH_SUCCESS(kr, "allocated bogus PGM region");
	touch_memory((uint8_t *)vm_addr);
}

T_DECL(bogus_pgm_region, "Handle crashes in bogus PGM regions",
		T_META_IGNORECRASHES("pgm_integration"), T_META_TAG_VM_PREFERRED)
{
	// What we're really testing here is the code that runs in ReportCrash to
	// generate the PGM report - it needs to gracefully handle unexpected PGM
	// state in a crashing process
	assert_crash(access_in_bogus_pgm_region);
}

static void
non_default_zone_use_after_free(void)
{
	malloc_zone_t *zone = malloc_create_zone(0, 0);
	void *ptr = malloc_zone_malloc(zone, 1);
	free(ptr);
	touch_memory(ptr);
}

T_DECL(non_default_zone_uaf_detection,
		"Use-after-free detection in a wrapped non-default zone",
		T_META_IGNORECRASHES("pgm_integration"),
		T_META_TAG_VM_PREFERRED)
{
	assert_crash(non_default_zone_use_after_free);
}

static boolean_t
check_bytes(uint8_t *ptr, size_t size)
{
	for (uint32_t i = 0; i < size; i++) {
		if (ptr[i] != 7) return FALSE;
	}
	return TRUE;
}

static void
smoke_test(void)
{
	malloc_zone_t *zone = malloc_default_zone();
	size_t size = arc4random_uniform(PAGE_SIZE);

	const uint32_t num_ptrs = 6 + 10;
	void *ptrs[num_ptrs] = {
		malloc(size),
		calloc(1, size),
		realloc(NULL, size),
		valloc(size),
		aligned_alloc(32, (size + 31) & (~31)) // size must be multiple of alignment
		// posix_memalign
	};
	int res = posix_memalign(&ptrs[5], 32, size);
	T_QUIET; T_ASSERT_EQ(res, 0, "posix_memalign");
	int count = malloc_zone_batch_malloc(zone, size, &ptrs[6], 10);
	// batch_malloc doesn't allocate if size > TINY_LIMIT_THRESHOLD
	for (uint32_t i = 6 + count; i < num_ptrs; i++) {
		ptrs[i] = malloc(size);
	}

	for (uint32_t i = 0; i < num_ptrs; i++) {
		T_QUIET; T_ASSERT_NOTNULL(ptrs[i], "allocate %u", i);
		memset(ptrs[i], /*value=*/7, size);
		ptrs[i] = realloc(ptrs[i], size);
		T_QUIET; T_ASSERT_TRUE(check_bytes(ptrs[i], size), "realloc %u", i);
	}

	for (uint32_t i = 0; i < num_ptrs; i += 2) {
		free(ptrs[i]);
		ptrs[i] = NULL;
	}
	malloc_zone_batch_free(zone, ptrs, num_ptrs);
}

static void
smoke_test_100(void)
{
	malloc_zone_t *zone = malloc_zones[0];
	T_QUIET; T_ASSERT_EQ_STR(malloc_get_zone_name(zone), "ProbGuardMallocZone", NULL);

	for (uint32_t i = 0; i < 100; i++) {
		smoke_test();
		T_QUIET; T_ASSERT_TRUE(malloc_zone_check(zone), "check zone integrity");
	}
}

T_DECL(allocation_sample_all, "Smoke test, sample 1/1", T_META_TAG_VM_PREFERRED)
{
	smoke_test_100();
	T_PASS("Smoke test, sample all");
}

T_DECL(allocation_sample_half, "Smoke test, sample 1/2",
		T_META_ENVVAR("MallocProbGuard=1"), T_META_TAG_VM_PREFERRED,
		T_META_ENVVAR("MallocProbGuardSampleRate=2"))
{
	smoke_test_100();
	T_PASS("Smoke test, sample half");
}

static const uint32_t k_num_expected_blocks = 4;
static void *expected_blocks[k_num_expected_blocks];
static size_t expected_sizes[k_num_expected_blocks];
static void
malloc_expected_block(uint32_t idx, size_t size)
{
	T_QUIET; T_ASSERT_LT(idx, k_num_expected_blocks, "idx < num blocks");
	expected_blocks[idx] = malloc(size);
	expected_sizes[idx] = malloc_size(expected_blocks[idx]);
}

static malloc_statistics_t stats_before, stats_after;
static void
setup_introspection_scenario(void)
{
	malloc_zone_t *zone = malloc_default_zone();
	malloc_zone_statistics(zone, &stats_before);

	malloc_expected_block(0,  5);
	malloc_expected_block(1,  0);
	malloc_expected_block(2, PAGE_SIZE);
	malloc_expected_block(3, 64);

	free(expected_blocks[3]);
	malloc_expected_block(3, 16);

	malloc_zone_statistics(zone, &stats_after);
}

T_DECL(introspection_statistics, "Zone statistics", T_META_TAG_VM_PREFERRED)
{
	setup_introspection_scenario();

	size_t max_size_slack = stats_before.max_size_in_use - stats_before.size_in_use;
	malloc_statistics_t stats = {
		.blocks_in_use = stats_after.blocks_in_use - stats_before.blocks_in_use,
		.size_in_use = stats_after.size_in_use - stats_before.size_in_use,
		.max_size_in_use = stats_after.max_size_in_use - stats_before.max_size_in_use + max_size_slack,
		.size_allocated = stats_after.size_allocated - stats_before.size_allocated
	};

	size_t total_size = (16 + 16 + PAGE_SIZE + 16);
	size_t max_size = total_size - 16 + 64;
	size_t size_allocated = k_num_expected_blocks * PAGE_SIZE;

	T_EXPECT_EQ(stats.blocks_in_use, k_num_expected_blocks, "blocks in use");
	T_EXPECT_EQ(stats.size_in_use, total_size, "size in use");
	T_EXPECT_EQ(stats.max_size_in_use, max_size, "max size in use");
	T_EXPECT_EQ(stats.size_allocated, size_allocated, "size allocated");
}

static void *memory_reader_ptrs[10];
static uint32_t memory_read_count;
static kern_return_t
memory_reader(task_t remote_task, vm_address_t remote_address, vm_size_t size, void **local_memory)
{
	T_QUIET; T_EXPECT_EQ(remote_task, 1337, "memory_reader(): remote_task");
	*local_memory = malloc(size);
	memcpy(*local_memory, (void *)remote_address, size);
	memory_reader_ptrs[memory_read_count] = *local_memory;
	memory_read_count++;
	return KERN_SUCCESS;
}

static void
free_read_memory(void)
{
	T_QUIET; T_EXPECT_GT(memory_read_count, 0, "memory was read");
	for (uint32_t i = 0; i < memory_read_count; i++) {
		free(memory_reader_ptrs[i]);
	}
}

static vm_range_t recorded_ranges[k_num_expected_blocks];
static uint32_t num_recorded;
static void
recorder(task_t task, void *context, unsigned type, vm_range_t *ranges, unsigned count)
{
	T_QUIET; T_EXPECT_TRUE((type == MALLOC_PTR_REGION_RANGE_TYPE) ||
			(type == MALLOC_PTR_IN_USE_RANGE_TYPE), "recorder(): type");
	T_QUIET; T_EXPECT_EQ(count, 1, "recorder(): count");
	static uint32_t total_reported = 0;
	if (total_reported >= stats_before.blocks_in_use) {
		T_QUIET; T_ASSERT_LT(num_recorded, k_num_expected_blocks, "num recorded < num expected blocks");
		recorded_ranges[num_recorded++] = *ranges;
	}
	total_reported++;
}

static void
check_enumerator(unsigned type)
{
	malloc_zone_t *zone = malloc_default_zone();
	kern_return_t kr = zone->introspect->enumerator(1337, NULL, type, (vm_address_t)zone, memory_reader, recorder);
	T_QUIET; T_EXPECT_EQ(kr, KERN_SUCCESS, "enumeration successful");
	T_QUIET; T_EXPECT_EQ(memory_read_count, 2, "memory read");
	T_EXPECT_EQ(num_recorded, k_num_expected_blocks, "recorded expected number of blocks/regions");

	for (uint32_t i = 0; i < k_num_expected_blocks; i++) {
		if (type == MALLOC_PTR_REGION_RANGE_TYPE) {
			T_EXPECT_EQ(recorded_ranges[i].address, trunc_page((vm_address_t)expected_blocks[i]), "region address");
			T_QUIET; T_EXPECT_EQ(recorded_ranges[i].size, (vm_size_t)PAGE_SIZE, "region size");
		} else {
			T_EXPECT_EQ(recorded_ranges[i].address, (vm_address_t)expected_blocks[i], "block address");
			T_QUIET; T_EXPECT_EQ(recorded_ranges[i].size, expected_sizes[i], "block size");
		}
	}

	free_read_memory();
}

T_DECL(introspection_enumerate_regions, "Region enumeration", T_META_TAG_VM_PREFERRED)
{
	setup_introspection_scenario();

	check_enumerator(MALLOC_PTR_REGION_RANGE_TYPE);
}

T_DECL(introspection_enumerate_blocks, "Block enumeration", T_META_TAG_VM_PREFERRED)
{
	setup_introspection_scenario();

	check_enumerator(MALLOC_PTR_IN_USE_RANGE_TYPE);
}

T_DECL(wrap_malloc_create_zone, "Wrap malloc_create_zone()", T_META_TAG_VM_PREFERRED)
{
	// Make sure we only query the environment during process launch.
	setenv("MallocProbGuard", "0", /*overwrite=*/true);

	uint32_t num_zones = malloc_num_zones;
	malloc_zone_t *zone = malloc_create_zone(0, 0);

	T_EXPECT_EQ_STR(malloc_get_zone_name(zone), "ProbGuardMallocZone", "PGM zone");
	T_EXPECT_EQ(zone->introspect->zone_type, 2, "MALLOC_ZONE_TYPE_PGM");

	T_EXPECT_EQ(malloc_num_zones, num_zones + 2, "registered both zones");
	T_EXPECT_EQ(malloc_zones[num_zones], zone, "PGM zone is registered");
	T_EXPECT_EQ(malloc_zones[num_zones + 1], get_wrapped_zone(zone),
			"Wrapped zone is registered");

	malloc_destroy_zone(zone);
	T_EXPECT_EQ(malloc_num_zones, num_zones, "unregistered both zones");
}

T_DECL(disable_pgm_on_lite_zone,
		"The lite zone's helper zone shouldn't be wrapped by PGM",
		T_META_TAG_VM_PREFERRED)
{
	// Enabling MSL Lite will register the lite zone (via malloc_zone_register)
	// and a helper zone (via malloc_create_zone). The latter needs to not
	// insert PGM to avoid extra allocations in the underlying zone that don't
	// have MSL metadata
	uint32_t num_zones = malloc_num_zones;

	bool enable = msl_turn_on_stack_logging(msl_mode_lite);
	T_ASSERT_TRUE(enable, "Enabled MSL Lite");

	T_EXPECT_EQ(malloc_num_zones, num_zones + 2,
			"Enabling MSL generated 2 new zones");
}
