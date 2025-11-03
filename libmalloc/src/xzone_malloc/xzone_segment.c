/* ----------------------------------------------------------------------------
Copyright (c) 2018-2022, Microsoft Research, Daan Leijen
Copyright © 2025 Apple Inc.
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" in the same directory as this file.
-----------------------------------------------------------------------------*/

#include "../internal.h"

#if CONFIG_XZONE_MALLOC

static void
xzm_madvise(xzm_malloc_zone_t zone, uint8_t *start, size_t size);

static void
_xzm_segment_group_segment_deallocate(xzm_segment_group_t sg,
		xzm_segment_t segment, bool free_from_table);

#pragma mark segment map

// mimalloc: _mi_segment_map_allocated_at
static void
_xzm_segment_table_allocated_at(xzm_main_malloc_zone_t main, void *data,
		xzm_segment_t metadata, bool normal)
{
	xzm_debug_assert((uintptr_t)data % XZM_SEGMENT_SIZE == 0);

	void *segment_end = _xzm_segment_end(metadata);
	xzm_debug_assert((uintptr_t)data < (uintptr_t)segment_end);

	xzm_segment_table_entry_s entry_val =
			_xzm_segment_to_segment_table_entry(metadata, normal);

	while (data < segment_end) {
#if CONFIG_EXTERNAL_METADATA_LARGE
		// If this allocation is in a new 64GB granule, allocate a new leaf
		// table to store the metadata pointers in
		size_t ext_idx = 0;
		__assert_only size_t index = _xzm_segment_table_index_of(data, &ext_idx);
		xzm_debug_assert(index < XZM_SEGMENT_TABLE_ENTRIES);
		xzm_debug_assert(ext_idx < XZM_EXTENDED_SEGMENT_TABLE_ENTRIES);

		if (ext_idx != 0) {
			xzm_extended_segment_table_entry_s *ext_addr =
					&main->xzmz_extended_segment_table[ext_idx];
			xzm_extended_segment_table_entry_s ext_entry = { 0 };
			ext_entry = os_atomic_load(ext_addr, relaxed);
			if (ext_entry.xeste_val == 0) {
				// Need to allocate a new segment table since this pointer is in
				// a new segment table (64GB span)
				_malloc_lock_lock(&main->xzmz_extended_segment_table_lock);
				// Load the table entry again to see if another thread populated
				// it while we were acquiring the lock
				ext_entry = os_atomic_load(ext_addr, relaxed);
				if (ext_entry.xeste_val == 0) {
					xzm_metapool_t mp;
					mp = &main->xzmz_metapools[XZM_METAPOOL_SEGMENT_TABLE];
					void *leaf_table = xzm_metapool_alloc(mp);
					xzm_assert(leaf_table);
					xzm_debug_assert(((uintptr_t)leaf_table /
							XZM_SEGMENT_TABLE_ALIGN) <= UINT32_MAX);
					ext_entry.xeste_val = (uint32_t)((uintptr_t)leaf_table /
							XZM_SEGMENT_TABLE_ALIGN);
					os_atomic_store(ext_addr, ext_entry, relaxed);
				}
				_malloc_lock_unlock(&main->xzmz_extended_segment_table_lock);
			}
		}
#endif // CONFIG_EXTERNAL_METADATA_LARGE

		xzm_segment_table_entry_s *entry;
		entry = _xzm_ptr_to_table_entry(data, main);
		xzm_debug_assert(entry != NULL);

		xzm_debug_assert(entry->xste_val == 0);

		// Store-release to publish the segment and chunk initializations
		// TODO: document all paired dependency/acquire loads
		os_atomic_store(entry, entry_val, release);

		data = (void *)((uintptr_t)data + XZM_SEGMENT_SIZE);
	}
}

// mimalloc: _mi_segment_map_freed_at
static void
_xzm_segment_table_freed_at(xzm_main_malloc_zone_t main, void *data,
		xzm_segment_t metadata, __assert_only bool full_segment)
{
	void *end = _xzm_segment_end(metadata);
	xzm_debug_assert(!full_segment ||
			_xzm_segment_start(metadata) == data);
	while (data < end) {
		xzm_segment_table_entry_s *entry;
		entry = _xzm_ptr_to_table_entry(data, main);
		xzm_debug_assert(entry != NULL);
		xzm_debug_assert(_xzm_segment_to_segment_table_entry(metadata, false).xste_val ==
				entry->xste_val);
		xzm_segment_table_entry_s null_entry;
		null_entry = _xzm_segment_to_segment_table_entry(NULL, false);
		os_atomic_store(entry, null_entry, relaxed);

		data = (void *)((uintptr_t)data + XZM_SEGMENT_SIZE);
	}
}

#pragma mark vm reclaim

#if CONFIG_XZM_DEFERRED_RECLAIM

static struct xzm_reclaim_buffer_s xzm_reclaim_buffer;

static bool
_xzm_reclaim_id_cache_is_empty(xzm_reclaim_id_cache_t cache)
{
	return cache->ric_head == 0;
}

static uint64_t
_xzm_reclaim_id_cache_pop(xzm_reclaim_id_cache_t cache)
{
	xzm_debug_assert(!_xzm_reclaim_id_cache_is_empty(cache));
	uint64_t id = cache->ric_ids[--cache->ric_head];
	xzm_debug_assert(id != VM_RECLAIM_ID_NULL);
	return id;
}

static void
_xzm_reclaim_id_cache_push(xzm_reclaim_id_cache_t cache, mach_vm_reclaim_id_t id)
{
	xzm_assert(cache->ric_head < cache->ric_len);
	xzm_debug_assert(id != VM_RECLAIM_ID_NULL);
	cache->ric_ids[cache->ric_head++] = id;
}

static void
_xzm_reclaim_id_cache_init(xzm_reclaim_buffer_t buffer)
{
	xzm_reclaim_id_cache_t id_cache = &buffer->xrb_id_cache;
	mach_vm_reclaim_count_t max_buffer_count;
	mach_vm_reclaim_error_t kr = mach_vm_reclaim_ring_capacity(
			buffer->xrb_ringbuffer, &max_buffer_count);
	xzm_assert(kr == VM_RECLAIM_SUCCESS);
	size_t min_id_cache_size =
			max_buffer_count * sizeof(mach_vm_reclaim_id_t);
	size_t id_cache_size = round_page(min_id_cache_size);
	if (id_cache->ric_ids == NULL ||
			id_cache->ric_len < max_buffer_count) {
		mach_vm_reclaim_id_t *ids = (mach_vm_reclaim_id_t *)
				mvm_allocate_pages(id_cache_size, 0, MALLOC_ABORT_ON_ERROR,
				VM_MEMORY_MALLOC);
		if (id_cache->ric_ids != NULL) {
			// Deallocate the old cache
			mvm_deallocate_pages((void *)(id_cache->ric_ids),
					id_cache->ric_len * sizeof(mach_vm_reclaim_id_t),
					MALLOC_ABORT_ON_ERROR);
		}
		id_cache->ric_ids = ids;
		id_cache->ric_len = id_cache_size / sizeof(mach_vm_reclaim_id_t);
	}
	id_cache->ric_head = 0;
	xzm_debug_assert(id_cache->ric_len >= max_buffer_count);
}

bool
xzm_reclaim_init(xzm_main_malloc_zone_t main,
		mach_vm_reclaim_count_t initial_count, mach_vm_reclaim_count_t max_count)
{
	// Pick a sane minimum number of entries and let vm_reclaim round up
	// to a page boundary. The intention is for the initial size to be
	// one page.
	mach_vm_reclaim_count_t buffer_capacity =
			mach_vm_reclaim_round_capacity(initial_count);
	mach_vm_reclaim_count_t max_buffer_capacity =
			mach_vm_reclaim_round_capacity(max_count);
	xzm_reclaim_buffer.xrb_id_cache.ric_len = 0;
	xzm_reclaim_buffer.xrb_id_cache.ric_ids = NULL;
	_malloc_lock_init(&xzm_reclaim_buffer.xrb_lock);
	mach_vm_reclaim_error_t err = mach_vm_reclaim_ring_allocate(
			&xzm_reclaim_buffer.xrb_ringbuffer, buffer_capacity,
			max_buffer_capacity);
	if (err == VM_RECLAIM_SUCCESS) {
		xzm_reclaim_buffer.xrb_len = buffer_capacity;
		main->xzmz_reclaim_buffer = &xzm_reclaim_buffer;
		_xzm_reclaim_id_cache_init(&xzm_reclaim_buffer);
	} else {
		malloc_report(ASL_LEVEL_ERR,
				"xzm: failed to initialize deferred "
				"reclamation buffer [%d] %s\n",
				err_get_code(err), mach_error_string(err));
	}
	return (err == VM_RECLAIM_SUCCESS);
}

static mach_vm_reclaim_state_t
_xzm_reclaim_mark_used_locked(xzm_reclaim_buffer_t buffer,
		mach_vm_reclaim_id_t id, uint8_t *addr, size_t size, bool reusable,
		bool *update_accounting_out)
{
	mach_vm_reclaim_error_t err;
	mach_vm_reclaim_state_t state;

	xzm_debug_assert(size <= UINT32_MAX);
	mach_vm_reclaim_action_t behavior = reusable ?
			VM_RECLAIM_FREE : VM_RECLAIM_DEALLOCATE;

	err = mach_vm_reclaim_try_cancel(buffer->xrb_ringbuffer, id,
			(mach_vm_address_t)addr, (mach_vm_size_t)size,
			behavior, &state, update_accounting_out);
	xzm_assert(err == VM_RECLAIM_SUCCESS);

	if (state == VM_RECLAIM_UNRECLAIMED) {
		_xzm_reclaim_id_cache_push(&buffer->xrb_id_cache, id);
	}

	return state;
}

static mach_vm_reclaim_state_t
_xzm_reclaim_mark_used(xzm_reclaim_buffer_t buffer, mach_vm_reclaim_id_t id,
		uint8_t *addr, size_t size, bool reusable)
{
	bool update_accounting = false;

	_malloc_lock_lock(&buffer->xrb_lock);

	mach_vm_reclaim_state_t state = _xzm_reclaim_mark_used_locked(buffer, id,
			addr, size, reusable, &update_accounting);

	_malloc_lock_unlock(&buffer->xrb_lock);

	if (update_accounting) {
		__assert_only mach_vm_reclaim_error_t err =
				mach_vm_reclaim_update_kernel_accounting(buffer->xrb_ringbuffer);
		xzm_debug_assert(err == VM_RECLAIM_SUCCESS);
	}

	return state;
}

static bool
_xzm_reclaim_is_reusable(xzm_reclaim_buffer_t buffer, mach_vm_reclaim_id_t reclaim_id, bool deallocate)
{
	mach_vm_reclaim_error_t err;
	mach_vm_reclaim_state_t state;
	err = mach_vm_reclaim_query_state(buffer->xrb_ringbuffer, reclaim_id,
			deallocate ? VM_RECLAIM_DEALLOCATE : VM_RECLAIM_FREE, &state);
	xzm_assert(err == VM_RECLAIM_SUCCESS);
	return mach_vm_reclaim_is_reusable(state);
}

uint64_t
xzm_reclaim_mark_free_locked(xzm_reclaim_buffer_t buffer, uint8_t *addr,
		size_t size, bool reusable, bool *update_accounting_out)
{
	mach_vm_reclaim_error_t kr;
	mach_vm_reclaim_id_t id;
	mach_vm_address_t vm_addr = (mach_vm_address_t)addr;
	uint32_t vm_size = (uint32_t)size;
	xzm_debug_assert(size <= UINT32_MAX);
	xzm_debug_assert(vm_addr % XZM_SEGMENT_SLICE_SIZE == 0);
	xzm_debug_assert(vm_size % XZM_SEGMENT_SLICE_SIZE == 0);
#ifdef DEBUG
	_malloc_lock_assert_owner(&buffer->xrb_lock);
#endif // DEBUG

	mach_vm_reclaim_action_t behavior = reusable ?
			VM_RECLAIM_FREE : VM_RECLAIM_DEALLOCATE;

	while (!_xzm_reclaim_id_cache_is_empty(&buffer->xrb_id_cache)) {
		id = _xzm_reclaim_id_cache_pop(&buffer->xrb_id_cache);
		kr = mach_vm_reclaim_try_enter(
				buffer->xrb_ringbuffer,
				vm_addr, vm_size, behavior, &id,
				update_accounting_out);
		xzm_assert(kr == VM_RECLAIM_SUCCESS);
		if (id != VM_RECLAIM_ID_NULL) {
			goto done;
		}
	}
	do {
		id = VM_RECLAIM_ID_NULL;
		kr = mach_vm_reclaim_try_enter(buffer->xrb_ringbuffer, vm_addr, vm_size,
				behavior, &id, update_accounting_out);
		xzm_assert(kr == VM_RECLAIM_SUCCESS);
		if (id == VM_RECLAIM_ID_NULL) {
			// If the ringbuffer is full, reap all of its contents and resize
			xzm_reclaim_sync_and_resize(buffer);
		}
	} while (id == VM_RECLAIM_ID_NULL);

done:
	return id;
}

static uint64_t
_xzm_reclaim_mark_free(xzm_reclaim_buffer_t buffer, uint8_t *addr, size_t size,
		bool reusable)
{
	uint64_t id;
	bool should_update_kernel_accounting = false;

	_malloc_lock_lock(&buffer->xrb_lock);

	id = xzm_reclaim_mark_free_locked(buffer, addr, size, reusable,
			&should_update_kernel_accounting);

	_malloc_lock_unlock(&buffer->xrb_lock);

	if (should_update_kernel_accounting) {
		__assert_only mach_vm_reclaim_error_t kr =
				mach_vm_reclaim_update_kernel_accounting(buffer->xrb_ringbuffer);
		xzm_debug_assert(kr == VM_RECLAIM_SUCCESS);
	}
	return id;
}

static bool
xzm_reclaim_mark_smaller(xzm_reclaim_buffer_t buffer, uint64_t *front_id,
		uint64_t *back_id, uint8_t *front_start, size_t front_free_size,
		size_t used_size, size_t back_free_size, bool deferred, bool pristine,
		bool reusable)
{
	const size_t span_size = front_free_size + used_size + back_free_size;
	xzm_debug_assert(span_size <= UINT32_MAX);

	bool should_update_used = false;
	bool should_update_free_front = false, should_update_free_back = false;

	_malloc_lock_lock(&buffer->xrb_lock);

	bool usable = true;
	mach_vm_reclaim_state_t state;
	if (deferred) {
		xzm_debug_assert(*front_id != VM_RECLAIM_ID_NULL);
		// Mark the entire span as used
		state = _xzm_reclaim_mark_used_locked(buffer, *front_id, front_start,
				span_size, reusable, &should_update_used);
		usable = mach_vm_reclaim_is_reusable(state);
		if (usable) {
			*front_id = VM_RECLAIM_ID_NULL;
		}
	}
	if (usable) {
		if (front_free_size && !pristine) {
			// Mark the front as free. Note that it already has a reclaim id
			xzm_debug_assert(*front_id == VM_RECLAIM_ID_NULL);
			*front_id = xzm_reclaim_mark_free_locked(buffer, front_start,
					front_free_size, reusable, &should_update_free_front);
		}

		if (back_free_size) {
			xzm_debug_assert(back_id);
			if (!pristine) {
				// Mark the back as free
				uint8_t *back_start = front_start + front_free_size + used_size;
				*back_id = xzm_reclaim_mark_free_locked(buffer, back_start,
						back_free_size, reusable, &should_update_free_back);
			} else {
				// Initialize the reclaim id now, because when the span metadata
				// is updated, it cannot overwrite any reclaim id we set
				*back_id = VM_RECLAIM_ID_NULL;
			}
		}
	}

	_malloc_lock_unlock(&buffer->xrb_lock);

	if (should_update_used || should_update_free_front ||
			should_update_free_back) {
		mach_vm_reclaim_update_kernel_accounting(buffer->xrb_ringbuffer);
	}

	return usable;
}

void
xzm_reclaim_force_sync(xzm_reclaim_buffer_t buffer)
{
	// This function is called in a loop when reclaim_mark_used fails while
	// trying to free a span in the reclaim buffer.
	mach_vm_reclaim_count_t capacity;
	__assert_only mach_vm_reclaim_error_t err;
	err = mach_vm_reclaim_ring_capacity(buffer->xrb_ringbuffer, &capacity);
	xzm_assert(err == VM_RECLAIM_SUCCESS);
	err = mach_vm_reclaim_ring_flush(buffer->xrb_ringbuffer, capacity);
	xzm_assert(err == VM_RECLAIM_SUCCESS);
}

void
xzm_reclaim_sync_and_resize(xzm_reclaim_buffer_t buffer)
{
	mach_vm_reclaim_error_t kr;
	mach_vm_reclaim_count_t count;
	kr = mach_vm_reclaim_ring_capacity(buffer->xrb_ringbuffer, &count);
	xzm_assert(kr == VM_RECLAIM_SUCCESS);
	mach_vm_reclaim_count_t new_count =
			mach_vm_reclaim_round_capacity(2 * count);

	kr = mach_vm_reclaim_ring_resize(buffer->xrb_ringbuffer, new_count);
	if (kr == VM_RECLAIM_SUCCESS) {
		_xzm_reclaim_id_cache_init(buffer);
	} else {
		// Must explicitly flush if the resize operation failed
		xzm_reclaim_force_sync(buffer);
	}
}

