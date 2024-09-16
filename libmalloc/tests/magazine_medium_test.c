//
//  magazine_medium_test.c
//  libmalloc_test
//
//  Created by Jason Teplitz on 5/17/21.
//

#include <darwintest.h>
#include "../src/internal.h"

#if CONFIG_MEDIUM_ALLOCATOR

#include "../src/magazine_medium.c"
#include "magazine_testing.h"

bool aggressive_madvise_enabled = false;
uint64_t magazine_medium_madvise_window_scale_factor = 1;
malloc_zero_policy_t malloc_zero_policy = MALLOC_ZERO_POLICY_DEFAULT;

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

static inline void
medium_test_rack_setup(rack_t *rack)
{
	test_rack_setup(rack, RACK_TYPE_MEDIUM);
}

static void
assert_block_madvise_headers(void *ptr, msize_t msize, bool dirty, bool intrusive_free_list)
{
	msize_t *madv_headers = MEDIUM_MADVISE_HEADER_FOR_PTR(ptr);
	msize_t index = MEDIUM_META_INDEX_FOR_PTR(ptr);
	msize_t orig_msize = msize;
	uintptr_t safe_start_ptr = (uintptr_t) ptr;
	uintptr_t end_ptr = (uintptr_t)ptr + (msize << SHIFT_MEDIUM_QUANTUM);
	uintptr_t safe_end_ptr = end_ptr;
	if (intrusive_free_list && !dirty) {
		safe_start_ptr = round_page_kernel((uintptr_t)ptr + sizeof(medium_inplace_free_entry_s) + sizeof(msize_t));
		safe_end_ptr = trunc_page_kernel((uintptr_t) ptr + (msize << SHIFT_MEDIUM_QUANTUM) - sizeof(msize_t));
	}
	index = MEDIUM_META_INDEX_FOR_PTR(safe_start_ptr);
	msize = MEDIUM_META_INDEX_FOR_PTR(safe_end_ptr) - index;
	msize_t end_index = MEDIUM_META_INDEX_FOR_PTR(safe_end_ptr);
	msize_t expected = msize | (dirty ? 0 : MEDIUM_IS_ADVISED);
	T_ASSERT_EQ(madv_headers[index], expected, "Start of block is marked correctly");
	if (msize > 1) {
		T_ASSERT_EQ(madv_headers[end_index - 1], expected, "End of block is marked correctly");
	}
	for (msize_t i = 1; i < msize - 1; i++) {
		T_QUIET; T_ASSERT_EQ(madv_headers[index + i], (msize_t) 0, "Middle of block is marked correctly");
	}
	if (intrusive_free_list) {
		// Make sure that the first and last pages are marked dirty
		index = MEDIUM_META_INDEX_FOR_PTR(ptr);
		if (MEDIUM_META_INDEX_FOR_PTR(safe_start_ptr) > MEDIUM_META_INDEX_FOR_PTR(ptr)) {
			msize_t first_page_header = madv_headers[index];
			T_ASSERT_NE(first_page_header, (msize_t)0, "free list is not marked as middle");
			T_ASSERT_EQ(first_page_header & MEDIUM_IS_ADVISED, 0, "free list is marked as dirty");
		}
		if (MEDIUM_META_INDEX_FOR_PTR(safe_end_ptr) < MEDIUM_META_INDEX_FOR_PTR(end_ptr)) {
			msize_t last_page_header = madv_headers[MEDIUM_META_INDEX_FOR_PTR(safe_end_ptr)];
			T_ASSERT_NE(last_page_header, (msize_t)0, "free list is not marked as middle");
			T_ASSERT_EQ(last_page_header & MEDIUM_IS_ADVISED, 0, "free list is marked as dirty");
		}
	}
}

static inline magazine_t *
get_magazine(struct rack_s *rack, void *ptr)
{
	mag_index_t mag_index = MAGAZINE_INDEX_FOR_MEDIUM_REGION(MEDIUM_REGION_FOR_PTR(ptr));
	return &(rack->magazines[mag_index]);
}


