#include <darwintest.h>
#include <inttypes.h>

#include <../src/internal.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_PREFERRED);

#if CONFIG_XZONE_MALLOC

#define XZM_BLOCK_INDEX_INVALID ~0

#ifndef NDEBUG
# define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
# define DEBUG_PRINT(...)
#endif

static const char operation_str[] = "depopulate";
static inline const char *get_operation_str(bool populate) {
	return populate ? operation_str + 2 : operation_str;
}

static xzm_block_index_t slice_select_random_block(xzm_slice_count_t chunk_idx,
		uint32_t chunk_capacity, size_t block_size, xzm_slice_count_t idx) {
	const size_t slice_start = (chunk_idx + idx) * XZM_SEGMENT_SLICE_SIZE;
	const size_t slice_limit = (chunk_idx + idx + 1) * XZM_SEGMENT_SLICE_SIZE;
	xzm_block_index_t block_start =
			_xzm_segment_offset_chunk_block_index_of(NULL, chunk_idx, block_size,
			slice_start);
	/* select the next block if this extends below the slice */
	if (block_start * block_size < slice_start) {
		block_start += 1;
	}
	xzm_block_index_t block_end =
			_xzm_segment_offset_chunk_block_index_of(NULL, chunk_idx, block_size,
			slice_limit - 1);
	/* select the previous block if this extends above the slice */
	if ((block_end + 1) * block_size - 1 >= slice_limit) {
		block_end -= 1;
	}

	const bool block_valid = (block_start >= 0 && block_start <= block_end &&
			block_end < chunk_capacity);
	const xzm_block_index_t block_idx = (block_valid ?
			block_start + (rand() % (block_end - block_start + 1)) :
			XZM_BLOCK_INDEX_INVALID);
	DEBUG_PRINT("%s: slice %u: 0x%lx-0x%lx, block: 0x%lx-0x%lx [%u-%u]/%u, block idx: %u\n",
			__func__, idx, slice_start, slice_limit,
			_xzm_segment_offset(NULL, chunk_idx, block_start, block_size),
			_xzm_segment_offset(NULL, chunk_idx, block_end + 1, block_size),
			block_start, block_end, chunk_capacity, block_idx);
	return block_idx;
}

static void do_operation(xzm_chunk_t chunk, xzm_slice_count_t chunk_idx,
		uint32_t chunk_capacity, xzm_block_index_t idx, uint64_t block_size,
		xzm_slice_count_t *slice_idx, xzm_slice_count_t *num_slices,
		bool populate) {
	if (populate) {
		_xzm_chunk_block_free_slices_on_allocate(chunk, chunk_idx,
				chunk_capacity, idx, block_size, slice_idx, num_slices);
	} else {
		_xzm_chunk_block_free_slices_on_deallocate(chunk, chunk_idx,
				chunk_capacity, idx, block_size, slice_idx, num_slices);
	}
}

static bool block_contains_slice(xzm_slice_count_t chunk_idx,
		uint32_t chunk_capacity, xzm_block_index_t block_idx,
		size_t block_size) {
	const size_t block = _xzm_segment_offset(NULL, chunk_idx, block_idx,
			block_size);
	const size_t left_up = roundup(block, XZM_SEGMENT_SLICE_SIZE);
	const bool next_partial = (block_idx + 1 == chunk_capacity);
	const size_t right =
			(!next_partial ? block + block_size : block + 2 * block_size);
	/* a following partial block will be treated as part of this block */
	const size_t right_down = rounddown(right, XZM_SEGMENT_SLICE_SIZE);
	/* slice boundaries must be within the range of our block */
	DEBUG_PRINT("%s: block %u: 0x%lx-0x%lx (next_partial: %u), slice: 0x%lx-0x%lx\n",
			__func__, block_idx, block, right, next_partial, left_up,
			right_down);
	return left_up < right_down && left_up >= block && right_down <= right;
}

