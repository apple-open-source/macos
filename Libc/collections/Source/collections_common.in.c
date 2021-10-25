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


/*
	Required defined macros:
	IN_COMMON(X, Y) - this macro should generate an appropriate name with
 		prefix X and suffix Y. This name must not conflict with any
 		other instance of this file. Example, for a map of strings,
		IN_COMMON may be defined as PREFIX ## os_map_str ## SUFFIX, an
 		internal 'bucket' function may use IN_COMMON(_, _bucket), which
 		would generate _os_map_str_bucket.
	opaque_os_com_t - This macros should define the type of the opaque
 		data structure of this collection directly. It should be the
 		same size as _os_com_internal_struct.
	os_com_key_t  - this macro should define the type of data that the
 		collection is keyed off of
	os_com_config_t - this macro should define the type of the configuration
		struct for this particular collection

	After including this file, the following macros will be defined. Do not
		attempt to use any of the following after undefining any
 		required macros for this file.
	inline functions, following the header for os_map equivalents:
	  - os_common_init
	  - os_common_destroy
	  - os_common_insert
	  - os_common_find
	  - os_common_delete
	  - os_common_clear
	  - os_common_count
	  - os_common_foreach
	  - os_common_entry
	Types:
	  - os_common_payload_handler_t
 */
#include <sys/types.h>

#include "collections_utilities.h"


// =========== Helpers,  shared by all common collections ===========

#ifndef _COLLETIONS_COM_HELPERS_C
#define _COLLETIONS_COM_HELPERS_C

struct _os_com_internal_struct {
		void		*data;
		uint32_t	count;
		uint32_t	size;
		uint16_t	grow_shift;
		uint16_t	max_bucket_offset;
};

#define _COM_MAX_FILL_NUMER 4
#define _COM_MAX_FILL_DENOM 5
#define _COM_GROW_SHIFT_RATE 8
#define _COM_MIN_FILL_DENOM 8

OS_ALWAYS_INLINE
static inline uint32_t
_com_next(uint32_t i, uint32_t size)
{
	i++;
	return i >= size ? 0 : i;
}

OS_ALWAYS_INLINE
static inline uint32_t
_com_prev(uint32_t i, uint32_t size)
{
	return ((i != 0) ? i : size) - 1;
}

#endif

// =========== Helpers,  defined per common collections ===========

#define _os_com_t IN_COMMON(_,_t)
typedef struct _os_com_internal_struct _os_com_t;

#define os_com_hash IN_COMMON(,_hash)
#define os_com_key_equals IN_COMMON(,_key_equals)


#define _os_com_bucket IN_COMMON(_,_probe_bucket)

static inline uint32_t
_os_com_bucket(_os_com_t *m, os_com_key_t key)
{
	return os_com_hash(key) % m->size;
}

#define _os_com_bucket_offset IN_COMMON(_,_bucket_offset)

static inline uint32_t
_os_com_bucket_offset(_os_com_t *m, os_com_key_t key, uint32_t i)
{
	uint32_t bucket = _os_com_bucket(m, key);
	return (i >= bucket) ? (i - bucket) : ((m->size + i) - bucket);
}

#ifdef DEBUG

#define _get_next_offset IN_COMMON(_,_verify_next_offset)

static inline int64_t
_get_next_offset(_os_com_t *m, _os_com_data_ref_t data,
		uint32_t index, uint32_t size)
{
	uint32_t next_index = _com_next(index, size);

	if (_get_val(data, next_index) == NULL) {
		return -1;
	}

	return _os_com_bucket_offset(m, _get_key(data, next_index),
					   next_index);
}

#define _os_com_verify IN_COMMON(_,_verify)

static void
_os_com_verify(_os_com_t *m)
{
	_os_com_data_ref_t data;
	_get_data_ref(m->data, m->size, &data);

	int64_t current_bucket_offset;
	int64_t next_bucket_offset;
	if (_get_val(data, 0) == NULL) {
		next_bucket_offset = -1;
	} else {
		next_bucket_offset = _os_com_bucket_offset(m,
					_get_key(data, 0), 0);
	}

	uint32_t size = m->size;

	for (uint32_t index = 0; index < size; index++){
		current_bucket_offset = next_bucket_offset;
		next_bucket_offset = _get_next_offset(m, data, index, size);

		DEBUG_ASSERT(next_bucket_offset <= current_bucket_offset + 1);
	}

}
#else

#define _os_com_verify(A)

#endif

#define _swap_if_needed IN_COMMON(_,_swap_if_needed)

static inline void
_swap_if_needed(_os_com_t *m, _os_com_data_ref_t data, os_com_key_t *key,
		void **val, uint32_t *bucket_offset, uint32_t i)
{
	if (*bucket_offset != 0) {
		os_com_key_t loop_key = _get_key(data, i);

		// Doesn't support inserting twice
		DEBUG_ASSERT(!os_com_key_equals(loop_key, *key));

		uint32_t loop_bucket_offset = _os_com_bucket_offset(m,
						loop_key, i);
		if (*bucket_offset > loop_bucket_offset) {
			if (*bucket_offset > m->max_bucket_offset) {
				assert(*bucket_offset <= UINT16_MAX);
				DEBUG_ASSERT(*bucket_offset ==
						 m->max_bucket_offset + 1);
				m->max_bucket_offset = *bucket_offset;
			}
			void *new_val = _get_val(data, i);
			_set_key(data, i, *key);
			_set_val(data, i, *val);
			*key = loop_key;
			*val = new_val;
			*bucket_offset = loop_bucket_offset;
		}
	}
}

