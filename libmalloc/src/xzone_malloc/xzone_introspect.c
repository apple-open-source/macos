/* ----------------------------------------------------------------------------
Copyright (c) 2018-2022, Microsoft Research, Daan Leijen
Copyright © 2025 Apple Inc.
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" in the same directory as this file.
-----------------------------------------------------------------------------*/

#include "../internal.h"

#if CONFIG_XZONE_MALLOC

#define XZM_DEBUG_ENUMERATOR 0

#pragma mark libmalloc segment introspection

kern_return_t
xzm_segment_group_segment_foreach_span(xzm_segment_t segment,
		xzm_span_enumerator_t enumerator)
{
	xzm_slice_t end = _xzm_segment_slices_end(segment);
	xzm_slice_t slice = _xzm_segment_slices_begin(segment);

	if (segment->xzs_kind == XZM_SEGMENT_KIND_HUGE) {
		return enumerator(slice, slice->xzcs_slice_count);
	}

	// Enumeration protocol: the kind bits of a chunk are updated last, after
	// the rest of the chunk metadata is initialized, so if we see a chunk slice
	// it should be valid.  Anything else may be in an intermediate state and is
	// not to be trusted, so rather than iterating span-by-span as we would
	// under the lock we need to scan linearly from chunk to chunk.
	kern_return_t kr;
	while (slice < end) {
		xzm_slice_kind_t kind = slice->xzc_bits.xzcb_kind;
		if (_xzm_slice_kind_is_chunk_safe(kind) ||
				// Guard pages aren't chunks, but should be enumerated like them
				kind == XZM_SLICE_KIND_GUARD) {
			xzm_slice_count_t slice_count;
			if (kind == XZM_SLICE_KIND_TINY_CHUNK) {
				slice_count = 1;
			} else {
				slice_count = slice->xzcs_slice_count;
			}

			kr = enumerator(slice, slice_count);
			if (kr) {
				return kr;
			}

			slice += slice_count;
		} else {
			// Scan forward to the next chunk.
			xzm_slice_t first_slice = slice;
			do {
				slice++;
			} while (!_xzm_slice_kind_is_chunk_safe(slice->xzc_bits.xzcb_kind) &&
					slice->xzc_bits.xzcb_kind != XZM_SLICE_KIND_GUARD &&
					slice < end);

			// Report the free span.
			kr = enumerator(first_slice,
					(xzm_slice_count_t)(slice - first_slice));
			if (kr) {
				return kr;
			}
		}
	}

	return KERN_SUCCESS;
}

kern_return_t
xzm_segment_table_foreach(xzm_segment_table_entry_s *segment_table,
		size_t num_entries, xzm_segment_table_enumerator_t enumerator,
		xzm_segment_t *last_segment_enumerated)
{
	xzm_segment_t last_segment = NULL;
	if (last_segment_enumerated) {
		last_segment = *last_segment_enumerated;
	}
	for (size_t i = 0; i < num_entries; i++) {
		xzm_segment_t segment =
				_xzm_segment_table_entry_to_segment(segment_table[i]);
		if (!segment) {
			continue;
		}
		// Huge segments can be in multiple adjacent entries in the segment map
		// if the segment spans multiple segment granules. Only enumerate the
		// first entry
		if (segment == last_segment) {
			continue;
		} else {
			last_segment = segment;
		}

		kern_return_t kr = enumerator((vm_address_t)segment);
		if (kr) {
			return kr;
		}
	}
	if (last_segment_enumerated) {
		*last_segment_enumerated = last_segment;
	}
	return KERN_SUCCESS;
}

#pragma mark libmalloc zone introspection

#if CONFIG_XZM_THREAD_CACHE

static kern_return_t
_xzm_introspect_enumerate_thread_caches(task_t task, memory_reader_t reader,
		xzm_main_malloc_zone_t main,
		MALLOC_NOESCAPE xzm_thread_cache_enumerator_t thread_cache_enumerator)
{
	xzm_debug_assert(main->xzmz_base.xzz_thread_cache_enabled);

	vm_address_t thread_cache_addr = (vm_address_t)LIST_FIRST(
			&main->xzmz_thread_cache_list);
	size_t thread_cache_size = sizeof(struct xzm_thread_cache_s) +
			(main->xzmz_base.xzz_thread_cache_xzone_count *
			sizeof(xzm_xzone_thread_cache_u));
	while (thread_cache_addr != 0) {
		xzm_thread_cache_t tc;
		kern_return_t kr = reader(task, thread_cache_addr, thread_cache_size,
				(void **)&tc);
		if (kr) {
			xzm_debug_abort("Failed to map thread cache");
			return kr;
		}

		kr = thread_cache_enumerator(thread_cache_addr, tc);
		if (kr) {
			// Allow KERN_RETURN_MAX as a way to request early exit
			if (kr != KERN_RETURN_MAX) {
				xzm_debug_abort("Failed to enumerate thread cache");
			}
			return kr;
		}

		thread_cache_addr = (vm_address_t)LIST_NEXT(tc, xtc_linkage);
	}

	return KERN_SUCCESS;
}

#endif // CONFIG_XZM_THREAD_CACHE

static kern_return_t
_xzm_introspect_small_chunk_blocks(xzm_malloc_zone_t zone,
		vm_address_t segment_addr, xzm_segment_t segment, xzm_chunk_t chunk,
		xzm_slice_count_t slice_count, uintptr_t start, vm_address_t start_addr,
		xzm_xzone_t xz, MALLOC_NOESCAPE xzm_chunk_enumerator_t chunk_enumerator)
{
	uint32_t block_size = (uint32_t)xz->xz_block_size;
	size_t capacity = xz->xz_chunk_capacity;

	union {
		vm_range_t range;
		bool free;
	} blocks[XZM_CHUNK_MAX_BLOCK_COUNT] = { 0 };

	size_t range_idx = 0;

	for (xzm_block_index_t block_index = 0; block_index < capacity;
			block_index++) {
		if (!_xzm_small_chunk_block_index_is_free(chunk, block_index)) {
			blocks[range_idx].range = (vm_range_t){
				.address = start_addr + block_index * block_size,
				.size = block_size,
			};
			range_idx++;
		}
	}

	return chunk_enumerator(segment_addr, segment, chunk, slice_count,
			start_addr, xz, (vm_range_t *)blocks, (unsigned)range_idx);
}

