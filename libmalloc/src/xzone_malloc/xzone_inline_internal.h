/* ----------------------------------------------------------------------------
Copyright (c) 2018-2022, Microsoft Research, Daan Leijen
Copyright © 2025 Apple Inc.
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" in the same directory as this file.
-----------------------------------------------------------------------------*/

#if CONFIG_XZONE_MALLOC

#ifndef __XZONE_INLINE_INTERNAL_H__
#define __XZONE_INLINE_INTERNAL_H__

#if !__has_feature(bounds_safety)

#define xzm_abort(msg)  ({ \
	_os_set_crash_log_cause_and_message(0, "BUG IN LIBMALLOC: " msg); \
	__builtin_trap(); \
})
#define xzm_abort_with_reason(msg, reason)  ({ \
	_os_set_crash_log_cause_and_message((reason), "BUG IN LIBMALLOC: " msg); \
	__builtin_trap(); \
})
#define xzm_client_abort(msg)  ({ \
	_os_set_crash_log_cause_and_message(0, "BUG IN CLIENT OF LIBMALLOC: " msg); \
	__builtin_trap(); \
})
#define xzm_client_abort_with_reason(msg, reason)  ({ \
	_os_set_crash_log_cause_and_message((reason), "BUG IN CLIENT OF LIBMALLOC: " msg); \
	__builtin_trap(); \
})

#define _xzm_assert_stringify(x) #x
#define xzm_assert_stringify(x) _xzm_assert_stringify(x)

// mimalloc: mi_assert
#define xzm_assert(pred) \
	if (os_unlikely(!(pred))) { \
		xzm_abort("malloc assertion \"" #pred "\" failed " \
				"(" __FILE__ ":" xzm_assert_stringify(__LINE__) ")"); \
	}

// mimalloc: mi_assert_internal
#ifdef DEBUG
#define xzm_debug_assert xzm_assert
#define xzm_debug_abort xzm_abort
#define xzm_debug_abort_with_reason xzm_abort_with_reason
#ifndef __assert_only
#define __assert_only
#endif
#else // DEBUG
#define xzm_debug_assert(...)
#define xzm_debug_abort(...)
#define xzm_debug_abort_with_reason(...)
#ifndef __assert_only
#define __assert_only __unused
#endif
#endif // DEBUG

