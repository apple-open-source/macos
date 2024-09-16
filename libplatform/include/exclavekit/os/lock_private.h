/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#ifndef __OS_LOCK_PRIVATE__
#define __OS_LOCK_PRIVATE__

#include <Availability.h>
#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <os/lock.h>
#include <os/base.h>
#include <threads.h>
#include <_liblibc/_threads.h>

OS_ASSUME_NONNULL_BEGIN

/*! @header
 * Low-level lock SPI
 */

#define OS_LOCK_SPI_VERSION 20230201

__BEGIN_DECLS

/*!
 * @typedef os_unfair_lock_options_t
 *
 * @const OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION
 * This flag is unused and only included to maintain compatability.
 *
 * @const OS_UNFAIR_LOCK_ADAPTIVE_SPIN
 * This flag is unused and only included to maintain compatability.
 */
OS_OPTIONS(os_unfair_lock_options, uint32_t,
	OS_UNFAIR_LOCK_NONE OS_SWIFT_NAME(None) = 0x00000000,
	OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION OS_SWIFT_NAME(DataSynchronization) = 0x00010000,
	OS_UNFAIR_LOCK_ADAPTIVE_SPIN OS_SWIFT_NAME(AdaptiveSpin) = 0x00040000
);

#if __swift__
#define OS_UNFAIR_LOCK_OPTIONS_COMPAT_FOR_SWIFT(name) \
		static const os_unfair_lock_options_t \
		name##_FOR_SWIFT OS_SWIFT_NAME(name) = name
OS_UNFAIR_LOCK_OPTIONS_COMPAT_FOR_SWIFT(OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION);
#undef OS_UNFAIR_LOCK_OPTIONS_COMPAT_FOR_SWIFT
#endif

/*!
 * @function os_unfair_lock_lock_with_options
 *
 * @abstract
 * Locks an os_unfair_lock.
 *
 * @param lock
 * Pointer to an os_unfair_lock.
 *
 * @param options
 * Options to alter the behavior of the lock. See os_unfair_lock_options_t.
 */
OS_UNFAIR_LOCK_AVAILABILITY
OS_EXPORT OS_NOTHROW OS_NONNULL_ALL
void os_unfair_lock_lock_with_options(os_unfair_lock_t lock,
		os_unfair_lock_options_t options);

#if __has_attribute(cleanup)

/*!
 * @function os_unfair_lock_scoped_guard_unlock
 *
 * @abstract
 * Used by os_unfair_lock_lock_scoped_guard
 */
OS_UNFAIR_LOCK_AVAILABILITY
OS_INLINE OS_ALWAYS_INLINE OS_NOTHROW OS_NONNULL_ALL
void
os_unfair_lock_scoped_guard_unlock(os_unfair_lock_t _Nonnull * _Nonnull lock)
{
	os_unfair_lock_unlock(*lock);
}

/*!
 * @function os_unfair_lock_lock_scoped_guard
 *
 * @abstract
 * Same as os_unfair_lock_lock() except that os_unfair_lock_unlock() is
 * automatically called when the enclosing C scope ends.
 *
 * @param name
 * Name for the variable holding the guard.
 *
 * @param lock
 * Pointer to an os_unfair_lock.
 *
 * @see os_unfair_lock_lock
 * @see os_unfair_lock_unlock
 */
#define os_unfair_lock_lock_scoped_guard(guard_name, lock) \
	os_unfair_lock_t \
		__attribute__((cleanup(os_unfair_lock_scoped_guard_unlock))) \
		guard_name = lock; \
	os_unfair_lock_lock(guard_name)

#endif // __has_attribute(cleanup)

/*! @group os_unfair_recursive_lock SPI
 *
 * @abstract
 * Similar to os_unfair_lock, but recursive.
 *
 * @discussion
 * Must be initialized with OS_UNFAIR_RECURSIVE_LOCK_INIT
 */

#define OS_UNFAIR_RECURSIVE_LOCK_AVAILABILITY \
		__OSX_AVAILABLE(10.14) __IOS_AVAILABLE(12.0) \
		__TVOS_AVAILABLE(12.0) __WATCHOS_AVAILABLE(5.0)

