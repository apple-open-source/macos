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
 *  @header SLPDALocator
 */

#ifndef _SLPDALocator_
#define _SLPDALocator_
#pragma once

#include <DirectoryServiceCore/DSLThread.h>

#define CONFIG_DA_FIND	1*60*60		// once per hour

class SLPDALocator /* : public DSLThread */
{
public:

	SLPDALocator								();
	~SLPDALocator								();
    static	SLPDALocator* TheSLPDAL				( void );
    
//	virtual void*		Run						();
			void		DoLookup				( void );
            Boolean		IsRunning				( void ) { return mIsRunning; }
			void		SetRunLoop				( CFRunLoopRef runLoop );
			
            void		Start					( void );
			void		DeleteSelfWhenFinished	( void ) { mDeleteSelfWhenFinished = true; }
//            void		Cancel					( void );
            SLPInternalError	Initialize				( void* daadvert_callback, SLPHandle serverState );
            SLPInternalError	Initialize				( void );
            void		Kick					( void );
			Boolean		SafeToUse				( void ) { return this == mSelfPtr; };
            
            Boolean		FinishedFirstLookup		( void ) { return !mInitialDALookupStillPending; };
			Boolean		IsLookupInProgress		( void ) { return mLookupInProgress; }
			void		LookupInProgress		( void ) { mLookupInProgress = true; }
            DATable*	GetDATable				( void );
            DATable*	GetDATableForRequester	( void );
            
            void		LocateAndAddDA			( long addrOfDA );
            void		AskDAForScopeSponserInfo( long addrOfDA );
            
            SLPHandle	GetServerState			( void ) { return mServerState; }

protected:			
	SLPDALocator*			mSelfPtr;
	SLPHandle				mServerState;
    DATable*				mDATable;
    Boolean					mDATableInitialized;
    Boolean					mIsRunning;
    Boolean					mLookupInProgress;
    Boolean					mTableReset;
    Boolean					mDeleteSelfWhenFinished;
    Boolean					mInitialDALookupStillPending;
    Boolean					mDALookupHasntHadAChanceToFindADAYet;
    void*					mDACallback;
    SOCKET					mSocket;
    struct sockaddr_in		mSockAddr_in;
    long*					mQueuedDAsToLookup;
    long					mNumQueuedDAsToLookup;
    CFRunLoopRef			mRunLoopRef;
	CFRunLoopTimerRef		mTimer;
};

#endif
