/*
 * Copyright (c) 2023 Apple Computer, Inc. All rights reserved.
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

#ifndef __EARLY_MALLOC_H
#define __EARLY_MALLOC_H
#pragma GCC visibility push(hidden)

#if CONFIG_EARLY_MALLOC

#include <stdbool.h>
#include <malloc/malloc.h>

/*!
 * @file early_allocator.h
 *
 * @discussion
 * The early allocator, cheekingly called "my first malloc" is a simple
 * allocator which is meant to support two use-cases such as:
 *
 * - malloc internal dynamic allocations,
 * - xzone first allocations in buckets until it is proven
 *   that buckets are used enough.
 *
 * Its purpose is to be extremely simple and compact, and is not meant
 * to be feature rich. It will never implement things like:
 * - returning memory to the system,
 * - any form of scalability,
 * - realloc,
 * - allocations with alignment requirements,
 * - allocations larger than MFM_ALLOC_SIZE_MAX,
 * - ...
 *
 * It also is of limited capacity, and if mfm_alloc() fails,
 * then the caller should retry with another backend.
 */


/*!
 * @macro MFM_ALLOC_SIZE_MAX
 *
 * @brief
 * The maximum allocation size that this allocator supports.
 */
#define MFM_ALLOC_SIZE_MAX      (16ul << 10)


extern void mfm_lock(void);

extern void mfm_unlock(void);

extern void mfm_reinit_lock(void);

/*!
 * @function mfm_initialize()
 *
 * @brief
 * Initializes the early allocator.
 *
 * @discussion
 * This function isn't thread safe and must be serialized
 * by the caller.
 */
extern void mfm_initialize(void);

/*!
 * @function mfm_alloc_size()
 *
 * @brief
 * Returns the size of an allocation made with @c mfm_alloc(),
 * or @c 0 if the pointer doesn't belong to it.
 */
extern size_t mfm_alloc_size(const void *ptr);

/*!
 * @function mfm_alloc()
 *
 * @brief
 * Perform an allocation with the early allocator.
 *
 * @discussion
 * The early allocator will only support a very small subset
 * of malloc features. Its allocations will always be zeroed,
 * and 16 bytes aligned.
 */
extern void *mfm_alloc(size_t size);

/*!
 * @function mfm_free()
 *
 * @brief
 * Frees a pointer allocated with @c mfm_alloc().
 *
 * @discussion
 * Passing a pointer that doesn't belong to the early allocator
 * is undefined and will result in process termination.
 */
extern void mfm_free(void *ptr);

/*!
 * @function mfm_claimed_address()
 *
 * @brief
 * Returns whether a pointer belongs to the early allocator.
 */
extern bool mfm_claimed_address(void *ptr);

/*!
 * @function mfm_zone_address()
 *
 * @brief
 * Returns an opaque pointer to the state of the early allocator, to be used
 * only to pass to introspection functions.
 */
extern void *mfm_zone_address(void);

extern const struct malloc_introspection_t mfm_introspect;

#endif // CONFIG_EARLY_MALLOC

#pragma GCC visibility pop
#endif // __EARLY_MALLOC_H
