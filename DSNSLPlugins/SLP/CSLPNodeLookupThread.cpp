/*
 *  CSLPNodeLookupThread.cpp
 *  DSSLPPlugIn
 *
 *  Created by imlucid on Wed Aug 15 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#include "CSLPNodeLookupThread.h"
#include "CommandLineUtilities.h"

SLPBoolean SLPScopeLookupNotifier( SLPHandle hSLP, const char* pcScope, SLPInternalError errCode, void* pvCookie );

CSLPNodeLookupThread::CSLPNodeLookupThread( CNSLPlugin* parentPlugin )
    : CNSLNodeLookupThread( parentPlugin )
{
	DBGLOG( "CSLPNodeLookupThread::CSLPNodeLookupThread\n" );
    mSLPRef = 0;
	mCanceled = false;
	mDoItAgain = false;
}

CSLPNodeLookupThread::~CSLPNodeLookupThread()
{
	DBGLOG( "CSLPNodeLookupThread::~CSLPNodeLookupThread\n" );
} 

void* CSLPNodeLookupThread::Run( void )
{
	DBGLOG( "CSLPNodeLookupThread::Run\n" );
    
    while ( !mCanceled )
	{
		SLPInternalError status = SLPOpen( "en", SLP_FALSE, &mSLPRef );
		
		if ( status )
		{
			DBGLOG( "CSLPNodeLookupThread::CSLPNodeLookupThread, SLPOpen returned (%d) %s\n", status, slperror(status) );
		}
		else if ( mSLPRef )
		{
			SLPInternalError	error = SLPFindScopesAsync( mSLPRef, SLPScopeLookupNotifier, this );
			
			if ( error )
				DBGLOG( "CSLPNodeLookupThread::CSLPNodeLookupThread, SLPFindScopesAsync returned (%d) %s\n", error, slperror(error) );
		}

		if( mSLPRef )
			SLPClose( mSLPRef );
		
		mSLPRef = NULL;
		
		if ( mDoItAgain )
			mDoItAgain = false;
		else
			break;
	}
	
    return NULL;
}

SLPBoolean SLPScopeLookupNotifier( SLPHandle hSLP, const char* pcScope, SLPInternalError errCode, void* pvCookie )
{
    CSLPNodeLookupThread*	lookupObj =  (CSLPNodeLookupThread*)pvCookie;
    SLPBoolean				wantMoreData = SLP_FALSE;

//#warning "This code is currently expecting single scopes only!  Need to support a scopelist!"
    if ( lookupObj && errCode == SLP_OK && pcScope && !lookupObj->IsCanceled() )
    {
        lookupObj->AddResult( pcScope );
        wantMoreData = SLP_TRUE;					// still going // KA - 4/21/00
    }

    return wantMoreData;
}
