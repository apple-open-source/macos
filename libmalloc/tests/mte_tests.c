#include <darwintest.h>
#include "mte_testing.h"

#if MALLOC_MTE_TESTING_SUPPORTED

#include <stdlib.h>
#include <malloc/malloc.h>
#include <arm_acle.h>
#include <../src/internal.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_NOT_PREFERRED);

/*
 * Given a pointer `__ptr` to an allocation obtained from the system allocator,
 * and a size `__size` that was used to request such allocation, check that:
 * - the pointer has a non-canonical logical tag in bits 56-59
 * - the tag associated to the memory pointed to by `__ptr` is the same as the
 *   logical tag in `__ptr`
 * - the tag associated to the memory pointed to by `__ptr + __size - 1` is the
 *   same as the logical tag in `__ptr`
 */
#define T_CHECK_TAGGED_ALLOCATION(__ptr, __size) __extension__({ \
	uint8_t *s = (uint8_t *)(__ptr); \
	uint8_t *e = (uint8_t *)((uintptr_t)s + ((__size) ?: 1) - 1); \
	T_QUIET; T_ASSERT_TRUE(PTR_IS_TAGGED(s), \
			"Should be tagged (ptr=%p, size=%zu)", __ptr, (size_t)__size); \
	T_QUIET; T_ASSERT_EQ(PTR_EXTRACT_TAG(s), \
			PTR_EXTRACT_TAG(__arm_mte_get_tag(s)), \
			"Should match stored tag (start)"); \
	T_QUIET; T_ASSERT_EQ(PTR_EXTRACT_TAG(s), \
			PTR_EXTRACT_TAG(__arm_mte_get_tag(e)), \
			"Should match stored tag (end)"); \
})

#define T_CHECK_CANONICALLY_TAGGED_ALLOCATION(__ptr, __size) __extension__({ \
	uint8_t *s = (uint8_t *)(__ptr); \
	uint8_t *e = (uint8_t *)((uintptr_t)s + ((__size) ?: 1) - 1); \
	T_QUIET; T_ASSERT_TRUE(!PTR_IS_TAGGED(s), \
			"Should be canonically tagged (ptr=%p, size=%zu)", \
			__ptr, (size_t)__size); \
	T_QUIET; T_ASSERT_EQ(PTR_EXTRACT_TAG(__arm_mte_get_tag(s)), 0, \
			"Should be canonically tagged (start)"); \
	T_QUIET; T_ASSERT_EQ(PTR_EXTRACT_TAG(__arm_mte_get_tag(e)), 0, \
			"Should be canonically tagged (end)"); \
})

// FIXME: I feel like this is quite fragile for testing and might quietly pass
// tests, for example when
// * xzone impl under test has bug
// * we fail to exhaust mfm, so the test never hits the bug
//
// Maybe we can make this more precise by having a way for tests to setup
// _xzm_xzone_try_reserve_early_budget() to return false for xzone tests
// And for mfm tests we should have a way to write assert(is block from mfm)
static void exhaust_early_budget(size_t lower_bound, size_t upper_bound,
	malloc_type_id_t type_id)
{
	// Exhaust the early allocator budget for all sizes
	T_LOG("Exhausting early allocator budget");
	size_t sz = lower_bound;
	const size_t early_alloc_max = MIN(8192, upper_bound);
	while (sz <= early_alloc_max) {
		void *a = NULL;
		for (size_t i = 0; i < 256; i++) {
			a = malloc_type_malloc(sz, type_id);
			if (!a) {
				T_ASSERT_NOTNULL(a, "early malloc");
			}
			free(a);
		}

		a = malloc_type_malloc(sz, type_id);
		sz = malloc_size(a) + 1;
		free(a);
	}
}

static const malloc_type_id_t k_ptr_type_id =
		__builtin_tmo_get_type_descriptor(void *);
static const malloc_type_id_t k_data_type_id =
		__builtin_tmo_get_type_descriptor(char[16]);

