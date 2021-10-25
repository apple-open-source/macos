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

#include "../src/has_section.c"
#include "../src/pgm_malloc.c"
#include "../src/stack_trace.c"

// Stub out cross-file dependencies.
void malloc_report(uint32_t flags, const char *fmt, ...) { }
void malloc_report_simple(const char *fmt, ...) { __builtin_trap(); }

static malloc_zone_t wrapped_zone;
static pgm_zone_t *zone;
static size_t size = 5;

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
	// rdar://74948496 ([PGM] Drop all requirements for wrapped_zone)
	wrapped_zone.version = 6;
	wrapped_zone.batch_malloc = malloc_default_zone()->batch_malloc;
	wrapped_zone.batch_free = malloc_default_zone()->batch_free;
	wrapped_zone.memalign = malloc_default_zone()->memalign;
	wrapped_zone.free_definite_size =  malloc_default_zone()->free_definite_size;
	wrapped_zone.calloc = malloc_default_zone()->calloc;
	zone = (pgm_zone_t *)pgm_create_zone(&wrapped_zone);
	wrapped_zone.calloc = NULL;
	wrapped_zone.free_definite_size = NULL;
	wrapped_zone.memalign = NULL;
	wrapped_zone.batch_free = NULL;
	wrapped_zone.batch_malloc = NULL;
}

static uint8_t *wrapped_zone_malloc_ret_value;
static void *
wrapped_zone_malloc(malloc_zone_t *zone, size_t size) {
	return ++wrapped_zone_malloc_ret_value;
}

T_DECL(delegate_unsampled, "delegation of unsampled allocations",
		T_META_ENVVAR("MallocProbGuardAllocations=1"))
{
	setup();
	wrapped_zone.malloc = wrapped_zone_malloc;

	T_EXPECT_EQ(CALL(malloc, PAGE_SIZE + 1), (void *)1, "requested size > page size");
	T_EXPECT_EQ(should_sample_counter_call_count, 0, "bad size; no call to should_sample_counter()");

	should_sample_counter_ret_value = FALSE;
	T_EXPECT_EQ(CALL(malloc, size), (void *)2, "not sampled");
	should_sample_counter_ret_value = TRUE;
	T_EXPECT_NOTNULL(CALL(malloc, size), "sampled");
	T_EXPECT_EQ(should_sample_counter_call_count, 2, "should_sample_counter() decides");

	T_QUIET; T_EXPECT_TRUE(is_full(zone), "zone full");
	T_EXPECT_EQ(CALL(malloc, size), (void *)3, "zone full");
	T_EXPECT_EQ(should_sample_counter_call_count, 2, "zone full; no call to should_sample_counter()");
}

static uint32_t wrapped_zone_free_call_count;
static void
wrapped_zone_free(malloc_zone_t *zone, void *ptr)
{
	T_EXPECT_EQ(ptr, (void *)1337, "free(): ptr");
	wrapped_zone_free_call_count++;
}

T_DECL(delegate_unguarded, "delegation of unguarded deallocations")
{
	setup();
	wrapped_zone.free = wrapped_zone_free;

	void *p1 = CALL(malloc, size);
	void *p2 = (void *)1337;
	T_EXPECT_TRUE (is_guarded(zone, (vm_address_t)p1), "guarded");
	T_EXPECT_FALSE(is_guarded(zone, (vm_address_t)p2), "unguarded");

	CALL(free, p1);
	T_EXPECT_EQ(wrapped_zone_free_call_count, 0, "handle guarded");

	CALL(free, p2);
	T_EXPECT_EQ(wrapped_zone_free_call_count, 1, "delegate unguarded");
}

T_DECL(size, "size is rounded up to multiple of 16")
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

T_DECL(alignment, "alignments")
{
	setup();

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

T_DECL(memalign_invalid_alignment, "memalign delegates for invalid alignment")
{
	setup();
	wrapped_zone.memalign = wrapped_zone_memalign;

	T_EXPECT_EQ(CALL(memalign, 2 * PAGE_SIZE, size), (void *)1, "alignment > page size");
	T_EXPECT_EQ(CALL(memalign, 32+16, size), (void *)2, "alignment not a power of 2");
	T_EXPECT_EQ(CALL(memalign, sizeof(void *) / 2, size), (void *)3, "alignment < sizeof(void *)");
}

static void *
wrapped_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size)
{
	return (void *)7;
}

