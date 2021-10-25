/*
* Copyright (c) 2020 Apple Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* This file contains Original Code and/or Modifications of Original Code
* as defined in and that are subject to the Apple Public Source License
* Version 2.0 (mhe 'License'). You may not use this file except in
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

// Do nothing if this header is used on it's own
#ifdef IN_SET

OS_ASSUME_NONNULL_BEGIN
__BEGIN_DECLS

#define os_set_t IN_SET(,_t)

OS_EXPORT
void
IN_SET(,_init)(os_set_t *s, os_set_config_t * _Nullable config,
			   int struct_version);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_set_init(os_set_t *s, os_set_config_t * _Nullable config) {
	IN_SET(,_init)(s, config, OS_SET_CONFIG_S_VERSION);
}

OS_EXPORT
void
IN_SET(,_destroy)(os_set_t *s);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_set_destroy(os_set_t *s) {
	IN_SET(,_destroy)(s);
}

OS_EXPORT
void
IN_SET(,_insert)(os_set_t *s, os_set_insert_val_t val);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_set_insert(os_set_t *s, os_set_insert_val_t val) {
	IN_SET(,_insert)(s, val);
}

OS_EXPORT
void *
IN_SET(,_find)(os_set_t *s, os_set_find_val_t val);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void * _Nullable
os_set_find(os_set_t *s, os_set_find_val_t val) {
	return IN_SET(,_find)(s, val);
}

OS_EXPORT
void *
IN_SET(,_delete)(os_set_t *s, os_set_find_val_t val);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void * _Nullable
os_set_delete(os_set_t *s, os_set_find_val_t val) {
	return IN_SET(,_delete)(s, val);
}

OS_EXPORT
void
IN_SET(,_clear)(os_set_t *s, OS_NOESCAPE IN_SET(,_payload_handler_t) handler);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_set_clear(os_set_t *s, OS_NOESCAPE IN_SET(,_payload_handler_t) handler) {
	IN_SET(,_clear)(s, handler);
}

OS_EXPORT
size_t
IN_SET(,_count)(os_set_t *s);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline size_t
os_set_count(os_set_t *s) {
	return IN_SET(,_count)(s);
}

OS_EXPORT
void
IN_SET(,_foreach)(os_set_t *s, OS_NOESCAPE IN_SET(,_payload_handler_t) handler);

OS_OVERLOADABLE OS_ALWAYS_INLINE
static inline void
os_set_foreach(os_set_t *s, OS_NOESCAPE IN_SET(,_payload_handler_t) handler) {
	IN_SET(,_foreach)(s, handler);
}

#undef os_set_t

__END_DECLS
OS_ASSUME_NONNULL_END

#endif // IN_SET
