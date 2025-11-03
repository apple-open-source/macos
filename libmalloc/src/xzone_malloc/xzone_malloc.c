/* ----------------------------------------------------------------------------
Copyright (c) 2018-2022, Microsoft Research, Daan Leijen
Copyright © 2025 Apple Inc.
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" in the same directory as this file.
-----------------------------------------------------------------------------*/

#include "../internal.h"

#if CONFIG_XZONE_MALLOC

#if defined(DEBUG) && !MALLOC_TARGET_EXCLAVES
#define xzm_trace(name, ...) MALLOC_TRACE(TRACE_xzone_##name, __VA_ARGS__)
#else
#define xzm_trace(...)
#endif

#pragma mark xzone lookup

// mimalloc: mi_bin
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t
_xzm_bin(size_t size)
{
	if (size == 0) {
		// TODO: fancy unmapped page
		return 0;
	} else if (size <= 128) {
		return howmany(size, XZM_GRANULE) - 1;
	} else {
		xzm_debug_assert(size <= XZM_SMALL_BLOCK_SIZE_MAX);

		// The bin calculation rounds up, so subtract to handle perfect fits
		size--;

		int msb = 63 - __builtin_clzl(size);
		return ((msb << 2) + ((size >> (msb - 2)) & 0x3)) - 20;
	}
}

MALLOC_STATIC_ASSERT(
		XZM_POINTER_BUCKETS_MAX <= 4,
		"The bucketing function supports up to 4 pointer buckets");

MALLOC_STATIC_ASSERT(
		sizeof(xzm_bucketing_keys_t) == 2 * sizeof(uint64_t),
		"Invalid size for struct xzm_bucketing_keys_t");

// This function assigns a type descriptor to a pointer bucket.
// It takes as input the type descriptor itself and a set of keys, which are
// obtained from the executable_boothash provided to the process by the kernel.
// It is both security and performance sensitive.
//
// The function is designed to fulfill three requirements:
//   1. It is stable with respect to identical type hashes, for executions of
//      the same process in the same boot session: this is required to prevent
//      an attacker from brute-forcing the bucketing assignment by repeatedly
//      crashing a given process.
//   2. It distributes the same type hash pseudo-uniformly across buckets, when
//      the execution hash changes: this is required to prevent an attacker from
//      predicting the bucket assignment across processes, or to statically
//      determine the assignment for a given binary across boot sessions.
//   3. It distributes different type hashes pseudo-uniformly across buckets,
//      given a fixed execution hash: it should not be possible for any
//      random pair of type hashes to always be assigned to the same bucket,
//      independently of the execution hash.
//
// The first requirement only holds when the key material is derived from the
// executable_boothash. It does not hold otherwise.
MALLOC_ALWAYS_INLINE MALLOC_INLINE MALLOC_USED
static uint8_t
_xzm_type_choose_ptr_bucket(const xzm_bucketing_keys_t *const keys,
		uint8_t ptr_bucket_count, malloc_type_descriptor_t type_desc)
{
	xzm_debug_assert(ptr_bucket_count <= XZM_POINTER_BUCKETS_MAX);
	uint8_t bucket = 0;

	if (ptr_bucket_count > 1) {
		const uint32_t type_hash = type_desc.hash;
		const uint64_t key_a = keys->xbk_key_data[0];
		const uint64_t key_b = keys->xbk_key_data[1];
		const uint32_t hash = ((key_a * (uint64_t)type_hash) + key_b) >> 32;

		switch (ptr_bucket_count) {
		case 2:
			bucket = hash & 0x1;
			break;
		case 3:
			bucket = hash % 0x3;
			break;
		case 4:
			bucket = hash & 0x3;
			break;
		default:
			__builtin_unreachable();
		}
	}

	return bucket;
}

