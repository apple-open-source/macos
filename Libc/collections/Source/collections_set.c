/*
* Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include <os/collections_set.h>

#include <os/base_private.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>

static inline bool
os_set_str_ptr_key_equals(const char * * a, const char * *b)
{
	return *a == *b || strcmp(*a, *b) == 0;
}

static inline uint32_t
os_set_str_ptr_hash(const char * *key)
{
	uint32_t hash = 0;
	for (const char *runner = *key; *runner; runner++) {
		hash += (unsigned char)(*runner);
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

static inline bool
os_set_32_ptr_key_equals(uint32_t *a, uint32_t *b)
{
	return *a == *b;
}

static inline uint32_t
os_set_32_ptr_hash(uint32_t *x_ptr)
{
	uint32_t x = *x_ptr;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return (uint32_t)x;
}

static inline bool
os_set_64_ptr_key_equals(uint64_t *a, uint64_t *b)
{
	return *a == *b;
}

static inline uint32_t
os_set_64_ptr_hash(uint64_t *key)
{
	return os_set_32_ptr_hash((uint32_t *)key);
}

// The following symbols are required for each include of collections_set.in.c
// IN_SET(, _t)
//      EXAMPLE: os_set_64_ptr_t
//      The opaque representation of the set.
// IN_SET(, _hash)
//      EXAMPLE: os_set_64_ptr_hash
//      The default hash function for the set
// IN_SET(,_key_equals)
//      Example: os_set_64_ptr_key_equals
//      The equality check for this set

#define IN_SET(PREFIX, SUFFIX) PREFIX ## os_set_str_ptr ## SUFFIX
#define os_set_insert_val_t const char **
#define os_set_find_val_t const char *
#include "collections_set.in.c"
#undef IN_SET
#undef os_set_insert_val_t
#undef os_set_find_val_t

#define IN_SET(PREFIX, SUFFIX) PREFIX ## os_set_32_ptr ## SUFFIX
#define os_set_insert_val_t uint32_t *
#define os_set_find_val_t uint32_t
#include "collections_set.in.c"
#undef IN_SET
#undef os_set_insert_val_t
#undef os_set_find_val_t

#define IN_SET(PREFIX, SUFFIX) PREFIX ## os_set_64_ptr ## SUFFIX
#define os_set_insert_val_t uint64_t *
#define os_set_find_val_t uint64_t
#include "collections_set.in.c"
#undef IN_SET
#undef os_set_insert_val_t
#undef os_set_find_val_t

