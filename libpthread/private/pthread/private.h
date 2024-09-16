/*
 * Copyright (c) 2012 Apple Inc. All rights reserved.
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

#ifndef __PTHREAD_PRIVATE_H__
#define __PTHREAD_PRIVATE_H__

#include <sys/cdefs.h>
#include <TargetConditionals.h>
#include <Availability.h>
#include <os/tsd.h>
#include <mach/std_types.h>
#include <sys/types.h>
#include <pthread/tsd_private.h>

__API_AVAILABLE(macos(10.9), ios(7.0))
pthread_t pthread_main_thread_np(void);

struct _libpthread_functions {
	unsigned long version;
	void (*exit)(int); // added with version=1
	void *(*malloc)(size_t); // added with version=2
	void (*free)(void *); // added with version=2
};

/*!
 * @function pthread_chdir_np
 *
 * @abstract
 * Sets the per-thread current working directory.
 *
 * @discussion
 * This will set the per-thread current working directory to the provided path.
 * If this is used on a workqueue (dispatch) thread, it MUST be unset with
 * pthread_fchdir_np(-1) before returning.
 *
 * posix_spawn_file_actions_addchdir_np is a better approach if this call would
 * only be used to spawn a new process with a given working directory.
 *
 * @param path
 * The path of the new working directory.
 *
 * @result
 * 0 upon success, -1 upon error and errno is set.
 */
__API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
int pthread_chdir_np(const char *path);

/*!
 * @function pthread_fchdir_np
 *
 * @abstract
 * Sets the per-thread current working directory.
 *
 * @discussion
 * This will set the per-thread current working directory to the provided
 * directory fd.  If this is used on a workqueue (dispatch) thread, it MUST be
 * unset with pthread_fchdir_np(-1) before returning.
 *
 * posix_spawn_file_actions_addfchdir_np is a better approach if this call would
 * only be used to spawn a new process with a given working directory.
 *
 * @param fd
 * A file descriptor to the new working directory.  Pass -1 to unset the
 * per-thread working directory.
 *
 * @result
 * 0 upon success, -1 upon error and errno is set.
 */
__API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
int pthread_fchdir_np(int fd);

__API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0))
int pthread_attr_setcpupercent_np(pthread_attr_t * __restrict, int, unsigned long);

/*!
 * @function pthread_attr_setworkinterval_np
 *
 * @abstract
 * A private interface for libdispatch to configure work interval
 * properties on pthreads it requests.
 *
 * @discussion
 * This method expects the mach port name to represent a valid send right
 * to a work interval object in kernel. It takes an additional +1 ref to that
 * send right which is released in pthread_attr_destroy.
 */
__API_AVAILABLE(macos(14.3), ios(17.4), tvos(17.4), watchos(10.4), xros(1.1), driverkit(23.4))
int pthread_attr_setworkinterval_np(pthread_attr_t * __restrict, mach_port_t);

__API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0))
int pthread_current_stack_contains_np(const void *, size_t);

/*!
 * @function pthread_self_is_exiting_np
 *
 * @abstract
 * Returns whether the current thread is exiting.
 *
 * @discussion
 * This can be useful for certain introspection tools to know that malloc/free
 * is called from the TSD destruction codepath.
 *
 * @result
 * 0 if the thread is not exiting
 * 1 if the thread is exiting
 */
__API_AVAILABLE(macos(10.16), ios(14.0), tvos(14.0), watchos(7.0))
int pthread_self_is_exiting_np(void);

#ifdef __LP64__
#define _PTHREAD_STRUCT_DIRECT_THREADID_OFFSET   -8
#define _PTHREAD_STRUCT_DIRECT_TSD_OFFSET       224
#else
#define _PTHREAD_STRUCT_DIRECT_THREADID_OFFSET  -16
#define _PTHREAD_STRUCT_DIRECT_TSD_OFFSET       176
#endif

#if !TARGET_OS_SIMULATOR
#if defined(__i386__)
#define _pthread_direct_tsd_relative_access(type, offset) \
		(type *)((char *)_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_SELF) + \
		_PTHREAD_STRUCT_DIRECT_TSD_OFFSET + _PTHREAD_STRUCT_DIRECT_##offset##_OFFSET)
#elif defined(OS_GS_RELATIVE)
#define _pthread_direct_tsd_relative_access(type, offset) \
		(type OS_GS_RELATIVE *)(_PTHREAD_STRUCT_DIRECT_##offset##_OFFSET)
#elif defined(_os_tsd_get_base)
#define _pthread_direct_tsd_relative_access(type, offset)  \
		(type *)((char *)_os_tsd_get_base() + _PTHREAD_STRUCT_DIRECT_##offset##_OFFSET)
#else
#error "unknown configuration"
#endif
#else
#include <pthread/pthread.h> // for pthread_threadid_np
#endif // !TARGET_OS_SIMULATOR

/* N.B. DO NOT USE UNLESS YOU ARE REBUILT AS PART OF AN OS TRAIN WORLDBUILD */
__header_always_inline __pure2 uint64_t
_pthread_threadid_self_np_direct(void)
{
#ifdef _pthread_direct_tsd_relative_access
	return *_pthread_direct_tsd_relative_access(uint64_t, THREADID);
#else
	uint64_t threadid = 0;
	pthread_threadid_np(NULL, &threadid);
	return threadid;
#endif
}

__header_always_inline __pure2 pthread_t
_pthread_self_direct(void)
{
#if TARGET_OS_SIMULATOR || defined(__i386__) || defined(__x86_64__)
	return (pthread_t)_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_SELF);
#elif defined(__arm__) || defined(__arm64__)
	uintptr_t tsd_base = (uintptr_t)_os_tsd_get_base();
	return (pthread_t)(tsd_base - _PTHREAD_STRUCT_DIRECT_TSD_OFFSET);
#else
#error unsupported architecture
#endif
}

__header_always_inline __pure2 mach_port_t
_pthread_mach_thread_self_direct(void)
{
	return (mach_port_t)(uintptr_t)
			_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_MACH_THREAD_SELF);
}

__header_always_inline int *
_pthread_errno_address_direct(void)
{
	return (int *)_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_ERRNO);
}

/* get the thread specific errno value */
__header_always_inline int
_pthread_get_errno_direct(void)
{
	return *_pthread_errno_address_direct();
}

/* set the thread specific errno value */
__header_always_inline void
_pthread_set_errno_direct(int value)
{
	*_pthread_errno_address_direct() = value;
}

/* To be used by dispatch only */
#ifndef PTHREAD_HAVE_YIELD_TO_ENQUEUER

#define PTHREAD_HAVE_YIELD_TO_ENQUEUER 1
int
_pthread_yield_to_enqueuer_4dispatch(unsigned long slot, void *expected_val,
	unsigned int timeout_ms);

#endif

#endif // __PTHREAD_PRIVATE_H__