static void blocks_all_used_but_one(xzm_chunk_t chunk,
		xzm_slice_count_t chunk_idx, uint32_t chunk_capacity,
		uint64_t block_size, bool populate) {
	const char *operation = get_operation_str(populate);
	xzm_slice_count_t slice_idx, num_slices;

	/* mark all blocks as used */
	chunk->xzc_free = 0;
	chunk->xzc_used = chunk->xzcs_slice_count;

	/* walk each full block, and check that if it were free and it contains a
	 * slice, then it would be (de)populated */
	printf("Checking %s of slice(s) for each block being free and all others used...\n",
			operation);
	for (xzm_block_index_t idx = 0; idx < chunk_capacity;
			++idx) {
		const uint64_t block = _xzm_segment_offset(NULL, chunk_idx, idx,
				block_size);

		chunk->xzc_free |= (1u << idx);
		do_operation(chunk, chunk_idx, chunk_capacity, idx, block_size,
				&slice_idx, &num_slices, populate);
		chunk->xzc_free &= ~(1u << idx);

		const size_t left = slice_idx * XZM_SEGMENT_SLICE_SIZE;
		const size_t right = left + num_slices * XZM_SEGMENT_SLICE_SIZE;

		const size_t slice_start = rounddown(block, XZM_SEGMENT_SLICE_SIZE);
		const size_t slice_limit = roundup(block + block_size,
				XZM_SEGMENT_SLICE_SIZE);

		if (block_contains_slice(chunk_idx, chunk_capacity, idx, block_size)) {
			T_ASSERT_NE(num_slices, 0, "Free block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") should %s slice",
					block, block + block_size, idx, chunk_capacity, block_size,
					operation);
			T_ASSERT_GE(left, slice_start,
					"Free block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") %s start 0x%lx should be greater than or equal to slice 0x%lx",
					block, block + block_size, idx, chunk_capacity, block_size,
					operation, left, slice_start);
			T_ASSERT_LE(right, slice_limit,
					"Free block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") %s end 0x%lx should be less than or equal to slice 0x%lx",
					block, block + block_size, idx, chunk_capacity, block_size,
					operation, right, slice_limit);
		} else {
			T_ASSERT_EQ(left, right, "Free block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") should not %s slice",
					block, block + block_size, idx, chunk_capacity, block_size,
					operation);
		}
	}
}

static void blocks_one_used_per_slice(xzm_chunk_t chunk,
		xzm_slice_count_t chunk_idx, uint32_t chunk_capacity,
		uint64_t block_size, bool populate) {
	const char *operation = get_operation_str(populate);
	xzm_slice_count_t slice_idx, num_slices;

	uint32_t slice_may_affect_vm = 0;
	/* mark a random block for each slice as used */
	puts("Marking random block for each slice as used...");
	for (xzm_slice_count_t idx = 0; idx < chunk->xzcs_slice_count; ++idx) {
		const xzm_block_index_t block_idx = slice_select_random_block(chunk_idx,
					chunk_capacity, block_size, idx);
		if (block_idx == XZM_BLOCK_INDEX_INVALID) {
			assert(idx < CHAR_BIT * sizeof(slice_may_affect_vm));
			/* slice does not contain a complete block, so mark the slice as
			 * capable of being (de)populated, since an allocated block
			 * may straddle the boundary of two such slices */
			slice_may_affect_vm |= (1u << idx);
		} else {
			assert(block_idx < CHAR_BIT * sizeof(chunk->xzc_free));
			chunk->xzc_free &= ~(1u << block_idx);
			chunk->xzc_used += 1;
		}
	}

	/* walk each block, and if it is free, check that its slice would not be
	 * populated, otherwise that its slice would be (de)populated if it were free */
	printf("Checking %s of slice(s) for all blocks with random used block per slice...\n",
			operation);
	for (xzm_block_index_t idx = 0; idx < chunk_capacity; ++idx) {
		const uint64_t block = _xzm_segment_offset(NULL, chunk_idx, idx, block_size);

		if (_xzm_small_chunk_block_index_is_free(chunk, idx)) {
			do_operation(chunk, chunk_idx, chunk_capacity, idx, block_size,
				&slice_idx, &num_slices, populate);

			const size_t left = slice_idx * XZM_SEGMENT_SLICE_SIZE;
			const size_t right = left + num_slices * XZM_SEGMENT_SLICE_SIZE;

			/* a block may straddle two slices, so (de)population can occur */
			assert(!num_slices || slice_idx < chunk->xzcs_slice_count);
			if (num_slices && (slice_may_affect_vm & (1u << slice_idx))) {
				T_ASSERT_NE(num_slices, 0,
						"Free block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") should %s slice",
						block, block + block_size, idx, chunk_capacity,
						block_size, operation);
				T_ASSERT_LT(left, right,
						"Free block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") should %s valid straddle slice(s) 0x%lx-0x%lx",
						block, block + block_size, idx, chunk_capacity,
						block_size, operation, left, right);
			} else {
				T_ASSERT_EQ(num_slices, 0,
						"Free block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") should not %s slice",
						block, block + block_size, idx, chunk_capacity,
						block_size, operation);
			}
		} else {
			chunk->xzc_free |= (1u << idx);
			do_operation(chunk, chunk_idx, chunk_capacity, idx, block_size,
				&slice_idx, &num_slices, populate);
			chunk->xzc_free &= ~(1u << idx);

			T_ASSERT_NE(num_slices, 0,
					"Random block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") should %s slice",
					block, block + block_size, idx, chunk_capacity, block_size,
					operation);

			const size_t left = slice_idx * XZM_SEGMENT_SLICE_SIZE;
			const size_t right = left + num_slices * XZM_SEGMENT_SLICE_SIZE;

			const size_t slice_start = rounddown(block, XZM_SEGMENT_SLICE_SIZE);
			const size_t slice_limit = roundup(block + block_size,
					XZM_SEGMENT_SLICE_SIZE);

			T_ASSERT_EQ(left, slice_start,
					"Random block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") %s start 0x%lx should match slice",
					block, block + block_size, idx, chunk_capacity, block_size,
					operation, left);
			T_ASSERT_EQ(right, slice_limit,
					"Random block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") %s end 0x%lx should match slice",
					block, block + block_size, idx, chunk_capacity, block_size,
					operation, right);
		}
	}
}