// mimalloc: mi_page_queue
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_xzone_index_t
_xzm_xzone_lookup(xzm_malloc_zone_t zone, size_t size,
		malloc_type_descriptor_t type_desc)
{
	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
	uint8_t bin = _xzm_bin(size);
	uint8_t bin_bucket_count = main->xzmz_xzone_bin_bucket_counts[bin];
	uint8_t bin_offset = main->xzmz_xzone_bin_offsets[bin];

	uint8_t bucket;
#if XZM_BUCKET_DATA_ONLY || XZM_BUCKET_POINTER_ONLY
	(void)bin_bucket_count;
	bucket = 0;
#else // XZM_BUCKET_DATA_ONLY || XZM_BUCKET_POINTER_ONLY

#if XZM_NARROW_BUCKETING
	if (main->xzmz_narrow_bucketing) {
		if (type_desc.summary.type_kind == MALLOC_TYPE_KIND_OBJC) {
			// - There are pure-data callsites that provide a 0 type hash
			//   because they assume that nothing is done with it.
			// - The ObjC runtime callsite also provides a 0 type hash, for the
			//   same reason.
			//
			// To prevent these callsites from deterministically bucketing
			// together, pick a different arbitrary type hash for ObjC.
			//
			// TODO: update the callsite in the ObjC runtime directly so that
			// this is no longer necessary
			type_desc.hash = 1;
		}
		bucket = _xzm_type_choose_ptr_bucket(&main->xzmz_bucketing_keys,
				bin_bucket_count, type_desc);
		goto bucket_computed;
	}
#endif // XZM_NARROW_BUCKETING

	bool pure_data = malloc_type_descriptor_is_pure_data(type_desc);
	if (pure_data) {
		bucket = XZM_XZONE_BUCKET_DATA;
	} else if (type_desc.summary.type_kind == MALLOC_TYPE_KIND_OBJC) {
		bucket = XZM_XZONE_BUCKET_OBJC;
#if XZM_BUCKET_VISIBILITY
	} else if (type_desc.type_id == MALLOC_TYPE_ID_NONE) {
		bucket = XZM_XZONE_BUCKET_PLAIN;
	} else if (malloc_type_descriptor_is_uninferred(type_desc)) {
		bucket = XZM_XZONE_BUCKET_UNINFERRED;
#endif // XZM_BUCKET_VISIBILITY
	} else {
		bool fallback_hash = (type_desc.type_id == MALLOC_TYPE_ID_NONE);
		if (fallback_hash) {
			type_desc.hash = malloc_entropy[0] >> 32;
		}
		uint8_t ptr_bucket = _xzm_type_choose_ptr_bucket(
				&main->xzmz_bucketing_keys,
				(bin_bucket_count - XZM_XZONE_BUCKET_POINTER_BASE), type_desc);
		bucket = XZM_XZONE_BUCKET_POINTER_BASE + ptr_bucket;
	}

#if XZM_NARROW_BUCKETING
bucket_computed:
#endif // XZM_NARROW_BUCKETING

#endif // XZM_BUCKET_DATA_ONLY || XZM_BUCKET_POINTER_ONLY
	xzm_debug_assert(bucket < bin_bucket_count);

	return (xzm_xzone_index_t)(bin_offset + bucket);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_allocation_index_t
_xzm_get_allocation_index(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_slot_config_t *cur_slot_config, bool is_xas)
{
	xzm_slot_config_t *slot = is_xas ? &xz->xz_slot_config :
			&xz->xz_list_config;
	xzm_slot_config_t slot_config = os_atomic_load(slot, relaxed);
	xzm_debug_assert(slot_config <= zone->xzz_max_slot_config);

	if (cur_slot_config) {
		*cur_slot_config = slot_config;
	}

	switch (slot_config) {
	case XZM_SLOT_CPU:
		return _malloc_cpu_number();
	case XZM_SLOT_CLUSTER:
#if CONFIG_XZM_CLUSTER_AWARE
		return _malloc_cpu_cluster_number();
#else // CONFIG_XZM_CLUSTER_AWARE
		return _malloc_cpu_number() % 2;
#endif // CONFIG_XZM_CLUSTER_AWARE
	case XZM_SLOT_SINGLE:
	default:
		return 0;
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_allocation_index_t
_xzm_get_limit_allocation_index(xzm_slot_config_t slot_config)
{
	switch (slot_config) {
	case XZM_SLOT_CPU:
		return logical_ncpus;
	case XZM_SLOT_CLUSTER:
#if CONFIG_XZM_CLUSTER_AWARE
		return ncpuclusters;
#else // CONFIG_XZM_CLUSTER_AWARE
		return MIN(2, logical_ncpus);
#endif // CONFIG_XZM_CLUSTER_AWARE
	case XZM_SLOT_SINGLE:
	default:
		return 1;
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_chunk_list_t
_xzm_xzone_chunk_list_for_index(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_list_t lists, xzm_allocation_index_t alloc_idx)
{
	xzm_xzone_index_t xz_idx = xz->xz_idx;
	size_t alloc_base_idx = alloc_idx * zone->xzz_xzone_count;
	xzm_debug_assert(alloc_base_idx + xz_idx <
			zone->xzz_slot_count * zone->xzz_xzone_count);
	return &lists[alloc_base_idx + xz_idx];
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_segment_group_t
_xzm_segment_group_for_id_and_front(xzm_malloc_zone_t zone,
		xzm_segment_group_id_t sgid, xzm_front_index_t front, bool huge)
{
	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
#if CONFIG_XZM_CLUSTER_AWARE
	const uint32_t clusterid = _malloc_cpu_cluster_number();
	xzm_debug_assert(clusterid < ncpuclusters);
#else
	const uint32_t clusterid = _malloc_cpu_number() % 2;
	xzm_debug_assert(clusterid < logical_ncpus);
#endif // CONFIG_XZM_CLUSTER_AWARE

	xzm_debug_assert(sgid < main->xzmz_segment_group_ids_count);
	if (sgid < XZM_SEGMENT_GROUP_POINTER_XZONES) {
		xzm_debug_assert(front == XZM_FRONT_INDEX_DEFAULT);
	} else {
		xzm_debug_assert(front < main->xzmz_allocation_front_count);
	}

	uint8_t sg_front_index = sgid + front;

	if (main->xzmz_segment_group_front_count < main->xzmz_segment_group_count) {
		uint8_t sg_index;
		bool use_data_large = false;
#if CONFIG_XZM_CLUSTER_AWARE
		// Route all huge allocations to the same global segment group that
		// has the huge cache enabled
		xzm_debug_assert(main->xzmz_defer_large);
		use_data_large = huge;
#endif // CONFIG_XZM_CLUSTER_AWARE
		if (use_data_large) {
			sg_index = XZM_SEGMENT_GROUP_DATA_LARGE;
		} else {
			sg_index = main->xzmz_segment_group_front_count * clusterid +
					sg_front_index;
		}
		xzm_debug_assert(sg_index < main->xzmz_segment_group_count);
		return &main->xzmz_segment_groups[sg_index];
	} else {
		return &main->xzmz_segment_groups[sg_front_index];
	}
}

MALLOC_NOINLINE
static void
_xzm_fork_lock_wait(xzm_malloc_zone_t zone)
{
	// This lock is taken first during fork, so that anything that needs to be
	// locked during fork that otherwise doesn't have one can be sent here.
	_malloc_lock_lock(&zone->xzz_fork_lock);
	_malloc_lock_unlock(&zone->xzz_fork_lock);
}

MALLOC_NOINLINE MALLOC_PRESERVE_MOST
static void
_xzm_walk_lock_wait(xzm_malloc_zone_t zone)
{
	// We take this lock prior to walking any chunk freelist.  A chunk that is
	// marked as being walked will send any allocating and deallocating threads
	// here.
	_malloc_lock_lock(&zone->xzz_lock);
	_malloc_lock_unlock(&zone->xzz_lock);
}

#pragma mark Large allocation and deallocation

// mimalloc: mi_large_huge_page_alloc
static void * __alloc_size(2)
_xzm_malloc_large_huge(xzm_malloc_zone_t zone, size_t size, size_t alignment,
		malloc_type_descriptor_t type_desc, xzm_malloc_options_t opt)
{
	bool clear = (opt & XZM_MALLOC_CLEAR);
	void *ptr = NULL;

	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);

	// Consider: _mi_os_good_alloc_size()?
	size_t rounded_size = roundup(size, XZM_SEGMENT_SLICE_SIZE);
	xzm_slice_kind_t kind;
	if (rounded_size > XZM_LARGE_BLOCK_SIZE_MAX ||
			alignment > XZM_ALIGNMENT_MAX) {
		kind = XZM_SLICE_KIND_HUGE_CHUNK;
	} else {
		kind = XZM_SLICE_KIND_LARGE_CHUNK;
	}
	xzm_slice_count_t slice_count;
	if (os_convert_overflow(rounded_size / XZM_SEGMENT_SLICE_SIZE,
			&slice_count)) {
		goto out;
	}

	bool use_data_for_large = true;
#if XZM_NARROW_BUCKETING
	if (main->xzmz_narrow_bucketing && !main->xzmz_use_ranges) {
		use_data_for_large = false;
	}
#endif

	xzm_segment_group_id_t sg_id;
	if ((use_data_for_large && malloc_type_descriptor_is_pure_data(type_desc)) ||
			kind == XZM_SLICE_KIND_HUGE_CHUNK ||
			main->xzmz_segment_group_ids_count == XZM_SEGMENT_GROUP_IDS_COUNT_DATA_ONLY) {
		// Use a separate segment group for large data only when using deferred
		// reclamation
		bool use_data_large = main->xzmz_defer_large;

#if CONFIG_MTE
		struct xzm_memtag_config_s *memtag_config = &zone->xzz_memtag_config;
		if (memtag_config->tag_data &&
				memtag_config->max_block_size <= XZM_SMALL_BLOCK_SIZE_MAX) {
			use_data_large = true;
		}
#endif

		sg_id = use_data_large ? XZM_SEGMENT_GROUP_DATA_LARGE :
				XZM_SEGMENT_GROUP_DATA;
	} else {
		sg_id = XZM_SEGMENT_GROUP_POINTER_LARGE;
	}
	xzm_debug_assert(sg_id < main->xzmz_segment_group_ids_count);
	xzm_segment_group_t sg = _xzm_segment_group_for_id_and_front(zone, sg_id,
			XZM_FRONT_INDEX_DEFAULT, kind == XZM_SLICE_KIND_HUGE_CHUNK);

	bool purgeable = (zone->xzz_flags & MALLOC_PURGEABLE);
	xzm_chunk_t chunk = xzm_segment_group_alloc_chunk(sg, kind, NULL,
			slice_count, NULL, alignment, clear, purgeable);
	if (os_unlikely(!chunk)) {
		goto out;
	}

	xzm_debug_assert(!clear || chunk->xzc_bits.xzcb_is_pristine);

	// Set the mzone_idx last to publish this chunk for the enumerator protocol
	chunk->xzc_mzone_idx = zone->xzz_mzone_idx;

	_malloc_lock_lock(&zone->xzz_lock);
	LIST_INSERT_HEAD(&zone->xzz_chunkq_large, chunk, xzc_entry);
	_malloc_lock_unlock(&zone->xzz_lock);

	size_t chunk_size = 0;
	ptr = (uint8_t *)_xzm_chunk_start(zone, chunk, &chunk_size);

#if CONFIG_MTE
	bool memtag_enabled = _xzm_segment_group_memtag_block(sg, chunk_size);
	bool canonical_tag = (opt & XZM_MALLOC_CANONICAL_TAG);
	if (memtag_enabled) {
		if (canonical_tag) {
			ptr = memtag_tag_canonical(ptr, chunk_size);
		} else {
			ptr = memtag_retag(ptr, chunk_size);
		}
	}
#endif


out:
	if (os_unlikely(!ptr)) {
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
	}
	return ptr;
}

// mimalloc: _mi_segment_huge_page_free
MALLOC_NOINLINE
static void
_xzm_free_large_huge(xzm_malloc_zone_t zone, xzm_chunk_t chunk)
{
	_malloc_lock_lock(&zone->xzz_lock);

	// Unset mzone_idx first to unpublish it for the enumerator protocol
	chunk->xzc_mzone_idx = XZM_MZONE_INDEX_INVALID;
	LIST_REMOVE(chunk, xzc_entry);

	_malloc_lock_unlock(&zone->xzz_lock);

	xzm_segment_group_t sg = _xzm_segment_group_for_slice(zone, chunk);
	xzm_segment_group_free_chunk(sg, chunk, zone->xzz_flags & MALLOC_PURGEABLE,
			/* small_madvise_needed */ false);
}

#if CONFIG_MTE

#pragma mark MTE

// FIXME: `zone` parameter of the following functions is unused.

// Initialize the tags for the given chunk.
static void *
_xzm_xzone_chunk_memtag_init(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk)
{
	size_t chunk_size = 0;
	uint8_t *chunk_start = _xzm_chunk_start_ptr(zone, chunk, &chunk_size);
	return memtag_init_chunk(chunk_start, chunk_size, xz->xz_block_size);
}

// Retag a single block.
static xzm_block_t
_xzm_xzone_block_memtag_retag(xzm_malloc_zone_t zone, xzm_block_t block,
		size_t block_size)
{
	return (xzm_block_t)memtag_retag((uint8_t *)block, block_size);
}
#endif // CONFIG_MTE

static inline xzm_malloc_options_t
_xzm_xzone_get_malloc_thread_options(void)
{
#if CONFIG_MTE
	// rdar://140822174
	// Check the flag in the TSD to see if canonical tagging was requested.
	malloc_thread_options_t options = malloc_get_thread_options();
	if (options.ReservedFlag) {
		return XZM_MALLOC_CANONICAL_TAG;
	}
#endif // CONFIG_MTE

	return 0;
}

#pragma mark chunk list

static void
_xzm_chunk_list_fork_lock(xzm_chunk_list_head_t list)
{
	xzm_chunk_list_head_u locked = {
		.xzch_fork_locked = true,
	};

	__assert_only xzm_chunk_list_head_u old_head = {
		.xzch_value = os_atomic_or_orig(&list->xzch_value, locked.xzch_value,
				relaxed),
	};
	xzm_debug_assert(!old_head.xzch_fork_locked);
}

static void
_xzm_chunk_list_fork_unlock(xzm_chunk_list_head_t list)
{
	xzm_chunk_list_head_u locked = {
		.xzch_fork_locked = true,
	};
	uint64_t unlocked = ~locked.xzch_value;

	__assert_only xzm_chunk_list_head_u old_head = {
		.xzch_value = os_atomic_and_orig(&list->xzch_value, unlocked, relaxed),
	};
	xzm_debug_assert(old_head.xzch_fork_locked);
}

static xzm_chunk_t
_xzm_chunk_list_pop(xzm_malloc_zone_t zone, xzm_chunk_list_head_t list,
		xzm_chunk_linkage_t linkage, bool *contended_out)
{
	xzm_chunk_t chunk = NULL;
	const bool is_batch = (linkage == XZM_CHUNK_LINKAGE_BATCH);
	xzm_chunk_list_head_u head = {
		.xzch_value = os_atomic_load(&list->xzch_value, dependency),
	};

	while (true) {
		if (os_unlikely(is_batch ?
				head.xzch_batch_fork_locked : head.xzch_fork_locked)) {
			_xzm_fork_lock_wait(zone);
			head.xzch_value = os_atomic_load(&list->xzch_value, dependency);
			continue;
		}

		chunk = (xzm_chunk_t)head.xzch_ptr;
		if (!chunk) {
			break;
		}

		xzm_chunk_list_head_u new_head;
		xzm_chunk_t next;
		if (!is_batch) {
			next = chunk->xzc_linkages[linkage];
			new_head = (xzm_chunk_list_head_u){
				.xzch_ptr = (uint64_t)next,
				.xzch_gen = head.xzch_gen + 1,
			};
		} else {
			xzm_debug_assert(head.xzch_batch_count);
			next = *_xzm_segment_slice_meta_batch_next(zone, chunk);
			new_head = (xzm_chunk_list_head_u){
				.xzch_batch_ptr = (uint64_t)next,
				.xzch_batch_gen = head.xzch_batch_gen + 1,
				.xzch_batch_count = head.xzch_batch_count - 1,
			};
			xzm_debug_assert(new_head.xzch_batch_count < head.xzch_batch_count);
		}
		xzm_debug_assert(chunk != next);
		bool success = os_atomic_cmpxchgv(&list->xzch_value, head.xzch_value,
				new_head.xzch_value, &head.xzch_value, dependency);
		if (success) {
			if (is_batch) {
				xzm_debug_assert(_xzm_slice_meta_is_batch_pointer(zone,
						(xzm_chunk_t)new_head.xzch_batch_ptr));
#if CONFIG_XZM_DEFERRED_RECLAIM
				*_xzm_slice_meta_reclaim_id(zone, chunk) = VM_RECLAIM_ID_NULL;
#endif // CONFIG_XZM_DEFERRED_RECLAIM
			}
			break;
		} else if (contended_out) {
			*contended_out = true;
		}
	}

	return chunk;
}

static void
_xzm_chunk_list_push(xzm_malloc_zone_t zone, xzm_chunk_list_head_t list,
		xzm_chunk_t chunk, xzm_chunk_linkage_t linkage, bool *contended_out)
{
	xzm_debug_assert(linkage != XZM_CHUNK_LINKAGE_BATCH);

	xzm_chunk_list_head_u head = {
		.xzch_value = os_atomic_load(&list->xzch_value, dependency),
	};

	while (true) {
		if (os_unlikely(head.xzch_fork_locked)) {
			_xzm_fork_lock_wait(zone);
			head.xzch_value = os_atomic_load(&list->xzch_value, dependency);
			continue;
		}

		xzm_chunk_t head_chunk = (xzm_chunk_t)head.xzch_ptr;
		xzm_debug_assert(head_chunk != chunk);
		xzm_chunk_list_head_u new_head = {
			.xzch_ptr = (uint64_t)chunk,
			.xzch_gen = head.xzch_gen + 1,
		};
		chunk->xzc_linkages[linkage] = head_chunk;

		bool success = os_atomic_cmpxchgv(&list->xzch_value, head.xzch_value,
				new_head.xzch_value, &head.xzch_value, release);
		if (success) {
			break;
		} else if (contended_out) {
			*contended_out = true;
		}
	}
}

static xzm_chunk_list_t
_xzm_chunk_list_get(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_slot_config_t *slot_config, xzm_chunk_list_t lists)
{
	xzm_allocation_index_t alloc_idx = _xzm_get_allocation_index(zone, xz,
			slot_config, false);
	return _xzm_xzone_chunk_list_for_index(zone, xz, lists, alloc_idx);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
_xzm_xzone_slot_record_contention(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_xzone_slot_counters_u *xsc, xzm_slot_config_t slot_config,
		bool is_xas, bool contended);

static xzm_chunk_t
_xzm_chunk_list_slot_pop(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_list_t lists)
{
	xzm_slot_config_t slot_config;
	xzm_chunk_list_t xcl = _xzm_chunk_list_get(zone, xz, &slot_config, lists);

	bool contended = false;
	xzm_chunk_t chunk = _xzm_chunk_list_pop(zone, &xcl->xcl_list,
			XZM_CHUNK_LINKAGE_MAIN, &contended);
	if (chunk) {
		_xzm_xzone_slot_record_contention(zone, xz, &xcl->xcl_counters,
				slot_config, false, contended);
	}

	return chunk;
}

static void
_xzm_chunk_list_slot_push(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_list_t lists, xzm_chunk_t chunk)
{
	xzm_slot_config_t slot_config;
	xzm_chunk_list_t xcl = _xzm_chunk_list_get(zone, xz, &slot_config, lists);
	bool contended = false;
	_xzm_chunk_list_push(zone, &xcl->xcl_list, chunk, XZM_CHUNK_LINKAGE_MAIN,
			&contended);
	_xzm_xzone_slot_record_contention(zone, xz, &xcl->xcl_counters, slot_config,
			false, contended);
}

static void
_xzm_chunk_batch_list_fork_lock(xzm_chunk_list_head_t list)
{
	xzm_chunk_list_head_u locked = {
		.xzch_batch_fork_locked = true,
	};

	__assert_only xzm_chunk_list_head_u old_head = {
		.xzch_value = os_atomic_or_orig(&list->xzch_value, locked.xzch_value,
				relaxed),
	};
	xzm_debug_assert(!old_head.xzch_batch_fork_locked);
}

static void
_xzm_chunk_batch_list_fork_unlock(xzm_chunk_list_head_t list)
{
	xzm_chunk_list_head_u locked = {
		.xzch_batch_fork_locked = true,
	};
	uint64_t unlocked = ~locked.xzch_value;

	__assert_only xzm_chunk_list_head_u old_head = {
		.xzch_value = os_atomic_and_orig(&list->xzch_value, unlocked, relaxed),
	};
	xzm_debug_assert(old_head.xzch_batch_fork_locked);
}

static void
_xzm_xzone_madvise_batch(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk);

static void
_xzm_chunk_batch_list_push(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk, size_t max)
{
	xzm_chunk_list_head_t list = &xz->xz_batch_list;
	xzm_chunk_list_head_u head = {
		.xzch_value = os_atomic_load(&list->xzch_value, dependency),
	};

	bool was_full;
	while (true) {
		if (os_unlikely(head.xzch_batch_fork_locked)) {
			_xzm_fork_lock_wait(zone);
			head.xzch_value = os_atomic_load(&list->xzch_value, dependency);
			continue;
		}

		xzm_debug_assert(head.xzch_batch_count <= max);
		was_full = (head.xzch_batch_count >= max);

		xzm_chunk_list_head_u new_head = {
			.xzch_batch_ptr = (uint64_t)chunk,
			.xzch_batch_gen = head.xzch_batch_gen + 1,
			.xzch_batch_count = os_likely(!was_full) ?
					head.xzch_batch_count + 1 : 1,
		};
		xzm_debug_assert(was_full ||
				new_head.xzch_batch_count > head.xzch_batch_count);
		xzm_chunk_t next = os_likely(!was_full) ?
				(xzm_chunk_t)head.xzch_ptr : NULL;
		xzm_debug_assert(chunk != next);
		*_xzm_segment_slice_meta_batch_next(zone, chunk) = next;

		bool success = os_atomic_cmpxchgv(&list->xzch_value, head.xzch_value,
				new_head.xzch_value, &head.xzch_value, release);
		if (success) {
			break;
		}
	}

	// Perform the batch madvise on the old head
	if (os_unlikely(was_full)) {
		_xzm_xzone_madvise_batch(zone, xz, (xzm_chunk_t)head.xzch_batch_ptr);
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_chunk_list_empty(xzm_chunk_list_head_t list)
{
	xzm_chunk_list_head_u head = {
		.xzch_value = os_atomic_load(&list->xzch_value, relaxed),
	};

	return !head.xzch_ptr;
}

#pragma mark Tiny allocation

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void *
_xzm_xzone_malloc_from_freelist_chunk_inline(xzm_malloc_zone_t zone,
		xzm_xzone_t xz, xzm_allocation_index_t alloc_idx,
		xzm_xzone_thread_cache_t cache, xzm_chunk_t chunk, bool small,
		bool walk_wait, bool *corrupt_out, bool *contended_out,
		bool *install_empty_out)
{
#if CONFIG_MTE
	bool memtag_enabled = chunk->xzc_tagged;
#endif

	bool install = !!install_empty_out;
	size_t granule = small ? XZM_SMALL_GRANULE : XZM_GRANULE;

	uint8_t *start = (uint8_t *)_xzm_chunk_start(zone, chunk, NULL);
	if (small) {
		xzm_debug_assert(chunk->xzc_bits.xzcb_kind ==
				XZM_SLICE_KIND_SMALL_FREELIST_CHUNK);
	} else {
		xzm_debug_assert(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_TINY_CHUNK);
	}

	// If we're trying to cache this chunk, it's important for the enumerator
	// that we initialize the cache to point to it before we mark it as
	// installed to a thread
	if (cache) {
		if (install) {
			cache->xztc_chunk = chunk;
			cache->xztc_chunk_start = start;
		} else {
			xzm_debug_assert(cache->xztc_chunk == chunk);
			xzm_debug_assert(cache->xztc_chunk_start == start);
		}
	}

	void *ptr = NULL;
	bool from_free_list = false;
	struct xzm_block_inline_meta_s block_meta;

	uint32_t contentions = 0;

	// Dependency order to observe initialization and zeroing of the block we
	// allocated
	xzm_chunk_atomic_meta_u old_meta = {
		.xca_value = os_atomic_load_wide(
				&chunk->xzc_atomic_meta.xca_value, dependency),
	};
	while (true) {
		if (os_unlikely(old_meta.xca_walk_locked)) {
			if (walk_wait) {
				_xzm_walk_lock_wait(zone);
				old_meta.xca_value = os_atomic_load_wide(
						&chunk->xzc_atomic_meta.xca_value, dependency);
				continue;
			} else {
				return NULL;
			}
		}

		xzm_chunk_atomic_meta_u new_meta = old_meta;

		if (install) {
			// We're considering this chunk as a candidate to install.  The
			// chunk we're looking at should not be installed in any slot, and
			// should be marked as being on the partial list (though in fact
			// we'll have just popped it).
			xzm_debug_assert(old_meta.xca_alloc_idx == XZM_SLOT_INDEX_EMPTY);
			xzm_debug_assert(old_meta.xca_on_partial_list);
			xzm_debug_assert(!old_meta.xca_on_empty_list);

			new_meta.xca_on_partial_list = false;

			// Does the chunk have any blocks?
			if (old_meta.xca_free_count == 0) {
				// No, no blocks to allocate.
				if (old_meta.xca_alloc_head == XZM_FREE_MADVISED) {
					// If the chunk is fully madvised and it was on the partial
					// list, it's the caller's responsibility to move it to the
					// empty list.
					new_meta.xca_on_empty_list = true;
					*install_empty_out = true;
				} else {
					// It shouldn't be possible for us to have gotten a full
					// chunk from the partial list, so the only remaining
					// possibility is an in-progress madvise.
					xzm_debug_assert(
							old_meta.xca_alloc_head == XZM_FREE_MADVISING);
				}

				// We may have initialized our thread cache entry to this chunk
				// on a previous iteration of the loop.  It's important that we
				// invalidate it now, before it can potentially be reused by
				// another thread cache, so that there's no ambiguity to the
				// enumerator about which cache owns the chunk.
				if (cache) {
					cache->xztc_state = XZM_XZONE_CACHE_EMPTY;
				}

				bool success = os_atomic_cmpxchgv(
						&chunk->xzc_atomic_meta.xca_value, old_meta.xca_value,
						new_meta.xca_value, &old_meta.xca_value, dependency);
				if (!success) {
					// The only way this can happen is MADVISING -> MADVISED
					xzm_debug_assert(!(*install_empty_out));
					continue;
				}

				xzm_trace(malloc_install_skip, (uint64_t)chunk,
						old_meta.xca_value_lo, new_meta.xca_value_lo,
						cache ? cache->xztc_freelist_state : 0);
				return NULL;
			} else {
				// Yes, we want to proceed with the allocation attempt and mark
				// the chunk as installed to our slot if successful.
				new_meta.xca_alloc_idx = alloc_idx + 1;

#if CONFIG_TINY_ALLOCATION_SLOT_LOCK
				// In this configuration, we actually can't proceed with the
				// attempt until after we've marked the chunk as installed to
				// our slot, as that's what allows a thread depopulating the
				// chunk to be able to wait until we've exited the critical
				// section.  So, we're going to mark the chunk as installed
				// directly, and then transition to the non-install logic to try
				// to actually allocate a block.
				bool success = os_atomic_cmpxchgv(
						&chunk->xzc_atomic_meta.xca_value, old_meta.xca_value,
						new_meta.xca_value, &old_meta.xca_value, dependency);
				if (!success) {
					continue;
				}

				// Update the chunk state for the next loop iteration
				old_meta = new_meta;

				install = false;
				continue;
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK
			}
		} else {
			// We found this chunk as the one currently installed in our slot,
			// and only want to allocate from it if it still is.
			if (old_meta.xca_alloc_idx != alloc_idx + 1) {
				// Shouldn't be possible when the chunk is installed to the
				// thread cache
				xzm_debug_assert(!cache);

				// No.  Go back to try to get a different one.
				return NULL;
			}

			// Does the chunk have any blocks?
			if (old_meta.xca_free_count == 0) {
				// We should try to uninstall this chunk.
				new_meta.xca_alloc_idx = XZM_SLOT_INDEX_EMPTY;

				// It's marked as installed, so it should be full.
				xzm_debug_assert(old_meta.xca_alloc_head == XZM_FREE_NULL);

				// It shouldn't be on any chunk lists either, and doesn't need
				// to be.  If it eventually becomes unfull, it's the
				// responsibility of that thread to move it back to the partial
				// list.
				xzm_debug_assert(!old_meta.xca_on_partial_list);
				xzm_debug_assert(!old_meta.xca_on_empty_list);

				if (cache) {
					// We need to mark our thread cache entry as invalid before
					// we mark the chunk as uninstalled, so that there's no
					// ambiguity about which cache owns the chunk if we get
					// pre-empted and that chunk gets installed to another cache
					cache->xztc_state = XZM_XZONE_CACHE_EMPTY;
				}

				bool success = os_atomic_cmpxchgv(
						&chunk->xzc_atomic_meta.xca_value, old_meta.xca_value,
						new_meta.xca_value, &old_meta.xca_value, dependency);
				if (!success) {
					continue;
				}

				xzm_trace(malloc_full_skip, (uint64_t)chunk,
						old_meta.xca_value_lo, new_meta.xca_value_lo,
						cache ? cache->xztc_freelist_state : 0);
				return NULL;
			}
		}

		// It looks like we should be able to allocate.
		from_free_list = false;

		if (cache) {
			// Allocating all blocks for this cache

			// We initialize the cache entry to the freelist _before_ we mark
			// the chunk as installed so that the enumerator can reliably walk
			// the local freelist in the cache if it sees the chunk marked as
			// installed.

			new_meta.xca_alloc_head = XZM_FREE_NULL;
			new_meta.xca_free_count = 0;

			cache->xztc_free_count = old_meta.xca_free_count - 1;

			if (old_meta.xca_alloc_head < XZM_FREE_LIMIT) {
				// Pop from the alloc head
				ptr = start + (old_meta.xca_alloc_head * granule);
#if CONFIG_MTE
				if (memtag_enabled) {
					memtag_disable_checking();
					block_meta = *(struct xzm_block_inline_meta_s *)ptr;
					memtag_enable_checking();
				} else {
#endif
					block_meta = *(struct xzm_block_inline_meta_s *)ptr;
#if CONFIG_MTE
				}
#endif

				cache->xztc_head = block_meta.xzb_linkage.xzbl_next_offset;
				cache->xztc_head_seqno = block_meta.xzb_linkage.xzbl_next_seqno;

				from_free_list = true;
			} else {
				xzm_debug_assert(old_meta.xca_alloc_head == XZM_FREE_NULL);
				xzm_debug_assert(old_meta.xca_free_count);

				// Note: it's important that we compute ptr in the loop, before
				// we pop, for the benefit of the memory tools: if we don't,
				// then the block will appear allocated but the tools won't be
				// able to find any references to it.  Getting it into a
				// register first will ensure that there's always a reference.
				size_t capacity = xz->xz_chunk_capacity;
				size_t n_allocated = capacity - old_meta.xca_free_count;
				ptr = start + (n_allocated * xz->xz_block_size);

				cache->xztc_head = XZM_FREE_NULL;
			}
		} else {
			// Allocating one block
			new_meta.xca_free_count--;
			if (old_meta.xca_alloc_head < XZM_FREE_LIMIT) {
				// Pop from the alloc head
				ptr = start + (old_meta.xca_alloc_head * granule);

#if CONFIG_MTE
				if (memtag_enabled) {
					memtag_disable_checking();
					block_meta = *(struct xzm_block_inline_meta_s *)ptr;
					memtag_enable_checking();
				} else {
#endif
					block_meta = *(struct xzm_block_inline_meta_s *)ptr;
#if CONFIG_MTE
				}
#endif

				new_meta.xca_alloc_head = block_meta.xzb_linkage.xzbl_next_offset;
				new_meta.xca_head_seqno = block_meta.xzb_linkage.xzbl_next_seqno;
				from_free_list = true;
			} else {
				xzm_debug_assert(old_meta.xca_free_count);
				xzm_debug_assert(old_meta.xca_alloc_head == XZM_FREE_NULL);

				// Note: it's important that we compute ptr in the loop, before we
				// pop, for the benefit of the memory tools: if we don't, then the
				// block will appear allocated but the tools won't be able to find
				// any references to it.  Getting it into a register first will
				// ensure that there's always a reference.
				size_t capacity = xz->xz_chunk_capacity;
				ptr = start + ((capacity - old_meta.xca_free_count) *
						xz->xz_block_size);
			}
		}

		bool success = os_atomic_cmpxchgv(&chunk->xzc_atomic_meta.xca_value,
				old_meta.xca_value, new_meta.xca_value, &old_meta.xca_value,
				dependency);
		if (!success) {
			// XXX: possible madvise race - record?  Retry?

			// Record the contention
			*contended_out = true;

			contentions++;

			// Try again.
			continue;
		}

		// We got it.
		xzm_trace(malloc_success, (uint64_t)chunk, old_meta.xca_value_lo,
				new_meta.xca_value_lo,
				cache ? cache->xztc_freelist_state : 0);
		break;
	}

#if CONFIG_MTE
	if (memtag_enabled) {
		if (small) {
			// Small blocks are tagged only on alloc
			ptr = memtag_retag(ptr, xz->xz_block_size);
		} else {
			if (from_free_list) {
				uint8_t tag = memtag_extract_tag((uint8_t *)block_meta.xzb_cookie);
				xzm_debug_assert(tag != 0);
				ptr = (void *)memtag_mix_tag(ptr, tag);
			} else {
				ptr = memtag_fixup_ptr(ptr);
			}
		}
	}
#endif

	if (from_free_list) {
		uint64_t expected_cookie = (uint64_t)ptr ^ zone->xzz_freelist_cookie;
#if CONFIG_MTE
		if (small) {
			// Small chunks do not encode the tag in the freelist cookie
			expected_cookie = (uint64_t)memtag_strip_address(
					(uint8_t *)expected_cookie);
		}
#endif
		if (expected_cookie != block_meta.xzb_cookie) {
			*corrupt_out = true;
			goto freelist_done;
		}

		// Check the PAC by adding the seqno back
		union xzm_block_linkage_u linkage = {
			.xzbl_next_offset = block_meta.xzb_linkage.xzbl_next_offset,
			.xzbl_next_seqno = block_meta.xzb_linkage.xzbl_next_seqno,
			.xzbl_seqno = old_meta.xca_head_seqno,
		};

		linkage.xzbl_next_value = (uintptr_t)ptrauth_sign_unauthenticated(
				linkage.xzbl_next, ptrauth_key_process_dependent_data,
				ptrauth_blend_discriminator(ptr,
						ptrauth_string_discriminator("xzb_linkage")));

		// Take the seqno back out of the signed value for the comparison with
		// the stored one
		linkage.xzbl_seqno = 0;

		if (block_meta.xzb_linkage.xzbl_next_value != linkage.xzbl_next_value) {
			*corrupt_out = true;
			goto freelist_done;
		}

freelist_done:
		;
	}

	return ptr;
}

static void *
_xzm_xzone_malloc_from_freelist_chunk(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_allocation_index_t alloc_idx, xzm_xzone_thread_cache_t cache,
		xzm_chunk_t chunk, bool small, bool *contended_out,
		bool *install_empty_out)
{
	const bool walk_wait = true;
	bool corrupt = false;
	void *ptr = _xzm_xzone_malloc_from_freelist_chunk_inline(zone, xz,
			alloc_idx, cache, chunk, small, walk_wait, &corrupt, contended_out,
			install_empty_out);
	if (corrupt) {
		_xzm_corruption_detected(ptr);
	}

	return ptr;
}

static void *
_xzm_xzone_malloc_from_empty_freelist_chunk(xzm_malloc_zone_t zone,
		xzm_xzone_t xz, xzm_allocation_index_t alloc_idx,
		xzm_xzone_thread_cache_t cache, xzm_chunk_t chunk,
		bool is_fresh)
{
	xzm_debug_assert(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_TINY_CHUNK ||
			chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK);
#if CONFIG_MTE
	bool small = chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK;
#endif

	// acquire to make sure the reclaim index is visible
	// TODO make this load relaxed. the address of the reclaim index is derived
	// from the chunk pointer that we popped from the list, and stores to the
	// list are done with a release barrier
	xzm_chunk_atomic_meta_u old_meta = {
		.xca_value = os_atomic_load_wide(
				&chunk->xzc_atomic_meta.xca_value, acquire),
	};

	// We found this on the batch list or the empty list, so it is either
	// enqueued for madvising or has been madvised
	xzm_debug_assert(old_meta.xca_alloc_head == XZM_FREE_MADVISED ||
			old_meta.xca_alloc_head == XZM_FREE_MADVISING);

	// A chunk is unusable if it is both on the partial list and the batch list;
	// e.g., if its insertion raced with our emptying of the partial list
	if (os_unlikely(old_meta.xca_on_partial_list)) {
		xzm_debug_assert(old_meta.xca_alloc_head == XZM_FREE_MADVISING);
		return NULL;
	}


	// We found this chunk on the batch or isolation list.  It is not installed
	// anywhere.
	xzm_debug_assert(!old_meta.xca_on_partial_list);
	xzm_debug_assert(old_meta.xca_alloc_idx == XZM_SLOT_INDEX_EMPTY);

	uint8_t *start = (uint8_t *)_xzm_chunk_start(zone, chunk, NULL);
	void *ptr = (void *)start;
#if CONFIG_MTE
	const bool memtag_enabled = xz->xz_tagged;
	if (memtag_enabled) {
		if (small) {
			// For small, retag blocks lazily instead of for the entire chunk
			// up front
			ptr = memtag_retag(start, xz->xz_block_size);
		} else {
			if (is_fresh) {
				ptr = _xzm_xzone_chunk_memtag_init(zone, xz, chunk);
			} else {
				ptr = memtag_fixup_ptr(ptr);
			}
		}
	}
#else
	(void)is_fresh;
#endif

	// Preserve the seqno across reuses - this is important for anti-replay
	xzm_chunk_atomic_meta_u new_meta = old_meta;

	// We've taken it off the isolation list and won't be putting it back.
	new_meta.xca_on_empty_list = false;

	// In the absence of deferred reclaim, we're guaranteed to be able to bring
	// this chunk back into use.
	new_meta.xca_alloc_idx = alloc_idx + 1;
	if (cache) {
		new_meta.xca_alloc_head = XZM_FREE_NULL;
		new_meta.xca_free_count = 0;

		cache->xztc_chunk = chunk;
		cache->xztc_chunk_start = start;
		cache->xztc_head = XZM_FREE_NULL;
		cache->xztc_free_count = xz->xz_chunk_capacity - 1;
		// preserve xztc_seqno
	} else {
		new_meta.xca_alloc_head = XZM_FREE_NULL;
		new_meta.xca_free_count = xz->xz_chunk_capacity - 1;
	}

	// This needs an acquire barrier because the pointer we're returning is not
	// derived from the metadata, so the dependency ordering we usually rely on
	// doesn't work.
	bool success = os_atomic_cmpxchgv(&chunk->xzc_atomic_meta.xca_value,
			old_meta.xca_value, new_meta.xca_value, &old_meta.xca_value,
			acquire);
	xzm_assert(success);

	xzm_trace(malloc_from_empty, (uint64_t)chunk, old_meta.xca_value_lo,
			new_meta.xca_value_lo, old_meta.xca_seqno);

	return ptr;
}

// Called whenever a tiny or small chunk is received from the span queues. This
// function needs to perform all the initialization needed to allocate a block
// from this chunk, or to free the chunk during zone destruction.
static void
_xzm_xzone_fresh_chunk_init(xzm_xzone_t xz, xzm_chunk_t chunk,
		xzm_slice_kind_t kind)
{
	xzm_debug_assert(chunk->xzc_xzone_idx == xz->xz_idx);

	chunk->xzc_bits.xzcb_preallocated = false;

	switch (kind) {
	case XZM_SLICE_KIND_SMALL_CHUNK:
		_xzm_chunk_reset_free(xz, chunk, true);
		break;
	case XZM_SLICE_KIND_TINY_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
		xzm_debug_assert(xz->xz_block_size <= UINT16_MAX);
		chunk->xzc_freelist_block_size = (uint16_t)xz->xz_block_size;

		xzm_debug_assert(xz->xz_chunk_capacity <= UINT16_MAX);
		chunk->xzc_freelist_chunk_capacity = (uint16_t)xz->xz_chunk_capacity;
#if CONFIG_MTE
		chunk->xzc_tagged = xz->xz_tagged;
#endif
		break;
	default:
		xzm_abort("Unexpected chunk kind");
		break;
	}
}

static void *
_xzm_xzone_malloc_from_fresh_freelist_chunk(xzm_malloc_zone_t zone,
		xzm_xzone_t xz, xzm_allocation_index_t alloc_idx,
		xzm_xzone_thread_cache_t cache, xzm_chunk_t chunk, bool small)
{
	xzm_debug_assert(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_TINY_CHUNK ||
			chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK);

	_xzm_xzone_fresh_chunk_init(xz, chunk, chunk->xzc_bits.xzcb_kind);

	xzm_chunk_atomic_meta_u new_meta;

	uint8_t *start = (uint8_t *)_xzm_chunk_start(zone, chunk, NULL);

	if (cache) {
		new_meta = (xzm_chunk_atomic_meta_u){
			.xca_alloc_head = XZM_FREE_NULL,
			.xca_free_count = 0,
			.xca_alloc_idx = alloc_idx + 1,
		};

		cache->xztc_chunk = chunk;
		cache->xztc_chunk_start = start;
		cache->xztc_head = XZM_FREE_NULL;
		cache->xztc_free_count = xz->xz_chunk_capacity - 1;
		// preserve xztc_seqno
	} else {
		new_meta = (xzm_chunk_atomic_meta_u){
			.xca_alloc_head = XZM_FREE_NULL,
			.xca_free_count = xz->xz_chunk_capacity - 1,
			.xca_alloc_idx = alloc_idx + 1,
		};
	}

	xzm_trace(malloc_from_fresh, (uint64_t)chunk, xz->xz_idx,
			new_meta.xca_value_lo, 0);
	os_atomic_store_wide(&chunk->xzc_atomic_meta.xca_value, new_meta.xca_value,
			relaxed);

	// Enumerator protocol: mzone_idx is initialized last to "publish" the
	// chunk.  Because there are no references to this chunk yet, this store is
	// guaranteed by dependency ordering to be visible to all future threads
	// that observe it when the chunk is store-released to its initial slot.
	chunk->xzc_mzone_idx = xz->xz_mzone_idx;

#if CONFIG_MTE
	if (chunk->xzc_tagged) {
		void *ptr;
		if (!small) {
			ptr = _xzm_xzone_chunk_memtag_init(zone, xz, chunk);
		} else {
			// For small, retag blocks lazily instead of for the entire chunk
			// up front
			ptr = (void *)_xzm_xzone_block_memtag_retag(zone, (xzm_block_t)start,
					xz->xz_block_size);
		}
		return ptr;
	}
#endif
	return (void *)start;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slot_config_t
_xzm_next_slot_config(xzm_slot_config_t slot_config)
{
	switch (slot_config) {
	case XZM_SLOT_SINGLE:
#if CONFIG_XZM_CLUSTER_AWARE
		if (ncpuclusters > 1) {
			return XZM_SLOT_CLUSTER;
		} else {
			return XZM_SLOT_CPU;
		}
#else // CONFIG_XZM_CLUSTER_AWARE
		return XZM_SLOT_CLUSTER;
#endif // CONFIG_XZM_CLUSTER_AWARE
	case XZM_SLOT_CLUSTER:
		return XZM_SLOT_CPU;
	case XZM_SLOT_CPU:
		xzm_debug_abort("Can't upgrade from XZM_SLOT_CPU");
		return XZM_SLOT_CPU;
	default:
		xzm_debug_abort("Invalid xzone slot config");
		return XZM_SLOT_CPU;
	}
}

#if CONFIG_XZM_THREAD_CACHE

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
_xzm_xzone_thread_cache_record_contention(xzm_malloc_zone_t zone,
		xzm_xzone_t xz, xzm_xzone_thread_cache_t cache);

#endif // CONFIG_XZM_THREAD_CACHE

static void
_xzm_xzone_upgrade_freelist_slot_config(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_xzone_slot_counters_u *xsc, xzm_xzone_slot_counters_u counter,
		xzm_slot_config_t next_slot_config, bool is_xas)
{
	if (is_xas) {
		xzm_trace(slot_upgrade, xz->xz_idx, (uint64_t)xsc,
			next_slot_config, counter.xsc_value);
	} else {
		xzm_trace(list_upgrade, xz->xz_idx, (uint64_t)xsc,
			next_slot_config, counter.xsc_value);
	}

	xzm_slot_config_t *slot_config_p = is_xas ? &xz->xz_slot_config :
			&xz->xz_list_config;
	os_atomic_store(slot_config_p, next_slot_config, relaxed);

	// Try to upgrade/reset the counters in all the slots,
	// regardless of whether or not we actually upgraded the xzone
	counter = (xzm_xzone_slot_counters_u){
		.xsc_slot_config = next_slot_config,
	};
	xzm_allocation_index_t limit_idx =
			_xzm_get_limit_allocation_index(next_slot_config);
	for (xzm_allocation_index_t alloc_idx = 0; alloc_idx < limit_idx;
			alloc_idx++) {
		xsc = is_xas ?
				&_xzm_xzone_allocation_slot_for_index(zone, xz,
					alloc_idx)->xas_counters :
				&_xzm_xzone_chunk_list_for_index(zone, xz,
					zone->xzz_partial_lists, alloc_idx)->xcl_counters;
		os_atomic_store(&xsc->xsc_value, counter.xsc_value, relaxed);
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
_xzm_xzone_slot_record_contention(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_xzone_slot_counters_u *xsc, xzm_slot_config_t slot_config,
		bool is_xas, bool contended)
{
#if CONFIG_XZM_THREAD_CACHE
	// XXX Somewhat wasteful to re-check this condition here - consider plumbing
	// thread cache loaded earlier through to here
	if (is_xas &&
			zone->xzz_thread_cache_enabled &&
			xz->xz_block_size <= XZM_THREAD_CACHE_THRESHOLD) {
		if (contended) {
			xzm_thread_cache_t tc = _xzm_get_thread_cache();
			if (os_likely(tc)) {
				xzm_xzone_index_t xz_idx = xz->xz_idx;
				xzm_xzone_thread_cache_t cache = &tc->xtc_xz_caches[xz_idx];
				_xzm_xzone_thread_cache_record_contention(zone, xz, cache);
			}
		}
		return;
	}
#endif // CONFIG_XZM_THREAD_CACHE

	xzm_xzone_slot_counters_u old_counters = {
		.xsc_value = os_atomic_load(&xsc->xsc_value,
				relaxed),
	};

	xzm_slot_config_t max_config = is_xas ? zone->xzz_max_slot_config :
			zone->xzz_max_list_config;
	if (old_counters.xsc_slot_config == max_config) {
		return;
	}

	// Only need to record if:
	// - We detected a contention, or
	// - Some contention has already been detected, so we need to record the
	//   operation to assess the rate
	if (contended || old_counters.xsc_contentions) {
		uint64_t inc = 1; // inc xsc_ops
		if (contended) {
			inc |= (1ull << 32); // inc xsc_contentions
		}

		xzm_xzone_slot_counters_u new_counters = {
			.xsc_value = os_atomic_add(&xsc->xsc_value, inc,
					relaxed),
		};

		// The "current" slot config that we observe in the slot
		// after incrementing the counters may be different than the one we
		// observed originally on the xzone:
		// - It could have a lower slot config if it was just upgraded
		//
		// Regardless, we'll use the slot config that we saw in the counters as
		// the one that dictates what we should do next from here on.
		xzm_slot_config_t current_slot_config = new_counters.xsc_slot_config;

		if (current_slot_config == max_config) {
			return;
		}

		uint32_t period = is_xas ? zone->xzz_slot_upgrade_period :
				zone->xzz_list_upgrade_period;
		uint32_t *thresholds = is_xas ? zone->xzz_slot_upgrade_threshold :
				zone->xzz_list_upgrade_threshold;
		if (os_unlikely(new_counters.xsc_contentions >=
				thresholds[current_slot_config])) {
			if (new_counters.xsc_contentions >
					thresholds[current_slot_config]) {
				// Someone else is upgrading all the slots, do nothing
				return;
			}

			// If we've detected contention above the threshold, try to
			// upgrade the xzone
			xzm_slot_config_t next_slot_config = _xzm_next_slot_config(
					new_counters.xsc_slot_config);
			xzm_debug_assert(new_counters.xsc_slot_config < next_slot_config);

			_xzm_xzone_upgrade_freelist_slot_config(zone, xz, xsc,
					new_counters, next_slot_config, is_xas);
		} else if (os_unlikely(new_counters.xsc_ops >= period)) {
			if (new_counters.xsc_ops > period) {
				// Someone else is resetting the counter, do nothing
				return;
			}
			// If the detection period has elapsed without meeting the
			// threshold, reset the counters for the next period.
			xzm_xzone_slot_counters_u orig_counters = new_counters;

			new_counters = (xzm_xzone_slot_counters_u){
				.xsc_slot_config = current_slot_config,
			};

			os_atomic_rmw_loop(&xsc->xsc_value,
					old_counters.xsc_value, new_counters.xsc_value, relaxed, {
				if (old_counters.xsc_value < orig_counters.xsc_value ||
						old_counters.xsc_slot_config >
								orig_counters.xsc_slot_config) {
					os_atomic_rmw_loop_give_up(break);
				}
			});
		}
	}
}

#if !CONFIG_TINY_ALLOCATION_SLOT_LOCK
// This inlines badly: the compiler unconditionally hoists the mrs out of the
// loop, which is exactly what we don't want since we normally won't need it
MALLOC_NOINLINE
static uint32_t
_malloc_ulock_self_owner_value(void)
{
	mach_port_t self_port = _pthread_mach_thread_self_direct();
	return (uint32_t)self_port >> 2;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_allocation_slot_atomic_meta_u
_xzm_allocation_slot_gate_wait(xzm_xzone_allocation_slot_t xas,
		xzm_allocation_slot_atomic_meta_u slot_meta)
{
	xzm_allocation_slot_atomic_meta_u *xasa = &xas->xas_atomic;

	xzm_debug_assert(slot_meta.xasa_gate.xsg_locked);

	if (!slot_meta.xasa_gate.xsg_waiters) {
		// We're the first waiter, so we first need to mark ourselves as
		// waiting.
		xzm_allocation_slot_atomic_meta_u new_slot_meta = slot_meta;
		new_slot_meta.xasa_gate.xsg_waiters = true;
		bool success = os_atomic_cmpxchgv(&xasa->xasa_value,
				slot_meta.xasa_value, new_slot_meta.xasa_value,
				&slot_meta.xasa_value, dependency);
		if (!success) {
			return slot_meta;
		}
		slot_meta.xasa_gate.xsg_waiters = true;
	}

	uint32_t wait_op = UL_UNFAIR_LOCK | ULF_NO_ERRNO |
			ULF_WAIT_ADAPTIVE_SPIN | ULF_WAIT_WORKQ_DATA_CONTENTION;
	int rc = __ulock_wait(wait_op, &xasa->xasa_ulock,
			slot_meta.xasa_ulock, 0);
	if (os_unlikely(rc < 0)) {
		switch (-rc) {
		case EINTR:
		case EFAULT:
			break;
		default:
			xzm_abort_with_reason("ulock_wait failure", -rc);
		}
	} else {
		// We should only wake as part of a wake-all broadcast, but
		// there's no way to assert that.  rc indicates the number of
		// waiters still in the ulock, but we can't assert anything
		// useful about that.
	}

	slot_meta = (xzm_allocation_slot_atomic_meta_u){
		.xasa_value = os_atomic_load(&xasa->xasa_value, dependency),
	};
	return slot_meta;
}

static void
_xzm_xzone_allocation_slot_fork_lock(xzm_xzone_allocation_slot_t xas)
{
	xzm_allocation_slot_atomic_meta_u *xasa = &xas->xas_atomic;

	xzm_allocation_slot_atomic_meta_u slot_meta = {
		.xasa_value = os_atomic_load(&xasa->xasa_value, dependency),
	};
	while (true) {
		if (slot_meta.xasa_gate.xsg_locked) {
			slot_meta = _xzm_allocation_slot_gate_wait(xas, slot_meta);
			continue;
		}

		xzm_allocation_slot_atomic_meta_u new_slot_meta = slot_meta;
		new_slot_meta.xasa_chunk.xsc_fork_locked = true;

		bool success = os_atomic_cmpxchgv(&xasa->xasa_value,
				slot_meta.xasa_value, new_slot_meta.xasa_value,
				&slot_meta.xasa_value, relaxed);
		if (!success) {
			continue;
		}

		break;
	}
}

static void
_xzm_xzone_allocation_slot_fork_unlock(xzm_xzone_allocation_slot_t xas)
{
	xzm_allocation_slot_atomic_meta_u *xasa = &xas->xas_atomic;

	xzm_allocation_slot_atomic_meta_u slot_meta = {
		.xasa_value = os_atomic_load(&xasa->xasa_value, dependency),
	};

	xzm_debug_assert(slot_meta.xasa_chunk.xsc_fork_locked);

	xzm_allocation_slot_atomic_meta_u new_slot_meta = slot_meta;
	new_slot_meta.xasa_chunk.xsc_fork_locked = false;

	uint64_t prev_slot_value = os_atomic_xchg(&xasa->xasa_value,
			new_slot_meta.xasa_value, release);
	xzm_assert(prev_slot_value == slot_meta.xasa_value);
}
#endif // !CONFIG_TINY_ALLOCATION_SLOT_LOCK

static xzm_chunk_t
_xzm_xzone_allocate_chunk_from_isolation(xzm_main_malloc_zone_t main,
		xzm_xzone_t xz)
{
	xzm_debug_assert(xz->xz_sequestered);
	xzm_isolation_zone_t iz = &main->xzmz_isolation_zones[xz->xz_idx];

	xzm_chunk_t chunk = NULL;

	// Don't bother trying to take the iz lock if it doesn't look like
	// there's anything in there
	//
	// XXX TODO: this should be a proper atomic relaxed load
	if (LIST_FIRST(&iz->xziz_chunkq)) {
#if CONFIG_XZM_DEFERRED_RECLAIM
		LIST_HEAD(, xzm_slice_s) busy_list = LIST_HEAD_INITIALIZER(busy_list);
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		_malloc_lock_lock(&iz->xziz_lock);
		chunk = LIST_FIRST(&iz->xziz_chunkq);
		while (chunk) {
			xzm_debug_assert(_xzm_chunk_is_empty(&main->xzmz_base, xz, chunk));
			LIST_REMOVE(chunk, xzc_entry);
#if CONFIG_XZM_DEFERRED_RECLAIM
			// Chunks stay in the reclaim buffer when removed from the
			// isolation zone, and must be removed from the buffer
			if ((xz->xz_block_size > XZM_TINY_BLOCK_SIZE_MAX &&
					main->xzmz_defer_small) ||
				((xz->xz_block_size <= XZM_TINY_BLOCK_SIZE_MAX &&
					main->xzmz_defer_tiny))) {
				if (!xzm_chunk_mark_used(&main->xzmz_base, chunk, NULL)) {
					// this chunk is busy being reclaimed by the kernel
					LIST_INSERT_HEAD(&busy_list, chunk, xzc_entry);
					chunk = LIST_FIRST(&iz->xziz_chunkq);
					continue;
				}
			}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
			// chunk is already initialized, other than the mzone idx
			chunk->xzc_mzone_idx = xz->xz_mzone_idx;
			break;
		}
#if CONFIG_XZM_DEFERRED_RECLAIM
		if (!LIST_EMPTY(&busy_list)) {
			// place any busy chunks back on the sequester list
			xzm_chunk_t busy_chunk, tmp_chunk;
			LIST_FOREACH_SAFE(busy_chunk, &busy_list, xzc_entry,
					tmp_chunk) {
				LIST_REMOVE(busy_chunk, xzc_entry);
				LIST_INSERT_HEAD(&iz->xziz_chunkq, busy_chunk, xzc_entry);
			}
		}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		_malloc_lock_unlock(&iz->xziz_lock);
	}


	return chunk;
}

static void *
_xzm_xzone_find_and_malloc_from_freelist_chunk(xzm_malloc_zone_t zone,
		xzm_xzone_t xz, xzm_allocation_index_t alloc_idx,
		xzm_xzone_thread_cache_t cache, xzm_chunk_t *chunk_out,
		bool *contended_out)
{
	void *ptr = NULL;
	xzm_chunk_t chunk = NULL;
	bool small = (xz->xz_block_size > XZM_TINY_BLOCK_SIZE_MAX);

	// The first place to try to get a new chunk from is the partial list.
	while ((chunk = _xzm_chunk_list_slot_pop(zone, xz,
			zone->xzz_partial_lists))) {
		xzm_debug_assert(chunk->xzc_atomic_meta.xca_on_partial_list &&
				!chunk->xzc_atomic_meta.xca_on_empty_list);
		bool empty = false;
		ptr = _xzm_xzone_malloc_from_freelist_chunk(zone, xz, alloc_idx, cache,
				chunk, small, contended_out, &empty);
		if (ptr) {
			// Great, we allocated from this chunk and can install it.
			goto done;
		} else if (empty) {
			xzm_debug_assert(!chunk->xzc_atomic_meta.xca_on_partial_list &&
					chunk->xzc_atomic_meta.xca_on_empty_list);
			// It's our responsibility to move this chunk to the empty list.
			_xzm_chunk_list_push(zone, &xz->xz_empty_list, chunk,
					XZM_CHUNK_LINKAGE_MAIN, NULL);
		}
	}

	xzm_debug_assert(!chunk);

	// Try the batch list. Some chunks may not be usable, so we need to build a
	// list of the busy ones that can be reinserted. Since some of these chunks
	// may still be on the partial list, we must reuse the batch linkage instead
	// of the inline linkage (c.f. empty list busy chunks below).
	xzm_chunk_t busy_chunk = NULL;
	while ((chunk = _xzm_chunk_list_pop(zone, &xz->xz_batch_list,
			XZM_CHUNK_LINKAGE_BATCH, NULL))) {
		xzm_debug_assert(!chunk->xzc_atomic_meta.xca_on_empty_list);
		ptr = _xzm_xzone_malloc_from_empty_freelist_chunk(zone, xz, alloc_idx,
				cache, chunk, false);
		if (ptr) {
			break;
		}

		*_xzm_segment_slice_meta_batch_next(zone, chunk) = busy_chunk;
		busy_chunk = chunk;
	}

	// Push unusable chunks back onto the batch list
	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
	while (busy_chunk) {
		xzm_debug_assert(!busy_chunk->xzc_atomic_meta.xca_on_empty_list);
		xzm_chunk_t next = *_xzm_segment_slice_meta_batch_next(zone, busy_chunk);
		_xzm_chunk_batch_list_push(zone, xz, busy_chunk, main->xzmz_batch_size);
		busy_chunk = next;
	}

	if (ptr) {
		// Great, we allocated from this chunk and can install it.
		goto done;
	}

#if CONFIG_XZM_DEFERRED_RECLAIM
	SLIST_HEAD(, xzm_slice_s) busy_list = SLIST_HEAD_INITIALIZER(busy_list);
#endif
	// Next, try the empty list. Some chunks may not be usable, so we need to
	// build a list of the busy ones that can be reinserted. Note that this list
	// is intrusive, and reuses the same linkage as that used by the partial and
	// empty lists.
	while ((chunk = _xzm_chunk_list_pop(zone, &xz->xz_empty_list,
			XZM_CHUNK_LINKAGE_MAIN, NULL))) {
		xzm_debug_assert(chunk->xzc_atomic_meta.xca_on_empty_list);

		bool was_reclaimed = true;
#if CONFIG_XZM_DEFERRED_RECLAIM
		// This relies on dependency ordering from the pop of the chunk
		// list to obtain the correct reclaim buffer index
		if (((main->xzmz_defer_tiny && !small) ||
				(main->xzmz_defer_small && small)) &&
				!xzm_chunk_mark_used(zone, chunk, &was_reclaimed)) {
			// this chunk is busy so we can't use it
			goto empty_busy;
		}
#endif // CONFIG_XZM_DEFERRED_RECLAIM

		ptr = _xzm_xzone_malloc_from_empty_freelist_chunk(zone, xz, alloc_idx,
				cache, chunk, was_reclaimed);
		if (ptr) {
			break;
		}

#if CONFIG_XZM_DEFERRED_RECLAIM
empty_busy:
		SLIST_INSERT_HEAD(&busy_list, chunk, xzc_slist_entry);
#else // CONFIG_XZM_DEFERRED_RECLAIM
		xzm_debug_abort("_xzm_xzone_malloc_from_empty_freelist_chunk failed");
#endif // CONFIG_XZM_DEFERRED_RECLAIM
	}

#if CONFIG_XZM_DEFERRED_RECLAIM
	// Push unusable chunks back onto the empty list
	while ((busy_chunk = SLIST_FIRST(&busy_list))) {
		xzm_debug_assert(busy_chunk->xzc_atomic_meta.xca_on_empty_list);
		SLIST_REMOVE_HEAD(&busy_list, xzc_slist_entry);
		_xzm_chunk_list_push(zone, &xz->xz_empty_list, busy_chunk,
				XZM_CHUNK_LINKAGE_MAIN, NULL);
	}
#endif // CONFIG_XZM_DEFERRED_RECLAIM

	if (ptr) {
		// Great, we allocated from this chunk and can install it.
		goto done;
	}

	// try the global isolation list
	if (xz->xz_sequestered) {
		xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
		chunk = _xzm_xzone_allocate_chunk_from_isolation(main, xz);
		if (chunk) {
			xzm_debug_assert(_xzm_chunk_is_empty(zone, xz, chunk));
			ptr = _xzm_xzone_malloc_from_empty_freelist_chunk(zone, xz, alloc_idx,
					cache, chunk, true);
			xzm_debug_assert(ptr);
			goto push_to_all_list_and_unlock;
		}
	}

	bool zero_on_free = (xz->xz_block_size <= XZM_ZERO_ON_FREE_THRESHOLD);
	if (!(chunk = _xzm_chunk_list_pop(zone, &xz->xz_preallocated_list,
				XZM_CHUNK_LINKAGE_MAIN, NULL))) {
		// Failed to get a chunk from the preallocated list, need to go to the
		// segment group to get a fresh chunk.
		bool clear_chunk = zero_on_free;
		bool purgeable = false; // TINY chunks can't be purgeable
		xzm_preallocate_list_s preallocated;
		SLIST_INIT(&preallocated);
		xzm_preallocate_list_s *list = &preallocated;
		xzm_segment_group_t sg = _xzm_segment_group_for_id_and_front(zone,
				xz->xz_segment_group_id, xz->xz_front, false);

		xzm_slice_kind_t kind = XZM_SLICE_KIND_TINY_CHUNK;
		xzm_slice_count_t slice_count = 1;
		if (small) {
			kind = XZM_SLICE_KIND_SMALL_FREELIST_CHUNK;
			slice_count =
					XZM_SMALL_FREELIST_CHUNK_SIZE / XZM_SEGMENT_SLICE_SIZE;
		}

		chunk = xzm_segment_group_alloc_chunk(sg, kind,
				&xz->xz_guard_config, slice_count, list, 0, clear_chunk,
				purgeable);
		if (!chunk) {
			// Not able to get anything from the segment group, so give up
			// entirely.
			xzm_debug_assert(!list || !SLIST_FIRST(list));
			goto done;
		} else {
			chunk->xzc_xzone_idx = xz->xz_idx;
			xzm_chunk_t c, t;
			SLIST_FOREACH_SAFE(c, list, xzc_slist_entry, t) {
				SLIST_REMOVE_HEAD(list, xzc_slist_entry);
				c->xzc_xzone_idx = xz->xz_idx;
				c->xzc_bits.xzcb_preallocated = true;
				_xzm_chunk_list_push(zone, &xz->xz_preallocated_list, c,
						XZM_CHUNK_LINKAGE_MAIN, NULL);
			}

			if (xz->xz_slot_config < zone->xzz_initial_slot_config &&
					xz->xz_block_size < zone->xzz_slot_initial_threshold) {
				xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
				uint64_t count = os_atomic_inc(&xz->xz_chunk_count, relaxed);
				if (count > main->xzmz_xzone_chunk_threshold) {
					// Upgrade/reset the counters in all the slots
					xzm_xzone_slot_counters_u counter = {
						.xsc_slot_config = xz->xz_slot_config,
					};
					_xzm_xzone_upgrade_freelist_slot_config(zone, xz, NULL,
							counter, zone->xzz_initial_slot_config, true);
				}
			}
		}
	} else {
		// We got a chunk from the preallocated list, which is not guaranteed to
		// be zero'd
		if (zero_on_free && !chunk->xzc_bits.xzcb_is_pristine) {
			size_t chunk_size = 0;
			void *ptr = _xzm_chunk_start_ptr(zone, chunk, &chunk_size);
			xzm_debug_assert(ptr);
			xzm_debug_assert(chunk_size == XZM_SEGMENT_SLICE_SIZE);
			bzero(ptr, chunk_size);
		}
	}

	ptr = _xzm_xzone_malloc_from_fresh_freelist_chunk(zone, xz, alloc_idx,
			cache, chunk, small);
	xzm_debug_assert(ptr);

push_to_all_list_and_unlock:
	// Put the chunk on the "all" list for the xzone so that it can be found
	// during fork
	_xzm_chunk_list_push(zone, &xz->xz_all_list, chunk, XZM_CHUNK_LINKAGE_ALL,
			NULL);

done:
	xzm_debug_assert((ptr && chunk) || (!ptr && !chunk));

	*chunk_out = chunk;
	return ptr;
}

MALLOC_NOINLINE
static void *
_xzm_xzone_malloc_freelist_outlined(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_allocation_index_t alloc_idx, xzm_xzone_allocation_slot_t xas,
		void *corrupt_block, xzm_malloc_options_t opt)
{
	if (os_unlikely(corrupt_block)) {
		_xzm_corruption_detected(corrupt_block);
	}

	bool clear = (opt & XZM_MALLOC_CLEAR);
	bool zero_on_free = (xz->xz_block_size <= XZM_ZERO_ON_FREE_THRESHOLD);
#if !CONFIG_TINY_ALLOCATION_SLOT_LOCK
	bool small = (xz->xz_block_size > XZM_TINY_BLOCK_SIZE_MAX);
#endif
#if CONFIG_MTE
	const bool memtag_enabled = xz->xz_tagged;
	bool needs_canonical_tagging = (opt & XZM_MALLOC_CANONICAL_TAG);
#endif

#if CONFIG_TINY_ALLOCATION_SLOT_LOCK
#ifdef DEBUG
	_malloc_lock_assert_owner(&xas->xas_lock);
#endif // DEBUG
#else // CONFIG_TINY_ALLOCATION_SLOT_LOCK
	uint32_t self_owner_value = 0;
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK

	xzm_allocation_slot_atomic_meta_u *xasa = &xas->xas_atomic;

	xzm_allocation_slot_atomic_meta_u slot_meta = {
		.xasa_value = os_atomic_load(&xasa->xasa_value, dependency),
	};

	void *ptr = NULL;
	xzm_chunk_t chunk = NULL;
	bool contended = false;

#if !CONFIG_TINY_ALLOCATION_SLOT_LOCK
	while (true) {
		if (slot_meta.xasa_gate.xsg_locked) {
			// No chunk is currently installed, and another thread is in the
			// middle of fetching and installing one.  Wait for them and then
			// try again.
			slot_meta = _xzm_allocation_slot_gate_wait(xas, slot_meta);
			continue;
		} else if (os_unlikely(slot_meta.xasa_chunk.xsc_fork_locked)) {
			// We can't acquire the gate because a fork is in progress that we
			// need to wait out.
			_xzm_fork_lock_wait(zone);

			slot_meta = (xzm_allocation_slot_atomic_meta_u){
				.xasa_value = os_atomic_load(&xasa->xasa_value, dependency),
			};
			continue;
		}

		// The gate isn't locked, so the next question is whether a chunk is
		// installed.
		chunk = (xzm_chunk_t)slot_meta.xasa_chunk.xsc_ptr;
		if (chunk) {
			// A chunk is installed, so let's try to allocate from it.
			ptr = _xzm_xzone_malloc_from_freelist_chunk(zone, xz, alloc_idx,
					NULL, chunk, small, &contended,
					/* install_empty_out */ NULL);
			if (ptr) {
				// Success!
				goto done;
			}
		}

		if (!self_owner_value) {
			self_owner_value = _malloc_ulock_self_owner_value();
		}

		// Either we had no chunk reference, or the one we had couldn't be
		// allocated from.  Let's try to become an allocator for our slot to go
		// get a new one.
		xzm_allocation_slot_atomic_meta_u new_slot_meta = {
			.xasa_gate = {
				.xsg_locked = true,
				.xsg_owner = self_owner_value,
				.xsg_gen = slot_meta.xasa_gate.xsg_gen,
			},
		};
		bool success = os_atomic_cmpxchgv(&xasa->xasa_value,
				slot_meta.xasa_value, new_slot_meta.xasa_value,
				&slot_meta.xasa_value, dependency);
		if (!success) {
			continue;
		}

		break;
	}

	xzm_debug_assert(self_owner_value);
#else // CONFIG_TINY_ALLOCATION_SLOT_LOCK
	xzm_debug_assert(!slot_meta.xasa_gate.xsg_locked);
#if CONFIG_MTE
	chunk = (xzm_chunk_t)slot_meta.xasa_chunk.xsc_ptr;
	if (chunk && (xz->xz_tagged && (opt & XZM_MALLOC_CANONICAL_TAG))) {
		// If we're serving a canonically-tagged request, we won't have actually
		// attempted to allocate from the chunk yet, so we need to do that first
		ptr = _xzm_xzone_malloc_from_freelist_chunk(zone, xz, alloc_idx,
				NULL, chunk, /* small */ false, &contended,
				/* install_empty_out */ NULL);
		if (ptr) {
			// Success!
			goto unlock;
		}
	}
#endif
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK

	// It's now up to us to get a usable chunk and install it.

	xzm_debug_assert(!ptr);

	ptr = _xzm_xzone_find_and_malloc_from_freelist_chunk(zone, xz, alloc_idx,
			NULL, &chunk, &contended);

	// Unlock the gate and publish our new chunk (or lack thereof)
	xzm_allocation_slot_atomic_meta_u new_slot_meta = {
		.xasa_chunk = {
			.xsc_ptr = (uint64_t)chunk,
			.xsc_gen = slot_meta.xasa_gate.xsg_gen + 1,
		},
	};

	uint64_t prev_slot_value = os_atomic_xchg(&xasa->xasa_value,
			new_slot_meta.xasa_value, release);
	xzm_allocation_slot_atomic_meta_u prev_slot_meta = {
		.xasa_value = prev_slot_value,
	};
#if CONFIG_TINY_ALLOCATION_SLOT_LOCK
	(void)prev_slot_meta;
	xzm_debug_assert(prev_slot_meta.xasa_value == slot_meta.xasa_value);
#else // CONFIG_TINY_ALLOCATION_SLOT_LOCK
	xzm_debug_assert(prev_slot_meta.xasa_gate.xsg_locked);
	xzm_debug_assert(prev_slot_meta.xasa_gate.xsg_owner == self_owner_value);
	xzm_debug_assert(prev_slot_meta.xasa_gate.xsg_unused == 0);
	xzm_debug_assert(prev_slot_meta.xasa_gate.xsg_gen ==
			slot_meta.xasa_gate.xsg_gen);

	if (prev_slot_meta.xasa_gate.xsg_waiters) {
		uint32_t wake_op = UL_UNFAIR_LOCK | ULF_WAKE_ALL | ULF_NO_ERRNO;
		int rc = __ulock_wake(wake_op, &xasa->xasa_ulock, 0);
		if (rc && rc != -ENOENT) {
			xzm_abort_with_reason("ulock_wake failure", -rc);
		}
	}
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK

#if CONFIG_TINY_ALLOCATION_SLOT_LOCK
#if CONFIG_MTE
unlock:
#endif
	_malloc_lock_unlock(&xas->xas_lock);
#else // CONFIG_TINY_ALLOCATION_SLOT_LOCK
done:
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK

	// Now that we've unlocked the gate, we can safely touch the chunk to
	// perform any necessary zeroing to the block we allocated.  It's very
	// important that we not do this inside the gate because taking a fault in
	// there makes it much more likely for other threads to contend and get
	// stuck waiting for us.
	if (ptr) {
		// The true contention information we'd want to record in the locking
		// case would be from the trylock rather than the list pop, but we
		// didn't forward that information so there's nothing to do here
#if !CONFIG_TINY_ALLOCATION_SLOT_LOCK
		xzm_slot_config_t slot_config = os_atomic_load(&xz->xz_slot_config,
				relaxed);
		_xzm_xzone_slot_record_contention(zone, xz, &xas->xas_counters,
				slot_config, true, contended);
#endif // !CONFIG_TINY_ALLOCATION_SLOT_LOCK

#if CONFIG_MTE
		if (memtag_enabled) {
			if (needs_canonical_tagging) {
				ptr = memtag_tag_canonical(ptr, xz->xz_block_size);
			} else {
				xzm_debug_assert(ptr == memtag_fixup_ptr(ptr));
			}
		}
#endif

		xzm_block_t block = ptr;
		*block = (struct xzm_block_inline_meta_s){ 0 };

		if (clear && !zero_on_free) {
			bzero(ptr, xz->xz_block_size);
		}
	} else {
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
	}
	return ptr;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void *
_xzm_xzone_malloc_freelist(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_malloc_options_t opt, const bool small)
{
	bool clear = (opt & XZM_MALLOC_CLEAR);

	xzm_slot_config_t slot_config;
	xzm_allocation_index_t alloc_idx = _xzm_get_allocation_index(zone, xz,
			&slot_config, true);
	xzm_xzone_allocation_slot_t xas =
			_xzm_xzone_allocation_slot_for_index(zone, xz, alloc_idx);

	bool contended = false;
#if CONFIG_TINY_ALLOCATION_SLOT_LOCK
	bool contended_dummy = false;
	if (!_malloc_lock_trylock(&xas->xas_lock)) {
		contended = true;
		_malloc_lock_lock(&xas->xas_lock);
	}
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK

	xzm_allocation_slot_atomic_meta_u *xasa = &xas->xas_atomic;

	xzm_allocation_slot_atomic_meta_u slot_meta = {
		.xasa_value = os_atomic_load(&xasa->xasa_value, dependency),
	};

	void *ptr = NULL;
#if CONFIG_MTE
	const bool fast_path =
			!slot_meta.xasa_gate.xsg_locked && slot_meta.xasa_chunk.xsc_ptr &&
			!(xz->xz_tagged && (opt & XZM_MALLOC_CANONICAL_TAG));
#else
	const bool fast_path =
			!slot_meta.xasa_gate.xsg_locked && slot_meta.xasa_chunk.xsc_ptr;
#endif
	// Fast path: the slot is not locked and a chunk is installed
	if (fast_path) {
		xzm_chunk_t chunk = (xzm_chunk_t)slot_meta.xasa_chunk.xsc_ptr;

		// Try to allocate from the installed chunk
		const bool walk_wait = false;
		bool corrupt = false;
		ptr = _xzm_xzone_malloc_from_freelist_chunk_inline(zone, xz, alloc_idx,
				NULL, chunk, small, walk_wait, &corrupt,
#if CONFIG_TINY_ALLOCATION_SLOT_LOCK
				&contended_dummy,
#else // CONFIG_TINY_ALLOCATION_SLOT_LOCK
				&contended,
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK
				/* install_empty_out */ NULL);
		if (ptr && !corrupt) {
			// Success!

#if CONFIG_TINY_ALLOCATION_SLOT_LOCK
			_malloc_lock_unlock(&xas->xas_lock);
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK

			_xzm_xzone_slot_record_contention(zone, xz, &xas->xas_counters,
					slot_config, true, contended);

			xzm_block_t block = ptr;
			*block = (struct xzm_block_inline_meta_s){ 0 };

			if (clear && xz->xz_block_size > XZM_ZERO_ON_FREE_THRESHOLD) {
				return memset(ptr, 0, xz->xz_block_size);
			}

			return ptr;
		}
	}

	return _xzm_xzone_malloc_freelist_outlined(zone, xz, alloc_idx, xas, ptr,
			opt);
}

MALLOC_NOINLINE
static void *
_xzm_xzone_malloc_tiny(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_malloc_options_t opt)
{
	return _xzm_xzone_malloc_freelist(zone, xz, opt, false);
}

MALLOC_NOINLINE
static void *
_xzm_xzone_malloc_small_freelist(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_malloc_options_t opt)
{
	return _xzm_xzone_malloc_freelist(zone, xz, opt, true);
}

#pragma mark Tiny deallocation

static void
_xzm_xzone_freelist_chunks_mark_empty(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t *chunks, size_t chunk_count)
{
	for (size_t i = 0; i < chunk_count; ++i) {
		xzm_chunk_t chunk = chunks[i];
		xzm_chunk_atomic_meta_u old_meta = {
			.xca_value = os_atomic_load_wide(
					&chunk->xzc_atomic_meta.xca_value, relaxed),
		};
		while (true) {
			xzm_chunk_atomic_meta_u new_meta = old_meta;

			xzm_debug_assert(old_meta.xca_alloc_head == XZM_FREE_MADVISING);
			new_meta.xca_alloc_head = XZM_FREE_MADVISED;
			if (!old_meta.xca_on_partial_list) {
				new_meta.xca_on_empty_list = true;
			}
			// release to publish the reclaim index if we stored one
			// TODO make this store relaxed. the reclaim index is no longer
			// set in this function
			bool success = os_atomic_cmpxchgv(&chunk->xzc_atomic_meta.xca_value,
					old_meta.xca_value, new_meta.xca_value, &old_meta.xca_value,
					release);
			if (!success) {
				continue;
			}

			xzm_trace(free_madvise, (uint64_t)chunk, old_meta.xca_value_lo,
					new_meta.xca_value_lo, old_meta.xca_seqno);
			break;
		}

		// because the partial list is atomically singly-linked, an empty chunk
		// could remain there until it is popped off the head, so we cannot put
		// it onto the empty list until after that happens
		if (!old_meta.xca_on_partial_list) {
			_xzm_chunk_list_push(zone, &xz->xz_empty_list, chunk,
					XZM_CHUNK_LINKAGE_MAIN, NULL);
		}
	}
}

static void
_xzm_xzone_madvise_freelist_chunk(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk)
{
#if CONFIG_XZM_DEFERRED_RECLAIM
	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
	if ((chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_TINY_CHUNK &&
			main->xzmz_defer_tiny) ||
		(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK &&
			main->xzmz_defer_small)) {
		xzm_chunk_mark_free(zone, chunk);
	} else
#endif // CONFIG_XZM_DEFERRED_RECLAIM
	{
		xzm_segment_group_t sg = _xzm_segment_group_for_slice(zone, chunk);
		xzm_segment_group_segment_madvise_chunk(sg, chunk);
	}

	_xzm_xzone_freelist_chunks_mark_empty(zone, xz, &chunk, 1);
}

static void
_xzm_xzone_small_chunks_mark_empty(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t *chunk, size_t chunk_count);

static void
_xzm_xzone_madvise_batch(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk)
{
	const xzm_slice_kind_t kind = chunk->xzc_bits.xzcb_kind;
	// We need to store the local list of entries in the batch. Otherwise,
	// the list pointers could be replaced by the deferred reclaim id, and
	// attempting to update metadata while under the reclaim buffer lock could
	// deadlock against a concurrent allocator that holds the isolation zone
	// lock and wants the reclaim buffer lock.
	xzm_chunk_t batch_list[1u << XZM_BATCH_SIZE_BITS];
	unsigned batch_list_size = 0;
#if CONFIG_XZM_DEFERRED_RECLAIM
	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
	xzm_reclaim_buffer_t buffer = main->xzmz_reclaim_buffer;
	const bool deferred_reclaim = (kind == XZM_SLICE_KIND_TINY_CHUNK) ?
			main->xzmz_defer_tiny : main->xzmz_defer_small;
	bool should_update_kernel_accounting = false;

	if (deferred_reclaim) {
		_malloc_lock_lock(&buffer->xrb_lock);
	}
#endif // CONFIG_XZM_DEFERRED_RECLAIM

	// Walk the batch, make a local copy, and perform the reclaim
	while (chunk) {
#if CONFIG_XZM_DEFERRED_RECLAIM
		xzm_debug_assert(batch_list_size < main->xzmz_batch_size &&
				batch_list_size < (1u << XZM_BATCH_SIZE_BITS));
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		batch_list[batch_list_size++] = chunk;

		xzm_debug_assert(kind == chunk->xzc_bits.xzcb_kind);
		// Fetch the pointer for the next chunk first, because madvising it
		// may replace the linkage with the deferred reclaim id
		xzm_chunk_t next = *_xzm_segment_slice_meta_batch_next(zone, chunk);
		xzm_debug_assert(_xzm_slice_meta_is_batch_pointer(zone, next));

#if CONFIG_XZM_DEFERRED_RECLAIM
		if (deferred_reclaim) {
			size_t size;
			uint8_t *addr = _xzm_chunk_start_ptr(zone, chunk, &size);
			uint64_t *reclaim_id = _xzm_slice_meta_reclaim_id(zone,	chunk);

			bool should_update_kernel_chunk = false;
			// This relies on release ordering from the push of the batch
			// list to obtain the correct reclaim buffer index
			*reclaim_id = xzm_reclaim_mark_free_locked(buffer, addr, size, true,
					&should_update_kernel_chunk);
			should_update_kernel_accounting |= should_update_kernel_chunk;
		} else
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		{
			xzm_segment_group_t sg = _xzm_segment_group_for_slice(zone,
					chunk);
			xzm_segment_group_segment_madvise_chunk(sg, chunk);
		}

		chunk = next;
	}

#if CONFIG_XZM_DEFERRED_RECLAIM
	if (deferred_reclaim) {
		_malloc_lock_unlock(&buffer->xrb_lock);

		if (should_update_kernel_accounting) {
			mach_vm_reclaim_update_kernel_accounting(buffer->xrb_ringbuffer);
		}
	}
#endif // CONFIG_XZM_DEFERRED_RECLAIM

	if (kind == XZM_SLICE_KIND_TINY_CHUNK ||
			kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK) {
		_xzm_xzone_freelist_chunks_mark_empty(zone, xz, batch_list,
				batch_list_size);
	} else if (kind == XZM_SLICE_KIND_SMALL_CHUNK) {
		_xzm_xzone_small_chunks_mark_empty(zone, xz, batch_list, batch_list_size);
	} else {
		xzm_abort_with_reason("Unexpected chunk kind", kind);
	}
}

MALLOC_NOINLINE
static void
_xzm_free_abort(void *ptr)
{
	xzm_client_abort_with_reason(
			"free to empty or invalid chunk detected (likely double-free)",
			ptr);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
_xzm_xzone_free_freelist_inline(xzm_malloc_zone_t zone,
		xzm_xzone_index_t xz_idx, xzm_chunk_t chunk, xzm_block_t block,
		uint16_t block_ref, size_t block_size, size_t chunk_capacity)
{
	xzm_xzone_t xz = NULL;
	union xzm_block_linkage_u linkage = { 0 };

	uint64_t now = 0;
	uint64_t last_empty_ts = 0;
	bool should_madvise = true;
	bool madvise_considered = false;

	uint32_t contentions = 0;

	xzm_xzone_allocation_slot_t xas = NULL;

	xzm_chunk_atomic_meta_u old_meta = {
		.xca_value = os_atomic_load_wide(
				&chunk->xzc_atomic_meta.xca_value, dependency),
	};

	xzm_chunk_atomic_meta_u new_meta;

	bool push_to_partial = false;
	while (true) {
		if (os_unlikely(old_meta.xca_walk_locked)) {
			_xzm_walk_lock_wait(zone);
			old_meta.xca_value = os_atomic_load_wide(
					&chunk->xzc_atomic_meta.xca_value, dependency);
			continue;
		}

		new_meta = old_meta;
		new_meta.xca_seqno++;

		xzm_debug_assert(!push_to_partial);

		if (old_meta.xca_free_count + 1 == chunk_capacity) {
			// We're freeing the last block.

			// Evaluate the madvise heuristic the first time through.
			if (!madvise_considered) {
				xz = &zone->xzz_xzones[xz_idx];
				if (old_meta.xca_alloc_idx &&
						old_meta.xca_alloc_idx != XZM_SLOT_INDEX_THREAD_INSTALLED) {
					xzm_chunk_list_t partial_list = _xzm_chunk_list_get(zone,
							xz, NULL, zone->xzz_partial_lists);
					bool last_chunk = _xzm_chunk_list_empty(&partial_list->xcl_list);
					if (last_chunk) {
						now = mach_absolute_time();
						xas =  _xzm_xzone_allocation_slot_for_index(zone, xz,
								old_meta.xca_alloc_idx - 1);
						last_empty_ts = os_atomic_load(
								&xas->xas_last_chunk_empty_ts, relaxed);
						if (now - last_empty_ts < zone->xzz_tiny_thrash_threshold) {
							should_madvise = false;
						}
					}
				}
				madvise_considered = true;
			}

			if (should_madvise &&
					old_meta.xca_alloc_idx != XZM_SLOT_INDEX_THREAD_INSTALLED) {
				new_meta.xca_alloc_head = XZM_FREE_MADVISING;
				new_meta.xca_free_count = 0;
				new_meta.xca_alloc_idx = XZM_SLOT_INDEX_EMPTY;
				goto do_cmpxchg;
			}
		} else if (old_meta.xca_free_count == 0) {
			if (os_unlikely(old_meta.xca_alloc_head != XZM_FREE_NULL)) {
				return _xzm_free_abort((void *)block);
			}

			if (old_meta.xca_alloc_idx == XZM_SLOT_INDEX_EMPTY) {
				// This chunk is not currently installed anywhere.  We
				// should move it to the partial list so it can be
				// considered for future allocations.
				xzm_debug_assert(!old_meta.xca_on_partial_list &&
						!old_meta.xca_on_empty_list);
				new_meta.xca_on_partial_list = true;
				push_to_partial = true;

				xz = &zone->xzz_xzones[xz_idx];
			} else {
				xzm_debug_assert(!old_meta.xca_on_partial_list);
			}
		}

		uint32_t seqno = (uint32_t)old_meta.xca_seqno & XZM_SEQNO_COUNTER_MASK;

		// Link the new block to the current head of the free list.
		linkage = (union xzm_block_linkage_u){
			.xzbl_next_offset = old_meta.xca_alloc_head,
			.xzbl_next_seqno = old_meta.xca_head_seqno,
			.xzbl_seqno = seqno,
		};
		linkage.xzbl_next_value = (uintptr_t)ptrauth_sign_unauthenticated(
				linkage.xzbl_next, ptrauth_key_process_dependent_data,
				ptrauth_blend_discriminator(block,
						ptrauth_string_discriminator("xzb_linkage")));
		linkage.xzbl_seqno = 0;

		os_atomic_store(&block->xzb_linkage.xzbl_next_value,
				linkage.xzbl_next_value, relaxed);

		// Install the new block as the head.
		new_meta.xca_alloc_head = block_ref;
		new_meta.xca_free_count++;
		new_meta.xca_head_seqno = seqno;

do_cmpxchg:;
		// Release ordering to publish our zeroing and initialization of the
		// block - pairs with the dependency order loads/cmpxchgs on allocation
		bool success = os_atomic_cmpxchgv(&chunk->xzc_atomic_meta.xca_value,
				old_meta.xca_value, new_meta.xca_value, &old_meta.xca_value,
				release);
		if (!success) {
			// Try again.
			push_to_partial = false;
			contentions++;
			continue;
		}

		xzm_trace(free, (uint64_t)chunk, old_meta.xca_value_lo,
				new_meta.xca_value_lo,
				(uint32_t)old_meta.xca_seqno | ((uint64_t)contentions) << 32);
		break;
	}

	// If we checked the time while evaluating the madvise heuristic, update it
	// in the allocation slot.
	if (now) {
		xzm_debug_assert(xas);
		os_atomic_store(&xas->xas_last_chunk_empty_ts, now, relaxed);
	}

	if (new_meta.xca_alloc_head == XZM_FREE_MADVISING) {
		xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
		xzm_debug_assert(xz);

#if CONFIG_TINY_ALLOCATION_SLOT_LOCK
		// We need to ensure that there are no threads in the critical section
		// of the allocation slot that we uninstalled the chunk from
		if (old_meta.xca_alloc_idx != XZM_SLOT_INDEX_EMPTY) {
			xas = _xzm_xzone_allocation_slot_for_index(zone, xz,
					old_meta.xca_alloc_idx - 1);
			_malloc_lock_lock(&xas->xas_lock);
			_malloc_lock_unlock(&xas->xas_lock);
		}
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK

		if (!main->xzmz_batch_size) {
			// Batching is disabled, do the madvise we committed to
			_xzm_xzone_madvise_freelist_chunk(zone, xz, chunk);
		} else {
			// Batching is enabled, enqueue the chunk. Note that the batch list
			// is not exclusive, the chunk could also still be on the partial
			// list as well
			_xzm_chunk_batch_list_push(zone, xz, chunk,
					main->xzmz_batch_size);
		}
	} else if (push_to_partial) {
		xzm_debug_assert(xz);
		_xzm_chunk_list_slot_push(zone, xz, zone->xzz_partial_lists, chunk);
	}
}

MALLOC_NOINLINE
static void
_xzm_xzone_free_freelist(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk, xzm_block_t block)
{
	size_t block_size = xz->xz_block_size;
	if (block_size <= XZM_ZERO_ON_FREE_THRESHOLD) {
		bzero(block, block_size);
	}

	void *ptr = block;
#if CONFIG_MTE
	ptr = memtag_strip_address((uint8_t *)ptr);
#endif

#if CONFIG_MTE
	if (xz->xz_tagged && block_size <= XZM_TINY_BLOCK_SIZE_MAX) {
		// Only tiny chunks are tagged on free
		block = _xzm_xzone_block_memtag_retag(zone, block, block_size);
	}
#endif

	uint64_t free_cookie = zone->xzz_freelist_cookie ^ (uint64_t)block;
#if CONFIG_MTE
	if (block_size > XZM_TINY_BLOCK_SIZE_MAX) {
		// Small blocks do not encode the tag in the freelist cookie
		free_cookie = (uint64_t)memtag_strip_address((uint8_t *)free_cookie);
	}
#endif
	os_atomic_store(&block->xzb_cookie, free_cookie, relaxed);

	uint16_t block_ref;
	if (xz->xz_block_size > XZM_TINY_BLOCK_SIZE_MAX) {
		size_t block_offset = _xzm_chunk_block_offset(zone, chunk, block);
		block_ref = block_offset / XZM_SMALL_GRANULE;
	} else {
		size_t block_offset = (uintptr_t)ptr & XZM_SEGMENT_SLICE_MASK;
		block_ref = block_offset / XZM_GRANULE;
	}

	_xzm_xzone_free_freelist_inline(zone, xz->xz_idx, chunk, block, block_ref,
			block_size, xz->xz_chunk_capacity);
}

#if CONFIG_XZM_THREAD_CACHE

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
_xzm_xzone_thread_cache_free_tiny(xzm_malloc_zone_t zone,
		xzm_xzone_thread_cache_t cache, xzm_block_t block, uint16_t block_ref)
{
	uint16_t seqno = cache->xztc_seqno & XZM_SEQNO_COUNTER_MASK;
	seqno |= XZM_SEQNO_THREAD_LOCAL;

	union xzm_block_linkage_u linkage = {
		.xzbl_next_offset = cache->xztc_head,
		.xzbl_next_seqno = cache->xztc_head_seqno,
		.xzbl_seqno = seqno,
	};
	linkage.xzbl_next_value = (uintptr_t)ptrauth_sign_unauthenticated(
			linkage.xzbl_next, ptrauth_key_process_dependent_data,
			ptrauth_blend_discriminator(block,
					ptrauth_string_discriminator("xzb_linkage")));
	linkage.xzbl_seqno = 0;

	os_atomic_store(&block->xzb_linkage.xzbl_next_value,
			linkage.xzbl_next_value, relaxed);

#ifdef DEBUG
	uint64_t orig_state = cache->xztc_freelist_state;
#endif

	xzm_xzone_thread_cache_atomic_meta_u tc_meta = {
		.xztcam_head = block_ref,
		.xztcam_free_count = cache->xztc_free_count + 1,
	};
	os_atomic_store(&cache->xztc_atomic_meta.xztcam_value,
			tc_meta.xztcam_value, relaxed);
	cache->xztc_head_seqno = seqno;
	cache->xztc_seqno++;

#ifdef DEBUG
	uint64_t new_state = cache->xztc_freelist_state;
#endif

	xzm_trace(thread_cache_free, (uint64_t)cache->xztc_chunk, orig_state,
			new_state, 0);
}

static void
_xzm_xzone_thread_cache_detach_link_to_remote(xzm_malloc_zone_t zone,
		xzm_xzone_thread_cache_t cache, xzm_chunk_atomic_meta_u old_meta)
{
	xzm_chunk_t chunk = cache->xztc_chunk;

	uint8_t *start = cache->xztc_chunk_start;
	uint64_t block_granules = chunk->xzc_freelist_block_size / XZM_GRANULE;
	uint64_t max_offset = (chunk->xzc_freelist_chunk_capacity - 1) * block_granules;

	uint64_t zone_freelist_cookie = zone->xzz_freelist_cookie;

	xzm_debug_assert(old_meta.xca_alloc_head < XZM_FREE_LIMIT);
	xzm_debug_assert(old_meta.xca_free_count);

	xzm_block_t cur_block = NULL;
	uint64_t cur_block_offset = old_meta.xca_alloc_head;
	uint64_t cur_seqno = old_meta.xca_head_seqno;
	size_t freelist_count = 1;
	while (true) {
		cur_block = (xzm_block_t)(
				start + (cur_block_offset * XZM_GRANULE));

#if CONFIG_MTE
		if (chunk->xzc_tagged) {
			cur_block = (xzm_block_t)memtag_fixup_ptr((void *)cur_block);
		}
#endif
		uint64_t expected_cookie = zone_freelist_cookie ^ (uintptr_t)cur_block;

		struct xzm_block_inline_meta_s block_meta = *cur_block;
		if (os_unlikely(block_meta.xzb_cookie != expected_cookie)) {
			xzm_client_abort_with_reason(
					"corrupt tiny freelist - cookie, client likely has a buffer"
					" overflow or use-after-free bug", block_meta.xzb_cookie);
		}

		union xzm_block_linkage_u linkage = {
			.xzbl_next_offset = block_meta.xzb_linkage.xzbl_next_offset,
			.xzbl_next_seqno = block_meta.xzb_linkage.xzbl_next_seqno,
			.xzbl_seqno = cur_seqno,
		};

		linkage.xzbl_next_value = (uintptr_t)ptrauth_sign_unauthenticated(
				linkage.xzbl_next, ptrauth_key_process_dependent_data,
				ptrauth_blend_discriminator(cur_block,
						ptrauth_string_discriminator("xzb_linkage")));

		linkage.xzbl_seqno = 0;

		if (block_meta.xzb_linkage.xzbl_next_value != linkage.xzbl_next_value) {
			xzm_client_abort_with_reason(
					"corrupt tiny freelist - linkage, client likely has a "
					"buffer overflow or use-after-free bug",
					block_meta.xzb_linkage.xzbl_next_value);
		}

		uint64_t next_seqno = (uint16_t)block_meta.xzb_linkage.xzbl_next_seqno;
		uint64_t next_block_offset = block_meta.xzb_linkage.xzbl_next_offset;

		if (next_block_offset == XZM_FREE_NULL) {
			// freelist walk should be complete
			if (os_unlikely(freelist_count != old_meta.xca_free_count)) {
				xzm_client_abort_with_reason(
						"corrupt tiny freelist - free count, client likely has a"
						" buffer overflow or use-after-free bug",
						freelist_count);
			}

			break;
		}

		if (os_unlikely(next_block_offset % block_granules != 0 ||
				next_block_offset > max_offset ||
				freelist_count >= old_meta.xca_free_count)) {
			xzm_client_abort_with_reason(
					"corrupt tiny freelist - inconsistent walk, client likely"
					" has a buffer overflow or use-after-free bug",
					freelist_count);
		}

		cur_block_offset = next_block_offset;
		cur_seqno = next_seqno;
		freelist_count++;
	}

	xzm_block_t tail_block = cur_block;
	uint64_t tail_seqno = cur_seqno;

	union xzm_block_linkage_u linkage = {
		.xzbl_next_offset = cache->xztc_head,
		.xzbl_next_seqno = cache->xztc_head_seqno,
		.xzbl_seqno = tail_seqno,
	};
	linkage.xzbl_next_value = (uintptr_t)ptrauth_sign_unauthenticated(
			linkage.xzbl_next, ptrauth_key_process_dependent_data,
			ptrauth_blend_discriminator(tail_block,
					ptrauth_string_discriminator("xzb_linkage")));
	linkage.xzbl_seqno = 0;

	os_atomic_store(&tail_block->xzb_linkage.xzbl_next_value,
			linkage.xzbl_next_value, relaxed);
}

static void
_xzm_xzone_thread_cache_detach(xzm_main_malloc_zone_t main, xzm_xzone_t xz,
		xzm_xzone_thread_cache_t cache)
{
	xzm_malloc_zone_t zone = &main->xzmz_base;
	xzm_chunk_t chunk = cache->xztc_chunk;

	size_t chunk_capacity = xz->xz_chunk_capacity;
	uint16_t local_free_count = cache->xztc_free_count;

	xzm_chunk_atomic_meta_u old_meta = {
		.xca_value = os_atomic_load_wide(
				&chunk->xzc_atomic_meta.xca_value, dependency),
	};

	xzm_chunk_atomic_meta_u new_meta;

	bool needs_link = (cache->xztc_head < XZM_FREE_LIMIT);
	bool push_to_partial = false;
	while (true) {
		if (os_unlikely(old_meta.xca_walk_locked)) {
			_xzm_walk_lock_wait(zone);
			old_meta.xca_value = os_atomic_load_wide(
					&chunk->xzc_atomic_meta.xca_value, dependency);
			continue;
		}

		new_meta = old_meta;
		new_meta.xca_alloc_idx = XZM_SLOT_INDEX_EMPTY;

		xzm_debug_assert(!push_to_partial);

		uint16_t new_free_count = old_meta.xca_free_count + local_free_count;
		if (new_free_count == chunk_capacity) {
			// The entire chunk will now be free

			// TODO madvise heuristic?  Doesn't seem like it would really make
			// sense here
			new_meta.xca_free_count = 0;
			new_meta.xca_alloc_head = XZM_FREE_MADVISING;
		} else if (new_free_count) {
			xzm_debug_assert(!old_meta.xca_on_partial_list &&
					!old_meta.xca_on_empty_list);
			new_meta.xca_free_count = new_free_count;
			new_meta.xca_on_partial_list = true;
			push_to_partial = true;

			if (old_meta.xca_alloc_head == XZM_FREE_NULL) {
				new_meta.xca_alloc_head = cache->xztc_head;
				new_meta.xca_head_seqno = cache->xztc_head_seqno;
			} else {
				xzm_debug_assert(old_meta.xca_alloc_head < XZM_FREE_LIMIT);
				if (needs_link) {
					_xzm_xzone_thread_cache_detach_link_to_remote(zone, cache,
							old_meta);
					needs_link = false;
				}
			}
		}

		// Release ordering pairs with the dependency order loads/cmpxchgs on
		// allocation
		bool success = os_atomic_cmpxchgv(&chunk->xzc_atomic_meta.xca_value,
				old_meta.xca_value, new_meta.xca_value, &old_meta.xca_value,
				release);
		if (!success) {
			// Try again.
			push_to_partial = false;
			continue;
		}

		xzm_trace(thread_detach, (uint64_t)chunk, old_meta.xca_value_lo,
				new_meta.xca_value_lo, cache->xztc_freelist_state);
		break;
	}

	if (new_meta.xca_alloc_head == XZM_FREE_MADVISING) {
#if CONFIG_TINY_ALLOCATION_SLOT_LOCK
#error "not compatible with thread caching"
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK

		// Do the madvise we committed to.
		_xzm_xzone_madvise_freelist_chunk(zone, xz, chunk);
	} else if (push_to_partial) {
		_xzm_chunk_list_slot_push(zone, xz, zone->xzz_partial_lists, chunk);
	}
}

static void
_xzm_xzone_thread_cache_destructor(void *arg)
{
	xzm_thread_cache_t tc = arg;
	xzm_main_malloc_zone_t main = tc->xtc_main;
	xzm_malloc_zone_t zone = &main->xzmz_base;

	tc->xtc_teardown_gen = os_atomic_inc(
			&main->xzmz_thread_cache_teardown_gen, relaxed);

	for (size_t i = XZM_XZONE_INDEX_FIRST;
			i < zone->xzz_thread_cache_xzone_count; i++) {
		xzm_xzone_thread_cache_t cache = &tc->xtc_xz_caches[i];
		if (cache->xztc_head <= XZM_FREE_LIMIT) {
			xzm_xzone_t xz = &zone->xzz_xzones[i];
			_xzm_xzone_thread_cache_detach(main, xz, cache);
		}
	}

	_malloc_lock_lock(&main->xzmz_thread_cache_list_lock);
	LIST_REMOVE(tc, xtc_linkage);
	_malloc_lock_unlock(&main->xzmz_thread_cache_list_lock);

	xzm_metapool_t mp = &main->xzmz_metapools[XZM_METAPOOL_THREAD_CACHE];
	xzm_metapool_free(mp, tc);
}

#endif // CONFIG_XZM_THREAD_CACHE

#pragma mark Small allocation

// mimalloc: mi_page_init
static void
_xzm_xzone_small_chunk_init(xzm_xzone_t xz, xzm_chunk_t chunk)
{
	// Precondition: xzcb_kind, xzcs_slice_count and xzsl_slice_offset are
	// already initialized
	xzm_debug_assert(chunk->xzc_free == 0);
	xzm_debug_assert(chunk->xzc_used == 0);
	xzm_debug_assert(chunk->xzc_mzone_idx == XZM_MZONE_INDEX_INVALID);

	_xzm_xzone_fresh_chunk_init(xz, chunk, XZM_SLICE_KIND_SMALL_CHUNK);

	// Enumerator protocol: mzone_idx is initialized last to "publish" the chunk
	chunk->xzc_mzone_idx = xz->xz_mzone_idx;
}

// mimalloc: mi_page_fresh_alloc
static xzm_chunk_t
_xzm_xzone_small_chunk_alloc(xzm_malloc_zone_t zone, xzm_xzone_t xz)
{
	// First, check the preallocated list
	xzm_chunk_t chunk = NULL;
	if (LIST_FIRST(&xz->xz_chunkq_preallocated)) {
		_malloc_lock_lock(&xz->xz_lock);
		if ((chunk = LIST_FIRST(&xz->xz_chunkq_preallocated))) {
			LIST_REMOVE(chunk, xzc_entry);
		}
		_malloc_lock_unlock(&xz->xz_lock);
	}

	// If none are present on the preallocated list, request a new run from the
	// segment group, and enqueue the spares into the preallocated list
	if (!chunk) {
		xzm_slice_kind_t kind = XZM_SLICE_KIND_SMALL_CHUNK;
		size_t span_size = XZM_SMALL_CHUNK_SIZE;
		xzm_slice_count_t slice_count =
				(xzm_slice_count_t)(span_size / XZM_SEGMENT_SLICE_SIZE);
		bool clear = false;
		bool purgeable = false; // Purgeable only applies to LARGE/HUGE chunks
		xzm_preallocate_list_s preallocated;
		SLIST_INIT(&preallocated);
		xzm_preallocate_list_s *list = &preallocated;
		xzm_segment_group_t sg = _xzm_segment_group_for_id_and_front(zone,
				xz->xz_segment_group_id, xz->xz_front, false);
		chunk = xzm_segment_group_alloc_chunk(sg, kind, &xz->xz_guard_config,
				slice_count, list, 0, clear, purgeable);
		if (!chunk) {
			xzm_debug_assert(!list || !SLIST_FIRST(list));
			return NULL;
		}
		chunk->xzc_xzone_idx = xz->xz_idx;
		// Most processes don't get guard pages, so will only allocate one chunk
		// from the segment group. Avoid taking the lock in this common case
		if (SLIST_FIRST(list)) {
			xzm_chunk_t c, t;
			_malloc_lock_lock(&xz->xz_lock);
			SLIST_FOREACH_SAFE(c, list, xzc_slist_entry, t) {
				SLIST_REMOVE_HEAD(list, xzc_slist_entry);
				c->xzc_xzone_idx = xz->xz_idx;
				c->xzc_bits.xzcb_preallocated = true;
				LIST_INSERT_HEAD(&xz->xz_chunkq_preallocated, c, xzc_entry);
			}
			_malloc_lock_unlock(&xz->xz_lock);
		}
	}

	xzm_debug_assert(chunk);
	_xzm_xzone_small_chunk_init(xz, chunk);

	return chunk;
}

static xzm_slice_t
_xzm_chunk_find_dirtiest_slice(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		uint32_t *slice_free_bitmap)
{
	// Attempt to find the dirtiest slice with at least one free block
	xzm_slice_count_t slice_count = _xzm_chunk_slice_count(chunk);
	struct xzm_slice_s *slices = _xzm_chunk_slices_of(chunk, slice_count);
	uint32_t lowest_free = UINT32_MAX;
	xzm_slice_t best_slice = NULL;
	for (xzm_slice_count_t i = 0; i < slice_count; ++i) {
		xzm_slice_t slice = &slices[i];
		uint32_t slice_mask = _xzm_xzone_slice_free_mask(zone, slice);
		uint32_t slice_free = chunk->xzc_free & slice_mask;
		if (slice_free && slice_free != slice_mask) {
			// This slice contains at least one used and one free block
			// TODO: consider replacing popcount with table (max blocks per
			// slice is bounded)
			uint32_t free_count = __builtin_popcount(slice_free);
			if (free_count < lowest_free) {
				lowest_free = free_count;
				best_slice = slice;
				*slice_free_bitmap = slice_free;
			}
		}
	}
	return best_slice;
}

// mimalloc: _mi_page_malloc
static void *
_xzm_xzone_alloc_from_chunk(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk, bool *is_zero_out)
{
	xzm_debug_assert(!_xzm_chunk_is_full(zone, xz, chunk));

	*is_zero_out = false;

	uintptr_t block;
	xzm_block_index_t block_index = UINT32_MAX;
	uintptr_t start = _xzm_chunk_start(zone, chunk, NULL);
	const xzm_segment_t segment = _xzm_segment_for_slice(zone, chunk);
	const size_t block_size = _xzm_chunk_block_size(zone, chunk);

	switch (chunk->xzc_bits.xzcb_kind) {
	case XZM_SLICE_KIND_SMALL_CHUNK:
		// TODO: Find a way to determine if this block is zeroed to
		// avoid excessive zeroing
		if (_xzm_chunk_is_empty(zone, xz, chunk) ||
				(block_size % XZM_SEGMENT_SLICE_SIZE) == 0) {
			// It is impossible for a free block to exist on a dirty slice for
			// this chunk -- don't bother searching for one
			block_index = __builtin_ffs(chunk->xzc_free) - 1;
		} else {
			uint32_t slice_free;
			xzm_slice_t best_slice = _xzm_chunk_find_dirtiest_slice(zone, chunk,
					&slice_free);

			if (best_slice != NULL) {
				block_index = __builtin_ffs(slice_free) - 1;

				if (slice_free && !powerof2(slice_free)) {
					// This slice has more than one free block; check
					// if the one we selected would dirty multiple slices
					uintptr_t block_start = start +
							(block_index + xz->xz_block_size);
					xzm_slice_t start_slice = _xzm_segment_slice_of(segment,
							block_start);
					if (start_slice < best_slice) {
						// This block would dirty multiple slices; choose the
						// next free block instead. Note that the next free
						// block may also dirty multiple slices; this implies
						// that there are no free blocks in between that might
						// dirty fewer slices.
						block_index = __builtin_ffs(slice_free &
								~(1u << block_index)) - 1;
					}
				}
			} else {
				// Unable to find a dirty slice, fall back to the first
				// available free block
				block_index = __builtin_ffs(chunk->xzc_free) - 1;
			}
		}

		xzm_debug_assert(block_index <= xz->xz_chunk_capacity);
		block = start + (block_index * xz->xz_block_size);


		// Mark the current block as not free
		chunk->xzc_free &= ~(1u << block_index);

		break;
	default:
		xzm_abort_with_reason("attempting to allocate from chunk of bad kind",
				(unsigned int)chunk->xzc_bits.xzcb_kind);
	}

	chunk->xzc_used++;

	return (void *)block;
}

static void
_xzm_xzone_upgrade_small_slot_config(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_xzone_allocation_slot_t xas, xzm_slot_config_t slot_config)
{
	// This allocation slot is hot, upgrade this xzone's slot config
	// for subsequent allocations and reset its contention counters
	xas->xas_contentions = 0;

	// Walk all the other allocation slots and reset their contention counters
	xzm_allocation_index_t limit_idx = _xzm_get_limit_allocation_index(slot_config);
	if (limit_idx > 1) {
		_malloc_lock_unlock(&xas->xas_lock);
		for (xzm_allocation_index_t alloc_idx = 0;
				alloc_idx < limit_idx;
				alloc_idx++) {
			xzm_xzone_allocation_slot_t xas_other =
					_xzm_xzone_allocation_slot_for_index(zone, xz,
					alloc_idx);
			if (xas_other != xas) {
				_malloc_lock_lock(&xas_other->xas_lock);
				xas_other->xas_contentions = 0;
				_malloc_lock_unlock(&xas_other->xas_lock);
			}
		}
		_malloc_lock_lock(&xas->xas_lock);
	}

	_malloc_lock_lock(&xz->xz_lock);
	// Another thread may have already upgraded the slot config
	if (xz->xz_slot_config == slot_config) {
		switch (xz->xz_slot_config) {
		case XZM_SLOT_SINGLE:
#if CONFIG_XZM_CLUSTER_AWARE
			if (ncpuclusters > 1) {
				xz->xz_slot_config = XZM_SLOT_CLUSTER;
			} else {
				xz->xz_slot_config = XZM_SLOT_CPU;
			}
#else // CONFIG_XZM_CLUSTER_AWARE
			xz->xz_slot_config = XZM_SLOT_CLUSTER;
#endif // CONFIG_XZM_CLUSTER_AWARE
			break;
		case XZM_SLOT_CLUSTER:
			xz->xz_slot_config = XZM_SLOT_CPU;
			break;
		case XZM_SLOT_CPU:
			xzm_abort("Can't upgrade from XZM_SLOT_CPU");
			break;
		default:
			xzm_abort("Invalid xzone slot config");
			break;
		}
		xzm_debug_assert(xz->xz_slot_config <= zone->xzz_max_slot_config);
	}
	_malloc_lock_unlock(&xz->xz_lock);
}

MALLOC_NOINLINE
static void *
_xzm_xzone_malloc_small(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_malloc_options_t opt)
{
	bool clear = (opt & XZM_MALLOC_CLEAR);
	void *ptr = NULL;
	bool is_zero = false;
	xzm_slot_config_t slot_config;

	xzm_allocation_index_t alloc_idx = _xzm_get_allocation_index(zone, xz,
			&slot_config, true);
	xzm_xzone_allocation_slot_t xas =
			_xzm_xzone_allocation_slot_for_index(zone, xz, alloc_idx);

	if (slot_config < zone->xzz_max_slot_config) {
		// This xzone's slot config is upgradable -- evaluate for contention
		if (!_malloc_lock_trylock(&xas->xas_lock)) {
			// Contended for this allocation slot
			_malloc_lock_lock(&xas->xas_lock);
			xas->xas_contentions++;
			if (xas->xas_contentions > zone->xzz_slot_upgrade_threshold[slot_config]) {
				_xzm_xzone_upgrade_small_slot_config(zone, xz, xas, slot_config);
			}
		}
		xas->xas_allocs++;
		// Reset the contention counter every T allocations -- this prevents
		// low-traffic xzones in long-lived processes from being upgraded over
		// time
		if ((xas->xas_allocs % zone->xzz_slot_upgrade_period) == 0) {
			xas->xas_contentions = 0;
		}
	} else {
		_malloc_lock_lock(&xas->xas_lock);
#ifdef DEBUG
		xas->xas_allocs++;
#endif
	}

	xzm_chunk_t chunk = xas->xas_chunk;
	if (!chunk || _xzm_chunk_is_full(zone, xz, chunk)) {
		// We need to go to the xzone to get a new chunk
		_malloc_lock_lock(&xz->xz_lock);

		if (chunk) {
			// If we have an existing chunk that's full, we need to uninstall it and
			// move it to the full list
			LIST_INSERT_HEAD(&xz->xz_chunkq_full, chunk, xzc_entry);

			// This is the final step of uninstallation - as soon as the chunk
			// transitions to the uninstalled state, other threads freeing to it
			// will go to its lock instead of the lock for this slot.  Store
			// release to publish all of the changes made to the chunk under the
			// slot lock, which will pair with the acquire when they take the
			// chunk lock
			os_atomic_store(&chunk->xzc_alloc_idx, XZM_SLOT_INDEX_EMPTY,
					release);
		}

		while ((chunk = LIST_FIRST(&xz->xz_chunkq_partial))) {
			_malloc_lock_lock(&chunk->xzc_lock);
			xzm_debug_assert(chunk->xzc_bits.xzcb_on_partial_list);
			LIST_REMOVE(chunk, xzc_entry);
			chunk->xzc_bits.xzcb_on_partial_list = false;
			if (chunk->xzc_used) {
				xzm_debug_assert(!_xzm_chunk_is_full(zone, xz, chunk));
				// We can use this chunk!  Install it.  We hold both the slot
				// and chunk locks now so threads that are freeing to this chunk
				// are guaranteed visibility of our modifications regardless of
				// what they observe for the slot index.
				xas->xas_chunk = chunk;
				os_atomic_store(&chunk->xzc_alloc_idx, alloc_idx + 1, relaxed);
				_malloc_lock_unlock(&chunk->xzc_lock);
				break;
			} else {
				// This chunk is fully free, and the thread that freed it is on
				// its way to remove it, so we don't want to use it.  We've
				// pulled it off the partial list so nobody else spends time on
				// it and marked it accordingly so that the freeing thread
				// knows.
				_malloc_lock_unlock(&chunk->xzc_lock);
			}
		}

		// Attempt to reuse a chunk from the batch list
		if (!chunk && xz->xz_chunkq_batch_count) {
			chunk = xz->xz_chunkq_batch;
			xzm_debug_assert(chunk);
			xz->xz_chunkq_batch = *_xzm_segment_slice_meta_batch_next(zone,
					chunk);
			xzm_debug_assert(_xzm_slice_meta_is_batch_pointer(zone,
					xz->xz_chunkq_batch));
#if CONFIG_XZM_DEFERRED_RECLAIM
			*_xzm_slice_meta_reclaim_id(zone, chunk) = VM_RECLAIM_ID_NULL;
#endif // CONFIG_XZM_DEFERRED_RECLAIM
			--xz->xz_chunkq_batch_count;
			xas->xas_chunk = chunk;
			os_atomic_store(&chunk->xzc_alloc_idx, alloc_idx + 1, relaxed);
		}

		// We're done with the xzone now - either we got a partial chunk from
		// it, or we didn't and we need to get a sequestered or fresh one
		_malloc_lock_unlock(&xz->xz_lock);

		if (!chunk && xz->xz_sequestered) {
			xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
			chunk = _xzm_xzone_allocate_chunk_from_isolation(main, xz);

			if (chunk) {
				// Now install the chunk to the slot (TODO: does this need to be
				// done under the iz lock? TODO: does this need to be done
				// before populating in exclaves?)
				xas->xas_chunk = chunk;
				os_atomic_store(&chunk->xzc_alloc_idx, alloc_idx + 1, relaxed);
			}
		}

		if (!chunk) {
			chunk = _xzm_xzone_small_chunk_alloc(zone, xz);
			if (chunk) {
				xzm_debug_assert(!_xzm_chunk_is_full(zone, xz, chunk));
				// Succeeded at allocating a chunk.  Install it to the slot.
				xas->xas_chunk = chunk;
				os_atomic_store(&chunk->xzc_alloc_idx, alloc_idx + 1, relaxed);
			} else {
				// There's no other way for us to get a chunk, so we need to
				// give up.
				xas->xas_chunk = NULL;
				goto out;
			}
		}
	}

	xzm_debug_assert(xas->xas_chunk == chunk);
	xzm_debug_assert(!_xzm_chunk_is_full(zone, xz, chunk));

	ptr = _xzm_xzone_alloc_from_chunk(zone, xz, chunk, &is_zero);
	xzm_debug_assert(ptr);

out:
	_malloc_lock_unlock(&xas->xas_lock);

	if (os_likely(ptr)) {
#if CONFIG_MTE
		bool memtag_enabled = xz->xz_tagged;
		bool needs_canonical_tagging = (opt & XZM_MALLOC_CANONICAL_TAG);
		if (memtag_enabled) {
			if (needs_canonical_tagging) {
				// We need this specific block to have canonical tagging (i.e. tag = 0)
				ptr = memtag_tag_canonical(ptr, xz->xz_block_size);
			} else {
				ptr = _xzm_xzone_block_memtag_retag(zone, ptr,
						xz->xz_block_size);
			}
		}
#endif

		// TODO: MallocCheckZeroOnFreeCorruption support
		if (!is_zero && clear &&
				xz->xz_block_size > XZM_ZERO_ON_FREE_THRESHOLD) {
			bzero(ptr, xz->xz_block_size);
		}
	} else {
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
	}
	return ptr;
}

#pragma mark General allocation

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_xzone_try_reserve_early_budget(xzm_malloc_zone_t zone, xzm_xzone_t xz)
{
	uint16_t early_budget = os_atomic_load(&xz->xz_early_budget, relaxed);
	uint16_t updated_budget;
	do {
		if (os_likely(!early_budget)) {
			return false;
		}

		updated_budget = early_budget - 1;
	} while (!os_atomic_cmpxchgv(&xz->xz_early_budget, early_budget,
			updated_budget, &early_budget, relaxed));

	return true;
}

#ifdef DEBUG
// mimalloc: mi_mem_is_zero
static bool
_xzm_mem_is_zero(uint8_t *ptr, size_t size)
{
	return !_malloc_memcmp_zero_aligned8(ptr, size);
}
#endif // DEBUG

#if CONFIG_XZM_THREAD_CACHE

static void *
_xzm_xzone_malloc_tiny_or_early(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_malloc_options_t opt)
{
	if (!(opt & XZM_MALLOC_NO_MFM) &&
			_xzm_xzone_try_reserve_early_budget(zone, xz)) {
		void *ptr = mfm_alloc(xz->xz_block_size);
		xzm_debug_assert(ptr);
#ifdef DEBUG
		if (opt & XZM_MALLOC_CLEAR) {
			xzm_debug_assert(_xzm_mem_is_zero(ptr, xz->xz_block_size));
		}
#endif
		return ptr;
	}

	void *ptr = _xzm_xzone_malloc_tiny(zone, xz, 0);
#ifdef DEBUG
	if (opt & XZM_MALLOC_CLEAR) {
		xzm_debug_assert(_xzm_mem_is_zero(ptr, xz->xz_block_size));
	}
#endif
	return ptr;
}

MALLOC_NOINLINE
static void *
_xzm_xzone_thread_cache_malloc_corrupt(void *corrupt_block)
{
	// This looks kind of weird, but I couldn't convince the optimizer not to
	// push a frame in _xzm_xzone_malloc() if I put the abort here directly, so
	// we need to launder it apparently
	return _xzm_xzone_malloc_freelist_outlined(NULL, NULL, 0, NULL, corrupt_block,
			0);
}

MALLOC_NOINLINE
static void *
_xzm_xzone_thread_cache_fill_and_malloc(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_xzone_thread_cache_t cache)
{
	void *ptr = NULL;
	bool contended = false;
	if (cache->xztc_state != XZM_XZONE_CACHE_EMPTY) {
		xzm_debug_assert(cache->xztc_head == XZM_FREE_NULL);

		ptr = _xzm_xzone_malloc_from_freelist_chunk(zone, xz,
				XZM_SLOT_INDEX_THREAD, cache, cache->xztc_chunk, false,
				&contended, NULL);
		if (ptr) {
			goto done;
		}
	}

	xzm_debug_assert(cache->xztc_state == XZM_XZONE_CACHE_EMPTY);

	xzm_chunk_t chunk = NULL;
	ptr = _xzm_xzone_find_and_malloc_from_freelist_chunk(zone, xz,
			XZM_SLOT_INDEX_THREAD, cache, &chunk, &contended);

done:
	if (ptr) {
		xzm_block_t block = ptr;
		*block = (struct xzm_block_inline_meta_s){ 0 };
	} else {
		xzm_debug_assert(cache->xztc_state == XZM_XZONE_CACHE_EMPTY);
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
	}

	return ptr;
}

static void
_xzm_thread_cache_create(xzm_malloc_zone_t zone)
{
	xzm_main_malloc_zone_t main = (xzm_main_malloc_zone_t)zone;
	xzm_metapool_t mp = &main->xzmz_metapools[XZM_METAPOOL_THREAD_CACHE];
	xzm_thread_cache_t tc = xzm_metapool_alloc(mp);

	*tc = (struct xzm_thread_cache_s){
		.xtc_main = main,
		.xtc_thread = pthread_self(),
	};

	uint64_t now = mach_absolute_time();
	for (size_t i = 0; i < zone->xzz_thread_cache_xzone_count; i++) {
		tc->xtc_xz_caches[i] = (xzm_xzone_thread_cache_u){
			.xztc_state = XZM_XZONE_NOT_CACHED,
			.xztc_timestamp = now,
		};
	}

	_malloc_lock_lock(&main->xzmz_thread_cache_list_lock);
	LIST_INSERT_HEAD(&main->xzmz_thread_cache_list, tc, xtc_linkage);
	_malloc_lock_unlock(&main->xzmz_thread_cache_list_lock);

	_pthread_setspecific_direct(__TSD_MALLOC_XZONE_THREAD_CACHE, tc);
}

MALLOC_NOINLINE
static void *
_xzm_thread_cache_create_and_malloc(xzm_malloc_zone_t zone,
		xzm_xzone_index_t xz_idx, xzm_xzone_t xz, xzm_malloc_options_t opt)
{
	_xzm_thread_cache_create(zone);

	return _xzm_xzone_malloc_tiny_or_early(zone, xz, opt);
}

MALLOC_NOINLINE
static void *
_xzm_xzone_thread_cache_record_and_malloc_outlined(xzm_malloc_zone_t zone,
		xzm_xzone_t xz, xzm_malloc_options_t opt,
		xzm_xzone_thread_cache_t cache)
{
	xzm_debug_assert(cache->xztc_head == XZM_XZONE_NOT_CACHED);

	uint64_t now = mach_absolute_time();
	uint64_t activation_time = zone->xzz_thread_cache_xzone_activation_time;
	if (now - cache->xztc_timestamp < activation_time) {
		// We reached the threshold number of allocations within the
		// activation threshold time, so activate caching for this xzone
		cache->xztc_head = XZM_XZONE_CACHE_EMPTY;

		// Initialize the seqno for this cache with the low bits of the
		// timestamp to add some non-determinism to its base
		cache->xztc_seqno = cache->xztc_timestamp & XZM_SEQNO_COUNTER_MASK;

		return _xzm_xzone_thread_cache_fill_and_malloc(zone, xz, cache);
	}

	// reset for the next period
	cache->xztc_timestamp = now;
	cache->xztc_allocs = 0;
	cache->xztc_contentions = 0;

	return _xzm_xzone_malloc_tiny_or_early(zone, xz, opt);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void *
_xzm_xzone_thread_cache_record_and_malloc(xzm_malloc_zone_t zone,
		xzm_xzone_t xz, xzm_malloc_options_t opt,
		xzm_xzone_thread_cache_t cache)
{
	xzm_debug_assert(cache->xztc_head == XZM_XZONE_NOT_CACHED);

	cache->xztc_allocs++;

	uint32_t activation_period = zone->xzz_thread_cache_xzone_activation_period;
	if (os_unlikely(cache->xztc_allocs == activation_period)) {
		return _xzm_xzone_thread_cache_record_and_malloc_outlined(zone, xz, opt,
				cache);
	}

	return _xzm_xzone_malloc_tiny_or_early(zone, xz, opt);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
_xzm_xzone_thread_cache_record_contention(xzm_malloc_zone_t zone,
		xzm_xzone_t xz, xzm_xzone_thread_cache_t cache)
{
	// We can end up here after caching is already engaged if serving an
	// allocation that can't be handled from the cache
	if (cache->xztc_head != XZM_XZONE_NOT_CACHED) {
		return;
	}

	cache->xztc_contentions++;

	uint32_t activation_contentions =
			zone->xzz_thread_cache_xzone_activation_contentions;
	if (os_unlikely(cache->xztc_contentions == activation_contentions)) {
		// We reached the threshold number of contentions within the threshold
		// period, so activate caching for this xzone
		cache->xztc_head = XZM_XZONE_CACHE_EMPTY;

		// Initialize the seqno for this cache with the low bits of the
		// timestamp to add some non-determinism to its base
		cache->xztc_seqno = cache->xztc_timestamp & XZM_SEQNO_COUNTER_MASK;
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void *
_xzm_xzone_thread_cache_malloc(xzm_malloc_zone_t zone, xzm_xzone_index_t xz_idx,
		xzm_xzone_t xz, xzm_malloc_options_t opt)
{
	xzm_thread_cache_t tc = _xzm_get_thread_cache();
	if (os_unlikely(!tc)) {
		return _xzm_thread_cache_create_and_malloc(zone, xz_idx, xz, opt);
	}

	xzm_debug_assert(xz_idx < zone->xzz_thread_cache_xzone_count);
	xzm_xzone_thread_cache_t cache = &tc->xtc_xz_caches[xz_idx];
	if (cache->xztc_head < XZM_FREE_LIMIT) {
		uint8_t *start = cache->xztc_chunk_start;
		void *ptr = start + (cache->xztc_head * XZM_GRANULE);

#if CONFIG_MTE
		if (_xzm_zone_memtag_enabled(zone)) {
			ptr = memtag_fixup_ptr(ptr);
		}
#endif
		struct xzm_block_inline_meta_s *block_meta_p =
				(struct xzm_block_inline_meta_s *)ptr;
		struct xzm_block_inline_meta_s block_meta = *block_meta_p;

		// Check the PAC by adding the seqno back
		union xzm_block_linkage_u linkage = {
			.xzbl_next_offset = block_meta.xzb_linkage.xzbl_next_offset,
			.xzbl_next_seqno = block_meta.xzb_linkage.xzbl_next_seqno,
			.xzbl_seqno = cache->xztc_head_seqno,
		};

		linkage.xzbl_next_value = (uintptr_t)ptrauth_sign_unauthenticated(
				linkage.xzbl_next, ptrauth_key_process_dependent_data,
				ptrauth_blend_discriminator(ptr,
						ptrauth_string_discriminator("xzb_linkage")));

		// Take the seqno back out of the signed value for the comparison with
		// the stored one
		linkage.xzbl_seqno = 0;

		if (os_unlikely(block_meta.xzb_linkage.xzbl_next_value !=
				linkage.xzbl_next_value)) {
			return _xzm_xzone_thread_cache_malloc_corrupt(ptr);
		}

		*block_meta_p = (struct xzm_block_inline_meta_s){ 0 };

#ifdef DEBUG
		uint64_t orig_state = cache->xztc_freelist_state;
#endif

		// We update the local freelist head and count together with an atomic
		// store so that the enumerator gets a consistent picture of the
		// freelist.  Having the head and the count agree is required in order
		// to handle the bump correctly:
		// - If the head were updated before the count, we'd think the bump
		//   offset was higher than its true value, so we'd falsely report the
		//   block at the bump offset as allocated when it isn't
		// - If the head were updated after the count, we'd think the bump
		//   offset was lower than its true value, so we'd falsely report the
		//   last block before the bump as free when it may not be
		xzm_xzone_thread_cache_atomic_meta_u tc_meta = {
			.xztcam_head = block_meta.xzb_linkage.xzbl_next_offset,
			.xztcam_free_count = cache->xztc_free_count - 1,
		};
		os_atomic_store(&cache->xztc_atomic_meta.xztcam_value,
				tc_meta.xztcam_value, relaxed);
		cache->xztc_head_seqno = block_meta.xzb_linkage.xzbl_next_seqno;

#ifdef DEBUG
		uint64_t new_state = cache->xztc_freelist_state;
#endif

		xzm_trace(thread_cache_malloc, (uint64_t)cache->xztc_chunk, orig_state,
				new_state, 0);

		return ptr;
	} else if (cache->xztc_head == XZM_FREE_NULL && cache->xztc_free_count) {
		uint8_t *start = cache->xztc_chunk_start;
		size_t capacity = xz->xz_chunk_capacity;
		void *ptr = start + ((capacity - cache->xztc_free_count) *
				xz->xz_block_size);
#if CONFIG_MTE
		if (_xzm_zone_memtag_enabled(zone)) {
			ptr = memtag_fixup_ptr(ptr);
		}
#endif

		struct xzm_block_inline_meta_s *block_meta_p =
				(struct xzm_block_inline_meta_s *)ptr;
		*block_meta_p = (struct xzm_block_inline_meta_s){ 0 };

#ifdef DEBUG
		uint64_t orig_state = cache->xztc_freelist_state;
#endif

		cache->xztc_free_count--;

#ifdef DEBUG
		uint64_t new_state = cache->xztc_freelist_state;
#endif

		xzm_trace(thread_cache_malloc, (uint64_t)cache->xztc_chunk, orig_state,
				new_state, 0);

		return ptr;
	} else if (cache->xztc_head == XZM_XZONE_NOT_CACHED) {
		return _xzm_xzone_thread_cache_record_and_malloc(zone, xz, opt, cache);
	} else {
		xzm_debug_assert(cache->xztc_head == XZM_FREE_NULL ||
				cache->xztc_head == XZM_XZONE_CACHE_EMPTY);
		return _xzm_xzone_thread_cache_fill_and_malloc(zone, xz, cache);
	}
}

#endif // CONFIG_XZM_THREAD_CACHE

static void *
_xzm_xzone_malloc(xzm_malloc_zone_t zone, size_t size,
		xzm_xzone_index_t xz_idx, xzm_malloc_options_t opt)
{
	void *ptr = NULL;

	xzm_xzone_t xz = &zone->xzz_xzones[xz_idx];

#if CONFIG_XZM_THREAD_CACHE
	// First, try thread cache
	if (zone->xzz_thread_cache_enabled &&
			size <= XZM_THREAD_CACHE_THRESHOLD
#if CONFIG_MTE
			&& !(opt & XZM_MALLOC_CANONICAL_TAG)
#endif
			) {
		return _xzm_xzone_thread_cache_malloc(zone, xz_idx, xz, opt);
	}
#endif // CONFIG_XZM_THREAD_CACHE

	if (!(opt & XZM_MALLOC_NO_MFM) && _xzm_malloc_zone_is_main(zone) &&
#if CONFIG_MTE
			// With memory tagging enabled, we shouldn't call into the early
			// allocator if we're being asked for a canonically tagged
			// allocation, as we don't support that in MFM.
			!((opt & XZM_MALLOC_CANONICAL_TAG) && zone->xzz_memtag_config.enabled) &&
#endif // CONFIG_MTE
			_xzm_xzone_try_reserve_early_budget(zone, xz)) {
		// To maintain compatibility with code expecting
		// malloc_size(malloc(size)) >= malloc_good_size(size), just always use
		// the block size
		ptr = mfm_alloc(xz->xz_block_size);
		// The way that we use the early allocator, it can't ever legitimately
		// fail, and we're depending on that here (to tail-call)
		xzm_debug_assert(ptr);
		return ptr;
	}

	if (size <= XZM_TINY_BLOCK_SIZE_MAX) {
		return _xzm_xzone_malloc_tiny(zone, xz, opt);
	} else if (zone->xzz_small_freelist_enabled) {
		return _xzm_xzone_malloc_small_freelist(zone, xz, opt);
	}

	xzm_debug_assert(size <= XZM_SMALL_BLOCK_SIZE_MAX);
	return _xzm_xzone_malloc_small(zone, xz, opt);
}

static size_t
xzm_malloc_zone_size(xzm_malloc_zone_t zone, const void *ptr);

void *
xzm_malloc_inline(xzm_malloc_zone_t zone, size_t size,
		malloc_type_descriptor_t type_desc, xzm_malloc_options_t opt)
{
	void *ptr;
	if (size > XZM_SMALL_BLOCK_SIZE_MAX) {
		ptr = _xzm_malloc_large_huge(zone, size, 0, type_desc, opt);
	} else {
		xzm_debug_assert((zone->xzz_flags & MALLOC_PURGEABLE) == 0);
		xzm_xzone_index_t xz_idx = _xzm_xzone_lookup(zone, size, type_desc);
		ptr = _xzm_xzone_malloc(zone, size, xz_idx, opt);
	}

#ifdef DEBUG
	xzm_debug_assert(ptr);
	if (opt & XZM_MALLOC_CLEAR) {
		xzm_debug_assert(_xzm_mem_is_zero(ptr, size));
	}
#if CONFIG_MTE
	// The `size` parameter means "requested size", but we make tagging decisions
	// based on block size.  Note that in the special case of allocations with
	// large alignments we may get an incorrect answer from
	// _xzm_zone_memtag_block(), since it can't account for the segment group
	// min block size, but that case doesn't take this path so the assert here
	// is still safe.
	size_t block_size = xzm_malloc_zone_size(zone, ptr);
	bool data = malloc_type_descriptor_is_pure_data(type_desc);
	bool memtag = _xzm_zone_memtag_block(zone, block_size, data);
	bool canonical_tag = (opt & XZM_MALLOC_CANONICAL_TAG);
	if (!mfm_claimed_address(ptr)) {
		if (memtag && !canonical_tag) {
			xzm_debug_assert(memtag_strip_address(ptr) != ptr);
		} else {
			xzm_debug_assert(memtag_strip_address(ptr) == ptr);
		}
	}
#endif // CONFIG_MTE
#endif // DEBUG
	return ptr;
}

MALLOC_NOINLINE
void *
xzm_malloc(xzm_malloc_zone_t zone, size_t size,
		malloc_type_descriptor_t type_desc, xzm_malloc_options_t opt)
{
	return xzm_malloc_inline(zone, size, type_desc, opt);
}

// mimalloc: mi_heap_malloc_aligned
static void * __alloc_align(2) __alloc_size(3)
_xzm_memalign(xzm_malloc_zone_t zone, size_t alignment, size_t size,
		malloc_type_descriptor_t type_desc, xzm_malloc_options_t opt)
{
	// - Assumption: alignment is a power of 2
	// - Assumption: all xzm size classes are aligned to their MSb/4
	//   (e.g. the 3548-byte size class is aligned to 2048/4=512)
	// - Assumption: all power-of-2 size classes less than or equal to page size
	//   are naturally aligned
	//
	// With those, the following definition of a memalign function will always
	// return properly aligned memory (for values of size/align <= page size)
	// memalign(align, size) =
	// { alloc(align)                    ,              size <= align
	// { alloc(2 * align)                ,     align <  size <= 2 * align
	// { alloc(roundup(size, 4 * align)) , 2 * align <  size <  4 * align
	// { alloc(size)                     , 4 * align <= size
	//
	// The mimalloc memalign implementation (roundup(alloc(size + align - 1)))
	// has a fixed internal fragmentation of align - 1. For the definition
	// above, the internal fragmentation is:
	// { align - size                          ,              size <= align
	// { 2 * align - size                      ,     align <  size <= 2 * align
	// { roundup(size, 4 * align) <= 2 * align , 2 * align <  size <  4 * align
	// { 0                                     , 4 * align <= size
	//
	// In all but the third case, the internal fragmentation should be strictly
	// better than the original implementation. An example of the third case
	// being worse is memalign(2k, 5k), which will require an 8k allocation
	// under the new scheme, but a 7k allocation under the original.
	// Empirically this case seems rare enough that wins from the other three
	// cases make up for it.

	xzm_debug_assert(powerof2(alignment)); // should be guaranteed

	// The early allocator doesn't support alignment
	opt |= XZM_MALLOC_NO_MFM;

	void *ptr;
	if (size > XZM_SMALL_BLOCK_SIZE_MAX ||
			alignment > XZM_SEGMENT_SLICE_SIZE) {
		ptr = _xzm_malloc_large_huge(zone, size, alignment, type_desc, opt);
	} else if (size <= alignment) {
		xzm_debug_assert(alignment <= XZM_SMALL_BLOCK_SIZE_MAX);
		ptr = xzm_malloc(zone, alignment, type_desc, opt);
	} else if (size <= 2 * alignment) {
		xzm_debug_assert(2 * alignment <= XZM_SMALL_BLOCK_SIZE_MAX);
		ptr = xzm_malloc(zone, 2 * alignment, type_desc, opt);
	} else if (size < 4 * alignment) {
		xzm_debug_assert(roundup(size, 4 * alignment) <=
				XZM_SMALL_BLOCK_SIZE_MAX);
		ptr = xzm_malloc(zone, roundup(size, 4 * alignment), type_desc, opt);
	} else {
		xzm_debug_assert(size <= XZM_SMALL_BLOCK_SIZE_MAX);
		ptr = xzm_malloc(zone, size, type_desc, opt);
	}

	xzm_debug_assert(ptr);
	xzm_debug_assert((uintptr_t)ptr % alignment == 0);
	return ptr;
}

void *
xzm_memalign(xzm_malloc_zone_t zone, size_t alignment, size_t size,
		malloc_type_descriptor_t type_desc, xzm_malloc_options_t opt)
{
	return _xzm_memalign(zone, alignment, size, type_desc, opt);
}

#pragma mark Small deallocation

static void
_xzm_xzone_chunk_free(xzm_malloc_zone_t zone, xzm_xzone_t xz, xzm_chunk_t chunk,
	bool small_madvise_needed);

static void
_xzm_xzone_small_chunks_mark_empty(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t *chunks, size_t chunk_count)
{
	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
	xzm_isolation_zone_t iz = &main->xzmz_isolation_zones[xz->xz_idx];
	const bool sequestered = xz->xz_sequestered;

	if (sequestered) {
		_malloc_lock_lock(&iz->xziz_lock);
	}

	for (size_t i = 0; i < chunk_count; ++i) {
		xzm_chunk_t chunk = chunks[i];

		// See _xzm_xzone_chunk_free
		chunk->xzc_mzone_idx = XZM_MZONE_INDEX_INVALID;

		if (sequestered) {
			// Reset such that the chunk is ready for next use immediately
			_xzm_chunk_reset_free(xz, chunk, true);

			// This chunk is no longer pristine
			chunk->xzc_bits.xzcb_is_pristine = false;

			LIST_INSERT_HEAD(&iz->xziz_chunkq, chunk, xzc_entry);
		} else {
			// Full reset - the chunk's span may be reused for anything
			_xzm_chunk_reset_free(xz, chunk, false);

			// Do not madvise again
			xzm_segment_group_t sg = _xzm_segment_group_for_slice(zone,
					chunk);
			xzm_segment_group_free_chunk(sg, chunk, false, false);
		}
	}

	if (sequestered) {
		_malloc_lock_unlock(&iz->xziz_lock);
	}
}

static xzm_block_t
_xzm_xzone_free_to_chunk(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk, xzm_block_t block)
{
#if CONFIG_MTE
	const bool memtag_enabled = xz->xz_tagged;
#endif

	switch(chunk->xzc_bits.xzcb_kind) {
	case XZM_SLICE_KIND_SMALL_CHUNK:
#if CONFIG_MTE
		if (memtag_enabled) {
			block = (xzm_block_t)memtag_strip_address((uint8_t *)block);
		}
#endif
		chunk->xzc_free |= (1u << _xzm_chunk_block_index(zone, chunk, block));
		break;
	default:
		xzm_abort_with_reason("Attempting to free to non-chunk slice",
				(unsigned int)chunk->xzc_bits.xzcb_kind);
	}

	chunk->xzc_used--;

	return block;
}

static void
_xzm_xzone_chunk_free(xzm_malloc_zone_t zone, xzm_xzone_t xz, xzm_chunk_t chunk,
		bool small_madvise_needed)
{
	xzm_segment_group_t sg = _xzm_segment_group_for_slice(zone, chunk);

	// Reset mzone_idx first to "unpublish" the chunk for the enumerator
	// protocol
	//
	// TODO: compiler barrier
	chunk->xzc_mzone_idx = XZM_MZONE_INDEX_INVALID;

	if (xz->xz_sequestered) {
		xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
		if (_xzm_chunk_should_defer_reclamation(main, chunk)) {
#if CONFIG_XZM_DEFERRED_RECLAIM
			if (_xzm_segment_slice_is_deferred(_xzm_segment_for_slice(zone,
					chunk), chunk)) {
				// The only reason a chunk should already be in the reclaim
				// buffer when passed to this function is if it's a tiny chunk
				// and the zone is being destroyed, or it's a small chunk that's
				// being freed from the batch
				xzm_debug_assert(
					((chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_TINY_CHUNK ||
					 chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK) &&
					 chunk->xzc_atomic_meta.xca_alloc_head == XZM_FREE_MADVISED) ||
					(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_CHUNK &&
					 !chunk->xzc_bits.xzcb_on_partial_list));
			} else {
				xzm_chunk_mark_free(zone, chunk);
			}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		} else {
			bool should_madvise = true;
			if (chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_TINY_CHUNK ||
					chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK) {
				if (chunk->xzc_atomic_meta.xca_alloc_head == XZM_FREE_MADVISED) {
					// The chunk is already madvised, no point doing it again
					should_madvise = false;
				} else {
					chunk->xzc_atomic_meta.xca_alloc_head = XZM_FREE_MADVISED;
					chunk->xzc_atomic_meta.xca_free_count = 0;
				}
			} else if (chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_CHUNK) {
				should_madvise = small_madvise_needed;
			} else {
				xzm_abort_with_reason("Unexpected chunk kind",
						chunk->xzc_bits.xzcb_kind);
			}

			if (should_madvise) {
				xzm_segment_group_segment_madvise_chunk(sg, chunk);
			}
		}

		// Reset such that the chunk is ready for next use immediately
		_xzm_chunk_reset_free(xz, chunk, true);

		// chunk->xzc_used == 0
		// xzc_xzone_idx unchanged
		// xzcb_kind unchanged
		// This chunk is no longer pristine, so bumping doesn't guarantee that
		// the allocation is cleared
		chunk->xzc_bits.xzcb_is_pristine = false;

		if (chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_TINY_CHUNK ||
				chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK) {
			// Tiny chunks are only freed during zone destruction, so we
			// can access the atomic metadata directly, since we're the
			// only thread with access to this zone
			chunk->xzc_atomic_meta.xca_on_partial_list = false;
			chunk->xzc_atomic_meta.xca_on_empty_list = true;
			chunk->xzc_atomic_meta.xca_alloc_idx = XZM_SLOT_INDEX_EMPTY;
		}

		xzm_isolation_zone_t iz = &main->xzmz_isolation_zones[xz->xz_idx];

		_malloc_lock_lock(&iz->xziz_lock);
		LIST_INSERT_HEAD(&iz->xziz_chunkq, chunk, xzc_entry);
		_malloc_lock_unlock(&iz->xziz_lock);
	} else {
#if CONFIG_XZM_DEFERRED_RECLAIM
		xzm_debug_assert(
				*_xzm_slice_meta_reclaim_id(zone, chunk) == VM_RECLAIM_ID_NULL);
#endif // CONFIG_XZM_DEFERRED_RECLAIM

		// Full reset - the chunk's span may be reused for anything
		_xzm_chunk_reset_free(xz, chunk, false);

		// will madvise if needed
		xzm_segment_group_free_chunk(sg, chunk, false, small_madvise_needed);
	}
}

// Attempt to madvise any slices now freed by a given block
static void
_xzm_xzone_chunk_madvise_free_slices(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk, xzm_block_t block)
{
	xzm_debug_assert(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_CHUNK);

	xzm_segment_group_t sg = _xzm_segment_group_for_slice(zone, chunk);

	// Only multi-slice chunks that use a bitmap freelist may
	// madvise individual body slices
	if (chunk->xzc_bits.xzcb_kind != XZM_SLICE_KIND_SMALL_CHUNK ||
			_xzm_segment_group_uses_deferred_reclamation(sg)) {
		// TODO: Support for aggressive deferred reclamation of small
		// slices (rdar://112088639)
		return;
	}

#if CONFIG_MTE
	if (xz->xz_tagged) {
		block = (xzm_block_t)memtag_strip_address((uint8_t *)block);
	}
#endif // CONFIG_MTE

	xzm_slice_count_t slice_idx, num_slices;
	const xzm_segment_t segment = _xzm_segment_for_slice(zone, chunk);
	const xzm_slice_count_t chunk_idx = _xzm_slice_index(segment, chunk);
	const xzm_block_index_t block_idx = _xzm_chunk_block_index(zone, chunk,
			block);
	const size_t block_size = _xzm_chunk_block_size(zone, chunk);
	_xzm_chunk_block_free_slices_on_deallocate(chunk, chunk_idx,
			xz->xz_chunk_capacity, block_idx, block_size, &slice_idx,
			&num_slices);
	// At least one slice is madvisable
	if (num_slices > 0) {
		xzm_debug_assert(slice_idx >= chunk_idx);
		xzm_segment_group_segment_madvise_span(sg, _xzm_segment_start(segment) +
				slice_idx * XZM_SEGMENT_SLICE_SIZE, num_slices);
	}
}

static void
_xzm_xzone_batch_small_push(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk, size_t batch_size)
{
#ifdef DEBUG
	_malloc_lock_assert_owner(&xz->xz_lock);
#endif

	xzm_chunk_t batch = NULL;
	// Empty the batch if it is full
	xzm_debug_assert(xz->xz_chunkq_batch_count <= batch_size);
	if (os_unlikely(xz->xz_chunkq_batch_count >= batch_size)) {
		batch = xz->xz_chunkq_batch;
		xz->xz_chunkq_batch = NULL;
		xz->xz_chunkq_batch_count = 0;
	}

	// Insert this chunk at the head of the batch
	*_xzm_segment_slice_meta_batch_next(zone, chunk) = xz->xz_chunkq_batch;
	xz->xz_chunkq_batch = chunk;
	++xz->xz_chunkq_batch_count;

	// Perform the madvise outside the zone lock
	_malloc_lock_unlock(&xz->xz_lock);

	if (batch) {
		_xzm_xzone_madvise_batch(zone, xz, batch);
	}
}

MALLOC_NOINLINE
static void
_xzm_xzone_free_block_to_small_chunk(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_chunk_t chunk, xzm_block_t block)
{
	xzm_allocation_index_t alloc_idx = os_atomic_load(&chunk->xzc_alloc_idx,
			relaxed);

	while (true) {
		if (alloc_idx == XZM_SLOT_INDEX_EMPTY) {
			// This chunk does not appear to be installed in a slot, so we
			// should first try to take its chunk lock.
			_malloc_lock_lock(&chunk->xzc_lock);

			// It's possible that this chunk was installed while we were waiting
			// for the lock.
			alloc_idx = os_atomic_load(&chunk->xzc_alloc_idx, relaxed);
			if (alloc_idx != XZM_SLOT_INDEX_EMPTY) {
				// We lost the race; this chunk was installed to a slot while
				// we were taking its lock.  Unlock it and try again.
				_malloc_lock_unlock(&chunk->xzc_lock);
				continue;
			}

			// The chunk is still not installed, so we hold the correct lock to
			// free it.
			bool chunk_was_full = _xzm_chunk_is_full(zone, xz, chunk);
			block = _xzm_xzone_free_to_chunk(zone, xz, chunk, block);

			// TODO: consider dropping the chunk lock while
			// madvising (additional madvise synchronization
			// required)
			_xzm_xzone_chunk_madvise_free_slices(zone, xz, chunk, block);
			if (_xzm_chunk_is_empty(zone, xz, chunk)) {
				// This chunk has become empty, so we need to go to the xzone to
				// remove it from the partial list.

				// We've just freed the last block and this chunk wasn't
				// installed, so it must be on the partial list (it can't be in
				// the full -> partial transition because that's done under both
				// the chunk and xzone locks).
				xzm_debug_assert(chunk->xzc_bits.xzcb_on_partial_list);

				// We need to acquire the xzone lock to remove this chunk from
				// the partial list.  We need to drop the chunk lock to avoid a
				// lock inversion with code iterating the partial list looking
				// for a chunk to reuse.  Because this chunk is empty, it's
				// guaranteed not to be reused, but it may be removed from the
				// partial list by the time we get there.
				_malloc_lock_unlock(&chunk->xzc_lock);

				_malloc_lock_lock(&xz->xz_lock);

				// Remove from the partial list, if it's still on it.
				if (chunk->xzc_bits.xzcb_on_partial_list) {
					LIST_REMOVE(chunk, xzc_entry);
					chunk->xzc_bits.xzcb_on_partial_list = false;
				}

				xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
				const uint8_t batch_size = main->xzmz_batch_size;
				if (batch_size) {
					// Enqueue the chunk into the batch, and drop the lock.
					_xzm_xzone_batch_small_push(zone, xz, chunk, batch_size);
				} else {
					// We're done with everything we need from the xzone now.
					_malloc_lock_unlock(&xz->xz_lock);

					// madvise and sequester or release back to the segment
					_xzm_xzone_chunk_free(zone, xz, chunk, false);
				}
			} else if (chunk_was_full) {
				// This chunk was full, so we need to go to the xzone to put it
				// on the partial list.  Full chunks that aren't installed to a
				// slot are guaranteed to be on the full list.  Because this
				// chunk is on the full list, no other threads will be trying to
				// acquire its lock while holding the xzone lock.  It is
				// therefore safe to take the xzone lock here while holding the
				// chunk lock.
				_malloc_lock_lock(&xz->xz_lock);

				// Remove from the full list
				LIST_REMOVE(chunk, xzc_entry);
				// Place on the partial list
				LIST_INSERT_HEAD(&xz->xz_chunkq_partial, chunk, xzc_entry);
				chunk->xzc_bits.xzcb_on_partial_list = true;
				xzm_debug_assert(!_xzm_chunk_is_full(zone, xz, chunk));

				_malloc_lock_unlock(&xz->xz_lock);
				_malloc_lock_unlock(&chunk->xzc_lock);
			} else {
				// Neither empty nor newly unfull, so just unlock.
				_malloc_lock_unlock(&chunk->xzc_lock);
			}
			break;
		} else {
			// This chunk appears to be installed in a slot, so we should first
			// try to lock that slot.

			xzm_xzone_allocation_slot_t xas =
					_xzm_xzone_allocation_slot_for_index(zone, xz, alloc_idx - 1);

			_malloc_lock_lock(&xas->xas_lock);

			xzm_allocation_index_t orig_alloc_idx = alloc_idx;

			// It's possible that this chunk was uninstalled (and maybe even
			// installed somewhere else) while we were waiting for the lock.
			alloc_idx = os_atomic_load(&chunk->xzc_alloc_idx, relaxed);
			if (alloc_idx != orig_alloc_idx) {
				// We lost the race; this chunk was uninstalled from this slot while
				// we were taking its lock.  Unlock it and try again.
				_malloc_lock_unlock(&xas->xas_lock);
				continue;
			}

			// The chunk is installed to the slot we locked, so we can proceed.
			block = _xzm_xzone_free_to_chunk(zone, xz, chunk, block);

			// TODO: consider dropping the AS lock while madvising
			// (additional madvise synchronization required)

			uint64_t thrash_threshold = zone->xzz_small_thrash_threshold;
			uint64_t thrash_limit_size = zone->xzz_small_thrash_limit_size;
			bool thrash_mitigation = (thrash_threshold &&
					xz->xz_block_size < thrash_limit_size);
			if (!thrash_mitigation) {
				_xzm_xzone_chunk_madvise_free_slices(zone, xz, chunk, block);
			}

			if (_xzm_chunk_is_empty(zone, xz, chunk)) {
				if (thrash_mitigation) {
					uint64_t now = mach_absolute_time();
					uint64_t time_elapsed = now - xas->xas_last_chunk_empty_ts;
					xas->xas_last_chunk_empty_ts = now;

					if (time_elapsed < thrash_threshold) {
						// We seem to be thrashing, so leave the chunk in place
						_malloc_lock_unlock(&xas->xas_lock);
						break;
					}
				}

				// This chunk has become empty and there are other partial
				// pages available, so uninstall it from the slot.
				xas->xas_chunk = NULL;

				// Since we've just freed the last block and we hold the
				// slot lock, nobody can have a reference to this chunk
				// right now.
				os_atomic_store(&chunk->xzc_alloc_idx, XZM_SLOT_INDEX_EMPTY,
						relaxed);

				// We're done with everything we need from the slot now.
				_malloc_lock_unlock(&xas->xas_lock);

				xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
				const uint8_t batch_size = main->xzmz_batch_size;
				if (batch_size) {
					_malloc_lock_lock(&xz->xz_lock);
					// Enqueue the chunk into the batch, and drop the lock.
					_xzm_xzone_batch_small_push(zone, xz, chunk, batch_size);
				} else {
					bool small_madvise = thrash_mitigation;
					// madvise and sequester or release back to the segment
					_xzm_xzone_chunk_free(zone, xz, chunk, small_madvise);
				}
			} else {
				// The chunk stays installed.
				_malloc_lock_unlock(&xas->xas_lock);
			}
			break;
		}
	}
}

static bool
_xzm_xzone_freelist_chunk_lock(xzm_chunk_t chunk,
		xzm_chunk_atomic_meta_u *old_meta_out)
{
	xzm_chunk_atomic_meta_u old_meta = {
		.xca_value = os_atomic_load_wide(
				&chunk->xzc_atomic_meta.xca_value, dependency),
	};
	xzm_chunk_atomic_meta_u new_meta = { 0 };
	while (true) {
		xzm_debug_assert(!old_meta.xca_walk_locked);
		if (old_meta.xca_alloc_head == XZM_FREE_MADVISING ||
				old_meta.xca_alloc_head == XZM_FREE_MADVISED) {
			return false;
		}

		new_meta = old_meta;
		new_meta.xca_walk_locked = true;
		bool success = os_atomic_cmpxchgv(&chunk->xzc_atomic_meta.xca_value,
				old_meta.xca_value, new_meta.xca_value, &old_meta.xca_value,
				dependency);
		if (!success) {
			continue;
		}

		xzm_trace(walk_lock, (uint64_t)chunk, old_meta.xca_value_lo,
				new_meta.xca_value_lo, 0);
		break;
	}

	*old_meta_out = old_meta;
	return true;
}

static void
_xzm_xzone_freelist_chunk_unlock(xzm_chunk_t chunk,
		xzm_chunk_atomic_meta_u old_meta)
{
	xzm_chunk_atomic_meta_u new_meta = old_meta;
	new_meta.xca_walk_locked = false;
	bool success = os_atomic_cmpxchgv(&chunk->xzc_atomic_meta.xca_value,
				old_meta.xca_value, new_meta.xca_value, &old_meta.xca_value,
				relaxed);
	xzm_assert(success);
	xzm_trace(walk_unlock, (uint64_t)chunk, old_meta.xca_value_lo,
			new_meta.xca_value_lo, 0);
}

MALLOC_NOINLINE
static bool
_xzm_xzone_freelist_chunk_block_is_free_slow(xzm_malloc_zone_t zone,
		xzm_chunk_t chunk, xzm_block_t block)
{
	bool is_free = false;

	// Re-compute the block information so it doesn't need to be passed
	xzm_xzone_t xz = &zone->xzz_xzones[chunk->xzc_xzone_idx];
	size_t granule = (xz->xz_block_size > XZM_TINY_BLOCK_SIZE_MAX) ?
			XZM_SMALL_GRANULE : XZM_GRANULE;
	uint8_t *start = (uint8_t *)_xzm_chunk_start(zone, chunk, NULL);
	uint64_t block_granules = xz->xz_block_size / granule;
	uint64_t block_offset = _xzm_chunk_block_offset(zone, chunk, block);
	uint64_t max_offset = (xz->xz_chunk_capacity - 1) * block_granules;

	uint64_t cur_block_offset = 0;
	size_t freelist_count = 0;

	bool cached = false;

	xzm_chunk_atomic_meta_u old_meta = { 0 };

#if CONFIG_XZM_THREAD_CACHE
	if (!zone->xzz_thread_cache_enabled ||
			xz->xz_block_size > XZM_THREAD_CACHE_THRESHOLD) {
		goto not_cached;
	}

	old_meta.xca_value = os_atomic_load_wide(&chunk->xzc_atomic_meta.xca_value,
			dependency);
	if (old_meta.xca_alloc_idx != XZM_SLOT_INDEX_THREAD_INSTALLED) {
		goto not_cached;
	}

	xzm_thread_cache_t tc = _xzm_get_thread_cache();
	if (!tc) {
		goto not_cached;
	}

	xzm_xzone_index_t xz_idx = xz->xz_idx;
	xzm_xzone_thread_cache_t cache = &tc->xtc_xz_caches[xz_idx];

	if (cache->xztc_head > XZM_FREE_LIMIT || cache->xztc_chunk != chunk) {
		goto not_cached;
	}

	cached = true;

	cur_block_offset = cache->xztc_head;
	freelist_count = 0;
	while (cur_block_offset != XZM_FREE_NULL &&
			cur_block_offset % block_granules == 0 &&
			cur_block_offset <= max_offset &&
			freelist_count < cache->xztc_free_count) {
		if (cur_block_offset == block_offset) {
			// We found our block on the local freelist, so it's free
			is_free = true;
			goto unlocked;
		}

		freelist_count++;

		xzm_block_t cur_block = (xzm_block_t)(
				start + (cur_block_offset * granule));
#if CONFIG_MTE
		if (chunk->xzc_tagged) {
			cur_block = (xzm_block_t)memtag_fixup_ptr((void *)cur_block);
		}
#endif
		cur_block_offset = cur_block->xzb_linkage.xzbl_next_offset;
	}

	bool local_valid = (cur_block_offset == XZM_FREE_NULL &&
			freelist_count <= cache->xztc_free_count);
	if (os_unlikely(!local_valid)) {
		xzm_client_abort_with_reason(
				"corrupt tiny local freelist, client likely has a buffer"
				" overflow or use-after-free bug", freelist_count);
	}

	// The local freelist has a bump offset.  If the given block is above it, it
	// must be free.
	//
	// N.B. Although we can check this here, if the block really is free because
	// it's above the bump we might not get here in the first place, but it
	// isn't guaranteed to have its free cookie set.
	uint64_t local_bump_offset = (xz->xz_chunk_capacity -
			(cache->xztc_free_count - freelist_count)) * block_granules;
	if (block_offset >= local_bump_offset) {
		is_free = true;
		goto unlocked;
	}

not_cached:
#endif // CONFIG_XZM_THREAD_CACHE

	_malloc_lock_lock(&zone->xzz_lock);

	bool active = _xzm_xzone_freelist_chunk_lock(chunk, &old_meta);
	if (!active) {
		is_free = true;
		goto unlock;
	}

	cur_block_offset = old_meta.xca_alloc_head;
	freelist_count = 0;
	while (cur_block_offset != XZM_FREE_NULL &&
			cur_block_offset % block_granules == 0 &&
			cur_block_offset <= max_offset &&
			freelist_count < old_meta.xca_free_count) {
		if (cur_block_offset == block_offset) {
			// We found our block on the freelist, so it's free.
			is_free = true;
			goto chunk_walk_unlock;
		}

		freelist_count++;

		xzm_block_t cur_block = (xzm_block_t)(
				start + (cur_block_offset * granule));
#if CONFIG_MTE
		if (chunk->xzc_tagged) {
			cur_block = (xzm_block_t)memtag_fixup_ptr((void *)cur_block);
		}
#endif
		cur_block_offset = cur_block->xzb_linkage.xzbl_next_offset;
	}

	if (cached) {
		// If this chunk is cached, we should have fully walked the remote
		// freelist
		if (os_unlikely(freelist_count != old_meta.xca_free_count)) {
			xzm_client_abort_with_reason(
					"corrupt tiny remote freelist, client likely has a buffer"
					" overflow or use-after-free bug", freelist_count);
		}
	} else {
		// If this chunk isn't cached, we should have walked the remote freelist
		// to its terminator, which may be the bump
		if (os_unlikely(cur_block_offset != XZM_FREE_NULL)) {
			xzm_client_abort_with_reason(
					"corrupt tiny freelist, client likely has a buffer overflow"
					" or use-after-free bug", freelist_count);
		}

		// If there's a bump and the block is above it, it's free.
		//
		// N.B. As above, although we can check this here, if the block really
		// is free because it's above the bump we might not get here in the
		// first place, but it isn't guaranteed to have its free cookie set.
		xzm_debug_assert(freelist_count <= old_meta.xca_free_count);

		uint64_t bump_offset = (xz->xz_chunk_capacity -
				(old_meta.xca_free_count - freelist_count)) * block_granules;
		if (block_offset >= bump_offset) {
			is_free = true;
			goto chunk_walk_unlock;
		}
	}

chunk_walk_unlock:
	old_meta.xca_walk_locked = true;
	_xzm_xzone_freelist_chunk_unlock(chunk, old_meta);

unlock:
	_malloc_lock_unlock(&zone->xzz_lock);
#if CONFIG_XZM_THREAD_CACHE
unlocked:
#endif
	return is_free;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_xzone_chunk_block_is_free(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		xzm_slice_kind_t kind, size_t block_offset, size_t block_size,
		xzm_block_t block)
{
	xzm_debug_assert(chunk->xzc_bits.xzcb_kind == kind);
	switch (kind) {
	case XZM_SLICE_KIND_TINY_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:;
		// We optimize here for the case where the block is _not_ free, as that
		// is what's expected on the fast path during free().  Quick heuristic
		// check first: if the block does not have the free cookie we would
		// expect it to if it were on the freelist, we assume that it's
		// allocated.  Note that this does _not_ work for pointers above the
		// bump, so we'll report them as allocated too - nano also does this, so
		// we have some evidence we can get away with it.
		uint64_t cookie_value;

#if CONFIG_MTE
		if (chunk->xzc_tagged) {
			// We need to TCO for this load because even though we just ldg'd
			// the tag in the block pointer, if we're being asked about a block
			// that doesn't belong to us it might race to get freed and
			// re-tagged while we're looking at it.
			memtag_disable_checking();
			cookie_value = os_atomic_load(&block->xzb_cookie, relaxed);
			memtag_enable_checking();
		} else {
#endif
			cookie_value = os_atomic_load(&block->xzb_cookie, relaxed);
#if CONFIG_MTE
		}
#endif

		uint64_t free_cookie = zone->xzz_freelist_cookie ^ (uint64_t)block;

#if CONFIG_MTE
		if (kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK) {
			// Small chunks do not encode the tag in the freelist cookie
			free_cookie = (uint64_t)memtag_strip_address((uint8_t *)free_cookie);
		}
#endif
		if (os_likely(cookie_value != free_cookie)) {
			return false;
		}

		// If the block _does_ look free, it's still possible that the
		// application just happened to have written the cookie value in the
		// slot.  We have a choice:
		// - Nano chooses to just assume that the block really is free.  This is
		//   a soundness problem, as it could cause free() to fail on a valid
		//   allocation.
		// - We can walk the freelist to see if we can really find the block.
		//   This is sound but relatively costly.
		//
		// For now we take the latter choice, at the risk there's a workload
		// that really hammers on malloc_size() of free blocks a lot.  If we
		// encounter such a workload, we could consider behaving like nano for
		// compatibility, but most uses of malloc_size() on free blocks are
		// incorrect so we would probably tell that workload to change.
		bool is_free = _xzm_xzone_freelist_chunk_block_is_free_slow(zone, chunk,
				block);
		return is_free;
	case XZM_SLICE_KIND_SMALL_CHUNK:;
		size_t block_index = block_offset / block_size;
		return _xzm_small_chunk_block_index_is_free(chunk,
				(xzm_block_index_t)block_index);
	default:
		break;
	}

	return false;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_chunk_t
__xzm_ptr_lookup(xzm_malloc_zone_t zone, xzm_segment_table_entry_s entry,
		const void *orig_ptr, const void *ptr, xzm_xzone_t *xz_out,
		xzm_block_t *block_out, size_t *block_size_out)
{
	xzm_segment_t segment = _xzm_segment_table_entry_to_segment(entry);
	if (!segment) {
		return NULL;
	}

	xzm_chunk_t chunk = _xzm_segment_chunk_of(segment, (uintptr_t)ptr);
	if (os_unlikely(!chunk)) {
		return NULL;
	}

	if (os_unlikely(chunk->xzc_mzone_idx != zone->xzz_mzone_idx)) {
		// The pointer belongs to one of the mzones, but not us.  Depending on
		// the context, we _could_ just go ahead and do what we were asked to
		// (e.g. free) anyway, if we had a way to turn the mzone index back into
		// the pointer, but that would probably be confusing to libmalloc and
		// any wrapper zones, etc, so instead we'll just treat pointers
		// belonging to other mzones as completely foreign.
		return NULL;
	}

	xzm_block_t block = (xzm_block_t)ptr;

	uintptr_t start = _xzm_chunk_start(zone, chunk, NULL);
	size_t block_offset = (uintptr_t)block - start;

	xzm_slice_kind_t kind = chunk->xzc_bits.xzcb_kind;
	xzm_xzone_t xz = NULL;
	size_t block_size = 0;
	if (os_likely(_xzm_slice_kind_uses_xzones(kind))) {
		xz = &zone->xzz_xzones[chunk->xzc_xzone_idx];
		block_size = xz->xz_block_size;

		if (os_unlikely(!XZM_FAST_ALIGNED(block_offset, block_size,
				xz->xz_align_magic))) {
			return NULL;
		}
	} else {
		block_size = ((size_t)chunk->xzcs_slice_count) <<
				XZM_SEGMENT_SLICE_SHIFT;
		size_t ptr_offset = block_offset % block_size;
		if (os_unlikely(ptr_offset)) {
			return NULL;
		}
	}

#ifdef DEBUG
	size_t block_index = block_offset / block_size;
	xzm_debug_assert(!_xzm_slice_kind_uses_xzones(chunk->xzc_bits.xzcb_kind) ||
			block_index < xz->xz_chunk_capacity);
#endif

#if CONFIG_MTE
	const bool memtag_enabled = _xzm_zone_memtag_enabled(zone);
	if (memtag_enabled) {
		// Load the tag for the block pointer we materialized
		block = (xzm_block_t)memtag_fixup_ptr((void *)block);

		// If the tag of the block doesn't match the tag we started with, we
		// consider it not allocated
		if (os_unlikely(!memtag_tags_match(orig_ptr, block))) {
			return NULL;
		}
	}
#endif

	if (os_unlikely(_xzm_xzone_chunk_block_is_free(zone, chunk, kind,
				block_offset, block_size, block))) {
		return NULL;
	}

	if (xz_out) {
		*xz_out = xz;
	}

	if (block_out) {
		*block_out = block;
	}
	if (block_size_out) {
		*block_size_out = block_size;
	}

	return chunk;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_chunk_t
_xzm_ptr_lookup(xzm_malloc_zone_t zone, const void *ptr, xzm_xzone_t *xz_out,
		xzm_block_t *block_out, size_t *block_size_out)
{
	const void *orig_ptr = ptr;

#if CONFIG_MTE
	// Strip the pointer to use it in pointer arithmetic operations
	ptr = memtag_strip_address((uint8_t *)ptr);
#endif

	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
	xzm_segment_table_entry_s *entry_p = _xzm_ptr_to_table_entry(ptr, main);
	if (os_unlikely(!entry_p)) {
		return NULL;
	}

	return __xzm_ptr_lookup(zone, *entry_p, orig_ptr, ptr, xz_out, block_out,
			block_size_out);
}

MALLOC_NOINLINE
static void
_xzm_free_not_found(xzm_malloc_zone_t zone, void *ptr, bool try)
{
	if (_xzm_malloc_zone_is_main(zone) && mfm_claimed_address(ptr)) {
		mfm_free(ptr);
		return;
	}

	if (!try) {
#if CONFIG_MTE
		// When tagging is enabled (based on malloc_has_sec_transition), we will
		// check if the logical tag of the pointer matches the tag stored in
		// memory. We need to do this here, in xzone, to make sure that we
		// validate the tag in the malloc_zone_free() path. All other paths
		// should be handled at the dispatch layer (malloc proper).
#endif // CONFIG_MTE
		malloc_report_pointer_was_not_allocated(
				MALLOC_REPORT_CRASH | MALLOC_REPORT_NOLOG, ptr);
	}

	find_zone_and_free(ptr, true);
}

MALLOC_NOINLINE
static void
_xzm_free_outlined(xzm_malloc_zone_t zone, void *ptr, bool try,
		xzm_segment_table_entry_s entry)
{
	void *orig_ptr = ptr;

#if CONFIG_MTE
	// Strip the pointer to use it in pointer arithmetic operations
	ptr = memtag_strip_address((uint8_t *)ptr);
#endif

	xzm_xzone_t xz = NULL;
	xzm_block_t block = NULL;
	xzm_chunk_t chunk = __xzm_ptr_lookup(zone, entry, orig_ptr, ptr, &xz,
			&block, NULL);
	if (os_unlikely(!chunk)) {
		return _xzm_free_not_found(zone, orig_ptr, try);
	}

	if (chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_TINY_CHUNK ||
			chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK) {
		_xzm_xzone_free_freelist(zone, xz, chunk, block);
	} else if (chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_CHUNK) {
		_xzm_xzone_free_block_to_small_chunk(zone, xz, chunk, block);
	} else {
		xzm_debug_assert(!xz);
		_xzm_free_large_huge(zone, chunk);
	}
	return;
}

#ifdef DEBUG
static void
_xzm_debug_validate_chunk_metadata(xzm_xzone_t xz, xzm_chunk_t chunk)
{
	xzm_debug_assert(xz->xz_block_size == chunk->xzc_freelist_block_size);
	xzm_debug_assert(xz->xz_chunk_capacity == chunk->xzc_freelist_chunk_capacity);
#if CONFIG_MTE
	xzm_debug_assert(xz->xz_tagged == chunk->xzc_tagged);
#endif
}
#else
#define _xzm_debug_validate_chunk_metadata(...)
#endif

// mimalloc: mi_free
static void
_xzm_free(xzm_malloc_zone_t zone, void *ptr, bool try)
{
	if (os_unlikely(!ptr)) {
		return;
	}

	void *orig_ptr = ptr;

	// Inline fast path for tiny free

#if CONFIG_MTE
	// Strip the pointer to use it in pointer arithmetic operations
	ptr = memtag_strip_address((uint8_t *)ptr);
#endif

	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
	xzm_segment_table_entry_s *entry_p = _xzm_ptr_to_table_entry(ptr, main);
	if (os_unlikely(!entry_p)) {
		goto not_found;
	}

	xzm_segment_table_entry_s entry = *entry_p;
	if (os_unlikely(!entry.xste_normal)) {
		goto outlined;
	}

	xzm_segment_t segment = _xzm_segment_table_entry_to_segment(entry);
	uintptr_t offset_in_segment = (uintptr_t)orig_ptr & XZM_SEGMENT_MASK;
	uint64_t slice_idx = offset_in_segment >> XZM_SEGMENT_SLICE_SHIFT;
	xzm_slice_t slice = &segment->xzs_slices[slice_idx];

	if (os_unlikely(slice->xzc_bits.xzcb_kind != XZM_SLICE_KIND_TINY_CHUNK)) {
		goto outlined;
	}

	xzm_chunk_t chunk = (xzm_chunk_t)slice;
	if (os_unlikely(chunk->xzc_mzone_idx != zone->xzz_mzone_idx)) {
		goto not_found;
	}

	xzm_xzone_index_t xz_idx = chunk->xzc_xzone_idx;
	__assert_only xzm_xzone_t xz = &zone->xzz_xzones[xz_idx];
	_xzm_debug_validate_chunk_metadata(xz, chunk);

	size_t block_size = chunk->xzc_freelist_block_size;
	size_t block_offset = (uintptr_t)orig_ptr & XZM_SEGMENT_SLICE_MASK;
	if (os_unlikely(block_offset % block_size)) {
		goto not_found;
	}

	// Check if block is free

	xzm_block_t block = (xzm_block_t)orig_ptr;

#if CONFIG_MTE
	const bool tagged = chunk->xzc_tagged;
	if (tagged) {
		block = (xzm_block_t)memtag_fixup_ptr((void *)ptr);

		if (os_unlikely(!memtag_tags_match(orig_ptr, block))) {
			goto not_found;
		}
	}
#endif

	uint64_t cookie_value = os_atomic_load(&block->xzb_cookie, relaxed);
	uint64_t free_cookie = zone->xzz_freelist_cookie ^ (uint64_t)orig_ptr;
	if (os_unlikely(cookie_value == free_cookie)) {
		goto outlined;
	}

	if (block_size > sizeof(struct xzm_block_inline_meta_s) &&
			block_size <= XZM_ZERO_ON_FREE_THRESHOLD) {
		bzero(block, block_size);
	}

#if CONFIG_MTE
	if (tagged) {
		block = _xzm_xzone_block_memtag_retag(zone, block, block_size);
		uint8_t tag = memtag_extract_tag((uint8_t *)block);
		free_cookie = (uint64_t)memtag_mix_tag(
				memtag_strip_address((uint8_t *)free_cookie), tag);
	}
#endif

	// Mark it as free now
	os_atomic_store(&block->xzb_cookie, free_cookie, relaxed);

	uint16_t block_ref = block_offset / XZM_GRANULE;

#if CONFIG_XZM_THREAD_CACHE
	if (block_size > XZM_THREAD_CACHE_THRESHOLD ||
			!zone->xzz_thread_cache_enabled) {
		goto not_cached;
	}

	xzm_thread_cache_t tc = _xzm_get_thread_cache();
	if (!tc) {
		goto not_cached;
	}

	xzm_xzone_thread_cache_t cache = &tc->xtc_xz_caches[xz_idx];
	if (cache->xztc_head > XZM_FREE_LIMIT || cache->xztc_chunk != chunk) {
		goto not_cached;
	}

	return _xzm_xzone_thread_cache_free_tiny(zone, cache, block, block_ref);

not_cached:
#endif // CONFIG_XZM_THREAD_CACHE
	return _xzm_xzone_free_freelist_inline(zone, xz_idx, chunk, block,
			block_ref, block_size, chunk->xzc_freelist_chunk_capacity);

outlined:
	return _xzm_free_outlined(zone, orig_ptr, try, entry);

not_found:
	return _xzm_free_not_found(zone, orig_ptr, try);
}

MALLOC_NOINLINE
static size_t
_xzm_ptr_size_outlined(xzm_malloc_zone_t zone, const void *ptr)
{
	// TODO: As part of rdar://101779148, check the mzone that this early
	// allocation came from
	if (_xzm_malloc_zone_is_main(zone) && mfm_claimed_address((void *)ptr)) {
		return mfm_alloc_size(ptr);
	}

	return 0;
}

// mimalloc: _mi_usable_size
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static size_t
_xzm_ptr_size(xzm_malloc_zone_t zone, const void *ptr, xzm_chunk_t *out_chunk)
{
	xzm_block_t block;
	size_t block_size;
	xzm_chunk_t chunk = _xzm_ptr_lookup(zone, ptr, NULL, &block, &block_size);
	if (out_chunk) {
		*out_chunk = chunk;
	}

	if (!chunk) {
		// Might be an early allocation, or a huge aligned allocation
		return _xzm_ptr_size_outlined(zone, ptr);
	}

	if (os_likely((void *)block == ptr)) {
		return block_size;
	}

	// Aligned allocation
	size_t block_offset = (uint8_t *)ptr - (uint8_t *)block;
	return block_size - block_offset;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void *
_xzm_realloc_inner(xzm_malloc_zone_t zone, void *ptr, size_t new_size,
		malloc_type_descriptor_t type_desc)
{
	xzm_debug_assert(ptr);
	xzm_debug_assert(new_size);

	xzm_chunk_t chunk;
	size_t old_size = _xzm_ptr_size(zone, ptr, &chunk);
	void *old_ptr = ptr;

	if (old_size == 0) {
#if CONFIG_MTE
		// This implicitly includes validation of the pointer tag when memory
		// tagging is enabled. We need this here to cover the realloc path of
		// malloc_zone_realloc(), which doesn't go through _realloc().
#endif
		malloc_report_pointer_was_not_allocated(
				MALLOC_REPORT_CRASH | MALLOC_REPORT_NOLOG, ptr);
	}

	if (chunk && os_unlikely(chunk->xzc_mzone_idx != zone->xzz_mzone_idx)) {
		xzm_client_abort_with_reason(
				"pointer zone mismatch, client may be passing the wrong malloc"
				" zone", ptr);
	}

	if (chunk && old_size > XZM_SMALL_BLOCK_SIZE_MAX
			&& new_size > XZM_SMALL_BLOCK_SIZE_MAX) {
		xzm_slice_count_t new_slice_count = (xzm_slice_count_t)
				(roundup(new_size,
				XZM_SEGMENT_SLICE_SIZE) / XZM_SEGMENT_SLICE_SIZE);

		xzm_segment_t segment = _xzm_segment_for_slice(zone, chunk);
		xzm_debug_assert(segment != NULL);

		bool realloc_in_place = false;
		if (old_size > XZM_LARGE_BLOCK_SIZE_MAX &&
				new_size > XZM_LARGE_BLOCK_SIZE_MAX &&
				os_likely((zone->xzz_flags & MALLOC_PURGEABLE) == 0)) {
			realloc_in_place = xzm_segment_group_try_realloc_huge_chunk(
					segment->xzs_segment_group, zone, segment, chunk,
					new_slice_count);
		} else if (old_size <= XZM_LARGE_BLOCK_SIZE_MAX &&
				segment->xzs_kind == XZM_SEGMENT_KIND_NORMAL &&
				new_size <= XZM_LARGE_BLOCK_SIZE_MAX &&
				os_likely((zone->xzz_flags & MALLOC_PURGEABLE) == 0)) {
			realloc_in_place = xzm_segment_group_try_realloc_large_chunk(
					segment->xzs_segment_group, segment, chunk,
					new_slice_count);
		}

		if (realloc_in_place) {
			return old_ptr;
		}
	}

	if (new_size <= old_size && new_size >= (old_size / 2)) {
		// reallocation still fits and not more than 50% waste
		// TODO: revisit?
		return old_ptr;
	}

	void *new_ptr = xzm_malloc(zone, new_size, type_desc, 0);
	if (os_unlikely(!new_ptr)) {
		return NULL;
	}

	const size_t valid_size = MIN(old_size, new_size);
#if CONFIG_REALLOC_CAN_USE_VMCOPY
	// When supported, request a CoW mapping
	if (valid_size > XZM_LARGE_BLOCK_SIZE_MAX) {
		if (mach_vm_copy(mach_task_self(), (mach_vm_address_t)old_ptr,
				valid_size, (mach_vm_address_t)new_ptr)	== KERN_SUCCESS) {
			return new_ptr;
		}
	}
#endif
	memcpy(new_ptr, old_ptr, valid_size);

	return new_ptr;
}

void *
xzm_realloc(xzm_malloc_zone_t zone, void *ptr, size_t new_size,
		malloc_type_descriptor_t type_desc)
{
	if (!ptr) {
		return xzm_malloc(zone, new_size, type_desc, 0);
	} else if (new_size == 0) {
		_xzm_free(zone, ptr, false);
		return xzm_malloc(zone, new_size, type_desc, 0);
	}

	void *new_ptr = _xzm_realloc_inner(zone, ptr, new_size, type_desc);
	if (new_ptr && new_ptr != ptr) {
		_xzm_free(zone, ptr, false);
	}

	return new_ptr;
}

#pragma mark libmalloc zone

static size_t
xzm_malloc_zone_size(xzm_malloc_zone_t zone, const void *ptr)
{
	return _xzm_ptr_size(zone, ptr, NULL);
}

static boolean_t
xzm_malloc_zone_claimed_address(xzm_malloc_zone_t zone, void *ptr)
{
	// Is it an early allocation?
	if (mfm_claimed_address(ptr)) {
		return true;
	}

#if CONFIG_MTE
	// We strip the pointer here as xzm_segment_table_query operates on
	// canonical pointers; however, since mfm_claimed_address validates
	// the logical tag of the pointer, we need to ensure we don't strip
	// the tag before passing it to MFM.
	// We don't need to check if MTE is enabled, as this doesn't need MTE ISA.
	ptr = memtag_strip_address(ptr);
#endif

	xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
	return xzm_segment_table_query(main, ptr) != NULL;
}

static inline void *
_xzm_malloc_zone_malloc_entry(xzm_malloc_zone_t zone, size_t size,
		malloc_type_id_t type_id, xzm_malloc_options_t options)
{
	return xzm_malloc_inline(zone, size,
			(malloc_type_descriptor_t){ .type_id = type_id, }, options);
}

static void *
xzm_malloc_zone_malloc_type_malloc(xzm_malloc_zone_t zone, size_t size,
		malloc_type_id_t type_id)
{
	return _xzm_malloc_zone_malloc_entry(zone, size, type_id, 0);
}

static void *
xzm_malloc_zone_malloc(xzm_malloc_zone_t zone, size_t size)
{
	return _xzm_malloc_zone_malloc_entry(zone, size, malloc_get_tsd_type_id(),
			_xzm_xzone_get_malloc_thread_options());
}

static inline void *
_xzm_malloc_zone_malloc_type_calloc_entry(xzm_malloc_zone_t zone, size_t count,
		size_t size, malloc_type_id_t type_id,
		xzm_malloc_options_t options)
{
	size_t total_bytes;
	if (os_unlikely(calloc_get_size(count, size, 0, &total_bytes))) {
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
		return NULL;
	}

	return xzm_malloc_inline(zone, total_bytes,
			(malloc_type_descriptor_t){ .type_id = type_id, },
			options | XZM_MALLOC_CLEAR);
}

static void *
xzm_malloc_zone_malloc_type_calloc(xzm_malloc_zone_t zone, size_t count,
		size_t size, malloc_type_id_t type_id)
{
	return _xzm_malloc_zone_malloc_type_calloc_entry(zone, count, size, type_id,
			0);
}

static void *
xzm_malloc_zone_calloc(xzm_malloc_zone_t zone, size_t count, size_t size)
{
	return _xzm_malloc_zone_malloc_type_calloc_entry(zone, count, size,
			malloc_get_tsd_type_id(), _xzm_xzone_get_malloc_thread_options());
}

static void * __alloc_size(2)
xzm_malloc_zone_valloc(xzm_malloc_zone_t zone, size_t size)
{
	return _xzm_memalign(zone, vm_page_quanta_size, size,
			malloc_get_tsd_type_descriptor(), 0);
}

static void
xzm_malloc_zone_free(xzm_malloc_zone_t zone, void *ptr)
{
	_xzm_free(zone, ptr, false);
}

static void
xzm_malloc_zone_try_free_default(xzm_malloc_zone_t zone, void *ptr)
{
	_xzm_free(zone, ptr, true);
}

static void
xzm_malloc_zone_free_definite_size(xzm_malloc_zone_t zone, void *ptr,
		size_t size)
{
	// Unfortunately, knowing the size doesn't really buy us anything.
	(void)size;

	_xzm_free(zone, ptr, false);
}

static void *
xzm_malloc_zone_malloc_type_realloc(xzm_malloc_zone_t zone, void *ptr,
		size_t new_size, malloc_type_id_t type_id)
{
	return xzm_realloc(zone, ptr, new_size,
			(malloc_type_descriptor_t){ .type_id = type_id, });
}

static void *
xzm_malloc_zone_realloc(xzm_malloc_zone_t zone, void *ptr, size_t new_size)
{
	return xzm_malloc_zone_malloc_type_realloc(zone, ptr, new_size,
			malloc_get_tsd_type_id());
}

static inline void *
_xzm_malloc_zone_malloc_type_memalign_entry(xzm_malloc_zone_t zone,
		size_t alignment, size_t size, malloc_type_id_t type_id,
		xzm_malloc_options_t options)
{
	return _xzm_memalign(zone, alignment, size,
			(malloc_type_descriptor_t){ .type_id = type_id, }, options);
}

static void *
xzm_malloc_zone_malloc_type_memalign(xzm_malloc_zone_t zone, size_t alignment,
		size_t size, malloc_type_id_t type_id)
{
	return _xzm_malloc_zone_malloc_type_memalign_entry(zone, alignment, size,
			type_id, 0);
}

static void *
xzm_malloc_zone_memalign(xzm_malloc_zone_t zone, size_t alignment, size_t size)
{
	return _xzm_malloc_zone_malloc_type_memalign_entry(zone, alignment, size,
			malloc_get_tsd_type_id(), _xzm_xzone_get_malloc_thread_options());
}

static void *
xzm_malloc_zone_malloc_type_malloc_with_options(xzm_malloc_zone_t zone,
		size_t align, size_t size, malloc_zone_malloc_options_t options,
		malloc_type_id_t type_id)
{
	xzm_malloc_options_t opt = 0;
	if (options & MALLOC_ZONE_MALLOC_OPTION_CLEAR) {
		opt |= XZM_MALLOC_CLEAR;
	}
	if (options & MALLOC_NP_OPTION_CANONICAL_TAG) {
		opt |= XZM_MALLOC_CANONICAL_TAG;
	}

	malloc_type_descriptor_t type_desc = { .type_id = type_id, };
	if (align > MALLOC_ZONE_MALLOC_DEFAULT_ALIGN) {
		return _xzm_memalign(zone, align, size, type_desc, opt);
	} else {
		return xzm_malloc_inline(zone, size, type_desc, opt);
	}
}

static void *
xzm_malloc_zone_malloc_with_options(xzm_malloc_zone_t zone, size_t align,
		size_t size, malloc_zone_malloc_options_t options)
{
	return xzm_malloc_zone_malloc_type_malloc_with_options(zone, align, size,
			options, malloc_get_tsd_type_id());
}

#pragma mark slowpath zone functions

// nothing for size

// nothing for claimed_address

static void *
xzm_malloc_zone_malloc_type_malloc_slow(xzm_malloc_zone_t zone, size_t size,
		malloc_type_id_t type_id)
{
	void *ptr = NULL;
	malloc_type_descriptor_t type_desc = { .type_id = type_id, };
	xzm_malloc_options_t options = _xzm_xzone_get_malloc_thread_options();

	if ((zone->xzz_flags & MALLOC_PURGEABLE) &&
			(size <= XZM_SMALL_BLOCK_SIZE_MAX)) {
		xzm_malloc_zone_t main = (xzm_malloc_zone_t)_xzm_malloc_zone_main(zone);
		ptr = xzm_malloc_inline(main, size, type_desc, options);
	} else {
		ptr = xzm_malloc_inline(zone, size, type_desc, options);
	}
	if (ptr && (zone->xzz_flags & MALLOC_DO_SCRIBBLE)) {
		memset(ptr, SCRIBBLE_BYTE, size);
	}
	return ptr;
}

static void *
xzm_malloc_zone_malloc_slow(xzm_malloc_zone_t zone, size_t size)
{
	return xzm_malloc_zone_malloc_type_malloc_slow(zone, size,
			malloc_get_tsd_type_id());
}

static void *
xzm_malloc_zone_malloc_type_calloc_slow(xzm_malloc_zone_t zone, size_t count,
		size_t size, malloc_type_id_t type_id)
{
	size_t total_bytes;
	if (calloc_get_size(count, size, 0, &total_bytes)) {
		return NULL;
	}

	malloc_type_descriptor_t type_desc = { .type_id = type_id, };
	xzm_malloc_options_t options = _xzm_xzone_get_malloc_thread_options() |
			XZM_MALLOC_CLEAR;
	if ((zone->xzz_flags & MALLOC_PURGEABLE) == 0 ||
			total_bytes > XZM_SMALL_BLOCK_SIZE_MAX) {
		return xzm_malloc_inline(zone, total_bytes, type_desc, options);
	} else {
		xzm_malloc_zone_t main = (xzm_malloc_zone_t)_xzm_malloc_zone_main(zone);
		return xzm_malloc_inline(main, total_bytes, type_desc, options);
	}
}

static void *
xzm_malloc_zone_calloc_slow(xzm_malloc_zone_t zone, size_t count,
		size_t size)
{
	return xzm_malloc_zone_malloc_type_calloc_slow(zone, count, size,
			malloc_get_tsd_type_id());
}

static void *
xzm_malloc_zone_valloc_slow(xzm_malloc_zone_t zone, size_t size)
{
	void *ptr = NULL;
	if ((zone->xzz_flags & MALLOC_PURGEABLE) &&
			(size <= XZM_SMALL_BLOCK_SIZE_MAX)) {
		xzm_malloc_zone_t main = (xzm_malloc_zone_t)_xzm_malloc_zone_main(zone);
		ptr = xzm_malloc_zone_valloc(main, size);
	} else {
		ptr = xzm_malloc_zone_valloc(zone, size);
	}
	if (ptr && (zone->xzz_flags & MALLOC_DO_SCRIBBLE)) {
		memset(ptr, SCRIBBLE_BYTE, size);
	}
	return ptr;
}

static void
xzm_malloc_zone_free_slow(xzm_malloc_zone_t zone, void *ptr)
{
	if (!ptr) {
		return;
	}

	size_t size = _xzm_ptr_size(zone, ptr, NULL);
	// The main ref stays NULL if this allocation came from zone
	xzm_malloc_zone_t main = NULL;
	if (size == 0 && (zone->xzz_flags & MALLOC_PURGEABLE)) {
		main = (xzm_malloc_zone_t)_xzm_malloc_zone_main(zone);
		size = _xzm_ptr_size(main, ptr, NULL);
	}
	if (size == 0) {
		xzm_client_abort_with_reason("pointer being freed was not allocated",
				ptr);
	}
	if ((zone->xzz_flags & MALLOC_DO_SCRIBBLE) &&
			(size > XZM_ZERO_ON_FREE_THRESHOLD)) {
		memset(ptr, SCRABBLE_BYTE, size);
	}

	if (main) {
		xzm_malloc_zone_free(main, ptr);
	} else {
		xzm_malloc_zone_free(zone, ptr);
	}
}

static void
xzm_malloc_zone_try_free_default_slow(xzm_malloc_zone_t zone, void *ptr)
{
	if (!ptr) {
		return;
	}

	size_t size = _xzm_ptr_size(zone, ptr, NULL);
	xzm_malloc_zone_t main = NULL;
	if (size == 0 && (zone->xzz_flags & MALLOC_PURGEABLE)) {
		main = (xzm_malloc_zone_t)_xzm_malloc_zone_main(zone);
		size = _xzm_ptr_size(main, ptr, NULL);
	}
	if (size == 0) {
		// pointer didn't come from this zone
		find_zone_and_free(ptr, true);
		return;
	}
	if (size && size > XZM_ZERO_ON_FREE_THRESHOLD &&
			(zone->xzz_flags & MALLOC_DO_SCRIBBLE)) {
		memset(ptr, SCRABBLE_BYTE, size);
	}

	if (main) {
		xzm_malloc_zone_try_free_default(main, ptr);
	} else {
		xzm_malloc_zone_try_free_default(zone, ptr);
	}
}

static void
xzm_malloc_zone_free_definite_size_slow(xzm_malloc_zone_t zone, void *ptr,
		size_t size)
{
	if (!ptr) {
		return;
	}

	// TODO: Should we verify that the size the client passes in matches our
	// size?
	size_t our_size = _xzm_ptr_size(zone, ptr, NULL);
	xzm_malloc_zone_t main = NULL;
	if (our_size == 0 && (zone->xzz_flags & MALLOC_PURGEABLE)) {
		main = (xzm_malloc_zone_t)_xzm_malloc_zone_main(zone);
		our_size = _xzm_ptr_size(main, ptr, NULL);
		xzm_debug_assert(our_size != 0);
	}
	if ((zone->xzz_flags & MALLOC_DO_SCRIBBLE) &&
			(size > XZM_ZERO_ON_FREE_THRESHOLD)) {
		memset(ptr, SCRABBLE_BYTE, size);
	}

	if (main) {
		xzm_malloc_zone_free_definite_size(main, ptr, size);
	} else {
		xzm_malloc_zone_free_definite_size(zone, ptr, size);
	}
}

static void *
xzm_malloc_zone_malloc_type_realloc_slow(xzm_malloc_zone_t zone, void *ptr,
		size_t new_size, malloc_type_id_t type_id)
{
	bool purgeable = zone->xzz_flags & MALLOC_PURGEABLE;
	bool scribble = zone->xzz_flags & MALLOC_DO_SCRIBBLE;
	if (!purgeable && !scribble) {
		return xzm_malloc_zone_malloc_type_realloc(zone, ptr, new_size,
				type_id);
	}

	if (!ptr) {
		return xzm_malloc_zone_malloc_type_malloc_slow(zone, new_size, type_id);
	} else if (new_size == 0) {
		xzm_malloc_zone_free_slow(zone, ptr);
		return xzm_malloc_zone_malloc_type_malloc_slow(zone, new_size, type_id);
	}

	malloc_type_descriptor_t type_desc = { .type_id = type_id, };
	size_t old_size = _xzm_ptr_size(zone, ptr, NULL);
	xzm_malloc_zone_t main = NULL;
	if (old_size == 0 && purgeable) {
		main = (xzm_malloc_zone_t)_xzm_malloc_zone_main(zone);
		old_size = _xzm_ptr_size(main, ptr, NULL);
	}
	xzm_assert(old_size);

	if ((old_size > new_size) && scribble) {
		// Shrinking: no matter what, we'll definitely need to scrabble the
		// tail, so do that now
		memset((uint8_t *)ptr + new_size, SCRABBLE_BYTE, old_size - new_size);
	}

	void *new_ptr;
	if (!purgeable || (!main && new_size > XZM_SMALL_BLOCK_SIZE_MAX) ||
			(main && new_size <= XZM_SMALL_BLOCK_SIZE_MAX)) {
		// The old pointer and new pointer will be from the same zone
		new_ptr = _xzm_realloc_inner(zone, ptr, new_size, type_desc);
	} else {
		if (new_size <= XZM_SMALL_BLOCK_SIZE_MAX) {
			// Need to serve the new allocation from the main zone, since it's
			// too small to be purgeable
			new_ptr = xzm_malloc_inline(&_xzm_malloc_zone_main(zone)->xzmz_base,
					new_size, type_desc, 0);
		} else {
			// Serve the new allocation from the purgeable zone
			new_ptr = xzm_malloc_inline(zone, new_size, type_desc, 0);
		}
		if (new_ptr) {
			memcpy(new_ptr, ptr, MIN(old_size, new_size));
		}
	}

	if (new_ptr) {
		// Regardless of whether we reallocated in place or not, we always want
		// to scribble the expanded space when growing
		if ((new_size > old_size) && scribble) {
			memset((uint8_t *)new_ptr + old_size, SCRIBBLE_BYTE,
					new_size - old_size);
		}
		if (new_ptr != ptr) {
			// If we didn't reallocate in place, we have some amount of
			// scrabbling left to do on the old allocation:
			if ((old_size > new_size) && scribble) {
				// If shrinking, we already scrabbled the tail, so just scrabble
				// the head now too
				memset(ptr, SCRABBLE_BYTE, new_size);
			} else if (scribble) {
				// If growing, scrabble the whole previous allocation since we
				// haven't done any of it yet
				memset(ptr, SCRABBLE_BYTE, old_size);
			}
			if (main) {
				_xzm_free(main, ptr, false);
			} else {
				_xzm_free(zone, ptr, false);
			}
		}
	}

	return new_ptr;
}

static void *
xzm_malloc_zone_realloc_slow(xzm_malloc_zone_t zone, void *ptr, size_t new_size)
{
	return xzm_malloc_zone_malloc_type_realloc_slow(zone, ptr, new_size,
			malloc_get_tsd_type_id());
}

static void *
xzm_malloc_zone_malloc_type_memalign_slow(xzm_malloc_zone_t zone, size_t alignment,
		size_t size, malloc_type_id_t type_id)
{
	void *ptr = NULL;
	xzm_malloc_options_t options = _xzm_xzone_get_malloc_thread_options();
	if ((zone->xzz_flags & MALLOC_PURGEABLE) &&
			(size <= XZM_SMALL_BLOCK_SIZE_MAX)) {
		xzm_malloc_zone_t main = (xzm_malloc_zone_t)_xzm_malloc_zone_main(zone);
		ptr = _xzm_malloc_zone_malloc_type_memalign_entry(main, alignment, size,
				type_id, options);
	} else {
		ptr = _xzm_malloc_zone_malloc_type_memalign_entry(zone, alignment, size,
				type_id, options);
	}
	if (ptr && (zone->xzz_flags & MALLOC_DO_SCRIBBLE)) {
		memset(ptr, SCRIBBLE_BYTE, size);
	}
	return ptr;
}

static void *
xzm_malloc_zone_memalign_slow(xzm_malloc_zone_t zone, size_t alignment,
		size_t size)
{
	return xzm_malloc_zone_malloc_type_memalign_slow(zone, alignment, size,
			malloc_get_tsd_type_id());
}

static void *
xzm_malloc_zone_malloc_type_malloc_with_options_slow(xzm_malloc_zone_t zone,
		size_t align, size_t size, malloc_zone_malloc_options_t options,
		malloc_type_id_t type_id)
{
	void *ptr = NULL;
	if ((zone->xzz_flags & MALLOC_PURGEABLE) &&
			(size <= XZM_SMALL_BLOCK_SIZE_MAX)) {
		xzm_malloc_zone_t main = (xzm_malloc_zone_t)_xzm_malloc_zone_main(zone);
		ptr = xzm_malloc_zone_malloc_type_malloc_with_options(main, align, size,
				options, type_id);
	} else {
		ptr = xzm_malloc_zone_malloc_type_malloc_with_options(zone, align, size,
				options, type_id);
	}
	if (ptr && !(options & MALLOC_ZONE_MALLOC_OPTION_CLEAR) &&
			(zone->xzz_flags & MALLOC_DO_SCRIBBLE)) {
		memset(ptr, SCRIBBLE_BYTE, size);
	}
	return ptr;
}

static void *
xzm_malloc_zone_malloc_with_options_slow(xzm_malloc_zone_t zone, size_t align,
		size_t size, malloc_zone_malloc_options_t options)
{
	return xzm_malloc_zone_malloc_type_malloc_with_options_slow(zone, align,
			size, options, malloc_get_tsd_type_id());
}

#pragma mark libmalloc zone introspection
size_t
xzm_good_size(xzm_malloc_zone_t zone, size_t size)
{
	if (size <= XZM_SMALL_BLOCK_SIZE_MAX) {
		xzm_main_malloc_zone_t main = _xzm_malloc_zone_main(zone);
		uint8_t bin = _xzm_bin(size);
		return main->xzmz_xzone_bin_sizes[bin];
	} else {
		// MAX() handles overflow in roundup()
		return MAX(roundup(size, XZM_SEGMENT_SLICE_SIZE), size);
	}
}

boolean_t
xzm_check(xzm_malloc_zone_t zone)
{
	// TODO: zone self-check
	//
	// Nano doesn't implement this - worth the trouble?
	(void)zone;
	return true;
}

void
xzm_log(xzm_malloc_zone_t zone, void *log_address)
{
	// Not implemented
	//
	// Doesn't seem that useful?  Nano doesn't implement this either.
}

OS_ENUM(xzm_lock_action, int,
	XZM_LOCK_LOCK,
	XZM_LOCK_UNLOCK,
	XZM_LOCK_INIT,
);

static void
_xzm_do_lock_action(_malloc_lock_s *lock, xzm_lock_action_t action)
{
	switch (action) {
	case XZM_LOCK_LOCK:
		_malloc_lock_lock(lock);
		break;
	case XZM_LOCK_UNLOCK:
		_malloc_lock_unlock(lock);
		break;
	case XZM_LOCK_INIT:
		_malloc_lock_init(lock);
		break;
	default:
		xzm_debug_abort("invalid xzm lock action");
		break;
	}
}

static void
_xzm_freelist_xzone_do_lock_action(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_lock_action_t action)
{
	if (action == XZM_LOCK_LOCK) {
		for (xzm_allocation_index_t i = 0; i < zone->xzz_slot_count; i++) {
			_xzm_chunk_list_fork_lock(&_xzm_xzone_chunk_list_for_index(zone,
					xz, zone->xzz_partial_lists, i)->xcl_list);
		}

		_xzm_chunk_batch_list_fork_lock(&xz->xz_batch_list);
		_xzm_chunk_list_fork_lock(&xz->xz_empty_list);
		_xzm_chunk_list_fork_lock(&xz->xz_preallocated_list);
		_xzm_chunk_list_fork_lock(&xz->xz_all_list);
	}

	xzm_chunk_list_head_u head = {
		.xzch_value = os_atomic_load(&xz->xz_all_list.xzch_value, dependency),
	};

	xzm_chunk_t chunk = (xzm_chunk_t)head.xzch_ptr;
	while (chunk) {
		xzm_chunk_atomic_meta_u old_meta;
		if (action == XZM_LOCK_LOCK) {
			(void)_xzm_xzone_freelist_chunk_lock(chunk, &old_meta);
		} else {
			old_meta.xca_value = os_atomic_load_wide(
					&chunk->xzc_atomic_meta.xca_value, dependency);
			if (old_meta.xca_alloc_head != XZM_FREE_MADVISING &&
					old_meta.xca_alloc_head != XZM_FREE_MADVISED) {
				_xzm_xzone_freelist_chunk_unlock(chunk, old_meta);
			}
		}
		chunk = chunk->xzc_linkages[XZM_CHUNK_LINKAGE_ALL];
	}

	if (action != XZM_LOCK_LOCK) {
		_xzm_chunk_list_fork_unlock(&xz->xz_all_list);
		_xzm_chunk_batch_list_fork_unlock(&xz->xz_batch_list);
		_xzm_chunk_list_fork_unlock(&xz->xz_preallocated_list);
		_xzm_chunk_list_fork_unlock(&xz->xz_empty_list);

		for (xzm_allocation_index_t i = 0; i < zone->xzz_slot_count; i++) {
			_xzm_chunk_list_fork_unlock(&_xzm_xzone_chunk_list_for_index(zone,
					xz, zone->xzz_partial_lists, i)->xcl_list);
		}

	}
}

static void
_xzm_small_xzone_lock_all(xzm_malloc_zone_t zone, xzm_xzone_t xz)
{
	xzm_debug_assert(xz->xz_block_size > XZM_TINY_BLOCK_SIZE_MAX);

	while (true) {
		_malloc_lock_lock(&xz->xz_lock);

		xzm_chunk_t chunk = xz->xz_chunkq_batch;
		while (chunk) {
			_malloc_lock_lock(&chunk->xzc_lock);
			chunk = *_xzm_segment_slice_meta_batch_next(zone, chunk);
			xzm_debug_assert(_xzm_slice_meta_is_batch_pointer(zone, chunk));
		}

		LIST_FOREACH(chunk, &xz->xz_chunkq_partial, xzc_entry) {
			_malloc_lock_lock(&chunk->xzc_lock);
		}

		LIST_FOREACH(chunk, &xz->xz_chunkq_full, xzc_entry) {
			bool gotlock = _malloc_lock_trylock(&chunk->xzc_lock);
			if (!gotlock) {
				// Potential lock inversion - need to unwind and retry.
				xzm_chunk_t chunk2 = xz->xz_chunkq_batch;

				while (chunk2) {
					_malloc_lock_unlock(&chunk2->xzc_lock);
					chunk2 = *_xzm_segment_slice_meta_batch_next(zone, chunk2);
					xzm_debug_assert(_xzm_slice_meta_is_batch_pointer(zone,
							chunk2));
				}

				LIST_FOREACH(chunk2, &xz->xz_chunkq_partial, xzc_entry) {
					_malloc_lock_unlock(&chunk2->xzc_lock);
				}

				LIST_FOREACH(chunk2, &xz->xz_chunkq_full, xzc_entry) {
					if (chunk2 == chunk) {
						break;
					}

					_malloc_lock_unlock(&chunk2->xzc_lock);
				}
				break;
			}
		}

		if (chunk) {
			// Encountered a trylock failure, so we need to unlock the xzone to
			// let the holder of the chunk lock we need make progress.
			//
			// TODO: There's a priority inversion here because we're not doing
			// anything to push on the holder of the chunk lock we need.  We
			// know exactly who we need to wait for, so we could give them an
			// artificial push by doing a timed ulock wait on their thread ID
			// after we release the lock - but before doing that we'd probably
			// just replace this whole approach instead.
			_malloc_lock_unlock(&xz->xz_lock);
			yield();
		} else {
			// Made it to the end of the full list without any trylock failures,
			// so we're done.
			break;
		}
	}
}

static void
_xzm_allocation_slots_do_lock_action(xzm_malloc_zone_t zone,
		xzm_lock_action_t action)
{
	for (uint8_t i = XZM_XZONE_INDEX_FIRST; i < zone->xzz_xzone_count; i++) {
		xzm_xzone_t xz = &zone->xzz_xzones[i];

		__unused xzm_slice_kind_t kind;
		if (xz->xz_block_size <= XZM_TINY_BLOCK_SIZE_MAX) {
			kind = XZM_SLICE_KIND_TINY_CHUNK;
		} else if (zone->xzz_small_freelist_enabled) {
			kind = XZM_SLICE_KIND_SMALL_FREELIST_CHUNK;
		} else {
			kind = XZM_SLICE_KIND_SMALL_CHUNK;
		}

		for (uint8_t j = 0; j < zone->xzz_slot_count; j++) {
			size_t slot_idx = (zone->xzz_xzone_count * j) + i;
			xzm_xzone_allocation_slot_t xas =
					&zone->xzz_xzone_allocation_slots[slot_idx];
#if !CONFIG_TINY_ALLOCATION_SLOT_LOCK
			if (kind == XZM_SLICE_KIND_TINY_CHUNK ||
					kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK) {
				if (action == XZM_LOCK_LOCK) {
					_xzm_xzone_allocation_slot_fork_lock(xas);
				} else {
					_xzm_xzone_allocation_slot_fork_unlock(xas);
				}
			} else
#endif // !CONFIG_TINY_ALLOCATION_SLOT_LOCK
			{
				_xzm_do_lock_action(&xas->xas_lock, action);
			}
		}
	}
}

static void
_xzm_foreach_lock(xzm_malloc_zone_t zone, xzm_lock_action_t action)
{
	// mzone-private locks
	//
	// There are 3 levels of locks:
	// - the xzone slot locks
	// - the xzone lock
	// - the chunk locks
	//
	// The chunk locks are the most difficult to handle for fork, as there are
	// so many of them.  The simplest strategy that works is to just find and
	// acquire them all.
	//
	// The listed order is the general locking order, but when full chunks
	// transition back to partial their chunk lock is held while acquiring the
	// xzone lock.  It's therefore not safe to hold the xzone lock while trying
	// to acquire the chunk lock of a full chunk.  To deal with that, use
	// trylock to acquire the locks of full chunks, and on failure back off,
	// release the xzone lock and retry.
	//
	// It's not necessary to acquire the chunk locks of chunks that are empty
	// and in the transition from the partial list to the isolation list,
	// because they won't be allocated from or freed to on the child side of
	// fork - they'll effectively leak when the thread responsible for them
	// disappears as part of fork.
	//
	// TODO: this is not efficient, particularly in processes that have lots of
	// chunks at the time of fork.  Before we pursue general userspace
	// enablement on macOS, where 3p code needs fork() to be reasonably fast, we
	// should do something better, but this is good enough for the situations
	// where it just needs to be correct.

	// The fork-lock bits send waiters to the fork lock, so we need to ensure
	// that we hold the fork lock whenever any of the fork-lock bits are set.
	// That means we need to acquire it before setting any of the fork-lock bits
	// and release it only after clearing all of them.
	//
	// The fork lock also provides mutual exclusion for mutation of the
	// fork-lock bits.  This is needed to prevent collisions between concurrent
	// executions of foreach-lock: one thread un-setting the fork-lock bits must
	// not trample another one making a pass setting them.
	if (action == XZM_LOCK_LOCK) {
		_xzm_do_lock_action(&zone->xzz_fork_lock, action);

		// The zone lock (xzz_lock) is used for tiny chunk walk lock
		// synchronization (among other things).  It is acquired under the
		// allocation slot lock when allocating from a partial chunk, and needs
		// to be held before performing any of the fork walk-locking, so it
		// needs to be taken here after the allocation slots are locked and
		// before any of the tiny chunks are locked.
		_xzm_do_lock_action(&zone->xzz_lock, action);

		// The freelist allocation slots must be locked before the all-chunk
		// lists are walked and unlocked after, so that no new chunks can be
		// allocated and added in the middle.  That's important because we need
		// the set of chunks we walk during the lock phase to be the same as
		// during the unlock phase.
		_xzm_allocation_slots_do_lock_action(zone, action);
	}

	for (uint8_t i = XZM_XZONE_INDEX_FIRST; i < zone->xzz_xzone_count; i++) {
		xzm_xzone_t xz = &zone->xzz_xzones[i];

		xzm_slice_kind_t kind;
		if (xz->xz_block_size <= XZM_TINY_BLOCK_SIZE_MAX) {
			kind = XZM_SLICE_KIND_TINY_CHUNK;
		} else if (zone->xzz_small_freelist_enabled) {
			kind = XZM_SLICE_KIND_SMALL_FREELIST_CHUNK;
		} else {
			kind = XZM_SLICE_KIND_SMALL_CHUNK;
		}

		if (kind == XZM_SLICE_KIND_TINY_CHUNK ||
				kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK) {
			_xzm_freelist_xzone_do_lock_action(zone, xz, action);
		} else if (kind == XZM_SLICE_KIND_SMALL_CHUNK) {
			if (action == XZM_LOCK_LOCK) {
				_xzm_small_xzone_lock_all(zone, xz);
			} else {
				xzm_chunk_t chunk = xz->xz_chunkq_batch;
				while (chunk) {
					_xzm_do_lock_action(&chunk->xzc_lock, action);
					chunk = *_xzm_segment_slice_meta_batch_next(zone, chunk);
					xzm_debug_assert(_xzm_slice_meta_is_batch_pointer(zone,
							chunk));
				}
				LIST_FOREACH(chunk, &xz->xz_chunkq_partial, xzc_entry) {
					_xzm_do_lock_action(&chunk->xzc_lock, action);
				}
				LIST_FOREACH(chunk, &xz->xz_chunkq_full, xzc_entry) {
					_xzm_do_lock_action(&chunk->xzc_lock, action);
				}

				_xzm_do_lock_action(&xz->xz_lock, action);
			}
		}
	}

	if (action != XZM_LOCK_LOCK) {
		_xzm_allocation_slots_do_lock_action(zone, action);
		_xzm_do_lock_action(&zone->xzz_lock, action);
		_xzm_do_lock_action(&zone->xzz_fork_lock, action);
	}
}

void
xzm_force_lock(xzm_malloc_zone_t zone)
{
	_xzm_foreach_lock(zone, XZM_LOCK_LOCK);
}

void
xzm_force_unlock(xzm_malloc_zone_t zone)
{
	_xzm_foreach_lock(zone, XZM_LOCK_UNLOCK);
}

void
xzm_reinit_lock(xzm_malloc_zone_t zone)
{
	_xzm_foreach_lock(zone, XZM_LOCK_INIT);
}

boolean_t
xzm_locked(xzm_malloc_zone_t zone)
{
	// This mechanism is dead:
	// - nanov2 doesn't implement it despite having locks that could lead to the
	//   deadlock this is meant to prevent
	// - there's an obvious copy-paste bug in szone_locked() for medium
	//
	// There are no references to malloc_gdb_po_unsafe() anywhere.  It should
	// all be removed.
	xzm_abort("xzm_locked not implemented");
	return true;
}

static void
_xzm_global_state_lock(xzm_main_malloc_zone_t main, xzm_lock_action_t action)
{
	// main zone shared locks
	for (uint8_t i = XZM_XZONE_INDEX_FIRST; i < main->xzmz_base.xzz_xzone_count;
			i++) {
		xzm_isolation_zone_t iz = &main->xzmz_isolation_zones[i];
		_xzm_do_lock_action(&iz->xziz_lock, action);
	}

	for (uint8_t i = 0; i < main->xzmz_segment_group_count; i++) {
		xzm_segment_group_t sg = &main->xzmz_segment_groups[i];
		_xzm_do_lock_action(&sg->xzsg_alloc_lock, action);
		_xzm_do_lock_action(&sg->xzsg_lock, action);
#if CONFIG_XZM_DEFERRED_RECLAIM
		if (sg->xzsg_id == XZM_SEGMENT_GROUP_DATA_LARGE) {
			_xzm_do_lock_action(&sg->xzsg_cache.xzsc_lock, action);
		}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
	}

	for (uint8_t i = 0; i < main->xzmz_range_group_count; i++) {
		xzm_range_group_t rg = &main->xzmz_range_groups[i];
		_xzm_do_lock_action(&rg->xzrg_lock, action);
	}

	// The extended segment table lock is held while allocating from the segment
	// table metapool, so we need to acquire this lock before the metapool locks
	_xzm_do_lock_action(&main->xzmz_extended_segment_table_lock, action);

	for (int i = 0; i < main->xzmz_metapool_count; i++) {
		xzm_metapool_t mp = &main->xzmz_metapools[i];
		_xzm_do_lock_action(&mp->xzmp_lock, action);
	}

	_xzm_do_lock_action(&main->xzmz_mzones_lock, action);
	_xzm_do_lock_action(&main->xzmz_thread_cache_list_lock, action);

#if CONFIG_XZM_DEFERRED_RECLAIM
	if (main->xzmz_reclaim_buffer) {
		_xzm_do_lock_action(&main->xzmz_reclaim_buffer->xrb_lock, action);
	}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
}

void
xzm_force_lock_global_state(malloc_zone_t *main_zone)
{
	xzm_debug_assert(_xzm_malloc_zone_is_xzm(main_zone));
	xzm_debug_assert(_xzm_malloc_zone_is_main((xzm_malloc_zone_t)main_zone));
	xzm_main_malloc_zone_t main_ref = (xzm_main_malloc_zone_t)main_zone;
	_xzm_global_state_lock(main_ref, XZM_LOCK_LOCK);
}

void
xzm_force_unlock_global_state(malloc_zone_t *main_zone)
{
	xzm_debug_assert(_xzm_malloc_zone_is_xzm(main_zone));
	xzm_debug_assert(_xzm_malloc_zone_is_main((xzm_malloc_zone_t)main_zone));
	xzm_main_malloc_zone_t main_ref = (xzm_main_malloc_zone_t)main_zone;
	_xzm_global_state_lock(main_ref, XZM_LOCK_UNLOCK);
}

void
xzm_force_reinit_lock_global_state(malloc_zone_t *main_zone)
{
	xzm_debug_assert(_xzm_malloc_zone_is_xzm(main_zone));
	xzm_debug_assert(_xzm_malloc_zone_is_main((xzm_malloc_zone_t)main_zone));
	xzm_main_malloc_zone_t main_ref = (xzm_main_malloc_zone_t)main_zone;
	_xzm_global_state_lock(main_ref, XZM_LOCK_INIT);
}

static void
xzm_malloc_zone_destroy(xzm_malloc_zone_t zone)
{
	xzm_main_malloc_zone_t main_ref = zone->xzz_main_ref;
	// It is not sane to permit destroy on the main mzone in the general case:
	// the libsystem initializer makes allocations that need to stay live.  A
	// sufficiently early interposing allocator (e.g. ASan) might plausibly be
	// able to do it early enough, but it doesn't do any harm to keep the main
	// zone around empty in that case.
	if (_xzm_malloc_zone_is_main(zone)) {
		return;
	}

	// Hold the zone lock while modifying the zone's chunk lists to guard
	// against possible fork() problems (e.g. a tiny chunk's walk_locked bit
	// being set)
	_malloc_lock_lock(&zone->xzz_lock);

	// During zone destroy, all the chunks belonging to this zone should exist
	// on zone local lists:
	// - tiny chunks should be on the xzone's ALL list
	// - small chunks will either be in a slot or on the partial or full list
	// - large and huge chunks will be on the zone's chunkq large
	// We require that clients not be allocating from or freeing to zones that
	// are mid destroy, so we don't need to acquire any locks while pulling
	// chunks off of these lists
	LIST_HEAD(, xzm_slice_s) chunks_to_free;
	LIST_INIT(&chunks_to_free);

	for (int i = XZM_XZONE_INDEX_FIRST; i < zone->xzz_xzone_count; i++) {
		xzm_xzone_t xz = &zone->xzz_xzones[i];
		if (xz->xz_block_size > XZM_TINY_BLOCK_SIZE_MAX &&
				!zone->xzz_small_freelist_enabled) {
			// small xzone, pull chunks off of the batch, partial, and full list
			xzm_slice_t chunk = xz->xz_chunkq_batch;
			xzm_slice_t temp_chunk = NULL;
			while (chunk) {
				xzm_debug_assert(!chunk->xzc_bits.xzcb_preallocated);
				LIST_INSERT_HEAD(&chunks_to_free, chunk, xzc_entry);
				temp_chunk = *_xzm_segment_slice_meta_batch_next(zone, chunk);
				xzm_debug_assert(_xzm_slice_meta_is_batch_pointer(zone,
						temp_chunk));
				xzm_debug_assert(main_ref->xzmz_batch_size);
#if CONFIG_XZM_DEFERRED_RECLAIM
				// Reset its reclaim id so it does not appear to have been deferred
				*_xzm_slice_meta_reclaim_id(zone, chunk) = VM_RECLAIM_ID_NULL;
#endif // CONFIG_XZM_DEFERRED_RECLAIM
				chunk = temp_chunk;
			}
			LIST_FOREACH_SAFE(chunk, &xz->xz_chunkq_partial, xzc_entry,
					temp_chunk) {
				xzm_debug_assert(!chunk->xzc_bits.xzcb_preallocated);
				LIST_REMOVE(chunk, xzc_entry);
				LIST_INSERT_HEAD(&chunks_to_free, chunk, xzc_entry);
			}
			LIST_FOREACH_SAFE(chunk, &xz->xz_chunkq_full, xzc_entry,
					temp_chunk) {
				xzm_debug_assert(!chunk->xzc_bits.xzcb_preallocated);
				LIST_REMOVE(chunk, xzc_entry);
				LIST_INSERT_HEAD(&chunks_to_free, chunk, xzc_entry);
			}
			// Slots
			for (xzm_allocation_index_t j = 0; j < zone->xzz_slot_count; j++) {
				xzm_xzone_allocation_slot_t xas =
						_xzm_xzone_allocation_slot_for_index(zone, xz, j);
				chunk = xas->xas_chunk;
				if (chunk) {
					xzm_debug_assert(!chunk->xzc_bits.xzcb_preallocated);
					xzm_debug_assert(chunk->xzc_alloc_idx == j+1);
					xzm_debug_assert(!chunk->xzc_bits.xzcb_on_partial_list);
					xas->xas_chunk = NULL;
					LIST_INSERT_HEAD(&chunks_to_free, chunk, xzc_entry);
				}
			}
			// Preallocated chunks
			LIST_FOREACH_SAFE(chunk, &xz->xz_chunkq_preallocated, xzc_entry,
					temp_chunk)
			{
				LIST_REMOVE(chunk, xzc_entry);
				// This chunk won't have an mzone set, but otherwise will be
				// initialized enough to be freed
				_xzm_xzone_fresh_chunk_init(xz, chunk,
						XZM_SLICE_KIND_SMALL_CHUNK);
#if CONFIG_XZM_DEFERRED_RECLAIM
				xzm_debug_assert(*_xzm_slice_meta_reclaim_id(zone, chunk) ==
						VM_RECLAIM_ID_NULL);
#endif // CONFIG_XZM_DEFERRED_RECLAIM

				LIST_INSERT_HEAD(&chunks_to_free, chunk, xzc_entry);
			}
		} else {
			// freelist xzone, empty the batch list, which will reset reclaim ids
			xzm_slice_t chunk = NULL;
			while ((chunk = _xzm_chunk_list_pop(zone, &xz->xz_batch_list,
					XZM_CHUNK_LINKAGE_BATCH, NULL))) {
				xzm_debug_assert(main_ref->xzmz_batch_size);
			}
			// enumerate all chunks via the ALL list
			while ((chunk = _xzm_chunk_list_pop(zone, &xz->xz_all_list,
					XZM_CHUNK_LINKAGE_ALL, NULL))) {
				xzm_assert(!chunk->xzc_bits.xzcb_preallocated);
				chunk->xzc_atomic_meta.xca_on_partial_list = false;
				chunk->xzc_atomic_meta.xca_on_empty_list = false;
				chunk->xzc_atomic_meta.xca_alloc_idx = XZM_SLOT_INDEX_EMPTY;

				LIST_INSERT_HEAD(&chunks_to_free, chunk, xzc_entry);
			}
			xzm_slice_kind_t kind;
			if (xz->xz_block_size > XZM_TINY_BLOCK_SIZE_MAX) {
				kind = XZM_SLICE_KIND_TINY_CHUNK;
			} else {
				kind = XZM_SLICE_KIND_SMALL_FREELIST_CHUNK;
			}

			// Preallocated freelist chunks
			while ((chunk = _xzm_chunk_list_pop(zone, &xz->xz_preallocated_list,
						XZM_CHUNK_LINKAGE_MAIN, NULL))) {
				// chunk should be madvised and reset by the free path below
				_xzm_xzone_fresh_chunk_init(xz, chunk, kind);
				xzm_debug_assert(chunk->xzc_atomic_meta.xca_alloc_head !=
						XZM_FREE_MADVISED);
#if CONFIG_XZM_DEFERRED_RECLAIM
				xzm_debug_assert(*_xzm_slice_meta_reclaim_id(zone, chunk) ==
						VM_RECLAIM_ID_NULL);
#endif // CONFIG_XZM_DEFERRED_RECLAIM

				LIST_INSERT_HEAD(&chunks_to_free, chunk, xzc_entry);
			}
		}
	}

	_malloc_lock_unlock(&zone->xzz_lock);

	xzm_slice_t span = NULL;
	xzm_slice_t temp_span = NULL;
	LIST_FOREACH_SAFE(span, &chunks_to_free, xzc_entry, temp_span) {
		xzm_xzone_t xz = &zone->xzz_xzones[span->xzc_xzone_idx];
#if CONFIG_XZM_DEFERRED_RECLAIM
		// If this chunk is non-sequestered and has a reclaim index, we need to
		// get it back from the reclaim buffer so it can be returned to the span
		// queue in xzm_chunk_free
		if (!xz->xz_sequestered && _xzm_chunk_should_defer_reclamation(main_ref, span)) {
			int retries = 0;
			while (_xzm_segment_slice_is_deferred(_xzm_segment_for_slice(zone,
					span), span)) {
				if (xzm_chunk_mark_used(zone, span, NULL)) {
					xzm_debug_assert(span->xzc_bits.xzcb_kind ==
							 XZM_SLICE_KIND_TINY_CHUNK ||
							span->xzc_bits.xzcb_kind ==
							 XZM_SLICE_KIND_SMALL_FREELIST_CHUNK);
					// We don't know if the chunk was successfully reclaimed, so
					// reset the allocation head so that it is synchronously
					// madvised
					span->xzc_atomic_meta.xca_alloc_head = XZM_FREE_NULL;
					break;
				}
				xzm_reclaim_force_sync(main_ref->xzmz_reclaim_buffer);
				xzm_assert(retries < 10);
				retries++;
			}
		}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		if (span->xzc_bits.xzcb_kind == XZM_SLICE_KIND_TINY_CHUNK ||
				span->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_FREELIST_CHUNK) {
			// If the chunk is already madvised, it should already be cleared if
			// it's below the zero on free threshold
			if (span->xzc_atomic_meta.xca_alloc_head != XZM_FREE_MADVISED) {
				size_t size = 0;
				void *chunk_start = _xzm_chunk_start_ptr(zone, span, &size);
				xzm_debug_assert(size == XZM_TINY_CHUNK_SIZE ||
						size == XZM_SMALL_FREELIST_CHUNK_SIZE);
				if (xz->xz_block_size <= XZM_ZERO_ON_FREE_THRESHOLD) {
#if CONFIG_MTE
					if (xz->xz_tagged) {
						memtag_disable_checking();
						bzero(chunk_start, size);
						memtag_enable_checking();
					} else {
#endif
						bzero(chunk_start, size);
#if CONFIG_MTE
					}
#endif
				}

				xzm_segment_group_t sg = _xzm_segment_group_for_slice(zone,
						span);
				xzm_segment_group_segment_madvise_chunk(sg, span);
				span->xzc_atomic_meta.xca_free_count = 0;
				span->xzc_atomic_meta.xca_alloc_head = XZM_FREE_MADVISED;
			}
			span->xzc_bits.xzcb_on_partial_list = false;
			_xzm_xzone_chunk_free(zone, &zone->xzz_xzones[span->xzc_xzone_idx],
					span, false);
		} else if (span->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_CHUNK) {
			// Small chunks are typically, in the non-MTE configuration,
			// madvised when their individual blocks are freed. Since we're
			// freeing the whole chunk, we cautiously madvise the whole chunk
			bool madvise_needed = true;
			span->xzc_bits.xzcb_on_partial_list = false;
			_xzm_xzone_chunk_free(zone, &zone->xzz_xzones[span->xzc_xzone_idx],
					span, madvise_needed);
		} else {
			xzm_abort_with_reason("Unexpected chunk kind",
					span->xzc_bits.xzcb_kind);
		}
	}

	LIST_FOREACH_SAFE(span, &zone->xzz_chunkq_large, xzc_entry, temp_span) {
		// NB: _xzm_free_large_huge removes the chunk from chunkq_large, which
		// is why we need to use LIST_FOREACH_SAFE here
		_xzm_free_large_huge(zone, span);
	}

	xzm_metapool_t mp = &main_ref->xzmz_metapools[XZM_METAPOOL_MZONE_IDX];
	xzm_reused_mzone_index_t reused = xzm_metapool_alloc(mp);
	reused->xrmi_mzone_idx = zone->xzz_mzone_idx;
	_malloc_lock_lock(&main_ref->xzmz_mzones_lock);
	SLIST_INSERT_HEAD(&main_ref->xzmz_reusable_mzidxq, reused, xrmi_mzone_entry);
	_malloc_lock_unlock(&main_ref->xzmz_mzones_lock);

	mvm_deallocate_plat(zone, zone->xzz_total_size, 0, NULL);
}

#pragma mark Test helpers

bool
xzm_ptr_lookup_4test(xzm_malloc_zone_t zone, void *ptr,
		xzm_slice_kind_t *kind_out, xzm_segment_group_id_t *sgid_out,
		xzm_xzone_bucket_t *bucket_out)
{
	xzm_xzone_t xz = NULL;
	xzm_chunk_t chunk = _xzm_ptr_lookup(zone, ptr, &xz, NULL, NULL);
	if (!chunk) {
		return false;
	}

	xzm_slice_kind_t kind = chunk->xzc_bits.xzcb_kind;
	*kind_out = kind;

	xzm_segment_t segment = _xzm_segment_for_slice(zone, chunk);
	*sgid_out = segment->xzs_segment_group->xzsg_id;

	if (_xzm_slice_kind_uses_xzones(kind)) {
		xzm_assert(xz);
		*bucket_out = xz->xz_bucket;
	}

	return true;
}

uint8_t
xzm_type_choose_ptr_bucket_4test(const xzm_bucketing_keys_t *const keys,
		uint8_t ptr_bucket_count, malloc_type_descriptor_t type_desc)
{
	return _xzm_type_choose_ptr_bucket(keys, ptr_bucket_count, type_desc);
}

#pragma mark Zone creation and initialization

struct xzm_process_config_s {
	xzm_slot_config_t xzpc_slot_config;
	bool xzpc_madvise_workaround;
	bool xzpc_disable_vm_user_ranges;
};

typedef const struct xzm_process_config_s *xzm_process_config_t;

#if CONFIG_MALLOC_PROCESS_IDENTITY

static const struct xzm_process_config_s _xzm_launchd_process_config = {
	// almost everything in launchd happens on the event queue
	.xzpc_slot_config = XZM_SLOT_SINGLE,
};

static const struct xzm_process_config_s _xzm_notifyd_process_config = {
	// almost everything in notifyd happens on its global workloop
	.xzpc_slot_config = XZM_SLOT_SINGLE,
};

static const xzm_process_config_t _xzm_process_configs[MALLOC_PROCESS_COUNT] = {
	[MALLOC_PROCESS_LAUNCHD] = &_xzm_launchd_process_config,
	[MALLOC_PROCESS_NOTIFYD] = &_xzm_notifyd_process_config,
};

#endif // CONFIG_MALLOC_PROCESS_IDENTITY

#define XZM_FRONT_RANDOM_SIZE 32

static xzm_front_index_t
_xzm_random_front_index(uint8_t front_random[static XZM_FRONT_RANDOM_SIZE],
		size_t allocation_front_count, xzm_xzone_index_t xz_idx)
{
	if (allocation_front_count == 1) {
		return XZM_FRONT_INDEX_DEFAULT;
	}

	xzm_assert(allocation_front_count == 2);

	size_t byte = xz_idx / CHAR_BIT;
	size_t bit = xz_idx % CHAR_BIT;

	xzm_assert(byte < XZM_FRONT_RANDOM_SIZE);
	return (bool)(front_random[byte] & (1 << bit));
}

// mimalloc: MI_PAGE_QUEUES_EMPTY
static const size_t _xzm_bin_sizes[] = {
	16,
	32,
	48,
	64,
	80,
	96,
	112,
	128,
	160,
	192,
	224,
	256,
	320,
	384,
	448,
	512,
	640,
	768,
	896,
	1024,
	1280,
	1536,
	1792,
	2048,
	2560,
	3072,
	3584,
	4096,
	5120,
	6144,
	7168,
	8192,
	10240,
	12288,
	14336,
	16384,
	20480,
	24576,
	28672,
	32768,
};

// mimalloc: MI_SEGMENT_SPAN_QUEUES_EMPTY
static const size_t _xzm_span_queue_slice_counts[] = {
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	10,
	12,
	14,
	16,
	20,
	24,
	28,
	32,
	40,
	48,
	56,
	64,
	80,
	96,
	112,
	128,
	160,
	192,
	224,
	256,
};

MALLOC_STATIC_ASSERT(
		countof_unsafe(_xzm_span_queue_slice_counts) == XZM_SPAN_QUEUE_COUNT,
		"all span queues have slice counts");

static const char ptr_buckets_boot_arg[] = "xzone_ptr_buckets";
static const char xzone_slot_config_boot_arg[] = "malloc_xzone_slot_config";
static const char xzone_guard_pages_boot_arg[] = "xzone_guard_pages";

static void
_xzm_initialize_const_zone_data(xzm_malloc_zone_t zone,
		size_t size, xzm_mzone_index_t mzone_idx, size_t xzone_count,
		size_t slot_count, xzm_xzone_t xzones,
		xzm_xzone_allocation_slot_t slots, xzm_main_malloc_zone_t main_ref,
		xzm_slot_config_t initial_slot_config, uint32_t initial_slot_threshold,
		xzm_slot_config_t max_slot_config,
		uint32_t list_upgrade_threshold_single,
		uint32_t list_upgrade_threshold_cluster, uint32_t list_upgrade_period,
		uint32_t slot_upgrade_threshold_single,
		uint32_t slot_upgrade_threshold_cluster, uint32_t slot_upgrade_period,
		uint64_t tiny_thrash_threshold, uint64_t small_thrash_threshold,
		uint64_t small_thrash_limit_size, uint64_t debug_flags,
		bool small_freelist, xzm_slot_config_t max_list_config,
		xzm_chunk_list_t partial_lists)
{
	// We're making some assumptions about layout here, but I think that's fine
	xzm_debug_assert((uintptr_t)zone + size >= (uintptr_t)slots +
			sizeof(struct xzm_xzone_allocation_slot_s) * xzone_count * slot_count);
	xzm_debug_assert((uintptr_t)slots >= (uintptr_t)xzones +
			sizeof(struct xzm_xzone_s) * xzone_count);

	*zone = (struct xzm_malloc_zone_s){
		.xzz_basic_zone = {
			.version = 16,
			.size = (void *)xzm_malloc_zone_size,
			.malloc = (void *)xzm_malloc_zone_malloc,
			.calloc = (void *)xzm_malloc_zone_calloc,
			.valloc = (void *)xzm_malloc_zone_valloc,
			.free = (void *)xzm_malloc_zone_free,
			.realloc = (void *)xzm_malloc_zone_realloc,
			.destroy = (void *)xzm_malloc_zone_destroy,
			// Note: batch_malloc and batch_free introduce way too much
			// complexity and maintenance burden to be worth whatever
			// marginal performance benefit they may offer, so we don't
			// really want to implement them.
			//
			// Technically they're supposed to be optional, so ideally we'd
			// just leave them NULL.  However, we've never had a default
			// zone that didn't have them, so we open ourselves up to
			// compatibility risks with code that either isn't prepared to
			// deal with getting 0 or isn't prepared to wrap a zone that
			// doesn't have an implementation.  Even within the project
			// we depend on the default zone having an implementation: the
			// virtual default zone calls through without checking for NULL. So,
			// rather than leaving them NULL, we keep trivial implementations
			// that just wrap plain malloc and free.
			.batch_malloc = malloc_zone_batch_malloc_fallback,
			.batch_free = malloc_zone_batch_free_fallback,
			.introspect = (struct malloc_introspection_t
							*)&xzm_malloc_zone_introspect,
			.memalign = (void *)xzm_malloc_zone_memalign,
			.free_definite_size = (void *)xzm_malloc_zone_free_definite_size,
			.pressure_relief = (void *)malloc_zone_pressure_relief_fallback,
			.claimed_address = (void *)xzm_malloc_zone_claimed_address,
			.try_free_default = (void *)xzm_malloc_zone_try_free_default,
			.malloc_with_options = (void *)xzm_malloc_zone_malloc_with_options,

			.malloc_type_malloc = (void *)xzm_malloc_zone_malloc_type_malloc,
			.malloc_type_calloc = (void *)xzm_malloc_zone_malloc_type_calloc,
			.malloc_type_realloc = (void *)xzm_malloc_zone_malloc_type_realloc,
			.malloc_type_memalign = (void *)xzm_malloc_zone_malloc_type_memalign,
			.malloc_type_malloc_with_options =
					(void *)xzm_malloc_zone_malloc_type_malloc_with_options,
		},
		.xzz_total_size = size,
		.xzz_mzone_idx = mzone_idx,
		.xzz_xzone_count = xzone_count,
		.xzz_slot_count = slot_count,
		.xzz_xzones = xzones,
		.xzz_xzone_allocation_slots = slots,
		.xzz_partial_lists = partial_lists,
		.xzz_main_ref = main_ref,
		.xzz_max_list_config = max_list_config,
		.xzz_initial_slot_config = initial_slot_config,
		.xzz_max_slot_config = max_slot_config,
		.xzz_small_freelist_enabled = small_freelist,
		.xzz_list_upgrade_threshold = {
			list_upgrade_threshold_single,
			list_upgrade_threshold_cluster,
		},
		.xzz_list_upgrade_period = list_upgrade_period,
		.xzz_slot_initial_threshold = initial_slot_threshold,
		.xzz_slot_upgrade_threshold = {
			slot_upgrade_threshold_single,
			slot_upgrade_threshold_cluster,
		},
		.xzz_slot_upgrade_period = slot_upgrade_period,
		.xzz_tiny_thrash_threshold = tiny_thrash_threshold,
		.xzz_small_thrash_threshold = small_thrash_threshold,
		.xzz_small_thrash_limit_size = small_thrash_limit_size,
		.xzz_lock = _MALLOC_LOCK_INIT,
		.xzz_fork_lock = _MALLOC_LOCK_INIT,
		.xzz_flags = debug_flags,
	};

	bool use_slowpath_zone_functions = false;
	if (debug_flags & MALLOC_DO_SCRIBBLE ||
			debug_flags & MALLOC_PURGEABLE) {
		use_slowpath_zone_functions = true;
	}

	if (use_slowpath_zone_functions) {
		zone->xzz_basic_zone.malloc = (void *)xzm_malloc_zone_malloc_slow;
		zone->xzz_basic_zone.calloc = (void *)xzm_malloc_zone_calloc_slow;
		zone->xzz_basic_zone.valloc = (void *)xzm_malloc_zone_valloc_slow;
		zone->xzz_basic_zone.free = (void *)xzm_malloc_zone_free_slow;
		zone->xzz_basic_zone.realloc = (void *)xzm_malloc_zone_realloc_slow;
		zone->xzz_basic_zone.memalign = (void *)xzm_malloc_zone_memalign_slow;
		zone->xzz_basic_zone.free_definite_size =
				(void *)xzm_malloc_zone_free_definite_size_slow;
		zone->xzz_basic_zone.try_free_default =
				(void *)xzm_malloc_zone_try_free_default_slow;
		zone->xzz_basic_zone.malloc_with_options =
				(void *)xzm_malloc_zone_malloc_with_options_slow;
		zone->xzz_basic_zone.malloc_type_malloc =
				(void *)xzm_malloc_zone_malloc_type_malloc_slow;
		zone->xzz_basic_zone.malloc_type_calloc =
				(void *)xzm_malloc_zone_malloc_type_calloc_slow;
		zone->xzz_basic_zone.malloc_type_realloc =
				(void *)xzm_malloc_zone_malloc_type_realloc_slow;
		zone->xzz_basic_zone.malloc_type_memalign =
				(void *)xzm_malloc_zone_malloc_type_memalign_slow;
		zone->xzz_basic_zone.malloc_type_malloc_with_options =
				(void *)xzm_malloc_zone_malloc_type_malloc_with_options_slow;
	}
}

static void
_xzm_initialize_xzone_data(xzm_malloc_zone_t zone,
		xzm_slot_config_t list_config, xzm_guard_page_config_t guard_config,
		uint8_t front_random[XZM_FRONT_RANDOM_SIZE], bool all_data)
{
	xzm_main_malloc_zone_t main_ref = _xzm_malloc_zone_main(zone);
	bool is_main = _xzm_malloc_zone_is_main(zone);

	xzm_debug_assert((is_main && front_random) || (!is_main && !front_random));

	// NOTE: although this is technically a layering violation with Libc and
	// CoreCrypto, we and they believe it to be safe
	//
	// TODO: allow overriding with a fixed value for testing
	uint64_t freelist_cookie = 0;
	arc4random_buf(&freelist_cookie, sizeof(freelist_cookie));
	if (!freelist_cookie) {
		freelist_cookie = 0xdeaddeaddeaddeadull;
	}
#if CONFIG_MTE
	if (zone->xzz_memtag_config.enabled) {
		// Clear the tag bits from the freelist cookie to ensure they can always be
		// used to store a block's tag
		freelist_cookie = (uint64_t)memtag_strip_address(
				(uint8_t *)freelist_cookie);
	}
#endif
	zone->xzz_freelist_cookie = freelist_cookie;

	xzm_xzone_index_t xzidx = XZM_XZONE_INDEX_FIRST;
	for (size_t i = 0; i < countof(_xzm_bin_sizes); i++) {
		if (is_main) {
			main_ref->xzmz_xzone_bin_offsets[i] = xzidx;
		}
		size_t bin_buckets = main_ref->xzmz_xzone_bin_bucket_counts[i];
		for (int bucket = 0; bucket < bin_buckets; xzidx++, bucket++) {
			xzm_xzone_t xz = &zone->xzz_xzones[xzidx];
			size_t block_size = main_ref->xzmz_xzone_bin_sizes[i];
			size_t chunk_size;
			if (block_size <= XZM_TINY_BLOCK_SIZE_MAX) {
				chunk_size = XZM_TINY_CHUNK_SIZE;
			} else if (zone->xzz_small_freelist_enabled) {
				chunk_size = XZM_SMALL_FREELIST_CHUNK_SIZE;
			} else {
				chunk_size = XZM_SMALL_CHUNK_SIZE;
			}

			size_t chunk_capacity = chunk_size / block_size;

			// The early budget is only in the main zone xzones, all other
			// zones decrement those
			uint16_t early_budget = 0;
			if (is_main && main_ref->xzmz_mfm_address) {
				if (block_size <= 256) {
					early_budget = 2048 / block_size;
				} else if (block_size <= 512) {
					early_budget = 4096 / block_size;
				} else if (block_size <= 2048) {
					early_budget = 8192 / block_size;
				} else if (block_size <= 8192) {
					early_budget = 1;
				}
			}

			xzm_segment_group_id_t sgid;
			bool sequestered;
			if ((bucket == XZM_XZONE_BUCKET_DATA || all_data) &&
#if XZM_NARROW_BUCKETING
					!main_ref->xzmz_narrow_bucketing &&
#endif
					!XZM_BUCKET_POINTER_ONLY) {
				sgid = XZM_SEGMENT_GROUP_DATA;
				sequestered = false;
			} else {
				sgid = XZM_SEGMENT_GROUP_POINTER_XZONES;
				sequestered = true;
			}

			// Enable sequestering for small chunks to avoid coalescing with
			// adjacent chunks that may not already be marked as free
			if (block_size > XZM_TINY_BLOCK_SIZE_MAX &&
					main_ref->xzmz_defer_small) {
				sequestered = true;
			}

			xzm_front_index_t front;
			if (is_main) {
				if (sgid == XZM_SEGMENT_GROUP_POINTER_XZONES) {
					front = _xzm_random_front_index(front_random,
							main_ref->xzmz_allocation_front_count, xzidx);
				} else {
					xzm_debug_assert(sgid == XZM_SEGMENT_GROUP_DATA);
					front = XZM_FRONT_INDEX_DEFAULT;
				}
			} else {
				xzm_xzone_t main_xz = &main_ref->xzmz_base.xzz_xzones[xzidx];
				front = main_xz->xz_front;
			}

			uint8_t run_length = 0;
			uint8_t guard_density = 0;
			if (guard_config->xgpc_enabled) {
				if (guard_config->xgpc_enabled_for_data) {
					// To avoid unbounded VA growth caused by unusable free
					// spans in the data segments, all xzones are sequestered
					// when guards are enabled
					sequestered = true;
				}

				if (sequestered) {
					if (block_size <= XZM_TINY_BLOCK_SIZE_MAX) {
						run_length = guard_config->xgpc_max_run_tiny;
						guard_density = guard_config->xgpc_tiny_guard_density;
					} else {
						run_length = guard_config->xgpc_max_run_small;
						guard_density = guard_config->xgpc_small_guard_density;
					}
				}
			}

			*xz = (struct xzm_xzone_s){
				.xz_segment_group_id = sgid,
				.xz_front = front,
				.xz_block_size = block_size,
				.xz_quo_magic = XZM_MAGIC_QUO(block_size),
				.xz_align_magic = XZM_MAGIC_ALIGNED(block_size),
				.xz_chunk_capacity = (uint32_t)chunk_capacity,
				.xz_lock = _MALLOC_LOCK_INIT,
				.xz_early_budget = early_budget,
				.xz_idx = xzidx,
				.xz_mzone_idx = zone->xzz_mzone_idx,
				.xz_bucket = (xzm_xzone_bucket_t)bucket,
				.xz_sequestered = sequestered,
				.xz_slot_config = XZM_SLOT_SINGLE,
				.xz_guard_config = {
					.xxgc_max_run_length = run_length,
					.xxgc_density = guard_density,
				}
			};

			// Initialize all possible slots with the initial slot config
			xzm_debug_assert(zone->xzz_slot_count ==
					_xzm_get_limit_allocation_index(zone->xzz_max_slot_config));
			xzm_allocation_index_t limit_list =
					_xzm_get_limit_allocation_index(zone->xzz_max_list_config);
			for (xzm_allocation_index_t i = 0; i < zone->xzz_slot_count;
					++i) {
				xzm_xzone_allocation_slot_t xas =
						_xzm_xzone_allocation_slot_for_index(zone, xz, i);
				xas->xas_counters.xsc_slot_config = xz->xz_slot_config;
#if CONFIG_TINY_ALLOCATION_SLOT_LOCK
				_malloc_lock_init(&xas->xas_lock);
#endif // CONFIG_TINY_ALLOCATION_SLOT_LOCK

				if (i >= limit_list) {
					continue;
				}

				xzm_chunk_list_t xcl =
						_xzm_xzone_chunk_list_for_index(zone, xz,
						zone->xzz_partial_lists, i);
				xcl->xcl_counters.xsc_slot_config = xz->xz_list_config;
			}

#if CONFIG_MTE
			xz->xz_tagged = _xzm_zone_memtag_block(zone, block_size,
					/*data=*/(bucket == XZM_XZONE_BUCKET_DATA));

			// Disable the early allocator for zones for which we don't
			// enable tagging. This is because MFM will always provide
			// tagged memory, and this might cause the client to violate
			// MTE mapping policies that are expected to hold for
			// data-only allocations, which are normally not tagged in
			// the default configuration.
			// `zone->xzz_memtag_config.enabled` is used as proxy for
			// `mfm_memtag_enabled`.
			if (zone->xzz_memtag_config.enabled && !xz->xz_tagged) {
				xz->xz_early_budget = 0;
			}
#endif // CONFIG_MTE
		}
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static const char *
_xzm_development_only_getenv(const char **envp, const char *key)
{
	return malloc_internal_security_policy ? _simple_getenv(envp, key) : NULL;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static const char *
_xzm_production_getenv(const char **envp, const char *key)
{
	return _simple_getenv(envp, key);
}

#ifdef _simple_getenv
#undef _simple_getenv
#endif

#define _simple_getenv(...) \
		static_assert(0, "Use _xzm_(development_only|release)_getenv instead")


malloc_zone_t *
xzm_main_malloc_zone_create(unsigned debug_flags, const char **envp,
		const char **apple, const char *bootargs)
{
	bool security_critical = false, security_critical_max_perf = false;
#if CONFIG_MALLOC_PROCESS_IDENTITY
	security_critical = malloc_process_is_security_critical(
			malloc_process_identity);
	security_critical_max_perf = malloc_process_is_security_critical_max_perf(
			malloc_process_identity);
#endif // CONFIG_MALLOC_PROCESS_IDENTITY

	uint8_t front_random[XZM_FRONT_RANDOM_SIZE];

	xzm_bucketing_keys_t bucketing_keys;
	// executable_boothash is a salted hash of the concatenation of the current
	// boot session UUID and cdhash of the main executable
	const char *boothash = _xzm_production_getenv(apple, "executable_boothash");
	if (!boothash) {
		// executable_boothash isn't populated when the executable isn't
		// codesigned (rdar://118451590). This should be very rare (only when
		// the system is running with reduced security), so to guard against
		// accidentally breaking the boothash, crash in known security-critical
		// processes
		if (security_critical) {
			xzm_abort("couldn't find executable_boothash");
		}

		arc4random_buf(&bucketing_keys, sizeof(bucketing_keys));
	} else {
		size_t boothash_len = strlen(boothash);
		if (boothash_len < 32) {
			xzm_abort_with_reason("invalid executable_boothash length",
					boothash_len);
		}

		const size_t part_len = 16;
		char boothash_part[part_len + 1];
		unsigned long long value;
		const size_t keys_cnt = sizeof(bucketing_keys.xbk_key_data) /
				sizeof(bucketing_keys.xbk_key_data[0]);

		for (size_t i = 0; i < keys_cnt; i++) {
			memcpy(boothash_part, &boothash[i * part_len], part_len);
			boothash_part[part_len] = '\0';

			value = strtoull(boothash_part, NULL, 16);
			if ((value == 0 && errno == EINVAL) ||
					(value == ULLONG_MAX && errno == ERANGE)) {
				xzm_abort("invalid executable_boothash string");
			}
			bucketing_keys.xbk_key_data[i] = value;
		}
	}

	// XXX We need a bunch of additional per-boot entropy for allocation front
	// assignments.  It may make sense to increase the amount of per-boot
	// entropy supplied by the kernel, but for now we make do by deriving
	// from what we've already got.
	//
	// TODO: If we do continue to derive, we should probably be seeding a PRNG
	// instead
	static_assert(sizeof(front_random) == CCSHA256_OUTPUT_SIZE,
			"front_random size");

	const struct ccdigest_info *di = ccsha256_di();
	ccdigest_di_decl(di, dc);
	ccdigest_init(di, dc);
	char diversifier[] = "xzone malloc front random";
	ccdigest_update(di, dc, sizeof(diversifier), diversifier);
	ccdigest_update(di, dc, sizeof(bucketing_keys),
			bucketing_keys.xbk_key_data);
	ccdigest_final(di, dc, front_random);
	ccdigest_di_clear(di, dc);


	// TODO: scrub executable_boothash from the apple array like we do for
	// malloc_entropy?

	xzm_process_config_t process_config = NULL;
#if CONFIG_MALLOC_PROCESS_IDENTITY
	if (malloc_process_identity != MALLOC_PROCESS_NONE) {
		process_config = _xzm_process_configs[malloc_process_identity];
	}
#endif // CONFIG_MALLOC_PROCESS_IDENTITY

	bool use_slowpath_zone_functions = false;
	if (debug_flags & MALLOC_DO_SCRIBBLE) {
		use_slowpath_zone_functions = true;
	}

	xzm_slot_config_t max_slot_config = XZM_SLOT_CPU;

	size_t override_ptr_bucket_count = 0;
#if XZM_NARROW_BUCKETING
	// Platforms that support narrow bucketing default to using it
	bool narrow_bucketing = true;
#endif

	char value_buf[256];
	const char *flag = malloc_common_value_for_key_copy(bootargs,
			ptr_buckets_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		const char *endp;
		long value = malloc_common_convert_to_long(flag, &endp);
		if (!*endp && value > 0 && value <= XZM_POINTER_BUCKETS_MAX) {
			override_ptr_bucket_count = (size_t)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"Invalid xzone_ptr_buckets value %ld - ignored.\n", value);
		}
	}

#if MALLOC_TARGET_IOS_ONLY || TARGET_OS_WATCH
	if (!security_critical) {
#if MALLOC_TARGET_IOS_ONLY
		override_ptr_bucket_count = 2;
#elif TARGET_OS_WATCH
		override_ptr_bucket_count = 1;
#endif
	}
#endif

	// There are a few ways MallocMaxMagazines might have been set:
	// - JetsamProperties sets it for a number of daemons on iOS
	// - A handful of projects found the envvar on their own and set it
	//   themselves in their launchd plists
	//
	// Pending an audit and rebalancing of bucketing by security sensitivity,
	// give processes that have this set and that we don't otherwise have
	// explicit policy for a reduced bucket configuration.
	//
	// If set to 1, we should also give them a reduced slot config.
	bool allow_malloc_max_magazines = true;
	// Note: this is load-bearing for wifip2pd
	if (security_critical) {
		allow_malloc_max_magazines = false;
#if TARGET_OS_VISION
		// On visionOS, don't clamp the security-critical-max-perf processes to
		// per-cluster scaling
		if (!security_critical_max_perf) {
			max_slot_config = XZM_SLOT_CLUSTER;
		}
#else
		// On the other platforms, since we're already clamping the
		// security-critical-max-perf processes to per-cluster scaling without
		// issue, clamp in all cases
		max_slot_config = XZM_SLOT_CLUSTER;
#endif
	}
	if (allow_malloc_max_magazines) {
		flag = _xzm_production_getenv(envp, "MallocMaxMagazines");
		if (flag) {
			long value = strtol(flag, NULL, 10);
			if (value == 1) {
				max_slot_config = XZM_SLOT_SINGLE;
			} else if (value == 2 || value == UINT16_MAX) {
				max_slot_config = XZM_SLOT_CLUSTER;
			}

#if MALLOC_TARGET_IOS_ONLY || MALLOC_TARGET_DK_IOS || \
		TARGET_OS_OSX || MALLOC_TARGET_DK_OSX
			if (value == 1 || value == 2 || value == UINT16_MAX) {
				override_ptr_bucket_count = 1;
			}
#endif
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzonePtrBucketCount");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value <= XZM_POINTER_BUCKETS_MAX) {
			override_ptr_bucket_count = (size_t)value;
#if XZM_NARROW_BUCKETING
			narrow_bucketing = false;
#endif
		}
	}

	size_t ptr_bucket_count = override_ptr_bucket_count ?:
			XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT;

	size_t bucket_base = XZM_XZONE_BUCKET_POINTER_BASE;
#if XZM_NARROW_BUCKETING
	if (narrow_bucketing) {
		bucket_base = 0;
	}
#endif

	bool use_early_allocator = true;
#if XZM_NARROW_BUCKETING
	if (narrow_bucketing && ptr_bucket_count == 1) {
		use_early_allocator = false;
	}
#endif

	flag = _xzm_production_getenv(envp, "MallocXzoneEarlyAlloc");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			use_early_allocator = (bool)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"MallocXzoneEarlyAlloc must be 0 or 1.\n");
		}
	}

	if (use_early_allocator) {
		mfm_initialize();
	}

	size_t buckets_per_bin = bucket_base + ptr_bucket_count;

	size_t bin_count = countof(_xzm_bin_sizes);
	size_t xzone_count = 1 + bin_count * buckets_per_bin;
	xzm_assert(xzone_count <= UINT8_MAX);

	bool madvise_workaround = false;
	if (process_config && process_config->xzpc_madvise_workaround) {
		madvise_workaround = true;
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneMadviseWorkaround");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			madvise_workaround = (bool)value;
		}
	}

#if CONFIG_MTE
	struct xzm_memtag_config_s memtag_config = {0};

	// If the process has been spawned by setting has_sec_transition=1,
	// load a default configuration for MTE support in xzone.
	// Note that we still allow overriding the configuration through
	// environment variables.
	if (malloc_has_sec_transition) {
		memtag_config.enabled = true;
		memtag_config.tag_data = false;
		memtag_config.max_block_size = XZM_SMALL_BLOCK_SIZE_MAX;
		if (malloc_sec_transition_policy & /*TASK_SEC_POLICY_USER_DATA*/0x02) {
			memtag_config.tag_data = true;
		}
	}

	uint64_t max_supported_memtag_block_size = XZM_SMALL_BLOCK_SIZE_MAX;

	// MTE debug mode is available on public builds
	flag = _xzm_production_getenv(envp, "MallocTagAll");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 1) {
			memtag_config.tag_data = true;
			memtag_config.max_block_size = max_supported_memtag_block_size;

			if (!malloc_has_sec_transition) {
				malloc_report(MALLOC_REPORT_CRASH,
						"Malloc MTE debug mode (MallocTagAll=1) requires the "
						"process to be started with MTE enabled.\n");
			}
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocTagAllInternal");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 1) {
			memtag_config.tag_data = true;
			memtag_config.max_block_size = max_supported_memtag_block_size;
		}
		// Skip sanity check for general MTE enablement.
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneMemtagEnable");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			memtag_config.enabled = (bool)value;
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneMemtagTagData");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			memtag_config.tag_data = (bool)value;
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneMemtagMaxBlockSize");
	if (flag) {
		unsigned long long value = strtoull(flag, NULL, 10);
		// TODO: allow all sizes?
		if (value <= XZM_SMALL_BLOCK_SIZE_MAX && !(value & 0xf)) {
			memtag_config.max_block_size = value;
		}
	}
#endif // CONFIG_MTE

	bool has_vm_user_ranges = true;
	if (process_config && process_config->xzpc_disable_vm_user_ranges) {
		has_vm_user_ranges = false;
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneHasRanges");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			has_vm_user_ranges = (bool)value;
		}
	}

#if XZM_NARROW_BUCKETING
	// Don't waste a PTE trying to separate data at the range level if it's
	// mixed into the same xzones anyway
	if (narrow_bucketing && !security_critical) {
		has_vm_user_ranges = false;
	}
#endif

	bool thread_caching = false;

#if CONFIG_NANOZONE
	bool nano_config = (_malloc_engaged_nano == NANO_V2);

#if MALLOC_TARGET_DK_OSX
	// TODO: clean up macOS DriverKit nano enablement config
	nano_config = false;
#endif

	bool security_critical_allows_nano_config = false;
#if CONFIG_MALLOC_PROCESS_IDENTITY
	if (malloc_process_identity == MALLOC_PROCESS_HARDENED_HEAP_CONFIG) {
		security_critical_allows_nano_config = true;
	}
#endif

	if (security_critical && !security_critical_allows_nano_config) {
		// Load-bearing for MTLCompilerService
		nano_config = false;
	}

	if (nano_config) {
		max_slot_config = XZM_SLOT_CPU;

		// Not dealing with thread caching in the simulator yet
#if !TARGET_OS_SIMULATOR
#if CONFIG_FEATUREFLAGS_SIMPLE
		thread_caching = os_feature_enabled_simple(libmalloc,
				SecureAllocator_ThreadCaching, false);
#else
		// Not for DriverKit yet
#endif // CONFIG_FEATUREFLAGS_SIMPLE
#endif // !TARGET_OS_SIMULATOR
	}

	flag = _xzm_production_getenv(envp, "MallocXzoneThreadCaching");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			thread_caching = (value == 1);
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"MallocXzoneThreadCaching must be one of 0,1 - got %ld\n",
					value);
		}
	}
#endif // CONFIG_NANOZONE

	if (process_config && process_config->xzpc_slot_config != XZM_SLOT_LAST) {
		max_slot_config = process_config->xzpc_slot_config;
	}

	flag = malloc_common_value_for_key_copy(bootargs,
			xzone_slot_config_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < XZM_SLOT_LAST) {
			max_slot_config = (xzm_slot_config_t)value;
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneSlotConfig");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < XZM_SLOT_LAST) {
			max_slot_config = (xzm_slot_config_t)value;
		}
	}

	xzm_slot_config_t slot_config = XZM_SLOT_SINGLE;
	uint32_t slot_threshold = 128;
	uint8_t chunk_threshold = 1;
#if TARGET_OS_OSX
	// On macOS, not known processes get initial per-cluster
	if (!(security_critical && !security_critical_max_perf) &&
			!malloc_space_efficient_enabled) {
		slot_config = XZM_SLOT_CLUSTER;
	}
#endif // TARGET_OS_OSX
	flag = _xzm_development_only_getenv(envp, "MallocXzoneInitialSlotConfig");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < XZM_SLOT_LAST) {
			slot_config = (xzm_slot_config_t)value;
		}
	}
	flag = _xzm_development_only_getenv(envp, "MallocXzoneInitialSlotThreshold");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			slot_threshold = (uint32_t)value;
		}
	}
	flag = _xzm_development_only_getenv(envp, "MallocXzoneInitialChunkThreshold");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value > 0 && value < UINT8_MAX) {
			chunk_threshold = (uint8_t)value;
		}
	}

	uint32_t list_upgrade_threshold_single = 32;
	uint32_t list_upgrade_threshold_cluster = 128;
	uint32_t slot_upgrade_threshold_single = 64;
	uint32_t slot_upgrade_threshold_cluster = 256;
	flag = _xzm_development_only_getenv(envp,
			"MallocXzoneListUpgradeThreshold");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			list_upgrade_threshold_single = (uint32_t)value;
			list_upgrade_threshold_cluster = (uint32_t)value;
		}
	}

	flag = _xzm_development_only_getenv(envp,
			"MallocXzoneListUpgradeThresholdSingle");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			list_upgrade_threshold_single = (uint32_t)value;
		}
	}

	flag = _xzm_development_only_getenv(envp,
			"MallocXzoneListUpgradeThresholdCluster");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			list_upgrade_threshold_cluster = (uint32_t)value;
		}
	}

	flag = _xzm_development_only_getenv(envp,
			"MallocXzoneSlotUpgradeThreshold");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			slot_upgrade_threshold_single = (uint32_t)value;
			slot_upgrade_threshold_cluster = (uint32_t)value;
		}
	}

	flag = _xzm_development_only_getenv(envp,
			"MallocXzoneSlotUpgradeThresholdSingle");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			slot_upgrade_threshold_single = (uint32_t)value;
		}
	}

	flag = _xzm_development_only_getenv(envp,
			"MallocXzoneSlotUpgradeThresholdCluster");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			slot_upgrade_threshold_cluster = (uint32_t)value;
		}
	}

	// Reset contention counters every 1K allocations to prevent long-lived
	// processes from slowly accruing additional slots
	uint32_t list_upgrade_period = 512;
	uint32_t slot_upgrade_period = 1024;
	flag = _xzm_development_only_getenv(envp, "MallocXzoneListUpgradePeriod");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			list_upgrade_period = (uint32_t)value;
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneSlotUpgradePeriod");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			slot_upgrade_period = (uint32_t)value;
		}
	}

	uint8_t slot_count = 0;
	switch (max_slot_config) {
	case XZM_SLOT_SINGLE:
		slot_count = 1;
		break;
	case XZM_SLOT_CLUSTER:
#if !CONFIG_XZM_CLUSTER_AWARE
		slot_count = MIN(2, logical_ncpus);
		break;
#else // !CONFIG_XZM_CLUSTER_AWARE
		if (ncpuclusters > 1) {
			slot_count = (uint8_t)ncpuclusters;
			break;
		}
		MALLOC_FALLTHROUGH;
#endif // !CONFIG_XZM_CLUSTER_AWARE
	case XZM_SLOT_CPU:
		max_slot_config = XZM_SLOT_CPU; // handle fallthrough
		slot_count = (uint8_t)logical_ncpus;
		break;
	default:
		xzm_abort("Invalid xzone slot config");
	}
	if (slot_config > max_slot_config) {
		slot_config = max_slot_config;
	}

	const uint64_t nsec_per_msec = 1000000ull;
	mach_timebase_info_data_t tb_info;
	kern_return_t kr = mach_timebase_info(&tb_info);
	if (kr) {
		xzm_abort_with_reason("mach_timebase_info failed", kr);
	}

	uint64_t small_thrash_threshold = 0;
	uint64_t small_thrash_limit_size = 0;

	uint64_t tiny_thrash_threshold_ns = 1 * nsec_per_msec;
	flag = _xzm_development_only_getenv(envp, "MallocXzoneTinyThrashThreshold");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			tiny_thrash_threshold_ns = (uint64_t)value * nsec_per_msec;
		}
	}
	uint64_t tiny_thrash_threshold =
			(tiny_thrash_threshold_ns * tb_info.denom / tb_info.numer);

	uint64_t small_thrash_threshold_ns = 0;
	uint64_t small_thrash_threshold_default_ns = 1 * nsec_per_msec;

	// Cap to the size limit for szone SMALL
	small_thrash_limit_size = KiB(16);