static void
_check_early_allocations(bool expects_tagged) {
	const size_t alloc_count = 16;
	void *allocations[alloc_count] = {};
	uintptr_t address;
	size_t tagged_allocations = 0;

	for (size_t i = 0; i < alloc_count; i++) {
		allocations[i] = malloc(16);
		tagged_allocations += PTR_IS_TAGGED(allocations[i]);
		if (expects_tagged) {
			T_CHECK_TAGGED_ALLOCATION(allocations[i], 16);
		}
	}

	for (size_t i = 0; i < alloc_count; i++) {
		free(allocations[i]);
	}

	if (expects_tagged) {
		T_QUIET; T_EXPECT_EQ(tagged_allocations, alloc_count,
			"expected all allocations to be non-canonically tagged");
	} else {
		T_QUIET; T_EXPECT_EQ(tagged_allocations, 0ul,
			"expected no tagged allocations");
	}
}


T_DECL(mte_check_early_alloc_default,
	"Check MFM supports tagged allocations by default",
	T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	_check_early_allocations(true);
	T_PASS("Early allocator allocations are tagged by default");
}

T_DECL(mte_check_early_alloc_disabled, "Check MFM support can be disabled",
	T_META_ENVVAR("MallocEarlyMallocSecTransitionSupport=0"),
	T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	_check_early_allocations(false);
	T_PASS("Tagging can be disabled in the early allocator");
}

typedef enum {
	MMI_FREE,
	MMI_MALLOC_ZONE_FREE,
	MMI_REALLOC,
} mismatch_interface_t;

static void
_check_mismatching_tag(bool test_early_alloc, mismatch_interface_t interface)
{
	const malloc_type_id_t descr = __builtin_tmo_get_type_descriptor(void *);
	const size_t max_sz = 32768;
	for (size_t alloc_sz = 1; alloc_sz <= max_sz; alloc_sz <<= 1) {
		T_LOG("alloc_sz = %zu", alloc_sz);

		pid_t child_pid = fork();
		T_QUIET; T_ASSERT_NE(child_pid, -1, "fork()");

		if (child_pid == 0) {
			uint8_t *allocation = NULL;

			if (!test_early_alloc) {
				// If we don't want to exercise the early allocator,
				// exhaust this size/type from its budget.
				for (int i = 0; i < 1000; i++) {
					allocation = malloc_type_malloc(alloc_sz, descr);
					if (!allocation) {
						T_ASSERT_NOTNULL(allocation, "early malloc");
					}
					free(allocation);
				}
			}

			allocation = malloc_type_malloc(alloc_sz, descr);
			T_QUIET; T_ASSERT_TRUE(PTR_IS_TAGGED(allocation),
				"Should be non-canonically tagged (sz=%zu, %p)",
				alloc_sz, allocation);
			uint64_t mask = __arm_mte_exclude_tag(allocation, 0x0001);
			uint8_t *modified = __arm_mte_create_random_tag(allocation, mask);
			T_QUIET; T_ASSERT_TRUE(PTR_IS_TAGGED(modified), "gmi");
			T_QUIET; T_ASSERT_NE_PTR(modified, allocation, "irg");

			switch (interface) {
			case MMI_FREE: {
				free(modified);
				T_FAIL("free(%p) Did not crash", modified);
				break;
			}
			case MMI_MALLOC_ZONE_FREE: {
				malloc_zone_free(malloc_default_zone(), modified);
				T_FAIL("malloc_zone_free(%p) Did not crash", modified);
				break;
			}
			case MMI_REALLOC: {
				size_t rsz = malloc_size(allocation) + 1;
				void *r = malloc_type_realloc(modified, rsz, descr);
				T_FAIL("realloc(%p) Did not crash: %p", modified, r);
				break;
			}
			}
		} else {
			int status;
			pid_t wait_pid = waitpid(child_pid, &status, 0);
			T_QUIET; T_ASSERT_EQ(wait_pid, child_pid,
					"Got child status (%d)", status);
			T_QUIET; T_ASSERT_TRUE(WIFSIGNALED(status),
					"Child terminated by signal");
			if (get_sec_transition_config() == SEC_TRANSITION_EMULATED) {
				T_QUIET; T_ASSERT_EQ(status, SIGBUS,
						"Emulated child terminated by plugin");
			} else {
				T_QUIET; T_ASSERT_EQ(status, SIGKILL,
						"Child terminated by a fatal exception");
			}
		}
	}
}

