/*
 *  CNBPNodeLookupThread.cpp
 *  DSNBPPlugIn
 *
 *  Created by imlucid on Wed Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
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
                // so we don't have zones, who needs 'em?
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
            
            // sns 1/3/02: The internal methods need some work. The c-str method for AddResult assumes
            // UTF8 which is wrong. The encoding is protocol-specific. However, I can't just fix the 
            // method because the CFStringRef version of AddResult feeds itself into the C-str version
            // which seems backwards. The only thing to do for now is to create the right string here.
            CFStringRef nodeRef = CFStringCreateWithCString( NULL, curZonePtr, CFStringGetSystemEncoding() );
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
