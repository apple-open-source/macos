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

#ifndef _MALLOC_TYPE_PRIVATE_H_
#define _MALLOC_TYPE_PRIVATE_H_

#if defined(__LP64__) // MALLOC_TARGET_64BIT

#include <malloc/_malloc_type.h>
#include <stdbool.h>
#include <stdint.h>  // uint8_t, uint16_t, ...

__BEGIN_DECLS

typedef enum {
	MALLOC_TYPE_CALLSITE_FLAGS_NONE = 0,
	MALLOC_TYPE_CALLSITE_FLAGS_FIXED_SIZE = 1 << 0,
	MALLOC_TYPE_CALLSITE_FLAGS_ARRAY = 1 << 1,
	MALLOC_TYPE_CALLSITE_FLAGS_HEADER_PREFIXED_ARRAY = 1 << 2
} malloc_type_callsite_flags_t;

typedef enum {
	MALLOC_TYPE_KIND_C = 0,
	MALLOC_TYPE_KIND_OBJC = 1,
	MALLOC_TYPE_KIND_SWIFT = 2,
	MALLOC_TYPE_KIND_CXX = 3
} malloc_type_kind_t;

typedef enum {
	MALLOC_TYPE_FLAGS_NONE = 0,
	MALLOC_TYPE_FLAGS_IS_POLYMORPHIC = 1 << 0,
	MALLOC_TYPE_FLAGS_HAS_MIXED_UNIONS = 1 << 1
} malloc_type_flags_t;

typedef struct {
	bool data_pointer : 1;
	bool struct_pointer : 1;
	bool immutable_pointer : 1;
	bool anonymous_pointer : 1;
	bool reference_count : 1;
	bool resource_handle : 1;
	bool spatial_bounds : 1;
	bool tainted_data : 1;
	bool generic_data : 1;
	uint16_t unused : 7;
} malloc_type_layout_semantics_t;

typedef struct {
	uint32_t version : 2;
	uint32_t unused : 4;
	malloc_type_callsite_flags_t callsite_flags : 4;
	malloc_type_kind_t type_kind : 2;
	malloc_type_flags_t type_flags : 4;
	malloc_type_layout_semantics_t layout_semantics;
} malloc_type_summary_t;

typedef union {
	struct {
		uint32_t hash;
		malloc_type_summary_t summary;
	};
	malloc_type_id_t type_id;
	void *storage;
} malloc_type_descriptor_t;


// Standard and zone entry points are declared in <malloc/_malloc_type.h> and
// conditionally included into <malloc/_malloc.h> and <malloc/malloc.h>.

__END_DECLS

#endif // MALLOC_TARGET_64BIT
#endif // _MALLOC_TYPE_PRIVATE_H_