static void
_check_mismatching_tag_free(bool test_early_alloc)
{
	_check_mismatching_tag(test_early_alloc, MMI_FREE);
}

static void
_check_mismatching_tag_malloc_zone_free(bool test_early_alloc)
{
	_check_mismatching_tag(test_early_alloc, MMI_MALLOC_ZONE_FREE);
}

static void
_check_mismatching_tag_realloc(bool test_early_alloc)
{
	_check_mismatching_tag(test_early_alloc, MMI_REALLOC);
}

T_DECL(mte_crash_free_mismatching_tag,
		"Freeing a pointer with a mismatching tag causes a fatal exception",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_free(false);
	T_PASS("Success");
}

T_DECL(mte_crash_free_mismatching_tag_early_alloc,
		"Freeing a pointer with a mismatching tag causes a "
		"fatal exception (early alloc)",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_free(true);
	T_PASS("Success");
}

T_DECL(mte_crash_malloc_zone_free_mismatching_tag,
		"Freeing a pointer with a mismatching tag causes a fatal exception",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_malloc_zone_free(false);
	T_PASS("Success");
}

T_DECL(mte_crash_malloc_zone_free_mismatching_tag_early_alloc,
		"Freeing a pointer with a mismatching tag causes a "
		"fatal exception (early alloc)",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_malloc_zone_free(true);
	T_PASS("Success");
}

T_DECL(mte_crash_realloc_mismatching_tag,
		"Reallocating a pointer with a mismatching tag causes a "
		"fatal exception",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_realloc(false);
	T_PASS("Success");
}

T_DECL(mte_crash_realloc_mismatching_tag_early_alloc,
		"Reallocating a pointer with a mismatching tag causes a "
		"fatal exception (early alloc)",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_realloc(true);
	T_PASS("Success");
}

T_DECL(mte_crash_free_mismatching_tag_no_sanitizers_traces,
		"Freeing a pointer with a mismatching tag causes a "
		"fatal exception (no sanitizer traces)",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_DISABLE_SANITIZERS_TRACES,
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_free(false);
	T_PASS("Success");
}

T_DECL(mte_crash_free_mismatching_tag_early_alloc_no_sanitizers_traces,
		"Freeing a pointer with a mismatching tag causes a "
		"fatal exception (early alloc) (no sanitizer traces)",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_DISABLE_SANITIZERS_TRACES,
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_free(true);
	T_PASS("Success");
}

T_DECL(mte_crash_malloc_zone_free_mismatching_tag_no_sanitizers_traces,
		"Freeing a pointer with a mismatching tag causes a "
		"fatal exception (no sanitizer traces)",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_DISABLE_SANITIZERS_TRACES,
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_malloc_zone_free(false);
	T_PASS("Success");
}

T_DECL(mte_crash_malloc_zone_free_mismatching_tag_early_alloc_no_sanitizers_traces,
		"Freeing a pointer with a mismatching tag causes a "
		"fatal exception (early alloc) (no sanitizer traces)",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_DISABLE_SANITIZERS_TRACES,
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_malloc_zone_free(true);
	T_PASS("Success");
}

T_DECL(mte_crash_realloc_mismatching_tag_no_sanitizers_traces,
		"Reallocating a pointer with a mismatching tag causes a "
		"fatal exception (no sanitizer traces)",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_DISABLE_SANITIZERS_TRACES,
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_realloc(false);
	T_PASS("Success");
}

T_DECL(mte_crash_realloc_mismatching_tag_early_alloc_no_sanitizers_traces,
		"Reallocating a pointer with a mismatching tag causes a "
		"fatal exception (early alloc) (no sanitizer traces)",
		T_META_IGNORECRASHES("mte_tests"),
		T_META_TAG_DISABLE_SANITIZERS_TRACES,
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_mismatching_tag_realloc(true);
	T_PASS("Success");
}