#if CONFIG_NANOZONE
	if (nano_config) {
		small_thrash_threshold_ns = small_thrash_threshold_default_ns;
	}
#else
	(void)small_thrash_threshold_default_ns;
#endif

#if CONFIG_MTE
	if (memtag_config.enabled) {
		small_thrash_threshold_ns = small_thrash_threshold_default_ns;
	}
#endif

	flag = _xzm_development_only_getenv(envp,
			"MallocXzoneSmallThrashThreshold");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			small_thrash_threshold_ns = (uint64_t)value * nsec_per_msec;
		}
	}

	small_thrash_threshold =
			(small_thrash_threshold_ns * tb_info.denom / tb_info.numer);

	flag = _xzm_development_only_getenv(envp,
			"MallocXzoneSmallThrashLimitSize");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value <= XZM_SMALL_BLOCK_SIZE_MAX) {
			small_thrash_limit_size = (uint64_t)value;
		}
	}

	uint32_t thread_cache_activation_period = 16384;
	uint32_t thread_cache_activation_contentions = 256;
	uint64_t thread_cache_activation_time = 0;
#if CONFIG_XZM_THREAD_CACHE
	flag = _xzm_production_getenv(envp, "MallocXzoneThreadCacheActivationPeriod");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			thread_cache_activation_period = (uint32_t)value;
		}
	}

	flag = _xzm_production_getenv(envp,
			"MallocXzoneThreadCacheActivationContentions");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT32_MAX) {
			thread_cache_activation_contentions = (uint32_t)value;
		}
	}

	uint64_t thread_cache_activation_time_ns = 1000 * nsec_per_msec;
	flag = _xzm_production_getenv(envp, "MallocXzoneThreadCacheActivationTime");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < UINT64_MAX) {
			thread_cache_activation_time_ns = (uint64_t)value * nsec_per_msec;
		}
	}

	thread_cache_activation_time =
			(thread_cache_activation_time_ns * tb_info.denom / tb_info.numer);