#endif // CONFIG_XZM_DEFERRED_RECLAIM

#pragma mark range group

OS_OPTIONS(xzm_range_group_alloc_flags, uint32_t,
	XZM_RANGE_GROUP_ALLOC_FLAGS_HUGE = 1 << 0,
	XZM_RANGE_GROUP_ALLOC_FLAGS_PURGEABLE = 1 << 1,
#if CONFIG_MTE
	XZM_RANGE_GROUP_ALLOC_FLAGS_MTE = 1 << 2,
#endif
);

static int
_xzm_range_group_vm_tag_for_segment(size_t size, bool huge)
{
	// Note: although there is already a VM_MEMORY_MALLOC_HUGE tag, which has
	// been there since prehistory, we'll use LARGE for huge segments to ensure
	// that any special handling from the kernel or other tools works exactly as
	// before (e.g. VM_MEMORY_MALLOC_HUGE is not included in
	// vm_memory_malloc_no_cow_mask)
	//
	// We use VM_MEMORY_MALLOC_SMALL for normal segment allocations so that they
	// are easily distinguisable from metadata allocations purely by tag.
	return huge ? VM_MEMORY_MALLOC_LARGE : VM_MEMORY_MALLOC_SMALL;
}

static void * __alloc_size(2)
_xzm_range_group_alloc_mvm_segment(xzm_main_malloc_zone_t main, size_t size,
		size_t align, plat_map_t *map, xzm_range_group_alloc_flags_t rga_flags)
{
	bool huge = (rga_flags & XZM_RANGE_GROUP_ALLOC_FLAGS_HUGE);
	bool purgeable = (rga_flags & XZM_RANGE_GROUP_ALLOC_FLAGS_PURGEABLE);

	uint32_t flags = 0;
	if (os_unlikely(purgeable)) {
		flags |= MALLOC_PURGEABLE;
	}

#if XZM_NARROW_BUCKETING
	// If we're doing narrow bucketing, and we ourselves aren't enabling
	// VM user ranges, but we've detected that VM user ranges are active in the
	// address space (<-> entropic_base is set), we want to pass DISABLE_ASLR to
	// skip the mvm-layer ASLR, which would cause our allocations to be placed
	// at the opposite end of the heap range from other pure data allocations
	// and use an additional PTE
	if (main->xzmz_narrow_bucketing && !main->xzmz_use_ranges && entropic_base) {
		flags |= DISABLE_ASLR;
	}
#endif

#if CONFIG_MTE
	if (rga_flags & XZM_RANGE_GROUP_ALLOC_FLAGS_MTE) {
		flags |= MALLOC_MTE_TAGGABLE;
	}
#endif

	int tag = _xzm_range_group_vm_tag_for_segment(size, huge);
	if (os_likely(align == 0)) {
		return mvm_allocate_pages_plat(size, XZM_SEGMENT_SHIFT, flags, tag, map);
	} else {
		// mvm_allocate_pages_plat takes the log2 of the alignment
		size_t align_pow = __builtin_ctzl(align);
		xzm_debug_assert(align_pow < UINT8_MAX);
		align_pow = MAX(align_pow, XZM_SEGMENT_SHIFT);
		return mvm_allocate_pages_plat(size, align_pow, flags, tag, map);
	}
}

MALLOC_USED
static void * __alloc_size(1)
_xzm_range_group_alloc_anywhere_segment(mach_vm_address_t hint, size_t size,
		size_t align, plat_map_t *map, xzm_range_group_alloc_flags_t rga_flags)
{
	bool huge = (rga_flags & XZM_RANGE_GROUP_ALLOC_FLAGS_HUGE);
	bool purgeable = (rga_flags & XZM_RANGE_GROUP_ALLOC_FLAGS_PURGEABLE);

	(void)map;
	int tag = _xzm_range_group_vm_tag_for_segment(size, huge);

	mach_vm_address_t vm_addr = hint;
	mach_vm_size_t allocation_size = (mach_vm_size_t)size;
	int flags = VM_FLAGS_ANYWHERE | VM_MAKE_TAG(tag);
	if (os_unlikely(purgeable)) {
		flags |= VM_FLAGS_PURGABLE;
	}

#if CONFIG_MTE
	if (rga_flags & XZM_RANGE_GROUP_ALLOC_FLAGS_MTE) {
		flags |= VM_FLAGS_MTE;
	}
#endif

	align = MAX(align, XZM_SEGMENT_SIZE);
	// alignment must be a power of 2 for the allocation mask to work
	xzm_debug_assert(powerof2(align));
	mach_vm_offset_t allocation_mask = (mach_vm_offset_t)align - 1;
	kern_return_t kr = mach_vm_map(mach_task_self(), &vm_addr, allocation_size,
			allocation_mask, flags, MEMORY_OBJECT_NULL, 0, FALSE,
			VM_PROT_DEFAULT, VM_PROT_ALL, VM_INHERIT_DEFAULT);
	if (kr) {
		if (kr != KERN_NO_SPACE) {
			xzm_debug_abort_with_reason("Failed to allocate data segment", kr);
			malloc_zone_error(0, false,
					"Failed to allocate segment (size=%lu, flags=%x, kr=%d)\n",
					(unsigned long)size, flags, kr);
		}

		return NULL;
	}

	xzm_debug_assert(vm_addr);
	xzm_debug_assert(vm_addr % align == 0);
	return (void *)vm_addr;
}

static uintptr_t
_xzm_range_group_bump_alloc_segment(xzm_range_group_t rg, size_t size,
		bool warn_on_exhaustion)
{
	uintptr_t segment_addr = 0;

	if (rg->xzrg_warned_full) {
		return segment_addr;
	}

	// Reserve space for a new segment
	_malloc_lock_lock(&rg->xzrg_lock);
	if (rg->xzrg_remaining >= size) {
		if (rg->xzrg_next == rg->xzrg_skip_addr) {
			if (rg->xzrg_direction == XZM_FRONT_INCREASING) {
				rg->xzrg_next += rg->xzrg_skip_size;
			} else {
				xzm_debug_assert(rg->xzrg_direction == XZM_FRONT_DECREASING);
				rg->xzrg_next -= rg->xzrg_skip_size;
			}
		}

		// In the decreasing direction, xzrg_next points to the _end_ of what
		// will be the next segment we serve, and we subtract the size to be
		// allocated from its initial value.  In the increasing direction, the
		// initial value is the start of the segment we're going to serve, and
		// we increase afterward.
		if (rg->xzrg_direction == XZM_FRONT_DECREASING) {
			rg->xzrg_next -= size;
		}

		segment_addr = rg->xzrg_next;
		xzm_debug_assert(segment_addr % size == 0);

		if (rg->xzrg_direction == XZM_FRONT_INCREASING) {
			rg->xzrg_next += size;
		}

		rg->xzrg_remaining -= size;
	}

	if (!segment_addr && warn_on_exhaustion) {
		if (!rg->xzrg_warned_full) {
			rg->xzrg_warned_full = true;
			malloc_report(ASL_LEVEL_WARNING, "Failed to allocate segment from range group - out of space\n");
		}
	}

	_malloc_lock_unlock(&rg->xzrg_lock);

	return segment_addr;
}

static void * __alloc_size(2)
_xzm_range_group_alloc_data_segment(xzm_range_group_t rg, size_t size,
		size_t alignment, plat_map_t *map, xzm_range_group_alloc_flags_t rga_flags)
{
	xzm_debug_assert(rg->xzrg_id == XZM_RANGE_GROUP_DATA);

#if   CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES
	if (rg->xzrg_main_ref->xzmz_use_ranges) {
		// On systems with VM user ranges, an ANYWHERE allocation with one of the
		// VM_MEMORY_MALLOC tags will be placed in the data range automatically.
		mach_vm_address_t hint = 0;

#if CONFIG_MACOS_RANGES
		// On macOS, the data range isn't strongly isolated.  We just choose an
		// otherwise empty normal range of the address space to allocate into
		// using a hint.
		hint = rg->xzrg_base;
#endif // CONFIG_MACOS_RANGES

		return _xzm_range_group_alloc_anywhere_segment(hint, size, alignment,
				map, rga_flags);
	}
#endif // CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES

	return _xzm_range_group_alloc_mvm_segment(rg->xzrg_main_ref, size,
			alignment, map, rga_flags);
}

static void * __alloc_size(2)
_xzm_range_group_alloc_ptr_segment(xzm_range_group_t rg, size_t size,
		plat_map_t *map, xzm_range_group_alloc_flags_t rga_flags)
{
	xzm_debug_assert(rg->xzrg_id == XZM_RANGE_GROUP_PTR);
	xzm_debug_assert(!(rga_flags & XZM_RANGE_GROUP_ALLOC_FLAGS_HUGE));
	xzm_debug_assert(size == XZM_SEGMENT_SIZE);
	xzm_debug_assert(rg->xzrg_main_ref->xzmz_segment_group_count !=
			XZM_SEGMENT_GROUP_IDS_COUNT_DATA_ONLY);

#if   CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES
	if (rg->xzrg_main_ref->xzmz_use_ranges)
#else
	if ((0))
#endif // MALLOC_TARGET_EXCLAVES
	{
		bool allow_fallback = false;
#if CONFIG_MALLOC_PROCESS_IDENTITY && CONFIG_MACOS_RANGES
		if (!malloc_process_is_security_critical(malloc_process_identity)) {
			allow_fallback = true;
		}
#endif // CONFIG_MALLOC_PROCESS_IDENTITY && CONFIG_MACOS_RANGES

		mach_vm_address_t segment_addr = _xzm_range_group_bump_alloc_segment(rg,
				size, !allow_fallback);
		if (!segment_addr) {
#if CONFIG_MALLOC_PROCESS_IDENTITY && CONFIG_MACOS_RANGES
			if (allow_fallback) {
				goto fallback;
			}
#endif // CONFIG_MALLOC_PROCESS_IDENTITY && CONFIG_MACOS_RANGES

			xzm_debug_abort("Pointer range exhausted");
			return NULL;
		}

#if CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES
		mach_vm_address_t vm_addr = segment_addr;
		mach_vm_size_t vm_size = (mach_vm_size_t)size;
		int alloc_flags = VM_FLAGS_OVERWRITE |
				VM_MAKE_TAG(VM_MEMORY_MALLOC_SMALL);

#if CONFIG_MTE
		if (rga_flags & XZM_RANGE_GROUP_ALLOC_FLAGS_MTE) {
			alloc_flags |= VM_FLAGS_MTE;
		}
#endif

		kern_return_t kr = mach_vm_map(mach_task_self(), &vm_addr, vm_size,
				/* mask */ 0, alloc_flags, MEMORY_OBJECT_NULL,
				/* offset */ 0, /* copy */ FALSE, VM_PROT_DEFAULT,
				VM_PROT_ALL, VM_INHERIT_DEFAULT);
		if (kr != KERN_SUCCESS) {
			xzm_abort_with_reason(
					"pointer range mach_vm_map() overwrite failed", kr);
		}
#endif // CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES

		return (void *)segment_addr;
	}

#if CONFIG_MALLOC_PROCESS_IDENTITY && CONFIG_MACOS_RANGES
fallback:
#endif // CONFIG_MALLOC_PROCESS_IDENTITY && CONFIG_MACOS_RANGES
	return _xzm_range_group_alloc_mvm_segment(rg->xzrg_main_ref, size, 0, map,
			rga_flags);
}

static void * __alloc_size(2)
xzm_range_group_alloc_segment(xzm_range_group_t rg, size_t size,
		size_t alignment, plat_map_t *map,
		xzm_range_group_alloc_flags_t rga_flags)
{
	if (rg->xzrg_id == XZM_RANGE_GROUP_DATA) {
		return _xzm_range_group_alloc_data_segment(rg, size, alignment, map,
				rga_flags);
	} else {
		xzm_debug_assert(alignment == 0);
		// Only huge segment bodies (which must be in the data range) can be
		// purgable
		xzm_debug_assert(!(rga_flags & XZM_RANGE_GROUP_ALLOC_FLAGS_PURGEABLE));
		return _xzm_range_group_alloc_ptr_segment(rg, size, map, rga_flags);
	}
}

static void
xzm_range_group_free_segment_body(xzm_range_group_t rg, void *body,
		size_t size, plat_map_t *map)
{
	xzm_debug_assert(rg->xzrg_id == XZM_RANGE_GROUP_DATA);

	int debug_flags = 0;
#ifdef DEBUG
	debug_flags = MALLOC_ABORT_ON_ERROR;
#endif // DEBUG
	mvm_deallocate_plat(body, size, debug_flags, map);
}

#if CONFIG_VM_USER_RANGES
static bool
parse_void_ranges(struct mach_vm_range *left_void,
		struct mach_vm_range *right_void)
{
	char buf[256];
	size_t bsz = sizeof(buf) - 1;
	char *s;

	int rc = sysctlbyname("vm.malloc_ranges", buf, &bsz, NULL, 0);
	if (rc == -1) {
		switch (errno) {
		case ENOENT:
#ifdef DEBUG
			malloc_report(ASL_LEVEL_INFO, "VM user ranges not supported\n");
#endif
			break;
		case EPERM:
			// TODO: make this fatal in processes that strictly need VM user
			// ranges
			malloc_report(ASL_LEVEL_ERR,
					"sysctlbyname(\"vm.malloc_ranges\") denied\n");
			break;
		default:
			xzm_abort_with_reason("sysctlbyname(\"vm.malloc_ranges\") failed",
					errno);
			break;
		}
		return false;
	}
	buf[bsz] = '\0';

	s = buf;

	left_void->min_address = strtoull(s, &s, 16);
	s++;

	left_void->max_address = strtoull(s, &s, 16);
	s++;

	right_void->min_address = strtoull(s, &s, 16);
	s++;

	right_void->max_address = strtoull(s, &s, 16);

	return true;
}
#endif // CONFIG_VM_USER_RANGES

#if MALLOC_TARGET_EXCLAVES || CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES

#define XZM_RANGE_SEPARATION   GiB(4)

#define XZM_DATA_RANGE_SIZE    GiB(10)
#define XZM_POINTER_RANGE_SIZE GiB(16)

#define XZM_PAGE_TABLE_GRANULE MiB(32)
#define XZM_PAGE_TABLE_BITS	   25

// Exclaves don't have struct mach_vm_range, so we'll just define our own little
// identical type
struct xzm_vm_range {
	uint64_t min_address;
	uint64_t max_address;
};


static_assert(sizeof(struct mach_vm_range) == sizeof(struct xzm_vm_range),
		"compatible vm range size");
static_assert(offsetof(struct mach_vm_range, min_address) ==
		offsetof(struct xzm_vm_range, min_address),
		"compatible vm range min_address offset");
static_assert(offsetof(struct mach_vm_range, max_address) ==
		offsetof(struct xzm_vm_range, max_address),
		"compatible vm range max_address offset");


static void
_xzm_main_malloc_zone_init_ptr_fronts(xzm_range_group_t range_groups,
		size_t allocation_front_count, struct xzm_vm_range *ranges,
		size_t range_count, plat_map_t *map)
{
	xzm_assert(allocation_front_count == 2);
	xzm_assert(range_count > 0);
	xzm_assert(ranges[0].min_address < ranges[0].max_address);
	if (range_count > 1) {
#if CONFIG_VM_USER_RANGES
		xzm_assert(range_count == 2);
		xzm_assert(ranges[1].min_address > ranges[0].max_address);
		xzm_assert(ranges[1].min_address < ranges[1].max_address);
#else
		xzm_abort_with_reason("unsupported range_count", range_count);
#endif
	}

	uint64_t total_span = 0;
	for (size_t i = 0; i < range_count; i++) {
		total_span += ranges[i].max_address - ranges[i].min_address;
	}
	uint64_t middle_pte_offset = roundup(total_span / 2,
			XZM_PAGE_TABLE_GRANULE);

	if (ranges[0].min_address + middle_pte_offset >= ranges[0].max_address) {
		xzm_assert(range_count == 2);
		middle_pte_offset += ranges[1].min_address - ranges[0].max_address;
	}

	uint64_t middle_pte = ranges[0].min_address + middle_pte_offset;
	xzm_assert(middle_pte % XZM_PAGE_TABLE_GRANULE == 0);

	uint64_t middle_pte_middle = middle_pte + (XZM_PAGE_TABLE_GRANULE / 2);

	uint64_t rg_up_size = 0;
	uint64_t rg_up_skip_addr = 0;
	uint64_t rg_up_skip_size = 0;

	uint64_t rg_down_size = 0;
	uint64_t rg_down_skip_addr = 0;
	uint64_t rg_down_skip_size = 0;

	if (range_count == 2) {
		if (middle_pte_middle > ranges[0].max_address) {
			xzm_assert(middle_pte_middle > ranges[1].min_address);
			xzm_assert(middle_pte_middle < ranges[1].max_address);

			// The right side (up) is not split
			rg_up_size = ranges[1].max_address - middle_pte_middle;

			// The left side (down) is split
			rg_down_size = (middle_pte_middle - ranges[1].min_address) +
					(ranges[0].max_address - ranges[0].min_address);
			rg_down_skip_addr = ranges[1].min_address;
			rg_down_skip_size = ranges[1].min_address - ranges[0].max_address;
		} else {
			xzm_assert(middle_pte_middle < ranges[0].max_address);
			xzm_assert(middle_pte_middle > ranges[0].min_address);

			// The right side (up) is split
			rg_up_size = (ranges[1].max_address - ranges[1].min_address) +
					(ranges[0].max_address - middle_pte_middle);
			rg_up_skip_addr = ranges[0].max_address;
			rg_up_skip_size = ranges[1].min_address - ranges[0].max_address;

			// The left side (down) is not split
			rg_down_size = middle_pte_middle - ranges[0].min_address;
		}
	} else {
		xzm_assert(ranges[0].min_address < middle_pte_middle);
		xzm_assert(middle_pte_middle < ranges[0].max_address);

		rg_up_size = ranges[0].max_address - middle_pte_middle;
		rg_down_size = middle_pte_middle - ranges[0].min_address;
	}

	xzm_range_group_t ptr_rg_up = &range_groups[XZM_RANGE_GROUP_PTR + 0];
	xzm_debug_assert(ptr_rg_up->xzrg_id == XZM_RANGE_GROUP_PTR);

	ptr_rg_up->xzrg_base = middle_pte_middle;
	ptr_rg_up->xzrg_next = ptr_rg_up->xzrg_base;
	ptr_rg_up->xzrg_size = rg_up_size;
	ptr_rg_up->xzrg_remaining = ptr_rg_up->xzrg_size;
	ptr_rg_up->xzrg_skip_addr = rg_up_skip_addr;
	ptr_rg_up->xzrg_skip_size = rg_up_skip_size;
	ptr_rg_up->xzrg_direction = XZM_FRONT_INCREASING;

	xzm_range_group_t ptr_rg_down = &range_groups[XZM_RANGE_GROUP_PTR + 1];
	xzm_debug_assert(ptr_rg_down->xzrg_id == XZM_RANGE_GROUP_PTR);

	ptr_rg_down->xzrg_base = middle_pte_middle;
	ptr_rg_down->xzrg_next = ptr_rg_down->xzrg_base;
	ptr_rg_down->xzrg_size = rg_down_size;
	ptr_rg_down->xzrg_remaining = ptr_rg_down->xzrg_size;
	ptr_rg_down->xzrg_skip_addr = rg_down_skip_addr;
	ptr_rg_down->xzrg_skip_size = rg_down_skip_size;
	ptr_rg_down->xzrg_direction = XZM_FRONT_DECREASING;

}

