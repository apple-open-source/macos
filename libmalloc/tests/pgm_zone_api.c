//
//  pgm_zone_api.c
//  libmalloc
//
//  Tests for ProbGuard zone APIs.
//

#include <darwintest.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(TRUE), T_META_NAMESPACE("pgm"));

#define PGM_MOCK_SHOULD_SAMPLE_COUNTER
static uint32_t should_sample_counter_call_count;
static boolean_t should_sample_counter_ret_value = TRUE;
static boolean_t
should_sample_counter(uint32_t counter_range)
{
	should_sample_counter_call_count++;
	return should_sample_counter_ret_value;
}

#include "../src/pgm_malloc.c"
// Dependencies
#include "../src/has_section.c"
#include "../src/malloc_common.c"
#include "../src/stack_trace.c"
#include "../src/wrapper_zones.c"

// Stub out cross-file dependencies.
void malloc_report(uint32_t flags, const char *fmt, ...) { }
void malloc_report_simple(const char *fmt, ...) { __builtin_trap(); }

static malloc_zone_t wrapped_zone;
static pgm_zone_t *zone;
static const size_t size = 5;

#define CALL_(func, args...) zone->malloc_zone.func(args)
#define CALL(func, args...) CALL_(func, (malloc_zone_t *)zone, ## args)

static void wrapped_zone_destroy(malloc_zone_t *z)
{
	T_QUIET; T_ASSERT_EQ(z, &wrapped_zone, NULL);
	free(zone->slots); free(zone->metadata);
}

static void
teardown(void)
{
	malloc_zone_register(&wrapped_zone);
	wrapped_zone.destroy = wrapped_zone_destroy;
	CALL(destroy);
	wrapped_zone.destroy = NULL;
}

static void
setup(void)
{
	T_ATEND(teardown);

	wrapped_zone.version = 13;
	wrapped_zone.calloc = malloc_default_zone()->calloc;
	zone = (pgm_zone_t *)pgm_create_zone(&wrapped_zone);
	wrapped_zone.calloc = NULL;
}

T_DECL(optional_apis, "Restrict zone API to what is supported by wrapped zone", T_META_TAG_VM_PREFERRED)
{
	setup();

	malloc_zone_t *z = &zone->malloc_zone;

	// Always supported
	T_EXPECT_NOTNULL(z->batch_malloc, NULL);
	T_EXPECT_NOTNULL(z->batch_free, NULL);
	T_EXPECT_NOTNULL(z->pressure_relief, NULL);
	T_EXPECT_NOTNULL(z->malloc_with_options, NULL);

	// Never supported
	T_EXPECT_NULL(z->try_free_default, NULL);

	// Supported if wrapped zone has it
	T_EXPECT_NULL(z->memalign, NULL);
	T_EXPECT_NULL(z->free_definite_size, NULL);
	T_EXPECT_NULL(z->claimed_address, NULL);
}

static size_t wrapped_zone_malloc_expected_size = size;
static uint32_t wrapped_zone_malloc_call_count;
static void *
wrapped_zone_malloc(malloc_zone_t *zone, size_t size)
{
	T_EXPECT_EQ(size, wrapped_zone_malloc_expected_size, "malloc(size)");
	return (void *)(uintptr_t)++wrapped_zone_malloc_call_count;
}

T_DECL(delegate_unsampled, "delegation of unsampled allocations",
		T_META_ENVVAR("MallocProbGuardAllocations=1"),
		T_META_TAG_VM_PREFERRED)
{
	setup();
	wrapped_zone.malloc = wrapped_zone_malloc;

	wrapped_zone_malloc_expected_size = PAGE_SIZE + 1;
	T_EXPECT_EQ(CALL(malloc, PAGE_SIZE + 1), (void *)1, "requested size > page size");
	T_EXPECT_EQ(should_sample_counter_call_count, 0, "bad size; no call to should_sample_counter()");
	wrapped_zone_malloc_expected_size = size;

	should_sample_counter_ret_value = FALSE;
	T_EXPECT_EQ(CALL(malloc, size), (void *)2, "not sampled");
	should_sample_counter_ret_value = TRUE;
	T_EXPECT_NOTNULL(CALL(malloc, size), "sampled");
	T_EXPECT_EQ(should_sample_counter_call_count, 2, "should_sample_counter() decides");

	T_QUIET; T_EXPECT_TRUE(is_full(zone), "zone full");
	T_EXPECT_EQ(CALL(malloc, size), (void *)3, "zone full");
	T_EXPECT_EQ(should_sample_counter_call_count, 2, "zone full; no call to should_sample_counter()");
}

static void *wrapped_zone_free_expected_ptrs[3];
static uint32_t wrapped_zone_free_call_count;
static void
wrapped_zone_free(malloc_zone_t *zone, void *ptr)
{
	T_QUIET; T_ASSERT_LT(wrapped_zone_free_call_count, 3, NULL);
	T_EXPECT_EQ(ptr, wrapped_zone_free_expected_ptrs[wrapped_zone_free_call_count], "free(ptr)");
	wrapped_zone_free_call_count++;
}

