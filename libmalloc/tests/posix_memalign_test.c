//
//  posix_memalign_test.c
//  libmalloc
//
//  test allocating and freeing all sizes and alignments
//

#include <darwintest.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/platform.h"

#if !MALLOC_TARGET_EXCLAVES
#include <mach/mach.h>
#endif // !MALLOC_TARGET_EXCLAVES

#include <malloc/malloc.h>
#include <malloc_private.h>


union memtag_ptr {
	uint64_t value;

	struct {
		uint64_t ptr_bits : 56;
		uint64_t ptr_tag : 4;
		uint64_t ptr_upper : 4;
	};
};

static uint8_t *
memtag_strip_address(uint8_t *tagged_addr)
{
	union memtag_ptr p = {
			.value = (uint64_t)tagged_addr,
	};
	return (uint8_t *)p.ptr_bits;
}

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

#if !MALLOC_TARGET_EXCLAVES
struct t_recorder_ctx {
	void *ptr;
	size_t size;
	bool found;
};

static void
pointer_recorder(task_t task, void *context, unsigned type, vm_range_t *ranges,
		unsigned count)
{
	if (!(type & MALLOC_PTR_IN_USE_RANGE_TYPE)) {
		return;
	}

	struct t_recorder_ctx *ctx = context;
	vm_address_t ptr_addr = (vm_address_t)(ctx->ptr);
	vm_size_t ptr_size = (vm_size_t)(ctx->size);
	for (unsigned i = 0; i < count; i++) {
		vm_range_t *range = &ranges[i];
		if (range->address <= ptr_addr &&
				range->address + range->size > ptr_addr) {
			T_QUIET; T_EXPECT_FALSE(ctx->found, "first time");

			vm_size_t offset = ptr_addr - range->address;
			T_QUIET; T_EXPECT_GE(range->size - offset, ctx->size,
					"allocation must be large enough");

			ctx->found = true;
		}
	}
}

static void
check_pointer_is_enumerated(void *ptr, size_t size)
{
	// Under MTE, `ptr` is tagged, but the enumerator reports canonical addresses.
	ptr = memtag_strip_address(ptr);

	vm_address_t *zones;
	unsigned zone_count;
	kern_return_t kr;

	kr = malloc_get_all_zones(mach_task_self(), /*reader=*/NULL, &zones,
			&zone_count);
	T_QUIET; T_ASSERT_EQ(kr, KERN_SUCCESS,
			"malloc_get_all_zones(mach_task_self(), ...)");

	struct t_recorder_ctx ctx = {
		.ptr = ptr,
		.size = size,
		.found = false,
	};

	for (unsigned i = 0; i < zone_count; i++) {
		malloc_zone_t *zone = (malloc_zone_t *)zones[i];
		zone->introspect->enumerator(mach_task_self(), &ctx,
				MALLOC_PTR_IN_USE_RANGE_TYPE, (vm_address_t)zone, NULL,
				pointer_recorder);
		if (ctx.found) {
			return;
		}
	}
	T_QUIET; T_FAIL("pointer %p not enumerated in any zone", ptr);
}
#endif // !MALLOC_TARGET_EXCLAVES

static inline void *
t_posix_memalign(size_t alignment, size_t size, bool scribble, bool enumerate)
{
	void *ptr = NULL;
	int result = posix_memalign(&ptr, alignment, size);
	size_t allocated_size = malloc_size(ptr);

	T_QUIET; T_ASSERT_NOTNULL(ptr, "allocation");
	T_QUIET; T_ASSERT_EQ((intptr_t)ptr % alignment, 0ul, "pointer should be properly aligned");
	T_QUIET; T_EXPECT_LE(size, allocated_size, "allocation size");

	T_QUIET; T_EXPECT_TRUE(malloc_claimed_address(ptr), "should be claimed");
#if !MALLOC_TARGET_EXCLAVES
	if (enumerate) {
		check_pointer_is_enumerated(ptr, size);
	}
#endif // !MALLOC_TARGET_EXCLAVES

	if (scribble) {
		// Scribble memory pointed to by `ptr` to make sure we're not using that
		// memory for control structures. This also makes sure the memory can be
		// written to.
		const uint64_t pat = 0xdeadbeefcafebabeull;
		memset_pattern8(ptr, &pat, size);
	}
	return ptr;
}