#endif // CONFIG_XZM_THREAD_CACHE

#if TARGET_OS_VISION
	bool vision_max_perf = !aggressive_madvise_enabled;
#endif

	uint32_t huge_cache_max_entry_bytes = 0;

#if CONFIG_XZM_DEFERRED_RECLAIM
	mach_vm_reclaim_count_t reclaim_buffer_count =
			XZM_RECLAIM_BUFFER_COUNT_DEFAULT;
	mach_vm_reclaim_count_t max_reclaim_buffer_count =
			XZM_RECLAIM_BUFFER_MAX_COUNT_DEFAULT;
	uint16_t huge_cache_size = XZM_HUGE_CACHE_SIZE_DEFAULT; // number of entries
	huge_cache_max_entry_bytes = XZM_HUGE_CACHE_MAX_ENTRY_BYTES_DEFAULT;
	bool defer_tiny = XZM_DEFERRED_RECLAIM_ENABLED_DEFAULT;
	bool defer_small = XZM_DEFERRED_RECLAIM_ENABLED_DEFAULT;
	bool defer_large = XZM_DEFERRED_RECLAIM_ENABLED_DEFAULT;

#if TARGET_OS_VISION
	if (vision_max_perf) {
		defer_tiny = true;
		defer_small = true;
		defer_large = true;
		huge_cache_size = XZM_HUGE_CACHE_SIZE_ENABLED;
	}
