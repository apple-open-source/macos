/*
 *  CBrowseServiceLookupThread.cpp
 *  DSBrowsePlugIn
 *
 *  Created by imlucid on Wed Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#include "CBrowsePlugin.h"

#include "CBrowseServiceLookupThread.h"
#include "CNSLDirNodeRep.h"
#include "CNSLResult.h"
#include "NSLDebugLog.h"

CBrowseServiceLookupThread::CBrowseServiceLookupThread( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep )
    : CNSLServiceLookupThread( parentPlugin, serviceType, nodeDirRep )
{
	DBGLOG( "CBrowseServiceLookupThread::CBrowseServiceLookupThread\n" );

    mServiceListRef = NULL;
	mBuffer = NULL;
}

CBrowseServiceLookupThread::~CBrowseServiceLookupThread()
{
	DBGLOG( "CBrowseServiceLookupThread::~CBrowseServiceLookupThread\n" );

	if ( mBuffer != NULL )
	{
		// pre-emptive-safe dispose
		char *buffer = mBuffer;
		mBuffer = NULL;
		free( buffer );
	}

    if ( mServiceListRef )
        ::CFRelease( mServiceListRef );
}

void* CBrowseServiceLookupThread::Run( void )
{
	DBGLOG( "CBrowseServiceLookupThread::Run\n" );

    return NULL;
}

OSStatus
CBrowseServiceLookupThread::DoLookupOnService( char* service, char *zone )
{
	OSStatus status = noErr;

	
	return status;	
}

void
CBrowseServiceLookupThread::SetDefaultNeighborhoodNamePtr( const char *name )
{
    mDefaultNeighborhoodName = name;
}