T_DECL(medium_realloc_madvise_headers, "medium realloc in place maintains madvise headers",
	   T_META_ENABLED(CONFIG_MEDIUM_ALLOCATOR),T_META_TAG_VM_PREFERRED)
{
	struct rack_s rack;
	medium_test_rack_setup(&rack);

	// Allocate two blocks and free the second, then try to realloc() the first.
	// This should extend in-place

	void *ptr = medium_malloc_should_clear(&rack, 1, false);
	T_ASSERT_NOTNULL(ptr, "allocation");
	void *ptr2 = medium_malloc_should_clear(&rack, 4, false);
	T_ASSERT_NOTNULL(ptr2, "allocation 2");
	T_ASSERT_EQ_PTR(ptr2, (void *)((uintptr_t)ptr + MEDIUM_BYTES_FOR_MSIZE(1)), "sequential allocations");
	// Allocate an extra block and free it last so we don't hit in medium's last free cache
	void *extra_ptr = medium_malloc_should_clear(&rack, 1, false);

	free_medium(&rack, ptr2, MEDIUM_REGION_FOR_PTR(ptr2), 0);
	free_medium(&rack, extra_ptr, MEDIUM_REGION_FOR_PTR(extra_ptr), 0);

	boolean_t realloced = medium_try_realloc_in_place(&rack, ptr, MEDIUM_BYTES_FOR_MSIZE(1), MEDIUM_BYTES_FOR_MSIZE(2));
	T_ASSERT_TRUE(realloced, "realloced");

	// Make sure the madvise headers are correct for both the realloc'd block and the new smaller block after it.
	assert_block_madvise_headers(ptr, 2, true, false);
	void *next_block = (unsigned char *)ptr + MEDIUM_BYTES_FOR_MSIZE(2);
	assert_block_madvise_headers(next_block, 3, true, false);
}

T_DECL(free_end_of_region, "End of region's footer is marked dirty",
	   T_META_ENABLED(CONFIG_MEDIUM_ALLOCATOR), T_META_TAG_VM_PREFERRED)
{
	// Check that the headers for the last block in a region are correct
	// when the block has been coalesced and is using an intrusive free list.
	struct rack_s rack;
	medium_test_rack_setup(&rack);

	// Use up all of the OOB entries so we force an intrusive free list
	void *oob_ptrs[MEDIUM_OOB_COUNT];
	for (size_t i = 0; i < MEDIUM_OOB_COUNT * 2; i++) {
		void *ptr = medium_malloc_should_clear(&rack, 1, false);
		T_QUIET; T_ASSERT_NOTNULL(ptr, "oob allocation");
		if (i % 2 == 0){
			oob_ptrs[i / 2] = ptr;
		}
	}
	for (size_t i = 0; i < MEDIUM_OOB_COUNT; i++){
		void *ptr = oob_ptrs[i];
		free_medium(&rack, ptr, MEDIUM_REGION_FOR_PTR(ptr), 0);
	}
	
	// Allocate the rest of the region in allocations just below the madvise window
	size_t num_allocated = MEDIUM_OOB_COUNT * 2;
	magazine_t *mag = get_magazine(&rack, oob_ptrs[0]);
	void *ptr = NULL, *last_ptr = NULL;
	size_t block_size = 0, final_block_size = 0;
	while (num_allocated < NUM_MEDIUM_BLOCKS) {
		size_t curr_block_size = ((medium_sliding_madvise_granularity(mag)) >> SHIFT_MEDIUM_QUANTUM) - 1;
		if (curr_block_size + num_allocated >= NUM_MEDIUM_BLOCKS) {
			// Last block, just allocate whatever remains
			curr_block_size = NUM_MEDIUM_BLOCKS - num_allocated;
			final_block_size = curr_block_size;
		} else {
			block_size = curr_block_size;
		}
		last_ptr = ptr;
		ptr = medium_malloc_should_clear(&rack, curr_block_size, false);
		T_QUIET; T_ASSERT_NOTNULL(ptr, "allocation under madvise window");
		num_allocated += curr_block_size;
	}
	
	// Now free the final two blocks so they coalesced together and madvised
	free_medium(&rack, last_ptr, MEDIUM_REGION_FOR_PTR(last_ptr), 0);
	
	free_medium(&rack, ptr, MEDIUM_REGION_FOR_PTR(ptr), 0);
	
	// The magazine caches the most recently freed pointer
	// so free one more to trigger madvise of last_ptr
	void *before_trailing_2 = (void *) ((uintptr_t)oob_ptrs[0] + (1UL << SHIFT_MEDIUM_QUANTUM));
	free_medium(&rack, before_trailing_2, MEDIUM_REGION_FOR_PTR(before_trailing_2), 0);

	assert_block_madvise_headers(last_ptr, block_size + final_block_size, false, true);
}

