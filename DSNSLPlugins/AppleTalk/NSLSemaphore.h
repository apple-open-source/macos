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
 *  @header NSLSemaphore
 */
 
#ifndef _NSLSemaphore_H_
#define _NSLSemaphore_H_

/**** Required system headers. ****/
// ANSI / POSIX headers
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

// Universal / CoreFoundation Headers
#include <CoreFoundation/CoreFoundation.h>

/******************************************************************************
	==>  NSLSemaphore class definition  <==
******************************************************************************/
class NSLSemaphore
{
public:
	/**** Typedefs, enums and constants. ****/
	enum eWaitTime {
		kForever = -1,
		kNever = 0
	} ;
	enum eErr {
		semDestroyedErr = 28020,
		semTimedOutErr,
		semNotOwnerErr,
		semAlreadyResetErr,
		semOtherErr
	} ;

	/**** Instance method protoypes. ****/
	// ctor and dtor.
			NSLSemaphore			( SInt32 initialCount = 0 ) ;
	virtual	~NSLSemaphore			( void ) ;

	// New methods.
	virtual void		Signal	( void ) ;
	virtual OSStatus	Wait	( SInt32 milliSecs = kForever ) ;

protected:
	/**** Instance variables. ****/
	pthread_mutex_t		mConditionLock ;
	pthread_cond_t		mSemaphore ;
	SInt32				mExcessSignals ;
	bool				mDestroying ;

private:
	/**** Invalid methods and undefined operations. ****/
	// Copy constructor
							NSLSemaphore	( const NSLSemaphore & ) ;
	// Assignment
			NSLSemaphore &	operator=	( const NSLSemaphore & ) ;
} ;

#endif	/* _NSLSemaphore_H_ */