static kern_return_t
_xzm_introspect_freelist_chunk_blocks(task_t task, memory_reader_t reader,
		xzm_malloc_zone_t zone, vm_address_t segment_addr,
		xzm_segment_t segment, xzm_chunk_t chunk, xzm_slice_count_t slice_count,
		uintptr_t start, vm_address_t start_addr, xzm_xzone_t xz,
		MALLOC_NOESCAPE xzm_chunk_enumerator_t chunk_enumerator)
{
	kern_return_t kr = KERN_FAILURE;

	uint32_t block_size = (uint32_t)xz->xz_block_size;
	size_t granule = block_size > XZM_TINY_BLOCK_SIZE_MAX ?
			XZM_SMALL_GRANULE : XZM_GRANULE;
	size_t capacity = xz->xz_chunk_capacity;

	xzm_chunk_atomic_meta_u meta = chunk->xzc_atomic_meta;

	if (meta.xca_alloc_head == XZM_FREE_MADVISING ||
			meta.xca_alloc_head == XZM_FREE_MADVISED) {
		xzm_debug_assert(meta.xca_free_count == 0);
		// This chunk is madvised, so there can be no blocks in use
		return chunk_enumerator(segment_addr, segment, chunk, slice_count,
				start_addr, xz, NULL, 0);
	}

	xzm_debug_assert(meta.xca_free_count <= capacity);

#if CONFIG_MTE
	// This code can be invoked by memory tools that are not running
	// under MTE, to introspect a zone mapped in from a process that
	// is actually running under MTE. Therefore, we only ldg when we are running
	// in a process spawned with has_sec_transition=1.
	//
	// This should only matter for the case of memory tools on non-MTE hardware
	// introspecting processes running under MTE emulation, as we need to ensure
	// we don't try to execute the unsupported MTE instructions.  On real
	// hardware, we expect memory tools to run with Allocation Tag Access
	// disabled (SCTLR.ATA=0), so there should be no need to do anything to
	// safely access the mapped memory of a remote process even if it is running
	// under MTE.
#endif

	// To produce an array of all of the live blocks in a tiny chunk:
	// - We walk the chunk freelist, marking everything on it as free
	// - If the chunk is marked as installed to a thread cache:
	//     - We search the thread caches to find the specific one that the chunk
	//       is installed to
	//     - Once we find the cache containing the chunk, we walk the cache
	//       freelist, marking all of those blocks as free as well
	// - We determine the bump offset as the difference between the free count
	//   and the number of blocks
	// - Then we prepare the range array by initializing the range for each
	//   block below the bump offset that wasn't marked free on the first pass

	// N.B. Nano handles inconsistent freelist state by assuming that whatever
	// it saw before the inconsistency is what's on it.  It might be better to
	// return an error and let the caller know that the state is inconsistent,
	// but for now we'll do as nano does for compatibility.

	union {
		vm_range_t range;
		bool free;
	} blocks[XZM_CHUNK_MAX_BLOCK_COUNT] = { 0 };

	// First, walk the chunk freelist.  We should only walk it to the length on
	// the chunk.  In the thread caching case, a detaching thread may be in the
	// process of linking a local freelist to the end, but we'll pick those
	// blocks up later and don't want to include them in this walk.
	bool cached = (meta.xca_alloc_idx == XZM_SLOT_INDEX_THREAD_INSTALLED);

	size_t block_granule_size = block_size / granule;
	uint64_t block_offset = meta.xca_alloc_head;

	size_t total_chunk_freelist_count = meta.xca_free_count;
	size_t current_chunk_freelist_count = 0;
	while (current_chunk_freelist_count < total_chunk_freelist_count &&
			block_offset < XZM_FREE_LIMIT &&
			block_offset % block_granule_size == 0) {
		size_t block_index = block_offset / block_granule_size;
		if (blocks[block_index].free) {
			xzm_debug_abort("loop in freelist");
			break;
		}
		current_chunk_freelist_count++;
		blocks[block_index].free = true;

		xzm_block_t block = (xzm_block_t)(
				start + (block_offset * granule));
#if CONFIG_MTE && !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
		if (malloc_has_sec_transition) {
			block = (xzm_block_t)memtag_fixup_ptr((void *)block);
		}
#endif
		block_offset = block->xzb_linkage.xzbl_next_offset;
	}

	size_t allocated_limit = capacity;
	if (cached) {
#if CONFIG_XZM_THREAD_CACHE
		if (zone->xzz_main_ref) {
			xzm_debug_abort("cached chunk on non-main zone");
			goto fail;
		}

		// We should have walked the full reported length of the remote freelist
		if (current_chunk_freelist_count != total_chunk_freelist_count) {
			xzm_debug_abort("Cached chunk freelist walk incomplete");
			// XXX By failing the enumeration here, we're being stricter than
			// nano was about weirdness in the freelist.  That seems worth the
			// increased visibility into possible bugs this enumerator might
			// have, so for this case fail hard rather than allowing it.
			goto fail;
		}

		xzm_main_malloc_zone_t main = (xzm_main_malloc_zone_t)zone;

		xzm_xzone_index_t xz_idx = xz->xz_idx;
		if (xz_idx >= zone->xzz_thread_cache_xzone_count) {
			xzm_debug_abort("out-of-bounds cached xzone index");
			goto fail;
		}

		__block xzm_thread_cache_t tc = NULL;
		__block xzm_xzone_thread_cache_t cache = NULL;

		// This chunk is installed to a thread cache, so we need to go find the
		// right one.  The priority order is:
		// - If a thread cache for a non-detaching thread cache holds the chunk,
		//   it owns it.  There should be at most one such cache.
		// - Otherwise, if one or more detaching caches holds it, the cache with
		//   the highest teardown generation owns it.
		kern_return_t kr2 = _xzm_introspect_enumerate_thread_caches(task,
				reader, main, ^(vm_address_t thread_cache_addr,
						xzm_thread_cache_t curr_tc){
			xzm_xzone_thread_cache_t curr_cache =
					&curr_tc->xtc_xz_caches[xz_idx];
			if ((curr_cache->xztc_state < XZM_FREE_LIMIT ||
					curr_cache->xztc_state == XZM_FREE_NULL) &&
					(vm_address_t)curr_cache->xztc_chunk_start == start_addr) {
				// This is a match.  Is is a better match?

				// If the cache we're looking at isn't tearing down, it's a
				// perfect match, and we can stop searching.
				if (!curr_tc->xtc_teardown_gen) {
					xzm_debug_assert(!tc || tc->xtc_teardown_gen);
					tc = curr_tc;
					cache = curr_cache;
					return KERN_RETURN_MAX;
				}

				// Otherwise, if it is tearing down, if it's more recent than
				// the last one we saw then it's the best match we've seen so
				// far.
				if (!tc || curr_tc->xtc_teardown_gen > tc->xtc_teardown_gen) {
					tc = curr_tc;
					cache = curr_cache;
				}
			}
			return KERN_SUCCESS;
		});

		if (kr2 && kr2 != KERN_RETURN_MAX) {
			xzm_debug_abort("Failure enumerating thread caches");
			kr = kr2;
			goto fail;
		}

		if (!tc) {
			xzm_debug_abort("Failed to find cache for cached chunk");
			goto fail;
		}

		xzm_debug_assert(tc && cache);

		// Walk the local freelist to add its free blocks to the set of known
		// free blocks.
		size_t total_local_freelist_count = cache->xztc_free_count;
		uint64_t block_offset = cache->xztc_head;
		size_t current_local_freelist_count = 0;
		while (current_local_freelist_count < total_local_freelist_count &&
				block_offset < XZM_FREE_LIMIT &&
				block_offset % block_granule_size == 0) {
			size_t block_index = block_offset / block_granule_size;
			if (blocks[block_index].free) {
				xzm_debug_abort("loop in local freelist");
				break;
			}
			current_local_freelist_count++;
			blocks[block_index].free = true;

			xzm_block_t block = (xzm_block_t)(
					start + (block_offset * granule));
#if CONFIG_MTE && !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
			if (malloc_has_sec_transition) {
				block = (xzm_block_t)memtag_fixup_ptr((void *)block);
			}
#endif
			block_offset = block->xzb_linkage.xzbl_next_offset;
		}

		xzm_debug_assert(block_offset == XZM_FREE_NULL);

		// Account for the bump on the local freelist
		if (current_local_freelist_count < total_local_freelist_count &&
				total_local_freelist_count <= capacity) {
			allocated_limit = capacity -
					(total_local_freelist_count - current_local_freelist_count);
		}
#else // CONFIG_XZM_THREAD_CACHE
		xzm_debug_abort("Unexpected cached chunk");
		goto fail;
#endif // CONFIG_XZM_THREAD_CACHE
	} else {
		xzm_debug_assert(block_offset == XZM_FREE_NULL);

		// Account for the bump on the remote freelist
		if (current_chunk_freelist_count < total_chunk_freelist_count &&
				total_chunk_freelist_count <= capacity) {
			allocated_limit = capacity -
					(total_chunk_freelist_count - current_chunk_freelist_count);
		}
	}

	size_t range_idx = 0;
	for (size_t i = 0; i < allocated_limit; i++) {
		if (!blocks[i].free) {
			blocks[range_idx].range = (vm_range_t){
				.address = start_addr + i * block_size,
				.size = block_size,
			};
			range_idx++;
		}
	}

	return chunk_enumerator(segment_addr, segment, chunk, slice_count,
			start_addr, xz, (vm_range_t *)blocks, (unsigned)range_idx);

fail:
	xzm_debug_assert(kr);
	return kr;
}

static kern_return_t
_xzm_introspect_chunk_blocks(task_t task, memory_reader_t reader,
		xzm_malloc_zone_t zone, vm_address_t segment_addr,
		xzm_segment_t segment, xzm_chunk_t chunk, xzm_slice_count_t slice_count,
		uintptr_t start, vm_address_t start_addr, xzm_xzone_t xz,
		MALLOC_NOESCAPE xzm_chunk_enumerator_t chunk_enumerator)
{
	xzm_slice_kind_t kind = chunk->xzc_bits.xzcb_kind;
	if (!_xzm_slice_kind_uses_xzones(kind)) {
		// This is a large or huge chunk, which has exactly one block
		vm_range_t range = {
			.address = start_addr,
			.size = slice_count * XZM_SEGMENT_SLICE_SIZE,
		};

		return chunk_enumerator(segment_addr, segment, chunk, slice_count,
				start_addr, NULL, &range, 1);
	}

	uint32_t block_size = (uint32_t)xz->xz_block_size;
	size_t capacity = xz->xz_chunk_capacity;
	// Sanity check
	if ((slice_count * XZM_SEGMENT_SLICE_SIZE) / block_size != capacity ||
			capacity > XZM_CHUNK_MAX_BLOCK_COUNT) {
		xzm_debug_abort("inconsistent xzone info");
		return KERN_FAILURE;
	}

	if (kind == XZM_SLICE_KIND_SMALL_CHUNK) {
		return _xzm_introspect_small_chunk_blocks(zone, segment_addr, segment,
				chunk, slice_count, start, start_addr, xz, chunk_enumerator);
	}

	xzm_debug_assert(kind == XZM_SLICE_KIND_TINY_CHUNK ||
			kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK);
	return _xzm_introspect_freelist_chunk_blocks(task, reader, zone,
			segment_addr, segment, chunk, slice_count, start, start_addr, xz,
			chunk_enumerator);

}