T_DECL(delegate_unguarded, "delegation of unguarded deallocations", T_META_TAG_VM_PREFERRED)
{
	setup();
	wrapped_zone.free = wrapped_zone_free;

	void *p1 = CALL(malloc, size);
	void *p2 = (void *)1337;
	T_EXPECT_TRUE (is_guarded(zone, (vm_address_t)p1), "guarded");
	T_EXPECT_FALSE(is_guarded(zone, (vm_address_t)p2), "unguarded");

	CALL(free, p1);
	T_EXPECT_EQ(wrapped_zone_free_call_count, 0, "handle guarded");

	wrapped_zone_free_expected_ptrs[0] = p2;
	CALL(free, p2);
	T_EXPECT_EQ(wrapped_zone_free_call_count, 1, "delegate unguarded");
}

T_DECL(size, "size is rounded up to multiple of 16", T_META_TAG_VM_PREFERRED)
{
	setup();
	size_t requested_sizes[] = {0, 1, 16, 17, 32, 33, 48, 49};
	size_t expected_sizes[] = {16, 16, 16, 32, 32, 48, 48, 64};
	uint32_t count = sizeof(requested_sizes) / sizeof(requested_sizes[0]);

	for (uint32_t i = 0; i < count; i++) {
		void *ptr = CALL(malloc, requested_sizes[i]);
		size_t size = CALL(size, ptr);
		T_EXPECT_EQ(size, expected_sizes[i], "size %lu", size);
	}
}

static boolean_t
is_aligned(void *ptr, uintptr_t alignment)
{
	return ((uintptr_t)ptr % alignment) == 0;
}

T_DECL(alignment, "alignments", T_META_TAG_VM_PREFERRED)
{
	wrapped_zone.memalign = malloc_default_zone()->memalign;
	setup();
	wrapped_zone.memalign = NULL;

	T_EXPECT_TRUE(is_aligned(CALL(malloc, size), 16), "malloc(): 16-byte aligned");
	T_EXPECT_TRUE(is_aligned(CALL(valloc, size), PAGE_SIZE), "valloc(): page size aligned");
	T_EXPECT_TRUE(is_aligned(CALL(memalign, sizeof(void *), size), 16), "memalign(sizeof(void *)): 16-byte aligned");
	T_EXPECT_TRUE(is_aligned(CALL(memalign,  8, size), 16), "memalign( 8): 16-byte aligned");
	T_EXPECT_TRUE(is_aligned(CALL(memalign, 16, size), 16), "memalign(16): 16-byte aligned");
	T_EXPECT_TRUE(is_aligned(CALL(memalign, 32, size), 32), "memalign(32): 32-byte aligned");
}

static uint8_t *wrapped_zone_memalign_ret_value;
static void *
wrapped_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size)
{
	return ++wrapped_zone_memalign_ret_value;
}

T_DECL(memalign_invalid_alignment, "memalign delegates for invalid alignment", T_META_TAG_VM_PREFERRED)
{
	wrapped_zone.memalign = wrapped_zone_memalign;
	setup();

	T_EXPECT_EQ(CALL(memalign, 2 * PAGE_SIZE, size), (void *)1, "alignment > page size");
	T_EXPECT_EQ(CALL(memalign, 32+16, size), (void *)2, "alignment not a power of 2");
	T_EXPECT_EQ(CALL(memalign, sizeof(void *) / 2, size), (void *)3, "alignment < sizeof(void *)");
}

static void *
wrapped_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size)
{
	return (void *)7;
}

T_DECL(calloc_overflow, "calloc delegates on overflow", T_META_TAG_VM_PREFERRED)
{
	setup();
	wrapped_zone.calloc = wrapped_zone_calloc;

	size_t size = 100;
	size_t num_items = (SIZE_MAX / size) + 1;
	T_EXPECT_LE(num_items * size, (size_t)PAGE_SIZE, "overflown total size fits into slot");

	T_EXPECT_EQ(CALL(calloc, num_items, size), (void *)7, "delegated call");
}

static bool
is_zeroed_page(uint8_t *ptr)
{
	T_QUIET; T_EXPECT_EQ((vm_address_t)ptr & PAGE_MASK, 0ul, "page aligned");
	for (uint32_t i = 0; i < PAGE_SIZE; i++) {
		if (ptr[i] != 0) return false;
	}
	return true;
}

T_DECL(calloc_zeroed_memory, "calloc provides zeroed memory",
		T_META_ENVVAR("MallocProbGuardSlots=2"),
		T_META_ENVVAR("MallocProbGuardMetadata=2"),
		T_META_ENVVAR("MallocProbGuardAllocations=1"),
		T_META_TAG_VM_PREFERRED)
{
	setup();

	void *ptr1 = CALL(malloc, PAGE_SIZE);
	memset(ptr1, /*value=*/7, PAGE_SIZE);
	T_EXPECT_FALSE(is_zeroed_page(ptr1), "dirty page");
	CALL(free, ptr1);

	void *ptr2 = CALL(calloc, 1, PAGE_SIZE);
	T_EXPECT_NE(ptr2, ptr1, "different page");
	T_EXPECT_TRUE(is_zeroed_page(ptr2), "zeroed page");
	CALL(free, ptr2);

	void *ptr3 = CALL(calloc, 1, PAGE_SIZE);
	T_EXPECT_EQ(ptr3, ptr1, "same page");
	T_EXPECT_TRUE(is_zeroed_page(ptr3), "zeroed page");
}

