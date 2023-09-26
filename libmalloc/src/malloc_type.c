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

void
_malloc_detect_interposition(void)
{
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
}

#else // MALLOC_TARGET_64BIT

void
_malloc_detect_interposition(void)
{
	// No compatibility support required
}

#endif // MALLOC_TARGET_64BIT