static void blocks_all_free(xzm_chunk_t chunk, xzm_slice_count_t chunk_idx,
		uint32_t chunk_capacity, uint64_t block_size, bool populate) {
	const char *operation = get_operation_str(populate);
	xzm_slice_count_t slice_idx, num_slices;

	/* all blocks are already free */
	assert(chunk->xzc_free == _xzm_xzone_free_mask(NULL, chunk_capacity));

	/* walk each full block, and check that its slice would be (de)populated */
	printf("Checking %s of slice for all blocks being free...\n", operation);
	for (xzm_block_index_t idx = 0; idx < chunk_capacity; ++idx) {
		const uint64_t block = _xzm_segment_offset(NULL, chunk_idx, idx,
				block_size);

		do_operation(chunk, chunk_idx, chunk_capacity, idx, block_size,
				&slice_idx, &num_slices, populate);

		const size_t left = slice_idx * XZM_SEGMENT_SLICE_SIZE;
		const size_t right = left + num_slices * XZM_SEGMENT_SLICE_SIZE;

		const size_t slice_start = rounddown(block, XZM_SEGMENT_SLICE_SIZE);
		const size_t slice_limit = roundup(block + block_size,
				XZM_SEGMENT_SLICE_SIZE);

		T_ASSERT_NE(num_slices, 0,
				"Free block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") should %s slice",
				block, block + block_size, idx, chunk_capacity, block_size,
				operation);
		T_ASSERT_EQ(left, slice_start,
				"Free block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") %s start should match slice 0x%lx",
				block, block + block_size, idx, chunk_capacity, block_size,
				operation, left);
		T_ASSERT_EQ(right, slice_limit,
				"Free block 0x%"PRIx64"-0x%"PRIx64" (%u/%u, size %"PRIu64") %s end should match slice 0x%lx",
				block, block + block_size, idx, chunk_capacity, block_size,
				operation, right);
	}
}

static void chunk_init(xzm_chunk_t chunk, xzm_slice_count_t slice_count,
		uint64_t chunk_capacity) {
	chunk->xzc_free = _xzm_xzone_free_mask(NULL, chunk_capacity);
	chunk->xzc_used = 0;
	chunk->xzc_bits.xzcb_kind = XZM_SLICE_KIND_SMALL_CHUNK;
	chunk->xzc_bits.xzcb_is_pristine = true;
	chunk->xzc_bits.xzcb_on_partial_list = false;
	chunk->xzcs_slice_count = slice_count;

	for (xzm_slice_count_t idx = 1; idx < chunk->xzcs_slice_count; ++idx) {
		xzm_slice_t slice = &chunk[idx];
		slice->xzc_bits.xzcb_kind = XZM_SLICE_KIND_MULTI_BODY;
		slice->xzsl_slice_offset_bytes = (uint32_t)(sizeof(*slice) * idx);
	}
}

typedef struct {
	xzm_slice_count_t slice_idx, num_slices;
} expected_t;