static kern_return_t
_xzm_introspect_enumerate(task_t task, memory_reader_t reader,
		vm_address_t zone_address, xzm_malloc_zone_t zone,
		vm_address_t main_address, xzm_main_malloc_zone_t main,
		bool include_blocks,
		MALLOC_NOESCAPE xzm_metapool_enumerator_t metapool_slab_enumerator,
		MALLOC_NOESCAPE xzm_segment_enumerator_t segment_enumerator,
		MALLOC_NOESCAPE xzm_chunk_enumerator_t chunk_enumerator,
		MALLOC_NOESCAPE xzm_free_span_enumerator_t span_enumerator)
{
	bool zone_is_main = (zone_address == main_address);
	xzm_debug_assert(!span_enumerator || zone_is_main);

	size_t zone_size = zone_is_main ? main->xzmz_total_size :
			zone->xzz_total_size;

	if (zone_is_main) {
		size_t metapools_size;
		if (os_mul_overflow(main->xzmz_metapool_count,
				sizeof(struct xzm_metapool_s), &metapools_size)) {
			xzm_debug_abort("Failed to compute metapools size");
			return KERN_FAILURE;
		}
		xzm_metapool_t metapools = (xzm_metapool_t)_xzm_introspect_rebase(
				main_address, main, main->xzmz_total_size, main->xzmz_metapools,
				metapools_size);
		if (!metapools) {
			xzm_debug_abort("Failed to rebase metapools");
			return KERN_FAILURE;
		}
		for (int i = 0; i < main->xzmz_metapool_count; i++) {
			xzm_metapool_t mp = &metapools[i];
			vm_address_t slab_addr = (vm_address_t)SLIST_FIRST(&mp->xzmp_slabs);
			while (slab_addr != 0) {
				xzm_metapool_slab_t slab = NULL;

				kern_return_t kr = reader(task, slab_addr,
						sizeof(struct xzm_metapool_slab_s), (void **)&slab);
				if (kr) {
					xzm_debug_abort("Failed to map metapool slab");
					return kr;
				}

				kr = metapool_slab_enumerator((vm_address_t)slab->xzmps_base,
						mp->xzmp_slab_size, mp->xzmp_id);
				if (kr) {
					return kr;
				}

				slab_addr = (vm_address_t)SLIST_NEXT(slab, xzmps_entry);
			}
		}
	}

	size_t table_size;
	if (os_mul_overflow(XZM_SEGMENT_TABLE_ENTRIES,
			sizeof(xzm_segment_table_entry_s), &table_size)) {
		xzm_debug_abort("failed to compute segment table size");
		return KERN_FAILURE;
	}
	xzm_segment_table_entry_s *segment_table =
			(xzm_segment_table_entry_s *)_xzm_introspect_rebase(main_address,
			main, main->xzmz_total_size, main->xzmz_segment_table, table_size);
	if (!segment_table) {
		xzm_debug_abort("failed to rebase segment table");
		return KERN_FAILURE;
	}

	xzm_segment_table_enumerator_t enumerator = ^(vm_address_t segment_addr){
		xzm_segment_t segment;
		// Even for huge segments, we don't need to map more than normal segment
		// size because we don't need to see anything in the body of huge
		// segments.
		//
		// XXX Note: the mapped segment is _not_ guaranteed to have the same
		// alignment as the original segment, so many of the manipulation
		// helpers can't be used with it.

		// Map in the segment metadata to see how big it is.
		kern_return_t kr = reader(task, segment_addr,
				sizeof(struct xzm_segment_s), (void **)&segment);
		if (kr) {
			xzm_debug_abort("failed to map segment header");
			return kr;
		}

		void *segment_body;
		kr = reader(task, (vm_address_t)_xzm_segment_start(segment),
				segment->xzs_slice_count * XZM_SEGMENT_SLICE_SIZE,
				&segment_body);
		if (kr) {
			xzm_debug_abort("failed to map segment");
			return kr;
		}

		kr = segment_enumerator(segment_addr, segment, "    ");
		if (kr) {
			return kr;
		}

		return xzm_segment_group_segment_foreach_span(segment,
				^(xzm_slice_t span, xzm_slice_count_t slice_count){
			ptrdiff_t idx = span - segment->xzs_slices;
			size_t start_offset = idx * XZM_SEGMENT_SLICE_SIZE;
			uintptr_t start = (uintptr_t)segment_body + start_offset;
			uintptr_t orig_start = (uintptr_t)_xzm_segment_slice_index_start(
					segment, (xzm_slice_count_t)idx);
			vm_address_t start_addr = (vm_address_t)orig_start;

			xzm_slice_kind_t kind = span->xzc_bits.xzcb_kind;
			if (_xzm_slice_kind_is_chunk_safe(kind) &&
					span->xzc_mzone_idx == zone->xzz_mzone_idx) {
				// This is a chunk that belongs to this zone.
				xzm_chunk_t chunk = span;

				xzm_xzone_t xz = NULL;
				if (_xzm_slice_kind_uses_xzones(kind)) {
					xz = (xzm_xzone_t)_xzm_introspect_rebase(zone_address, zone,
							zone_size, &zone->xzz_xzones[chunk->xzc_xzone_idx],
							sizeof(struct xzm_xzone_s));
					if (!xz) {
						xzm_debug_abort("failed to rebase xzone");
						return KERN_FAILURE;
					}
				}

				if (include_blocks) {
					return _xzm_introspect_chunk_blocks(task, reader, zone,
							segment_addr, segment, chunk, slice_count, start,
							start_addr, xz, chunk_enumerator);
				} else {
					return chunk_enumerator(segment_addr, segment, chunk,
							slice_count, start_addr, xz, NULL, 0);
				}
			} else if (zone_is_main &&
					span->xzc_mzone_idx == XZM_MZONE_INDEX_INVALID) {
				// Include free spans and sequestered chunks when enumerating
				// the main zone
				//
				// TODO: could include xzone for sequestered chunks that belong
				// to one
				return span_enumerator(segment_addr, segment, span,
						slice_count, start_addr);
			}

			// Either a free span we don't care about or a Valid chunk that
			// belongs to a different zone: skip, continue iteration
			return KERN_SUCCESS;
		});
	};

	xzm_segment_t last_segment_enumerated = NULL;
	kern_return_t kr = xzm_segment_table_foreach(segment_table,
			XZM_SEGMENT_TABLE_ENTRIES, enumerator, &last_segment_enumerated);
	if (kr) {
		return kr;
	}

	size_t ext_seg_table_size;
	if (os_mul_overflow(main->xzmz_extended_segment_table_entries,
			sizeof(xzm_extended_segment_table_entry_s), &ext_seg_table_size)) {
		xzm_debug_abort("failed to compute extended segment table size");
		return KERN_FAILURE;
	}
	xzm_extended_segment_table_entry_s *ext_segment_table =
			(xzm_extended_segment_table_entry_s *) _xzm_introspect_rebase(
				main_address, main, main->xzmz_total_size,
				main->xzmz_extended_segment_table, ext_seg_table_size);
	if (ext_segment_table) {
		for (size_t i = 0; i < main->xzmz_extended_segment_table_entries; i++) {
			// If leaf table pointer is non-null, map it in and enumerate over
			// it
			if (ext_segment_table[i].xeste_val != 0) {
				// There is (or was) at least one segment in the 64GB span
				// represented by this segment map entry
				xzm_segment_table_entry_s *table;

				vm_address_t table_addr = ((vm_address_t)ext_segment_table[i].xeste_val *
						XZM_SEGMENT_TABLE_ALIGN);

				kern_return_t kr = reader(task, table_addr,
						XZM_SEGMENT_TABLE_SIZE, (void **)&table);
				if (kr) {
					xzm_debug_abort("Failed to map segment table");
					return kr;
				}

				kr = xzm_segment_table_foreach(table, XZM_SEGMENT_TABLE_ENTRIES,
						enumerator, &last_segment_enumerated);
				if (kr) {
					return kr;
				}
			}
		}
	}

	return KERN_SUCCESS;
}

#if CONFIG_XZM_DEFERRED_RECLAIM

static kern_return_t
_xzm_introspect_map_reclaim_buffer(task_t task, memory_reader_t reader,
		vm_address_t metadata_addr, xzm_reclaim_buffer_t *xzm_metadata_out,
		mach_vm_reclaim_ring_t *buffer_out)
{
	xzm_reclaim_buffer_t xzm_buffer = NULL;
	mach_vm_reclaim_ring_t buffer = NULL;
	kern_return_t kr;

	kr = reader(task, metadata_addr,
			sizeof(struct xzm_reclaim_buffer_s), (void **)&xzm_buffer);
	if (kr) {
		xzm_debug_abort_with_reason("failed to map reclaim buffer metadata", kr);
		goto out;
	}

	vm_address_t buffer_addr = (vm_address_t)xzm_buffer->xrb_ringbuffer;
	size_t buffer_size = (xzm_buffer->xrb_len *
		sizeof(struct mach_vm_reclaim_entry_s)) +
		offsetof(struct mach_vm_reclaim_ring_s, entries);
	if (buffer_addr != 0) {
		xzm_debug_assert(buffer_size % vm_page_quanta_size == 0);
		kr = reader(task, buffer_addr, buffer_size, (void **)&buffer);
		if (kr) {
			xzm_debug_abort_with_reason("failed to map reclaim buffer", kr);
			goto out;
		}
	}

out:
	*xzm_metadata_out = xzm_buffer;
	*buffer_out = buffer;
	return kr;
}

#endif // CONFIG_XZM_DEFERRED_RECLAIM

static kern_return_t
_xzm_introspect_map_zone_and_main(task_t task, vm_address_t zone_address,
		memory_reader_t reader, xzm_malloc_zone_t *zone_p_out,
		xzm_main_malloc_zone_t *main_p_out, vm_address_t *main_address_out)
{
	xzm_malloc_zone_t zone = NULL;

	// Map the base structure first to find its size, then map that full size
	kern_return_t kr = reader(task, zone_address, sizeof(*zone),
			(void **)&zone);
	if (kr) {
		xzm_debug_abort("failed to map zone");
		return kr;
	}

	uint64_t zone_size = zone->xzz_total_size;
	if (zone_size < sizeof(*zone)) {
		xzm_debug_abort("inconsistent zone region info");
		return KERN_FAILURE;
	}

	kr = reader(task, zone_address, zone_size, (void **)&zone);
	if (kr) {
		xzm_debug_abort("failed to map full zone");
		return kr;
	}

	xzm_main_malloc_zone_t main = NULL;
	uint64_t main_zone_size;
	vm_address_t main_address = 0;
	if (zone->xzz_main_ref) {
		main_address = (vm_address_t)zone->xzz_main_ref;

		kr = reader(task, main_address, sizeof(*main), (void **)&main);
		if (kr) {
			xzm_debug_abort("failed to map main zone");
			return kr;
		}

		main_zone_size = main->xzmz_total_size;
		if (main_zone_size < sizeof(*main)) {
			xzm_debug_abort("inconsistent main zone info");
			return KERN_FAILURE;
		}

		kr = reader(task, main_address, main_zone_size, (void **)&main);
		if (kr) {
			xzm_debug_abort("failed to map full main zone");
			return kr;
		}
	} else {
		main = (xzm_main_malloc_zone_t)zone;
		if (main->xzmz_total_size != zone_size) {
			xzm_debug_abort("inconsistent main zone size");
			return KERN_FAILURE;
		}

		main_address = zone_address;
		main_zone_size = zone_size;
	}

	if (main_zone_size < main->xzmz_total_size) {
		xzm_debug_abort("inconsistent main region size");
		return KERN_FAILURE;
	}

	xzm_assert(zone);
	xzm_assert(main);
	xzm_assert(main_address);
	*zone_p_out = zone;
	*main_p_out = main;
	*main_address_out = main_address;

	return KERN_SUCCESS;
}