#if CONFIG_VM_USER_RANGES

static void
_xzm_main_malloc_zone_choose_ptr_ranges(struct mach_vm_range left_void,
		struct mach_vm_range right_void, size_t ptr_rg_size, uint64_t entropy,
		struct mach_vm_range *ranges_out, size_t *ranges_count_inout)
{
	// For now, the caller needs to be able to handle 2 result ranges
	xzm_assert(*ranges_count_inout == 2);

	xzm_assert(left_void.min_address);
	xzm_assert(left_void.max_address >= left_void.min_address);
	xzm_assert(right_void.min_address >= left_void.max_address);
	xzm_assert(right_void.max_address >= right_void.min_address);

#define xzm_trunc_page_table_granule(addr) \
		((addr) & ~(XZM_PAGE_TABLE_GRANULE - 1))

	// Note: the void boundaries should already be aligned to the page table
	// granule anyway

	// |<----------------total span--------------->|
	// |<-left  void->|<-data body->|<-right void->|
	// |<usable>|<pad>|<-data body->|<pad>|<usable>|
	// |<usable>|<-------data span------->|<usable>|

	uint64_t left_void_min = roundup(left_void.min_address,
			XZM_PAGE_TABLE_GRANULE);
	uint64_t left_void_limit =
			xzm_trunc_page_table_granule(left_void.max_address);
	if (left_void_limit < left_void_min) {
		// Shouldn't ever happen - the kernel would have to give us a
		// sub-granule left void that isn't granule-aligned.  If it does, we can
		// pretend it gave us an empty left void that's actually "in" the data
		// range, technically.
		left_void_min = left_void_limit;
	}
	xzm_assert(left_void_min <= left_void_limit);

	uint64_t right_void_min = roundup(right_void.min_address,
			XZM_PAGE_TABLE_GRANULE);
	uint64_t right_void_limit =
			xzm_trunc_page_table_granule(right_void.max_address);
	if (right_void_limit < right_void_min) {
		// Same thing, shouldn't happen
		right_void_limit = right_void_min;
	}
	xzm_assert(right_void_min <= right_void_limit);

	xzm_assert(left_void_limit <= right_void_min);

	uint64_t total_span = right_void_limit - left_void_min;

	uint64_t data_body_span = right_void_min - left_void_limit;

	uint64_t data_left_pad = MIN(XZM_RANGE_SEPARATION,
			left_void_limit - left_void_min);
	uint64_t data_left_pad_start = left_void_limit - data_left_pad;

	uint64_t data_right_pad = MIN(XZM_RANGE_SEPARATION,
			right_void_limit - right_void_min);
	uint64_t data_right_pad_limit = right_void_min + data_right_pad;

	uint64_t data_span = data_left_pad + data_body_span + data_right_pad;

	xzm_assert(data_span < total_span);
	uint64_t usable_space = total_span - data_span;

	xzm_assert(usable_space >= ptr_rg_size);
	uint64_t starting_space = usable_space - ptr_rg_size;

	xzm_assert(starting_space % XZM_PAGE_TABLE_GRANULE == 0);

	// Note: + 1 because the final granule address is also usable
	uint64_t starting_candidate_granules =
			(starting_space / XZM_PAGE_TABLE_GRANULE) + 1;

	// Note: start_granules is small relative to entropy, so the modulo bias is
	// not significant
	uint64_t start_granule = entropy % starting_candidate_granules;

	uint64_t start_address = left_void_min +
			(start_granule * XZM_PAGE_TABLE_GRANULE);

	if (start_address >= data_left_pad_start) {
		start_address += data_span;
	}

	uint64_t limit_address = start_address + ptr_rg_size;

	if (start_address < data_left_pad_start &&
			limit_address > data_left_pad_start) {
		// The pointer range is split across the data range
		ranges_out[0] = (struct mach_vm_range){
			.min_address = start_address,
			.max_address = data_left_pad_start,
		};

		uint64_t left_range_span = data_left_pad_start - start_address;
		uint64_t right_range_span = ptr_rg_size - left_range_span;
		ranges_out[1] = (struct mach_vm_range){
			.min_address = data_right_pad_limit,
			.max_address = data_right_pad_limit + right_range_span,
		};

		*ranges_count_inout = 2;
	} else {
		// The pointer range is fully on one side of the data range
		ranges_out[0] = (struct mach_vm_range){
			.min_address = start_address,
			.max_address = limit_address,
		};

		*ranges_count_inout = 1;
	}
}

static kern_return_t
_xzm_main_malloc_zone_create_ptr_range(struct mach_vm_range range)
{
	// It's important that we use a malloc tag in the recipe so that the kernel
	// gives us a single object rather than chunking into many.
	mach_vm_range_recipe_v1_t recipe = {
		.range = range,
		.range_tag = MACH_VM_RANGE_FIXED,
		.vm_tag = VM_MEMORY_MALLOC_SMALL,
	};

	kern_return_t kr = mach_vm_range_create(mach_task_self(),
			MACH_VM_RANGE_FLAVOR_V1, (mach_vm_range_recipes_raw_t)&recipe,
			sizeof(recipe));
	switch (kr) {
	case KERN_SUCCESS:
		break;
	case KERN_DENIED:
		// TODO: make this fatal in processes that strictly need VM user ranges
		malloc_report(ASL_LEVEL_ERR, "mach_vm_range_create() denied\n");
		return kr;
	case KERN_NOT_SUPPORTED:
		// Strange - in a process that doesn't have VM user ranges we would have
		// expected the sysctl to fail
		xzm_debug_abort("mach_vm_range_create() not supported?");
		return kr;
	default:
		xzm_abort_with_reason("unexpected error from mach_vm_range_create()",
				kr);
		return kr;
	}

	// Avoid malloc-no-CoW semantics on the pointer range reservation by
	// replacing the VM object for it with one that has a non-malloc tag.
	// Giving it VM_PROT_NONE causes the kernel to give us a single object
	// rather than chunking (which is important to avoid creating tons of
	// pointless VM objects), and hides it in vmmap by default.
	mach_vm_address_t overwrite_addr = (mach_vm_address_t)range.min_address;
	mach_vm_size_t overwrite_size =
			(mach_vm_size_t)(range.max_address - range.min_address);
	int alloc_flags = VM_FLAGS_OVERWRITE;
	kr = mach_vm_map(mach_task_self(), &overwrite_addr, overwrite_size,
			/* mask */ 0, alloc_flags, MEMORY_OBJECT_NULL, /* offset */ 0,
			/* copy */ FALSE, VM_PROT_NONE, VM_PROT_NONE, VM_INHERIT_DEFAULT);
	if (kr != KERN_SUCCESS) {
		xzm_abort_with_reason(
				"pointer range initial overwrite failed", kr);
	}

	return KERN_SUCCESS;
}

#endif // CONFIG_VM_USER_RANGES

#endif // MALLOC_TARGET_EXCLAVES || CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES

void
xzm_main_malloc_zone_init_range_groups(xzm_main_malloc_zone_t main)
{
	// Basic initialization is done in xzm_main_malloc_zone_create() - here we
	// mainly deal with VM user ranges.
	MALLOC_STATIC_ASSERT(XZM_RANGE_GROUP_COUNT == 3,
			"all range groups need to be initialized");

#if   CONFIG_VM_USER_RANGES
	struct mach_vm_range left_void, right_void;
	bool user_ranges_supported = parse_void_ranges(&left_void, &right_void);
	if (!user_ranges_supported) {
		return;
	}

	// VM user range support:
	//
	// We'll use the kernel default heap range for the DATA range.
	//
	// The ranges in the PTR range group:
	// - Should be separated from the data range (as defined by
	//   [void1.max_address, void2.min_address)) by at least 4G
	// - Should allow each allocation front to span 8G, possibly crossing the
	//   DATA range if necessary

	// The configurations we support are:
	// - No user ranges at all, in which case we shouldn't get here
	// - User ranges support with 2 allocation fronts
	if (main->xzmz_allocation_front_count != 2) {
		xzm_abort_with_reason("unsupported allocation front count",
				main->xzmz_allocation_front_count);
	}

	size_t ptr_rg_size = XZM_POINTER_RANGE_SIZE;

	struct mach_vm_range ptr_ranges[2];
	size_t ptr_range_count = 2;
	_xzm_main_malloc_zone_choose_ptr_ranges(left_void, right_void, ptr_rg_size,
			malloc_entropy[1], ptr_ranges, &ptr_range_count);

	for (size_t i = 0; i < ptr_range_count; i++) {
		kern_return_t kr =
				_xzm_main_malloc_zone_create_ptr_range(ptr_ranges[i]);
		if (kr != KERN_SUCCESS) {
			return;
		}
	}

	main->xzmz_use_ranges = true;

	_xzm_main_malloc_zone_init_ptr_fronts(main->xzmz_range_groups,
			main->xzmz_allocation_front_count,
			(struct xzm_vm_range *)ptr_ranges, ptr_range_count, NULL);

	xzm_range_group_t data_rg = &main->xzmz_range_groups[XZM_RANGE_GROUP_DATA];
	xzm_debug_assert(data_rg->xzrg_id == XZM_RANGE_GROUP_DATA);

	// Note: these are recorded purely for introspection purposes
	data_rg->xzrg_base = (mach_vm_address_t)left_void.max_address;
	data_rg->xzrg_size = right_void.min_address - left_void.max_address;

	// end of CONFIG_VM_USER_RANGES
#elif CONFIG_MACOS_RANGES
	// We want a similar layout to embedded, with:
	// - A data range and a pointer range located in the first 64GB (L2) of the
	//   address space to economize PTE usage
	// - Guaranteed minimum separation between the pointer range and everything
	//   else
	// - Both ranges separated from the traditional "low space" by a few GB of
	//   buffer distance
	//
	// However, on macOS there are no "voids" for us to need the
	// mach_vm_range_create() interface to access, nor is there a special data
	// range that the kernel knows about.  Instead, we create our own strongly
	// isolated pointer range reservation, and have a more relaxed model for the
	// data range that permits reuse with general VA, allowing us to model it as
	// a simple starting address hint.  An implication of the data range not
	// being strongly isolated is that it doesn't need to be contiguous.
	//
	// Either range should be able to grow to their standard size without
	// overflowing the first L2.
	//
	// So, our placement strategy will be:
	// - Place the pointer range, with its guards, in the space
	// - Then choose the data range hint somewhere in the remaining space

	// Start at 16GB to leave room in the low space for other VM allocations
#define XZM_MACOS_RANGES_START GiB(16)
	// End at 63GB to avoid crossing the commpage
#define XZM_MACOS_RANGES_END   GiB(63)

	uint64_t range_first_candidate = XZM_MACOS_RANGES_START;
	uint64_t ptr_reservation_size = XZM_RANGE_SEPARATION +
			XZM_POINTER_RANGE_SIZE + XZM_RANGE_SEPARATION;
	uint64_t range_last_candidate = XZM_MACOS_RANGES_END - ptr_reservation_size;

	uint64_t ptr_candidate_span = range_last_candidate - range_first_candidate;
	uint64_t ptr_candidate_granules =
			ptr_candidate_span / XZM_PAGE_TABLE_GRANULE;

	uint64_t ptr_entropy = (uint32_t)(malloc_entropy[1]);
	uint64_t ptr_granule = ptr_entropy % ptr_candidate_granules;

	uint64_t ptr_start =
			range_first_candidate + (ptr_granule * XZM_PAGE_TABLE_GRANULE);

	xzm_assert(ptr_start + ptr_reservation_size <= XZM_MACOS_RANGES_END);

	// Reserve the pointer range with a big max-protection == PROT_NONE region.
	// It is important that we not give it a malloc tag or protection above
	// PROT_NONE to avoid chunking or special CoW treatment from the VM - we
	// need for this to be just one entry.
	mach_vm_address_t ptr_addr = (mach_vm_address_t)ptr_start;
	mach_vm_size_t reservation_size = (mach_vm_size_t)ptr_reservation_size;
	int alloc_flags = 0; // fixed, no tag
	kern_return_t kr = mach_vm_map(mach_task_self(), &ptr_addr,
			reservation_size, /* mask */ 0, alloc_flags, MEMORY_OBJECT_NULL,
			/* offset */ 0, /* copy */ FALSE, VM_PROT_NONE, VM_PROT_NONE,
			VM_INHERIT_DEFAULT);
	if (kr != KERN_SUCCESS) {
		// We could fall back to mvm allocation, but we want this to fail loudly
		// if something starts preventing us from being able to make the
		// reservation we need
		xzm_abort_with_reason(
				"pointer range initial reservation failed", kr);
	}

	main->xzmz_use_ranges = true;

	mach_vm_address_t ptr_base = ptr_addr + XZM_RANGE_SEPARATION;

	struct xzm_vm_range range = {
		.min_address = ptr_base,
		.max_address = ptr_base + XZM_POINTER_RANGE_SIZE,
	};
	_xzm_main_malloc_zone_init_ptr_fronts(main->xzmz_range_groups,
			main->xzmz_allocation_front_count, &range, 1, NULL);

	// Choose a starting hint for the data range

	uint64_t data_candidate_span = ptr_candidate_span - XZM_DATA_RANGE_SIZE;
	uint64_t data_candidate_granules =
			data_candidate_span / XZM_PAGE_TABLE_GRANULE;

	uint64_t data_entropy = malloc_entropy[1] >> 32;
	uint64_t data_granule = data_entropy % data_candidate_granules;

	uint64_t data_start;
	if (data_granule < ptr_granule) {
		data_start = XZM_MACOS_RANGES_START +
				(data_granule * XZM_PAGE_TABLE_GRANULE);
	} else {
		uint64_t ptr_reservation_granules =
				ptr_reservation_size / XZM_PAGE_TABLE_GRANULE;
		uint64_t data_adjusted_granule =
				data_granule + ptr_reservation_granules;
		data_start = XZM_MACOS_RANGES_START +
				(data_adjusted_granule * XZM_PAGE_TABLE_GRANULE);
	}

	xzm_assert(data_start < ptr_start ||
			data_start >= ptr_start + ptr_reservation_size);
	xzm_assert(data_start + XZM_DATA_RANGE_SIZE <= XZM_MACOS_RANGES_END);

	xzm_range_group_t data_rg = &main->xzmz_range_groups[XZM_RANGE_GROUP_DATA];
	xzm_debug_assert(data_rg->xzrg_id == XZM_RANGE_GROUP_DATA);

	data_rg->xzrg_base = (mach_vm_address_t)data_start;
#endif // CONFIG_MACOS_RANGES
}

#pragma mark segment group

static void _xzm_segment_group_clear_chunk(xzm_segment_group_t sg,
		uint8_t *start, size_t size);

static void _xzm_segment_group_split_huge_segment(xzm_segment_group_t sg,
		xzm_segment_t segment, xzm_slice_count_t required_slices);

#if CONFIG_XZM_DEFERRED_RECLAIM

static void
__xzm_segment_cache_remove(xzm_segment_cache_t cache,
		xzm_segment_t segment)
{
	xzm_debug_assert(cache->xzsc_count > 0);
	cache->xzsc_count--;
	TAILQ_REMOVE(&cache->xzsc_head, segment, xzs_cache_entry);
}

