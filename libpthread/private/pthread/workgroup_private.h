/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef __PTHREAD_WORKGROUP_PRIVATE_H__
#define __PTHREAD_WORKGROUP_PRIVATE_H__

#include <pthread/pthread.h>
#include <os/workgroup.h>

__BEGIN_DECLS

/*!
 * @function pthread_create_with_workgroup_np
 *
 * @abstract
 * Creates a pthread that joins a specified workgroup and can never leave it.
 *
 * @param wg
 * The workgroup the new thread should join.  Must not be NULL.
 *
 * @result
 * Returns any result returned by pthread_create(3): zero if successful,
 * otherwise one of its documented error numbers.
 *
 * @discussion
 * Parameters follow pthread_create(3), with the addition of `wg`.
 *
 * To eventually terminate the thread, the `start_routine` must return - the
 * thread may not terminate using pthread_exit(3).
 *
 * Failure by the new thread to join the specified workgroup for any reason will
 * result in a crash, so clients must take care to ensure that the failures
 * documented for os_workgroup_join(3) won't occur.
 *
 * The thread may not leave its workgroup.
 */
SPI_AVAILABLE(macos(12.0), ios(15.0), tvos(15.0), watchos(8.0))
int
pthread_create_with_workgroup_np(pthread_t _Nullable * _Nonnull thread,
		os_workgroup_t _Nonnull wg, const pthread_attr_t * _Nullable attr,
		void * _Nullable (* _Nonnull start_routine)(void * _Nullable),
		void * _Nullable arg);

#if defined(PTHREAD_WORKGROUP_SPI) && PTHREAD_WORKGROUP_SPI

/*
 * Internal implementation details below.
 */

struct pthread_workgroup_functions_s {
#define PTHREAD_WORKGROUP_FUNCTIONS_VERSION 1
	int pwgf_version;
	// V1
	int (* _Nonnull pwgf_create_with_workgroup)(
			pthread_t _Nullable * _Nonnull thread, os_workgroup_t _Nonnull wg,
			const pthread_attr_t * _Nullable attr,
			void * _Nullable (* _Nonnull start_routine)(void * _Nullable),
			void * _Nullable arg);
};

SPI_AVAILABLE(macos(12.0), ios(15.0), tvos(15.0), watchos(8.0))
void
pthread_install_workgroup_functions_np(
		const struct pthread_workgroup_functions_s * _Nonnull pwgf);

#endif // defined(PTHREAD_WORKGROUP_SPI) && PTHREAD_WORKGROUP_SPI

__END_DECLS

#endif /* __PTHREAD_WORKGROUP_PRIVATE_H__ */