#endif

	flag = _xzm_development_only_getenv(envp, "MallocDeferredReclaim");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			defer_tiny = (value == 1);
			defer_small = (value == 1);
			defer_large = (value == 1);
			huge_cache_size = (value == 1) ? XZM_HUGE_CACHE_SIZE_ENABLED : 0;
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocDeferredReclaim must be one of 0,1 - got %ld\n", value);
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocDeferredReclaimBufferCount");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value <= UINT32_MAX) {
			reclaim_buffer_count = (mach_vm_reclaim_count_t)value;
		}
	}
	flag = _xzm_development_only_getenv(envp, "MallocDeferredReclaimBufferMaxCount");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value <= UINT32_MAX) {
			max_reclaim_buffer_count = (mach_vm_reclaim_count_t)value;
		}
	}

	// Round capacities up to page-alignment, or down to the maximum
	reclaim_buffer_count = mach_vm_reclaim_round_capacity(reclaim_buffer_count);
	max_reclaim_buffer_count = mach_vm_reclaim_round_capacity(max_reclaim_buffer_count);

	// MallocLargeCache enables both the huge cache and deferred reclamation
	// for all large allocations
	flag = _xzm_production_getenv(envp, "MallocLargeCache");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			huge_cache_size = (value == 1) ? XZM_HUGE_CACHE_SIZE_ENABLED : 0;
			defer_large = (value == 1);
			defer_tiny = (value == 1);
			defer_small = (value == 1);
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocLargeCache must be 0 or 1.\n");
		}
	}