static void
_check_malloc_size_invalid_pointers()
{
	int p;
	void *stack_ptr = &p;
	void *arbitrary = (void *)0x804080546fe24ed0;

	T_EXPECT_EQ(malloc_size(stack_ptr), 0ul, "malloc_size(stack_ptr)");
	T_EXPECT_EQ(malloc_size(arbitrary), 0ul, "malloc_size(arbitrary)");
}

T_DECL(mte_no_crash_malloc_size_invalid_ptr,
		"Calling malloc_size() on invalid pointers should not crash",
		T_META_TAG_DISABLE_SANITIZERS_TRACES,
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_malloc_size_invalid_pointers();
	T_PASS("Success");
}

T_DECL(mte_no_crash_malloc_size_invalid_ptr_no_sanitizers_traces,
		"Calling malloc_size() on invalid pointers should not crash",
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_check_malloc_size_invalid_pointers();
	T_PASS("Success");
}

T_DECL(mte_all_sizes_tagged, "Check that all expected sizes are tagged",
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	const size_t min_sz = 16;
	const size_t max_sz = 32768;
	exhaust_early_budget(min_sz, max_sz, k_ptr_type_id);

	size_t size = 0;
	while (size <= max_sz) {
		T_LOG("size %zu", size);

		void *a = malloc_type_malloc(size, k_ptr_type_id);
		T_CHECK_TAGGED_ALLOCATION(a, size);

		size_t block_sz = malloc_size(a);
		// Test the upper bound explicitly.
		size = (size < block_sz && block_sz == max_sz) ? max_sz : block_sz + 1;
		free(a);
	}

	T_PASS("All sizes tagged correctly");
}

/*
 * After exhausting the budget for the early allocator, this test will loop
 * through all the xzone size classes (by querying them using malloc_size()),
 * and for each size class it will:
 * - request an allocation, and check that it is properly tagged
 * - resize that allocation within the same size class, and check that when
 *   realloc does not move the allocation, it also does not get retagged
 * - shrink the allocation down to the smaller size class, and check that it is
 *   getting retagged
 * - grow the allocation from the smaller size class to the one above the
 *   original allocation, and check that it is properly tagged
 */
T_DECL(mte_realloc_shrink_grow,
		"Check that tagging works as expected when resizing through realloc",
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	const size_t min_sz = 16;
	const size_t max_sz = 32768;
	exhaust_early_budget(min_sz, max_sz, k_ptr_type_id);

	size_t size = min_sz;
	size_t prev_size = size;
	while (size <= max_sz) {
		void *a = malloc_type_malloc(size, k_ptr_type_id);
		size_t effective_size = malloc_size(a);
		size_t stable_realloc_size = effective_size - 8;
		size_t next_size = MIN(max_sz, effective_size + 1);
		T_QUIET; T_ASSERT_NOTNULL(a, "valid initial allocation (%zu)", size);
		T_LOG("Resize: %zu -> %zu -> %zu -> %zu", size, stable_realloc_size,
				prev_size, next_size);

		// The initial allocation we obtain should be tagged.
		T_CHECK_TAGGED_ALLOCATION(a, size);

		// Resize the allocation through realloc, within the same size class.
		void *r = malloc_type_realloc(a, stable_realloc_size, k_ptr_type_id);
		T_QUIET; T_ASSERT_NOTNULL(r, "realloc s (%zu -> %zu): %p",
				size, stable_realloc_size, r);
		// Verify the tags of the resized allocation.
		T_CHECK_TAGGED_ALLOCATION(r, stable_realloc_size);

		// It should not have moved, and it should not have been retagged.
		T_QUIET; T_ASSERT_EQ_PTR(PTR_STRIP_TAG(a), PTR_STRIP_TAG(r),
				"Should not move");
		T_QUIET; T_ASSERT_EQ(PTR_EXTRACT_TAG(a), PTR_EXTRACT_TAG(r),
				"Should have the same tag");

		// Shrink the allocation down to the previous size class.
		void *r1 = malloc_type_realloc(r, stable_realloc_size, k_ptr_type_id);
		T_QUIET; T_ASSERT_NOTNULL(r1, "realloc 1 (%zu -> %zu): %p",
				size, prev_size, r1);
		T_CHECK_TAGGED_ALLOCATION(r1, prev_size);

		// Grow the allocation up to the larger size class.
		void *r2 = malloc_type_realloc(r1, next_size, k_ptr_type_id);
		T_QUIET; T_ASSERT_NOTNULL(r2, "realloc 2 (%zu -> %zu): %p",
				prev_size, next_size, r2);
		T_CHECK_TAGGED_ALLOCATION(r2, next_size);
		free(r2);

		prev_size = size;
		size = effective_size + 1;
	}
	T_PASS("All growing/shrinking reallocations are tagged correctly");
}

