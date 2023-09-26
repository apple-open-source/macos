#include <stdlib.h>
#include <darwintest.h>
#include <malloc_private.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_XZONE);

T_DECL(malloc_checkfix_zero_on_free, "Test malloc_zero_on_free_disable() SPI",
		T_META_ENVVAR("MallocZeroOnFree=1"))
{
	// Drive some activity up front
	void *p1 = malloc(16);
	T_ASSERT_NOTNULL(p1, "malloc 1");
	void *p2 = malloc(512);
	T_ASSERT_NOTNULL(p1, "malloc 2");

	free(p2);

	// Call the checkfix SPI
	malloc_zero_on_free_disable();

	// Drive some more activity
	void *p3 = calloc(1, 512);
	T_ASSERT_NOTNULL(p3, "calloc 1");

	free(p3);
	free(p1);

	T_PASS("Reached the end");
}

static void
assert_all_zero(char *allocation, size_t size)
{
	for (size_t i = 0; i < size; i++) {
		T_QUIET; T_ASSERT_EQ(allocation[i], '\0', "byte %zu should be 0", i);
	}
}

T_DECL(malloc_zone_batch_zero_on_free, "malloc_zone_batch_free must zero-on-free",
		T_META_ENVVAR("MallocZeroOnFree=1"),
		T_META_ENVVAR("MallocCheckZeroOnFreeCorruption=1"),
		T_META_ENVVAR("MallocNanoZone=0"))
{
	const int n = 3;
	const size_t size = 272;
	void *allocations[n];
	for (int i = 0; i < n; i++) {
		allocations[i] = malloc(size);
		T_QUIET; T_ASSERT_NOTNULL(allocations[i], "malloc()");
		memset(allocations[i], 'a', size);
	}

	malloc_zone_batch_free(malloc_default_zone(), allocations, n);

	char *allocation = calloc(1, size);
	T_QUIET; T_ASSERT_NOTNULL(allocation, "calloc()");

	assert_all_zero(allocation, size);

	free(allocation);

	T_PASS("Successful calloc after batch free");
}

static void
check_zeroing_mode(void)
{
	// Note: normally we would T_ASSERT that malloc returned non-NULL pointers,
	// but that may trigger undesired allocations that would interfere with the
	// intended sequence, so we'll just let them crash instead if that happens

	// Exercise nano support (may still be tiny if !CONFIG_NANOZONE)
	const size_t nano_alloc_size = 16;
	void *p1 = malloc(nano_alloc_size);
	memset(p1, 'a', nano_alloc_size);
	void *p2 = malloc(nano_alloc_size);
	memset(p2, 'b', nano_alloc_size);

	free(p1);
	p1 = malloc(nano_alloc_size); // we probably got the old p1 back
	// Regardless of whether or not we got the old one, we should be guaranteed
	// by zero-on-alloc mode that the allocation is zero-filled
	assert_all_zero(p1, nano_alloc_size);
	free(p1);
	free(p2);

	// Exercise tiny support
	const size_t tiny_alloc_size = 320;
	p1 = malloc(tiny_alloc_size);
	memset(p1, 'c', tiny_alloc_size);
	p2 = malloc(tiny_alloc_size);
	memset(p2, 'd', tiny_alloc_size);

	free(p1);
	p1 = malloc(tiny_alloc_size); // we probably got the old p1 back
	// Regardless of whether or not we got the old one, we should be guaranteed
	// that the allocation is zero-filled
	assert_all_zero(p1, tiny_alloc_size);

	// Also check realloc: p2 is most likely next to p1, so if we free p2 and
	// realloc up p1 it should coalesce.  Regardless of whether or not that
	// happens, we should be guaranteed that it's zero-filled
	free(p2);

	void *p3 = realloc(p1, tiny_alloc_size + 64);
	assert_all_zero(p3, tiny_alloc_size + 64);

	free(p3);

	T_PASS("No issues for zeroing mode");
}

T_DECL(malloc_zero_on_alloc, "Exercise zero-on-alloc mode",
		T_META_ENVVAR("MallocZeroOnAlloc=1"),
		T_META_ENVVAR("MallocNanoZone=1"))
{
	check_zeroing_mode();
}

T_DECL(malloc_zero_on_free, "Exercise zero-on-free mode",
		T_META_ENVVAR("MallocZeroOnFree=1"),
		T_META_ENVVAR("MallocNanoZone=1"))
{
	check_zeroing_mode();
}