static void run_manual_tests(xzm_chunk_t chunks, xzm_slice_count_t slice_count,
		xzm_slice_count_t chunk_idx, bool populate) {
	const char *operation = get_operation_str(populate);
	xzm_slice_count_t slice_idx, num_slices;
	xzm_chunk_t chunk = &chunks[chunk_idx];

	/*
	 * slice: |<-16KiB->|<-16KiB->|<-16KiB->|<-16KiB->|
	 * block: |<-14KiB>|<-14KiB>|<-14KiB>|<-14KiB>|
	 * free:  |    0   |   1    |   1    |   0    |
	 * vm op: |    0   |   1    |   1    |   0    |
	 */
	puts("Checking block size smaller than slice size...");
	{
		const uint64_t block_size = 14336;
		_Static_assert(block_size < XZM_SEGMENT_SLICE_SIZE,
				"Block smaller than slice size");
		const uint32_t chunk_capacity = XZM_SMALL_CHUNK_SIZE / block_size;
		chunk_init(chunks, slice_count, chunk_capacity);
		chunk->xzc_free = _xzm_xzone_free_mask(NULL, chunk_capacity) & 0b0110;
		const expected_t expected[] = {{0, 0}, {1, 1}, {1, 1},  {0, 0}};
		_Static_assert(countof(expected) == chunk_capacity,
				"Correct number of expected results");
		for (xzm_block_index_t idx = 0; idx < chunk_capacity; ++idx) {
			// Can only operate on free blocks
			if (!_xzm_small_chunk_block_index_is_free(chunk, idx)) {
				continue;
			}

			do_operation(chunk, chunk_idx, chunk_capacity, idx, block_size,
				&slice_idx, &num_slices, populate);

			T_ASSERT_EQ(num_slices, expected[idx].num_slices,
					"Block %u/%u (size %"PRIu64") should %s %u slice(s)\n",
					idx, chunk_capacity, block_size, operation, expected[idx].num_slices);
			if (!expected[idx].num_slices) {
				continue;
			}

			T_ASSERT_EQ(slice_idx, expected[idx].slice_idx,
					"Block %u/%u (size %"PRIu64") should %s from slice %u\n",
					idx, chunk_capacity, block_size, operation, expected[idx].slice_idx);
		}
	}

	/*
	 * slice: |<16KiB>|<16KiB>|<16KiB>|<16KiB>|
	 * block: |<16KiB>|<16KiB>|<16KiB>|<16KiB>|
	 * free:  |   1   |   0   |   1   |   0   |
	 * vm op: |   1   |   0   |   1   |   0   |
	 */
	puts("Checking block size equal to slice size...");
	{
		const uint64_t block_size = XZM_SEGMENT_SLICE_SIZE;
		_Static_assert(block_size == XZM_SEGMENT_SLICE_SIZE,
				"Block equal to slice size");
		const uint32_t chunk_capacity = XZM_SMALL_CHUNK_SIZE / block_size;
		chunk_init(chunks, slice_count, chunk_capacity);
		chunk->xzc_free = _xzm_xzone_free_mask(NULL, chunk_capacity) & 0b0101;
		const expected_t expected[] = {{0, 1}, {0, 0}, {2, 1}, {0, 0}};
		_Static_assert(countof(expected) == chunk_capacity,
				"Correct number of expected results");
		for (xzm_block_index_t idx = 0; idx < chunk_capacity; ++idx) {
			// Can only operate on free blocks
			if (!_xzm_small_chunk_block_index_is_free(chunk, idx)) {
				continue;
			}

			do_operation(chunk, chunk_idx, chunk_capacity, idx, block_size,
				&slice_idx, &num_slices, populate);

			T_ASSERT_EQ(num_slices, expected[idx].num_slices,
					"Block %u/%u (size %"PRIu64") should %s %u slice(s)\n",
					idx, chunk_capacity, block_size, operation, expected[idx].num_slices);
			if (!expected[idx].num_slices) {
				continue;
			}

			T_ASSERT_EQ(slice_idx, expected[idx].slice_idx,
					"Block %u/%u (size %"PRIu64") should %s from slice %u\n",
					idx, chunk_capacity, block_size, operation, expected[idx].slice_idx);
		}
	}

	/*
	 * slice: |<16KiB>|<16KiB>|<16KiB>|<16KiB>|
	 * block: |<-20KiB->|<-20KiB->|<-20KiB->|
	 * free:  |    1    |    0    |    1    |
	 * vm op: |    1    |    0    |    1    |
	 */
	puts("Checking block size greater than slice size...");
	{
		const uint64_t block_size = 20480;
		_Static_assert(block_size > XZM_SEGMENT_SLICE_SIZE,
				"Block greater than slice size");
		const uint32_t chunk_capacity = XZM_SMALL_CHUNK_SIZE / block_size;
		chunk_init(chunks, slice_count, chunk_capacity);
		chunk->xzc_free = _xzm_xzone_free_mask(NULL, chunk_capacity) & 0b101;
		const expected_t expected[] = {{0, 1}, {0, 0}, {3, 1}};
		_Static_assert(countof(expected) == chunk_capacity,
				"Correct number of expected results");
		for (xzm_block_index_t idx = 0; idx < chunk_capacity; ++idx) {
			// Can only operate on free blocks
			if (!_xzm_small_chunk_block_index_is_free(chunk, idx)) {
				continue;
			}

			do_operation(chunk, chunk_idx, chunk_capacity, idx, block_size,
				&slice_idx, &num_slices, populate);

			T_ASSERT_EQ(num_slices, expected[idx].num_slices,
					"Block %u/%u (size %"PRIu64") should %s %u slice(s)\n",
					idx, chunk_capacity, block_size, operation, expected[idx].num_slices);
			if (!expected[idx].num_slices) {
				continue;
			}

			T_ASSERT_EQ(slice_idx, expected[idx].slice_idx,
					"Block %u/%u (size %"PRIu64") should %s from slice %u\n",
					idx, chunk_capacity, block_size, operation, expected[idx].slice_idx);
		}
	}
}