T_DECL(mte_realloc_cross_range,
		"Check that reallocating between tagging ranges works",
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	const size_t tagged_sz = 10240;
	const size_t not_tagged_sz = 65536;

	// We don't need to exhaust the early budget, since it's only used for
	// allocations <= 8192 bytes.
	void *a = malloc_type_malloc(tagged_sz, k_ptr_type_id);
	T_QUIET; T_ASSERT_NOTNULL(a, "valid initial allocation (%zu)", tagged_sz);
	T_CHECK_TAGGED_ALLOCATION(a, tagged_sz);

	// Resize the allocation through realloc, moving it outside of the
	// tagged range.
	void *r1 = malloc_type_realloc(a, not_tagged_sz, k_ptr_type_id);
	T_QUIET; T_ASSERT_NOTNULL(r1, "valid reallocation (%zu)", not_tagged_sz);
	T_ASSERT_FALSE(PTR_IS_TAGGED(r1), "Should not be tagged: %zu",
			not_tagged_sz);

	// Now resize it back into the tagged range.
	void *r2 = malloc_type_realloc(r1, tagged_sz, k_ptr_type_id);
	T_QUIET; T_ASSERT_NOTNULL(r2, "valid reallocation (%zu)", tagged_sz);
	T_ASSERT_TRUE(PTR_IS_TAGGED(r2), "Should be tagged: %zu", tagged_sz);
	T_CHECK_TAGGED_ALLOCATION(r2, tagged_sz);

	free(r2);
	T_PASS("Reallocation across tagging ranges works correctly");
}

static void
_test_no_tag_range(size_t min_sz, size_t max_sz, malloc_type_id_t type_id,
	bool test_early_alloc)
{
	if (!test_early_alloc) {
		exhaust_early_budget(min_sz, max_sz, type_id);
	}

	size_t sz = min_sz;
	while (sz <= max_sz) {
		char *a = (char *)malloc_type_malloc(sz, type_id);
		char *e = a + sz - 1;
		T_QUIET; T_ASSERT_NOTNULL(a, "valid allocation: %p", a);
		T_QUIET; T_ASSERT_FALSE(PTR_IS_TAGGED(a),
				"Should not be tagged: %zu", sz);
		T_QUIET; T_ASSERT_EQ_PTR(a, __arm_mte_get_tag(a),
				"Memory should not be tagged (start): %zu", sz);
		T_QUIET; T_ASSERT_EQ_PTR(e, __arm_mte_get_tag(e),
				"Memory should not be tagged (end): %zu", sz);

		if (sz <= 32768) {
			sz = malloc_size(a) + 1;
		} else {
			sz = sz + (4 * PAGE_SIZE);
		}
		free(a);
	}
}

static void
_test_no_tag_data(bool test_early_alloc)
{
	const size_t min_sz = 16;
	const size_t max_sz = 32768;

	malloc_type_descriptor_t data_desc = {.type_id = k_data_type_id};
	T_QUIET;
	T_ASSERT_TRUE(malloc_type_descriptor_is_pure_data(data_desc), "descriptor");

	_test_no_tag_range(min_sz, max_sz, k_data_type_id, test_early_alloc);
}

T_DECL(mte_no_tag_data,
		"Check that we do not tag data allocations",
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_test_no_tag_data(false);
	T_PASS("Data allocations are not being tagged in xzone");
}

T_DECL(mte_no_tag_data_early_alloc,
		"Check that we do not tag data allocations (early alloc)",
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();
	_test_no_tag_data(true);
	T_PASS("Data allocations are not being tagged in the early allocator");
}

