/*
 *  CBrowseNodeLookupThread.cpp
 *  DSBrowsePlugIn
 *
 *  Created by imlucid on Wed Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#include "CBrowsePlugin.h"
#include "CBrowseNodeLookupThread.h"

CBrowseNodeLookupThread::CBrowseNodeLookupThread( CNSLPlugin* parentPlugin )
    : CNSLNodeLookupThread( parentPlugin )
{
	DBGLOG( "CBrowseNodeLookupThread::CBrowseNodeLookupThread\n" );

	mBuffer = NULL;
}

CBrowseNodeLookupThread::~CBrowseNodeLookupThread()
{
	DBGLOG( "CBrowseNodeLookupThread::~CBrowseNodeLookupThread\n" );

	if ( mBuffer != NULL )
	{
		// pre-emptive-safe dispose
		char *buffer = mBuffer;
		mBuffer = NULL;
		free( buffer );
	}

}

void* CBrowseNodeLookupThread::Run( void )
{
	DBGLOG( "CBrowseNodeLookupThread::Run\n" );

	OSStatus	status = noErr;
    
    AddResult( "Browse Network" );
    AddResult( "Browse Local" );

    return NULL;
}


