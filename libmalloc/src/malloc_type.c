/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "internal.h"

#if MALLOC_TARGET_64BIT

// Type Descriptor Layout
//  0-31 | hash
// 32-63 | summary
//   32-33 | version
//   34-37 | (unused)
//   38-41 | callsite_flags
//     38-40 | (enum values)
//        41 | (unused enum bits)
//   42-43 | type_kind
//     42-43 | (enum values)
//   44-47 | type_flags
//     44-45 | (enum values)
//     46-47 | (unused enum bits)
//   48-63 | layout_semantics
//        48 | data_pointer
//        49 | struct_pointer
//        50 | immutable_pointer
//        51 | anonymous_pointer
//        52 | reference_count
//        53 | resource_handle
//        54 | spatial_bounds
//        55 | tainted_data
//        56 | generic_data
//     57-63 | (unused)

// Enums sizes (required numbers of bits to encode enum)
MALLOC_STATIC_ASSERT(MALLOC_TYPE_CALLSITE_FLAGS_HEADER_PREFIXED_ARRAY < (1 << 3),
		"malloc_type_callsite_flags_t size");
MALLOC_STATIC_ASSERT(MALLOC_TYPE_KIND_CXX < (1 << 2),
		"malloc_type_kind_t size");
MALLOC_STATIC_ASSERT(MALLOC_TYPE_FLAGS_HAS_MIXED_UNIONS < (1 << 2),
		"malloc_type_flags_t size");

// Struct sizes
MALLOC_STATIC_ASSERT(sizeof(malloc_type_layout_semantics_t) == 2,
		"malloc_type_layout_semantics_t size");
MALLOC_STATIC_ASSERT(sizeof(malloc_type_summary_t) == 4,
		"malloc_type_summary_t size");
MALLOC_STATIC_ASSERT(sizeof(malloc_type_descriptor_t) == 8,
		"malloc_type_descriptor_t size");

// Type descriptor union
MALLOC_STATIC_ASSERT(sizeof(malloc_type_descriptor_t) == sizeof(malloc_type_id_t),
		"same size as malloc_type_id_t");
MALLOC_STATIC_ASSERT(sizeof(malloc_type_descriptor_t) == sizeof(void *),
		"same size as malloc_type_id_t");

extern void *_malloc_text_start __asm("segment$start$__TEXT");
extern void *_malloc_text_end __asm("segment$end$__TEXT");

extern malloc_zone_t * __single * __unsafe_indexable malloc_zones;
#if MALLOC_TARGET_EXCLAVES
# define MALLOC_DEFAULT_ZONE malloc_zones[0]
#else
extern malloc_zone_t * __single default_zone;
# define MALLOC_DEFAULT_ZONE default_zone
#endif /* MALLOC_TARGET_EXCLAVES */

#if !MALLOC_TARGET_EXCLAVES
static bool
_malloc_symbol_interposed(void *symbol)
{
#if __has_feature(ptrauth_calls)
	symbol = ptrauth_strip(symbol, ptrauth_key_function_pointer);
#endif
	uintptr_t symbol_addr = (uintptr_t)symbol;
	uintptr_t malloc_start_addr = (uintptr_t)&_malloc_text_start;
	uintptr_t malloc_end_addr = (uintptr_t)&_malloc_text_end;
	return symbol_addr < malloc_start_addr || symbol_addr >= malloc_end_addr;
}
#endif // !MALLOC_TARGET_EXCLAVES

