#include <darwintest.h>
#include <sys/mman.h>

#define TESTING_METAPOOL 1
#include "xzone_testing.h"

#if CONFIG_XZONE_MALLOC

#include "../src/xzone/xzone_metapool.c"

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
			metadata_size, metadata_size, NULL);
	xzm_metapool_init(&data_pool, 1, VM_MEMORY_MALLOC, data_slab_size,
			data_size, data_size, &metadata_pool);

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

#else // CONFIG_XZONE_MALLOC

T_DECL(xzm_metapool_not_supported, "xzone metapool not supported")
{
	T_SKIP("xzone metapool not supported on this platform");
}

#endif // CONFIG_XZONE_MALLOC
