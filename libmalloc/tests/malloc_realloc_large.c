#include <stdlib.h>
#include <malloc/malloc.h>
#include <darwintest.h>
#include <sys/mman.h>

#include <../src/internal.h>

#if CONFIG_XZONE_MALLOC
#define LARGE_BLOCK_SIZE_MAX XZM_LARGE_BLOCK_SIZE_MAX
#else
#define LARGE_BLOCK_SIZE_MAX MiB(2)
#define XZM_SEGMENT_SIZE MiB(4)
#endif

// Xzone malloc currently tries to realloc any LARGE or HUGE allocations (>32K)
// in-place, provided that the new size is in the same class as the old one.
// This should always be possible if the new size is smaller, but can't always
// be done if the new size is larger. As such, this file only checks that the
// allocation doesn't change the pointer when shrinking the allocation.
//
// If the old size class isn't the same as the new class (e.g. LARGE to HUGE or
// vice versa), then the new allocation must be in a different segment, and
// thus can't be in-place.
//
// NB: This test file is here to prevent accidental regressions/crashes caused
// by xzone malloc attempting to realloc in place. There is no guarantee that
// realloc will always try to realloc in place. If this test begins failing due
// to a future change, that doesn't imply that the change is bad, this test
// might need to be modified/removed instead.

static bool memchk(void *ptr, uint8_t contents, size_t size)
{
	for (size_t i = 0; i < size; i++) {
		if (((uint8_t *)ptr)[i] != contents) {
			return false;
		}
	}
	return true;
}

T_DECL(realloc_large_huge, "call realloc on LARGE and HUGE allocations",
		T_META_TAG_XZONE_ONLY,
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	// Large allocation shrink in place
	size_t size1 = LARGE_BLOCK_SIZE_MAX;
	size_t size2 = LARGE_BLOCK_SIZE_MAX / 4;
	void *ptr1 = malloc(LARGE_BLOCK_SIZE_MAX);
	memset(ptr1, 'A', size1);
	void *ptr2 = realloc(ptr1, size2);
	T_ASSERT_TRUE(memchk(ptr2, 'A', size2), "contents unchanged after realloc");
	T_ASSERT_LE(size2, malloc_size(ptr2), "realloc LARGE smaller");
	T_ASSERT_EQ(ptr1, ptr2, "realloc LARGE smaller in-place");
	free(ptr2);

	// Large allocation grow in place
	size1 = LARGE_BLOCK_SIZE_MAX / 4;
	size2 = LARGE_BLOCK_SIZE_MAX / 2;
	ptr1 = malloc(size1);
	memset(ptr1, 'B', size1);
	ptr2 = realloc(ptr1, size2);
	T_ASSERT_TRUE(memchk(ptr2, 'B', size1), "contents unchanged after realloc");
	T_ASSERT_LE(size2, malloc_size(ptr2),
			"realloc LARGE larger");
	// realloc-ing to a larger size can't always be done in-place
	free(ptr2);

	// Huge allocation shrink in place
	size1 = LARGE_BLOCK_SIZE_MAX * 8;
	size2 = LARGE_BLOCK_SIZE_MAX * 2;
	ptr1 = malloc(size1);
	memset(ptr1, 'C', size1);
	ptr2 = realloc(ptr1, size2);
	T_ASSERT_TRUE(memchk(ptr2, 'C', size2), "contents unchanged after realloc");
	T_ASSERT_LE(size2, malloc_size(ptr2),
			"realloc HUGE smaller");

#if MALLOC_TARGET_EXCLAVES
	T_EXPECTFAIL_WITH_REASON("Exclaves don't support resizing mappings");
#endif // MALLOC_TARGET_EXCLAVES
	T_ASSERT_EQ(ptr1, ptr2, "realloc HUGE smaller in-place");
	free(ptr2);

	// Huge allocation grow in place
	size1 = LARGE_BLOCK_SIZE_MAX * 2;
	size2 = LARGE_BLOCK_SIZE_MAX * 4;
	ptr1 = malloc(size1);
	memset(ptr1, 'D', size1);
	ptr2 = realloc(ptr1, size2);
	T_ASSERT_TRUE(memchk(ptr2, 'D', size1), "contents unchanged after realloc");
	T_ASSERT_LE(size2, malloc_size(ptr2),
			"realloc HUGE larger 1");
	// realloc-ing to a larger size can't always be done in-place
	free(ptr2);

	// Huge allocation grow in place, size not a multiple of segment granule
	_Static_assert(XZM_SEGMENT_SIZE == MiB(4),
			"revise constants if changing segment size");
	size1 = MiB(3);
	size2 = MiB(5);
	ptr1 = malloc(size1);
	memset(ptr1, 'd', size1);
	ptr2 = realloc(ptr1, size2);
	T_ASSERT_TRUE(memchk(ptr2, 'd', size1), "contents unchanged after realloc");
	T_ASSERT_LE(size2, malloc_size(ptr2),
			"realloc HUGE larger 2");
	// realloc-ing to a larger size can't always be done in-place
	free(ptr2);

	// Large aligned realloc
	size1 = LARGE_BLOCK_SIZE_MAX / 2;
	size2 = LARGE_BLOCK_SIZE_MAX / 8;
	ptr1 = aligned_alloc(2048, size1);
	memset(ptr1, 'E', size1);
	ptr2 = realloc(ptr1, size2);
	T_ASSERT_TRUE(memchk(ptr2, 'E', size2), "contents unchanged after realloc");
	T_ASSERT_LE(size2, malloc_size(ptr2),
			"realloc LARGE aligned");
	free(ptr2);

	// Large allocation to huge reallocation
	size1 = LARGE_BLOCK_SIZE_MAX / 2;
	size2 = LARGE_BLOCK_SIZE_MAX * 2;
	ptr1 = malloc(size1);
	memset(ptr1, 'F', size1);
	ptr2 = realloc(ptr1, size2);
	T_ASSERT_TRUE(memchk(ptr2, 'F', size1), "contents unchanged after realloc");
	T_ASSERT_LE(size2, malloc_size(ptr2),
			"realloc LARGE to HUGE");
	T_ASSERT_NE(ptr1, ptr2, "realloc LARGE to HUGE not in-place");
	free(ptr2);

	// Huge allocation to large reallocation
	size1 = LARGE_BLOCK_SIZE_MAX * 2;
	size2 = LARGE_BLOCK_SIZE_MAX / 2;
	ptr1 = malloc(size1);
	memset(ptr1, 'G', size1);
	ptr2 = realloc(ptr1, size2);
	T_ASSERT_TRUE(memchk(ptr2, 'G', size2), "contents unchanged after realloc");
	T_ASSERT_LE(size2, malloc_size(ptr2),
			"realloc HUGE to LARGE");
	T_ASSERT_NE(ptr1, ptr2, "realloc HUGE to LARGE not in-place");
	free(ptr2);

	// Huge aligned realloc
	size1 = LARGE_BLOCK_SIZE_MAX * 8;
	size2 = LARGE_BLOCK_SIZE_MAX * 2;
	ptr1 = aligned_alloc(0x10000, size1); // align to 4 slices/os pages
	memset(ptr1, 'H', size1);
	ptr2 = realloc(ptr1, size2);
	T_ASSERT_TRUE(memchk(ptr2, 'H', size2), "contents unchanged after realloc");
	T_ASSERT_LE(size2, malloc_size(ptr2),
		"realloc HUGE aligned");
	free(ptr2);
}