static void
__xzm_segment_cache_insert(xzm_segment_cache_t cache, xzm_segment_t segment)
{
	xzm_debug_assert(cache->xzsc_count < cache->xzsc_max_count);
	TAILQ_INSERT_HEAD(&cache->xzsc_head, segment, xzs_cache_entry);
	cache->xzsc_count++;
}

static void
_xzm_segment_group_cache_invalidate(xzm_segment_group_t sg,
		xzm_segment_t segment)
{
#ifdef DEBUG
	_malloc_lock_assert_owner(&sg->xzsg_cache.xzsc_lock);
#endif
	__xzm_segment_cache_remove(&sg->xzsg_cache, segment);
	// Free memory backing segment header
	xzm_metapool_free(&sg->xzsg_main_ref->xzmz_metapools[XZM_METAPOOL_SEGMENT],
			segment);
}

static void
_xzm_segment_group_cache_mark_free(xzm_segment_group_t sg,
		xzm_segment_t segment)
{
#ifdef DEBUG
	_malloc_lock_assert_owner(&sg->xzsg_cache.xzsc_lock);
	// Make sure that this segment isn't in the segment table before we put it
	// into the cache
	xzm_segment_table_entry_s *entry;
	entry = _xzm_ptr_to_table_entry(_xzm_segment_start(segment),
			sg->xzsg_main_ref);
	xzm_debug_assert(entry->xste_val == 0);
#endif
	xzm_debug_assert(segment->xzs_reclaim_id == VM_RECLAIM_ID_NULL);

	xzm_main_malloc_zone_t main = sg->xzsg_main_ref;
	xzm_reclaim_buffer_t buffer = main->xzmz_reclaim_buffer;

	uint8_t *addr = _xzm_segment_start(segment);
	size_t size = _xzm_segment_size(segment);
	segment->xzs_reclaim_id = _xzm_reclaim_mark_free(buffer, addr, size, false);
	__xzm_segment_cache_insert(&sg->xzsg_cache, segment);
}

// Attempt to re-use a segment from the cache. Returns true if successful.
// If unsuccessful, the caller should invalidate the segment's cache entry.
static bool
_xzm_segment_group_cache_mark_used(xzm_segment_group_t sg,
		xzm_segment_t segment)
{
#ifdef DEBUG
	_malloc_lock_assert_owner(&sg->xzsg_cache.xzsc_lock);
#endif
	xzm_debug_assert(segment->xzs_reclaim_id != VM_RECLAIM_ID_NULL);
	xzm_main_malloc_zone_t main = sg->xzsg_main_ref;
	xzm_reclaim_buffer_t buffer = main->xzmz_reclaim_buffer;
	mach_vm_reclaim_state_t state;

	state = _xzm_reclaim_mark_used(buffer, segment->xzs_reclaim_id,
			_xzm_segment_start(segment), _xzm_segment_size(segment), false);
	if (!mach_vm_reclaim_is_reusable(state)) {
		// Entry has been reclaimed by the kernel since being placed in cache
		_xzm_segment_group_cache_invalidate(sg, segment);
		return false;
	}
	segment->xzs_reclaim_id = VM_RECLAIM_ID_NULL;
	__xzm_segment_cache_remove(&sg->xzsg_cache, segment);
	return true;
}

// Evict a segment from the cache
static void
_xzm_segment_group_cache_evict(xzm_segment_group_t sg)
{
#ifdef DEBUG
	_malloc_lock_assert_owner(&sg->xzsg_cache.xzsc_lock);
#endif
	// approximate the oldest segment by evicting the tail
	xzm_segment_t segment = TAILQ_LAST(&sg->xzsg_cache.xzsc_head,
				xzm_segment_cache_head_s);
	xzm_debug_assert(segment->xzs_reclaim_id != VM_RECLAIM_ID_NULL);
	if (_xzm_segment_group_cache_mark_used(sg, segment)) {
		_malloc_lock_unlock(&sg->xzsg_cache.xzsc_lock);
		// Segment isn't in segment table while in the cache, so pass false for
		// free_from_table while deallocating
		_xzm_segment_group_segment_deallocate(sg, segment, false);
		_malloc_lock_lock(&sg->xzsg_cache.xzsc_lock);
	}
}

#endif // CONFIG_XZM_DEFERRED_RECLAIM

// mimalloc: mi_slice_bin8
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static size_t
xzm_slice_bin8(xzm_slice_count_t slice_count)
{
	xzm_debug_assert(slice_count != 0);
	if (slice_count <= 8) {
		return slice_count - 1;
	}

	xzm_debug_assert(slice_count <= XZM_SLICES_PER_SEGMENT);
	slice_count--;

	int msb = 63 - __builtin_clzl(slice_count);
	return ((msb << 2) + ((slice_count >> (msb - 2)) & 0x3)) - 5;
}

// mimalloc: mi_slice_bin
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static size_t
xzm_slice_bin(xzm_slice_count_t slice_count)
{
	xzm_debug_assert(slice_count * XZM_SEGMENT_SLICE_SIZE <= XZM_SEGMENT_SIZE);
	xzm_debug_assert(xzm_slice_bin8(XZM_SLICES_PER_SEGMENT) <
			XZM_SPAN_QUEUE_COUNT);
	size_t bin = xzm_slice_bin8(slice_count);
	xzm_debug_assert(bin < XZM_SPAN_QUEUE_COUNT);
	return bin;
}

// mimalloc: mi_span_queue_for
static xzm_span_queue_t
xzm_span_queue_for(xzm_segment_group_t sg, xzm_slice_count_t slice_count)
{
	size_t bin = xzm_slice_bin(slice_count);
	xzm_span_queue_t sq = &sg->xzsg_spans[bin];
	xzm_debug_assert(sq->xzsq_slice_count >= slice_count);
	return sq;
}

#ifdef DEBUG
static void
_xzm_segment_group_assert_correct_span_queue(xzm_segment_group_t sg,
		xzm_slice_t slice)
{
	xzm_slice_kind_t kind = slice->xzc_bits.xzcb_kind;
	xzm_assert(_xzm_slice_kind_is_free_span(kind));

	xzm_slice_count_t slice_count;
	if (kind == XZM_SLICE_KIND_SINGLE_FREE) {
		slice_count = 1;
	} else {
		slice_count = slice->xzcs_slice_count;
	}

	xzm_span_queue_t sq = xzm_span_queue_for(sg, slice_count);
	xzm_free_span_t span;
	LIST_FOREACH(span, &sq->xzsq_queue, xzc_entry) {
		if (span == slice) {
			return;
		}
	}
	xzm_abort("Didn't find free span in expected span queue");
}

// mimalloc: mi_segment_is_valid
static bool
_xzm_segment_group_segment_is_valid(xzm_segment_group_t sg,
		xzm_segment_t segment)
{
	xzm_assert(segment->xzs_segment_group == sg);

	xzm_slice_t end = _xzm_segment_slices_end(segment);
	xzm_slice_t slice = _xzm_segment_slices_begin(segment);

	if (segment->xzs_kind == XZM_SEGMENT_KIND_HUGE) {
		xzm_assert(segment->xzs_used == 1);
		xzm_chunk_t chunk = slice;
		xzm_assert(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_HUGE_CHUNK);
		xzm_assert(chunk->xzcs_slice_count == segment->xzs_slice_count);
		return true;
	}

	_malloc_lock_assert_owner(&sg->xzsg_lock);

	while (slice < end) {
		xzm_slice_kind_t kind = slice->xzc_bits.xzcb_kind;
		switch (kind) {
		case XZM_SLICE_KIND_TINY_CHUNK:
			slice++;
			break;
		case XZM_SLICE_KIND_SMALL_CHUNK:
		case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
		case XZM_SLICE_KIND_LARGE_CHUNK: {
			size_t slice_index = _xzm_slice_index(segment, slice);
			size_t slice_count = slice->xzcs_slice_count;
			xzm_assert(slice_count > 1);

			slice++;
			size_t extra = MIN(slice_count - 1, XZM_MAX_SLICE_OFFSET);
			for (size_t i = 1; i <= extra; i++, slice++) {
				xzm_assert(slice->xzc_bits.xzcb_kind ==
						XZM_SLICE_KIND_MULTI_BODY);
				xzm_assert(slice->xzsl_slice_offset_bytes ==
						(uint32_t)(sizeof(struct xzm_slice_s) * i));
			}

			size_t last_slice_index = slice_index + slice_count - 1;
			xzm_assert(last_slice_index < segment->xzs_slice_entry_count);
			xzm_slice_t last = &segment->xzs_slices[last_slice_index];
			if (last >= slice) {
				xzm_assert(last->xzc_bits.xzcb_kind ==
						XZM_SLICE_KIND_MULTI_BODY);
				xzm_assert(last->xzsl_slice_offset_bytes ==
						(uint32_t)(sizeof(struct xzm_slice_s) *
						(slice_count - 1)));
			}
			slice = last + 1;
			break;
		}
		case XZM_SLICE_KIND_GUARD: {
			size_t slice_count = slice->xzcs_slice_count;
			slice++;

			for (size_t i = 1; i < slice_count; i++, slice++) {
				xzm_assert(slice->xzc_bits.xzcb_kind ==
						XZM_SLICE_KIND_MULTI_BODY);
				xzm_assert(slice->xzsl_slice_offset_bytes ==
						   (uint32_t)(sizeof(struct xzm_slice_s) * i));
			}

			// Adjacent guards should always be coalesced
			if (slice < end) {
				xzm_assert(slice->xzc_bits.xzcb_kind != XZM_SLICE_KIND_GUARD);
			}

			break;
		}
		case XZM_SLICE_KIND_HUGE_CHUNK:
			xzm_abort("huge chunk in normal segment");
			break;
		case XZM_SLICE_KIND_SINGLE_FREE: {
			xzm_assert(slice->xzc_mzone_idx == XZM_MZONE_INDEX_INVALID);
			_xzm_segment_group_assert_correct_span_queue(sg, slice);
#if CONFIG_XZM_DEFERRED_RECLAIM
			mach_vm_reclaim_id_t *reclaim_id =
					_xzm_segment_slice_meta_reclaim_id(segment, slice);
			xzm_assert(*reclaim_id == VM_RECLAIM_ID_NULL ||
					!slice->xzc_bits.xzcb_is_pristine);
#endif // CONFIG_XZM_DEFERRED_RECLAIM
			slice++;
			break;
		}
		case XZM_SLICE_KIND_MULTI_FREE: {
			xzm_assert(slice->xzc_mzone_idx == XZM_MZONE_INDEX_INVALID);
			_xzm_segment_group_assert_correct_span_queue(sg, slice);

			size_t slice_index = _xzm_slice_index(segment, slice);
			size_t slice_count = slice->xzcs_slice_count;
			xzm_assert(slice_count > 1);

			size_t last_slice_index = slice_index + slice_count - 1;
			xzm_assert(last_slice_index < segment->xzs_slice_entry_count);

			xzm_slice_t last = &segment->xzs_slices[last_slice_index];
			xzm_assert(last->xzc_bits.xzcb_kind ==
					XZM_SLICE_KIND_MULTI_BODY);
			xzm_assert(last->xzsl_slice_offset_bytes ==
					(uint32_t)(sizeof(struct xzm_slice_s) * (slice_count - 1)));

#if CONFIG_XZM_DEFERRED_RECLAIM
			mach_vm_reclaim_id_t *reclaim_id =
					_xzm_segment_slice_meta_reclaim_id(segment, slice);
			xzm_assert(*reclaim_id == VM_RECLAIM_ID_NULL ||
					!slice->xzc_bits.xzcb_is_pristine);
#endif // CONFIG_XZM_DEFERRED_RECLAIM

			slice = last + 1;
			break;
		}
		default:
			xzm_abort_with_reason("Unexpected slice kind", (unsigned)kind);
			break;
		}
	}

	return true;
}
#endif // DEBUG

#if CONFIG_XZM_DEFERRED_RECLAIM

static void
_xzm_segment_group_span_mark_free(xzm_segment_group_t sg, xzm_free_span_t span)
{
	xzm_debug_assert(_xzm_segment_group_uses_deferred_reclamation(sg));
	xzm_debug_assert(_xzm_slice_kind_is_free_span(span->xzc_bits.xzcb_kind));

	xzm_main_malloc_zone_t main = sg->xzsg_main_ref;
	xzm_malloc_zone_t zone = &main->xzmz_base;
	xzm_reclaim_buffer_t buffer = main->xzmz_reclaim_buffer;

	mach_vm_reclaim_id_t *reclaim_id = _xzm_slice_meta_reclaim_id(zone, span);
	xzm_debug_assert(*reclaim_id == VM_RECLAIM_ID_NULL);
	size_t span_size = _xzm_free_span_size(span);
	uint8_t *span_start = _xzm_slice_start(zone, span);

	*reclaim_id = _xzm_reclaim_mark_free(buffer, span_start, span_size, true);
}

static bool
_xzm_segment_group_span_mark_used(xzm_segment_group_t sg, xzm_free_span_t span)
{
	xzm_debug_assert(_xzm_segment_group_uses_deferred_reclamation(sg));
	xzm_debug_assert(_xzm_slice_kind_is_free_span(span->xzc_bits.xzcb_kind));
	xzm_main_malloc_zone_t main = sg->xzsg_main_ref;
	xzm_malloc_zone_t zone = &main->xzmz_base;

	if (!_xzm_slice_is_deferred(zone, span)) {
		// span has not been marked free
		return true;
	}

	xzm_reclaim_buffer_t buffer = main->xzmz_reclaim_buffer;

	mach_vm_reclaim_id_t *reclaim_id = _xzm_slice_meta_reclaim_id(zone, span);
	xzm_debug_assert(*reclaim_id != VM_RECLAIM_ID_NULL);
	size_t span_size = _xzm_free_span_size(span);
	uint8_t *span_start = _xzm_slice_start(zone, span);
	mach_vm_reclaim_state_t state;

	state = _xzm_reclaim_mark_used(buffer, *reclaim_id, span_start,
			span_size, true);
	if (mach_vm_reclaim_is_reusable(state)) {
		*reclaim_id = VM_RECLAIM_ID_NULL;
		return true;
	}
	return false;
}

static bool
_xzm_segment_group_span_mark_smaller(xzm_segment_group_t sg,
		xzm_free_span_t span, xzm_slice_count_t front_free_count,
		xzm_slice_count_t used_count, xzm_slice_count_t back_free_count)
{
	xzm_debug_assert(_xzm_segment_group_uses_deferred_reclamation(sg));
	xzm_debug_assert(_xzm_slice_kind_is_free_span(span->xzc_bits.xzcb_kind));
	xzm_debug_assert(front_free_count + used_count + back_free_count ==
			_xzm_free_span_slice_count(span));

	xzm_main_malloc_zone_t main = sg->xzsg_main_ref;
	xzm_malloc_zone_t zone = &main->xzmz_base;
	xzm_reclaim_buffer_t buffer = main->xzmz_reclaim_buffer;

	const bool deferred = _xzm_slice_is_deferred(zone, span);
	uint64_t *span_id = _xzm_slice_meta_reclaim_id(zone, span);
	uint8_t *span_start = _xzm_slice_start(zone, span);
	bool pristine = span->xzc_bits.xzcb_is_pristine;

	// Actual span metadata for the front/middle/back spans has not yet been
	// updated, we only set the deferred reclaim metadata for these spans
	xzm_free_span_t back_span = span + front_free_count + used_count;
	const size_t front_size = front_free_count << XZM_SEGMENT_SLICE_SHIFT;
	const size_t used_size = used_count << XZM_SEGMENT_SLICE_SHIFT;
	const size_t back_size = back_free_count << XZM_SEGMENT_SLICE_SHIFT;
	xzm_debug_assert(!back_size || span_start + front_size + used_size ==
			_xzm_slice_start(zone, back_span));
	uint64_t *back_id = back_size ?
			_xzm_slice_meta_reclaim_id(zone, back_span) : NULL;
	return xzm_reclaim_mark_smaller(buffer, span_id, back_id, span_start,
			front_size, used_size, back_size, deferred, pristine, true);
}

void
xzm_chunk_mark_free(xzm_malloc_zone_t zone, xzm_chunk_t chunk)
{
	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
	xzm_debug_assert(_xzm_chunk_should_defer_reclamation(main, chunk));

	xzm_reclaim_buffer_t buffer = main->xzmz_reclaim_buffer;

	mach_vm_reclaim_id_t *reclaim_id = _xzm_slice_meta_reclaim_id(zone, chunk);
	xzm_debug_assert(*reclaim_id == VM_RECLAIM_ID_NULL);
	size_t chunk_size;
	uint8_t *chunk_start = _xzm_chunk_start_ptr(zone, chunk, &chunk_size);

	*reclaim_id = _xzm_reclaim_mark_free(buffer, chunk_start, chunk_size,
			true);
}