static kern_return_t
xzm_ptr_in_use_enumerator(task_t task, void *context, unsigned type_mask,
		vm_address_t zone_address, memory_reader_t reader,
		vm_range_recorder_t recorder)
{
	xzm_malloc_zone_t zone;
	xzm_main_malloc_zone_t main;
	vm_address_t main_address;
	bool zone_is_main = false;

	reader = reader_or_in_memory_fallback(reader, task);

	bool record_admin = (type_mask & MALLOC_ADMIN_REGION_RANGE_TYPE);
	bool record_ptr_region = (type_mask & MALLOC_PTR_REGION_RANGE_TYPE);
	bool record_ptr_in_use = (type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE);

	kern_return_t kr = _xzm_introspect_map_zone_and_main(task, zone_address,
			reader, &zone, &main, &main_address);
	if (kr) {
		return kr;
	}

	zone_is_main = (zone_address == main_address);

	if (zone_is_main) {
		vm_address_t mfm_addr = (vm_address_t)main->xzmz_mfm_address;
		if (mfm_addr) {
			kr = mfm_introspect.enumerator(task, context, type_mask, mfm_addr,
					reader, recorder);
			if (kr) {
				return kr;
			}
		}
	}

	return _xzm_introspect_enumerate(task, reader, zone_address,
			zone, main_address, main,
			/* include_blocks */record_ptr_in_use,
			^(vm_address_t slab_addr, vm_size_t slab_size, xzm_metapool_id_t mp_id){
		// Metapool slab enumerator
		if (record_admin && zone_is_main) {
			vm_range_t segment_meta_range = {
				.address = slab_addr,
				.size = slab_size,
			};
			recorder(task, context, MALLOC_ADMIN_REGION_RANGE_TYPE,
					&segment_meta_range, 1);
		}
		return KERN_SUCCESS;
	}, ^(vm_address_t segment_addr, xzm_segment_t segment, const char *indent){
		// Segment enumerator
		// Nothing to do, since segment metadata is recorded by the slab
		// enumerator
		return KERN_SUCCESS;
	}, ^(vm_address_t segment_addr, xzm_segment_t segment, xzm_chunk_t chunk,
			xzm_slice_count_t slice_count, vm_address_t start_addr,
			xzm_xzone_t xz, vm_range_t *ranges, size_t count){
		// Chunk enumerator
		xzm_slice_kind_t kind = chunk->xzc_bits.xzcb_kind;
		if (record_admin && kind == XZM_SLICE_KIND_HUGE_CHUNK) {
			// Record the info slices of huge segments against the mzone they
			// belong to
			vm_range_t header_range = {
				.address = segment_addr,
				.size = XZM_METAPOOL_SEGMENT_BLOCK_SIZE,
			};
			recorder(task, context, MALLOC_ADMIN_REGION_RANGE_TYPE,
					&header_range, 1);
		}

		if (!record_ptr_region && !record_ptr_in_use) {
			return KERN_SUCCESS;
		}

		vm_range_t region_range = {
			.address = start_addr,
			.size = slice_count * XZM_SEGMENT_SLICE_SIZE,
		};
		if (_xzm_slice_kind_uses_xzones(kind)) {
			if (record_ptr_region) {
				recorder(task, context, MALLOC_PTR_REGION_RANGE_TYPE,
						&region_range, 1);
			}

			if (record_ptr_in_use) {
				recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, ranges,
						(unsigned)count);
			}
		} else {
			recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE |
					MALLOC_PTR_REGION_RANGE_TYPE, &region_range, 1);
		}

		return KERN_SUCCESS;
	}, !zone_is_main ? NULL : ^(vm_address_t segment_addr,
			xzm_segment_t segment, xzm_chunk_t span,
			xzm_slice_count_t slice_count, vm_address_t start_addr){
		// Main zone span enumerator

		// Record all free spans and chunks with no mzone against the main zone,
		// with the exception of huge chunks that could be in the reclaim buffer
		if (record_ptr_region) {
			bool should_record = true;

#if CONFIG_XZM_DEFERRED_RECLAIM
			// Unfortunately, there's no way for us to reliably tell whether a
			// given huge chunk is in the reclaim buffer, because when marking
			// them free there's a window where we haven't yet stored the
			// reclaim index in xzs_reclaim_id.  So, we err on the side of
			// caution and just never record them.
			if (segment->xzs_kind == XZM_SEGMENT_KIND_HUGE &&
					span->xzc_bits.xzcb_kind == XZM_SLICE_KIND_HUGE_CHUNK) {
				should_record = false;
			}
#endif // CONFIG_XZM_DEFERRED_RECLAIM

			if (should_record) {
				vm_range_t region_range = {
					.address = start_addr,
					.size = slice_count * XZM_SEGMENT_SLICE_SIZE,
				};
				recorder(task, context, MALLOC_PTR_REGION_RANGE_TYPE,
					&region_range, 1);
			}
		}

		return KERN_SUCCESS;
	});
}

#if XZM_DEBUG_ENUMERATOR

struct xzm_debug_recorder_context_s {
	vm_range_recorder_t *orig_recorder;
	void *orig_context;
};

static void
_xzm_debug_range_recorder(task_t task, void *context, unsigned type,
		vm_range_t *ranges, unsigned count)
{
	const char *type_str = "(invalid?)";
	switch (type) {
	case MALLOC_PTR_IN_USE_RANGE_TYPE | MALLOC_PTR_REGION_RANGE_TYPE:
		type_str = "PTR_IN_USE | PTR_REGION";
		break;
	case MALLOC_PTR_IN_USE_RANGE_TYPE:
		type_str = "PTR_IN_USE";
		break;
	case MALLOC_PTR_REGION_RANGE_TYPE:
		type_str = "PTR_REGION";
		break;
	case MALLOC_ADMIN_REGION_RANGE_TYPE:
		type_str = "ADMIN_REGION";
		break;
	default:
		break;
	}

	printf("XZM ENUMERATOR: %s (%x) - %p %u ranges\n", type_str, type, ranges,
			count);
	for (unsigned i = 0; i < count; i++) {
		printf("XZM ENUMERATOR: %p[%u]: %p %llu\n", ranges, i,
				(void *)ranges[i].address, (unsigned long long)ranges[i].size);
	}

	struct xzm_debug_recorder_context_s *ctx = context;
	ctx->orig_recorder(task, ctx->orig_context, type, ranges, count);
}

static kern_return_t
xzm_debug_ptr_in_use_enumerator(task_t task, void *context, unsigned type_mask,
		vm_address_t zone_address, memory_reader_t reader,
		vm_range_recorder_t recorder)
{
	struct xzm_debug_recorder_context_s ctx = {
		.orig_recorder = recorder,
		.orig_context = context,
	};

	return xzm_ptr_in_use_enumerator(task, &ctx, type_mask, zone_address,
			reader, _xzm_debug_range_recorder);

}

#endif // XZM_DEBUG_ENUMERATOR