T_DECL(mte_no_tag_over_32K,
		"Check that we do not tag allocations above 32K",
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	const size_t min_sz = 32768 + 1;
	const size_t max_sz = (1 << 22);

	_test_no_tag_range(min_sz, max_sz, k_ptr_type_id, false);
	T_PASS("Allocations > 32K are not being tagged");
}

// When instructed, we tag all allocations.  Meaning we additionally tag:
//  * Pure data allocations
//  * Allocations >= 32 kB
T_DECL(mte_tag_all,
		"Check that we tag all allocations in Debug mode",
		T_META_ENVVAR("MallocTagAllInternal=1"),
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	const size_t early_size = 16;

	// Tagging in early allocator in Debug mode
	void *data = malloc_type_malloc(16, k_data_type_id);
	void *typed = malloc_type_malloc(16, k_ptr_type_id);
	T_CHECK_TAGGED_ALLOCATION(data, early_size);
	T_CHECK_TAGGED_ALLOCATION(typed, early_size);
	free(data);
	free(typed);

	const size_t min_sz = 16;
	// TODO: support for tagging larger allocations
	const size_t max_sz = 32768;

	exhaust_early_budget(min_sz, max_sz, k_data_type_id);
	exhaust_early_budget(min_sz, max_sz, k_ptr_type_id);

	size_t sz = min_sz;
	while (sz <= max_sz) {
		data = malloc_type_malloc(sz, k_data_type_id);
		typed = malloc_type_malloc(sz, k_ptr_type_id);
		T_CHECK_TAGGED_ALLOCATION(data, sz);
		T_CHECK_TAGGED_ALLOCATION(typed, sz);

		sz = malloc_size(data) + 1;

		free(data);
		free(typed);
	}

	T_PASS("All allocations are tagged in Debug mode");
}

T_DECL(mte_tag_all_canonical_tagging,
		"Check that canonical tagging requests are respected in Debug mode",
		T_META_ENVVAR("MallocTagAllInternal=1"),
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	const size_t align = MALLOC_ZONE_MALLOC_DEFAULT_ALIGN;
	const size_t early_size = 16;
	const malloc_zone_malloc_options_t options =
			MALLOC_ZONE_MALLOC_OPTION_CANONICAL_TAG;

	// Canonical tagging in early allocator in Debug mode
	void *data = malloc_type_zone_malloc_with_options(
			NULL, align, early_size, k_data_type_id, options);
	void *typed = malloc_type_zone_malloc_with_options(
			NULL, align, early_size, k_ptr_type_id, options);
	T_CHECK_CANONICALLY_TAGGED_ALLOCATION(data, early_size);
	T_CHECK_CANONICALLY_TAGGED_ALLOCATION(typed, early_size);
	free(data);
	free(typed);

	const size_t min_sz = 16;
	const size_t max_sz = MiB(8); // Stand-in for max malloc() size

	exhaust_early_budget(min_sz, max_sz, k_data_type_id);
	exhaust_early_budget(min_sz, max_sz, k_ptr_type_id);

	size_t sz = min_sz;
	while (sz <= max_sz) {
		data = malloc_type_zone_malloc_with_options(
				NULL, align, sz, k_data_type_id, options);
		typed = malloc_type_zone_malloc_with_options(
				NULL, align, sz, k_ptr_type_id, options);
		T_CHECK_CANONICALLY_TAGGED_ALLOCATION(data, sz);
		T_CHECK_CANONICALLY_TAGGED_ALLOCATION(typed, sz);

		sz = malloc_size(data) + 1;

		free(data);
		free(typed);
	}

	T_PASS("Canonical tagging requests are respected in Debug mode");
}