T_DECL(madvise_scale_factor, "madvise_scale_factor changes window size",
	   T_META_ENABLED(CONFIG_MEDIUM_ALLOCATOR), T_META_TAG_VM_PREFERRED)
{
	struct rack_s rack;
	medium_test_rack_setup(&rack);

	magazine_t *mag = NULL;
	void *ptr = medium_malloc_should_clear(&rack, 1, false);
	mag = get_magazine(&rack, ptr);
	free_medium(&rack, ptr, MEDIUM_REGION_FOR_PTR(ptr), 0);
	ptr = NULL;

	magazine_medium_madvise_window_scale_factor = 1;
	uint64_t granularity = medium_sliding_madvise_granularity(mag);
	T_QUIET; T_ASSERT_EQ(granularity, (uint64_t) MEDIUM_MADVISE_MIN, "window is at min size when magazine is empty");
	magazine_medium_madvise_window_scale_factor = 4;
	granularity = medium_sliding_madvise_granularity(mag);
	T_QUIET; T_ASSERT_EQ(granularity, 4ULL * MEDIUM_MADVISE_MIN, "scale factor multiplies min size");
	magazine_medium_madvise_window_scale_factor = 1;

	// Allocate the majority of the magazine
	for (size_t i = 0; i < NUM_MEDIUM_BLOCKS / 2; i++) {
		ptr = medium_malloc_should_clear(&rack, 1, false);
		T_QUIET; T_ASSERT_NOTNULL(ptr, "allocation");
	}

	uint64_t num_bytes_allocated = (NUM_MEDIUM_BLOCKS / 2) << SHIFT_MEDIUM_QUANTUM;
	uint64_t base_window_size = 1 << (64 - __builtin_clzl(num_bytes_allocated >> MEDIUM_MADVISE_SHIFT));
	T_QUIET; T_ASSERT_GE(base_window_size, (uint64_t) MEDIUM_MADVISE_MIN, "window grows as more bytes are allocated");

	granularity = medium_sliding_madvise_granularity(mag);
	T_QUIET; T_ASSERT_EQ(granularity, base_window_size, "window grows correctly");
	magazine_medium_madvise_window_scale_factor = 8;
	granularity = medium_sliding_madvise_granularity(mag);
	T_QUIET; T_ASSERT_EQ(granularity, 8 * base_window_size, "larger window also scales up");
}

T_DECL(medium_free_deallocate, "check medium regions deallocate when empty",
		T_META_ENABLED(CONFIG_MEDIUM_ALLOCATOR), T_META_TAG_VM_PREFERRED)
{
	struct rack_s rack;
	memset(&rack, 'a', sizeof(rack));
	rack_init(&rack, RACK_TYPE_MEDIUM, 1, 0);
	// force recirc to be released when empty
	recirc_retained_regions = 0;

	magazine_t *mag = &rack.magazines[0];
	T_ASSERT_EQ(mag->mag_last_region, NULL, "no regions before allocation");

	void *ptr = medium_malloc_should_clear(&rack, 1, false);
	T_ASSERT_NE(ptr, NULL, "allocate");

	region_t region = MEDIUM_REGION_FOR_PTR(ptr);
	mag_index_t mag_index = MAGAZINE_INDEX_FOR_MEDIUM_REGION(region);
	magazine_t *dest_mag = &(rack.magazines[mag_index]);
	T_ASSERT_GE(mag_index, -1, "assert ptr not in recirc");

	// Is to pass the recirc discriminant in medium_free_try_recirc_to_depot
	// which normally prevents recirc unless 1.5x of a region has been
	// allocated in the magazine as a whole.
	dest_mag->num_bytes_in_magazine = (5 * MEDIUM_REGION_PAYLOAD_BYTES);

	SZONE_MAGAZINE_PTR_LOCK(dest_mag);
	medium_free_no_lock(&rack, dest_mag, mag_index, region, ptr, 1);

	T_ASSERT_EQ(mag->mag_last_region, NULL, "no regions after last free");

	magazine_t *depot = &rack.magazines[DEPOT_MAGAZINE_INDEX];
	T_ASSERT_EQ(depot->mag_num_objects, 0, "no objects in depot after last free");
	T_ASSERT_EQ(depot->num_bytes_in_magazine, 0ul, "no region in depot after last free");
}

#else // CONFIG_MEDIUM_ALLOCATOR

// binaries are required to contain at least 1 test
T_DECL(medium_test_skip, "skip medium tests")
{
	T_SKIP("MallocMedium is not compiled on this platform");
}

#endif // CONFIG_MEDIUM_ALLOCATOR