static void
xzm_print(task_t task, unsigned level, vm_address_t zone_address,
		memory_reader_t reader, print_task_printer_t printer)
{
	xzm_malloc_zone_t zone;
	xzm_main_malloc_zone_t main;
	vm_address_t main_address;
	bool zone_is_main = false;

	kern_return_t kr = _xzm_introspect_map_zone_and_main(task, zone_address,
			reader, &zone, &main, &main_address);
	if (kr) {
		return;
	}

	zone_is_main = (zone_address == main_address);

	printer("Begin xzone malloc JSON:\n");
	printer("{\n");
	printer("\"desc\": \"xzone malloc\", \n");
	printer("\"addr\": \"%p\", \n", zone_address);
	printer("\"segment_size\": %zu, \n", XZM_SEGMENT_SIZE);
	printer("\"slice_size\": %zu, \n", XZM_SEGMENT_SLICE_SIZE);
	printer("\"mzone\": %d, \n", (int)zone->xzz_mzone_idx);
	printer("\"is_main\": %d, \n", zone_is_main);
	printer("\"max_list_config\": %d, \n", (int)zone->xzz_max_list_config);
	printer("\"initial_slot_config\": %d, \n", (int)zone->xzz_initial_slot_config);
	printer("\"slot_initial_threshold\": %u, \n", zone->xzz_slot_initial_threshold);
	printer("\"max_slot_config\": %d, \n", (int)zone->xzz_max_slot_config);

	// TODO: early allocator info

	__block size_t dispositions_count = 0;
	__block int *dispositions = NULL;
	__block vm_address_t dispositions_start_addr = 0;
	kern_return_t (^print_dispositions)(vm_address_t, vm_size_t, const char *);
	print_dispositions = ^kern_return_t(vm_address_t addr, vm_size_t size, const char *indent) {
		kern_return_t kr = KERN_SUCCESS;

		// When operating on a core dump, no pages can be queried
		if (task == TASK_NULL) {
			return kr;
		}

		// If dispositions doesn't cover the full range of this request,
		// (possibly) reallocate and re-query the VM
		vm_address_t request_end = addr + size;
		vm_address_t dispositions_end = dispositions_start_addr +
				(dispositions_count * vm_page_size);
		if ((dispositions_start_addr > addr) ||
				(dispositions_end < request_end)) {
			dispositions_start_addr = addr;

			// This interface is usually used to query the disposition of a
			// full segment, so to reduce the number of calls into the vm, request at least a segment
			size_t request_pages = howmany(size, vm_page_size);
			if (request_pages < (XZM_SEGMENT_SIZE / vm_page_size)) {
				request_pages = XZM_SEGMENT_SIZE / vm_page_size;
			}

			// TODO: mixed page size difficulties
			if (request_pages > dispositions_count) {
				if (dispositions) {
					mach_vm_deallocate(mach_task_self(),
							(mach_vm_address_t)dispositions,
							dispositions_count * sizeof(dispositions[0]));
					dispositions = NULL;
				}

				dispositions_count = request_pages;
				kr = mach_vm_allocate(mach_task_self(),
						(mach_vm_address_t *)&dispositions,
						dispositions_count * sizeof(dispositions[0]),
						VM_FLAGS_ANYWHERE);
				if (kr) {
					xzm_debug_abort("failed to allocate memory for vm stats");
					return kr;
				}
			}

			mach_vm_size_t mvs_page_span = (mach_vm_size_t)request_pages;
			kr = mach_vm_page_range_query(task,
					(mach_vm_address_t)addr, MAX(size, XZM_SEGMENT_SIZE),
					(mach_vm_address_t)dispositions, &mvs_page_span);
			if (kr) {
				xzm_debug_abort("Failed to query vm stats");
				return kr;
			}
		}

		printer("%s    \"dispositions\": \"", indent);

		size_t dirty_count = 0;
		size_t swapped_count = 0;
		size_t disposition_idx =
				(addr - dispositions_start_addr) / vm_page_size;
		for (size_t i = 0; i < (size / vm_page_size); i++) {
			if (disposition_idx >= dispositions_count) {
				xzm_debug_abort("inconsistent slice counts");
				return KERN_FAILURE;
			}

			int disposition = dispositions[disposition_idx];
			if (disposition & VM_PAGE_QUERY_PAGE_DIRTY) {
				dirty_count++;
				printer("d");
			} else if (disposition & VM_PAGE_QUERY_PAGE_PAGED_OUT) {
				swapped_count++;
				printer("s");
			} else {
				printer("c");
			}

			disposition_idx++;
		}

		printer("\", \n"); // dispositions
		printer("%s    \"dirty_count\": %zu, \n", indent, dirty_count);
		printer("%s    \"swapped_count\": %zu, \n", indent, swapped_count);

		return KERN_SUCCESS;
	};

	__block bool first_span = true;
	__block bool print_segment_dispositions = true;
	const xzm_segment_enumerator_t segment_enumerator =
			^(vm_address_t segment_addr, xzm_segment_t segment,
			const char *indent) {
		// Segment enumerator

		if (!first_span) {
			printer(", ");
		}

		printer("%s\"%p\": {\n", indent, (void *)segment_addr);
		printer("%s    \"addr\": \"%p\", \n", indent, (void *)segment_addr);
		xzm_segment_group_id_t sg_id = segment->xzs_segment_group -
				main->xzmz_segment_groups;
		printer("%s    \"segment_group\": \"%s\", \n", indent,
				_xzm_segment_group_id_to_string(sg_id));
		printer("%s    \"body_addr\": \"%p\", \n", indent,
				segment->xzs_segment_body);
		printer("%s    \"used\": %u, \n", indent, segment->xzs_used);
		printer("%s    \"kind\": \"%s\", \n", indent,
				_xzm_segment_kind_to_string(segment->xzs_kind));
#if CONFIG_XZM_DEFERRED_RECLAIM
		if (segment->xzs_reclaim_id == VM_RECLAIM_ID_NULL) {
			printer("%s    \"reclaim_id\": -1, \n", indent);
		} else {
			printer("%s    \"reclaim_id\": %llu, \n", indent,
					segment->xzs_reclaim_id);
		}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		if (print_segment_dispositions) {
			print_dispositions((vm_address_t)segment->xzs_segment_body,
					segment->xzs_slice_count * XZM_SEGMENT_SLICE_SIZE, indent);
		}

		printer("%s    \"slice_count\": %u, \n", indent,
				segment->xzs_slice_count);
		printer("%s    \"slice_entry_count\": %u \n", indent,
				segment->xzs_slice_entry_count);

		printer("%s}\n", indent); // segment
		first_span = false;

		return KERN_SUCCESS;
	};

	if (zone_is_main) {
		printer("\"bucketing_key\": \"%016llx%016llx\", \n",
				main->xzmz_bucketing_keys.xbk_key_data[0],
				main->xzmz_bucketing_keys.xbk_key_data[1]);
		printer("\"guard_config\": {\n");
		printer("    \"guards_enabled\": %d, \n",
				main->xzmz_guard_config.xgpc_enabled);
		printer("    \"data_guards_enabled\": %d, \n",
				main->xzmz_guard_config.xgpc_enabled_for_data);
		printer("    \"tiny_run_size\": %d, \n",
				main->xzmz_guard_config.xgpc_max_run_tiny);
		printer("    \"tiny_guard_density\": %d, \n",
				main->xzmz_guard_config.xgpc_tiny_guard_density);
		printer("    \"small_run_size\": %d, \n",
				main->xzmz_guard_config.xgpc_max_run_small);
		printer("    \"small_guard_density\": %d \n",
				main->xzmz_guard_config.xgpc_small_guard_density);
		printer("}, \n");
		printer("\"chunk_threshold\": %u, \n", main->xzmz_xzone_chunk_threshold);
		printer("\"ptr_bucket_count\": %d, \n", main->xzmz_ptr_bucket_count);
		// guard_config

#if CONFIG_MTE
		printer("\"mte_config\": {\n");
		printer("    \"enabled\": %d, \n",
				(int)main->xzmz_base.xzz_memtag_config.enabled);
		printer("    \"tag_data\": %d, \n",
				(int)main->xzmz_base.xzz_memtag_config.tag_data);
		printer("    \"max_block_size\": %d \n",
				(int)main->xzmz_base.xzz_memtag_config.max_block_size);
		printer("}, \n"); // mte_config
#endif // CONFIG_MTE

		printer("\"defer_tiny\": %s, \n", main->xzmz_defer_tiny ?
				"true" : "false");
		printer("\"defer_small\": %s, \n", main->xzmz_defer_small ?
				"true" : "false");
		printer("\"defer_large\": %s, \n", main->xzmz_defer_large ?
				"true" : "false");
		printer("\"deallocate_segment\": %s, \n", main->xzmz_deallocate_segment ?
				"true" : "false");

		printer("\"use_early_alloc\": %s, \n", main->xzmz_mfm_address ?
				"true" : "false");

		printer("\"batch_size\": %u, \n", main->xzmz_batch_size);

#if CONFIG_XZM_DEFERRED_RECLAIM
		if (main->xzmz_reclaim_buffer != NULL) {
			vm_address_t xzm_buffer_addr = (vm_address_t)main->xzmz_reclaim_buffer;
			xzm_reclaim_buffer_t xzm_reclaim_buffer;
			mach_vm_reclaim_ring_t ringbuffer;
			kr = _xzm_introspect_map_reclaim_buffer(task, reader,
					xzm_buffer_addr, &xzm_reclaim_buffer, &ringbuffer);
			if (kr) {
				xzm_debug_abort("failed to map reclaim buffer");
				return;
			}
			if (ringbuffer != NULL) {
				printer("\"reclaim_buffer\": { \n");
				printer("    \"buffer_len\": %llu, \n",
						ringbuffer->len);
				printer("    \"max_len\": %llu, \n",
						ringbuffer->max_len);
				printer("    \"sampling_period_abs\": %llu, \n", ringbuffer->sampling_period_abs);
				printer("    \"last_sample_abs\": %llu, \n",
						ringbuffer->last_sample_abs);
				printer("    \"reclaimable_bytes\": %llu, \n",
						os_atomic_load(&ringbuffer->reclaimable_bytes,
							relaxed));
				printer("    \"reclaimable_bytes_min\": %llu, \n",
						os_atomic_load(&ringbuffer->reclaimable_bytes_min,
							relaxed));

				printer("    \"head\": %llu, \n",
						os_atomic_load(&ringbuffer->head, relaxed));
				printer("    \"busy\": %llu, \n",
						os_atomic_load(&ringbuffer->busy, relaxed));
				printer("    \"tail\": %llu, \n",
						os_atomic_load(&ringbuffer->tail, relaxed));

				printer("    \"entries\": [ \n");

				for (mach_vm_reclaim_count_t i = 0; i < ringbuffer->len; i++) {
					mach_vm_reclaim_entry_t entry = &ringbuffer->entries[i];
					printer("        { \n");
					printer("            \"id\": %u, \n", i);
					printer("            \"address\": \"%p\", \n", entry->address);
					printer("            \"size\": %u, \n", entry->size);
					// TODO: add string decoder to libsyscall
					printer("            \"behavior\": %u \n", entry->behavior);
					printer("        }");
					if (i < ringbuffer->len - 1) {
						printer(",");
					}
					printer(" \n");
				}
				printer("    ] \n"); // entries
			}
			printer("}, \n"); // reclaim buffer
		}

#endif // CONFIG_XZM_DEFERRED_RECLAIM

		printer("\"allocation_front_count\": %u, \n",
				main->xzmz_allocation_front_count);
		printer("\"range_group_count\": %u, \n", main->xzmz_range_group_count);
		printer("\"range_groups\": {\n");

		size_t range_group_size;
		if (os_mul_overflow(main->xzmz_range_group_count,
						sizeof(struct xzm_range_group_s), &range_group_size)) {
			xzm_debug_abort("failed to compute range group size");
			return;
		}
		struct xzm_range_group_s *mapped_range_groups =
				(struct xzm_range_group_s *)_xzm_introspect_rebase(main_address,
				main, main->xzmz_total_size, main->xzmz_range_groups,
				range_group_size);
		if (!mapped_range_groups) {
			xzm_debug_abort("failed to map range_groups");
			return;
		}

		for (uint8_t i = 0; i < main->xzmz_range_group_count; i++) {
			printer("    ");
			if (i) {
				printer(", ");
			}
			xzm_range_group_t rg = &mapped_range_groups[i];
			printer("\"%d\": {\n", (int)i);
			printer("        \"id\": \"%s\", \n",
					_xzm_range_group_id_to_string(rg->xzrg_id));
			printer("        \"front\": %d, \n", (int)rg->xzrg_front);
			printer("        \"lock\": %u, \n", *(uint32_t *)&rg->xzrg_lock);
			printer("        \"base\": \"%p\", \n", (void *)rg->xzrg_base);
			printer("        \"size\": %zu, \n", rg->xzrg_size);
			printer("        \"skip_addr\": \"%p\", \n",
					(void *)rg->xzrg_skip_addr);
			printer("        \"skip_size\": %zu, \n", rg->xzrg_skip_size);
			printer("        \"next\": \"%p\", \n", (void *)rg->xzrg_next);
			printer("        \"remaining\": %zu, \n", rg->xzrg_remaining);
			printer("        \"direction\": \"%s\"\n",
					rg->xzrg_direction == XZM_FRONT_INCREASING ? "up" : "down");
			printer("    }\n"); // range group
		}

		printer("}, \n"); // range_groups

		printer("\"segment_group_ids_count\": %u, \n",
				main->xzmz_segment_group_ids_count);
		printer("\"segment_group_front_count\": %u, \n",
				main->xzmz_segment_group_front_count);
		printer("\"segment_group_count\": %u, \n",
				main->xzmz_segment_group_count);
		printer("\"segment_groups\": {\n");

		size_t segment_group_size;
		if (os_mul_overflow(main->xzmz_segment_group_count,
				sizeof(struct xzm_segment_group_s), &segment_group_size)) {
			xzm_debug_abort("failed to compute segment group size");
			return;
		}
		struct xzm_segment_group_s *mapped_segment_groups =
				(struct xzm_segment_group_s *)_xzm_introspect_rebase(main_address,
				main, main->xzmz_total_size, main->xzmz_segment_groups,
				segment_group_size);
		if (!mapped_segment_groups) {
			xzm_debug_abort("failed to map segment_groups");
			return;
		}

		for (uint8_t i = 0; i < main->xzmz_segment_group_count; i++) {
			printer("    ");
			if (i) {
				printer(", ");
			}
			xzm_segment_group_t sg = &mapped_segment_groups[i];
			printer("\"%d\": {\n", (int)i);
			printer("        \"id\": \"%s\", \n",
					_xzm_segment_group_id_to_string(sg->xzsg_id));
			printer("        \"front\": %d, \n", (int)sg->xzsg_front);
			printer("        \"range_group\": \"%p\", \n",
					sg->xzsg_range_group);
			printer("        \"segment_cache\": { \n");
			printer("            \"max_count\": %u, \n",
					(unsigned)sg->xzsg_cache.xzsc_max_count);
			printer("            \"count\": %u, \n",
					(unsigned)sg->xzsg_cache.xzsc_count);
			printer("            \"max_entry_slices\": %u, \n",
					(unsigned)sg->xzsg_cache.xzsc_max_entry_slices);
			printer("            \"segments\": { \n");
			if (sg->xzsg_cache.xzsc_count) {
				// Segments in the segment cache will not be present in the segment
				// table, so they must be enumerated here
				vm_address_t segment_addr = (vm_address_t)TAILQ_FIRST(
						&sg->xzsg_cache.xzsc_head);
				while (segment_addr != 0) {
					xzm_segment_t segment;
					kern_return_t kr = reader(task, segment_addr,
							sizeof(struct xzm_segment_s), (void **)&segment);
					if (kr) {
						xzm_debug_abort("Failed to map cached segment");
						return;
					}

					kr = segment_enumerator(segment_addr, segment,
							"                    ");
					if (kr) {
						xzm_debug_abort("Failed to enumerate segment");
						return;
					}
					segment_addr = (vm_address_t)TAILQ_NEXT(segment,
							xzs_cache_entry);
				}
			}
			printer("            } \n"); // segments
			printer("        } \n"); // segment cache
			printer("    }\n"); // segment group
		}

		printer("}, \n"); // segment_groups

		printer("\"xzones\": {\n");

		size_t xzone_size;
		if (os_mul_overflow(main->xzmz_base.xzz_xzone_count,
				sizeof(struct xzm_xzone_s), &xzone_size)) {
			xzm_debug_abort("failed to compute xzone array size");
			return;
		}
		uintptr_t rebased_xzones = _xzm_introspect_rebase(main_address, main,
				main->xzmz_total_size, main->xzmz_base.xzz_xzones, xzone_size);
		struct xzm_xzone_s *mapped_xzones =
				(struct xzm_xzone_s *)rebased_xzones;
		if (!mapped_xzones) {
			xzm_debug_abort("failed to map main xzones");
			return;
		}

		size_t slots_size;
		if (os_mul3_overflow(main->xzmz_base.xzz_xzone_count,
				main->xzmz_base.xzz_slot_count,
				sizeof(struct xzm_xzone_allocation_slot_s), &slots_size)) {
			xzm_debug_abort("failed to compute allocation slots size");
			return;
		}
		uintptr_t rebased_slots = _xzm_introspect_rebase(main_address, main,
				main->xzmz_total_size,
				main->xzmz_base.xzz_xzone_allocation_slots, slots_size);
		struct xzm_xzone_allocation_slot_s *mapped_slots =
				(struct xzm_xzone_allocation_slot_s *)rebased_slots;
		if (!mapped_slots) {
			xzm_debug_abort("failed to map main allocation slots");
			return;
		}

		for (uint8_t xzidx = XZM_XZONE_INDEX_FIRST;
				xzidx < zone->xzz_xzone_count; xzidx++) {
			xzm_xzone_t xz = &mapped_xzones[xzidx];
			printer("    \"%d\": {\n", (int)xzidx);
			printer("        \"early_budget\": %u, \n", xz->xz_early_budget);
			printer("        \"id\": %d, \n", (int)xz->xz_idx);
			printer("        \"bucket\": %d, \n", (int)xz->xz_bucket);
			printer("        \"segment_group_id\": %d, \n",
					xz->xz_segment_group_id);
			printer("        \"front\": %d, \n", xz->xz_front);
			printer("        \"batch_count\": %u, \n",
					xz->xz_block_size <= XZM_TINY_BLOCK_SIZE_MAX ?
					xz->xz_batch_list.xzch_batch_count :
					xz->xz_chunkq_batch_count);
			printer("        \"block_size\": %llu, \n", xz->xz_block_size);
			printer("        \"chunk_count\": %llu, \n", xz->xz_chunk_count);
			printer("        \"chunk_capacity\": %u, \n", xz->xz_chunk_capacity);
			printer("        \"sequestered\": %d,\n", (int)xz->xz_sequestered);
			printer("        \"list_config\": \"%s\",\n",
					_xzm_slot_config_to_string(xz->xz_list_config));
			printer("        \"slot_config\": \"%s\",\n",
					_xzm_slot_config_to_string(xz->xz_slot_config));
			printer("        \"allocation_slots\": [\n");

			for (xzm_allocation_index_t slot_idx = 0;
					slot_idx < zone->xzz_slot_count;
					slot_idx++) {
				xzm_xzone_allocation_slot_t xas = &mapped_slots[
						(slot_idx * zone->xzz_xzone_count) + xzidx];
				printer("            {\n");

				if (xz->xz_block_size <= XZM_TINY_BLOCK_SIZE_MAX ||
						zone->xzz_small_freelist_enabled) {
					printer("                \"atomic_value\": \"0x%llx\",\n",
							xas->xas_atomic.xasa_value);
					printer("                \"xsg_locked\": \"0x%llx\",\n",
							xas->xas_atomic.xasa_gate.xsg_locked);
					printer("                \"xsg_waiters\": \"0x%llx\",\n",
							xas->xas_atomic.xasa_gate.xsg_waiters);
					printer("                \"xsc_ptr\": \"0x%llx\",\n",
							xas->xas_atomic.xasa_chunk.xsc_ptr);

					printer("                \"operations\": %lu,\n",
							xas->xas_counters.xsc_ops);
					printer("                \"contentions\": %lu,\n",
							xas->xas_counters.xsc_contentions);

					printer("                \"slot_config\": \"%s\",\n",
							_xzm_slot_config_to_string(
							xas->xas_counters.xsc_slot_config));
				} else {
					printer("                \"chunk\": \"%p\",\n",
							(void *)xas->xas_chunk);
					printer("                \"allocations\": %lu,\n",
							xas->xas_allocs);
					printer("                \"contentions\": %lu,\n",
							xas->xas_contentions);
				}

				printer("                \"last_chunk_empty_ts\": %llu\n",
						xas->xas_last_chunk_empty_ts);
				printer("            }");
				if (slot_idx < zone->xzz_slot_count - 1) {
					printer(",");
				}
				printer("\n");
			}

			printer("        ]\n"); // allocation slots
			printer("    }"); // xzone
			if (xzidx < zone->xzz_xzone_count - 1) {
				printer(",");
			}
			printer("\n");
		}

		printer("}, \n"); // xzones

#if CONFIG_XZM_THREAD_CACHE
		printer("\"thread_cache_enabled\": %s, \n",
				zone->xzz_thread_cache_enabled ?  "true" : "false");
		printer("\"thread_cache_activation_period\": %lu, \n",
				zone->xzz_thread_cache_xzone_activation_period);
		printer("\"thread_cache_activation_contentions\": %lu, \n",
				zone->xzz_thread_cache_xzone_activation_contentions);
		printer("\"thread_cache_activation_time\": %llu, \n",
				zone->xzz_thread_cache_xzone_activation_time);

		if (zone->xzz_thread_cache_enabled) {
			printer("\"thread_caches\": [ \n");
			__block bool first_thread_cache = true;
			kr = _xzm_introspect_enumerate_thread_caches(task, reader, main,
					^(vm_address_t thread_cache_addr, xzm_thread_cache_t tc){
				printer("    ");
				if (!first_thread_cache) {
					printer(", ");
				} else {
					first_thread_cache = false;
				}
				printer("{\n");
				printer("        \"thread\": \"%p\",\n", (void *)tc->xtc_thread);
				printer("        \"xz_caches\": {\n", (void *)tc->xtc_thread);
				for (uint8_t xzidx = XZM_XZONE_INDEX_FIRST;
						xzidx < zone->xzz_thread_cache_xzone_count; xzidx++) {
					xzm_xzone_thread_cache_t cache = &tc->xtc_xz_caches[xzidx];

					printer("            \"%d\": {\n", (int)xzidx);
					printer("                \"xz_idx\": %d, \n", (int)xzidx);

					uint16_t head = cache->xztc_head;
					if (head == XZM_XZONE_NOT_CACHED) {
						printer("                \"head\": \"NOT_CACHED\", \n");
						printer("                \"timestamp\": \"%llu\", \n",
								cache->xztc_timestamp);
						printer("                \"contentions\": \"%llu\", \n",
								(uint64_t)cache->xztc_contentions);
						printer("                \"allocs\": \"%llu\" \n",
								(uint64_t)cache->xztc_allocs);
					} else if (head == XZM_XZONE_CACHE_EMPTY) {
						printer("                \"head\": \"EMPTY\" \n");
					} else {
						printer("                \"head\": \"0x%llx\", \n",
								(uint64_t)head);
						printer("                \"chunk\": \"%p\", \n",
								cache->xztc_chunk);
						printer("                \"chunk_start\": \"%p\", \n",
								cache->xztc_chunk_start);
						printer("                \"head_seqno\": \"0x%llx\", \n",
								(uint64_t)cache->xztc_head_seqno);
						printer("                \"free_count\": \"0x%llx\", \n",
								(uint64_t)cache->xztc_free_count);
						printer("                \"seqno\": \"0x%llx\" \n",
								(uint64_t)cache->xztc_seqno);
					}


					printer("            }"); // xzone thread cache
					if (xzidx < zone->xzz_thread_cache_xzone_count - 1) {
						printer(",");
					}
					printer("\n");
				}
				printer("        } \n"); // xzone thread caches
				printer("    } \n"); // thread cache

				return KERN_SUCCESS;
			});

			printer("], \n"); // thread_caches
		}
#endif // CONFIG_XZM_THREAD_CACHE
	}

	printer("\"spans\": {\n");

	first_span = true;
	// un-cached segments will have their dispositions enumerated via the
	// spans/chunks they contain
	print_segment_dispositions = false;

	kr = _xzm_introspect_enumerate(task, reader, zone_address,
			zone, main_address, main,
			/* include_blocks */false,
			^(vm_address_t slab_addr, vm_size_t slab_size, xzm_metapool_id_t
					metapool_id) {
		// Metapool slab enumerator

		printer("    ");
		if (!first_span) {
			printer(", ");
		}

		printer("\"%p\": {\n", (void *)slab_addr);
		printer("        \"addr\": \"%p\", \n", (void *)slab_addr);
		printer("        \"kind\": \"%s\", \n",
				_xzm_metapool_id_to_string(metapool_id));
		print_dispositions(slab_addr, slab_size, "    ");
		printer("        \"size\": %u \n",
				slab_size);

		printer("    }\n");
		first_span = false;

		return KERN_SUCCESS;
	},
	segment_enumerator,
	^(vm_address_t segment_addr, xzm_segment_t segment, xzm_chunk_t chunk,
			xzm_slice_count_t slice_count, vm_address_t start_addr,
			xzm_xzone_t xz, vm_range_t *ranges, size_t count){
		// Chunk enumerator

		printer("    ");
		if (!first_span) {
			printer(", ");
		}

		printer("\"%p\": {\n", (void *)start_addr);
		printer("        \"addr\": \"%p\", \n", (void *)start_addr);
		printer("        \"metadata_addr\": \"%p\", \n", (void *)(segment_addr +
				((vm_address_t)chunk - (vm_address_t)segment)));
		printer("        \"mzone\": %d, \n", (int)chunk->xzc_mzone_idx);
		printer("        \"xzone\": %d, \n", (int)chunk->xzc_xzone_idx);
		printer("        \"segment\": \"%p\", \n", (void *)segment_addr);
		printer("        \"segment_group\": %zu, \n",
				segment->xzs_segment_group - main->xzmz_segment_groups);

		xzm_slice_kind_t kind = chunk->xzc_bits.xzcb_kind;
		const char *kind_str = _xzm_slice_kind_to_string(kind);
		printer("        \"kind\": \"%s\", \n", kind_str);
		printer("        \"slice_count\": %u, \n", slice_count);
		printer("        \"block_size\": %u, \n",
				xz ? (unsigned)xz->xz_block_size : 0);
		printer("        \"in_use\": 1, \n");

		xzm_slice_count_t slice_index = (xzm_slice_count_t)
				(chunk - segment->xzs_slices);
		xzm_xzone_slice_metadata_u *metadata =
				&segment->xzs_slice_metadata[slice_index];
		printer("        \"slice_metadata\": \"%p\", \n",
				metadata->xzsm_batch_next);

		kern_return_t kr = print_dispositions(start_addr, slice_count *
				XZM_SEGMENT_SLICE_SIZE, "    ");
		if (kr) {
			return kr;
		}

		if (_xzm_slice_kind_uses_xzones(kind)) {
			printer("        \"bucket\": %u,\n", (unsigned)xz->xz_bucket);
		}

		switch (kind) {
		case XZM_SLICE_KIND_TINY_CHUNK:
		case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
			printer("        \"meta\": \"0x%llx\",\n",
					chunk->xzc_atomic_meta.xca_value);
			printer("        \"xca_alloc_head\": \"0x%llx\",\n",
					chunk->xzc_atomic_meta.xca_alloc_head);
			printer("        \"xca_free_count\": \"0x%llx\",\n",
					chunk->xzc_atomic_meta.xca_free_count);
			printer("        \"xca_alloc_idx\": \"0x%llx\",\n",
					chunk->xzc_atomic_meta.xca_alloc_idx);
			printer("        \"xca_on_partial_list\": \"0x%llx\",\n",
					chunk->xzc_atomic_meta.xca_on_partial_list);
			printer("        \"xca_on_empty_list\": \"0x%llx\",\n",
					chunk->xzc_atomic_meta.xca_on_empty_list);
			printer("        \"xca_walk_locked\": \"0x%llx\",\n",
					chunk->xzc_atomic_meta.xca_walk_locked);
			printer("        \"xca_head_seqno\": \"0x%llx\",\n",
					chunk->xzc_atomic_meta.xca_head_seqno);
			printer("        \"xca_seqno\": \"0x%llx\",\n",
					chunk->xzc_atomic_meta.xca_seqno);
			break;
		case XZM_SLICE_KIND_SMALL_CHUNK:
			printer("        \"free\": \"0x%x\",\n", chunk->xzc_free);
			printer("        \"used\": %u,\n", (unsigned)chunk->xzc_used);
			printer("        \"alloc_idx\": %u,\n",
					(unsigned)chunk->xzc_alloc_idx);
			break;
		default:
			break;
		}

		printer("        \"is_preallocated\": %d,\n",
				(int)chunk->xzc_bits.xzcb_preallocated);
		printer("        \"is_pristine\": %d\n",
				(int)chunk->xzc_bits.xzcb_is_pristine);
		printer("    }\n");
		first_span = false;

		return KERN_SUCCESS;
	}, !zone_is_main ? NULL : ^(vm_address_t segment_addr,
			xzm_segment_t segment, xzm_chunk_t span,
			xzm_slice_count_t slice_count, vm_address_t start_addr){
		// Main zone span enumerator

		// TODO: better sharing with the chunk enumerator
		printer("    ");
		if (!first_span) {
			printer(", ");
		}

		printer("\"%p\": {\n", (void *)start_addr);
		printer("        \"addr\": \"%p\", \n", (void *)start_addr);
		printer("        \"metadata_addr\": \"%p\", \n", (void *)(segment_addr +
				((vm_address_t)span - (vm_address_t)segment)));
		printer("        \"mzone\": %d, \n", (int)span->xzc_mzone_idx);
		printer("        \"xzone\": %d, \n", (int)span->xzc_xzone_idx);
		printer("        \"segment\": \"%p\", \n", (void *)segment_addr);
		printer("        \"segment_group\": %zu, \n",
				segment->xzs_segment_group - main->xzmz_segment_groups);

		xzm_slice_kind_t kind = span->xzc_bits.xzcb_kind;
		const char *kind_str = _xzm_slice_kind_to_string(kind);
		printer("        \"kind\": \"%s\", \n", kind_str);
		printer("        \"slice_count\": %u, \n", slice_count);

		xzm_slice_count_t slice_index = (xzm_slice_count_t)
				(span - segment->xzs_slices);
		xzm_xzone_slice_metadata_u *metadata =
				&segment->xzs_slice_metadata[slice_index];
		printer("        \"slice_metadata\": \"%p\", \n",
				metadata->xzsm_batch_next);

		kern_return_t kr = print_dispositions(start_addr, slice_count *
				XZM_SEGMENT_SLICE_SIZE, "    ");
		if (kr) {
			return kr;
		}

		printer("        \"is_preallocated\": %d,\n",
				(int)span->xzc_bits.xzcb_preallocated);
		printer("        \"in_use\": 0 \n");
		printer("    }\n"); // span
		first_span = false;

		return KERN_SUCCESS;
	});

	if (dispositions) {
		mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)dispositions,
				dispositions_count * sizeof(dispositions[0]));
	}

	if (kr) {
		return;
	}

	printer("}\n"); // spans
	printer("}\n"); // overall
	printer("End xzone malloc JSON\n");
}