static void
_test_chunk_init_retag(size_t blk_sz, size_t num_blocks)
{
	void **blocks = (void **)calloc(num_blocks, sizeof(void *));
	size_t blocks_to_free = arc4random_uniform(num_blocks / 4);

	// Allocate the blocks
	for (size_t i = 0; i < num_blocks; i++) {
		blocks[i] = malloc_type_malloc(blk_sz, k_ptr_type_id);
		T_QUIET; T_ASSERT_NOTNULL(blocks[i], "Block allocated");
		T_QUIET; T_ASSERT_TRUE(PTR_IS_TAGGED(blocks[i]),
				"Block should be tagged");
	}

	// Free a random number of blocks, at random positions
	for (size_t i = 0; i < blocks_to_free; i++) {
		size_t idx_to_free = 0;
		do {
			idx_to_free = arc4random_uniform(num_blocks);
		} while (blocks[idx_to_free] == NULL);
		free(blocks[idx_to_free]);
		blocks[idx_to_free] = NULL;
	}

	for (size_t i = 0; i < num_blocks; i++) {
		// Examine only the blocks we haven't freed.
		void *block = blocks[i];
		if (block == NULL)
			continue;

		uintptr_t block_addr = (uintptr_t)block;
		uintptr_t left_addr = block_addr - 16;
		size_t actual_blk_sz = malloc_size(block);
		uintptr_t right_addr = block_addr + actual_blk_sz;
		uintptr_t block_page_start = trunc_page(block_addr);
		uintptr_t block_page_end = trunc_page(block_addr + actual_blk_sz - 1);
		uintptr_t left_page = trunc_page(left_addr);
		uintptr_t right_page = trunc_page(right_addr);
		void *block_tag = __arm_mte_get_tag(block);
		void *left_tag = NULL;
		void *right_tag = NULL;

		T_QUIET; T_ASSERT_EQ_PTR(block, block_tag,
			"Materialized tag should match the logical tag");
		if (left_page == block_page_start) {
			left_tag = __arm_mte_get_tag((void *)left_addr);
			T_QUIET; T_ASSERT_NE(PTR_EXTRACT_TAG(block_tag),
					PTR_EXTRACT_TAG(left_tag),
					"Left block should have a different tag");
		}
		if (right_page == block_page_end) {
			right_tag = __arm_mte_get_tag((void *)right_addr);
			T_QUIET; T_ASSERT_NE(PTR_EXTRACT_TAG(block_tag),
					PTR_EXTRACT_TAG(right_tag),
					"Right block should have a different tag");
		}

		// Free the allocation, causing it to be retagged.
		free(block);
		asm volatile("" ::: "memory");
		// Read the tag back from the block.
		block_tag = __arm_mte_get_tag(block);

		T_QUIET; T_ASSERT_NE_PTR(block, block_tag,
				"Block should have been retagged with a different tag");

		if (left_page == block_page_start && PTR_EXTRACT_TAG(left_tag) != 0) {
			T_QUIET; T_ASSERT_NE(PTR_EXTRACT_TAG(block_tag),
					PTR_EXTRACT_TAG(left_tag),
					"Retagged with a tag different from the left block");
		}
		if (right_page == block_page_end && PTR_EXTRACT_TAG(right_tag) != 0) {
			T_QUIET; T_ASSERT_NE(PTR_EXTRACT_TAG(block_tag),
					PTR_EXTRACT_TAG(right_tag),
					"Retagged with a tag different from the right block");
		}
	}

	free(blocks);
}

T_DECL(mte_chunk_init_and_retagging,
		"Chunk initialization and retagging",
		T_META_TAG_XZONE_ONLY)
{
	T_SKIP_REQUIRES_SEC_TRANSITION_HARDWARE();

	const size_t min_blk_sz = 32;
	const size_t max_blk_sz = 1024;
	const size_t num_blocks = 256;

	exhaust_early_budget(min_blk_sz, max_blk_sz, k_ptr_type_id);

	for (size_t blk_sz = min_blk_sz; blk_sz < max_blk_sz; blk_sz += 16) {
		_test_chunk_init_retag(blk_sz, num_blocks);
	}

	T_PASS("Chunk initialization and retagging works");
}

#elif !MALLOC_TARGET_EXCLAVES

T_DECL(mte_unsupported_target, "Skip testing on unsupported targets",
		T_META_TAG_VM_PREFERRED, T_META_TAG_NO_ALLOCATOR_OVERRIDE)
{
	T_SKIP("MTE tests are only implemented for arm64 targets");
}

#endif // MALLOC_MTE_TESTING_SUPPORTED
