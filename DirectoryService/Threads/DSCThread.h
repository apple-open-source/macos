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
 * Defines the base application thread.
 */

#ifndef __DSCThread_h__
#define __DSCThread_h__	1

#include "DSLThread.h"
#include "PrivateTypes.h"

const sInt32 kEventPeriod			= 60;		// ticks
const sInt32 kLongPeriod			= 10;		// ticks
const sInt32 kShortPeriod			=  5;		// ticks

const uInt32 kMinPerHour			= 60;
const uInt32 kSecondsPerMin			= 60;
const uInt32 kMilliSecsPerSec		= 1000;
const uInt32 kMicroSecsPerSec		= 1000*1000;
const uInt32 kNanoSecPerSec			= 1000*1000*1000;

const uInt32 kBadValue				= 0xffffffff;
void ** const kThreadIgnoreResult	= NULL;


class DSCThread : public DSLThread
{
public:

	enum eSignature {
		// Normal Threads
		kTSUndefinedThread			= '----',
		kTSMainThread				= 'main',
		kTSAppThread				= 'appt', 
		kTSHandlerThread			= 'hndl',
		kTSInternalHandlerThread	= 'ihdl',
		kTSCheckpwHandlerThread		= 'cpht',
		kTSPlugInHndlrThread		= 'pihn',
		kTSListenerThread			= 'lstn',
		kTSLauncherThread			= 'lnch',
		kTSNodeRegisterThread		= 'ndrg',
		kTSTCPListenerThread		= 'tcpl',
		kTSTCPConnectionThread		= 'tcpc',
		kTSTCPHandlerThread			= 'tcph'
	};

	enum eRunState {
		kThreadRun	= 0x00000000,
		kThreadStop	= 0x00000001,
		kThreadWait	= 0x00000002
	};

						DSCThread				( const OSType inThreadSig = 0 );
	virtual			   ~DSCThread				( void );

public:
	// Class methods (static)
	static	uInt32		GetCurThreadRunState	( void );
	static	sInt32		Count					( void );

	// Class methods
	virtual uInt32		GetID					( void ) const;
	virtual OSType		GetSignature			( void ) const;

protected:
	virtual void*		Run						( void );
	virtual long		ThreadMain				( void ) = 0;	// pure virtual
	virtual void		LastChance				( void ) { }

	virtual	uInt32		GetThreadRunState		( void );
	virtual	void		SetThreadRunState		( eRunState inState );

	virtual	void		SetThreadRunStateFlag	( eRunState inStateFlag );
	virtual	void		UnSetThreadRunStateFlag	( eRunState inStateFlag );

	static	sInt32		fStatThreadCount;

protected:
	FourCharCode		fThreadSignature;

private:
	uInt32				fStateFlags;
};



#endif // __DSCThread_h__
