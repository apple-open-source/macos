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
 * @header DSMutexSemaphore
 * Implementation of the DSMutexSemaphore ( mutually exclusive lock ) class.
 */

#include <sys/time.h>	// for struct timespec and gettimeofday()

#include "DSMutexSemaphore.h"

//--------------------------------------------------------------------------------------------------
//	DSMutexSemaphore class implementation
//--------------------------------------------------------------------------------------------------

DSMutexSemaphore::DSMutexSemaphore ( bool owned )
	: DSSemaphore ( owned ? 0 : 1 ),
		mOwner ( owned ? ::pthread_self() : NULL )
{
}

DSMutexSemaphore::~DSMutexSemaphore ( void )
{
}

void
DSMutexSemaphore::Signal ( void )
{
	::pthread_mutex_lock( &mConditionLock );

	// If this is not the owning thread, simply return
	if ( mOwner != ::pthread_self() )
	{
		::pthread_mutex_unlock ( &mConditionLock );
		return;
	}
	// Release the lock as mExcessSignals goes positive.
	if ( !mExcessSignals++ )
	{
		mOwner = NULL;
		::pthread_cond_signal( &mSemaphore );
	}
	::pthread_mutex_unlock ( &mConditionLock );
} // Signal


sInt32 DSMutexSemaphore::Wait ( sInt32 milliSecs )
{
	// Unlike Mach's mutex_lock(), pthread_mutex_lock() is not a spin lock.
	// However, locking recursively on the mutex will cause a deadlock.
	// So, the pthread implementation mimics the cthread implementation:
	// the mutex locks access to the semaphore/condition.
	::pthread_mutex_lock ( &mConditionLock );

	// mOwner==NULL implies mExcessSignals==1
	if ( mOwner == NULL ) {
		mOwner = ::pthread_self();
	}
	else if ( mOwner != ::pthread_self() )
	{
		if ( milliSecs == kNever )
		{
			::pthread_mutex_unlock ( &mConditionLock );
			return semTimedOutErr;
		}
		if ( milliSecs == kForever )
		{
			while ( !mDestroying && ( mExcessSignals <= 0 ) )
				::pthread_cond_wait ( &mSemaphore, &mConditionLock );
		}
		else
		{
			struct timeval	tvNow;
			struct timespec	tsTimeout;

			// Timeout is passed as an absolute time!
			::gettimeofday ( &tvNow, NULL );
			TIMEVAL_TO_TIMESPEC ( &tvNow, &tsTimeout );
			tsTimeout.tv_sec += ( milliSecs / 1000 );
			tsTimeout.tv_nsec += ( ( milliSecs % 1000 ) * 1000000 );
			while ( !mDestroying && ( mExcessSignals <= 0 ) )
				if ( ETIMEDOUT == ::pthread_cond_timedwait ( &mSemaphore,
											&mConditionLock, &tsTimeout ) )
				{
					::pthread_mutex_unlock ( &mConditionLock );
					return semTimedOutErr;
				}
		}
		if ( mDestroying )
		{
			::pthread_mutex_unlock ( &mConditionLock );
			return semDestroyedErr;
		}
		mOwner = ::pthread_self();
	}

	// The executing thread is guaranteed to have the lock at this point.
	mExcessSignals--;
	::pthread_mutex_unlock ( &mConditionLock );

	return( semNoErr );

} // Wait