void
_malloc_detect_interposition(void)
{
#if !MALLOC_TARGET_EXCLAVES
	bool non_typed_symbols_interposed = _malloc_symbol_interposed(malloc) ||
			_malloc_symbol_interposed(calloc) ||
			_malloc_symbol_interposed(free) ||
			_malloc_symbol_interposed(realloc) ||
			_malloc_symbol_interposed(valloc) ||
			_malloc_symbol_interposed(aligned_alloc) ||
			_malloc_symbol_interposed(posix_memalign) ||
			_malloc_symbol_interposed(malloc_zone_malloc) ||
			_malloc_symbol_interposed(malloc_zone_calloc) ||
			_malloc_symbol_interposed(malloc_zone_free) ||
			_malloc_symbol_interposed(malloc_zone_realloc) ||
			_malloc_symbol_interposed(malloc_zone_valloc) ||
			_malloc_symbol_interposed(malloc_zone_memalign);

	bool typed_symbols_interposed =
			_malloc_symbol_interposed(malloc_type_malloc) ||
			_malloc_symbol_interposed(malloc_type_calloc) ||
			_malloc_symbol_interposed(malloc_type_free) ||
			_malloc_symbol_interposed(malloc_type_realloc) ||
			_malloc_symbol_interposed(malloc_type_valloc) ||
			_malloc_symbol_interposed(malloc_type_aligned_alloc) ||
			_malloc_symbol_interposed(malloc_type_posix_memalign) ||
			_malloc_symbol_interposed(malloc_type_zone_malloc) ||
			_malloc_symbol_interposed(malloc_type_zone_calloc) ||
			_malloc_symbol_interposed(malloc_type_zone_free) ||
			_malloc_symbol_interposed(malloc_type_zone_realloc) ||
			_malloc_symbol_interposed(malloc_type_zone_valloc) ||
			_malloc_symbol_interposed(malloc_type_zone_memalign);

	bool interposition_compat = non_typed_symbols_interposed &&
			!typed_symbols_interposed;
	if (malloc_interposition_compat != interposition_compat) {
		malloc_interposition_compat = interposition_compat;
	}
#endif // !MALLOC_TARGET_EXCLAVES
}

/*********	malloc_type	************/

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static malloc_type_descriptor_t
_malloc_type_outlined_set_tsd(malloc_type_id_t type_id)
{
	malloc_type_descriptor_t prev_type_desc = malloc_get_tsd_type_descriptor();

	if (!type_id) {
		// We need this to be non-zero so that we can use it for the recursion
		// check below
		type_id = MALLOC_TYPE_ID_NONZERO;
	}
	malloc_set_tsd_type_descriptor(
			(malloc_type_descriptor_t){ .type_id = type_id });

	return prev_type_desc;
}

#if !MALLOC_TARGET_EXCLAVES
MALLOC_NOINLINE
static void * __sized_by_or_null(size)
_malloc_type_malloc_outlined(size_t size, malloc_type_id_t type_id,
		bool do_interposition_compat)
{
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	void *ptr;
	if (malloc_interposition_compat && !prev_type_desc.type_id &&
			do_interposition_compat) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		ptr = malloc(size);
	} else {
		ptr = _malloc_zone_malloc(MALLOC_DEFAULT_ZONE, size, MZ_POSIX);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return ptr;
}

MALLOC_NOINLINE
static void * __sized_by_or_null(count * size)
_malloc_type_calloc_outlined(size_t count, size_t size, malloc_type_id_t type_id)
{
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	void *ptr;
	if (malloc_interposition_compat && !prev_type_desc.type_id) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		ptr = calloc(count, size);
	} else {
		ptr = _malloc_zone_calloc(MALLOC_DEFAULT_ZONE, count, size, MZ_POSIX);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return ptr;
}
#endif // !MALLOC_TARGET_EXCLAVES

// TODO: malloc_type_free_outlined

#if !MALLOC_TARGET_EXCLAVES
MALLOC_NOINLINE
static void * __sized_by_or_null(size)
_malloc_type_realloc_outlined(void * __unsafe_indexable ptr, size_t size,
		malloc_type_id_t type_id)
{
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	void *new_ptr;
	if (malloc_interposition_compat && !prev_type_desc.type_id) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		new_ptr = realloc(ptr, size);
	} else {
		new_ptr = _realloc(ptr, size);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return new_ptr;
}
#endif // !MALLOC_TARGET_EXCLAVES

