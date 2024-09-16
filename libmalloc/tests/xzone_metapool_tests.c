#include <darwintest.h>
#include <sys/mman.h>

#include "../src/internal.h" // MALLOC_TARGET_EXCLAVES

#if CONFIG_XZONE_MALLOC

#if MALLOC_TARGET_EXCLAVES
#include <vas/vas.h>
#else
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#endif // MALLOC_TARGET_EXCLAVES

static void *
test_mvm_allocate_plat(uintptr_t addr, size_t size, int flags,
		uint32_t debug_flags, int vm_page_label, plat_map_t *map_out)
{
#if MALLOC_TARGET_EXCLAVES
	const _liblibc_map_type_t type = LIBLIBC_MAP_TYPE_PRIVATE |
			((flags & VM_FLAGS_ANYWHERE) ? 0 : LIBLIBC_MAP_TYPE_FIXED) |
			((debug_flags & MALLOC_NO_POPULATE) ? LIBLIBC_MAP_TYPE_NOCOMMIT : 0);
	return mmap_plat(map_out, addr, size,
			LIBLIBC_MAP_PERM_READ | LIBLIBC_MAP_PERM_WRITE, type, 0,
			(unsigned)vm_page_label);
#else
	vm_address_t vm_addr;
	kern_return_t kr;

	kr = vm_allocate(mach_task_self(), &vm_addr, size,
			VM_FLAGS_ANYWHERE | VM_MAKE_TAG(vm_page_label));
	return kr == KERN_SUCCESS ? (void *)vm_addr : NULL;
#endif // MALLOC_TARGET_EXCLAVES
}
#define mvm_allocate_plat(addr, size, align, flags, debug_flags, vm_page_label, plat) \
	test_mvm_allocate_plat(addr, size, flags, debug_flags, vm_page_label, plat)


static int
test_mvm_madvise(void *addr, size_t size, int advice, plat_map_t *map)
{
#if MALLOC_TARGET_EXCLAVES
	return !madvise_plat(map, addr, size, LIBLIBC_MAP_HINT_UNUSED);
#else
	kern_return_t kr = madvise(addr, size, advice);
	return kr != - 1;
#endif // MALLOC_TARGET_EXCLAVES
}
#define mvm_madvise_plat(addr, size, advice, debug_flags, plat) \
	test_mvm_madvise(addr, size, advice, plat)

static void test_malloc_lock_lock(_malloc_lock_s *lock) {
#if MALLOC_HAS_OS_LOCK
	os_unfair_lock_lock(lock);
#else
	T_QUIET; T_ASSERT_EQ(pthread_mutex_lock(lock), 0, "Lock lock");
#endif // MALLOC_HAS_OS_LOCK
}
#define _malloc_lock_lock(lock) test_malloc_lock_lock(lock);

static void test_malloc_lock_unlock(_malloc_lock_s *lock) {
#if MALLOC_HAS_OS_LOCK
	os_unfair_lock_unlock(lock);
#else
	T_QUIET; T_ASSERT_EQ(pthread_mutex_unlock(lock), 0, "Unlock lock");
#endif // MALLOC_HAS_OS_LOCK
}
#define _malloc_lock_unlock(lock) test_malloc_lock_unlock(lock);

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

	xzm_metapool_init(&metadata_pool, 1, metadata_slab_size, metadata_size,
			metadata_size, NULL);
	xzm_metapool_init(&data_pool, 1, data_slab_size, data_size,
			data_size, &metadata_pool);

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
