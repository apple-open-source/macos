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
 *  @header CSLPNodeLookupThread
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
        wantMoreData = SLP_TRUE;					// still going
	}

    return wantMoreData;
}
