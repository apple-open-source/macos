/******************************************************************************
	File:		NSLSemaphore.cpp

	Contains:	Implementation of the NSLSemaphore (lock) base class.
				IMPORTANT:
				* This is an independently derived implementation,
				* OPTIMIZED FOR MAC OS X'S POSIX THREADS,
				* of Metrowerks' PowerPlant Thread classes, which likely
				* makes the class and method names in this header file also
				* copyright Metrowerks.

	Version:	$Revision: 1.2 $

	Copyright:	© 1998-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:
		DRI:				Chris Jalbert
		Other Contact:		Michael Dasenbrock
		Technology:			RAdmin, AppleShare X; Directory Services, Mac OS X

	Writers:
		(cpj)	Chris Jalbert

	Change History (most recent first):

		 <6>	 09/16/99	cpj		Rolled in Andrea's timeout fixes.
									Stripped out cthread (Hera) version.
		 <5>	 09/04/99	cpj		Added namespace qualifiers.
		 <4>	 06/26/99	cpj		Ported to Beaker. Code compiles.
		 <1>	 07/13/98	cpj		Initial checkin.
		 <0>	 02/10/98	cpj		Initial creation.
 *****************************************************************************/


// ANSI / POSIX Headers
#include <sys/time.h>	// for struct timespec and gettimeofday()

// Project Headers
#include "NSLSemaphore.h"

#if 0
#if !(TARGET_OS_MAC && TARGET_API_MAC_OSX)
#error "This is implementation is only for Mac OS X!"
#endif	/* !(TARGET_OS_UNIX && TARGET_API_MAC_OSX) */

#ifndef _POSIX_THREADS
#error "pthread implementation not available!"
#endif	/* _POSIX_THREADS */
#endif

//using namespace PowerPlant ;


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