T_DECL(posix_memalign_free, "posix_memalign all power of two alignments <= 4096",
	   T_META_TAG_VM_PREFERRED, T_META_TAG_ALL_ALLOCATORS)
{
	for (size_t alignment = sizeof(void*); alignment < 4096; alignment *= 2) {
		bool enumerate = true;
		// test several sizes
		for (size_t size = alignment; size <= 256*alignment; size += 8) {
			void* ptr = t_posix_memalign(alignment, size, true, enumerate);
			free(ptr);
			enumerate = false;
		}
	}
}

T_DECL(posix_memalign_alignment_not_a_power_of_2,
	   "posix_memalign should return EINVAL if alignment is not a power of 2",
	   T_META_TAG_VM_PREFERRED, T_META_TAG_ALL_ALLOCATORS)
{
	{
		void *ptr = NULL;
		int result = posix_memalign(&ptr, 24, 48); // alignment is even, but not a power of two
		T_QUIET; T_ASSERT_NULL(ptr, "ptr should be null");
		T_QUIET; T_ASSERT_EQ(result, EINVAL, "posix_memalign should return EINVAL");
	}

	{
		void *ptr = NULL;
		int result = posix_memalign(&ptr, 23, 46); // alignment is odd, and not a power of two
		T_QUIET; T_ASSERT_NULL(ptr, "ptr should be null");
		T_QUIET; T_ASSERT_EQ(result, EINVAL, "posix_memalign should return EINVAL");
	}
}

T_DECL(posix_memalign_alignment_not_a_multiple_of_voidstar,
	   "posix_memalign should return EINVAL if alignment is not a multiple of sizeof(void*)",
	   T_META_TAG_VM_PREFERRED, T_META_TAG_ALL_ALLOCATORS)
{
	void *ptr = NULL;
	const size_t alignment = sizeof(void*)+1;
	int result = posix_memalign(&ptr, alignment, alignment * 2);
	T_QUIET; T_ASSERT_NULL(ptr, "ptr should be null");
	T_QUIET; T_ASSERT_EQ(result, EINVAL, "posix_memalign should return EINVAL");
}

T_DECL(posix_memalign_allocate_size_0,
       "posix_memalign should return something that can be passed to free() when size is 0",
	   T_META_TAG_VM_PREFERRED, T_META_TAG_ALL_ALLOCATORS)
{
	void *ptr = NULL;
	int result = posix_memalign(&ptr, 8, 0);
	T_QUIET; T_ASSERT_EQ(result, 0, "posix_memalign should not return an error when asked for size 0");
	free(ptr);
}

#if defined(__LP64__)
T_DECL(posix_memalign_large, "posix_memalign large power of two alignments",
		T_META_TAG_VM_NOT_PREFERRED, T_META_TAG_ALL_ALLOCATORS)
{
	// 64GB on macOS, 64MB on embedded
	uint64_t max_alignment = TARGET_OS_OSX ? UINT64_C(68719476736) : UINT64_C(67108864);
	for (size_t alignment = sizeof(void*); alignment <= max_alignment; alignment *= 2) {
		// don't scribble - we don't want to actually touch that many pages, we just
		// verify that the allocated pointer looks reasonable
		void* ptr = t_posix_memalign(alignment, alignment, false, true);
		free(ptr);
	}
	T_END;
}
#endif // __LP64__

T_DECL(posix_memalign_single_page_large,
		"Allocate tiny blocks with large alignment",
		T_META_TAG_VM_PREFERRED, T_META_TAG_XZONE_ONLY)
{
	void *ptr1 = t_posix_memalign(32768, 1, false, true);
	T_ASSERT_NOTNULL(ptr1, "Allocated aligned ptr %p", ptr1);
	void *ptr2 = t_posix_memalign(16*1024, 48 * 1024, false, true);
	T_ASSERT_NOTNULL(ptr2, "Allocated aligned ptr %p", ptr2);
	void *ptr3 = t_posix_memalign(32768, 1, false, true);
	T_ASSERT_NOTNULL(ptr3, "Allocated aligned ptr %p", ptr3);

	free(ptr1);
	free(ptr2);
	free(ptr3);
}