MALLOC_NOINLINE
static void * __sized_by_or_null(size)
_malloc_type_aligned_alloc_outlined(size_t alignment, size_t size,
		malloc_type_id_t type_id)
{
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	void *ptr;
	if (malloc_interposition_compat && !prev_type_desc.type_id) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		ptr = aligned_alloc(alignment, size);
	} else {
		// We've set the TSD, so no need to pass the type descriptor via the
		// parameter
		ptr = _malloc_zone_memalign(MALLOC_DEFAULT_ZONE, alignment, size,
			MZ_POSIX | MZ_C11, MALLOC_TYPE_DESCRIPTOR_NONE);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return ptr;
}

MALLOC_NOINLINE
static int
_malloc_type_posix_memalign_outlined(void * __unsafe_indexable *memptr,
		size_t alignment, size_t size, malloc_type_id_t type_id)
{
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	int retval;
	if (malloc_interposition_compat && !prev_type_desc.type_id) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		retval = posix_memalign(memptr, alignment, size);
	} else {
		retval = _posix_memalign(memptr, alignment, size);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return retval;
}

#if !MALLOC_TARGET_EXCLAVES
MALLOC_NOINLINE
static void * __sized_by_or_null(size)
_malloc_type_zone_malloc_outlined(malloc_zone_t *zone, size_t size,
		malloc_type_id_t type_id)
{
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	void *ptr;
	if (malloc_interposition_compat && !prev_type_desc.type_id) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		ptr = malloc_zone_malloc(zone, size);
	} else {
		ptr = _malloc_zone_malloc(zone, size, MZ_NONE);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return ptr;
}

MALLOC_NOINLINE
static void * __sized_by_or_null(count * size)
_malloc_type_zone_calloc_outlined(malloc_zone_t *zone, size_t count, size_t size,
		malloc_type_id_t type_id)
{
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	void *ptr;
	if (malloc_interposition_compat && !prev_type_desc.type_id) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		ptr = malloc_zone_calloc(zone, count, size);
	} else {
		ptr = _malloc_zone_calloc(zone, count, size, MZ_NONE);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return ptr;
}

// TODO: malloc_type_zone_free_outlined

MALLOC_NOINLINE
static void * __sized_by_or_null(size)
_malloc_type_zone_realloc_outlined(malloc_zone_t *zone, void *ptr, size_t size,
		malloc_type_id_t type_id)
{
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	void *new_ptr;
	if (malloc_interposition_compat && !prev_type_desc.type_id) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		new_ptr = malloc_zone_realloc(zone, ptr, size);
	} else {
		// We've set the TSD, so no need to pass the type descriptor via the
		// parameter
		new_ptr = _malloc_zone_realloc(zone, ptr, size,
				MALLOC_TYPE_DESCRIPTOR_NONE);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return new_ptr;
}
#endif // !MALLOC_TARGET_EXCLAVES

MALLOC_NOINLINE
static void * __sized_by_or_null(size)
_malloc_type_zone_memalign_outlined(malloc_zone_t *zone, size_t alignment,
		size_t size, malloc_type_id_t type_id)
{
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	void *ptr;
	if (malloc_interposition_compat && !prev_type_desc.type_id) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		ptr = malloc_zone_memalign(zone, alignment, size);
	} else {
		// We've set the TSD, so no need to pass the type descriptor via the
		// parameter
		ptr = _malloc_zone_memalign(zone, alignment, size, MZ_NONE,
				MALLOC_TYPE_DESCRIPTOR_NONE);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return ptr;
}

MALLOC_NOINLINE
static void * __sized_by_or_null(size)
_malloc_type_zone_malloc_with_options_outlined(malloc_zone_t *zone,
		size_t align, size_t size, malloc_type_id_t type_id,
		malloc_zone_malloc_options_t options)
{
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

#if !MALLOC_TARGET_EXCLAVES
	void *ptr = _malloc_zone_malloc_with_options_outlined(zone, align, size,
			options);
#else
	void *ptr = malloc_zone_malloc_with_options(zone, align, size, options);
#endif // !MALLOC_TARGET_EXCLAVES

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return ptr;
}

