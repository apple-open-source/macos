/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 
/*!
 *  @header NSLSemaphore
 */
 
// ANSI / POSIX Headers
#include <sys/time.h>	// for struct timespec and gettimeofday()

// Project Headers
#include "NSLSemaphore.h"

/******************************************************************************
	==>  NSLSemaphore class implementation  <==
******************************************************************************/

NSLSemaphore::NSLSemaphore (
	SInt32 initialCount)
	: mExcessSignals (initialCount), mDestroying (false)
{
	::pthread_mutex_init (&mConditionLock, NULL) ;
	::pthread_cond_init (&mSemaphore, NULL) ;
}

NSLSemaphore::~NSLSemaphore (void)
{
	::pthread_mutex_lock (&mConditionLock) ;
	mDestroying = true ;
	::pthread_mutex_unlock (&mConditionLock) ;
	::pthread_cond_broadcast (&mSemaphore) ;

	::pthread_cond_destroy (&mSemaphore) ;
	::pthread_mutex_destroy (&mConditionLock) ;
}

void
NSLSemaphore::Signal (void)
{
	::pthread_mutex_lock (&mConditionLock) ;
	mExcessSignals++ ;
	::pthread_mutex_unlock (&mConditionLock) ;
	::pthread_cond_signal (&mSemaphore) ;
}

OSStatus
NSLSemaphore::Wait (SInt32 milliSecs)
{
	::pthread_mutex_lock (&mConditionLock) ;
	if ((mExcessSignals <= 0) && (milliSecs == kNever)) {
		::pthread_mutex_unlock (&mConditionLock) ;
		return semTimedOutErr ;
	}
	if (milliSecs == kForever) {
		while (!mDestroying && (mExcessSignals <= 0))
			::pthread_cond_wait (&mSemaphore, &mConditionLock) ;
	} else {
		struct timeval	tvNow ;
		struct timespec	tsTimeout ;

		// Timeout is passed as an absolute time!
		::gettimeofday (&tvNow, NULL) ;
		TIMEVAL_TO_TIMESPEC (&tvNow, &tsTimeout) ;
		tsTimeout.tv_sec += (milliSecs / 1000) ;
		tsTimeout.tv_nsec += ((milliSecs % 1000) * 1000000) ;
		while (!mDestroying && (mExcessSignals <= 0))
			if (ETIMEDOUT == ::pthread_cond_timedwait (&mSemaphore,
											&mConditionLock, &tsTimeout)) {
				::pthread_mutex_unlock (&mConditionLock) ;
				return semTimedOutErr ;
			}
	}
	if (!mDestroying)
		mExcessSignals-- ;
	::pthread_mutex_unlock (&mConditionLock) ;
	return (mDestroying ? (OSStatus)semDestroyedErr : noErr) ;
}
