/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 *  @header CNBPNodeLookupThread
 */

#include "CNBPPlugin.h"
#include "CNBPNodeLookupThread.h"

CNBPNodeLookupThread::CNBPNodeLookupThread( CNSLPlugin* parentPlugin )
    : CNSLNodeLookupThread( parentPlugin )
{
	DBGLOG( "CNBPNodeLookupThread::CNBPNodeLookupThread\n" );

	mBuffer = NULL;
	mNABuffer = NULL;
}

CNBPNodeLookupThread::~CNBPNodeLookupThread()
{
	DBGLOG( "CNBPNodeLookupThread::~CNBPNodeLookupThread\n" );

	if ( mBuffer != NULL )
	{
		// pre-emptive-safe dispose
		char *buffer = mBuffer;
		mBuffer = NULL;
		free( buffer );
	}

	if ( mNABuffer != NULL )
	{
		// pre-emptive-safe dispose
		NBPNameAndAddress *buffer = mNABuffer;
		mNABuffer = NULL;
		free( buffer );
	}
}

void* CNBPNodeLookupThread::Run( void )
{
	DBGLOG( "CNBPNodeLookupThread::Run\n" );
    
	OSStatus	status = noErr;
    long 		actualCount = 0;
    long 		bufferSize = sizeof(Str32) * kMaxZonesOnTryOne;
    char 		*curZonePtr;
    
    DoLocalZoneLookup();
    
    mBuffer = (char *)malloc( bufferSize );

    // go get the zone list
    do
    {
        if ( mBuffer )
        {
            status = ZIPGetZoneList( LOOKUP_ALL, mBuffer, bufferSize, &actualCount );
            if ( status == -1 )
            {
                // so we don't have zones
                // But we do want to publish our * zone...
                AddResult(kNoZoneLabel);
                status = 0;
                actualCount = 0;
            }
        }
        else
        {
            status = memFullErr;
        }
        
        // ZIPGetZoneList returns +1 if the maxCount was exceeded.
        if ( status > 0 )
        {
            bufferSize *= 2;
            mBuffer = (char *)realloc( mBuffer, bufferSize );
            if ( mBuffer == nil )
                status = memFullErr;
        }
    }
    while ( status > 0 && mBuffer );

    // send the results to the manager
    for (long index = 0; index < actualCount; index++)
    {
        curZonePtr = &mBuffer[sizeof (Str32) * index];

        if ( !curZonePtr )
        {
            DBGLOG("NBPLookup::StartNeighborhoodLookup got a null value from its mBuffer!\n");
            break;
        }
        
        if ( curZonePtr && *curZonePtr != '\0' )
        {
            DBGLOG("Going to call Manager notifier with new Zone: %s\n", curZonePtr);
            
            // The internal methods need some work. The c-str method for AddResult assumes
            // UTF8 which is wrong. The encoding is protocol-specific. However, I can't just fix the 
            // method because the CFStringRef version of AddResult feeds itself into the C-str version
            // which seems backwards. The only thing to do for now is to create the right string here.
            CFStringRef nodeRef = CFStringCreateWithCString( NULL, curZonePtr, NSLGetSystemEncoding() );
            if ( nodeRef )
            {
                AddResult( nodeRef );
                CFRelease( nodeRef );
            }
            else
                DBGLOG("CNBPNodeLookupThread::Run() could not make a CFString!\n");
        }
    }
	
    return NULL;
}

void CNBPNodeLookupThread::DoLocalZoneLookup( void )
{
    long 				actualCount = 0;
    char				zoneBuf[33 * 5];					// enough to support virtaul zones
    long 				bufferSize = sizeof(zoneBuf);		// just want to get one
    OSStatus			status = ZIPGetZoneList( LOOKUP_CURRENT, zoneBuf, bufferSize, &actualCount );
    
    // LOOKUP_CURRENT only checks "en0" 
    // if it fails, get all the local zones
    // this is the fallback because it is not guaranteed that the first
    // zone is really the user's default zone.
    if ( status == -1 )
    {
        // go get the local zone list
        status = ZIPGetZoneList( LOOKUP_LOCAL, zoneBuf, bufferSize, &actualCount );
    }
    
    if ( status == -1 )
    {
        ((CNBPPlugin*)GetParentPlugin())->SetLocalZone( (char*)kNoZoneLabel );
    }
    else
    {
        ((CNBPPlugin*)GetParentPlugin())->SetLocalZone( (char*)zoneBuf );
    }
    
    DBGLOG( "CNBPNodeLookupThread::DoLocalZoneLookup, setting our current Zone to %s\n", ((CNBPPlugin*)GetParentPlugin())->GetLocalZone() );
}