MALLOC_INLINE MALLOC_ALWAYS_INLINE
static void * __sized_by_or_null(size)
_malloc_type_malloc(malloc_zone_t *zone, size_t size, malloc_type_id_t type_id,
		bool do_interposition_compat)
{
	if (zone->version >= 16) {
		return zone->malloc_type_malloc(zone, size, type_id);
	}

#if !MALLOC_TARGET_EXCLAVES
	if (zone->version < 13) {
		return _malloc_type_malloc_outlined(size, type_id,
				do_interposition_compat);
	}
#endif // !MALLOC_TARGET_EXCLAVES

	return zone->malloc(zone, size);
}

void * __sized_by_or_null(size)
malloc_type_malloc(size_t size, malloc_type_id_t type_id)
{
#if !MALLOC_TARGET_EXCLAVES
	if (os_unlikely(malloc_logger || malloc_slowpath ||
			malloc_too_large(size))) {
		return _malloc_type_malloc_outlined(size, type_id, true);
	}
#endif // !MALLOC_TARGET_EXCLAVES

	return _malloc_type_malloc(malloc_zones[0], size, type_id, true);
}

void * __sized_by_or_null(count * size)
malloc_type_calloc(size_t count, size_t size, malloc_type_id_t type_id)
{
#if !MALLOC_TARGET_EXCLAVES
	if (os_unlikely(malloc_logger || malloc_slowpath)) {
		return _malloc_type_calloc_outlined(count, size, type_id);
	}
#endif // !MALLOC_TARGET_EXCLAVES

	malloc_zone_t *zone0 = malloc_zones[0];

	if (zone0->version >= 16) {
		return zone0->malloc_type_calloc(zone0, count, size, type_id);
	}

#if !MALLOC_TARGET_EXCLAVES
	if (zone0->version < 13) {
		return _malloc_type_calloc_outlined(count, size, type_id);
	}
#endif // !MALLOC_TARGET_EXCLAVES

	return zone0->calloc(zone0, count, size);
}

void
malloc_type_free(void * __unsafe_indexable ptr, malloc_type_id_t type_id)
{
	// TODO: this is not interposition-safe - need to revist when we want to
	// actually start using this
	return _free(ptr);
}

void * __sized_by_or_null(size)
malloc_type_realloc(void * __unsafe_indexable ptr, size_t size,
		malloc_type_id_t type_id)
{
#if !MALLOC_TARGET_EXCLAVES
	if (os_unlikely(malloc_logger || malloc_slowpath ||
			malloc_too_large(size))) {
		return _malloc_type_realloc_outlined(ptr, size, type_id);
	}
#endif // !MALLOC_TARGET_EXCLAVES

	void * __malloc_bidi_indexable retval = NULL;
	if (!ptr || size == 0) {
		retval = _malloc_type_malloc(malloc_zones[0], size, type_id, false);
	} else {
		malloc_zone_t *zone = find_registered_zone(ptr, NULL, false);
		if (!zone) {
			// Let _realloc deal with this
			return _realloc(ptr, size);
		}

#if !MALLOC_TARGET_EXCLAVES
		if (zone == default_zone) {
			zone = malloc_zones[0];
		}
#endif // !MALLOC_TARGET_EXCLAVES

		if (zone->version >= 16) {
			retval = zone->malloc_type_realloc(zone, ptr, size, type_id);
		} else {
			retval = zone->realloc(zone, ptr, size);
		}
	}

	if (!retval) {
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
	} else if (size == 0) {
		_free(ptr);
	}

	return retval;
}

void * __sized_by_or_null(size)
malloc_type_valloc(size_t size, malloc_type_id_t type_id)
{
	// valloc is not used often enough to warrant fastpath handling, so we'll
	// just always pass the type information via the TSD
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	void *ptr;
	if (malloc_interposition_compat && !prev_type_desc.type_id) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		ptr = valloc(size);
	} else {
		ptr = _malloc_zone_valloc(MALLOC_DEFAULT_ZONE, size, MZ_POSIX);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return ptr;
}

