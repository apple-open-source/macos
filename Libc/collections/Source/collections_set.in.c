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

#include <sys/types.h>

#include "collections_utilities.h"

#define opaque_os_set_t IN_SET(,_t)

#define _os_set_data_segregated IN_SET(_,_data_segregated)
struct _os_set_data_segregated {
	os_set_insert_val_t 	*vals;
};

#define _os_com_data_ref_t IN_SET(_,_data_ref_t)
typedef struct _os_set_data_segregated _os_com_data_ref_t;

#define _alloc_data IN_SET(_,_alloc_data)

static inline void *
_alloc_data(uint32_t size)
{
	assert(size < UINT32_MAX);
	void *result = calloc(size, sizeof(os_set_insert_val_t));
	assert(result != NULL);
	return result;
}


#define _get_data_ref IN_SET(_,_get_data_ref)

static inline void
_get_data_ref(void *data_pointer, uint32_t size, _os_com_data_ref_t *data)
{
	data->vals = (os_set_insert_val_t *)data_pointer;
}


#define _free_data IN_SET(_,_free_data)

static inline void
_free_data(_os_com_data_ref_t *data)
{
	free((void *)data->vals);
}

// For sets - the 'key' and 'val' used by the common implementation are the
// same value.
#define _get_key(data, i) data.vals[i]

#define _get_val(data, i) data.vals[i]

#define _set_key(data, i, key) (void)data

#define _set_val(data, i, val) data.vals[i] = val



#define IN_COMMON(X,Y) IN_SET(X,Y)
#define os_com_key_t os_set_insert_val_t
#define os_com_config_t os_set_config_t
#define opaque_os_com_t opaque_os_set_t

#include "collections_common.in.c"


#define os_set_init IN_SET(,_init)

void
os_set_init(opaque_os_set_t *s_raw, os_set_config_t *config,
		  int struct_version)
{
	os_common_init(s_raw, config, struct_version);
}


#define os_set_destroy IN_SET(,_destroy)

void
os_set_destroy(opaque_os_set_t *s_raw)
{
	os_common_destroy(s_raw);
}


#define os_set_insert IN_SET(,_insert)

void
os_set_insert(opaque_os_set_t *s, os_set_insert_val_t val) {
	os_common_insert(s, val, val);
}

#define os_set_find IN_SET(,_find)

void * _Nullable
os_set_find(opaque_os_set_t *s, os_set_find_val_t val) {
	return os_common_find(s, &val);
}

#define os_set_delete IN_SET(,_delete)

void * _Nullable
os_set_delete(opaque_os_set_t *s, os_set_find_val_t val) {
	return os_common_delete(s, &val);
}

#define os_set_payload_handler_t IN_SET(,_payload_handler_t)
typedef bool (^os_set_payload_handler_t) (os_set_insert_val_t);

#define os_set_clear IN_SET(,_clear)

void
os_set_clear(opaque_os_set_t *s,
			OS_NOESCAPE os_set_payload_handler_t handler)
{
	if (handler == NULL) {
		os_common_clear(s, NULL);
	} else {
		os_common_clear(s, ^bool (os_set_insert_val_t val, __unused void *p){
			return handler(val);
		});
	}
}

#define os_set_count IN_SET(,_count)

size_t
os_set_count(opaque_os_set_t *s)
{
	return os_common_count(s);
}

#define os_set_foreach IN_SET(,_foreach)

void
os_set_foreach(opaque_os_set_t *s,
			OS_NOESCAPE os_set_payload_handler_t handler)
{
	os_common_foreach(s, ^bool (os_set_insert_val_t val, __unused void *p){
		return handler(val);
	});
}

#undef opaque_os_com_t
#undef os_com_config_t
#undef os_com_key_t
#undef IN_COMMON