T_DECL(calloc_overflow, "calloc delegates on overflow")
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
		T_META_ENVVAR("MallocProbGuardAllocations=1"))
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

T_DECL(realloc_null_pointer, "realloc forwards to malloc for null pointers")
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

T_DECL(realloc_unguarded_and_unsampled, "realloc only delegates for old-unguarded and new-unsampled combination")
{
	setup();
	wrapped_zone.realloc = wrapped_zone_realloc;
	should_sample_counter_ret_value = FALSE;

	T_EXPECT_EQ(CALL(realloc, (void *)8, size), (void *)9, "delegated call");
}

static void **wrapped_zone_batch_malloc_expected_results;
static uint32_t wrapped_zone_batch_malloc_call_count;
static unsigned
wrapped_zone_batch_malloc(malloc_zone_t *zone, size_t size, void **results, unsigned count)
{
	T_EXPECT_EQ(results, wrapped_zone_batch_malloc_expected_results, "batch_malloc(): results");
	T_EXPECT_EQ(count, 1, "batch_malloc(): count");
	wrapped_zone_batch_malloc_call_count++;
	return 10;
}

T_DECL(batch_malloc, "batch_malloc implementation",
		T_META_ENVVAR("MallocProbGuardAllocations=2"))
{
	setup();
	wrapped_zone.batch_malloc = wrapped_zone_batch_malloc;

	T_EXPECT_EQ(CALL(batch_malloc, size, NULL, /*count=*/0), 0, "zero count");
	T_EXPECT_EQ(should_sample_counter_call_count, 0, "early return for zero count");

	void *results[3];
	wrapped_zone_batch_malloc_expected_results = &results[2];
	T_EXPECT_EQ(CALL(batch_malloc, size, results, 3), 12, "return sampled plus delegated");
	T_EXPECT_EQ(should_sample_counter_call_count, 3, "determine sample count");
	T_EXPECT_EQ(wrapped_zone_batch_malloc_call_count, 1, "delegate unsampled");
}

static uint32_t wrapped_zone_batch_free_call_count;
static void
wrapped_zone_batch_free(malloc_zone_t *zone, void **to_be_freed, unsigned count)
{
	T_EXPECT_EQ(count, 3, "batch_free(): count");
	T_EXPECT_EQ(to_be_freed[0], (void *)1, "batch_free(): to_be_freed[0]");
	T_EXPECT_EQ(to_be_freed[1], NULL, "batch_free(): to_be_freed[1]");
	T_EXPECT_EQ(to_be_freed[2], (void *)3, "batch_free(): to_be_freed[2]");
	wrapped_zone_batch_free_call_count++;
}

T_DECL(batch_free, "batch_free implementation")
{
	setup();
	wrapped_zone.batch_free = wrapped_zone_batch_free;

	void *to_be_freed[] = {(void *)1, CALL(malloc, size), (void *)3};
	CALL(batch_free, to_be_freed, 3);
	T_EXPECT_EQ(wrapped_zone_batch_free_call_count, 1, "delegate unguarded");
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

T_DECL(introspection_enumerator, "In-process block enumeration")
{
	setup();
	expected_ranges[0] = (vm_range_t){(vm_address_t)CALL(malloc,  7), 16};
	expected_ranges[1] = (vm_range_t){(vm_address_t)CALL(malloc, 17), 32};

	kern_return_t ret = CALL_(introspect->enumerator, mach_task_self(), (void *)42, MALLOC_PTR_IN_USE_RANGE_TYPE,
			(vm_address_t)zone, /*reader=*/NULL, range_recorder);
	T_EXPECT_EQ(ret, KERN_SUCCESS, "Enumeration successful");
	T_EXPECT_EQ(range_recorder_call_count, 2, NULL);
}
