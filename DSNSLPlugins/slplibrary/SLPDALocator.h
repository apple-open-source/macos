/*
 *  SLPDALocator.h
 *  NSLPlugins
 *
 *  Created by Kevin Arnold on Thu Oct 05 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _SLPDALocator_
#define _SLPDALocator_
#pragma once

#include "LThread.h"

#define CONFIG_DA_FIND	900

class SLPDALocator : public LThread
{
public:

	SLPDALocator								();
	~SLPDALocator								();
    static	SLPDALocator* TheSLPDAL				( void );
    
	virtual void*		Run						();
            Boolean		IsRunning				( void ) { return mIsRunning; }
            void		Start					( void );
            void		Cancel					( void ) { mCanceled = true; CFRunLoopStop(mRunLoopRef); }
            SLPInternalError	Initialize				( void* daadvert_callback, SLPHandle serverState );
            SLPInternalError	Initialize				( void );
            void		Kick					( void );
            void		KillSLPDALocator		( void );
			Boolean		SafeToUse				( void ) { return this == mSelfPtr; };
            
            Boolean		FinishedFirstLookup		( void ) { return !mInitialDALookupStillPending; };
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
    Boolean					mCanceled;
    Boolean					mInitialDALookupStillPending;
    Boolean					mDALookupHasntHadAChanceToFindADAYet;
    void*					mDACallback;
    SOCKET					mSocket;
    struct sockaddr_in		mSockAddr_in;
    long*					mQueuedDAsToLookup;
    long					mNumQueuedDAsToLookup;
    CFRunLoopRef			mRunLoopRef;
};

#endif