#define OS_UNFAIR_RECURSIVE_LOCK_INIT \
		(os_unfair_recursive_lock){ .ourl_lock = _LIBLIBC_MTX_RECURSIVE_INIT }

/*!
 * @typedef os_unfair_recursive_lock
 *
 * @abstract
 * Low-level lock that allows waiters to block efficiently on contention
 *
 * @discussion
 * See os_unfair_lock.
 *
 */
OS_UNFAIR_RECURSIVE_LOCK_AVAILABILITY
typedef struct os_unfair_recursive_lock_s {
	struct _liblibc_mtx ourl_lock;
} os_unfair_recursive_lock, *os_unfair_recursive_lock_t;

/*!
 * @function os_unfair_recursive_lock_lock_with_options
 *
 * @abstract
 * See os_unfair_lock_lock_with_options
 */
OS_UNFAIR_RECURSIVE_LOCK_AVAILABILITY
OS_EXPORT OS_NOTHROW OS_NONNULL_ALL
void os_unfair_recursive_lock_lock_with_options(os_unfair_recursive_lock_t lock,
		os_unfair_lock_options_t options);

/*!
 * @function os_unfair_recursive_lock_lock
 *
 * @abstract
 * See os_unfair_lock_lock
 */
OS_UNFAIR_RECURSIVE_LOCK_AVAILABILITY
OS_EXPORT OS_NOTHROW OS_NONNULL_ALL
OS_SWIFT_UNAVAILABLE_FROM_ASYNC("Use OSAllocatedUnfairLock.performWhileLocked() for async-safe scoped locking")
void os_unfair_recursive_lock_lock(os_unfair_recursive_lock_t lock);

/*!
 * @function os_unfair_recursive_lock_trylock
 *
 * @abstract
 * See os_unfair_lock_trylock
 */
OS_UNFAIR_RECURSIVE_LOCK_AVAILABILITY
OS_EXPORT OS_NOTHROW OS_WARN_RESULT OS_NONNULL_ALL
OS_SWIFT_UNAVAILABLE_FROM_ASYNC("Use OSAllocatedUnfairLock.tryPerformWhileLocked() for async-safe scoped locking")
bool os_unfair_recursive_lock_trylock(os_unfair_recursive_lock_t lock);

/*!
 * @function os_unfair_recursive_lock_unlock
 *
 * @abstract
 * See os_unfair_lock_unlock
 */
OS_UNFAIR_RECURSIVE_LOCK_AVAILABILITY
OS_EXPORT OS_NOTHROW OS_NONNULL_ALL
OS_SWIFT_UNAVAILABLE_FROM_ASYNC("Use OSAllocatedUnfairLock.performWhileLocked() for async-safe scoped locking")
void os_unfair_recursive_lock_unlock(os_unfair_recursive_lock_t lock);

/*!
 * @function os_unfair_recursive_lock_assert_owner
 *
 * @abstract
 * See os_unfair_lock_assert_owner
 */
OS_UNFAIR_RECURSIVE_LOCK_AVAILABILITY
OS_INLINE OS_ALWAYS_INLINE OS_NOTHROW OS_NONNULL_ALL
void
os_unfair_recursive_lock_assert_owner(const os_unfair_recursive_lock *lock)
{
	os_unfair_lock_assert_owner((os_unfair_lock *)&lock->ourl_lock);
}

/*!
 * @function os_unfair_recursive_lock_assert_not_owner
 *
 * @abstract
 * See os_unfair_lock_assert_not_owner
 */
OS_UNFAIR_RECURSIVE_LOCK_AVAILABILITY
OS_INLINE OS_ALWAYS_INLINE OS_NOTHROW OS_NONNULL_ALL
void
os_unfair_recursive_lock_assert_not_owner(const os_unfair_recursive_lock *lock)
{
	os_unfair_lock_assert_not_owner((os_unfair_lock *)&lock->ourl_lock);
}

__END_DECLS

OS_ASSUME_NONNULL_END

#endif // __OS_LOCK_PRIVATE__
