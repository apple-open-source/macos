/* ----------------------------------------------------------------------------
Copyright (c) 2018-2022, Microsoft Research, Daan Leijen
Copyright © 2025 Apple Inc.
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" in the same directory as this file.
-----------------------------------------------------------------------------*/

#include "../internal.h"

#if CONFIG_XZONE_MALLOC

void
xzm_metapool_init(xzm_metapool_t mp, xzm_metapool_id_t pool_id, uint8_t vm_tag,
		uint32_t slab_size, uint32_t block_align, uint32_t block_size,
		xzm_metapool_t metadata_pool)
{
	if (block_align) {
		xzm_debug_assert(slab_size % block_size == 0);
		xzm_debug_assert(powerof2(block_size));
		xzm_debug_assert(powerof2(block_align));
		xzm_debug_assert(block_size >= block_align);
	}
	xzm_debug_assert(slab_size % PAGE_MAX_SIZE == 0);

	// In the inline case, the slab metadata must fit in the first block
	xzm_debug_assert(metadata_pool ||
			block_size >= sizeof(struct xzm_metapool_slab_s));

	// In the out of line case, the metadata metapool must serve blocks big
	// enough to be slabs or blocks
	xzm_debug_assert(!metadata_pool ||
			metadata_pool->xzmp_block_size >= sizeof(struct xzm_metapool_slab_s));
	xzm_debug_assert(!metadata_pool ||
			metadata_pool->xzmp_block_size >= sizeof(struct xzm_metapool_block_s));

	*mp = (struct xzm_metapool_s){
		.xzmp_lock = _MALLOC_LOCK_INIT,
		.xzmp_id = pool_id,
		.xzmp_vm_tag = vm_tag,
		.xzmp_slab_size = slab_size,
		.xzmp_slab_limit = (slab_size / block_size) * block_size,
		.xzmp_block_align = block_align,
		.xzmp_block_size = block_size,
		.xzmp_metadata_metapool = metadata_pool,
	};
}

static bool
_xzm_metapool_should_madvise(xzm_metapool_t mp)
{
	return (mp->xzmp_metadata_metapool) &&
			(mp->xzmp_block_size >= vm_page_size);
}

// Only used in exclaves and the debug dylib
static xzm_metapool_slab_t __unused
_xzm_metapool_slab_for_block(xzm_metapool_t mp, void *blockp)
{
	uintptr_t block_ptr = (uintptr_t)blockp;
	xzm_metapool_slab_t slab = NULL;
	SLIST_FOREACH(slab, &mp->xzmp_slabs, xzmps_entry) {
		uintptr_t slab_base = (uintptr_t)slab->xzmps_base;
		uintptr_t slab_limit = (uintptr_t)(
				slab->xzmps_base + mp->xzmp_slab_size);
		if (block_ptr >= slab_base && block_ptr < slab_limit) {
			xzm_debug_assert(!((block_ptr - slab_base) % mp->xzmp_block_size));
			return slab;
		}
	}
	return NULL;
}