bool
xzm_chunk_mark_used(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		bool *was_reclaimed)
{
	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
	xzm_debug_assert(_xzm_slice_kind_is_chunk(chunk->xzc_bits.xzcb_kind));
	xzm_debug_assert(_xzm_chunk_should_defer_reclamation(main, chunk));

	xzm_reclaim_buffer_t buffer = main->xzmz_reclaim_buffer;

	mach_vm_reclaim_id_t *reclaim_id = _xzm_slice_meta_reclaim_id(zone, chunk);
	xzm_debug_assert(*reclaim_id != VM_RECLAIM_ID_NULL);
	size_t chunk_size;
	uint8_t *chunk_start = _xzm_chunk_start_ptr(zone, chunk, &chunk_size);
	mach_vm_reclaim_state_t state;

	state = _xzm_reclaim_mark_used(buffer, *reclaim_id, chunk_start,
			chunk_size, true);

	if (was_reclaimed) {
		*was_reclaimed = (state != VM_RECLAIM_UNRECLAIMED);
	}
	if (mach_vm_reclaim_is_reusable(state)) {
		*reclaim_id = VM_RECLAIM_ID_NULL;
		return true;
	}
	return false;
}

#endif // CONFIG_XZM_DEFERRED_RECLAIM

// mimalloc: mi_segment_span_free
//
// Precondition: sg is locked (except for huge segments)
static void
_xzm_segment_group_segment_span_free(xzm_segment_group_t sg,
		xzm_segment_t segment, xzm_slice_count_t slice_index,
		xzm_slice_count_t slice_count, bool set_id, bool is_pristine)
{
	xzm_debug_assert(slice_count != 0);
	xzm_debug_assert(slice_index < segment->xzs_slice_entry_count);

	// set first and last slice (the intermediates can be undetermined)
	//
	// TODO: leaving the intermediates undetermined means that you can't
	// reliably check whether an arbitrary slice in a segment belongs to a
	// chunk.  That would be useful for:
	// - the checked memcpy trick
	// - malloc_claimed_address()
	// - possibly other things?
	//
	// However, for large allocations it would require updating large numbers of
	// slices, which is probably not worth the cost
	xzm_free_span_t span = &segment->xzs_slices[slice_index];
	span->xzc_bits.xzcb_is_pristine = is_pristine;
	if (slice_count == 1) {
		xzm_debug_assert(segment->xzs_kind != XZM_SEGMENT_KIND_HUGE);
		span->xzc_bits.xzcb_kind = XZM_SLICE_KIND_SINGLE_FREE;
	} else {
		span->xzc_bits.xzcb_kind = XZM_SLICE_KIND_MULTI_FREE;
		span->xzcs_slice_count = slice_count;

		xzm_debug_assert(slice_index + slice_count - 1 < segment->xzs_slice_entry_count);
		xzm_slice_t last = &segment->xzs_slices[slice_index + slice_count - 1];
		last->xzc_bits.xzcb_kind = XZM_SLICE_KIND_MULTI_BODY;
		last->xzsl_slice_offset_bytes =
				(uint32_t)(sizeof(struct xzm_slice_s) * (slice_count - 1));
	}

	if (segment->xzs_kind == XZM_SEGMENT_KIND_NORMAL) {
#ifdef DEBUG
		_malloc_lock_assert_owner(&sg->xzsg_lock);
#endif
		xzm_span_queue_t sq = xzm_span_queue_for(sg, slice_count);
		LIST_INSERT_HEAD(&sq->xzsq_queue, span, xzc_entry);
	}
#if CONFIG_XZM_DEFERRED_RECLAIM
	if (set_id) {
		mach_vm_reclaim_id_t *reclaim_id = _xzm_segment_slice_meta_reclaim_id(
				segment, span);
		*reclaim_id = VM_RECLAIM_ID_NULL;
	} else if (!is_pristine) {
		xzm_debug_assert(*_xzm_segment_slice_meta_reclaim_id(segment, span) !=
				VM_RECLAIM_ID_NULL);
	}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
}

// mimalloc: mi_segment_slice_split
static xzm_free_span_t
_xzm_segment_group_segment_slice_split(xzm_segment_group_t sg,
		xzm_segment_t segment, xzm_free_span_t span,
		xzm_slice_count_t slice_count, bool uses_dr, bool front)
{
	xzm_debug_assert(_xzm_segment_for_slice(&sg->xzsg_main_ref->xzmz_base, span) == segment);
	xzm_debug_assert(span->xzc_bits.xzcb_kind == XZM_SLICE_KIND_MULTI_FREE);
	xzm_debug_assert(span->xzcs_slice_count > slice_count);
	xzm_debug_assert(segment->xzs_kind != XZM_SEGMENT_KIND_HUGE);

	// Find the start and length of the piece being split off and update its
	// slices
	xzm_free_span_t retval;
	xzm_slice_count_t index_to_free;
	xzm_slice_count_t count_to_free = span->xzcs_slice_count - slice_count;
	if (front) {
		retval = span + count_to_free;
		// We don't update the backpointers here because this span is about to
		// be used as a large chunk, but we do need to update the slice count
		// and kind since this span could be given back to _segment_slice_split
		// to split off the back end
		retval->xzcs_slice_count = span->xzcs_slice_count - count_to_free;
		// We could probably copy the bits wholesale, but for now only
		// explicitly copy the ones we know we need
		retval->xzc_bits.xzcb_kind = XZM_SLICE_KIND_MULTI_FREE;
		// Preserve whether the span is pristine, since it was undefined
		retval->xzc_bits.xzcb_is_pristine = span->xzc_bits.xzcb_is_pristine;
		index_to_free = _xzm_slice_index(segment, span);
	} else {
		retval = span;
		index_to_free = _xzm_slice_index(segment, span) + slice_count;
	}
	// If the segment group uses deferred reclaim, then the reclaim id for the
	// split span has already been initialized, so don't overwrite it
	_xzm_segment_group_segment_span_free(sg, segment, index_to_free,
			count_to_free, !uses_dr, span->xzc_bits.xzcb_is_pristine);
	return retval;
}

static void
_xzm_segment_group_segment_create_guard(xzm_segment_group_t sg,
		xzm_segment_t segment, xzm_slice_count_t index)
{
	xzm_slice_t slice = &segment->xzs_slices[index];
	xzm_slice_count_t slice_count = 1;

	// Coalesce with next guard page
	if (&slice[1] < _xzm_segment_slices_end(segment) &&
			slice[1].xzc_bits.xzcb_kind == XZM_SLICE_KIND_GUARD) {
		slice_count += slice[1].xzcs_slice_count;
	}

	// Coalesce with previous guard page
	if (slice > _xzm_segment_slices_begin(segment)) {
		xzm_slice_t prev = _xzm_span_slice_first(slice - 1);
		if (prev->xzc_bits.xzcb_kind == XZM_SLICE_KIND_GUARD) {
			index -= prev->xzcs_slice_count;
			slice_count += prev->xzcs_slice_count;
			slice = prev;
		}
	}

	if (slice_count > 1) {
		// Setup backpointers
		for (int i = 1; i < slice_count; i++) {
			slice[i].xzsl_slice_offset_bytes = i * sizeof(struct xzm_slice_s);
			slice[i].xzc_bits.xzcb_kind = XZM_SLICE_KIND_MULTI_BODY;
		}
	} else {
		// This is a new guard page entry, increment segment count to avoid
		// trying to free this segment while it has guards
		segment->xzs_used++;
	}

	xzm_debug_assert(slice == &segment->xzs_slices[index]);

	slice->xzcs_slice_count = slice_count;
	// mprotect
	size_t size = XZM_SEGMENT_SLICE_SIZE * slice_count;
	void *start = _xzm_segment_slice_index_start(segment, index);
	int rc = mprotect(start, size, PROT_NONE);
	if (rc) {
		xzm_abort_with_reason("Failed to mprotect guard page", errno);
	}

	// Atomic store maybe?
	slice->xzc_bits.xzcb_kind = XZM_SLICE_KIND_GUARD;
}

// mimalloc: mi_segment_span_allocate
static xzm_chunk_t
_xzm_segment_group_segment_span_mark_allocated(xzm_segment_group_t sg,
		xzm_segment_t segment, xzm_slice_kind_t kind, size_t slice_index,
		xzm_slice_count_t slice_count)
{
	xzm_debug_assert(_xzm_slice_kind_is_chunk(kind));
	xzm_debug_assert(slice_index < segment->xzs_slice_entry_count);

	xzm_slice_t slice = &segment->xzs_slices[slice_index];
	xzm_chunk_t chunk = slice;

	// set slice back pointers for the first XZM_MAX_SLICE_OFFSET entries
	size_t extra = MIN(slice_count - 1, XZM_MAX_SLICE_OFFSET);
	if (slice_index + extra >= segment->xzs_slice_entry_count) {
		// huge objects may have more slices than available entries in the
		// segment->xzs_slices table
		extra = segment->xzs_slice_entry_count - slice_index - 1;
	}
	slice++;
	for (size_t i = 1; i <= extra; i++, slice++) {
		slice->xzc_bits.xzcb_kind = XZM_SLICE_KIND_MULTI_BODY;
		slice->xzsl_slice_offset_bytes =
				(uint32_t)(sizeof(struct xzm_slice_s) * i);
	}

	// And also for the last one, if not set already (the last one is needed for
	// coalescing)
	size_t last_slice_index = slice_index + slice_count - 1;
	if (kind != XZM_SLICE_KIND_HUGE_CHUNK) {
		xzm_debug_assert(last_slice_index < segment->xzs_slice_entry_count);

		xzm_slice_t last = &segment->xzs_slices[last_slice_index];
		if (last >= slice) {
			last->xzc_bits.xzcb_kind = XZM_SLICE_KIND_MULTI_BODY;
			last->xzsl_slice_offset_bytes =
					(uint32_t)(sizeof(struct xzm_slice_s) * (slice_count - 1));
		}
	}

	// Update the chunk slice last, setting the kind at the very end to
	// "publish" the chunk for the enumerator protocol
	if (kind != XZM_SLICE_KIND_TINY_CHUNK) {
		chunk->xzcs_slice_count = slice_count;
	} else {
		xzm_debug_assert(slice_count == 1);
	}
	// TODO: atomic store, compiler barrier
	chunk->xzc_bits.xzcb_kind = kind;

#if CONFIG_XZM_DEFERRED_RECLAIM
	mach_vm_reclaim_id_t *reclaim_id = _xzm_segment_slice_meta_reclaim_id(
			segment, chunk);
	*reclaim_id = VM_RECLAIM_ID_NULL;
#endif // CONFIG_XZM_DEFERRED_RECLAIM
	segment->xzs_used++;
	return chunk;
}

// Taken from xnu/osfmk/kern/zalloc.c
static inline uint32_t
dist_u32(uint32_t a, uint32_t b)
{
	return a < b ? b - a : a - b;
}

static uint32_t
_xzm_random_clear_n_bits(uint32_t mask, uint32_t pop, uint32_t n)
{
	for(; n--; pop--) {
		uint32_t bit = arc4random_uniform(pop);
		uint32_t m = mask;
		// Clear the bottom `bit` bits from m...
		for (; bit; bit--) {
			m &= (m - 1);
		}
		// ... in order to clear the `bit`th least significant set bit in mask
		mask ^= 1 << __builtin_ctz(m);
	}
	return mask;
}

// Create a bitmap `width` bits wide with `pop` set bits
static uint32_t
_xzm_random_bits(uint32_t pop, uint32_t width)
{
	uint32_t mask = (uint32_t)((1ull << width) - 1);
	uint32_t retval;
	uint32_t cur;

	if (3 * width / 4 <= pop) {
		// Caller wants >75% of the bits set, so set them all and clear <25%
		retval = mask;
		cur = width;
	} else if (pop <= width / 4) {
		retval = 0;
		cur = 0;
	} else {
		// A masked value from arc4random should contain ~`width/2` set bits
		retval = arc4random() & mask;
		cur = __builtin_popcount(retval);

		if (dist_u32(cur, pop) > dist_u32(width - cur, pop)) {
			// If the opposite mask has a closer popcount, then start with that
			cur = width - cur;
			retval ^= mask;
		}
	}

	if (cur < pop) {
		// Setting `pop - cur` bits is really clearing that many from the
		// opposite mask.
		retval ^= mask;
		retval = _xzm_random_clear_n_bits(retval, width - cur, pop - cur);
		retval ^= mask;
	} else if (pop < cur) {
		retval = _xzm_random_clear_n_bits(retval, cur, cur - pop);
	}
	xzm_debug_assert(__builtin_popcount(retval) == pop);
	xzm_debug_assert((retval & ~mask) == 0);
	return retval;
}

static xzm_chunk_t
_xzm_segment_group_segment_span_init_run(xzm_segment_group_t sg,
		xzm_segment_t segment, xzm_slice_kind_t kind,
		xzm_preallocate_list_s *preallocate_list, xzm_slice_count_t start_index,
		xzm_slice_count_t total_slices, xzm_slice_count_t guard_count,
		xzm_slice_count_t num_chunks)
{
	xzm_chunk_t retval = NULL;
	uint32_t guard_mask;
	if (guard_count) {
		guard_mask = _xzm_random_bits(guard_count, num_chunks + 1);
	} else {
		guard_mask = 0;
	}

	xzm_slice_count_t slices_per_chunk = 0;
	if (kind == XZM_SLICE_KIND_TINY_CHUNK) {
		slices_per_chunk = 1;
	} else if (kind == XZM_SLICE_KIND_SMALL_CHUNK) {
		slices_per_chunk = XZM_SMALL_CHUNK_SIZE / XZM_SEGMENT_SLICE_SIZE;
	} else if (kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK) {
		slices_per_chunk =
				XZM_SMALL_FREELIST_CHUNK_SIZE / XZM_SEGMENT_SLICE_SIZE;
	} else {
		xzm_debug_assert(!preallocate_list);
		xzm_debug_assert(guard_count == 0);
		xzm_debug_assert(kind == XZM_SLICE_KIND_LARGE_CHUNK);
		xzm_debug_assert(num_chunks == 1);
		slices_per_chunk = total_slices;
	}
	xzm_debug_assert((num_chunks * slices_per_chunk + guard_count) ==
			total_slices);
	xzm_debug_assert((start_index + total_slices) <=
			segment->xzs_slice_entry_count);

	xzm_slice_count_t index = start_index;
	bool is_pristine = segment->xzs_slices[index].xzc_bits.xzcb_is_pristine;

	for (int i = 0; i < num_chunks; i++) {
		if (guard_mask & 1) {
			_xzm_segment_group_segment_create_guard(sg, segment, index);
			index++;
		}
		guard_mask >>= 1;

		xzm_chunk_t chunk = _xzm_segment_group_segment_span_mark_allocated(sg,
				segment, kind, index, slices_per_chunk);
		chunk->xzc_bits.xzcb_is_pristine = is_pristine;
		index += slices_per_chunk;

		if (i == 0) {
			retval = chunk;
		} else {
			SLIST_INSERT_HEAD(preallocate_list, chunk, xzc_slist_entry);
		}
	}

	xzm_debug_assert(guard_mask <= 1);
	if (guard_mask) {
		_xzm_segment_group_segment_create_guard(sg, segment, index);
		index++;
	}

	xzm_debug_assert(index - start_index == total_slices);
	return retval;
}