void * __sized_by_or_null(size)
malloc_type_aligned_alloc(size_t alignment, size_t size,
		malloc_type_id_t type_id)
{
#if !MALLOC_TARGET_EXCLAVES
	if (os_unlikely(malloc_logger || malloc_slowpath ||
			malloc_too_large(size))) {
		return _malloc_type_aligned_alloc_outlined(alignment, size, type_id);
	}
#endif // !MALLOC_TARGET_EXCLAVES

	malloc_zone_t *zone0 = malloc_zones[0];
	if (zone0->version >= 16 && alignment >= sizeof(void *) &&
			powerof2(alignment) && !(size & (alignment - 1))) {
		void * __malloc_bidi_indexable ptr = zone0->malloc_type_memalign(zone0,
				alignment, size, type_id);
		if (os_unlikely(!ptr)) {
			malloc_set_errno_fast(MZ_POSIX, ENOMEM);
		}
		return ptr;
	}

	// Everything else takes the slower path
	return _malloc_type_aligned_alloc_outlined(alignment, size, type_id);
}

int
malloc_type_posix_memalign(void * __unsafe_indexable *memptr, size_t alignment,
		size_t size, malloc_type_id_t type_id)
{
#if !MALLOC_TARGET_EXCLAVES
	if (os_unlikely(malloc_logger || malloc_slowpath ||
			malloc_too_large(size))) {
		return _malloc_type_posix_memalign_outlined(memptr, alignment, size,
				type_id);
	}
#endif // !MALLOC_TARGET_EXCLAVES

	malloc_zone_t *zone0 = malloc_zones[0];
	if (zone0->version >= 16 && alignment >= sizeof(void *) &&
			powerof2(alignment)) {
		void * __malloc_bidi_indexable ptr = zone0->malloc_type_memalign(zone0,
				alignment, size, type_id);
		if (os_unlikely(!ptr)) {
			return ENOMEM;
		}
		*memptr = ptr;
		return 0;
	}

	return _malloc_type_posix_memalign_outlined(memptr, alignment, size,
			type_id);
}

void * __sized_by_or_null(size)
malloc_type_zone_malloc(malloc_zone_t *zone, size_t size,
		malloc_type_id_t type_id)
{
#if !MALLOC_TARGET_EXCLAVES
	if (os_unlikely(malloc_logger || malloc_slowpath ||
			malloc_too_large(size))) {
		return _malloc_type_zone_malloc_outlined(zone, size, type_id);
	}

	if (zone == default_zone) {
		zone = malloc_zones[0];
	}
#endif // !MALLOC_TARGET_EXCLAVES

	if (zone->version >= 16) {
		return zone->malloc_type_malloc(zone, size, type_id);
	}

#if !MALLOC_TARGET_EXCLAVES
	if (zone->version < 13) {
		return _malloc_type_zone_malloc_outlined(zone, size, type_id);
	}
#endif // !MALLOC_TARGET_EXCLAVES

	return zone->malloc(zone, size);
}

void * __sized_by_or_null(count * size)
malloc_type_zone_calloc(malloc_zone_t *zone, size_t count, size_t size,
		malloc_type_id_t type_id)
{
#if !MALLOC_TARGET_EXCLAVES
	if (os_unlikely(malloc_logger || malloc_slowpath)) {
		return _malloc_type_zone_calloc_outlined(zone, count, size, type_id);
	}

	if (zone == default_zone) {
		zone = malloc_zones[0];
	}
#endif // !MALLOC_TARGET_EXCLAVES

	if (zone->version >= 16) {
		return zone->malloc_type_calloc(zone, count, size, type_id);
	}

#if !MALLOC_TARGET_EXCLAVES
	if (zone->version < 13) {
		return _malloc_type_zone_calloc_outlined(zone, count, size, type_id);
	}
#endif // !MALLOC_TARGET_EXCLAVES

	return zone->calloc(zone, count, size);
}

void
malloc_type_zone_free(malloc_zone_t *zone, void * __unsafe_indexable ptr,
		malloc_type_id_t type_id)
{
	// TODO: this is not interposition-safe - need to revist when we want to
	// actually start using this
	return malloc_zone_free(zone, ptr);
}