void *
xzm_metapool_alloc(xzm_metapool_t mp)
{
	xzm_metapool_block_t block_meta = NULL;
	uint8_t *block = NULL;
	_malloc_lock_lock(&mp->xzmp_lock);

	block_meta = SLIST_FIRST(&mp->xzmp_blocks);
	if (block_meta) {
		SLIST_REMOVE_HEAD(&mp->xzmp_blocks, xzmpb_entry);
		block = block_meta->xzmpb_base;
		if (mp->xzmp_metadata_metapool) {
			xzm_metapool_free(mp->xzmp_metadata_metapool, block_meta);
		} else {
			*block_meta = (struct xzm_metapool_block_s){ 0 };
		}
		goto done;
	}

	if (!mp->xzmp_current_slab ||
			mp->xzmp_current_block == mp->xzmp_slab_limit) {
		size_t allocation_size = (size_t)mp->xzmp_slab_size;
		uint8_t align = mp->xzmp_block_align ?
				(uint8_t)__builtin_ctz(mp->xzmp_block_align) : 0;
		uint32_t debug_flags = MALLOC_GUARDED_METADATA;
		void *vm_addr = mvm_allocate_plat(0, allocation_size, align,
				VM_FLAGS_ANYWHERE, debug_flags, VM_MEMORY_MALLOC,
				mvm_plat_map(map));
		if (vm_addr == NULL) {
			xzm_client_abort(
					"failed to allocate malloc metadata, out of virtual address"
					" space, client likely has a memory leak");
		}

		if (mp->xzmp_vm_tag != VM_MEMORY_MALLOC) {
			// Re-tag the region with the actual desired tag (we need to use
			// VM_MEMORY_MALLOC first to get placed as metadata)

			mach_vm_address_t overwrite_vm_addr = (mach_vm_address_t)vm_addr;
			mach_vm_size_t overwrite_vm_size = (mach_vm_size_t)allocation_size;
			mach_vm_offset_t overwrite_mask = mp->xzmp_block_align ?
					mp->xzmp_block_align - 1 : 0;
			int alloc_flags = VM_FLAGS_OVERWRITE | VM_MAKE_TAG(mp->xzmp_vm_tag);
			kern_return_t kr = mach_vm_map(mach_task_self(), &overwrite_vm_addr,
					overwrite_vm_size, overwrite_mask, alloc_flags,
					MEMORY_OBJECT_NULL, /* offset */ 0,
					/* copy */ false, VM_PROT_DEFAULT, VM_PROT_ALL,
					VM_INHERIT_DEFAULT);
			if (kr != KERN_SUCCESS) {
				xzm_abort("Failed to overwrite malloc metadata");
			}
		}

		xzm_metapool_slab_t new_slab;
		if (mp->xzmp_metadata_metapool) {
			new_slab = xzm_metapool_alloc(mp->xzmp_metadata_metapool);
			mp->xzmp_current_block = 0;
		} else {
			new_slab = (xzm_metapool_slab_t)vm_addr;
			mp->xzmp_current_block = mp->xzmp_block_size;
		}

		*new_slab = (struct xzm_metapool_slab_s) {
			.xzmps_base = (uint8_t *)vm_addr,
		};

		mp->xzmp_current_slab = new_slab;
		SLIST_INSERT_HEAD(&mp->xzmp_slabs, new_slab, xzmps_entry);
	}

	uint8_t *current_base = mp->xzmp_current_slab->xzmps_base;
	block = (void *)(current_base + mp->xzmp_current_block);
	mp->xzmp_current_block += mp->xzmp_block_size;


done:
	_malloc_lock_unlock(&mp->xzmp_lock);

	xzm_debug_assert(block);
	return block;
}

#ifdef DEBUG
static bool
_xzm_metapool_block_is_allocated(xzm_metapool_t mp, void *blockp)
{
	xzm_metapool_slab_t slab = _xzm_metapool_slab_for_block(mp, blockp);
	if (!slab) {
		// We didn't find it in any of the slabs
		return false;
	}

	// check if it's on the freelist
	xzm_metapool_block_t cur_block = NULL;
	SLIST_FOREACH(cur_block, &mp->xzmp_blocks, xzmpb_entry) {
		if (cur_block->xzmpb_base == blockp) {
			return false;
		}
	}

	return true;
}
#endif // DEBUG

void
xzm_metapool_free(xzm_metapool_t mp, void *blockp)
{
	_malloc_lock_lock(&mp->xzmp_lock);

	xzm_debug_assert(_xzm_metapool_block_is_allocated(mp, blockp));

	xzm_metapool_block_t block_meta;
	if (mp->xzmp_metadata_metapool) {
		block_meta = xzm_metapool_alloc(mp->xzmp_metadata_metapool);
		if (_xzm_metapool_should_madvise(mp)) {
			plat_map_t *map_ptr = NULL;
			mvm_madvise_plat(blockp, mp->xzmp_block_size, MADV_FREE_REUSABLE, 0,
					map_ptr);
		}
	} else{
		block_meta = (xzm_metapool_block_t)blockp;
	}
	block_meta->xzmpb_base = blockp;
	SLIST_INSERT_HEAD(&mp->xzmp_blocks, block_meta, xzmpb_entry);

	_malloc_lock_unlock(&mp->xzmp_lock);
}

#endif // CONFIG_XZONE_MALLOC