MALLOC_INLINE
static void
_xzm_corruption_detected(void *corrupt_block)
{
	// TODO: load the corrupt value into a register so it also appears in crash
	// reports
	xzm_client_abort_with_reason("memory corruption of free block",
			corrupt_block);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_main_malloc_zone_t
_xzm_malloc_zone_main(xzm_malloc_zone_t zone)
{
	return zone->xzz_main_ref ?: (xzm_main_malloc_zone_t)zone;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_malloc_zone_is_main(xzm_malloc_zone_t zone)
{
	return !zone->xzz_main_ref;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_malloc_zone_is_xzm(malloc_zone_t *zone)
{
	return zone->version >= 14 &&
			zone->introspect->zone_type == MALLOC_ZONE_TYPE_XZONE;
}

#pragma mark magic math

// see https://lemire.me/blog/2019/02/20/more-fun-with-fast-remainders-when-the-divisor-is-a-constant/
//
// Implementation copied from zalloc
#define XZM_MAGIC_QUO(s)      (((1ull << 32) - 1) / (uint64_t)(s) + 1)
#define XZM_MAGIC_ALIGNED(s)  (~0u / (uint32_t)(s) + 1)

// Returns (offs / size) if offs is small enough and magic = XZM_MAGIC_QUO(size)
static inline uint32_t
XZM_FAST_QUO(uint64_t offs, uint64_t __unused size, uint64_t magic)
{
	uint32_t quo = (offs * magic) >> 32;
	xzm_debug_assert(offs / size == quo);
	return quo;
}

// Returns (offs % size) if offs is small enough and magic ==
// XZM_MAGIC_QUO(size)
static inline uint32_t
XZM_FAST_MOD(uint64_t offs, uint64_t magic, uint64_t size)
{
	uint32_t lowbits = (uint32_t)(offs * magic);

	uint32_t mod = (lowbits * size) >> 32;
	xzm_debug_assert(offs % size == mod);
	return mod;
}

// Returns whether (offs % size) == 0 if offs is small enough and magic ==
// XZM_MAGIC_ALIGNED(size)
static inline bool
XZM_FAST_ALIGNED(uint64_t offs, uint64_t __unused size, uint32_t magic)
{
	bool aligned = (uint32_t)(offs * magic) < magic;
	xzm_debug_assert(aligned == ((offs % size) == 0));
	return aligned;
}

#pragma mark metadata helpers

// mimalloc: mi_segment_map_index_of
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static size_t
_xzm_segment_table_index_of(const void *segment_body, size_t *extended_idx)
{
	uintptr_t segment_bits = (uintptr_t)segment_body;
	if (segment_bits >= XZM_LIMIT_ADDRESS) {
		*extended_idx = 0;
		return XZM_SEGMENT_TABLE_ENTRIES;
	}

	uintptr_t segindex = segment_bits / XZM_SEGMENT_SIZE;
#if CONFIG_EXTERNAL_METADATA_LARGE
	// The segment map index we return in the large address space is the index
	// into the bottom level table, the extended_idx is the index into top table
	// (setting extended_idx = 0 implies that the body is in the low 64GB of VA)
	*extended_idx = segindex / XZM_SEGMENT_TABLE_ENTRIES;
	xzm_debug_assert(*extended_idx < XZM_EXTENDED_SEGMENT_TABLE_ENTRIES);
	return segindex % XZM_SEGMENT_TABLE_ENTRIES;
#else
	*extended_idx = 0;
	xzm_debug_assert(segindex < XZM_SEGMENT_TABLE_ENTRIES);
	return segindex;
#endif // CONFIG_EXTERNAL_METADATA_LARGE
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_segment_t
_xzm_segment_table_entry_to_segment(xzm_segment_table_entry_s entry)
{
	return (xzm_segment_t)
			((uintptr_t)entry.xste_val << XZM_METAPOOL_SEGMENT_BLOCK_SHIFT);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_segment_table_entry_s
_xzm_segment_to_segment_table_entry(xzm_segment_t segment, bool normal)
{
	xzm_debug_assert((uintptr_t)segment % XZM_METAPOOL_SEGMENT_ALIGN == 0);
	// TODO: On MacOS (47 bit address space), the upper portion of the address
	// space won't fit in this encoding
	xzm_assert(((uintptr_t)segment >> XZM_METAPOOL_SEGMENT_BLOCK_SHIFT) <
			XZM_SEGMENT_TABLE_LIMIT_ENTRY);
	return (xzm_segment_table_entry_s) {
		.xste_val = (uint32_t)
				((uintptr_t)segment >> XZM_METAPOOL_SEGMENT_BLOCK_SHIFT),
		.xste_normal = normal,
	};
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_segment_table_entry_s *
_xzm_ptr_to_table_entry(const void *segment_body,
		xzm_main_malloc_zone_t main)
{
	size_t ext_idx = 0;
	size_t index = _xzm_segment_table_index_of(segment_body, &ext_idx);
#if CONFIG_EXTERNAL_METADATA_LARGE
	if (ext_idx == 0) {
		if (os_unlikely(index >= XZM_SEGMENT_TABLE_ENTRIES)) {
			// Pointer out of bounds, greater than XZM_LIMIT_ADDRESS
			return NULL;
		}
		// This pointer is in the first 64GB of VA, so it comes directly from
		// the segment table in the main zone
		xzm_debug_assert((uintptr_t)segment_body < XZM_SEGMENT_TABLE_COVERAGE);
		return &main->xzmz_segment_table[index];
	} else if (ext_idx >= XZM_EXTENDED_SEGMENT_TABLE_ENTRIES) {
		return NULL;
	} else {
		xzm_segment_table_entry_s *leaf_table;
		xzm_extended_segment_table_entry_s *map;
		map = main->xzmz_extended_segment_table;
		xzm_debug_assert(map != 0);
		leaf_table = (void *)((uintptr_t)(map[ext_idx].xeste_val) *
				XZM_SEGMENT_TABLE_ALIGN);
		if (leaf_table == NULL) {
			// there are no segments in the given 64GB span
			return NULL;
		}
		return &leaf_table[index];
	}
#else
	if (index >= XZM_SEGMENT_TABLE_ENTRIES) {
		return NULL;
	}
	return &main->xzmz_segment_table[index];
#endif // CONFIG_EXTERNAL_METADATA_LARGE

}

// mimalloc: _mi_segment_of
// Note: This will allow inner pointers, or any pointer inside the (4MB) segment
// granule of an allocated segment
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_segment_t
xzm_segment_table_query(xzm_main_malloc_zone_t main, const void *ptr)
{
	xzm_segment_table_entry_s *leaf_entry;
	leaf_entry = _xzm_ptr_to_table_entry(ptr, main);
	if (leaf_entry == NULL) {
		return NULL;
	}
	return _xzm_segment_table_entry_to_segment(*leaf_entry);
}

// mimalloc: _mi_page_segment
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_segment_t
_xzm_segment_for_slice(xzm_malloc_zone_t zone, xzm_slice_t slice)
{
	xzm_segment_t segment = (xzm_segment_t)
			((uintptr_t)slice & ~(XZM_METAPOOL_SEGMENT_BLOCK_SIZE - 1));
	xzm_debug_assert(!segment || (slice >= segment->xzs_slices &&
			slice < (segment->xzs_slices + segment->xzs_slice_entry_count)));
	return segment;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_segment_group_t
_xzm_segment_group_for_slice(xzm_malloc_zone_t zone, xzm_slice_t slice)
{
	xzm_segment_t segment = _xzm_segment_for_slice(zone, slice);
	return segment->xzs_segment_group;
}

// mimalloc: mi_slice_index
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_count_t
_xzm_slice_index(xzm_segment_t segment, xzm_slice_t slice)
{
	xzm_debug_assert((uintptr_t)slice >= (uintptr_t)segment->xzs_slices);
	ptrdiff_t index = slice - segment->xzs_slices;
	xzm_debug_assert(index < (ptrdiff_t)segment->xzs_slice_entry_count);
	return (xzm_slice_count_t)index;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static const char *
_xzm_range_group_id_to_string(xzm_range_group_id_t id)
{
	switch(id) {
	case XZM_RANGE_GROUP_DATA:
		return "data";
	case XZM_RANGE_GROUP_PTR:
		return "pointer";
	default:
		return "unknown";
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static const char *
_xzm_segment_group_id_to_string(xzm_segment_group_id_t id)
{
	switch(id) {
	case XZM_SEGMENT_GROUP_DATA:
		return "data";
	case XZM_SEGMENT_GROUP_DATA_LARGE:
		return "data_large";
	case XZM_SEGMENT_GROUP_POINTER_XZONES:
		return "pointer_xzones";
	case XZM_SEGMENT_GROUP_POINTER_LARGE:
		return "pointer_large";
	default:
		return "unknown";
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_segment_group_id_is_data(xzm_segment_group_id_t id)
{
	switch(id) {
	case XZM_SEGMENT_GROUP_DATA:
	case XZM_SEGMENT_GROUP_DATA_LARGE:
		return true;
	case XZM_SEGMENT_GROUP_POINTER_XZONES:
	case XZM_SEGMENT_GROUP_POINTER_LARGE:
		return false;
	default:
		xzm_abort_with_reason("unknown segment group id", id);
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_segment_slice_index_start(xzm_segment_t segment, xzm_slice_count_t idx)
{
	return (uint8_t *)((uintptr_t)segment->xzs_segment_body +
			(idx * XZM_SEGMENT_SLICE_SIZE));
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_segment_slice_start(xzm_segment_t segment, xzm_slice_t slice)
{
	return _xzm_segment_slice_index_start(segment,
			_xzm_slice_index(segment, slice));
	// TODO: mimalloc offset optimization for small block sizes?
}

// FIXME: `zone` parameter isn't used in _xzm_segment_for_slice(),
// _xzm_slice_start(), _xzm_chunk_start(), _xzm_chunk_start_ptr() and can be
// removed.

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_slice_start(xzm_malloc_zone_t zone, xzm_slice_t slice)
{
	return _xzm_segment_slice_start(_xzm_segment_for_slice(zone, slice), slice);
}

// mimalloc: _mi_page_start
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uintptr_t
_xzm_chunk_start(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		size_t *chunk_size_out)
{
	if (chunk_size_out) {
		switch (chunk->xzc_bits.xzcb_kind) {
		case XZM_SLICE_KIND_TINY_CHUNK:
			*chunk_size_out = XZM_TINY_CHUNK_SIZE;
			break;
		case XZM_SLICE_KIND_SMALL_CHUNK:
			*chunk_size_out = XZM_SMALL_CHUNK_SIZE;
			break;
		case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
			*chunk_size_out = XZM_SMALL_FREELIST_CHUNK_SIZE;
			break;
		case XZM_SLICE_KIND_LARGE_CHUNK:
		case XZM_SLICE_KIND_HUGE_CHUNK:
			*chunk_size_out = ((size_t)chunk->xzcs_slice_count) <<
					XZM_SEGMENT_SLICE_SHIFT;
			break;
		default:
			xzm_abort_with_reason("asking for start of chunk with invalid kind",
					(unsigned)chunk->xzc_bits.xzcb_kind);
		}
	}

	return (uintptr_t)_xzm_slice_start(zone, (xzm_slice_t)chunk);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_chunk_start_ptr(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		size_t *chunk_size_out)
{
	xzm_debug_assert(chunk_size_out);
	uintptr_t ptr = _xzm_chunk_start(zone, chunk, chunk_size_out);
	return (uint8_t *)ptr;
}


MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_count_t
_xzm_segment_slice_count(xzm_segment_t segment)
{
	return segment->xzs_slice_count;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static size_t
_xzm_segment_size(xzm_segment_t segment)
{
	xzm_slice_count_t body_slice_count = _xzm_segment_slice_count(segment);
	return (size_t)body_slice_count << XZM_SEGMENT_SLICE_SHIFT;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_segment_start(xzm_segment_t segment)
{
	return _xzm_segment_slice_index_start(segment, 0);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static size_t
_xzm_segment_slice_offset_of(xzm_segment_t segment, uintptr_t ptr)
{
	const ptrdiff_t diff = ptr - (uintptr_t)_xzm_segment_start(segment);
#ifdef DEBUG
	// Huge segments aren't always a multiple of the segment size, so it's
	// possible for malloc_size() to be passed a pointer that is within a
	// segment granule, but isn't within the segment that owns that granule. We
	// need to not crash in the debug dylib when that happens
	size_t rounded_size = roundup(_xzm_segment_size(segment), XZM_SEGMENT_SIZE);
	xzm_debug_assert(diff >= 0 && diff < (ptrdiff_t)rounded_size);
#endif // DEBUG
	return (size_t)diff;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_count_t
_xzm_segment_slice_offset_index(xzm_segment_t segment, size_t offset)
{
	return (xzm_slice_count_t)(offset >> XZM_SEGMENT_SLICE_SHIFT);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_count_t
_xzm_segment_slice_index_of(xzm_segment_t segment, uintptr_t ptr)
{
	size_t offset = _xzm_segment_slice_offset_of(segment, ptr);
	return _xzm_segment_slice_offset_index(segment, offset);
}

// mimalloc: _mi_segment_page_of
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_t
_xzm_segment_slice_of(xzm_segment_t segment, uintptr_t ptr)
{
	xzm_slice_count_t idx = _xzm_segment_slice_index_of(segment, ptr);
	if (os_likely(idx < segment->xzs_slice_entry_count)) {
		return &segment->xzs_slices[idx];
	} else {
		return NULL;
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_segment_slice_index_end(xzm_segment_t segment, xzm_slice_count_t idx)
{
	return _xzm_segment_slice_index_start(segment, idx + 1);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_segment_slice_end_of(xzm_segment_t segment, uintptr_t ptr)
{
	xzm_slice_count_t idx = _xzm_segment_slice_index_of(segment, ptr);
	return _xzm_segment_slice_index_end(segment, idx);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_segment_end(xzm_segment_t segment)
{
	return _xzm_segment_slice_index_end(segment, segment->xzs_slice_count - 1);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_segment_slice_start_of(xzm_segment_t segment, uintptr_t ptr)
{
	xzm_slice_count_t idx = _xzm_segment_slice_index_of(segment, ptr);
	return _xzm_segment_slice_index_start(segment, idx);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_t
_xzm_segment_slices_begin(xzm_segment_t segment)
{
	return &segment->xzs_slices[0];
}

// mimalloc: mi_segment_slices_end
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_t
_xzm_segment_slices_end(xzm_segment_t segment)
{
	// Return a one-past-the-end pointer without immediately trapping
	return &segment->xzs_slices[segment->xzs_slice_entry_count];
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static const char *
_xzm_segment_kind_to_string(xzm_segment_kind_t kind)
{
	switch(kind) {
	case XZM_SEGMENT_KIND_NORMAL:
		return "normal_segment";
	case XZM_SEGMENT_KIND_HUGE:
		return "huge_segment";
	default:
		return "unknown";
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static const char *
_xzm_metapool_id_to_string(xzm_metapool_id_t id)
{
	switch(id) {
	case XZM_METAPOOL_SEGMENT:
		return "segment metadata slab";
	case XZM_METAPOOL_SEGMENT_TABLE:
		return "segment table slab";
	case XZM_METAPOOL_MZONE_IDX:
		return "mzone index slab";
	case XZM_METAPOOL_THREAD_CACHE:
		return "thread cache slab";
	case XZM_METAPOOL_METADATA:
		return "metapool metadata slab";
	default:
		return "unknown slab";
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_span_contains_slice(xzm_slice_t span, xzm_slice_t slice)
{
	switch (span->xzc_bits.xzcb_kind) {
	case XZM_SLICE_KIND_TINY_CHUNK:
	case XZM_SLICE_KIND_SINGLE_FREE:
		return (span == slice);
	case XZM_SLICE_KIND_SMALL_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
	case XZM_SLICE_KIND_LARGE_CHUNK:
	case XZM_SLICE_KIND_HUGE_CHUNK:
	case XZM_SLICE_KIND_MULTI_FREE:
	case XZM_SLICE_KIND_GUARD:
		xzm_debug_assert(slice >= span);
		return (slice < span + span->xzcs_slice_count);
	default:
		return false;
	}
}

// mimalloc: mi_slice_first
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_t
_xzm_span_slice_first(xzm_slice_t slice)
{
	// "likely" in the sense that the most common case for lookups will be tiny
	// chunks
	if (os_likely(slice->xzc_bits.xzcb_kind != XZM_SLICE_KIND_MULTI_BODY)) {
		return slice;
	}

	xzm_slice_t out_slice = (xzm_slice_t)
			((uintptr_t)slice - slice->xzsl_slice_offset_bytes);

	xzm_debug_assert(out_slice >= ((xzm_segment_t)((uintptr_t)slice &
			~(XZM_METAPOOL_SEGMENT_BLOCK_SIZE - 1)))->xzs_slices);
	if (os_likely(_xzm_span_contains_slice(out_slice, slice))) {
		return out_slice;
	}
	// not contained in the span we refer to - leave it up to the caller to
	// handle this
	return slice;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_slice_kind_is_chunk(xzm_slice_kind_t kind)
{
	switch (kind) {
	case XZM_SLICE_KIND_INVALID:
	case XZM_SLICE_KIND_SINGLE_FREE:
	case XZM_SLICE_KIND_MULTI_FREE:
	case XZM_SLICE_KIND_MULTI_BODY:
		return false;
	case XZM_SLICE_KIND_TINY_CHUNK:
	case XZM_SLICE_KIND_SMALL_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
	case XZM_SLICE_KIND_LARGE_CHUNK:
	case XZM_SLICE_KIND_HUGE_CHUNK:
		return true;
	default:
		xzm_abort_with_reason("bad chunk kind", (unsigned)kind);
	}
}

// Like _xzm_slice_kind_is_chunk, but doesn't abort on totally bogus kinds
// (useful during enumeration when we have no guarantees about what we're
// looking at)
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_slice_kind_is_chunk_safe(xzm_slice_kind_t kind)
{
	switch (kind) {
	case XZM_SLICE_KIND_TINY_CHUNK:
	case XZM_SLICE_KIND_SMALL_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
	case XZM_SLICE_KIND_LARGE_CHUNK:
	case XZM_SLICE_KIND_HUGE_CHUNK:
		return true;
	default:
		return false;
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_slice_kind_uses_xzones(xzm_slice_kind_t kind)
{
	switch (kind) {
	case XZM_SLICE_KIND_TINY_CHUNK:
	case XZM_SLICE_KIND_SMALL_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
		return true;
	default:
		return false;
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_chunk_should_defer_reclamation(xzm_main_malloc_zone_t main,
		xzm_chunk_t chunk)
{
	xzm_debug_assert(_xzm_slice_kind_is_chunk(chunk->xzc_bits.xzcb_kind));
	switch (chunk->xzc_bits.xzcb_kind) {
	case XZM_SLICE_KIND_TINY_CHUNK:
		return main->xzmz_defer_tiny;
	case XZM_SLICE_KIND_SMALL_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
		return main->xzmz_defer_small;
	case XZM_SLICE_KIND_LARGE_CHUNK:
	case XZM_SLICE_KIND_HUGE_CHUNK:
		return main->xzmz_defer_large;
	default:
		xzm_abort("Attempt to check for deferred reclamation on "
				"non-chunk slice");
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static const char *
_xzm_slice_kind_to_string(xzm_slice_kind_t kind)
{
	switch (kind) {
	case XZM_SLICE_KIND_INVALID:
		return "invalid";
	case XZM_SLICE_KIND_SINGLE_FREE:
		return "single_free";
	case XZM_SLICE_KIND_MULTI_FREE:
		return "multi_free";
	case XZM_SLICE_KIND_MULTI_BODY:
		return "multi_body";
	case XZM_SLICE_KIND_TINY_CHUNK:
		return "tiny_chunk";
	case XZM_SLICE_KIND_SMALL_CHUNK:
		return "small_chunk";
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
		return "small_freelist_chunk";
	case XZM_SLICE_KIND_LARGE_CHUNK:
		return "large_chunk";
	case XZM_SLICE_KIND_HUGE_CHUNK:
		return "huge_chunk";
	case XZM_SLICE_KIND_GUARD:
		return "guard_page";
	default:
		return "unknown";
	}
}

// mimalloc: _mi_segment_page_of
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_chunk_t
_xzm_segment_chunk_of(xzm_segment_t segment, uintptr_t ptr)
{
	xzm_slice_t slice = _xzm_segment_slice_of(segment, ptr);
	if (!slice) {
		return NULL;
	}

	xzm_slice_t first = _xzm_span_slice_first(slice);
	return _xzm_slice_kind_is_chunk(first->xzc_bits.xzcb_kind) ? first : NULL;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static size_t
_xzm_segment_offset(xzm_segment_t segment, xzm_slice_count_t chunk_idx,
		xzm_block_index_t block_idx, uint64_t block_size)
{
	return chunk_idx * XZM_SEGMENT_SLICE_SIZE + block_idx * block_size;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_block_index_t
_xzm_segment_offset_chunk_block_index_of(xzm_segment_t segment,
		xzm_slice_count_t chunk_idx, uint64_t block_size, size_t offset)
{
	xzm_debug_assert(offset >= chunk_idx * XZM_SEGMENT_SLICE_SIZE);
	return (xzm_block_index_t)
			(offset - chunk_idx * XZM_SEGMENT_SLICE_SIZE) / block_size;
}

// mimalloc: mi_page_block_size
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint64_t
_xzm_chunk_block_size(xzm_malloc_zone_t zone, xzm_chunk_t chunk)
{
	switch (chunk->xzc_bits.xzcb_kind) {
	case XZM_SLICE_KIND_TINY_CHUNK:
	case XZM_SLICE_KIND_SMALL_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
		// TODO: depending on the size class scheme, it may be better to
		// directly compute the block size from the xzone index using the
		// inverse of the bin function
		return zone->xzz_xzones[chunk->xzc_xzone_idx].xz_block_size;
	case XZM_SLICE_KIND_LARGE_CHUNK:
	case XZM_SLICE_KIND_HUGE_CHUNK:
		return ((uint64_t)chunk->xzcs_slice_count) << XZM_SEGMENT_SLICE_SHIFT;
	default:
		xzm_abort_with_reason("asking for size of chunk with invalid kind",
				(unsigned)chunk->xzc_bits.xzcb_kind);
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_count_t
_xzm_chunk_slice_count(xzm_chunk_t chunk)
{
	return (chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_TINY_CHUNK) ? 1 :
			chunk->xzcs_slice_count;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_t
_xzm_chunk_slices_of(xzm_chunk_t chunk, size_t num_slices)
{
	return (xzm_slice_t)chunk;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_chunk_is_empty(xzm_malloc_zone_t zone, xzm_xzone_t xz, xzm_chunk_t chunk)
{
	switch (chunk->xzc_bits.xzcb_kind) {
	case XZM_SLICE_KIND_SMALL_CHUNK:
		return chunk->xzc_used == 0;
	case XZM_SLICE_KIND_TINY_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
		return chunk->xzc_atomic_meta.xca_free_count == xz->xz_chunk_capacity ||
				chunk->xzc_atomic_meta.xca_alloc_head == XZM_FREE_MADVISING ||
				chunk->xzc_atomic_meta.xca_alloc_head == XZM_FREE_MADVISED;
	default:
		xzm_abort_with_reason("bad chunk kind",
				(unsigned int)chunk->xzc_bits.xzcb_kind);
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_chunk_is_full(xzm_malloc_zone_t zone, xzm_xzone_t xz, xzm_chunk_t chunk)
{
	switch (chunk->xzc_bits.xzcb_kind) {
	case XZM_SLICE_KIND_SMALL_CHUNK:
		return (chunk->xzc_used == xz->xz_chunk_capacity);
	case XZM_SLICE_KIND_TINY_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
		return chunk->xzc_atomic_meta.xca_free_count == 0 &&
				chunk->xzc_atomic_meta.xca_alloc_head != XZM_FREE_MADVISING &&
				chunk->xzc_atomic_meta.xca_alloc_head != XZM_FREE_MADVISED;
	default:
		xzm_abort_with_reason("bad chunk kind",
				(unsigned int)chunk->xzc_bits.xzcb_kind);
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint32_t
_xzm_xzone_free_mask(xzm_xzone_t xz, size_t chunk_capacity)
{
	xzm_debug_assert(!xz || xz->xz_chunk_capacity == chunk_capacity);
	return (uint32_t)((1ull << chunk_capacity) - 1);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
_xzm_chunk_reset_free(xzm_xzone_t xz, xzm_chunk_t chunk, bool reusable)
{
	if (!reusable) {
		chunk->xzc_xzone_idx = XZM_XZONE_INDEX_INVALID;
	}

	switch (chunk->xzc_bits.xzcb_kind) {
	case XZM_SLICE_KIND_TINY_CHUNK:
	case XZM_SLICE_KIND_SMALL_FREELIST_CHUNK:
		// Tiny chunks should only be freed when a zone is destroyed, at which
		// point we don't need to support concurrent access to those chunks. As
		// such, we can access the atomic state non-atomically in this path
		xzm_debug_assert(chunk->xzc_atomic_meta.xca_alloc_head ==
				XZM_FREE_MADVISED);
		xzm_debug_assert(chunk->xzc_atomic_meta.xca_free_count == 0);

		if (!reusable) {
			// Reset everything to 0 to allow reuse of this slice for any
			// purpose
			chunk->xzc_atomic_meta.xca_value = 0;
			chunk->xzc_freelist_block_size = 0;
			chunk->xzc_freelist_chunk_capacity = 0;
#if CONFIG_MTE
			chunk->xzc_tagged = false;
#endif
		}
		break;
	case XZM_SLICE_KIND_SMALL_CHUNK:
		chunk->xzc_used = 0;
		chunk->xzc_alloc_idx = XZM_SLOT_INDEX_EMPTY;
		if (reusable) {
			chunk->xzc_free |= _xzm_xzone_free_mask(xz, xz->xz_chunk_capacity);
		} else {
			chunk->xzc_free = 0;
		}
		break;
	default:
		xzm_abort_with_reason("bad chunk kind",
				(unsigned int)chunk->xzc_bits.xzcb_kind);
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_count_t
_xzm_free_span_slice_count(xzm_free_span_t span)
{
	return (span->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SINGLE_FREE) ? 1 :
			span->xzcs_slice_count;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_slice_count_t
_xzm_free_span_size(xzm_free_span_t span)
{
	return _xzm_free_span_slice_count(span) << XZM_SEGMENT_SLICE_SHIFT;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_block_offset_t
_xzm_chunk_offset_of_ptr(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		uintptr_t ptr)
{
	uintptr_t start = _xzm_chunk_start(zone, chunk, NULL);
#if CONFIG_MTE
	// Remove tag bits for pointer arithmetic
	ptr = (uintptr_t)memtag_strip_address((uint8_t *)ptr);
#endif
	xzm_block_offset_t offset = (xzm_block_offset_t)(ptr - start);
	return offset;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_block_offset_t
_xzm_chunk_block_offset(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		xzm_block_t block)
{
	return _xzm_chunk_offset_of_ptr(zone, chunk, (uintptr_t)block);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_chunk_block_start_of(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		uintptr_t ptr)
{
	xzm_debug_assert(chunk);
	xzm_debug_assert(ptr);

	xzm_block_offset_t offset = _xzm_chunk_offset_of_ptr(zone, chunk, ptr);
	size_t adjust = offset % _xzm_chunk_block_size(zone, chunk);
	return (uint8_t *)(ptr - adjust);
}

// mimalloc: _mi_page_ptr_unalign
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_block_t
_xzm_chunk_block_of(xzm_malloc_zone_t zone, xzm_chunk_t chunk, uintptr_t ptr)
{
	return (xzm_block_t)_xzm_chunk_block_start_of(zone, chunk, ptr);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_block_index_t
_xzm_chunk_block_index(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		xzm_block_t block)
{
	return (xzm_block_index_t)(_xzm_chunk_block_offset(zone, chunk, block) /
			_xzm_chunk_block_size(zone, chunk));
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_block_index_t
_xzm_chunk_block_index_of_ptr(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		uintptr_t ptr)
{
	return (xzm_block_index_t)(_xzm_chunk_offset_of_ptr(zone, chunk, ptr) /
			_xzm_chunk_block_size(zone, chunk));
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_xzm_chunk_block_index_start(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		xzm_block_index_t idx)
{
	xzm_debug_assert(idx <
			zone->xzz_xzones[chunk->xzc_xzone_idx].xz_chunk_capacity);
	return (uint8_t *)(_xzm_chunk_start(zone, chunk, NULL) +
			(idx * _xzm_chunk_block_size(zone, chunk)));
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_small_chunk_block_index_is_free(xzm_chunk_t chunk,
		xzm_block_index_t block_index)
{
	// Only applicable to chunks which use a bitmap freelist implementation
	xzm_debug_assert(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_CHUNK);
	return (bool)(chunk->xzc_free & (1u << block_index));
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_chunk_block_index_range_is_free(xzm_malloc_zone_t zone, xzm_chunk_t chunk,
		xzm_block_index_t start, xzm_block_index_t end)
{
	// Only applicable to chunks which use a bitmap freelist implementation
	xzm_debug_assert(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_CHUNK);
	xzm_debug_assert(end >= start);

	// Check the inclusive span from start to end
	uint32_t span = (end - start) + 1;
	xzm_debug_assert(span <= 32);

	uint32_t mask = (uint32_t)(((1ull << span) - 1) << start);
	return (chunk->xzc_free & mask) == mask;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
_xzm_chunk_block_free_slices_on_allocate(const xzm_chunk_t chunk,
		xzm_slice_count_t chunk_idx, uint32_t chunk_capacity,
		xzm_block_index_t block_idx, uint64_t block_size,
		xzm_slice_count_t *slice_idx, xzm_slice_count_t *num_slices)
{
	xzm_debug_assert(chunk &&
			chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_CHUNK);

	// Offset of this block relative to the body of the segment
	const size_t block = _xzm_segment_offset(NULL, chunk_idx, block_idx,
			block_size);
	const size_t block_end = block + block_size - 1;

	// Find the beginning of the first slice touched by the block
	const xzm_slice_count_t first_slice =
			_xzm_segment_slice_offset_index(NULL, block);
	// Find the beginning of the slice after the last touched by the block
	const xzm_slice_count_t limit_slice =
			_xzm_segment_slice_offset_index(NULL, block_end) + 1;

	// Find the blocks corresponding to these slices
	const xzm_block_index_t first_block =
			_xzm_segment_offset_chunk_block_index_of(NULL, chunk_idx, block_size,
			first_slice * XZM_SEGMENT_SLICE_SIZE);
	const xzm_block_index_t end_block =
			_xzm_segment_offset_chunk_block_index_of(NULL, chunk_idx, block_size,
			limit_slice * XZM_SEGMENT_SLICE_SIZE - 1);

	// Compute the offset of the first slice to populate, by checking if
	// any of the affected blocks are already in use (not all free).
	// If so, then we start with the end of the slice corresponding to the
	// beginning of our current block, instead of the beginning of that
	// slice
	const xzm_slice_count_t left = !_xzm_chunk_block_index_range_is_free(NULL,
			chunk, first_block, block_idx) ?
			_xzm_segment_slice_offset_index(NULL, block) + 1 : first_slice;

	// Compute the offset of the last slice to populate, by checking if
	// any of the affected blocks are already in use, as above.
	// If so, then we end with the beginning of the slice corresponding to
	// the end of our current block, instead of the end of that slice.
	// Exclude the end block if it is partial, because it is never free
	const xzm_block_index_t last_block =
			(end_block != chunk_capacity ? end_block : end_block - 1);
	xzm_debug_assert(block_idx <= last_block && last_block < chunk_capacity);
	const xzm_slice_count_t right = !_xzm_chunk_block_index_range_is_free(NULL,
			chunk, block_idx, last_block) ?
			_xzm_segment_slice_offset_index(NULL, block_end) : limit_slice;

	*slice_idx = left;
	*num_slices = (left <= right ? right - left : 0);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
_xzm_chunk_block_free_slices_on_deallocate(const xzm_chunk_t chunk,
		xzm_slice_count_t chunk_idx, uint32_t chunk_capacity,
		xzm_block_index_t block_idx, uint64_t block_size,
		xzm_slice_count_t *slice_idx, xzm_slice_count_t *num_slices)
{
	xzm_debug_assert(chunk &&
			chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_CHUNK);

	// Find the contiguous span from left (inclusive) to right (exclusive)
	xzm_slice_count_t left, right;

	// Offset of this block relative to the body of the segment
	const size_t block = _xzm_segment_offset(NULL, chunk_idx, block_idx,
			block_size);
	const size_t block_end = block + block_size - 1;

	// Determine the start of the first slice touched by this block
	const xzm_slice_count_t first_slice =
			_xzm_segment_slice_offset_index(NULL, block);
	// Determine end of last slice touched by this block
	const xzm_slice_count_t limit_slice =
			_xzm_segment_slice_offset_index(NULL, block_end) + 1;

	// Round the slice boundaries down to their corresponding blocks
	const xzm_block_index_t first_block =
			_xzm_segment_offset_chunk_block_index_of(NULL, chunk_idx, block_size,
			first_slice * XZM_SEGMENT_SLICE_SIZE);
	xzm_block_index_t last_block =
			_xzm_segment_offset_chunk_block_index_of(NULL, chunk_idx, block_size,
			limit_slice * XZM_SEGMENT_SLICE_SIZE - 1);

	// If the slices of this chunk are not perfectly divisible by the
	// block-size, the "partial" chunk at the end of the slice will never
	// be free because we cannot allocate from it
	if (last_block == chunk_capacity) {
		last_block -= 1;
	}

	// Determine if LHS blocks are free, and include/exclude them from the
	// madvisable range accordingly
	if (_xzm_chunk_block_index_range_is_free(NULL, chunk, first_block, block_idx)) {
		left = first_slice;
	} else {
		left = _xzm_segment_slice_offset_index(NULL, block) + 1;
	}
	xzm_debug_assert(left >= chunk_idx);

	// Determine if RHS blocks are free, and include/exclude them from the
	// madvisable range accordingly
	if (_xzm_chunk_block_index_range_is_free(NULL, chunk, block_idx, last_block)) {
		right = limit_slice;
	} else {
		right = _xzm_segment_slice_offset_index(NULL, block_end);
	}
	xzm_debug_assert(right <= chunk_idx + chunk->xzcs_slice_count);

	*slice_idx = left;
	*num_slices = (left <= right ? right - left : 0);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_chunk_t *
_xzm_segment_slice_meta_batch_next(xzm_malloc_zone_t zone, xzm_slice_t slice)
{
	xzm_segment_t segment = _xzm_segment_for_slice(zone, slice);
	xzm_xzone_slice_metadata_u *metadata =
			&segment->xzs_slice_metadata[_xzm_slice_index(segment, slice)];
	return &metadata->xzsm_batch_next;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_slice_meta_is_batch_pointer(xzm_malloc_zone_t zone, xzm_slice_t slice)
{
#if CONFIG_XZM_DEFERRED_RECLAIM
	if ((mach_vm_reclaim_id_t)slice == VM_RECLAIM_ID_NULL) {
		return false;
	}
#endif // CONFIG_XZM_DEFERRED_RECLAIM
	const uintptr_t slice_addr = (uintptr_t)slice;
	xzm_segment_t segment = _xzm_segment_for_slice(zone, slice);
	return !slice || (slice_addr >= (uintptr_t)(segment->xzs_slices) &&
			slice_addr < (uintptr_t)(segment->xzs_slices + segment->xzs_slice_entry_count));
}

#if CONFIG_XZM_DEFERRED_RECLAIM

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static mach_vm_reclaim_id_t *
_xzm_segment_slice_meta_reclaim_id(xzm_segment_t segment,
		xzm_slice_t slice)
{
	xzm_xzone_slice_metadata_u *metadata =
			&segment->xzs_slice_metadata[_xzm_slice_index(segment, slice)];
	return &metadata->xzsm_reclaim_id;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static mach_vm_reclaim_id_t *
_xzm_slice_meta_reclaim_id(xzm_malloc_zone_t zone, xzm_slice_t slice)
{
	xzm_segment_t segment = _xzm_segment_for_slice(zone, slice);
	return _xzm_segment_slice_meta_reclaim_id(segment, slice);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_segment_slice_is_deferred(xzm_segment_t segment, xzm_slice_t slice)
{
	mach_vm_reclaim_id_t *reclaim_index = _xzm_segment_slice_meta_reclaim_id(
			segment, slice);
	return (*reclaim_index != VM_RECLAIM_ID_NULL);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_slice_is_deferred(xzm_malloc_zone_t zone, xzm_slice_t slice)
{
	xzm_segment_t segment = _xzm_segment_for_slice(zone, slice);
	return _xzm_segment_slice_is_deferred(segment, slice);
}

#endif // CONFIG_XZM_DEFERRED_RECLAIM

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_slice_kind_is_free_span(xzm_slice_kind_t kind)
{
	return (kind == XZM_SLICE_KIND_SINGLE_FREE ||
			kind == XZM_SLICE_KIND_MULTI_FREE);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint32_t
_xzm_xzone_slice_free_mask(xzm_malloc_zone_t zone, xzm_slice_t slice)
{
	xzm_chunk_t chunk = _xzm_span_slice_first(slice);
	xzm_debug_assert(chunk->xzc_bits.xzcb_kind == XZM_SLICE_KIND_SMALL_CHUNK);
	xzm_xzone_t xz = &zone->xzz_xzones[chunk->xzc_xzone_idx];

	uintptr_t slice_start = (uintptr_t)_xzm_slice_start(zone, slice);
	uintptr_t slice_end = slice_start + XZM_SEGMENT_SLICE_SIZE - 1;

	// If the slices of this chunk are not perfectly divisible by the
	// block-size, there will be a "partial" block at the end of the chunk
	// needing special consideration
	xzm_block_index_t first = _xzm_chunk_block_index_of_ptr(zone, chunk,
			slice_start);
	if (first == xz->xz_chunk_capacity) {
		// This slice corresponds to the partial block, nothing can be allocated from it
		return 0u;
	}
	xzm_block_index_t last = _xzm_chunk_block_index_of_ptr(zone, chunk,
			slice_end);
	if (last == xz->xz_chunk_capacity) {
		// The partial chunk resides at the end of this slice, exclude it from
		// the mask
		last--;
	}

	xzm_block_index_t span = (last - first) + 1;
	xzm_debug_assert(first <= last);

	return (uint32_t)(((1ull << span) - 1) << first);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uintptr_t
_xzm_introspect_rebase(uintptr_t orig_base, void *new_base, size_t size,
		void *ptr, size_t ptr_size)
{
	if ((uintptr_t)ptr < orig_base) {
		return 0;
	}

	uintptr_t offset = (uintptr_t)ptr - orig_base;
	uintptr_t offset_end;
	if (os_add_overflow(offset, ptr_size, &offset_end)) {
		return 0;
	}
	if (offset_end > size) {
		return 0;
	}

	return (uintptr_t)new_base + offset;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_xzone_allocation_slot_t
_xzm_xzone_allocation_slot_for_index(xzm_malloc_zone_t zone, xzm_xzone_t xz,
		xzm_allocation_index_t alloc_idx)
{
	xzm_xzone_index_t xz_idx = xz->xz_idx;
	size_t alloc_base_idx = alloc_idx * zone->xzz_xzone_count;
	xzm_debug_assert(alloc_base_idx + xz_idx <
			zone->xzz_slot_count * zone->xzz_xzone_count);
	return &zone->xzz_xzone_allocation_slots[alloc_base_idx + xz_idx];
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static const char *
_xzm_slot_config_to_string(xzm_slot_config_t slot_config)
{
	switch(slot_config) {
	case XZM_SLOT_SINGLE:
		return "SINGLE";
	case XZM_SLOT_CLUSTER:
		return "CLUSTER";
	case XZM_SLOT_CPU:
		return "CPU";
	case XZM_SLOT_LAST:
	default:
		xzm_debug_abort("unexpected slot config");
		return "UNKNOWN";
	}
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_segment_group_has_madvise_workaround(xzm_segment_group_t sg)
{
	return sg->xzsg_main_ref->xzmz_madvise_workaround;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_segment_group_uses_deferred_reclamation(xzm_segment_group_t sg)
{
#if CONFIG_XZM_DEFERRED_RECLAIM
	switch(sg->xzsg_id) {
	case XZM_SEGMENT_GROUP_DATA:
	case XZM_SEGMENT_GROUP_POINTER_XZONES:
		// XXX: There is an implicit assumption that tiny chunks are
		// always sequestered. If tiny chunks every support recirculation,
		// they'll be subject to deferred reclaim alongside their small
		// counterparts once freed back to the segment group
		return sg->xzsg_main_ref->xzmz_defer_small;
	case XZM_SEGMENT_GROUP_POINTER_LARGE:
	case XZM_SEGMENT_GROUP_DATA_LARGE:
		return sg->xzsg_main_ref->xzmz_defer_large;
	default:
		xzm_abort_with_reason("unknown segment group id", sg->xzsg_id);
	}
#else // CONFIG_XZM_DEFERRED_RECLAIM
	return false;
#endif // CONFIG_XZM_DEFERRED_RECLAIM
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static size_t
_xzm_segment_group_min_block_size(xzm_segment_group_t sg)
{
	// Note: large alignments can force small allocations into segment groups for
	// bigger allocations, so this query is not always precise (which is
	// acceptable for our purposes).
	const size_t small_block_size_min = 16;
	const size_t large_block_size_min = XZM_SMALL_BLOCK_SIZE_MAX + 1;

	switch (sg->xzsg_id) {
	case XZM_SEGMENT_GROUP_POINTER_XZONES:
	case XZM_SEGMENT_GROUP_DATA:
		return small_block_size_min;
	case XZM_SEGMENT_GROUP_POINTER_LARGE:
	case XZM_SEGMENT_GROUP_DATA_LARGE:
		return large_block_size_min;
	default:
		xzm_abort_with_reason("unknown segment group id", sg->xzsg_id);
	}
}

#if CONFIG_MTE

// Return whether this zone might contain tagged allocations.  This is used as
// the fast path check that avoids touching the xzone (xzm_xzone_t).  Note that
// unlike `xz->xz_tagged` this function does not take the <size,data>
// characteristics of the allocation into account, so it should only be used
// when this precision is not required.
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_zone_memtag_enabled(xzm_malloc_zone_t zone)
{
	return zone->xzz_memtag_config.enabled;
}

// Return whether we tag allocations with these <size,data> characteristics
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_zone_memtag_block(xzm_malloc_zone_t zone, size_t block_size, bool data)
{
	struct xzm_memtag_config_s *cfg = &(zone->xzz_memtag_config);
	return cfg->enabled &&
			(block_size <= cfg->max_block_size) &&
			(!data || cfg->tag_data);
}

// Return whether we tag allocations of this size in this segment group
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_segment_group_memtag_block(xzm_segment_group_t sg, size_t block_size)
{
	xzm_malloc_zone_t zone = &(sg->xzsg_main_ref->xzmz_base);
	bool data = _xzm_segment_group_id_is_data(sg->xzsg_id);
	// Note: the block_size may be less than the min_block_size for the segment
	// group in the case where alignment is forcing the allocation of a
	// smaller-than-normal block from the segment group
	size_t min_block_size = _xzm_segment_group_min_block_size(sg);
	return block_size >= min_block_size &&
			_xzm_zone_memtag_block(zone, block_size, data);
}

// Return whether this segment group might contain tagged allocations
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
_xzm_segment_group_memtag_enabled(xzm_segment_group_t sg)
{
	xzm_malloc_zone_t zone = &(sg->xzsg_main_ref->xzmz_base);
	bool data = _xzm_segment_group_id_is_data(sg->xzsg_id);
	size_t min_block_size = _xzm_segment_group_min_block_size(sg);
	return _xzm_zone_memtag_block(zone, min_block_size, data);
}

#endif // CONFIG_MTE

#if CONFIG_XZM_THREAD_CACHE

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static xzm_thread_cache_t
_xzm_get_thread_cache(void)
{
	return _pthread_getspecific_direct(__TSD_MALLOC_XZONE_THREAD_CACHE);
}

#endif // CONFIG_XZM_THREAD_CACHE

#endif // __has_feature(bounds_safety)

#endif // __XZONE_INLINE_INTERNAL_H__

#endif // CONFIG_XZONE_MALLOC
