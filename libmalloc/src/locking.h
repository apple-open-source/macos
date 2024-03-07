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

#ifndef __LOCKING_H
#define __LOCKING_H

typedef struct lock_exclaves_s {
	// Must match liblibc/include/libc/std/pthread.h
	uint32_t _Alignas(16) opaque[4];
} lock_exclave;

// Exclaves always uses pthread.h to avoid alignment difference with os/lock.h
#if !MALLOC_TARGET_EXCLAVES && !MALLOC_TARGET_EXCLAVES_INTROSPECTOR && __has_include(<os/lock.h>)
# define MALLOC_HAS_OS_LOCK 1
# include <os/lock.h>
# if __has_include(<os/lock_private.h>)
#  include <os/lock_private.h>
# endif // __has_include(<os/lock_private.h>)
# if defined(OS_UNFAIR_LOCK_INLINE) && OS_UNFAIR_LOCK_INLINE
#  define os_unfair_lock_lock_with_options(lock, options) \
		os_unfair_lock_lock_with_options_inline(lock, options)
#  define os_unfair_lock_trylock(lock) \
		os_unfair_lock_trylock_inline(lock)
#  define os_unfair_lock_unlock(lock) \
		os_unfair_lock_unlock_inline(lock)
# endif // defined(OS_UNFAIR_LOCK_INLINE) && OS_UNFAIR_LOCK_INLINE

typedef os_unfair_lock _malloc_lock_s;
# define _MALLOC_LOCK_INIT OS_UNFAIR_LOCK_INIT
#elif __has_include(<pthread.h>)
# define MALLOC_HAS_OS_LOCK 0
# include <pthread.h>

# if MALLOC_TARGET_EXCLAVES_INTROSPECTOR
typedef lock_exclave _malloc_lock_s;

#  define _MALLOC_LOCK_INIT (lock_exclave){0}
# else
_Static_assert(sizeof(lock_exclave) == sizeof(pthread_mutex_t),
	"lock_exclave size must match pthread_mutex_t on EC");
_Static_assert(_Alignof(lock_exclave) == _Alignof(pthread_mutex_t),
	"lock_exclave alignment must match pthread_mutex_t on EC");

typedef pthread_mutex_t _malloc_lock_s;
#  define _MALLOC_LOCK_INIT (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER
# endif // MALLOC_TARGET_EXCLAVES_INTROSPECTOR
#else
# error "No platform locking support!"
#endif // __has_include(<os/lock.h>)

__attribute__((always_inline))
static inline void
_malloc_lock_init(_malloc_lock_s *lock) {
#if !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
	*lock = _MALLOC_LOCK_INIT;
#else
	malloc_zone_error(MALLOC_ABORT_ON_ERROR, false,
			"exclaves lock not supported");
#endif // !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
}

MALLOC_ALWAYS_INLINE
static inline void
_malloc_lock_lock(_malloc_lock_s *lock) {
#if !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
# if MALLOC_HAS_OS_LOCK
#  if __has_include(<os/lock_private.h>)
	return os_unfair_lock_lock_with_options(lock, OS_UNFAIR_LOCK_ADAPTIVE_SPIN |
											OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION);
#  else
	return os_unfair_lock_lock(lock);
#  endif // __has_include(<os/lock_private.h>)
# else
	if (pthread_mutex_lock(lock)) {
		malloc_zone_error(MALLOC_ABORT_ON_ERROR, false,
				"Failed to acquire lock %p!\n", lock);
	}
# endif // MALLOC_HAS_OS_LOCK
#else
	malloc_zone_error(MALLOC_ABORT_ON_ERROR, false,
			"exclaves lock not supported");
#endif // !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
}

MALLOC_ALWAYS_INLINE
static inline bool
_malloc_lock_trylock(_malloc_lock_s *lock) {
#if !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
# if MALLOC_HAS_OS_LOCK
	return os_unfair_lock_trylock(lock);
# else
    return !pthread_mutex_trylock(lock);
# endif // MALLOC_HAS_OS_LOCK
#else
	malloc_zone_error(MALLOC_REPORT_CRASH, false,
			"exclaves lock not supported");
	return false;
#endif // !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
}

MALLOC_ALWAYS_INLINE
static inline void
_malloc_lock_unlock(_malloc_lock_s *lock) {
#if !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
# if MALLOC_HAS_OS_LOCK
	os_unfair_lock_unlock(lock);
# else
	if (pthread_mutex_unlock(lock)) {
		malloc_zone_error(MALLOC_ABORT_ON_ERROR, false,
				"Failed to release lock %p!\n", lock);
	}
# endif // MALLOC_HAS_OS_LOCK
#else
	malloc_zone_error(MALLOC_ABORT_ON_ERROR, false,
			"exclaves lock not supported");
#endif // !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
}

MALLOC_ALWAYS_INLINE
static inline void
_malloc_lock_assert_owner(_malloc_lock_s *lock) {
#if !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
# if MALLOC_HAS_OS_LOCK
	os_unfair_lock_assert_owner(lock);
# endif // MALLOC_HAS_OS_LOCK
#else
	malloc_zone_error(MALLOC_ABORT_ON_ERROR, false,
			"exclaves lock not supported");
#endif // !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
}

#endif // __LOCKING_H