#if CONFIG_NANOZONE && !TARGET_OS_OSX
	// MallocLargeCache is only supported for processes with Nano
	if (!nano_config) {
		defer_large = false;
		huge_cache_size = 0;
#if MALLOC_TARGET_IOS_ONLY
		defer_tiny = false;
		defer_small = false;
#endif // MALLOC_TARGET_IOS_ONLY
	}
#endif // CONFIG_NANOZONE && !TARGET_OS_OSX

	flag = _xzm_development_only_getenv(envp, "MallocXzoneHugeCacheSize");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value <= UINT16_MAX) {
			huge_cache_size = (uint16_t)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"xzm: unsupported value for MallocXzoneHugeCacheSize (%ld)",
					value);
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneHugeCacheMaxEntryBytes");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value <= UINT32_MAX) {
			huge_cache_max_entry_bytes = (uint32_t)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"xzm: unsupported value for MallocXzoneHugeCacheMaxEntryBytes (%ld)",
					value);
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneDeferTiny");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			defer_tiny = (value == 1);
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocXzoneDeferTiny must be one of 0,1 - got %ld\n", value);
		}
	}
	flag = _xzm_development_only_getenv(envp, "MallocXzoneDeferSmall");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			defer_small = (value == 1);
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocXzoneDeferSmall must be one of 0,1 - got %ld\n", value);
		}
	}
	flag = _xzm_development_only_getenv(envp, "MallocXzoneDeferLarge");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			defer_large = (value == 1);
		} else {
			malloc_report(ASL_LEVEL_ERR, "MallocXzoneDeferLarge must be one of "
					"0,1 - got %ld\n", value);
		}
	}

	// Known processes do not get deferred reclamation
	if ((security_critical && !security_critical_max_perf) ||
			malloc_space_efficient_enabled) {
		defer_tiny = false;
		defer_small = false;
		defer_large = false;
		huge_cache_size = 0;
	}

	if (huge_cache_size && !defer_large) {
		// Deferral of xzones is only supported in conjunction with large/huge
		malloc_report(ASL_LEVEL_ERR, "Huge cache requires deferred reclamation "
				"for large.\n");
		defer_large = true;
	}

	if ((defer_tiny || defer_small) && !defer_large) {
		// Deferral of xzones is only supported in conjunction with large/huge
		malloc_report(ASL_LEVEL_ERR, "Deferred reclamation cannot be used for "
				"xzones without large\n");
		defer_large = true;
	}
