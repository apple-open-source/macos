/*
 * Copyright (c) 1999-2006 Apple Computer, Inc. All rights reserved.
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

#include "webdav_utils.h"

/*****************************************************************************/
/*
 * Lock a webdavnode
 */
__private_extern__ int webdav_lock(struct webdavnode *pt, enum webdavlocktype locktype)
{
	if (locktype == WEBDAV_SHARED_LOCK)
		lck_rw_lock_shared(&pt->pt_rwlock);
	else
		lck_rw_lock_exclusive(&pt->pt_rwlock);

	pt->pt_lockState = locktype;
	
#if 0
	/* For Debugging... */
	if (locktype != WEBDAV_SHARED_LOCK) {
		pt->pt_activation = (void *) current_thread();
	}
#endif

	return (0);
}

/*****************************************************************************/
/*
 * Unlock a webdavnode
 */
__private_extern__ void webdav_unlock(struct webdavnode *pt)
{
	lck_rw_done(&pt->pt_rwlock);
	pt->pt_lockState = 0;
}

void timespec_to_webdav_timespec64(struct timespec ts, struct webdav_timespec64 *wts)
{
	wts->tv_sec = ts.tv_sec;
	wts->tv_nsec = ts.tv_nsec;
}

void webdav_timespec64_to_timespec(struct webdav_timespec64 wts, struct timespec *ts)
{
#ifdef __LP64__
	ts->tv_sec = wts.tv_sec;
	ts->tv_nsec = wts.tv_nsec;
#else
	ts->tv_sec = (uint32_t)wts.tv_sec;
	ts->tv_nsec = (uint32_t)wts.tv_nsec;	
#endif
}