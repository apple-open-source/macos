//
//  malloc_size_test.c
//  libmalloc
//
//  Tests for malloc_size() on both good and bad pointers.
//

#include <darwintest.h>
#include <stdlib.h>
#include <malloc/malloc.h>

#include "base.h"

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

static void
test_malloc_size_valid(size_t min, size_t max, size_t incr)
{
	for (size_t sz = min; sz <= max; sz += incr) {
		void *ptr = malloc(sz);
		T_ASSERT_NOTNULL(ptr, "Allocate size %llu\n", (uint64_t)sz);
		T_ASSERT_GE(malloc_size(ptr), sz, "Check size value");
		T_ASSERT_GE(malloc_good_size(sz), sz, "Check good size value");
		free(ptr);
	}
}

static void
test_malloc_size_invalid(size_t min, size_t max, size_t incr)
{
	for (size_t sz = min; sz <= max; sz += incr) {
		void *ptr = malloc(sz);
		T_ASSERT_NOTNULL(ptr, "Allocate size %llu\n", (uint64_t)sz);
		T_ASSERT_EQ(malloc_size(ptr + 1), 0UL, "Check offset by 1 size value");
		T_ASSERT_EQ(malloc_size(ptr + sz/2), 0UL, "Check offset by half size value");
		free(ptr);
	}
}

T_DECL(malloc_size_valid, "Test malloc_size() on valid pointers, non-Nano",
	   T_META_ENVVAR("MallocNanoZone=0"), T_META_TAG_XZONE, T_META_TAG_VM_NOT_PREFERRED)
{
	// Test various sizes, roughly targetting each allocator range.
	test_malloc_size_valid(2, 256, 16);
	test_malloc_size_valid(512, 8192, 256);
	test_malloc_size_valid(8192, 65536, 1024);
}

T_DECL(malloc_size_valid_nanov2, "Test malloc_size() on valid pointers for Nanov2",
	   T_META_ENVVAR("MallocNanoZone=V2"), T_META_TAG_XZONE, T_META_TAG_VM_NOT_PREFERRED)
{
	test_malloc_size_valid(2, 256, 16);
}

T_DECL(malloc_size_invalid, "Test malloc_size() on invalid pointers, non-Nano",
	   T_META_ENVVAR("MallocNanoZone=0"), T_META_TAG_VM_NOT_PREFERRED)
{
	// Test various sizes, roughly targetting each allocator range.
	test_malloc_size_invalid(2, 256, 16);
	test_malloc_size_invalid(512, 8192, 256);
	test_malloc_size_invalid(8192, 32768, 1024);
}

T_DECL(malloc_size_invalid_nanov2, "Test malloc_size() on valid pointers for Nanov2",
	   T_META_ENVVAR("MallocNanoZone=V2"), T_META_TAG_VM_NOT_PREFERRED)
{
	test_malloc_size_invalid(2, 256, 16);
}

// Exclaves doesn't support calling malloc_size() on freed pointers,
// specifically tiny ones, since the pages containing the inline freelist may
// have been depopulated
#if !MALLOC_TARGET_EXCLAVES
T_DECL(malloc_size_invalid_xzone,
		"Test malloc_size() on invalid pointers for xzone",
		T_META_TAG_XZONE_ONLY,
	    T_META_TAG_VM_NOT_PREFERRED)
{
	// Exhaust early budget
	void *ptr = NULL;
	for (int i = 0; i < 128; i++) {
		ptr = malloc(64);
		T_QUIET; T_ASSERT_NOTNULL(ptr, "Early TINY allocation");
		free(ptr);

		ptr = malloc(4097);
		T_QUIET; T_ASSERT_NOTNULL(ptr, "Early SMALL allocation");
		free(ptr);
	}

	// malloc_size should report 0 on freed pointers
	ptr = malloc(64); // TINY allocation
	T_ASSERT_NOTNULL(ptr, "TINY allocation");

	free(ptr);
	T_EXPECT_EQ_ULONG(0UL, malloc_size(ptr), "return 0 for free TINY allocations");

	ptr = malloc(4097);
	void *ptr2 = malloc(4097);
	T_ASSERT_NOTNULL(ptr, "SMALL allocation");
	T_ASSERT_NOTNULL(ptr2, "SMALL allocation");
	free(ptr2);
	T_EXPECT_EQ_ULONG(0UL, malloc_size(ptr2), "return 0 for free SMALL allocations");
	free(ptr);
	T_EXPECT_EQ_ULONG(0UL, malloc_size(ptr), "return 0 for free SMALL allocations");

	ptr = malloc(65536); // LARGE allocation
	T_ASSERT_NOTNULL(ptr, "LARGE allocation");
	free(ptr);
	T_ASSERT_EQ_ULONG(0UL, malloc_size(ptr), "return 0 for free LARGE allocations");

	// Allocate then free a large block, and then place a smaller aligned
	// allocation at the same place. If the metadata from the original
	// allocation isn't zeroed out, calls to malloc_size with pointers
	// past the new allocation will return nonzero, which is wrong
	ptr = malloc(MiB(1));
	T_ASSERT_NOTNULL(ptr, NULL);
	free(ptr);
	ptr2 = aligned_alloc(KiB(16), KiB(160));
	if (ptr == ptr2) {
		// We aren't guaranteed that ptr2 will be placed at the same place as the
		// first allocation, so only check that the OOB pointer has a size of 0
		// if they are together
		T_ASSERT_EQ((size_t)0, malloc_size(ptr2 + KiB(192)), "aligned size 0");
	}
	free(ptr2);
}
#endif // !MALLOC_TARGET_EXCLAVES