void * __sized_by_or_null(size)
malloc_type_zone_realloc(malloc_zone_t *zone, void * __unsafe_indexable ptr,
		size_t size, malloc_type_id_t type_id)
{
#if !MALLOC_TARGET_EXCLAVES
	if (os_unlikely(malloc_logger || malloc_slowpath ||
			malloc_too_large(size))) {
		return _malloc_type_zone_realloc_outlined(zone, ptr, size, type_id);
	}

	if (zone == default_zone) {
		zone = malloc_zones[0];
	}
#endif // !MALLOC_TARGET_EXCLAVES

	if (zone->version >= 16) {
		return zone->malloc_type_realloc(zone, ptr, size, type_id);
	}

	return _malloc_zone_realloc(zone, ptr, size,
			(malloc_type_descriptor_t) { .type_id = type_id, });
}

void * __sized_by_or_null(size)
malloc_type_zone_valloc(malloc_zone_t *zone, size_t size,
		malloc_type_id_t type_id)
{
	// valloc is not used often enough to warrant fastpath handling, so we'll
	// just always pass the type information via the TSD
	malloc_type_descriptor_t prev_type_desc = _malloc_type_outlined_set_tsd(
			type_id);

	void *ptr;
	if (malloc_interposition_compat && !prev_type_desc.type_id) {
		// We're (potentially) interposed at the symbol level and aren't in a
		// recursive call, so call through the external symbol
		ptr = malloc_zone_valloc(zone, size);
	} else {
		ptr = _malloc_zone_valloc(zone, size, MZ_NONE);
	}

	malloc_set_tsd_type_descriptor(prev_type_desc);
	return ptr;
}

void * __sized_by_or_null(size)
malloc_type_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size,
		malloc_type_id_t type_id)
{
#if !MALLOC_TARGET_EXCLAVES
	if (os_unlikely(malloc_logger || malloc_slowpath ||
			malloc_too_large(size))) {
		return _malloc_type_zone_memalign_outlined(zone, alignment, size,
				type_id);
	}

	if (zone == default_zone) {
		zone = malloc_zones[0];
	}
#endif // !MALLOC_TARGET_EXCLAVES

	if (zone->version >= 16 && alignment >= sizeof(void *) &&
			powerof2(alignment)) {
		return zone->malloc_type_memalign(zone, alignment, size, type_id);
	}

	// Everything else takes the slower path
	return _malloc_type_zone_memalign_outlined(zone, alignment, size, type_id);
}

void * __sized_by_or_null(size)
malloc_type_zone_malloc_with_options(malloc_zone_t *zone, size_t align,
		size_t size, malloc_type_id_t type_id,
		malloc_zone_malloc_options_t options)
{
	if (os_unlikely((align != 0) && (!powerof2(align) ||
			((size & (align - 1)) != 0)))) { // equivalent to (size % align != 0)
		return NULL;
	}

#if !MALLOC_TARGET_EXCLAVES
	if (os_unlikely(malloc_logger || malloc_slowpath)) {
		return _malloc_type_zone_malloc_with_options_outlined(zone, align,
				size, type_id, options);
	}

	if (zone == default_zone) {
		zone = malloc_zones[0];
	}
#endif // !MALLOC_TARGET_EXCLAVES

	if (zone == NULL) {
		zone = malloc_zones[0];
	}

	if (zone->version >= 16 && zone->malloc_type_malloc_with_options) {
		return zone->malloc_type_malloc_with_options(zone, align, size, options,
				type_id);
	}

	return _malloc_type_zone_malloc_with_options_outlined(zone, align, size,
			type_id, options);
}

void * __sized_by_or_null(size)
malloc_type_zone_malloc_with_options_internal(malloc_zone_t *zone, size_t align,
		size_t size, malloc_type_id_t type_id, malloc_options_np_t options)
{
	return malloc_type_zone_malloc_with_options(zone, align, size, type_id,
			options);
}

#else // MALLOC_TARGET_64BIT

void
_malloc_detect_interposition(void)
{
	// No compatibility support required
}

#endif // MALLOC_TARGET_64BIT