#define _os_com_insert_no_rehash IN_COMMON(_,_insert_no_rehash)

static void
_os_com_insert_no_rehash(_os_com_t *m, os_com_key_t key, void *val)
{
	uint32_t size = m->size, loop_limit = m->size;
	uint32_t i = os_com_hash(key) % size;
	_os_com_data_ref_t data;
	_get_data_ref(m->data, m->size, &data);

	uint32_t bucket_offset = 0;
	for (;;) {
		DEBUG_ASSERT(bucket_offset ==
				 _os_com_bucket_offset(m, key, i));
		assert(loop_limit-- != 0);
		void *loop_val = _get_val(data, i);
		if (loop_val == NULL) {
			break;
		}

		_swap_if_needed(m, data, &key, &val, &bucket_offset, i);

		bucket_offset++;
		i = _com_next(i, size);
	}

	DEBUG_ASSERT(_get_val(data, i) == NULL);

	if (bucket_offset > m->max_bucket_offset) {
		assert(bucket_offset <= UINT16_MAX);
		DEBUG_ASSERT(bucket_offset == m->max_bucket_offset + 1);
		m->max_bucket_offset = bucket_offset;
	}
	_set_key(data, i, key);
	_set_val(data, i, val);
	m->count++;
}

#define _os_com_rehash IN_COMMON(_,_rehash)

static void
_os_com_rehash(_os_com_t *m, int direction)
{
	_os_com_verify(m);

	uint32_t old_size = m->size;

	_os_com_data_ref_t old_data;
	_get_data_ref(m->data, m->size, &old_data);

	// Grow shift is used instead of simply doubling the size each time in
	// order to increase the expected utilization, thus decreasing overall
	// memory usage, at the cost of more frequent rehashing.

	if (direction > 0) {
		m->size += (1 << m->grow_shift);
		if (m->size ==
			((uint32_t)_COM_GROW_SHIFT_RATE << m->grow_shift)) {
			m->grow_shift++;
		}
	} else if (direction < 0) {
		if (m->grow_shift > MAP_MINSHIFT) {
			m->grow_shift--;
		}
		m->size = roundup(m->size / 2, (1 << m->grow_shift));
	}

	m->count = 0;
	m->max_bucket_offset = 0;
	m->data = _alloc_data(m->size);
	assert(m->data);

	for (uint32_t i = 0; i < old_size; i++) {
		if (_get_val(old_data, i) == NULL) {
			continue;
		}

		_os_com_insert_no_rehash(m, _get_key(old_data, i),
			_get_val(old_data, i));
	}
	_free_data(&old_data);

	_os_com_verify(m);
}


#define _os_com_find_helper_empty_key IN_COMMON(_,_find_helper_empty_key)

#define _os_com_find_helper IN_COMMON(_,_find_helper)

static inline void *
_os_com_find_helper(_os_com_t *m, os_com_key_t key, uint32_t *i)
{
	if (m->count == 0) {
		return NULL;
	}


	uint32_t size = m->size, loop_limit = m->size;
	_os_com_data_ref_t data;
	_get_data_ref(m->data, m->size, &data);

	*i = _os_com_bucket(m, key);

	uint32_t bucket_offset = 0;

	for (;;) {
		assert(loop_limit-- != 0);

		if (bucket_offset > m->max_bucket_offset ||
			_get_val(data, *i) == NULL) {
			return NULL;
		}

		os_com_key_t loop_key = _get_key(data, *i);

		if (os_com_key_equals(key, loop_key)) {
			return _get_val(data, *i);
		}
		*i = _com_next(*i, size);
		bucket_offset++;
	}
}

#define _os_com_remove_entry IN_COMMON(_,_remove_entry)

static void
_os_com_remove_entry(_os_com_t *m, uint32_t current_index)
{
	_os_com_data_ref_t data;
	_get_data_ref(m->data, m->size, &data);

	uint32_t size = m->size;

	uint32_t next_index = _com_next(current_index, size);
	void *next_val = _get_val(data, next_index);
	os_com_key_t next_key = _get_key(data, next_index);
	while(next_val != NULL &&
		  _os_com_bucket(m, next_key) != next_index) {
		_set_key(data, current_index, next_key);
		_set_val(data, current_index, next_val);

		DEBUG_ASSERT(_os_com_bucket_offset(m,
					next_key, current_index) <
				 _os_com_bucket_offset(m, next_key,
					next_index));

		current_index = next_index;
		next_index = _com_next(current_index, size);
		next_val = _get_val(data, next_index);
		next_key = _get_key(data, next_index);
	}

	_set_val(data, current_index, NULL);
	m->count--;

	if ((m->size >= MAP_MINSIZE * 2) &&
		(m->count < m->size / _COM_MIN_FILL_DENOM)) {
		// if the collection density drops below 12%, shrink it
		_os_com_rehash(m, -1);
	}
}

