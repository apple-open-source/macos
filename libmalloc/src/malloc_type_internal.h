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

#ifndef __MALLOC_TYPE_INTERNAL_H__
#define __MALLOC_TYPE_INTERNAL_H__

#if MALLOC_TARGET_64BIT

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static malloc_type_descriptor_t
malloc_get_tsd_type_descriptor(void)
{
	return (malloc_type_descriptor_t){
		.storage = _pthread_getspecific_direct(__TSD_MALLOC_TYPE_DESCRIPTOR),
	};
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint64_t
malloc_get_tsd_type_id(void)
{
	return malloc_get_tsd_type_descriptor().type_id;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
malloc_set_tsd_type_descriptor(malloc_type_descriptor_t type_desc)
{
	_pthread_setspecific_direct(__TSD_MALLOC_TYPE_DESCRIPTOR,
			type_desc.storage);
}

// TODO: can we get a guarantee from the compiler that no valid type ID will
// ever have this value?
#define MALLOC_TYPE_ID_NONE 0ull
#define MALLOC_TYPE_ID_NONZERO 1ull
#define MALLOC_TYPE_DESCRIPTOR_NONE (malloc_type_descriptor_t){ 0 }

union malloc_type_layout_bits_u {
	uint16_t bits;
	malloc_type_layout_semantics_t layout;
};

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
malloc_type_descriptor_is_pure_data(malloc_type_descriptor_t type_desc)
{
	static const union malloc_type_layout_bits_u data_layout = {
		.layout = { .generic_data = true, },
	};
	union malloc_type_layout_bits_u type_desc_layout = {
		.layout = type_desc.summary.layout_semantics,
	};
	return type_desc_layout.bits == data_layout.bits;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
malloc_type_descriptor_is_uninferred(malloc_type_descriptor_t type_desc)
{
	return type_desc.summary.type_kind == MALLOC_TYPE_KIND_C &&
			type_desc.summary.callsite_flags == MALLOC_TYPE_CALLSITE_FLAGS_NONE;
}

#else // MALLOC_TARGET_64BIT

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint64_t
malloc_get_tsd_type_id(void)
{
	return 0;
}

#endif // MALLOC_TARGET_64BIT

MALLOC_NOEXPORT
extern bool malloc_interposition_compat;

MALLOC_NOEXPORT
void
_malloc_detect_interposition(void);

#endif // __MALLOC_TYPE_INTERNAL_H__
