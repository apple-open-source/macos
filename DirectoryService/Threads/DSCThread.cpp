/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header DSCThread
 * Defines abstract basic application thread object.
 */

#include "DSCThread.h"
#include "PrivateTypes.h"
#include <syslog.h>

sInt32 DSCThread::fStatThreadCount = 0;

// ----------------------------------------------------------------------------
//	* DSCThread()
//
// ----------------------------------------------------------------------------

DSCThread::DSCThread ( const OSType inThreadSig )
{
	fStatThreadCount++;
	fThreadSignature	= inThreadSig;

} // DSCThread


// ----------------------------------------------------------------------------
//	* ~DSCThread()
//
// ----------------------------------------------------------------------------

DSCThread::~DSCThread ()
{
	fStatThreadCount--;
} // ~DSCThread


// ----------------------------------------------------------------------------
//	* GetCurThreadRunState() (static)
//
// ----------------------------------------------------------------------------

uInt32 DSCThread::GetCurThreadRunState ( void )
{
	// All threads in our universe are DSCThread objects
	DSCThread *cur = (DSCThread *)DSLThread::GetCurrentThread();
	if ( cur == nil ) throw((sInt32)eMemoryError);

	return( cur->GetThreadRunState() );

} // GetCurThreadRunState


// ----------------------------------------------------------------------------
//	* Count() (static)
//
// ----------------------------------------------------------------------------

sInt32 DSCThread::Count ( void )
{
	return( fStatThreadCount );

} // Count


// ----------------------------------------------------------------------------
//	* GetID()
//
// ----------------------------------------------------------------------------

uInt32 DSCThread::GetID ( void ) const
{
	// member from LThread
	return( (uInt32)fThread );
} // GetID


// ----------------------------------------------------------------------------
//	* GetSignature()
//
// ----------------------------------------------------------------------------

OSType DSCThread::GetSignature ( void ) const
{
	return( fThreadSignature );
} // GetSignature


// ----------------------------------------------------------------------------
//	* Run()
//
//		- DSLThread::Run override
//
// ----------------------------------------------------------------------------

void *DSCThread::Run ( void )
{
	uInt32 result = 0;

	try {
		result = ThreadMain();
	}

	catch (...)
	{
		#ifdef DEBUG
		syslog(LOG_INFO,"DSCThread::Caught unknown error from ThreadMain.");
		#endif
	}

	try {
		// Give the thread a last chance to do clean up before
		// it gets destructed
		LastChance();
	}

	catch (...)
	{
		#ifdef DEBUG
		syslog(LOG_INFO,"DSCThread::Caught unknown error from LastChance.");
		#endif
	}

	return( (void *)result );

} // Run

// ----------------------------------------------------------------------------
//	* GetThreadRunState()
//
// ----------------------------------------------------------------------------

OSType DSCThread::GetThreadRunState ( void )
{
	return( fStateFlags );
} // GetThreadRunState


// ----------------------------------------------------------------------------
//	* SetThreadRunState()
//
// ----------------------------------------------------------------------------

void DSCThread::SetThreadRunState ( eRunState inState )
{
	fStateFlags = inState;
} // SetThreadRunState


// ----------------------------------------------------------------------------
//	* SetThreadRunStateFlag()
//
// ----------------------------------------------------------------------------

void DSCThread::SetThreadRunStateFlag ( eRunState inStateFlag )
{
	fStateFlags |= inStateFlag;
} // SetThreadRunStateFlag


// ----------------------------------------------------------------------------
//	* UnSetThreadRunStateFlag()
//
// ----------------------------------------------------------------------------

void DSCThread::UnSetThreadRunStateFlag ( eRunState inStateFlag )
{
	fStateFlags &= ~inStateFlag;
} // UnSetThreadRunStateFlag