// mimalloc: mi_segments_page_find_and_allocate
// Precondition: sg is locked
static xzm_chunk_t
_xzm_segment_group_find_and_allocate_chunk(xzm_segment_group_t sg,
		xzm_slice_kind_t kind, xzm_xzone_guard_config_t guard_config,
		xzm_preallocate_list_s *preallocate_list, xzm_slice_count_t slice_count,
		size_t alignment)
{
	xzm_debug_assert(_xzm_slice_kind_is_chunk(kind));
	xzm_debug_assert(kind != XZM_SLICE_KIND_TINY_CHUNK || slice_count == 1);
	xzm_debug_assert(slice_count != 0);
	xzm_debug_assert(slice_count * XZM_SEGMENT_SLICE_SIZE <=
			XZM_LARGE_BLOCK_SIZE_MAX);
	xzm_debug_assert(alignment == 0 || kind == XZM_SLICE_KIND_LARGE_CHUNK);

	xzm_debug_assert(kind != XZM_SLICE_KIND_TINY_CHUNK || guard_config != NULL);
	xzm_debug_assert(kind != XZM_SLICE_KIND_SMALL_CHUNK || guard_config != NULL);
	xzm_debug_assert(kind != XZM_SLICE_KIND_SMALL_FREELIST_CHUNK ||
			guard_config != NULL);
	xzm_debug_assert(kind != XZM_SLICE_KIND_LARGE_CHUNK || guard_config == NULL);

	if (alignment <= XZM_SEGMENT_SLICE_SIZE) {
		// Large chunks guarantee page alignment
		alignment = 0;
	}
	xzm_slice_count_t alignment_slices;
	if (os_convert_overflow(alignment / XZM_SEGMENT_SLICE_SIZE, &alignment_slices)) {
		xzm_debug_abort_with_reason("Unexpected align value", alignment);
		return NULL;
	}

	xzm_slice_count_t total_slice_count;
	uint8_t chunks_in_run;
	uint8_t guards;
	if (guard_config && guard_config->xxgc_max_run_length) {
		chunks_in_run = arc4random_uniform(guard_config->xxgc_max_run_length) + 1;
		total_slice_count = chunks_in_run * slice_count;
		guards = (guard_config->xxgc_density * total_slice_count) / 256;
		uint32_t remainder = (guard_config->xxgc_density * total_slice_count) %
				256;
		// short circuit to avoid a call to corecrypto in common case that the
		// density of guard pages goes perfectly into the allocated pages
		if (remainder && remainder > arc4random_uniform(256)) {
			guards++;
		}
		total_slice_count += guards;
	} else {
		total_slice_count = slice_count;
		chunks_in_run = 1;
		guards = 0;
	}
	xzm_debug_assert(total_slice_count <=
			(XZM_LARGE_BLOCK_SIZE_MAX / XZM_SEGMENT_SLICE_SIZE) ||
			// Aligned allocations can request more than LARGE_BLOCK_SIZE slices
			// from the span queue
			alignment != 0);
	// At present, we only allow 1 guard page between chunks in a run, so it
	// shouldn't be possible to have more guards than chunks
	xzm_debug_assert(chunks_in_run >= guards);

	if (alignment_slices) {
		// We only need to allocate (slice_count + alignment_slices - 1) slices
		// to guarantee that there will be a slice_count long span at the
		// correct alignment
		xzm_slice_count_t max_align_slices =
				alignment_slices ? alignment_slices - 1 : 0;

		if (os_add_overflow(total_slice_count, max_align_slices,
				&total_slice_count)) {
			xzm_debug_abort_with_reason("Unexpected total slice count",
					slice_count + max_align_slices);
			return NULL;
		}

		xzm_debug_assert(total_slice_count < XZM_SLICES_PER_SEGMENT);
	}

	for (xzm_span_queue_t sq = xzm_span_queue_for(sg, total_slice_count);
			sq < &sg->xzsg_spans[XZM_SPAN_QUEUE_COUNT];
			sq++) {
		// TODO: rather than allowing a range of span sizes in a span queue,
		// should all the spans be exactly the span queue size?  Then this would
		// be a pop rather than a list scan.
		xzm_free_span_t span, tmp;
		LIST_FOREACH_SAFE(span, &sq->xzsq_queue, xzc_entry, tmp) {
			xzm_slice_count_t span_slice_count =
					_xzm_free_span_slice_count(span);
			if (span_slice_count >= total_slice_count) {
				xzm_malloc_zone_t zone = &sg->xzsg_main_ref->xzmz_base;
				xzm_segment_t segment = _xzm_segment_for_slice(zone, span);
#if CONFIG_XZM_DEFERRED_RECLAIM
				xzm_slice_count_t old_total_slice_count = total_slice_count;
#endif // CONFIG_XZM_DEFERRED_RECLAIM
				xzm_slice_count_t front_free_count = 0;

				if (alignment_slices) {
					// Split off the front to round the address up to alignment
					xzm_slice_count_t actual_index = _xzm_slice_index(segment,
							span);
					xzm_slice_count_t desired_index = roundup(actual_index,
							alignment_slices);

					front_free_count = desired_index - actual_index;
					xzm_debug_assert(slice_count <= (total_slice_count - front_free_count));

					// Take the alignment slices back out of our request
					total_slice_count = slice_count;

					if (front_free_count) {
						span_slice_count -= front_free_count;
					}
				}

				xzm_slice_count_t back_free_count =
						span_slice_count - total_slice_count;

				bool uses_dr = false;
#if CONFIG_XZM_DEFERRED_RECLAIM
				uses_dr = _xzm_segment_group_uses_deferred_reclamation(sg);
				if (uses_dr) {
					if (!_xzm_segment_group_span_mark_smaller(sg, span,
							front_free_count, total_slice_count,
							back_free_count)) {
						total_slice_count = old_total_slice_count;
						// span is busy being reclaimed by the kernel
						continue;
					}
				}
#endif // CONFIG_XZM_DEFERRED_RECLAIM

				LIST_REMOVE(span, xzc_entry);

				if (front_free_count) {
					span = _xzm_segment_group_segment_slice_split(sg, segment,
							span, span_slice_count, uses_dr, true);
				}

				if (back_free_count) {
					_xzm_segment_group_segment_slice_split(sg, segment, span,
							total_slice_count, uses_dr, false);
				}

				xzm_slice_count_t index = _xzm_slice_index(segment, span);

				xzm_chunk_t chunk;
				chunk = _xzm_segment_group_segment_span_init_run(sg, segment,
							kind, preallocate_list, index, total_slice_count,
							guards, chunks_in_run);

				xzm_debug_assert(chunk);
				xzm_debug_assert(_xzm_segment_group_segment_is_valid(sg,
						segment));

				return chunk;
			}
		}
	}

	return NULL;
}

// mi_segment_init
static xzm_chunk_t
_xzm_segment_group_init_segment(xzm_segment_group_t sg, xzm_segment_t segment,
		void *body, size_t body_size, bool huge, bool is_pristine)
{
	xzm_chunk_t chunk = NULL;
	xzm_assert((uintptr_t)segment < XZM_LIMIT_ADDRESS);
	xzm_assert((uintptr_t)body < XZM_LIMIT_ADDRESS);
	xzm_debug_assert((uintptr_t)segment % XZM_METAPOOL_SEGMENT_ALIGN == 0);
	xzm_debug_assert((uintptr_t)body % XZM_SEGMENT_SIZE == 0);
	xzm_debug_assert(body_size % XZM_SEGMENT_SLICE_SIZE == 0);

	xzm_slice_count_t total_slices = 0;
	if (os_convert_overflow(body_size / XZM_SEGMENT_SLICE_SIZE, &total_slices)) {
		xzm_abort("Slice count too large in init_segment");
	}
	segment->xzs_segment_group = sg;
	segment->xzs_slice_count = total_slices;
	segment->xzs_slice_entry_count = MIN(total_slices, XZM_SLICES_PER_SEGMENT);
	segment->xzs_used = 0;
	segment->xzs_segment_body = body;
#if CONFIG_XZM_DEFERRED_RECLAIM
	segment->xzs_reclaim_id = VM_RECLAIM_ID_NULL;
#endif // CONFIG_XZM_DEFERRED_RECLAIM

	segment->xzs_kind = huge ? XZM_SEGMENT_KIND_HUGE : XZM_SEGMENT_KIND_NORMAL;
	if (huge) {
		chunk = _xzm_segment_group_segment_span_mark_allocated(sg, segment,
				XZM_SLICE_KIND_HUGE_CHUNK, 0, segment->xzs_slice_count);
		chunk->xzc_bits.xzcb_is_pristine = is_pristine;
	} else {
		// Lock the segment group to add this span - we'll return to the caller
		// with the segment group locked so they can then directly allocate what
		// they need
		_malloc_lock_lock(&sg->xzsg_lock);

		_xzm_segment_group_segment_span_free(sg, segment, 0, total_slices,
				true, is_pristine);
	}
	xzm_debug_assert(_xzm_segment_group_segment_is_valid(sg, segment));
	return chunk;
}

// mimalloc: mi_segment_alloc
//
// Used to allocate both normal and huge segments.
//
// Postcondition: for normal segments, the segment group lock is held on
// successful return
static bool
_xzm_segment_group_alloc_segment(xzm_segment_group_t sg, size_t required_bytes,
		size_t alignment, xzm_chunk_t *huge_chunk, bool purgeable)
{
	xzm_chunk_t chunk;
	xzm_debug_assert((required_bytes == 0 && huge_chunk == NULL) ||
			(required_bytes > 0 && huge_chunk != NULL));

	bool huge = (required_bytes != 0);

	// non-default segment alignment is only supported for huge chunks
	xzm_debug_assert(huge || alignment == 0);

	// The total number of bytes we need to allocate is then:
	// - For normal segments, exactly the standard segment size
	// - For huge segments, the required body size, rounded up to the next slice
	size_t total_required_bytes;
	if (huge) {
		total_required_bytes = roundup(required_bytes, XZM_SEGMENT_SLICE_SIZE);
	} else {
		total_required_bytes = XZM_SEGMENT_SIZE;
	}

	xzm_range_group_t rg = sg->xzsg_range_group;

	xzm_range_group_alloc_flags_t rga_flags = 0;
	if (huge) {
		rga_flags |= XZM_RANGE_GROUP_ALLOC_FLAGS_HUGE;
	}

	if (purgeable) {
		rga_flags |= XZM_RANGE_GROUP_ALLOC_FLAGS_PURGEABLE;
	}

#if CONFIG_MTE
	// XXX Note: we need to allocate all data segments as taggable in order for
	// tag_data to work, but the vast majority of the space will be for
	// large/huge, which is a significant waste.  We're okay with that because
	// tag_data is not the default/production configuration, but we may need to
	// be more efficient about this in the future.
	if (_xzm_segment_group_memtag_enabled(sg)) {
		rga_flags |= XZM_RANGE_GROUP_ALLOC_FLAGS_MTE;
	}
#endif

	void *segment_body = xzm_range_group_alloc_segment(rg, total_required_bytes,
			alignment, mvm_plat_map(*map_ptr), rga_flags);
	if (!segment_body) {
		return false;
	}

	xzm_assert((uintptr_t)segment_body < XZM_LIMIT_ADDRESS);

	xzm_segment_t segment_meta = xzm_metapool_alloc(
			&sg->xzsg_main_ref->xzmz_metapools[XZM_METAPOOL_SEGMENT]);


	chunk = _xzm_segment_group_init_segment(sg, segment_meta, segment_body,
			total_required_bytes, huge, true);

	// Publish the segment in the segment table now that it has been properly
	// initialized
	_xzm_segment_table_allocated_at(sg->xzsg_main_ref, segment_body,
			segment_meta, !huge);

	if (huge) {
		*huge_chunk = chunk;
	}
	return true;
}

#if CONFIG_XZM_DEFERRED_RECLAIM

static xzm_chunk_t
_xzm_segment_group_alloc_huge_chunk_from_cache(xzm_segment_group_t sg,
		xzm_slice_count_t slice_count)
{
	xzm_debug_assert(sg->xzsg_id == XZM_SEGMENT_GROUP_DATA_LARGE);

	xzm_segment_t best_seg, cur_seg, seg_tmp;
	xzm_segment_cache_t cache = &sg->xzsg_cache;
	xzm_chunk_t chunk = NULL;

	_malloc_lock_lock(&cache->xzsc_lock);

	if (cache->xzsc_count == 0) {
		_malloc_lock_unlock(&cache->xzsc_lock);
		return NULL;
	}

	xzm_reclaim_buffer_t buffer = sg->xzsg_main_ref->xzmz_reclaim_buffer;
	while (1) {
		best_seg = NULL;
		TAILQ_FOREACH_SAFE(cur_seg, &cache->xzsc_head, xzs_cache_entry, seg_tmp) {
			if (cur_seg->xzs_slice_count >= slice_count &&
					// allow up to 50% fragmentation
					(cur_seg->xzs_slice_count < (2 * slice_count)) &&
					(best_seg == NULL ||
					cur_seg->xzs_slice_count < best_seg->xzs_slice_count)) {
				if (_xzm_reclaim_is_reusable(buffer,
						cur_seg->xzs_reclaim_id, true)) {
					best_seg = cur_seg;
				} else {
					// Kernel has already reclaimed this entry or
					// is in the process of trying to reclaim it.
					_xzm_segment_group_cache_invalidate(sg, cur_seg);
				}
			}
		}

		if (best_seg == NULL) {
			// Unable to find a suitable entry
			_malloc_lock_unlock(&cache->xzsc_lock);
			return NULL;
		}

		if (_xzm_segment_group_cache_mark_used(sg, best_seg)) {
			// entry has been reclaimed
			break;
		}
	}

	_malloc_lock_unlock(&cache->xzsc_lock);

	// Mark segment as allocated since it has been removed from the cache
	_xzm_segment_table_allocated_at(sg->xzsg_main_ref,
			_xzm_segment_start(best_seg), best_seg, false);

	chunk = (xzm_chunk_t)_xzm_segment_slices_begin(best_seg);

	return chunk;
}

#endif // CONFIG_XZM_DEFERRED_RECLAIM

// mimalloc: mi_segment_huge_page_alloc
static xzm_chunk_t
_xzm_segment_group_alloc_huge_chunk(xzm_segment_group_t sg,
		xzm_slice_count_t slice_count, bool clear, size_t alignment,
		bool purgeable)
{
	if (alignment < XZM_SEGMENT_SIZE) {
		// Huge chunks guarantee segment alignment
		alignment = 0;
	}

	xzm_debug_assert(alignment % XZM_SEGMENT_SIZE == 0);
	__assert_only bool defer_large = sg->xzsg_main_ref->xzmz_defer_large;
	xzm_debug_assert(sg->xzsg_id == XZM_SEGMENT_GROUP_DATA_LARGE ||
			!defer_large);
	xzm_debug_assert(sg->xzsg_id == XZM_SEGMENT_GROUP_DATA || defer_large);

	size_t required_bytes = (size_t)slice_count * XZM_SEGMENT_SLICE_SIZE;
	xzm_chunk_t chunk = NULL;

#if CONFIG_XZM_DEFERRED_RECLAIM
	if (sg->xzsg_id == XZM_SEGMENT_GROUP_DATA_LARGE &&
			sg->xzsg_cache.xzsc_max_count > 0 &&
			slice_count <= sg->xzsg_cache.xzsc_max_entry_slices &&
			alignment <= XZM_SEGMENT_SIZE) {
		chunk = _xzm_segment_group_alloc_huge_chunk_from_cache(sg, slice_count);
		if (chunk) {
			if (clear) {
				size_t chunk_size = 0;
				uint8_t *start = _xzm_chunk_start_ptr(
						&sg->xzsg_main_ref->xzmz_base,
						chunk, &chunk_size);
#if CONFIG_REALLOC_CAN_USE_VMCOPY
				// rdar://140793773
				bzero(start, chunk_size);
#else
				_xzm_segment_group_clear_chunk(sg, start, chunk_size);
#endif
				chunk->xzc_bits.xzcb_is_pristine = true;
			} else {
				chunk->xzc_bits.xzcb_is_pristine = false;
			}
#ifdef DEBUG
			size_t chunk_size = 0;
			uintptr_t start = (uintptr_t)_xzm_chunk_start_ptr(
					&sg->xzsg_main_ref->xzmz_base, chunk, &chunk_size);
			xzm_debug_assert(alignment == 0 || (start % alignment) == 0);
#endif // DEBUG
			return chunk;
		}
	}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
	// huge chunks allocated from the VM are inherently clear
	bool allocated = _xzm_segment_group_alloc_segment(sg, required_bytes,
			alignment, &chunk, purgeable);
	return allocated ? chunk : NULL;
}

static xzm_chunk_t
_xzm_segment_group_alloc_segment_and_chunk(xzm_segment_group_t sg,
		xzm_slice_kind_t kind, xzm_xzone_guard_config_t guard_config,
		xzm_preallocate_list_s *preallocate_list, xzm_slice_count_t slice_count,
		size_t alignment)
{
	xzm_chunk_t chunk = NULL;

	bool allocated = _xzm_segment_group_alloc_segment(sg, 0, 0, NULL, false);
	if (!allocated) {
		goto alloc_done;
	}

	// We hold the main lock again (alloc took it for us).  Since we were
	// able to allocate, we should be sure to get the chunk.
	chunk = _xzm_segment_group_find_and_allocate_chunk(sg, kind, guard_config,
			preallocate_list, slice_count, alignment);
	xzm_debug_assert(chunk);
	_malloc_lock_unlock(&sg->xzsg_lock);

alloc_done:
	_malloc_lock_unlock(&sg->xzsg_alloc_lock);
	return chunk;
}

static void
_xzm_segment_group_bzero_chunk(xzm_segment_group_t sg, uint8_t *start, size_t size)
{
	// Put a ceiling on the amount of memory we dirty at a time
	size_t max_clear_size = KiB(512);

	while (size) {
		size_t next_clear_size = MIN(size, max_clear_size);
		bzero(start, next_clear_size);
		xzm_madvise(&sg->xzsg_main_ref->xzmz_base, start, next_clear_size);

		start += next_clear_size;
		size -= next_clear_size;
	}
}

static void
_xzm_segment_group_clear_chunk(xzm_segment_group_t sg, uint8_t *start, size_t size)
{
#if CONFIG_MADV_ZERO
	if (madvise(start, size, MADV_ZERO)) {
#ifdef DEBUG
		malloc_zone_error(0, false,
				"Failed to madvise(MADV_ZERO) chunk at %p, error: %d\n",
				start, errno);
#endif
		return _xzm_segment_group_bzero_chunk(sg, start, size);
	}
#else
	return _xzm_segment_group_bzero_chunk(sg, start, size);
#endif // CONFIG_MADV_ZERO
}

static void
_xzm_segment_group_overwrite_chunk(uint8_t *start, size_t size,
		xzm_range_group_alloc_flags_t rga_flags)
{
	mach_vm_address_t vm_addr = (mach_vm_address_t)start;
	mach_vm_size_t vm_size = (mach_vm_size_t)size;
	int alloc_flags = VM_FLAGS_OVERWRITE | VM_MAKE_TAG(VM_MEMORY_MALLOC_SMALL);
#if CONFIG_MTE
	if (rga_flags & XZM_RANGE_GROUP_ALLOC_FLAGS_MTE) {
		alloc_flags |= VM_FLAGS_MTE;
	}
#endif
	kern_return_t kr = mach_vm_map(mach_task_self(), &vm_addr, vm_size,
			/* mask */ 0, alloc_flags, MEMORY_OBJECT_NULL,
			/* offset */ 0, /* copy */ FALSE, VM_PROT_DEFAULT,
			VM_PROT_ALL, VM_INHERIT_DEFAULT);
	if (kr != KERN_SUCCESS) {
		xzm_abort_with_reason("mach_vm_map() overwrite failed", kr);
	}
}

