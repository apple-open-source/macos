/*
* Copyright (c) 2019 Apple Inc. All rights reserved.
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

#define opaque_os_map_t IN_MAP(,_t)

#define _os_map_data_segregated IN_MAP(_,_data_segregated)
struct _os_map_data_segregated {
	os_map_key_t 	*keys;
	void 		**vals;
};

#define _os_com_data_ref_t IN_MAP(_,_data_ref_t)
typedef struct _os_map_data_segregated _os_com_data_ref_t;

#define _alloc_data IN_MAP(_,_alloc_data)

static inline void *
_alloc_data(uint32_t size)
{
	assert(size < UINT32_MAX);
	void *result = calloc(size, sizeof(os_map_key_t) + sizeof(void *));
	assert(result != NULL);
	return result;
}


#define _get_data_ref IN_MAP(_,_get_data_ref)

static inline void
_get_data_ref(void *data_pointer, uint32_t size, _os_com_data_ref_t *data)
{
	data->keys = (os_map_key_t *)(data_pointer);
	data->vals = (void *)(((char *)(data_pointer)) +
			      (size * sizeof(os_map_key_t)));
}


#define _free_data IN_MAP(_,_free_data)

static inline void
_free_data(_os_com_data_ref_t *data)
{
	free((void *)data->keys);
}

#define _get_key(data, i) data.keys[i]

#define _get_val(data, i) data.vals[i]

#define _set_key(data, i, key) data.keys[i] = key

#define _set_val(data, i, val) data.vals[i] = val




#define IN_COMMON(X,Y) IN_MAP(X,Y)
#define os_com_key_t os_map_key_t
#define os_com_config_t os_map_config_t
#define opaque_os_com_t opaque_os_map_t

#include "collections_common.in.c"


#define os_map_init IN_MAP(,_init)

void
os_map_init(opaque_os_map_t *m_raw, os_map_config_t *config,
		  int struct_version)
{
	os_common_init(m_raw, config, struct_version);
}

#define os_map_destroy IN_MAP(,_destroy)

void
os_map_destroy(opaque_os_map_t *m_raw)
{
	os_common_destroy(m_raw);
}

#define os_map_insert IN_MAP(,_insert)

void
os_map_insert(opaque_os_map_t *m_raw, os_map_key_t key, void *val)
{
	os_common_insert(m_raw, key, val);
}

#define os_map_find IN_MAP(,_find)

void *
os_map_find(opaque_os_map_t *m_raw, os_map_key_t key)
{
	return os_common_find(m_raw, key);
}


#define os_map_delete IN_MAP(,_delete)

void *
os_map_delete(opaque_os_map_t *m_raw, os_map_key_t key)
{
	return os_common_delete(m_raw, key);
}

#define os_map_payload_handler_t IN_COMMON(,_payload_handler_t)
typedef bool (^os_map_payload_handler_t) (os_map_key_t, void *);

#define os_map_clear IN_MAP(,_clear)

void
os_map_clear(opaque_os_map_t *m_raw,
			OS_NOESCAPE os_common_payload_handler_t handler)
{
	os_common_clear(m_raw, handler);
}

#define os_map_count IN_MAP(,_count)

size_t
os_map_count(opaque_os_map_t *m_raw)
{
	return os_common_count(m_raw);
}

#define os_map_foreach IN_MAP(,_foreach)

void
os_map_foreach(opaque_os_map_t *m_raw,
		 OS_NOESCAPE os_common_payload_handler_t handler)
{
	os_common_foreach(m_raw, handler);
}

#ifdef MAP_SUPPORTS_ENTRY

#define os_map_entry IN_MAP(,_entry)

os_map_key_t
os_map_entry(opaque_os_map_t *m_raw, os_map_key_t key)
{
	return os_common_entry(m_raw, key);
}

#endif

#undef opaque_os_com_t
#undef os_com_config_t
#undef os_com_key_t
#undef IN_COMMON
