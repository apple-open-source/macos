/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * @header DSEventSemaphore
 * Implementation of the DSEventSemaphore (gating lock) class.
 */

#include <limits.h>		// for LONG_MAX
#include <sys/time.h>	// for struct timespec and gettimeofday()

#include "DSEventSemaphore.h"

/******************************************************************************
	==>  DSEventSemaphore class implementation  <==
******************************************************************************/

DSEventSemaphore::DSEventSemaphore (bool posted)
	: DSSemaphore (posted ? LONG_MAX : 0)
{
}

DSEventSemaphore::~DSEventSemaphore (void)
{
}

// ---------------------------------------------------------------------------
//	Signal()
//	Make the semaphore available to all waiting threads as well as the threads
//	that will call Wait() before the next call to Reset()
// ---------------------------------------------------------------------------

void DSEventSemaphore::Signal ( void )
{
	::pthread_mutex_lock( &mConditionLock );
    //	mExcessSignals = LONG_MAX;
	if (mExcessSignals < 0)
	{
		mExcessSignals = LONG_MAX;
	}
	else
	{
		mExcessSignals++;
	}
	::pthread_cond_broadcast (&mSemaphore);
	::pthread_mutex_unlock (&mConditionLock);
}


// ---------------------------------------------------------------------------
//	Wait()
//	Return immediately if Signal() has been called since the last reset.
//	Otherwise, block until a thread calls Signal().
//	This implementation is different than DSSemaphore's because it does not
//	loop over condition_wait(); if it wakes up it assumes things are good to
//	go. (The thread that calls Signal() almost always immediately calls
//	Reset().)
// ---------------------------------------------------------------------------

sInt32 DSEventSemaphore::Wait (sInt32 milliSecs)
{
	::pthread_mutex_lock (&mConditionLock);
	if ( (mExcessSignals <= 0) && (milliSecs == kNever) )
	{
		::pthread_mutex_unlock (&mConditionLock);
		return semTimedOutErr;
	}
	// This is the difference between this implementation and its parent's!
	if (mExcessSignals <= 0)
	{
		if (milliSecs == kForever)
			::pthread_cond_wait (&mSemaphore, &mConditionLock);
		else {
			struct timeval	tvNow;
			struct timespec	tsTimeout;

			// Timeout is passed as an absolute time!
			::gettimeofday (&tvNow, NULL);
			TIMEVAL_TO_TIMESPEC (&tvNow, &tsTimeout);
			tsTimeout.tv_sec += (milliSecs / 1000);
			tsTimeout.tv_nsec += ((milliSecs % 1000) * 1000000);
			if (ETIMEDOUT == ::pthread_cond_timedwait (&mSemaphore,
											&mConditionLock, &tsTimeout)) {
				::pthread_mutex_unlock (&mConditionLock);
				return semTimedOutErr;
			}
		}
	}

	if ( (!mDestroying) && (mExcessSignals > 0) )
	{
		mExcessSignals--;
	}
	::pthread_mutex_unlock (&mConditionLock);
	return (mDestroying ? semDestroyedErr : semNoErr);
}

// ---------------------------------------------------------------------------
//	Reset()
//	Make the seamphore unavailable to any thread until the next call to
//	Signal()
// ---------------------------------------------------------------------------
uInt32
DSEventSemaphore::Reset (void)
{
	::pthread_mutex_lock (&mConditionLock);
	if (mExcessSignals <= 0) {
		::pthread_mutex_unlock (&mConditionLock);
		return semAlreadyResetErr;
	}
	mExcessSignals = 0;
	::pthread_mutex_unlock (&mConditionLock);
	return 1;
}