static void
xzm_print_task(task_t task, unsigned level, vm_address_t zone_address,
		memory_reader_t reader, print_task_printer_t printer)
{
	xzm_print(task, level, zone_address, reader, printer);
}

static void
xzm_print_self(xzm_malloc_zone_t zone, boolean_t verbose)
{
	xzm_print(mach_task_self(), verbose ? MALLOC_VERBOSE_PRINT_LEVEL : 0,
			(vm_address_t)zone, _malloc_default_reader, malloc_report_simple);
}

static kern_return_t
xzm_statistics(task_t task, vm_address_t zone_address,
		memory_reader_t reader, print_task_printer_t printer,
		malloc_statistics_t *stats)
{
	// It's straightforward to compute blocks_in_use and size_in_use from an
	// enumeration pass.
	//
	// size_allocated, which is the sum of the virtual sizes of all live
	// non-metadata VM reservations, is also fairly easy.
	//
	// max_size_in_use, which is supposed to track the "high water mark of
	// touched memory", is harder:
	// - nano doesn't even try
	// - szone malloc sort of does, but its accounting is wrong in several
	//   ways:
	//     - it doesn't account for madvise, so its calculation for "in-use"
	//       memory in regions is too pessimistic (not everything outside the
	//       pristine parts of a region was necessarily all in use at the same
	//       time)
	//     - regions that only became partially full before being deallocated
	//       are incorrectly assumed to have been fully used
	//     - it doesn't keep track of the high water mark in large, so the
	//       current size of large is taken as the max
	//
	// Keeping a running max of the sizes of live chunks in each segment group
	// seems like it could be reasonable.  We'd also have to somehow deal with
	// sequestered empty chunks, though, since they look "live" to the range
	// group as-is.
	//
	// Nano's policy of not even trying seems best, if we can get away with it,
	// and the lack of complaints from default enablement of nano on macOS seems
	// like strong evidence that we can, so that's what we should go with unless
	// a compelling need to do otherwise arises.
	(void)printer;
	*stats = (malloc_statistics_t){ 0 };

	reader = reader_or_in_memory_fallback(reader, task);

	xzm_malloc_zone_t zone;
	xzm_main_malloc_zone_t main;
	vm_address_t main_address;
	bool zone_is_main = false;

	kern_return_t kr = _xzm_introspect_map_zone_and_main(task, zone_address,
			reader, &zone, &main, &main_address);
	if (kr) {
		return kr;
	}

	zone_is_main = (zone_address == main_address);

	if (zone_is_main) {
		vm_address_t mfm_addr = (vm_address_t)main->xzmz_mfm_address;
		if (mfm_addr) {
			mfm_introspect.task_statistics(task, mfm_addr, reader, stats);

			// We don't know how to report max_size_in_use, so don't confuse things
			// by only including the max_size_in_use from mfm
			stats->max_size_in_use = 0;
		}
	}

	return _xzm_introspect_enumerate(task, reader, zone_address,
			zone, main_address, main, /* include_blocks */ false,
			^(vm_address_t slab_addr, vm_size_t slab_size, xzm_metapool_id_t mp_id){
		// Metapool slab enumerator
		return KERN_SUCCESS;
	}, ^(vm_address_t segment_addr, xzm_segment_t segment, const char *indent){
		// Segment enumerator

		// Nothing interesting for stats at the segment level
		return KERN_SUCCESS;
	}, ^(vm_address_t segment_addr, xzm_segment_t segment, xzm_chunk_t chunk,
			xzm_slice_count_t slice_count, vm_address_t start_addr,
			xzm_xzone_t xz, vm_range_t *ranges, size_t count){
		// Chunk enumerator
		size_t chunk_size = slice_count * XZM_SEGMENT_SLICE_SIZE;
		size_t used = 0;

		xzm_slice_kind_t kind = chunk->xzc_bits.xzcb_kind;
		switch (kind) {
		case XZM_SLICE_KIND_TINY_CHUNK:
		case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:;
			xzm_chunk_atomic_meta_u meta = chunk->xzc_atomic_meta;
			uint32_t capacity = xz->xz_chunk_capacity;
			if (meta.xca_alloc_head != XZM_FREE_MADVISING &&
					meta.xca_alloc_head != XZM_FREE_MADVISED) {
				used = (size_t)(capacity - meta.xca_free_count);
				stats->blocks_in_use += used;
				stats->size_in_use += used * xz->xz_block_size;
			}
			break;
		case XZM_SLICE_KIND_SMALL_CHUNK:
			used = chunk->xzc_used;
			stats->blocks_in_use += used;
			stats->size_in_use += used * xz->xz_block_size;
			break;
		default:
			stats->blocks_in_use++;
			stats->size_in_use += chunk_size;
			break;
		}
		stats->size_allocated += chunk_size;

		return KERN_SUCCESS;
	}, !zone_is_main ? NULL : ^(vm_address_t segment_addr,
			xzm_segment_t segment, xzm_chunk_t span,
			xzm_slice_count_t slice_count, vm_address_t start_addr){
		// Main zone span enumerator

		// Record all free spans and chunks with no mzone against the main zone
		stats->size_allocated += slice_count * XZM_SEGMENT_SLICE_SIZE;

		return KERN_SUCCESS;
	});
}

