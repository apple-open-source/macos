/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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


#ifndef __RADIX_TREE_H
#define __RADIX_TREE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * This is a radix tree implementation mapping 64 bit keys to 64 bit values.
 *
 * Its in-memory representation is also valid as a serial representation.
 */

struct radix_tree; 

static const uint64_t radix_tree_invalid_value = (uint64_t) -1; 

/*
 * Lookup a key in the radix tree and return its value.  Returns 
 * radix_tree_invalid_value (ie -1) if not found 
 */
__attribute__((visibility("default")))
uint64_t 
radix_tree_lookup(struct radix_tree *tree, uint64_t key);

/*
 * Insert an range of keys into a radix tree (possibly reallocing it).  Returns true on
 * success.
 *
 * Arguments:
 *
 *   treep: The tree to modify.  Will write to *treep if the tree needs to be realloc'd
 *   key: The first key to set
 *   size: The number of keys to set
 *   value: The value to set them too

 */
bool 
radix_tree_insert(struct radix_tree **treep, uint64_t key, uint64_t size, uint64_t value);


/* 
 * Delete a range of keys from a radix tree.  Returns true on success.
 *
 * Arguments
 *
 *   treep: The tree to modify.  Will write to *treep if the tree needs to be realloc'd
 *   key: The first key to delete
 *   size: The number of keys to delete
 */
bool
radix_tree_delete(struct radix_tree **treep, uint64_t key, uint64_t size);


/*
 * Create a radix tree
 */
struct radix_tree *
radix_tree_create();

/*
 * deallocate a radix tree
 */
void
radix_tree_destory(struct radix_tree *tree);

/*
 * Count the number of keys in a radix tree.
 */
__attribute__((visibility("default")))
uint64_t
radix_tree_count(struct radix_tree *tree);

/*
 * Get the size of the radix tree buffer
 */
uint64_t
radix_tree_size(struct radix_tree *tree);

#endif