// mimalloc: mi_segments_page_alloc
xzm_chunk_t
xzm_segment_group_alloc_chunk(xzm_segment_group_t sg, xzm_slice_kind_t kind,
		xzm_xzone_guard_config_t guard_config, xzm_slice_count_t slice_count,
		xzm_preallocate_list_s *preallocate_list, size_t alignment, bool clear,
		bool purgeable) {
	if (kind == XZM_SLICE_KIND_HUGE_CHUNK) {
		xzm_debug_assert(guard_config == NULL);
		xzm_debug_assert(preallocate_list == NULL);
		xzm_debug_assert((slice_count >
				XZM_LARGE_BLOCK_SIZE_MAX / XZM_SEGMENT_SLICE_SIZE) ||
				(alignment > XZM_ALIGNMENT_MAX));
		return _xzm_segment_group_alloc_huge_chunk(sg, slice_count, clear,
				alignment, purgeable);
	}
	xzm_debug_assert(kind == XZM_SLICE_KIND_LARGE_CHUNK || alignment == 0);

	// Due to alignment, it's possible for the xzone layer to request a single
	// page large chunk. The segment layer assumes that such chunks can't exist,
	// so we round up the slice count here
	if (kind == XZM_SLICE_KIND_LARGE_CHUNK && slice_count == 1) {
		slice_count = 2;
	}

	// Consider: round up slice_count like mimalloc does?

	// We don't want to hold the main segment group lock while interacting with
	// the VM so that other allocations and deallocations that don't need to can
	// be served concurrently, but we do want to limit ourselves to allocating
	// only one new segment at a time so that we don't overshoot what we need if
	// many threads arrive during a period where a new segment is needed.
	//
	// So, we also have an "allocations lock", and the protocol is that a thread
	// wanting to allocate new VM must acquire it before going off to the VM.

	xzm_chunk_t chunk = NULL;

	_malloc_lock_lock(&sg->xzsg_lock);
	chunk = _xzm_segment_group_find_and_allocate_chunk(sg, kind, guard_config,
			preallocate_list, slice_count, alignment);
	if (chunk) {
		// Happy path: we got the chunk and are done.
		_malloc_lock_unlock(&sg->xzsg_lock);
		goto done;
	}

	// First try didn't succeed, so we need a new segment.  See if we can get
	// the alloc lock to allocate a new segment.
	bool gotlock = _malloc_lock_trylock(&sg->xzsg_alloc_lock);
	if (os_likely(gotlock)) {
		// We got it, so we can try to directly allocate a new segment.
		_malloc_lock_unlock(&sg->xzsg_lock);
		chunk = _xzm_segment_group_alloc_segment_and_chunk(sg, kind,
				guard_config, preallocate_list, slice_count, alignment);
	} else {
		// We didn't get it, so somebody else is allocating.  We need to drop
		// the main lock...
		_malloc_lock_unlock(&sg->xzsg_lock);

		// ... and wait for them on the alloc lock.
		_malloc_lock_lock(&sg->xzsg_alloc_lock);

		// Now that we've got the alloc lock, reacquire the main lock and try to
		// allocate from the new segment that the thread we were waiting for
		// would have installed.
		_malloc_lock_lock(&sg->xzsg_lock);
		chunk = _xzm_segment_group_find_and_allocate_chunk(sg, kind,
				guard_config, preallocate_list, slice_count, alignment);
		_malloc_lock_unlock(&sg->xzsg_lock);

		if (chunk) {
			// We were able to allocate from the new segment.
			_malloc_lock_unlock(&sg->xzsg_alloc_lock);
		} else {
			// The entire new segment has already been exhausted while we were
			// waiting for the alloc lock.  We have it now, so it's our turn to
			// allocate a new segment.
			chunk = _xzm_segment_group_alloc_segment_and_chunk(sg, kind,
					guard_config, preallocate_list, slice_count, alignment);
		}
	}

done:

	if (chunk) {
		size_t chunk_size;
		uint8_t *start = _xzm_chunk_start_ptr(&sg->xzsg_main_ref->xzmz_base,
				chunk, &chunk_size);
#if CONFIG_MTE
		const bool memtag_enabled =
				_xzm_segment_group_memtag_block(sg, chunk_size);
#endif
		if (!chunk->xzc_bits.xzcb_is_pristine) {
			if (_xzm_segment_group_has_madvise_workaround(sg) &&
					kind == XZM_SLICE_KIND_LARGE_CHUNK) {
				xzm_range_group_alloc_flags_t rga_flags = 0;
#if CONFIG_MTE
				if (memtag_enabled) {
					rga_flags |= XZM_RANGE_GROUP_ALLOC_FLAGS_MTE;
				}
#endif
				_xzm_segment_group_overwrite_chunk(start, chunk_size, rga_flags);
				chunk->xzc_bits.xzcb_is_pristine = true;
			} else if (clear) {
				// TODO: is this the right cutoff?
				if (kind == XZM_SLICE_KIND_TINY_CHUNK) {
					// It's just one page that we're going to fault anyway
					bzero(start, chunk_size);
				} else {
					_xzm_segment_group_clear_chunk(sg, start, chunk_size);
				}

				chunk->xzc_bits.xzcb_is_pristine = true;
			}
		}

		if (os_unlikely(purgeable)) {
			xzm_debug_assert(guard_config == NULL);
			xzm_debug_assert(kind == XZM_SLICE_KIND_LARGE_CHUNK);
			mach_vm_address_t vm_addr = (mach_vm_address_t)start;
			mach_vm_size_t vm_size = (mach_vm_size_t)chunk_size;
			int alloc_flags = VM_FLAGS_OVERWRITE |
					VM_MAKE_TAG(VM_MEMORY_MALLOC_SMALL) | VM_FLAGS_PURGABLE;
#if CONFIG_MTE
			if (memtag_enabled) {
				alloc_flags |= VM_FLAGS_MTE;
			}
#endif
			kern_return_t kr = mach_vm_map(mach_task_self(), &vm_addr, vm_size,
					/* mask */ 0, alloc_flags, MEMORY_OBJECT_NULL,
					/* offset */ 0, /* copy */ FALSE, VM_PROT_DEFAULT,
					VM_PROT_ALL, VM_INHERIT_DEFAULT);
			if (kr != KERN_SUCCESS) {
				xzm_abort_with_reason("mach_vm_map() overwrite failed", kr);
			}
		}
	}

	return chunk;
}

// mimalloc: mi_segment_span_remove_from_queue
static void
_xzm_segment_group_segment_span_remove_from_queue(xzm_segment_group_t sg,
		xzm_free_span_t span, xzm_slice_count_t slice_count)
{
	(void)sg; (void)slice_count;
	LIST_REMOVE(span, xzc_entry);
}

// mimalloc: mi_segment_span_free_coalesce
//
// TODO: more nuanced policy for zero-tracking
// - Right now we do the easy thing, which is to mark the entire coalesced free
//   span as dirty because the chunk being deallocated is
// - However, that's probably not optimal if we're coalescing something small
//   with a very large free span - e.g. the initial pristine span
// - One possibility would be to compare the sizes of the chunk being freed and
//   the spans being coalesced with - if the spans we're coalescing with are
//   relatively large and already zero-initialized, it may be better to just
//   zero the chunk being freed and maintain the zero initialization of the new
//   span as a whole
// - The risk of that, though, is that we may waste time zeroing chunks that
//   aren't going to wind up being used to serve cleared allocations anyway
static xzm_free_span_t
_xzm_segment_group_segment_span_free_coalesce(xzm_segment_group_t sg,
		xzm_segment_t segment, xzm_chunk_t chunk, bool *success_out)
{
	xzm_slice_count_t slice_count;
	if (_xzm_slice_kind_is_chunk(chunk->xzc_bits.xzcb_kind)) {
		slice_count = _xzm_chunk_slice_count(chunk);
	} else if (_xzm_slice_kind_is_free_span(chunk->xzc_bits.xzcb_kind)) {
		slice_count = _xzm_free_span_slice_count(chunk);
	} else {
		xzm_abort("attempting to coalesce slice of unexpected type");
	}

	xzm_free_span_t span = chunk;

	if (success_out) {
		*success_out = true;
	}

	// "unpublish" the chunk for enumeration as early as possible by resetting
	// its kind
	span->xzc_bits.xzcb_kind = XZM_SLICE_KIND_INVALID;

	xzm_slice_t next = chunk + slice_count;
	if (next < _xzm_segment_slices_end(segment) &&
			_xzm_slice_kind_is_free_span(next->xzc_bits.xzcb_kind)) {
#if CONFIG_XZM_DEFERRED_RECLAIM
		if (_xzm_segment_group_uses_deferred_reclamation(sg)) {
			if (!_xzm_segment_group_span_mark_used(sg, next)) {
				if (success_out) {
					*success_out = false;
				}
				goto previous;
			}
		}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		xzm_slice_count_t next_slice_count = _xzm_free_span_slice_count(next);
		slice_count += next_slice_count; // extend
		_xzm_segment_group_segment_span_remove_from_queue(sg, next,
				next_slice_count);
	}

#if CONFIG_XZM_DEFERRED_RECLAIM
previous:
#endif // CONFIG_XZM_DEFERRED_RECLAIM
	if (span > _xzm_segment_slices_begin(segment)) {
		xzm_slice_t prev = _xzm_span_slice_first(span - 1);
		if (_xzm_slice_kind_is_free_span(prev->xzc_bits.xzcb_kind)) {
#if CONFIG_XZM_DEFERRED_RECLAIM
			if (_xzm_segment_group_uses_deferred_reclamation(sg)) {
				if (!_xzm_segment_group_span_mark_used(sg, prev)) {
					if (success_out) {
						*success_out = false;
					}
					goto done;
				}
			}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
			xzm_slice_count_t prev_slice_count =
					_xzm_free_span_slice_count(prev);
			slice_count += prev_slice_count;
			_xzm_segment_group_segment_span_remove_from_queue(sg, prev,
					prev_slice_count);
			span = prev;
		}
	}

#if CONFIG_XZM_DEFERRED_RECLAIM
done:
#endif // CONFIG_XZM_DEFERRED_RECLAIM
	// and add the new free span
	_xzm_segment_group_segment_span_free(sg, segment,
			_xzm_slice_index(segment, span), slice_count, true, false);
	return span;
}

static void
_xzm_segment_group_segment_deallocate(xzm_segment_group_t sg,
		xzm_segment_t segment, bool free_from_table)
{
	// Remove the segment from the segment map
	if (free_from_table) {
		_xzm_segment_table_freed_at(sg->xzsg_main_ref,
				_xzm_segment_start(segment), segment, true);
	}

	size_t size = segment->xzs_slice_count * XZM_SEGMENT_SLICE_SIZE;
	xzm_range_group_free_segment_body(sg->xzsg_range_group,
			_xzm_segment_start(segment), size, mvm_plat_map(segment->xzs_map));
	xzm_metapool_free(&sg->xzsg_main_ref->xzmz_metapools[XZM_METAPOOL_SEGMENT],
			segment);
}

// mimalloc: mi_segment_free
static void
_xzm_segment_group_segment_free(xzm_segment_group_t sg, xzm_segment_t segment)
{
	xzm_debug_assert(segment->xzs_used == 0);
	xzm_free_span_t span = _xzm_segment_slices_begin(segment);

#if CONFIG_XZM_DEFERRED_RECLAIM
	xzm_free_span_t next;
	if (_xzm_segment_group_uses_deferred_reclamation(sg)) {
		if (!_xzm_segment_group_span_mark_used(sg, span)) {
			// kernel is holding this span busy
			goto fail;
		}
		while (_xzm_free_span_slice_count(span) < _xzm_segment_slice_count(segment)) {
			bool success;
			_xzm_segment_group_segment_span_remove_from_queue(sg, span,
					span->xzcs_slice_count);
			span = _xzm_segment_group_segment_span_free_coalesce(sg, segment,
					span, &success);
			if (!success) {
				// kernel is holding an adjacent span busy
				goto fail;
			}
		}
	}
#endif // CONFIG_XZM_DEFERRED_RECLAIM

	// The segment should have exactly one free span, which we need to now
	// remove from its span queue
	xzm_debug_assert(span->xzc_bits.xzcb_kind == XZM_SLICE_KIND_MULTI_FREE);
	xzm_debug_assert(span->xzcs_slice_count == segment->xzs_slice_count);

	_xzm_segment_group_segment_span_remove_from_queue(sg, span,
			span->xzcs_slice_count);

	// Drop the segment group lock before going off to the VM
	_malloc_lock_unlock(&sg->xzsg_lock);

	_xzm_segment_group_segment_deallocate(sg, segment, true);
	return;

#if CONFIG_XZM_DEFERRED_RECLAIM
fail:;
	// Kernel is holding a span busy, place any re-used spans back in the
	// buffer.
	next = _xzm_segment_slices_begin(segment);
	do {
		span = next;
		if (!_xzm_segment_slice_is_deferred(segment, span)) {
			_xzm_segment_group_span_mark_free(sg, span);
		}
		next = span + _xzm_free_span_slice_count(span);
	} while (next < _xzm_segment_slices_end(segment));
	_malloc_lock_unlock(&sg->xzsg_lock);
	return;
#endif // CONFIG_XZM_DEFERRED_RECLAIM
}

// trim unneeded space off the end of a huge segment
static void
_xzm_segment_group_split_huge_segment(xzm_segment_group_t sg, xzm_segment_t segment,
		xzm_slice_count_t required_slices)
{
	xzm_debug_assert(segment->xzs_kind == XZM_SEGMENT_KIND_HUGE);
	xzm_debug_assert(segment->xzs_slice_count >= required_slices);
	if (segment->xzs_slice_count == required_slices) {
		return;
	}

	uint8_t *start = _xzm_segment_start(segment);
	uint8_t *end = _xzm_segment_end(segment);

	uint8_t *remainder = (uint8_t *)(start +
			required_slices * XZM_SEGMENT_SLICE_SIZE);
	if (remainder < end) {
		size_t total_remainder_size = (size_t)(end - remainder);
#if CONFIG_XZM_DEFERRED_RECLAIM
		// new segments must be created on a SEGMENT_SIZE boundary to be annotated
		// in the segment table
		uint8_t *remainder_seg = (uint8_t *)roundup((uintptr_t)remainder,
				XZM_SEGMENT_SIZE);
		xzm_metapool_t metapool =
				&sg->xzsg_main_ref->xzmz_metapools[XZM_METAPOOL_SEGMENT];
		xzm_segment_t remainder_metadata = xzm_metapool_alloc(metapool);
		size_t remainder_seg_size = (end - remainder_seg);

		// If the remainder that we're freeing spans a segment granule, we need
		// to clear the entries from the segment map
		if (remainder_seg < end) {
			_xzm_segment_table_freed_at(sg->xzsg_main_ref, remainder_seg,
					segment, false);
		}

		_malloc_lock_lock(&sg->xzsg_cache.xzsc_lock);
		if (remainder_seg < end &&
				remainder_seg_size > XZM_LARGE_BLOCK_SIZE_MAX &&
				sg->xzsg_cache.xzsc_count < sg->xzsg_cache.xzsc_max_count) {
			// create a new segment from the end of this one and add it back to
			// the cache

			_xzm_segment_group_init_segment(sg, remainder_metadata,
					remainder_seg, remainder_seg_size, true, false);
			_xzm_segment_group_cache_mark_free(sg, remainder_metadata);

			_malloc_lock_unlock(&sg->xzsg_cache.xzsc_lock);

			if (remainder_seg > remainder) {
				// free the unused portion of the current segment
				size_t remainder_size = total_remainder_size -
						remainder_seg_size;
				xzm_range_group_free_segment_body(sg->xzsg_range_group,
						(void *)remainder, remainder_size, NULL);
			}
		} else {
			_malloc_lock_unlock(&sg->xzsg_cache.xzsc_lock);
			// cannot create a cached segment out of the remainder,
			// free it instead.
			xzm_metapool_free(metapool, remainder_metadata);
			xzm_range_group_free_segment_body(sg->xzsg_range_group,
					(void *)remainder, total_remainder_size, NULL);
		}
#else // CONFIG_XZM_DEFERRED_RECLAIM
		uint8_t *remainder_seg = (uint8_t *)roundup((uintptr_t)remainder,
				XZM_SEGMENT_SIZE);
		// If the body that we're freeing spans a segment granule, we need to
		// clear the entries from the segment map
		if (remainder_seg < end) {
			_xzm_segment_table_freed_at(sg->xzsg_main_ref, remainder_seg,
					segment, false);
		}
		xzm_range_group_free_segment_body(sg->xzsg_range_group, (void *)remainder,
				total_remainder_size, NULL);
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		// re-initialize original segment with reduced slice count
		_xzm_segment_group_init_segment(sg, segment,
				_xzm_segment_start(segment),
				required_slices * XZM_SEGMENT_SLICE_SIZE, true, false);
	}
	xzm_debug_assert(_xzm_segment_end(segment) == remainder);
}

#if CONFIG_XZM_DEFERRED_RECLAIM