#else // CONFIG_XZM_DEFERRED_RECLAIM
	const uint16_t huge_cache_size = 0;
	const bool defer_tiny = false;
	const bool defer_small = false;
	const bool defer_large = false;
#endif // CONFIG_XZM_DEFERRED_RECLAIM

	bool small_freelist = false;
	bool guards_enabled = false;
	bool guards_enabled_data = false;
	uint8_t tiny_run_size = 0;
	uint8_t tiny_guard_density = 0;
	uint8_t small_run_size = 0;
	uint8_t small_guard_density = 0;
	uint8_t batch_size = 0;
#if TARGET_OS_OSX
	batch_size = !malloc_space_efficient_enabled ? 10 : 0;
	small_freelist = (!security_critical || security_critical_max_perf)
			&& !malloc_space_efficient_enabled;
#elif TARGET_OS_VISION
	batch_size = vision_max_perf ? 10 : 0;
#endif

	// Default config:
	// - Guards enabled and batching disabled for security critical (read,
	//   known) processes
	// - Data guarded
	// - Tiny max run size = 8
	// - Tiny density = 64 (20% guard density)
	// - Small max run size = 3
	// - Small density = 32 (11% on 4-page small chunk config)

	if (security_critical) {
		guards_enabled = true;
	}

	if (security_critical && !security_critical_max_perf) {
		batch_size = 0;
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneGuarded");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			guards_enabled = (bool)value;
		}
	}

	flag = malloc_common_value_for_key_copy(bootargs,
			xzone_guard_pages_boot_arg, value_buf, sizeof(value_buf));
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			guards_enabled = (bool)value;
		} else {
			malloc_report(ASL_LEVEL_ERR, "%s must be 0 or 1.\n",
					xzone_guard_pages_boot_arg);
		}
	}

	if (guards_enabled) {
		guards_enabled_data = true;
		flag = _xzm_development_only_getenv(envp, "MallocXzoneGuardedData");
		if (flag) {
			long value = strtol(flag, NULL, 10);
			if (value == 0 || value == 1) {
				guards_enabled_data = (bool)value;
			}
		}

		tiny_run_size = 8;
		flag = _xzm_development_only_getenv(envp, "MallocXzoneGuardTinyRun");
		if (flag) {
			long value = strtol(flag, NULL, 10);
			if (value > 0) {
				tiny_run_size = value;
			}
		}

		tiny_guard_density = 64;
		flag = _xzm_development_only_getenv(envp,
				"MallocXzoneGuardTinyDensity");
		if (flag) {
			long value = strtol(flag, NULL, 10);
			if (value > 0) {
				tiny_guard_density = value;
			}
		}

		small_run_size = 3;
		flag = _xzm_development_only_getenv(envp, "MallocXzoneGuardSmallRun");
		if (flag) {
			long value = strtol(flag, NULL, 10);
			if (value > 0) {
				small_run_size = value;
			}
		}

		small_guard_density = 32;
		flag = _xzm_development_only_getenv(envp,
				"MallocXzoneGuardSmallDensity");
		if (flag) {
			long value = strtol(flag, NULL, 10);
			if (value > 0) {
				small_guard_density = value;
			}
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneBatchSize");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < (1u << XZM_BATCH_SIZE_BITS)) {
			batch_size = value;
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocSmallFreelist");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			small_freelist = (bool)value;
		}
	}