static void run_programmatic_tests(xzm_chunk_t chunk,
		xzm_slice_count_t slice_count, xzm_slice_count_t chunk_idx,
		bool populate) {
	/* should match _xzm_bin_sizes for sizes in the range
	 * (XZM_TINY_BLOCK_SIZE_MAX, XZM_SMALL_BLOCK_SIZE_MAX] */
	const size_t block_sizes[] = {
		5120, 6144, 7168, 8192, 10240, 12288, 14336, 16384, 20480, 24576, 28672,
		32768,
	};
	void (* const tests[])(xzm_chunk_t, xzm_slice_count_t, uint32_t, uint64_t, bool) = {
		&blocks_all_used_but_one,
		&blocks_one_used_per_slice,
		&blocks_all_free,
	};

	for (unsigned i = 0; i < countof(block_sizes); ++i) {
		const uint64_t xz_block_size = block_sizes[i];
		const uint32_t xz_chunk_capacity = XZM_SMALL_CHUNK_SIZE / xz_block_size;

		for (unsigned j = 0; j < countof(tests); ++j) {
			chunk_init(chunk, slice_count, xz_chunk_capacity);
			(tests[j])(&chunk[chunk_idx], chunk_idx, xz_chunk_capacity,
					xz_block_size, populate);
		}
	}
}

static void run_tests(bool populate) {
	struct xzm_slice_s chunk[XZM_SMALL_CHUNK_SIZE / XZM_SEGMENT_SLICE_SIZE];
	const xzm_slice_count_t slice_count = XZM_SMALL_CHUNK_SIZE /
			 XZM_SEGMENT_SLICE_SIZE;
	const xzm_slice_count_t chunk_idx = 0;

	run_manual_tests(chunk, slice_count, chunk_idx, populate);
	run_programmatic_tests(chunk, slice_count, chunk_idx, populate);
}

T_DECL(slice_on_allocate, "Slice range computation on allocation from small chunk",
		T_META_TAG_XZONE_ONLY)
{
	run_tests(true);
}

T_DECL(slice_on_deallocate, "Slice range computation on deallocation from small chunk",
		T_META_TAG_XZONE_ONLY)
{
	run_tests(false);
}

#else // CONFIG_XZONE_MALLOC

T_DECL(slice_on_allocate, "Slice range computation on allocation from small chunk",
		T_META_ENABLED(false), T_META_TAG_NO_ALLOCATOR_OVERRIDE)
{
	T_SKIP("Nothing to test for !CONFIG_XZONE_MALLOC");
}

T_DECL(slice_on_deallocate, "Slice range computation on deallocation from small chunk",
		T_META_ENABLED(false), T_META_TAG_NO_ALLOCATOR_OVERRIDE)
{
	T_SKIP("Nothing to test for !CONFIG_XZONE_MALLOC");
}

#endif // CONFIG_XZONE_MALLOC