static bool
_xzm_segment_group_free_huge_chunk_to_cache(xzm_segment_group_t sg,
		xzm_segment_t segment, xzm_chunk_t chunk) {
	xzm_segment_cache_t cache = &sg->xzsg_cache;
	xzm_segment_t entry = NULL, tmp_entry = NULL;
	xzm_debug_assert(cache->xzsc_max_count > 0);

	if (segment->xzs_slice_count > cache->xzsc_max_entry_slices) {
		// Do this check (and all others that can cause us to return false)
		// before modifying the segment table
		return false;
	}

	// The data for this segment could be asynchronously reclaimed and reused
	// before the metadata is invalidated/removed from the segment table, so we
	// need to remove this segment from the segment table before putting it into
	// the cache. If reused, the segment will be marked allocated in
	// _xzm_segment_group_alloc_huge_chunk_from_cache
	_xzm_segment_table_freed_at(sg->xzsg_main_ref,
			_xzm_segment_start(segment), segment, true);

#if CONFIG_MTE
	// We are committed to returning the chunk to the cache and have removed
	// access to it from the segment table.  We can safely retag now before taking
	// the cache lock.
	if (_xzm_segment_group_memtag_enabled(sg)) {
		size_t chunk_size = 0;
		void *ptr = _xzm_chunk_start_ptr(
				&sg->xzsg_main_ref->xzmz_base, chunk, &chunk_size);
		memtag_tag_canonical(ptr, chunk_size);
		// Note: for better protection from canonical pointers into huge chunks we
		// could retag with a random tag here (which will require code changes on
		// the alloc path also).
	}
#endif

	_malloc_lock_lock(&cache->xzsc_lock);

	xzm_reclaim_buffer_t buffer = sg->xzsg_main_ref->xzmz_reclaim_buffer;

	if (sg->xzsg_cache.xzsc_count == sg->xzsg_cache.xzsc_max_count) {
		// cache is full, sweep through the cache to find invalid entries
		TAILQ_FOREACH_SAFE(entry, &sg->xzsg_cache.xzsc_head,
				xzs_cache_entry, tmp_entry) {
			if (!_xzm_reclaim_is_reusable(buffer,
					entry->xzs_reclaim_id, true)) {
				_xzm_segment_group_cache_invalidate(sg, entry);
				continue;
			} else {
				// cache entries are kept in LRU order - encountering an
				// available one implies all other cache entries are also
				// available
				break;
			}
		}
	}

	while (cache->xzsc_count == cache->xzsc_max_count) {
		// Cache is full, evict the oldest entry
		_xzm_segment_group_cache_evict(sg);
	}

	// insert segment into cache
	_xzm_segment_group_cache_mark_free(sg, segment);
	_malloc_lock_unlock(&cache->xzsc_lock);
	return true;
}

#endif // CONFIG_XZM_DEFERRED_RECLAIM

// mimalloc: _mi_segment_huge_page_free
static void
_xzm_segment_group_free_huge_chunk(xzm_segment_group_t sg, xzm_chunk_t chunk,
		bool purgeable)
{
	xzm_segment_t segment = _xzm_segment_for_slice(
			&sg->xzsg_main_ref->xzmz_base, chunk);
	xzm_debug_assert(segment->xzs_kind == XZM_SEGMENT_KIND_HUGE);
	xzm_debug_assert(segment->xzs_used == 1);

#if CONFIG_XZM_DEFERRED_RECLAIM
	if (sg->xzsg_cache.xzsc_max_count > 0 &&
			!purgeable &&
			segment->xzs_slice_count <= sg->xzsg_cache.xzsc_max_entry_slices &&
			segment->xzs_slice_count >
			(XZM_LARGE_BLOCK_SIZE_MAX / XZM_SEGMENT_SLICE_SIZE)) {
		if (_xzm_segment_group_free_huge_chunk_to_cache(sg, segment, chunk)) {
			return;
		}
	}
#else
	// No special handling of purgeable huge segments without the huge cache
	(void)purgeable;
#endif // CONFIG_XZM_DEFERRED_RECLAIM

	_xzm_segment_group_segment_deallocate(sg, segment, true);
}

static void
xzm_madvise(xzm_malloc_zone_t zone, uint8_t *start, size_t size)
{
	__assert_only int rc = mvm_madvise_plat(start, size, MADV_FREE_REUSABLE, 0,
			mvm_plat_map(xzm_segment_table_query(_xzm_malloc_zone_main(zone),
			start)->xzs_map));

#ifdef DEBUG
	if (rc) {
		// TODO: time for a compatibility break?  Make this fatal?
		malloc_zone_error(0, false,
				"Failed to madvise chunk at %p, error: %d\n", start, errno);
	}
#endif // DEBUG
}

void
xzm_segment_group_segment_madvise_span(xzm_segment_group_t sg,
		uint8_t *slice_start, xzm_slice_count_t count)
{
	xzm_debug_assert((uintptr_t)slice_start % XZM_SEGMENT_SLICE_SIZE == 0);
	size_t span_size = count * XZM_SEGMENT_SLICE_SIZE;
	xzm_madvise(&sg->xzsg_main_ref->xzmz_base, slice_start, span_size);
}

void
xzm_segment_group_segment_madvise_chunk(xzm_segment_group_t sg,
		xzm_chunk_t chunk)
{
	xzm_debug_assert(_xzm_slice_kind_is_chunk(chunk->xzc_bits.xzcb_kind));

	size_t chunk_size = 0;
	uint8_t *start = _xzm_chunk_start_ptr(&sg->xzsg_main_ref->xzmz_base, chunk,
			&chunk_size);
	xzm_madvise(&sg->xzsg_main_ref->xzmz_base, start, chunk_size);
}

// mimalloc: _mi_segment_page_free
void
xzm_segment_group_free_chunk(xzm_segment_group_t sg, xzm_chunk_t chunk,
		bool purgeable, bool small_madvise_needed)
{
	xzm_slice_kind_t kind = chunk->xzc_bits.xzcb_kind;
	xzm_debug_assert(_xzm_slice_kind_is_chunk(kind));

	if (kind == XZM_SLICE_KIND_HUGE_CHUNK) {
		_xzm_segment_group_free_huge_chunk(sg, chunk, purgeable);
		return;
	}

	size_t chunk_size = 0;
	uint8_t *start = _xzm_chunk_start_ptr(
			&sg->xzsg_main_ref->xzmz_base, chunk, &chunk_size);
	xzm_range_group_alloc_flags_t rga_flags = 0;
#if CONFIG_MTE
	if (_xzm_segment_group_memtag_enabled(sg)) {
		rga_flags |= XZM_RANGE_GROUP_ALLOC_FLAGS_MTE;
		// Clear tags for chunk before handing it back to segment group
		memtag_tag_canonical(start, chunk_size);
	}
#endif

	if (os_unlikely(purgeable)) {
		xzm_debug_assert(kind == XZM_SLICE_KIND_LARGE_CHUNK);
		// Remove the purgeability from this allocation before freeing back to
		// the segment
		_xzm_segment_group_overwrite_chunk(start, chunk_size, rga_flags);
	}

	xzm_segment_t segment = _xzm_segment_for_slice(
			&sg->xzsg_main_ref->xzmz_base, chunk);

	if (_xzm_segment_group_has_madvise_workaround(sg) &&
			kind == XZM_SLICE_KIND_LARGE_CHUNK) {
		_xzm_segment_group_overwrite_chunk(start, chunk_size, rga_flags);
	} else if (!_xzm_segment_group_uses_deferred_reclamation(sg) &&
			// Small chunks will have already been aggressively madvised
			// by the time they are free
			(kind != XZM_SLICE_KIND_SMALL_CHUNK || small_madvise_needed)) {
		xzm_segment_group_segment_madvise_chunk(sg, chunk);
	}

	_malloc_lock_lock(&sg->xzsg_lock);

	xzm_debug_assert(_xzm_segment_group_segment_is_valid(sg, segment));
#if CONFIG_XZM_DEFERRED_RECLAIM
	xzm_debug_assert(!(_xzm_segment_group_uses_deferred_reclamation(sg) &&
			_xzm_segment_slice_is_deferred(segment, chunk)));
#endif // CONFIG_XZM_DEFERRED_RECLAIM

	xzm_free_span_t span = _xzm_segment_group_segment_span_free_coalesce(sg, segment, chunk, NULL);
	segment->xzs_used--;
#if !CONFIG_XZM_DEFERRED_RECLAIM
	(void)span;
#endif // !CONFIG_XZM_DEFERRED_RECLAIM

	xzm_debug_assert(kind != XZM_SLICE_KIND_HUGE_CHUNK);
	const bool can_deallocate = sg->xzsg_main_ref->xzmz_deallocate_segment &&
			_xzm_segment_group_id_is_data(segment->xzs_segment_group->xzsg_id);
	if (segment->xzs_used == 0 && can_deallocate) {
		// Drops the segment group lock
		_xzm_segment_group_segment_free(sg, segment);
	} else {
#if CONFIG_XZM_DEFERRED_RECLAIM
		if (_xzm_segment_group_uses_deferred_reclamation(sg)) {
			_xzm_segment_group_span_mark_free(sg, span);
		}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		// TODO: sequester segments more efficiently - just leaving the final
		// whole-segment span in its span queue means its metadata page stays
		// dirty
		xzm_debug_assert(_xzm_segment_group_segment_is_valid(sg, segment));
		_malloc_lock_unlock(&sg->xzsg_lock);
	}
}

bool
xzm_segment_group_try_realloc_large_chunk(xzm_segment_group_t sg,
		xzm_segment_t segment, xzm_chunk_t chunk,
		xzm_slice_count_t new_slice_count)
{
	xzm_debug_assert(_xzm_segment_for_slice(&sg->xzsg_main_ref->xzmz_base,
			chunk) == segment);
	xzm_debug_assert(new_slice_count >
			(XZM_SMALL_BLOCK_SIZE_MAX / XZM_SEGMENT_SLICE_SIZE));
	xzm_debug_assert(new_slice_count <=
			(XZM_LARGE_BLOCK_SIZE_MAX / XZM_SEGMENT_SLICE_SIZE));
	xzm_debug_assert(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_LARGE_CHUNK);

	if (chunk->xzcs_slice_count < new_slice_count) {
		_malloc_lock_lock(&sg->xzsg_lock);
		xzm_slice_count_t slices_to_add = (new_slice_count -
				chunk->xzcs_slice_count);
		xzm_slice_t next_slice = chunk + chunk->xzcs_slice_count;
		xzm_slice_count_t next_free_slices = _xzm_free_span_slice_count(next_slice);
		// Check if adjacent chunk is in the right segment, free, and
		// large enough to realloc into
		if (next_slice >= _xzm_segment_slices_end(segment) ||
				!_xzm_slice_kind_is_free_span(next_slice->xzc_bits.xzcb_kind) ||
				next_free_slices < slices_to_add) {
			_malloc_lock_unlock(&sg->xzsg_lock);
			return false;
		}

		const xzm_slice_count_t next_slices_to_free =
				next_free_slices - slices_to_add;
		bool uses_dr = false;
#if CONFIG_XZM_DEFERRED_RECLAIM
		uses_dr = _xzm_segment_group_uses_deferred_reclamation(sg);
		if (uses_dr) {
			if (!_xzm_segment_group_span_mark_smaller(sg, next_slice, 0,
					slices_to_add, next_slices_to_free)) {
				// kernel is holding next span busy
				_malloc_lock_unlock(&sg->xzsg_lock);
				return false;
			}
		}
#endif // CONFIG_XZM_DEFERRED_RECLAIM

		_xzm_segment_group_segment_span_remove_from_queue(sg, next_slice,
				next_free_slices);

		// We can only split if there will be 1 or more free slices left over
		if (next_slices_to_free) {
			_xzm_segment_group_segment_slice_split(sg, segment, next_slice,
					slices_to_add, uses_dr, false);
		}

		for (int i = 0; i < slices_to_add; i++) {
			next_slice[i].xzc_bits.xzcb_kind = XZM_SLICE_KIND_MULTI_BODY;
			next_slice[i].xzsl_slice_offset_bytes = (uint32_t)
					(((uintptr_t)&next_slice[i]) - ((uintptr_t)chunk));
		}
		chunk->xzcs_slice_count = new_slice_count;
		xzm_debug_assert(_xzm_segment_group_segment_is_valid(sg, segment));
		_malloc_lock_unlock(&sg->xzsg_lock);

#if CONFIG_MTE
		// If block grows on realloc(), tag additional size with same tag as
		// allocation.
		if (_xzm_segment_group_memtag_enabled(sg)) {
			size_t additional_size = (slices_to_add * XZM_SEGMENT_SLICE_SIZE);
			size_t chunk_size;
			void *start = (void *)_xzm_chunk_start(
					&sg->xzsg_main_ref->xzmz_base, chunk, &chunk_size);
			size_t offset = chunk_size - additional_size;
			void *additional_start = _memtag_load_tag(start) + offset;
			memtag_set_tag(additional_start, additional_size);
		}
#endif

		return true;
	} else if (chunk->xzcs_slice_count > new_slice_count) {
		_malloc_lock_lock(&sg->xzsg_lock);

		xzm_slice_count_t slices_to_free = (chunk->xzcs_slice_count -
				new_slice_count);
		xzm_free_span_t span_to_free = chunk + new_slice_count;

		chunk->xzcs_slice_count = new_slice_count;

		xzm_slice_t last_slice = chunk + (chunk->xzcs_slice_count - 1);
		last_slice->xzc_bits.xzcb_kind = XZM_SLICE_KIND_MULTI_BODY;
		last_slice->xzsl_slice_offset_bytes = (uint32_t)
				(((uintptr_t)last_slice) - ((uintptr_t)chunk));

		// create a fake chunk out of the remainder before freeing it
		xzm_segment_t segment = _xzm_segment_for_slice(
				&sg->xzsg_main_ref->xzmz_base, chunk);
		xzm_slice_kind_t tail_kind = slices_to_free > 1 ?
				XZM_SLICE_KIND_LARGE_CHUNK : XZM_SLICE_KIND_TINY_CHUNK;
		_xzm_segment_group_segment_span_mark_allocated(sg, segment, tail_kind,
				_xzm_slice_index(segment, span_to_free), slices_to_free);
		_malloc_lock_unlock(&sg->xzsg_lock);
		// Realloc in place is disabled for the purgeable zone, so we can always
		// pass purgeable=false here
		xzm_segment_group_free_chunk(sg, span_to_free, false, false);
		return true;
	}
	return true; // old size == new size, so no-op
}

bool
xzm_segment_group_try_realloc_huge_chunk(xzm_segment_group_t sg,
		xzm_malloc_zone_t zone, xzm_segment_t segment,
		xzm_chunk_t chunk, xzm_slice_count_t new_slice_count)
{
	xzm_debug_assert(_xzm_segment_for_slice(&sg->xzsg_main_ref->xzmz_base,
			chunk) == segment);
	xzm_debug_assert(new_slice_count >
			(XZM_LARGE_BLOCK_SIZE_MAX / XZM_SEGMENT_SLICE_SIZE));
	xzm_debug_assert(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_HUGE_CHUNK);


	if (chunk->xzcs_slice_count < new_slice_count) {
		size_t current_size = 0;
		vm_address_t current_ptr = (vm_address_t)_xzm_chunk_start(zone, chunk,
				&current_size);
		vm_address_t addr_to_request = current_ptr + current_size;
		size_t slices_to_request = new_slice_count - chunk->xzcs_slice_count;
		size_t size_to_request = slices_to_request * XZM_SEGMENT_SLICE_SIZE;

		uintptr_t segment_to_check = roundup(addr_to_request, XZM_SEGMENT_SIZE);
		while (segment_to_check < (addr_to_request+size_to_request)) {
			// TODO: Once we have deferred reclaim for huge chunks, we have the
			// option to do something more clever here (e.g. if all segments
			// are unallocated or are still waiting to be reclaimed, then we
			// can acquire those and realloc)
			if (xzm_segment_table_query(sg->xzsg_main_ref,
					(void *)segment_to_check)) {
				return false;
			}
			segment_to_check += XZM_SEGMENT_SIZE;
		}

		int label = VM_MEMORY_REALLOC;
		void *addr = mvm_allocate_plat(addr_to_request, size_to_request,
				0, VM_FLAGS_FIXED, 0, label, mvm_plat_map(segment->xzs_map));
		if (addr) {
			size_t new_body_size = new_slice_count * XZM_SEGMENT_SLICE_SIZE;
			_xzm_segment_group_init_segment(sg, segment,
					_xzm_segment_start(segment), new_body_size, true, false);

			// If we expanded into new segment granules, mark them as allocated
			uintptr_t first_new_segment = roundup(addr_to_request,
					XZM_SEGMENT_SIZE);
			if ((uintptr_t)current_ptr + new_body_size > first_new_segment) {
				_xzm_segment_table_allocated_at(_xzm_malloc_zone_main(zone),
						(void *)first_new_segment, segment, false);
#if CONFIG_MTE
				// If block grows on realloc(), tag additional size with same tag as
				// allocation.
				if (_xzm_segment_group_memtag_enabled(sg)) {
					void *tagged_addr_to_request =
							_memtag_load_tag((void *)current_ptr) +
							current_size;
					memtag_set_tag(tagged_addr_to_request, size_to_request);
				}
#endif
			}

			xzm_debug_assert(_xzm_segment_group_segment_is_valid(sg, segment));
			return true;
		}
		return false;
	} else if (chunk->xzcs_slice_count > new_slice_count) {
		_xzm_segment_group_split_huge_segment(sg, segment, new_slice_count);
		xzm_debug_assert(_xzm_segment_group_segment_is_valid(sg, segment));
		return true;
	}
	return true; // old size == new size, so no-op
}

#endif // CONFIG_XZONE_MALLOC