static void
xzm_statistics_self(xzm_malloc_zone_t zone, malloc_statistics_t *stats)
{
	if (_xzm_malloc_zone_is_main(zone)) {
		mfm_lock();
	}

	xzm_force_lock(zone);
	xzm_statistics(mach_task_self(), (vm_address_t)zone, _malloc_default_reader,
			malloc_report_simple, stats);
	xzm_force_unlock(zone);
	if (_xzm_malloc_zone_is_main(zone)) {
		mfm_unlock();
	}
}

static void
xzm_statistics_task(task_t task, vm_address_t zone_address,
		memory_reader_t reader, malloc_statistics_t *stats)
{
	xzm_statistics(task, zone_address, reader, NULL, stats);
}


const struct malloc_introspection_t xzm_malloc_zone_introspect = {
#if XZM_DEBUG_ENUMERATOR
	.enumerator = 	(void *)xzm_debug_ptr_in_use_enumerator,
#else // XZM_DEBUG_ENUMERATOR
	.enumerator = 	(void *)xzm_ptr_in_use_enumerator,
#endif // XZM_DEBUG_ENUMERATOR
	.print_task = 	(void *)xzm_print_task,
	.good_size =	(void *)xzm_good_size,
	.check = 		(void *)xzm_check,
	.print =		(void *)xzm_print_self,
	.statistics = 	(void *)xzm_statistics_self,
	.task_statistics = (void*)xzm_statistics_task,
	.log = 			(void *)xzm_log,
	.zone_locked =	(void *)xzm_locked,
	.force_lock = 	(void *)xzm_force_lock,
	.force_unlock =	(void *)xzm_force_unlock,
	.reinit_lock = 	(void *)xzm_reinit_lock,
	// discharge checking is a vestigial interface relating to the historical
	// ObjC gc - not to be implemented
	.enable_discharge_checking = NULL,
	.disable_discharge_checking = NULL,
#ifdef __BLOCKS__
	.enumerate_discharged_pointers = NULL,
#else // __BLOCKS__
	.enumerate_unavailable_without_blocks = NULL,
#endif // __BLOCKS__
	.zone_type = MALLOC_ZONE_TYPE_XZONE,
};

#endif // CONFIG_XZONE_MALLOC