T_DECL(realloc_null_pointer, "realloc forwards to malloc for null pointers", T_META_TAG_VM_PREFERRED)
{
	setup();
	wrapped_zone.malloc = wrapped_zone_malloc;
	should_sample_counter_ret_value = FALSE;

	T_EXPECT_EQ(CALL(realloc, NULL, size), (void *)1, "forwarded to malloc");
}

static void *
wrapped_zone_realloc(malloc_zone_t *zone, void *ptr, size_t new_size)
{
	T_EXPECT_EQ(ptr, (void *)8, "zone_realloc(): ptr");
	return (void *)9;
}

T_DECL(realloc_unguarded_and_unsampled, "realloc only delegates for old-unguarded and new-unsampled combination",
		T_META_TAG_VM_PREFERRED)
{
	setup();
	wrapped_zone.realloc = wrapped_zone_realloc;
	should_sample_counter_ret_value = FALSE;

	T_EXPECT_EQ(CALL(realloc, (void *)8, size), (void *)9, "delegated call");
}

T_DECL(batch_malloc, "batch_malloc implementation",
		T_META_ENVVAR("MallocProbGuardAllocations=2"),
		T_META_TAG_VM_PREFERRED)
{
	setup();
	wrapped_zone.malloc = wrapped_zone_malloc;

	T_EXPECT_EQ(CALL(batch_malloc, size, NULL, /*num_requested=*/0), 0, "zero count");
	T_EXPECT_EQ(should_sample_counter_call_count, 0, "early return for zero count");

	void *results[3];
	T_EXPECT_EQ(CALL(batch_malloc, size, results, 3), 3, "3 allocations");
	T_EXPECT_EQ(should_sample_counter_call_count, 2, "2 PGM allocations, then quarantine is full");
	T_EXPECT_EQ(wrapped_zone_malloc_call_count, 1, "3rd allocation from wrapped zone");
	T_EXPECT_TRUE(is_guarded(zone, (vm_address_t)results[0]), "PGM allocation");
	T_EXPECT_TRUE(is_guarded(zone, (vm_address_t)results[1]), "PGM allocation");
	T_EXPECT_EQ(results[2], (void *)1, "Wrapped zone allocation");
}

T_DECL(batch_free, "batch_free implementation", T_META_TAG_VM_PREFERRED)
{
	setup();
	wrapped_zone.free = wrapped_zone_free;

	void *to_be_freed[] = {(void *)1, CALL(malloc, size), (void *)3};
	// Freed in reverse order
	wrapped_zone_free_expected_ptrs[0] = (void *)3;
	wrapped_zone_free_expected_ptrs[1] = (void *)1;

	T_EXPECT_EQ(zone->num_allocations, 1, "1 PGM allocation");
	CALL(batch_free, to_be_freed, 3);
	T_EXPECT_EQ(zone->num_allocations, 0, "Freed the PGM allocation");
	T_EXPECT_EQ(wrapped_zone_free_call_count, 2, "delegate unguarded");
}

static vm_range_t expected_ranges[2];
static uint32_t range_recorder_call_count;
static void
range_recorder(task_t task, void *context, unsigned type, vm_range_t *range, unsigned count)
{
	T_QUIET; T_ASSERT_LT(range_recorder_call_count, 2, NULL);
	vm_range_t *expected_range = &expected_ranges[range_recorder_call_count++];

	T_QUIET; T_EXPECT_EQ(task, mach_task_self(), NULL);
	T_QUIET; T_EXPECT_EQ(context, (void *)42, NULL);
	T_QUIET; T_EXPECT_EQ(type, MALLOC_PTR_IN_USE_RANGE_TYPE, NULL);
	T_EXPECT_EQ(range->address, expected_range->address, "block: %lu", expected_range->address);
	T_QUIET; T_EXPECT_EQ(range->size, expected_range->size, NULL);
	T_QUIET; T_EXPECT_EQ(count, 1, NULL);
}

T_DECL(introspection_enumerator, "In-process block enumeration", T_META_TAG_VM_PREFERRED)
{
	setup();
	expected_ranges[0] = (vm_range_t){(vm_address_t)CALL(malloc,  7), 16};
	expected_ranges[1] = (vm_range_t){(vm_address_t)CALL(malloc, 17), 32};

	kern_return_t ret = CALL_(introspect->enumerator, mach_task_self(), (void *)42, MALLOC_PTR_IN_USE_RANGE_TYPE,
			(vm_address_t)zone, /*reader=*/NULL, range_recorder);
	T_EXPECT_EQ(ret, KERN_SUCCESS, "Enumeration successful");
	T_EXPECT_EQ(range_recorder_call_count, 2, NULL);
}
