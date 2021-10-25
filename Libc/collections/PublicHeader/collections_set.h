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

#ifndef _OS_COLLECTIONS_SET_H
#define _OS_COLLECTIONS_SET_H

#include <os/base.h>
#include <stdint.h>
#include <stdbool.h>
#include <TargetConditionals.h>
#include <sys/types.h>

OS_ASSUME_NONNULL_BEGIN

struct os_set_config_s
{
	const char *name; // Only used when DEBUG is set
	uint32_t initial_size; // If 0, default will be used
};

// Increment this when changing os_set_config_s
#define OS_SET_CONFIG_S_VERSION 1

typedef struct os_set_config_s os_set_config_t;


// *** HASH SETS ***
// Stores values. Not safe for concurrent use.

#if TARGET_RT_64_BIT
#define OPAQUE_SET_SIZE 3
#else
#define OPAQUE_SET_SIZE 4
#endif

struct _os_opaque_32_ptr_set_s {
	void *data[OPAQUE_SET_SIZE];
};

struct _os_opaque_64_ptr_set_s {
	void *data[OPAQUE_SET_SIZE];
};

struct _os_opaque_str_ptr_set_s {
	void *data[OPAQUE_SET_SIZE];
};

// Set with 'pointers to 32 bit values' values (uint32_t *)
// Values must be valid pointers.
typedef struct _os_opaque_32_ptr_set_s os_set_32_ptr_t ;

// Set with 'pointers to 64 bit values' values (uint64_t *)
// Values must be valid pointers.
typedef struct _os_opaque_64_ptr_set_s os_set_64_ptr_t ;

// Set with 'pointers to string pointers' values (const char **)
// Values must be valid pointers.
typedef struct _os_opaque_str_ptr_set_s os_set_str_ptr_t ;


/*!
* @function os_set_init
* Initialize a set.
*
* @param s
* The set to initialize
*
* @param config
* The configuration to use for this set
*
* @discussion
* An initialized set will use additional memory which must be freed with
* os_set_destroy.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_set_init(os_set_32_ptr_t *s, os_set_config_t * _Nullable config);

/*!
* @function os_set_destroy
* Destroy a set.
*
* @param s
* The set to destroy.
*
* @discussion
* This will free all memory used by the set, but will not take any action
* on the values that were in the set at the time.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_set_destroy(os_set_32_ptr_t *s);

/*!
* @function os_set_insert
* Insert an element into a set.
*
* @param s
* The set to insert into
*
* @param val
* The value to insert; cannot be NULL (use os_set_delete to remove entries)
*
* @discussion
* This will insert an element into the set, growing the set if needed. Does not
* support replacing an existing val, so inserting twice without a remove will
* cause undefined behavior. Inserting pointers that are considered equal is undefined. Changing a value
* after inserting is not allowed.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_set_insert(os_set_32_ptr_t *s, uint32_t *val);

/*!
* @function os_set_find
* Find an element in a set.
*
* @param s
* The set to search
*
* @param val
* The val to search for
*
* @result
* The value stored at key, or NULL if no value is present.
*
* @discussion
* For 32_ptr, 64_ptr, and str_ptr sets, the val passed to find is the pointed-to value. Of type uint32_t, uint64_t,
* and const char * respectively.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void * _Nullable
os_set_find(os_set_32_ptr_t *s, uint32_t val);

/*!
* @function os_set_delete
* Remove an element from a set.
*
* @param s
* The set to remove the element from
*
* @param val
* The val of the element to be removed
*
* @result
* The value stored at key, or NULL if no value is present.
*
* @discussion
* Has no effect if the key is not present. For 32_ptr, 64_ptr, and str_ptr sets, the val passed to find is the
* pointed-to value. Of type uint32_t, uint64_t, and const char * respectively.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void * _Nullable
os_set_delete(os_set_32_ptr_t *s, uint32_t val);

typedef bool (^os_set_32_ptr_payload_handler_t_) (uint32_t *);

/*!
* @function os_set_clear
* Removes all elements from a set.
*
* @param s
* The set to remove the elements from
*
* @param handler
* A handler that will be called for all elements in the set. Handler may be
* NULL.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_set_clear(os_set_32_ptr_t *s,
	     OS_NOESCAPE os_set_32_ptr_payload_handler_t_ _Nullable handler);

/*!
* @function os_set_count
* Gets the number of items present in a set
*
* @param s
* The set to get the count of
*
* @result
* Returns the number of items present in the set
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline size_t
os_set_count(os_set_32_ptr_t *s);

/*!
* @function os_set_foreach
*Iterate over the values in a set.
*
* @param s
* The set to iterate over
*
* @param handler
* The handler to call for each entry in the set.
*
* @discussion
* Will invoke handler for each value in the set. Modifying the set
* during this iteration is not permitted. The handler may be called on the
* entries in any order.
*/
OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_set_foreach(os_set_32_ptr_t *s,
		 OS_NOESCAPE os_set_32_ptr_payload_handler_t_ handler);


OS_ASSUME_NONNULL_END

#include <os/_collections_set.h>

#endif /* _OS_COLLECTIONS_SET_H */