#if TARGET_OS_OSX
T_DECL(malloc_size_large_allocation, "Test malloc_size() on buffers > 4GB",
		T_META_TAG_XZONE, T_META_TAG_VM_NOT_PREFERRED)
{
	void *ptr = malloc(GiB(4));
	T_ASSERT_NOTNULL(ptr, "4GB allocation");
	T_ASSERT_GE(malloc_size(ptr), GiB(4), "size of 4GB allocation");
	free(ptr);

	ptr = malloc(GiB(6));
	T_ASSERT_NOTNULL(ptr, "6GB allocation");
	T_ASSERT_GE(malloc_size(ptr), GiB(6), "size of 6GB allocation");
	free(ptr);
}
#endif // TARGET_OS_OSX

T_DECL(malloc_size_multi_segment,
		"Make a multi-segment allocation, and pass inner pointers to malloc_size",
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_NOT_ELIGIBLE)
{
	void *ptr = malloc(MiB(12));
	T_ASSERT_NOTNULL(ptr, "HUGE allocation");
	T_ASSERT_LE(MiB(12), malloc_size(ptr), "Allocated sufficient size");
	T_ASSERT_EQ(0, malloc_size((void*)((uintptr_t)ptr + KiB(8))),
			"malloc_size is 0 for inner huge pointer in first segment granule");
	T_ASSERT_EQ(0, malloc_size((void*)((uintptr_t)ptr + MiB(8))),
			"malloc_size is 0 for inner huge pointer in last segment granule");

	free(ptr);
}

#if TARGET_OS_OSX
T_DECL(malloc_size_outside_embedded_space,
		"Make enough allocations to push address space beyond 64GB",
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_NOT_ELIGIBLE)
{
	// Make 32 2GB allocations (don't fault them to avoid dirty memory) to
	// exhaust the embedded/low segment table address space
	const size_t size = GiB(2);
	const size_t num_pointers = GiB(64) / size;
	void *ptrs[num_pointers];
	for (int i = 0; i < num_pointers; i++) {
		ptrs[i] = malloc(size);
		T_ASSERT_NOTNULL(ptrs[i], "Padding allocation");
	}

	// The next allocation should always be above the 64GB mark
	void *high_ptr = malloc(size);
	T_ASSERT_NOTNULL(high_ptr, "Allocation outside embedded address space");
	T_ASSERT_LE(GiB(64), (uintptr_t)high_ptr, "Allocated pointer above 64GB");
	T_ASSERT_LE(size, malloc_size(high_ptr),
			"Size of pointer outside embedded address space");

	free(high_ptr);
	T_ASSERT_EQ(0, malloc_size(high_ptr), "Size is zero after freeing");

	for (int i = 0; i < num_pointers; i++) {
		free(ptrs[i]);
	}
}
#endif // TARGET_OS_OSX

T_DECL(malloc_size_max_good_size, "Check malloc_good_size(SIZE_MAX)",
		T_META_TAG_XZONE, T_META_TAG_NANO_ON_XZONE,
		T_META_ENVVAR("MallocNanoZone=1"))
{
	size_t request_size = SIZE_MAX - 5;
	size_t good_size = malloc_good_size(request_size);
	T_ASSERT_GE(good_size, request_size, "good_size valid for request");
}