T_DECL(realloc_overlap_mmap,
		"Make sure that realloc in place doesn't overwrite existing mmap",
		T_META_TAG_XZONE_ONLY)
{
	// Allocate a huge buffer
	void *ptr = malloc(LARGE_BLOCK_SIZE_MAX * 2);
	T_ASSERT_NOTNULL(ptr, "Allocated huge buffer");

	// mmap some anonymous memory just past the end of that allocation
	void *map_addr = (void*)((uintptr_t)ptr + LARGE_BLOCK_SIZE_MAX * 3);
#if MALLOC_TARGET_EXCLAVES
	plat_map_t plat_map = { 0 };
	_liblibc_map_type_t type = LIBLIBC_MAP_TYPE_PRIVATE |
			LIBLIBC_MAP_TYPE_FIXED | LIBLIBC_MAP_TYPE_NORAND;
	// liblibc errors if you try to mmap with PROT_NONE
	void *map = mmap_plat(&plat_map, (uintptr_t)map_addr, LARGE_BLOCK_SIZE_MAX,
			PROT_READ, type, 0, 0);
	if (map == NULL) {
		T_SKIP("VM isn't setup to mmap after the huge allocation");
	}
#else // MALLOC_TARGET_EXCLAVES
	void *map = mmap(map_addr, LARGE_BLOCK_SIZE_MAX, PROT_NONE, MAP_ANON | MAP_PRIVATE, 0, 0);
	if (map == MAP_FAILED || map != map_addr) {
		// Couldn't map memory just past the end of the huge allocation, due to
		// the VM layout (which is outside the control of this test).  We could
		// attempt a retry, but if we free and then reallocate the huge buffer,
		// it will probably land in the same place. In empirical testing, we
		// don't usually hit this condition, since usually the huge allocation
		// goes into a large free VM region, so I'm comfortable skipping the
		// test if we get really unlucky here.
		T_SKIP("VM isn't setup to mmap after the huge allocation");
	}
#endif // MALLOC_TARGET_EXCLAVES

	T_ASSERT_EQ(map_addr, map, "mmap'd at expected address");

	// realloc the huge buffer larger, make sure that it isn't done in-place
	void * ptr2 = realloc(ptr, LARGE_BLOCK_SIZE_MAX * 4);
	T_ASSERT_NOTNULL(ptr2, "Reallocated HUGE buffer larger");
	T_ASSERT_NE(ptr2, ptr, "realloc overwrote existing mmap");
	memset(ptr2, 0xaa, LARGE_BLOCK_SIZE_MAX * 4);
	free(ptr2);

#if MALLOC_TARGET_EXCLAVES
	munmap_plat(&plat_map, map, LARGE_BLOCK_SIZE_MAX);
#else
	munmap(map, LARGE_BLOCK_SIZE_MAX);
#endif // MALLOC_TARGET_EXCLAVES
}