// =========== Implementation of common collection functions ===========


#define os_common_init IN_COMMON(_common_,_init)

static inline void
os_common_init(opaque_os_com_t *m_raw, os_com_config_t *config,
		  __unused int struct_version)
{
	static_assert(sizeof(opaque_os_com_t) == sizeof(_os_com_t),
			  "Opaque collection type incorrect size");
	_os_com_t *m = (_os_com_t *)m_raw;

	if (config) {
		m->size = MAX(config->initial_size, MAP_MINSIZE);
	} else {
		m->size = MAP_MINSIZE;
	}

	m->count = 0;
	m->max_bucket_offset = 0;
	m->data = _alloc_data(m->size);
	assert(m->data != NULL);
	m->grow_shift = MAP_MINSHIFT;
	DEBUG_ASSERT_MAP_INVARIANTS(m);
}


#define os_common_destroy IN_COMMON(_common_,_destroy)

static inline void
os_common_destroy(opaque_os_com_t *m_raw)
{
	_os_com_t *m = (_os_com_t *)m_raw;
	free(m->data);
	m->data = NULL;
	m->size = 0;
}

#define os_common_insert IN_COMMON(_common_,_insert)

static inline void
os_common_insert(opaque_os_com_t *m_raw, os_com_key_t key, void *val)
{
	_os_com_t *m = (_os_com_t *)m_raw;

	assert(val != NULL);

	if (m->count >= _COM_MAX_FILL_NUMER * m->size /
		_COM_MAX_FILL_DENOM) {
		_os_com_rehash(m, 1);
	}

	_os_com_insert_no_rehash(m, key, val);

	DEBUG_ASSERT_MAP_INVARIANTS(m);
}


#define os_common_find IN_COMMON(_common_,_find)

static inline void *
os_common_find(opaque_os_com_t *m_raw, os_com_key_t key)
{
	_os_com_t *m = (_os_com_t *)m_raw;
	uint32_t i;
	return _os_com_find_helper(m, key, &i);
}


#define os_common_delete IN_COMMON(_common_,_delete)

static inline void *
os_common_delete(opaque_os_com_t *m_raw, os_com_key_t key)
{
	_os_com_t *m = (_os_com_t *)m_raw;
	uint32_t i;

	void *val = _os_com_find_helper(m, key, &i);
	if (val == NULL) {
		return NULL;
	}

	_os_com_remove_entry(m, i);

	return val;
}

#define os_common_payload_handler_t IN_COMMON(_common_,_payload_handler_t)
typedef bool (^os_common_payload_handler_t) (os_com_key_t, void *);

#define os_common_clear IN_COMMON(_common_,_clear)

static inline void
os_common_clear(opaque_os_com_t *m_raw,
			OS_NOESCAPE os_common_payload_handler_t handler)
{
	_os_com_t *m = (_os_com_t *)m_raw;

	_os_com_data_ref_t oldData;
	_get_data_ref(m->data, m->size, &oldData);
	uint32_t oldSize = m->size;

	m->count = 0;
	m->max_bucket_offset = 0;
	m->size =  MAP_MINSIZE;
	m->data = _alloc_data(m->size);
	m->grow_shift = MAP_MINSHIFT;
	DEBUG_ASSERT_MAP_INVARIANTS(m);

	if (handler != NULL) {
		for (uint32_t i = 0; i < oldSize; i++) {
			void *val = _get_val(oldData, i);

			if (val != NULL) {
				handler(_get_key(oldData, i), val);
			}
		}
	}

	_free_data(&oldData);
}


#define os_common_count IN_COMMON(_common_,_count)

static inline size_t
os_common_count(opaque_os_com_t *m_raw)
{
	_os_com_t *m = (_os_com_t *)m_raw;
	return m->count;
}


#define os_common_foreach IN_COMMON(_common_,_foreach)

static inline void
os_common_foreach(opaque_os_com_t *m_raw,
		 OS_NOESCAPE os_common_payload_handler_t handler)
{
	_os_com_t *m = (_os_com_t *)m_raw;
	_os_com_data_ref_t data;
	_get_data_ref(m->data, m->size, &data);

	for (uint32_t i = 0; i < m->size; i++) {
		void *val = _get_val(data, i);

		if (val != NULL) {
			if (!handler(_get_key(data, i), val)){
				break;
			}
		}
	}
}

#ifdef MAP_SUPPORTS_ENTRY

#define os_common_entry IN_COMMON(_common_,_entry)

static inline os_com_key_t
os_common_entry(opaque_os_com_t *m_raw, os_com_key_t key)
{
	_os_com_t *m = (_os_com_t *)m_raw;
	_os_com_data_ref_t data;
	_get_data_ref(m->data, m->size, &data);

	uint32_t i;
	if (_os_com_find_helper(m, key, &i) == NULL) {
		return (os_com_key_t)NULL;
	}
	return _get_key(data, i);
}

#endif // MAP_SUPPORTS_ENTRY

