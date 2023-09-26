/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
#include <os/lock.h>
#include <xrt/sync.h>
_Static_assert(sizeof(xrt_sync_object_t) <=
		sizeof(struct os_unfair_lock_s), "os_unfair_lock and xrt_sync_object_t size mismatch");

void os_unfair_lock_lock(os_unfair_lock_t lock)
{
	xrt_sync_mutex_lock((xrt_sync_mutex_t *) &lock->_os_unfair_lock_opaque);
}

bool os_unfair_lock_trylock(os_unfair_lock_t lock)
{
	return xrt_sync_mutex_try_lock((xrt_sync_mutex_t *) &lock->_os_unfair_lock_opaque);
}

void os_unfair_lock_unlock(os_unfair_lock_t lock)
{
	xrt_sync_mutex_unlock((xrt_sync_mutex_t *) &lock->_os_unfair_lock_opaque);
}

void os_unfair_lock_assert_owner(const os_unfair_lock *lock)
{
	assert(lock != NULL);
}

void os_unfair_lock_assert_not_owner(const os_unfair_lock *lock)
{
	assert(lock != NULL);
}
