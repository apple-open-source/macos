//
//  aligned_alloc_test.c
//  libmalloc
//
//  test allocating and freeing all sizes and alignments
//

#include <darwintest.h>
#include <errno.h>
#include <malloc/malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_XZONE);

static inline void*
t_aligned_alloc(size_t alignment, size_t size)
{
	void *ptr = aligned_alloc(alignment, size);
	size_t allocated_size = malloc_size(ptr);

	T_QUIET; T_ASSERT_NOTNULL(ptr, "allocation");
	T_QUIET; T_ASSERT_EQ((intptr_t)ptr % alignment, 0ul, "pointer should be properly aligned");
	T_QUIET; T_EXPECT_LE(size, allocated_size, "allocation size");

	// Scribble memory pointed to by `ptr` to make sure we're not using that
	// memory for control structures. This also makes sure the memory can be
	// written to.
	const uint64_t pat = 0xdeadbeefcafebabeull;
	memset_pattern8(ptr, &pat, size);
	return ptr;
}

T_DECL(aligned_alloc_free, "aligned_alloc all power of two alignments <= 64kb",
		T_META_TAG_VM_NOT_PREFERRED)
{
#if TARGET_OS_WATCH
	const size_t max_alignment = 4096;
#else
	const size_t max_alignment = 64 * 1024;
#endif // TARGET_OS_WATCH
	for (size_t alignment = sizeof(void*); alignment < max_alignment;
			alignment *= 2) {
		// test several sizes that are multiples of the alignment
		for (size_t size = alignment; size <= 256*alignment; size += alignment) {
			const int num_ptrs = 10;
			void *ptrs[num_ptrs];
			for (int i = 0; i < num_ptrs; i++) {
				ptrs[i] = t_aligned_alloc(alignment, size);
			}
			for (int i = 0; i < num_ptrs; i++) {
				free(ptrs[i]);
			}
		}
	}
}

T_DECL(aligned_alloc_alignment_not_multiple_of_size, "aligned_alloc should set errno to EINVAL if size is not a multiple of alignment",
		T_META_TAG_VM_PREFERRED)
{
	{
		void *ptr = aligned_alloc(8, 12);
		T_QUIET; T_ASSERT_NULL(ptr, "ptr should be null");
		T_QUIET; T_ASSERT_EQ(errno, EINVAL, "errno should be EINVAL");
	}

	{
		void *ptr = aligned_alloc(32, 16);
		T_QUIET; T_ASSERT_NULL(ptr, "ptr should be null");
		T_QUIET; T_ASSERT_EQ(errno, EINVAL, "errno should be EINVAL");
	}
}

T_DECL(aligned_alloc_alignment_not_power_of_two, "aligned_alloc should set errno to EINVAL if alignment is not a power of two (implementation constraint)",
		T_META_TAG_VM_PREFERRED)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-power-of-two-alignment"
	{
		void *ptr = aligned_alloc(24, 48); // alignment is even, but not a power of two
		T_QUIET; T_ASSERT_NULL(ptr, "ptr should be null");
		T_QUIET; T_ASSERT_EQ(errno, EINVAL, "errno should be EINVAL");
	}

	{
		void *ptr = aligned_alloc(23, 46); // alignment is odd, and not a power of two
		T_QUIET; T_ASSERT_NULL(ptr, "ptr should be null");
		T_QUIET; T_ASSERT_EQ(errno, EINVAL, "errno should be EINVAL");
	}
#pragma GCC diagnostic pop
}

T_DECL(aligned_alloc_alignment_not_a_multiple_of_voidstar, "aligned_alloc should set errno to EINVAL if alignment is not a multiple of sizeof(void*) (implementation constraint)",
		T_META_TAG_VM_PREFERRED)
{
	const size_t alignment = sizeof(void*)+1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-power-of-two-alignment"
	void *ptr = aligned_alloc(alignment, alignment * 2);
#pragma GCC diagnostic pop
	T_QUIET; T_ASSERT_NULL(ptr, "ptr should be null");
	T_QUIET; T_ASSERT_EQ(errno, EINVAL, "aligned_alloc should set errno to EINVAL");
}