#if CONFIG_XZM_CLUSTER_AWARE
	bool per_cluster_segment_groups = false;
#endif // CONFIG_XZM_CLUSTER_AWARE
#if TARGET_OS_OSX
	// On macOS, use per-cluster segment groups by default when not space
	// efficient
	per_cluster_segment_groups = !malloc_space_efficient_enabled;
#elif TARGET_OS_VISION && CONFIG_XZM_CLUSTER_AWARE
	per_cluster_segment_groups = vision_max_perf;
#endif
	size_t segment_group_ids_count = XZM_SEGMENT_GROUP_IDS_COUNT;
	flag = _xzm_development_only_getenv(envp, "MallocXzoneDataOnly");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 1) {
			segment_group_ids_count = XZM_SEGMENT_GROUP_IDS_COUNT_DATA_ONLY;
		} else if (value != 0) {
			malloc_report(ASL_LEVEL_ERR,
					"MallocXzoneDataOnly must be 0 or 1.\n");
		}
	}

# if CONFIG_XZM_CLUSTER_AWARE
	flag = _xzm_development_only_getenv(envp, "MallocXzonePerClusterSegmentGroups");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			per_cluster_segment_groups = value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"MallocXzonePerClusterSegmentGroups must be 0 or 1.\n");
		}
	}
# endif // CONFIG_XZM_CLUSTER_AWARE

	// By default, have two allocation fronts
	size_t allocation_front_count = 2;

	flag = _xzm_development_only_getenv(envp, "MallocXzoneAllocationFronts");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 1 || value == 2) {
			allocation_front_count = (size_t)value;
		} else {
			malloc_report(ASL_LEVEL_ERR,
					"Unsupported MallocXzoneAllocationFronts\n");
		}
	}

	xzm_slot_config_t max_list_config = XZM_SLOT_SINGLE;
#if TARGET_OS_OSX
	// On macOS, upgrade the max list config to the max slot config unless it
	// is a known process
	if ((!security_critical || security_critical_max_perf) &&
			!malloc_space_efficient_enabled) {
		max_list_config = max_slot_config;
	}
#endif // TARGET_OS_OSX
	xzm_slot_config_t list_config = XZM_SLOT_SINGLE;
	flag = _xzm_development_only_getenv(envp, "MallocXzoneListConfig");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < XZM_SLOT_LAST) {
			list_config = (xzm_slot_config_t)value;
		}
	}

	flag = _xzm_development_only_getenv(envp, "MallocXzoneMaxListConfig");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value >= 0 && value < XZM_SLOT_LAST && value < max_slot_config) {
			max_list_config = (xzm_slot_config_t)value;
		}
	}

	bool segment_deallocate = true;
#if TARGET_OS_OSX
	// On macOS, skip deallocation unless it is a known process
	if ((!security_critical || security_critical_max_perf) &&
			!malloc_space_efficient_enabled) {
		segment_deallocate = false;
	}
#endif // TARGET_OS_OSX
	flag = _xzm_development_only_getenv(envp, "MallocXzoneSegmentDeallocate");
	if (flag) {
		long value = strtol(flag, NULL, 10);
		if (value == 0 || value == 1) {
			segment_deallocate = (bool)value;
		}
	}

#if CONFIG_XZM_CLUSTER_AWARE
	// Known processes do not get per-cluster segment groups
	if (security_critical && !security_critical_max_perf) {
		per_cluster_segment_groups = false;
	}
#endif // CONFIG_XZM_CLUSTER_AWARE

	// There is one PTR range group per allocation front, and one PTR_LARGE
	// (when applicable) and DATA each globally
	size_t range_group_count = allocation_front_count +
			(XZM_RANGE_GROUP_COUNT - 1);
#if !CONFIG_XZM_CLUSTER_AWARE
	size_t segment_group_cluster_count = 1;
#else
	size_t segment_group_cluster_count = per_cluster_segment_groups ? ncpuclusters : 1;
#endif // !CONFIG_XZM_CLUSTER_AWARE

	size_t segment_group_front_count = segment_group_ids_count;
	if (segment_group_ids_count > XZM_SEGMENT_GROUP_IDS_COUNT_DATA_ONLY &&
			allocation_front_count > 1) {
		// If POINTER_XZONES is active it has N fronts
		segment_group_front_count += allocation_front_count - 1;
	}

	size_t segment_group_count = segment_group_front_count *
			segment_group_cluster_count;

	size_t metapool_count = XZM_METAPOOL_COUNT;

	size_t tail_allocation_offset = sizeof(struct xzm_main_malloc_zone_s);

	size_t main_xzones_offset = tail_allocation_offset;
	tail_allocation_offset += sizeof(struct xzm_xzone_s) * xzone_count;

	size_t main_xzone_slots_offset = tail_allocation_offset;
	size_t total_slot_count = xzone_count * slot_count;
	tail_allocation_offset +=
			sizeof(struct xzm_xzone_allocation_slot_s) * total_slot_count;

	size_t main_xzone_partial_offset = tail_allocation_offset;
	tail_allocation_offset += sizeof(struct xzm_chunk_list_s) * total_slot_count;

	size_t bin_sizes_offset = tail_allocation_offset;
	tail_allocation_offset += sizeof(uint64_t) * bin_count;

	size_t bin_bucket_counts_offset = tail_allocation_offset;
	tail_allocation_offset += sizeof(uint8_t) * bin_count;

	size_t xzone_bin_offsets_offset = tail_allocation_offset;
	tail_allocation_offset += sizeof(uint8_t) * bin_count;

	size_t isolation_zones_offset = tail_allocation_offset;
	tail_allocation_offset += sizeof(struct xzm_isolation_zone_s) * xzone_count;

	size_t range_groups_offset = tail_allocation_offset;
	tail_allocation_offset +=
			sizeof(struct xzm_range_group_s) * range_group_count;

	size_t segment_groups_offset = tail_allocation_offset;
	tail_allocation_offset +=
			sizeof(struct xzm_segment_group_s) * segment_group_count;

	size_t metapools_offset = tail_allocation_offset;
	tail_allocation_offset += sizeof(struct xzm_metapool_s) * metapool_count;

	size_t segment_table_offset = tail_allocation_offset;
	tail_allocation_offset +=
			sizeof(xzm_segment_table_entry_s) * XZM_SEGMENT_TABLE_ENTRIES;

#if CONFIG_EXTERNAL_METADATA_LARGE
	size_t extended_segment_table_offset = tail_allocation_offset;
	tail_allocation_offset += sizeof(xzm_extended_segment_table_entry_s) *
			XZM_EXTENDED_SEGMENT_TABLE_ENTRIES;
#endif // CONFIG_EXTERNAL_METADATA_LARGE

	size_t total_size = tail_allocation_offset;

	int flags = VM_FLAGS_ANYWHERE;
	int tag = 0;
	plat_map_t *plat_map_ptr = NULL;
	tag = VM_MEMORY_MALLOC;
	mach_vm_address_t vm_addr = (mach_vm_address_t)mvm_allocate_plat(0,
			total_size, 0, flags, MALLOC_GUARDED_METADATA, tag, plat_map_ptr);
	if (!vm_addr) {
		xzm_abort("Failed to allocate xzm zone");
	}

	xzm_main_malloc_zone_t main = (xzm_main_malloc_zone_t)vm_addr;
	xzm_xzone_t main_xzones_ptr = (xzm_xzone_t)((uintptr_t)main + main_xzones_offset);
	xzm_xzone_allocation_slot_t main_xzone_slots_ptr = (xzm_xzone_allocation_slot_t)
		((uint8_t*)main + main_xzone_slots_offset);

	xzm_chunk_list_t partial_lists = (xzm_chunk_list_t)((uint8_t *)main +
			main_xzone_partial_offset);

	xzm_mzone_index_t mzone_idx = XZM_MZONE_INDEX_MAIN;

	*main = (struct xzm_main_malloc_zone_s){
		.xzmz_total_size = total_size,
		.xzmz_bucketing_keys = bucketing_keys,
#if XZM_NARROW_BUCKETING
		.xzmz_narrow_bucketing = narrow_bucketing,
#endif
		.xzmz_madvise_workaround = madvise_workaround,
		.xzmz_defer_tiny = defer_tiny,
		.xzmz_defer_small = defer_small,
		.xzmz_defer_large = defer_large,
		.xzmz_deallocate_segment = segment_deallocate,
		.xzmz_range_group_count = (uint8_t)range_group_count,
		.xzmz_segment_group_ids_count = segment_group_ids_count,
		.xzmz_segment_group_front_count = segment_group_front_count,
		.xzmz_segment_group_count = segment_group_count,
		.xzmz_metapool_count = (uint8_t)metapool_count,
		.xzmz_allocation_front_count = (uint8_t)allocation_front_count,
		.xzmz_mfm_address = mfm_zone_address(),
		.xzmz_batch_size = batch_size,
		.xzmz_bin_count = (uint8_t)bin_count,
		.xzmz_ptr_bucket_count = (uint8_t)ptr_bucket_count,
		.xzmz_xzone_chunk_threshold = (uint8_t)chunk_threshold,
		.xzmz_xzone_bin_sizes = (uint64_t *)
				((uintptr_t)main + bin_sizes_offset),
		.xzmz_xzone_bin_bucket_counts = (uint8_t *)
				((uintptr_t)main + bin_bucket_counts_offset),
		.xzmz_xzone_bin_offsets = (uint8_t *)
				((uintptr_t)main + xzone_bin_offsets_offset),
		.xzmz_isolation_zones = (struct xzm_isolation_zone_s *)
				((uintptr_t)main + isolation_zones_offset),
		.xzmz_range_groups = (struct xzm_range_group_s *)
				((uintptr_t)main + range_groups_offset),
		.xzmz_segment_groups = (struct xzm_segment_group_s *)
				((uintptr_t)main + segment_groups_offset),
		.xzmz_metapools = (struct xzm_metapool_s *)(
				(uintptr_t)main + metapools_offset),
		.xzmz_segment_table = (xzm_segment_table_entry_s *)
				((uintptr_t)main + segment_table_offset),
#if CONFIG_EXTERNAL_METADATA_LARGE
		.xzmz_extended_segment_table_entries =
				XZM_EXTENDED_SEGMENT_TABLE_ENTRIES,
		.xzmz_extended_segment_table = (xzm_extended_segment_table_entry_s *)
				((uintptr_t)main + extended_segment_table_offset),
#endif // CONFIG_EXTERNAL_METADATA_LARGE
		.xzmz_max_mzone_idx = mzone_idx,
		.xzmz_mzones_lock = _MALLOC_LOCK_INIT,
		.xzmz_guard_config = {
			.xgpc_enabled = guards_enabled,
			.xgpc_enabled_for_data = guards_enabled_data,
			.xgpc_max_run_tiny = tiny_run_size,
			.xgpc_tiny_guard_density = tiny_guard_density,
			.xgpc_max_run_small = small_run_size,
			.xgpc_small_guard_density = small_guard_density,
		},
		.xzmz_thread_cache_list =
				LIST_HEAD_INITIALIZER(main->xzmz_thread_cache_list),
	};
	_xzm_initialize_const_zone_data(&main->xzmz_base, total_size, mzone_idx,
			xzone_count, slot_count, main_xzones_ptr, main_xzone_slots_ptr,
			NULL, slot_config, slot_threshold, max_slot_config,
			list_upgrade_threshold_single, list_upgrade_threshold_cluster,
			list_upgrade_period, slot_upgrade_threshold_single,
			slot_upgrade_threshold_cluster, slot_upgrade_period,
			tiny_thrash_threshold, small_thrash_threshold,
			small_thrash_limit_size, debug_flags, small_freelist,
			max_list_config, partial_lists);
#if CONFIG_MTE
	main->xzmz_base.xzz_memtag_config = memtag_config;
#endif // CONFIG_MTE

#if CONFIG_XZM_DEFERRED_RECLAIM
	if (defer_tiny || defer_small || defer_large || huge_cache_size) {
		bool success = xzm_reclaim_init(main, reclaim_buffer_count,
				max_reclaim_buffer_count);
		if (!success) {
			huge_cache_size = 0;
			main->xzmz_defer_tiny = defer_tiny = false;
			main->xzmz_defer_small = defer_small = false;
			main->xzmz_defer_large = defer_large = false;
		}
	}
#endif // CONFIG_XZM_DEFERRED_RECLAIM

	main->xzmz_base.xzz_thread_cache_enabled = thread_caching;
	main->xzmz_base.xzz_thread_cache_xzone_activation_period =
			thread_cache_activation_period;
	main->xzmz_base.xzz_thread_cache_xzone_activation_contentions =
			thread_cache_activation_contentions;
	main->xzmz_base.xzz_thread_cache_xzone_activation_time =
			thread_cache_activation_time;

#if CONFIG_XZM_THREAD_CACHE
	if (thread_caching) {
		int rc = pthread_key_init_np(__TSD_MALLOC_XZONE_THREAD_CACHE,
				_xzm_xzone_thread_cache_destructor);
		if (os_unlikely(rc)) {
			xzm_abort_with_reason("pthread_key_init_np failed", rc);
		}
	}
#endif // CONFIG_XZM_THREAD_CACHE

	// Initialize the main mzone structures
	for (size_t i = 0; i < bin_count; i++) {
		main->xzmz_xzone_bin_sizes[i] = _xzm_bin_sizes[i];
	}

	for (size_t i = 0; i < bin_count; i++) {
		main->xzmz_xzone_bin_bucket_counts[i] = buckets_per_bin;
	}

	for (size_t i = 0; i < xzone_count; i++) {
		xzm_isolation_zone_t iz = &main->xzmz_isolation_zones[i];
		_malloc_lock_init(&iz->xziz_lock);
	}

	size_t rg_idx = 0;
	for (size_t i = 0; i < XZM_RANGE_GROUP_COUNT; i++) {
		xzm_range_group_id_t rgid = (xzm_range_group_id_t)i;
		size_t rg_fronts = (rgid == XZM_RANGE_GROUP_PTR) ?
				allocation_front_count : 1;
		for (size_t j = 0; j < rg_fronts; j++) {
			xzm_range_group_t rg = &main->xzmz_range_groups[rg_idx];
			rg->xzrg_id = rgid;
			rg->xzrg_front = (xzm_front_index_t)j;
			rg->xzrg_main_ref = main;
			_malloc_lock_init(&rg->xzrg_lock);

			rg_idx++;
		}
	}

	if (has_vm_user_ranges) {
		xzm_main_malloc_zone_init_range_groups(main);
	}

	if (!main->xzmz_use_ranges) {
		// If we don't actually have ranges we can't do allocation fronts
		allocation_front_count = 1;
		main->xzmz_allocation_front_count = 1;
	}

	for (size_t i = 0; i < segment_group_count; i++) {
		xzm_segment_group_t sg = &main->xzmz_segment_groups[i];
		size_t sg_front_idx = i % segment_group_front_count;
		sg->xzsg_id = sg_front_idx < XZM_SEGMENT_GROUP_POINTER_XZONES ?
				(xzm_segment_group_id_t)sg_front_idx :
				XZM_SEGMENT_GROUP_POINTER_XZONES;

		_malloc_lock_init(&sg->xzsg_lock);
		_malloc_lock_init(&sg->xzsg_alloc_lock);

		if (_xzm_segment_group_id_is_data(sg->xzsg_id)) {
			sg->xzsg_range_group =
					&main->xzmz_range_groups[XZM_RANGE_GROUP_DATA];
		} else if (sg->xzsg_id == XZM_SEGMENT_GROUP_POINTER_LARGE) {

			if (!sg->xzsg_range_group) {
				xzm_front_index_t front = _xzm_random_front_index(front_random,
						allocation_front_count, XZM_XZONE_INDEX_INVALID);
				size_t rg_idx = XZM_RANGE_GROUP_PTR + front;
				sg->xzsg_range_group = &main->xzmz_range_groups[rg_idx];
			}
		} else {
			xzm_assert(sg->xzsg_id == XZM_SEGMENT_GROUP_POINTER_XZONES);
			size_t ptr_front_idx =
					sg_front_idx - XZM_SEGMENT_GROUP_POINTER_XZONES;
			size_t rg_idx = XZM_RANGE_GROUP_PTR + ptr_front_idx;
			xzm_debug_assert(rg_idx < main->xzmz_range_group_count);
			sg->xzsg_range_group = &main->xzmz_range_groups[rg_idx];
		}

		sg->xzsg_front = sg->xzsg_range_group->xzrg_front;
		sg->xzsg_main_ref = main;

		if (i == XZM_SEGMENT_GROUP_DATA_LARGE) {
			// Huge allocations are all routed to the same segment group,
			// so only initialize the huge cache for that one segment group
			sg->xzsg_cache = (struct xzm_segment_cache_s){
				.xzsc_max_count = huge_cache_size,
				.xzsc_count = 0,
				// TODO: consider limiting the maximum entry size based on the
				// reclaim threshold
				.xzsc_max_entry_slices =
						(huge_cache_max_entry_bytes / XZM_SEGMENT_SLICE_SIZE),
			};
			TAILQ_INIT(&sg->xzsg_cache.xzsc_head);
			_malloc_lock_init(&sg->xzsg_cache.xzsc_lock);
		}

		for (size_t j = 0; j < XZM_SPAN_QUEUE_COUNT; j++) {
			sg->xzsg_spans[j].xzsq_slice_count =
					(xzm_slice_count_t)_xzm_span_queue_slice_counts[j];
		}
	}

	// NOTE: The order of these metapool allocators matters for the fork lock,
	// we have to grab the metadata metapool last, since the other metapools
	// acquire its lock after taking their own
	xzm_metapool_t metadata_mp = &main->xzmz_metapools[XZM_METAPOOL_METADATA];
	uint32_t mp_metadata_size = MAX(sizeof(struct xzm_metapool_block_s),
			sizeof(struct xzm_metapool_slab_s));
	uint32_t mp_metadata_slab_size = KiB(16); // arbitrary slab size
	xzm_metapool_init(metadata_mp, XZM_METAPOOL_METADATA, VM_MEMORY_MALLOC,
			mp_metadata_slab_size, mp_metadata_size, mp_metadata_size, NULL);

	xzm_metapool_t segment_mp = &main->xzmz_metapools[XZM_METAPOOL_SEGMENT];
	xzm_metapool_init(segment_mp, XZM_METAPOOL_SEGMENT, VM_MEMORY_MALLOC,
			XZM_METAPOOL_SEGMENT_SLAB_SIZE, XZM_METAPOOL_SEGMENT_ALIGN,
			XZM_METAPOOL_SEGMENT_BLOCK_SIZE, metadata_mp);

	xzm_metapool_t leaf_table_mp = &main->xzmz_metapools[XZM_METAPOOL_SEGMENT_TABLE];
	xzm_metapool_init(leaf_table_mp, XZM_METAPOOL_SEGMENT_TABLE,
			VM_MEMORY_MALLOC, XZM_METAPOOL_SEGMENT_TABLE_SLAB_SIZE,
			XZM_SEGMENT_TABLE_ALIGN, XZM_SEGMENT_TABLE_SIZE, metadata_mp);

	xzm_metapool_t mzone_idx_mp = &main->xzmz_metapools[XZM_METAPOOL_MZONE_IDX];
	xzm_metapool_init(mzone_idx_mp, XZM_METAPOOL_MZONE_IDX, VM_MEMORY_MALLOC,
			XZM_METAPOOL_MZIDX_SLAB_SIZE, XZM_METAPOOL_MZIDX_BLOCK_ALIGN,
			XZM_METAPOOL_MZIDX_BLOCK_SIZE, NULL);

	xzm_metapool_t tc_mp = &main->xzmz_metapools[XZM_METAPOOL_THREAD_CACHE];

	// XXX Needs to be re-worked if we allow per-bin bucket counts
	xzm_debug_assert(_xzm_bin_sizes[XZM_THREAD_CACHE_BINS - 1] ==
			XZM_THREAD_CACHE_THRESHOLD);
	size_t thread_cache_xzone_count =
			1 + (XZM_THREAD_CACHE_BINS * buckets_per_bin);

	main->xzmz_base.xzz_thread_cache_xzone_count =
			(uint8_t)thread_cache_xzone_count;

	size_t thread_cache_block_size = sizeof(struct xzm_thread_cache_s) +
			(thread_cache_xzone_count * sizeof(xzm_xzone_thread_cache_u));
	xzm_metapool_init(tc_mp, XZM_METAPOOL_THREAD_CACHE,
			VM_MEMORY_MALLOC_NANO,
			XZM_METAPOOL_THREAD_CACHE_SLAB_SIZE, 0,
			(uint32_t)thread_cache_block_size, metadata_mp);

	// Initialize the per-mzone structures
	bool data_only = (main->xzmz_segment_group_ids_count ==
			XZM_SEGMENT_GROUP_IDS_COUNT_DATA_ONLY);
	_xzm_initialize_xzone_data(&main->xzmz_base, list_config, &main->xzmz_guard_config, front_random, data_only);

	flag = _xzm_production_getenv(envp, "MallocReportConfig");
	if (flag) {
		// Report our config to stderr for debugging purposes
#if CONFIG_XZM_DEFERRED_RECLAIM
		int vm_reclaim_enabled;
		size_t vm_reclaim_enabled_size = sizeof(vm_reclaim_enabled);
		int kr = sysctlbyname("vm.reclaim.enabled",
				&vm_reclaim_enabled, &vm_reclaim_enabled_size, 0, 0);
#endif // CONFIG_XZM_DEFERRED_RECLAIM
		malloc_report(ASL_LEVEL_INFO,
				"XZM Config:\n"
				"\tData Only: %d\n"
				"\tAllocation Fronts: %d\n"
#if XZM_NARROW_BUCKETING
				"\tNarrow Bucketing: %d\n"
#endif
				"\tGuards Enabled: %d\n"
				"\tScribble: %d\n"
				"\tTiny/Small Batch Max: %d\n"
#if CONFIG_XZM_DEFERRED_RECLAIM
				"\tDefer Tiny: %d\n"
				"\tDefer Small: %d\n"
				"\tDefer Large: %d\n"
				"\tHuge Cache Size: %d\n"
				"\tHuge Cache Max Entry Bytes: %u\n"
				"\tReclaim Buffer Count: %u/%u (%s)\n"
#endif // CONFIG_XZM_DEFERRED_RECLAIM
				"\tSmall Freelist: %u\n"
#if CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES
				"\tData Range: 0x%llx/%lu\n"
				"\tPointer Range 1: 0x%llx/%lu\n"
				"\tPointer Range 2: 0x%llx/%lu\n"
#endif // CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES
				"\tEarly Allocator: %s\n"
				"\tSegment Deallocate: %u\n"
#if CONFIG_MTE
				"\tMTE (enabled/data/max size): %d/%d/%llu\n"
#endif
				"\tInitial Slot Config: %s/%s (Chunk, Size Thresholds: %u, %u)\n"
				"\tInitial List Config: %s/%s\n"
				"\tList Upgrade Thresholds: %d/%d, %d/%d\n"
				"\tSlot Upgrade Thresholds: %d/%d, %d/%d\n"
				"\tTiny Thrash Threshold: %llu ms\n"
				"\tSmall Thrash Threshold: %llu ms, %llu bytes\n"
#if CONFIG_XZM_THREAD_CACHE
				"\tThread Caching: %s (%u allocs, %u contentions, %llu ms)\n"
#endif
				"\tPointer Bucket Count: %lu\n",
				data_only,
				(int)allocation_front_count,
#if XZM_NARROW_BUCKETING
				main->xzmz_narrow_bucketing,
#endif
				guards_enabled, !!(debug_flags & MALLOC_DO_SCRIBBLE),
				batch_size,
#if CONFIG_XZM_DEFERRED_RECLAIM
				defer_tiny, defer_small, defer_large,
				huge_cache_size, huge_cache_max_entry_bytes,
				reclaim_buffer_count, max_reclaim_buffer_count,
				(kr || !vm_reclaim_enabled) ? "DISABLED": "ENABLED",
#endif // CONFIG_XZM_DEFERRED_RECLAIM
				small_freelist,
#if CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES
				!malloc_internal_security_policy ? 0 :
						main->xzmz_range_groups[XZM_RANGE_GROUP_DATA].xzrg_base,
				main->xzmz_range_groups[XZM_RANGE_GROUP_DATA].xzrg_size,
				!malloc_internal_security_policy ? 0 :
						main->xzmz_range_groups[XZM_RANGE_GROUP_PTR + 0].xzrg_base,
				main->xzmz_range_groups[XZM_RANGE_GROUP_PTR + 0].xzrg_size,
				!malloc_internal_security_policy ||
						main->xzmz_allocation_front_count < 2 ? 0 :
						main->xzmz_range_groups[XZM_RANGE_GROUP_PTR + 1].xzrg_base,
				main->xzmz_allocation_front_count < 2 ? 0 :
						main->xzmz_range_groups[XZM_RANGE_GROUP_PTR + 1].xzrg_size,
#endif // CONFIG_VM_USER_RANGES || CONFIG_MACOS_RANGES
				main->xzmz_mfm_address ? "enabled" : "disabled",
				segment_deallocate,
#if CONFIG_MTE
				memtag_config.enabled, memtag_config.tag_data,
				memtag_config.max_block_size,
#endif
				_xzm_slot_config_to_string(slot_config),
				_xzm_slot_config_to_string(max_slot_config),
				chunk_threshold, slot_threshold,
				_xzm_slot_config_to_string(list_config),
				_xzm_slot_config_to_string(max_list_config),
				list_upgrade_threshold_single,
				list_upgrade_period, list_upgrade_threshold_cluster,
				list_upgrade_period, slot_upgrade_threshold_single,
				slot_upgrade_period, slot_upgrade_threshold_cluster,
				slot_upgrade_period,
				tiny_thrash_threshold_ns / nsec_per_msec,
				small_thrash_threshold_ns / nsec_per_msec,
				small_thrash_limit_size,
#if CONFIG_XZM_THREAD_CACHE
				thread_caching ? "enabled" : "disabled",
				thread_cache_activation_period,
				thread_cache_activation_contentions,
				thread_cache_activation_time_ns / nsec_per_msec,
#endif // CONFIG_XZM_THREAD_CACHE
				(unsigned long)ptr_bucket_count);
	}

	return &main->xzmz_base.xzz_basic_zone;
}

MALLOC_NOEXPORT
malloc_zone_t *
xzm_malloc_zone_create(unsigned debug_flags, xzm_main_malloc_zone_t main_ref)
{
	size_t tail_allocation_offset = sizeof(struct xzm_malloc_zone_s);
	size_t xzones_offset = tail_allocation_offset;

	uint8_t xzone_count = main_ref->xzmz_base.xzz_xzone_count;
	uint8_t slot_count = main_ref->xzmz_base.xzz_slot_count;
	tail_allocation_offset += sizeof(struct xzm_xzone_s) * xzone_count;
	size_t slots_offset = tail_allocation_offset;

	size_t total_slot_count = xzone_count * slot_count;
	tail_allocation_offset += sizeof(struct xzm_xzone_allocation_slot_s) *
			total_slot_count;

	size_t xzone_partial_offset = tail_allocation_offset;
	tail_allocation_offset += sizeof(struct xzm_chunk_list_s) * total_slot_count;

	// Try to pop a previously destroyed mzone index from the reuse list,
	// otherwise use the next unused value
	_malloc_lock_lock(&main_ref->xzmz_mzones_lock);
	xzm_mzone_index_t mzone_idx;
	xzm_reused_mzone_index_t reuse = SLIST_FIRST(&main_ref->xzmz_reusable_mzidxq);
	if (reuse) {
		SLIST_REMOVE_HEAD(&main_ref->xzmz_reusable_mzidxq, xrmi_mzone_entry);
		mzone_idx = reuse->xrmi_mzone_idx;
		xzm_metapool_t mp = &main_ref->xzmz_metapools[XZM_METAPOOL_MZONE_IDX];
		xzm_metapool_free(mp, reuse);
	} else if (main_ref->xzmz_max_mzone_idx == XZM_MZONE_INDEX_MAX) {
		mzone_idx = XZM_MZONE_INDEX_INVALID;
	} else {
		main_ref->xzmz_max_mzone_idx += 1;
		mzone_idx = main_ref->xzmz_max_mzone_idx;
	}
	_malloc_lock_unlock(&main_ref->xzmz_mzones_lock);

	if (os_unlikely(mzone_idx == XZM_MZONE_INDEX_INVALID)) {
		return NULL;
	}

	// allocate a new mzone
	int flags = VM_FLAGS_ANYWHERE;
	int tag = 0;
	plat_map_t *plat_map_ptr = NULL;
	tag = VM_MEMORY_MALLOC;

	mach_vm_address_t vm_addr = (mach_vm_address_t)mvm_allocate_plat(0,
			tail_allocation_offset, 0, flags, MALLOC_GUARDED_METADATA, tag,
			plat_map_ptr);
	if (!vm_addr) {
		return NULL;
	}
	xzm_malloc_zone_t new_zone = (xzm_malloc_zone_t)vm_addr;

	xzm_chunk_list_t partial_lists = (xzm_chunk_list_t)((uint8_t *)new_zone +
			xzone_partial_offset);

	// Currently we copy the slot upgrade parameters from the main zone into
	// newly created zones. To limit fragmentation, we could cap the new
	// zones at single/per cluster. Pre-xzm, new malloc zones were all
	// scalable zones (you could only have one nano zone), so capping
	// scalability of non-default zones has prior art
	_xzm_initialize_const_zone_data(new_zone, tail_allocation_offset, mzone_idx,
			xzone_count, slot_count,
			(xzm_xzone_t)((uintptr_t)new_zone + xzones_offset),
			(xzm_xzone_allocation_slot_t)((uintptr_t)new_zone + slots_offset),
			main_ref, main_ref->xzmz_base.xzz_initial_slot_config,
			main_ref->xzmz_base.xzz_slot_initial_threshold,
			main_ref->xzmz_base.xzz_max_slot_config,
			main_ref->xzmz_base.xzz_list_upgrade_threshold[0],
			main_ref->xzmz_base.xzz_list_upgrade_threshold[1],
			main_ref->xzmz_base.xzz_list_upgrade_period,
			main_ref->xzmz_base.xzz_slot_upgrade_threshold[0],
			main_ref->xzmz_base.xzz_slot_upgrade_threshold[1],
			main_ref->xzmz_base.xzz_slot_upgrade_period,
			main_ref->xzmz_base.xzz_tiny_thrash_threshold,
			/* small_thrash_threshold */ 0, /* small_thrash_limit_size */ 0,
			debug_flags, main_ref->xzmz_base.xzz_small_freelist_enabled,
			main_ref->xzmz_base.xzz_max_list_config, partial_lists);
#if CONFIG_MTE
	// TODO: Do we want a different MTE config in new zones?
	new_zone->xzz_memtag_config = main_ref->xzmz_base.xzz_memtag_config;
#endif // CONFIG_MTE

	xzm_debug_assert(new_zone != NULL);

	bool data_only = (main_ref->xzmz_segment_group_ids_count ==
			XZM_SEGMENT_GROUP_IDS_COUNT_DATA_ONLY);
	// Currently new zones are never nano, so start all new xzone slot configs
	// at single.
	// TODO: if the envvar is set (MallocXzoneInitialSlotConfig), use
	// that slot config in non-default zones
	_xzm_initialize_xzone_data(new_zone, XZM_SLOT_SINGLE,
			&main_ref->xzmz_guard_config, NULL, data_only);
	LIST_INIT(&new_zone->xzz_chunkq_large);

	return &new_zone->xzz_basic_zone;
}

#endif // CONFIG_XZONE_MALLOC
