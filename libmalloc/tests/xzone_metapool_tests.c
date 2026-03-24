#include <darwintest.h>
#include <sys/mman.h>

#define TESTING_METAPOOL 1
#include "xzone_testing.h"

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_PREFERRED,
		T_META_TAG_NO_ALLOCATOR_OVERRIDE);

#if CONFIG_XZONE_MALLOC

#include "../src/xzone_malloc/xzone_metapool.c"

#if !MALLOC_TARGET_EXCLAVES
bool check_page_is_dirty(void *page) {
	int disposition = 0;
	mach_vm_size_t disp_count = 1;

	kern_return_t kr = mach_vm_page_range_query(mach_task_self(),
			(mach_vm_address_t)page, KiB(16), (mach_vm_address_t)&disposition,
			&disp_count);
	T_QUIET; T_ASSERT_EQ(kr, KERN_SUCCESS, "Query page disposition");
	return disposition & VM_PAGE_QUERY_PAGE_DIRTY;
}
#endif // !MALLOC_TARGET_EXCLAVES

T_DECL(xzone_metapool_metadata,
		"Check that metapools with metadata allocator madvise blocks") {
	struct xzm_metapool_s metadata_pool = { 0 };
	struct xzm_metapool_s data_pool = { 0 };

	// Constants
	const size_t metadata_size = MAX(sizeof(struct xzm_metapool_block_s),
			sizeof(struct xzm_metapool_slab_s));
	const size_t metadata_slab_size = KiB(16);

	const size_t data_size = KiB(16);
	const size_t data_slab_size = KiB(512);
	const size_t num_data_blocks = data_slab_size / data_size;

	xzm_metapool_init(&metadata_pool, 1, VM_MEMORY_MALLOC, metadata_slab_size,
			metadata_size, metadata_size, NULL, NULL, 0);
	xzm_metapool_init(&data_pool, 1, VM_MEMORY_MALLOC, data_slab_size,
			data_size, data_size, &metadata_pool, NULL, 0);

	// Allocate a full slab of data blocks, ensure that they all came from the
	// same slab, dirty all of their pages, and then free them all. After
	// freeing, the pages should be madvised
	void *data[num_data_blocks];
	for (int i = 0; i < num_data_blocks; i++) {
		data[i] = xzm_metapool_alloc(&data_pool);
		T_ASSERT_NOTNULL(data[i], "Allocate data block");
		if (i != 0) {
			T_ASSERT_EQ((uintptr_t)data[i] - (uintptr_t)data[i-1], data_size,
				"All data blocks are from the same slab");
		}
	}

	for (int i = 0; i < num_data_blocks; i++) {
		memset(data[i], 'A', data_size);
#if !MALLOC_TARGET_EXCLAVES
		T_EXPECT_TRUE(check_page_is_dirty(data[i]),
				"Page is dirty after memset");
#endif // !MALLOC_TARGET_EXCLAVES

	}

	for (int i = 0; i < num_data_blocks; i++) {
		xzm_metapool_free(&data_pool, data[i]);
#if !MALLOC_TARGET_EXCLAVES
		T_EXPECT_FALSE(check_page_is_dirty(data[i]),
				"Freed page is not dirty");
#endif // !MALLOC_TARGET_EXCLAVES
	}
}

T_DECL(xzone_metapool_metadata_start_space,
		"Check metadata metapool start space handling") {
	struct xzm_metapool_s metadata_pool = { 0 };

	// Constants
	const size_t metadata_size = 32;
	const size_t metadata_slab_size = KiB(16);

	void *start_space_alloc = aligned_alloc(32, 32 * 5);
	T_ASSERT_NOTNULL(start_space_alloc, "aligned_alloc %p", start_space_alloc);
	bzero(start_space_alloc, 32 * 5);

	xzm_metapool_init(&metadata_pool, 1, VM_MEMORY_MALLOC, metadata_slab_size,
			metadata_size, metadata_size, NULL,
			// Give a super misaligned start
			(void *)((uint8_t *)start_space_alloc + 3),
			// And also make the end misaligned by taking 3 off the end
			(5 * 32) - (2 * 3));

	void *block1 = xzm_metapool_alloc(&metadata_pool);
	T_ASSERT_EQ(block1, (void *)((uint8_t *)start_space_alloc + 32 * 2),
			"Expected first block %p", block1);

	void *block2 = xzm_metapool_alloc(&metadata_pool);
	T_ASSERT_EQ(block2, (void *)((uint8_t *)start_space_alloc + 32 * 3),
			"Expected second block %p", block2);

	void *block3 = xzm_metapool_alloc(&metadata_pool);
	T_LOG("block3 %p", block3);

	bool elsewhere = (uintptr_t)block3 < (uintptr_t)start_space_alloc ||
			(uintptr_t)block3 >= (uintptr_t)start_space_alloc + 5 * 32;
	T_ASSERT_TRUE(elsewhere, "block3 is from somewhere else");

	xzm_metapool_free(&metadata_pool, block2);
	void *block4 = xzm_metapool_alloc(&metadata_pool);
	T_ASSERT_EQ(block4, block2, "block2 free roundtrip");

	xzm_metapool_free(&metadata_pool, block3);
	void *block5 = xzm_metapool_alloc(&metadata_pool);
	T_ASSERT_EQ(block5, block3, "block3 free roundtrip");

	free(start_space_alloc);
}

T_DECL(xzone_metapool_slab_overflow,
		"Check metadata metapool slab overflow handling") {
	struct xzm_metapool_s pool = { 0 };

	// There's space for two 7kb slots in a 16kb slab - the first will be
	// occupied by the inline slab metadata and the second by the actual block
	// on the slab
	uint32_t block_align = 0;
	uint32_t block_size = 7000;
	uint32_t slab_size = KiB(16);
	xzm_metapool_init(&pool, 1, VM_MEMORY_MALLOC, slab_size,
			block_align, block_size, NULL, NULL, 0);

	void *block1 = xzm_metapool_alloc(&pool);
	void *block2 = xzm_metapool_alloc(&pool);

	T_ASSERT_NE((uintptr_t)block1 & ~(KiB(16) - 1),
			(uintptr_t)block2 & ~(KiB(16) - 1),
			"blocks should be on different pages");

	xzm_metapool_free(&pool, block1);
	xzm_metapool_free(&pool, block2);

	T_PASS("surived alloc + free roundtrip");
}
#else // CONFIG_XZONE_MALLOC

T_DECL(xzm_metapool_not_supported, "xzone metapool not supported")
{
	T_SKIP("xzone metapool not supported on this platform");
}

#endif // CONFIG_XZONE_MALLOC
