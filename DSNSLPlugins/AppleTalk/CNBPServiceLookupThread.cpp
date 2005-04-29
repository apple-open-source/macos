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
 *  @header CNBPServiceLookupThread
 */

#include "GenericNBPURL.h"
#include "CNBPPlugin.h"

#include "CNBPServiceLookupThread.h"
#include "CNSLDirNodeRep.h"
#include "CNSLResult.h"
#include "NSLDebugLog.h"

CNBPServiceLookupThread::CNBPServiceLookupThread( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep )
    : CNSLServiceLookupThread( parentPlugin, serviceType, nodeDirRep )
{
	DBGLOG( "CNBPServiceLookupThread::CNBPServiceLookupThread\n" );

    mServiceListRef = NULL;
	mNABuffer = NULL;
	mBuffer = NULL;
	mResultList = NULL;
	
	if ( mResultList )
		CFRelease( mResultList );
	mResultList = NULL;
}

CNBPServiceLookupThread::~CNBPServiceLookupThread()
{
	DBGLOG( "CNBPServiceLookupThread::~CNBPServiceLookupThread\n" );

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

    if ( mServiceListRef )
        ::CFRelease( mServiceListRef );
}

void* CNBPServiceLookupThread::Run( void )
{
	DBGLOG( "CNBPServiceLookupThread::Run\n" );
    char		serviceType[256] = {0};
    char		searchScope[256] = {0};
    
    // "searchScope" must be retrieved as NSLGetSystemEncoding() because AppleTalk.framework expects
    // it. Searching on UTF8 causes a zone mismatch.
    
    if ( AreWeCanceled() )
    {
        DBGLOG( "CNBPServiceLookupThread::Run, we were canceled before we even started\n" );
    }
    else if ( GetNodeName() && ::CFStringGetCString(GetNodeName(), searchScope, sizeof(searchScope), NSLGetSystemEncoding()) )
    {
        if ( GetServiceTypeRef() && ::CFStringGetCString(GetServiceTypeRef(), serviceType, sizeof(serviceType), kCFStringEncodingUTF8) )
        {
            OSStatus 			status = noErr;
            short				zoneStatus;
    
            zoneStatus = this->ConvertToLocalZoneIfThereAreNoZones( searchScope );

            // change "afp" url service type to "AFPServer" NBPType 
            if ( strcmp( serviceType, kAFPServerURLType ) == 0 )
                strcpy( serviceType, kAFPServerNBPType );

            status = this->DoLookupOnService( serviceType, searchScope );		// this will handle a search

            if ( status == -1 )
            {
                if ( zoneStatus == kMustSearchZoneNameAppleTalk )
                {
                    // if the neighborhood name is "AppleTalk" and there are zones, then we have to search the zone
                    // in case the network actually has a zone named, "AppleTalk".  In this case, we don't report
                    // errors because we shouldn't be surprised if we don't find a zone named "AppleTalk"
                    status = 0;
                }
                else
                {
                    // we could report a no-zone error here, but to be consistent with behavior on 9, we just stop the
                    // search and report noErr
                    status = 0;
                }
            }
        }
        else
            DBGLOG( "CNBPServiceLookupThread::Run, CFStringGetCString returned false on the serviceType" );
    }
    else
        DBGLOG( "CNBPServiceLookupThread::Run, CFStringGetCString returned false on the searchScope" );
    return NULL;
}

OSStatus
CNBPServiceLookupThread::DoLookupOnService( char* service, char *zone )
{
	OSStatus status = noErr;
	long actualCount;

	if ( service && zone )
	{
		long 			bufferSize = sizeof(NBPNameAndAddress) * kMaxServicesOnTryOne;
        
		// go get the service list
		mNABuffer = (NBPNameAndAddress *)malloc( bufferSize );
		actualCount = kMaxServicesOnTryOne;
		do
		{
			if ( mNABuffer && !AreWeCanceled() )
				status = NBPGetServerList( service, zone, mNABuffer, &actualCount );
			else
				status = eMemoryAllocError;
			
			// NBPGetServerList should return +1 if the maxCount was exceeded.
			// currently, it doesn't, so this code will never get used.
			if ( status > 0 )
			{
				bufferSize *= 2;
				mNABuffer = (NBPNameAndAddress *)realloc( mNABuffer, bufferSize );
				if ( mNABuffer == nil )
					status = eMemoryAllocError;
				actualCount = bufferSize / sizeof(NBPNameAndAddress);
			}	
		}
		while ( status > 0 && mNABuffer && !AreWeCanceled() );
	}
	
	// report
	if ( status == noErr )
	{
		char	urlStr[256]={0};
		UInt16	urlLen;
		char	nameUTF8Buf[256];
		char	zoneUTF8Buf[256];
		
		if ( !mResultList )
			mResultList = CFArrayCreateMutable( NULL, 0, NULL );
		
		// change "AFPServer" NBPType to "afp" url service type
		if ( strcmp( service, kAFPServerNBPType ) == 0 )
			strcpy( service, kAFPServerURLType );
		
		for ( long index = 0; index < actualCount && !AreWeCanceled(); index++ )
		{
			if ( mNABuffer[index].name[0] != 0 )
			{
				urlLen = 256;

				// we first need to convert the service name and zone into UTF8
				CFStringRef		zoneUTF8Ref = CFStringCreateWithCString( NULL, zone, NSLGetSystemEncoding() );
				CFStringRef		nameUTF8Ref = CFStringCreateWithCString( NULL, mNABuffer[index].name, NSLGetSystemEncoding() );
				
				if ( zoneUTF8Ref && nameUTF8Ref && CFStringGetCString( zoneUTF8Ref, zoneUTF8Buf, sizeof(zoneUTF8Buf), kCFStringEncodingUTF8 ) && CFStringGetCString( nameUTF8Ref, nameUTF8Buf, sizeof(nameUTF8Buf), kCFStringEncodingUTF8 ) )
					MakeGenericNBPURL( service, zoneUTF8Buf, nameUTF8Buf, urlStr, &urlLen );
				else
					urlLen = 0;

				urlStr[urlLen] = '\0';

				if ( zoneUTF8Ref )
					CFRelease( zoneUTF8Ref );

				if ( nameUTF8Ref )
					CFRelease( nameUTF8Ref );
					
				if ( urlLen && !AreWeCanceled() )
				{
                    CNSLResult* newResult = new CNSLResult();
                    char serviceType[256] = {0};
                    char *endPtr;
                    short servceTypeLen;
                    
                    endPtr = strstr( urlStr, ":/" );
                    if ( endPtr )
                    {
                        servceTypeLen = endPtr - urlStr;
                        strncpy( serviceType, urlStr, servceTypeLen );
                        serviceType[servceTypeLen] = '\0';
                    }
                    
                    DBGLOG( "CNBPServiceLookupThread::DoLookupOnService creating new result with type:%s url:%s\n",
                            serviceType, urlStr );
                    
                    newResult->SetURL( urlStr );
                    newResult->SetServiceType( serviceType );
                    
                    CFStringRef nameKeyRef = ::CFStringCreateWithCString( NULL, kDSNAttrRecordName, kCFStringEncodingUTF8 );
                    CFStringRef nameValueRef = ::CFStringCreateWithCString( NULL, mNABuffer[index].name, NSLGetSystemEncoding() );
                    
                    if ( nameKeyRef && nameValueRef )
                    {
                        newResult->AddAttribute( nameKeyRef, nameValueRef );		// this should be what is displayed
                    }
                    else
                        newResult->AddAttribute( kDSNAttrRecordName, mNABuffer[index].name );		// this should be what is displayed
	
					CFArrayAppendValue( mResultList, newResult );
                        
                    if ( nameKeyRef )
                        ::CFRelease( nameKeyRef );

                    if ( nameValueRef )
                        ::CFRelease( nameValueRef );
				}
			}
		}

		if ( !AreWeCanceled() && mResultList && CFArrayGetCount(mResultList) > 0 )
			GetNodeToSearch()->AddServices( mResultList );	
		else
		{
			CFIndex		numSearches = ::CFArrayGetCount( mResultList );
			CNSLResult*	result;
			
			for ( CFIndex i=numSearches-1; i>=0; i-- )
			{
				result = (CNSLResult*)::CFArrayGetValueAtIndex( mResultList, i );
				::CFArrayRemoveValueAtIndex( mResultList, i );
				delete result;
			}
		}
	}
	
	return status;	
}

void
CNBPServiceLookupThread::SetDefaultNeighborhoodNamePtr( const char *name )
{
    mDefaultNeighborhoodName = name;
}

//-----------------------------------------------------------------------------------------------------------------
//	ConvertToLocalZoneIfThereAreNoZones:
//
//	RETURNS: an enumerated type representing the status of the AppleTalk environment
//					kNotConverted = situation normal, not searching the "AppleTalk" zone
//					kConvertedToLocal = container is "AppleTalk" and there are no zones on the AppleTalk network
//					kMustSearchZoneNameAppleTalk = container is "AppleTalk" and there are zones.  We must check to see if there are
//													services in a real zone named "AppleTalk"
//	zoneName		 ->		the name of the zone for this search
//
// This function is only relevant if we are searching our container "AppleTalk" which we can't distinguish from a real zone.
// If the neighborhood is "AppleTalk" and there are no zones, then we need to do a search on "*" which is the local zone.
//-----------------------------------------------------------------------------------------------------------------

short
CNBPServiceLookupThread::ConvertToLocalZoneIfThereAreNoZones( char* zoneName )
{
	short result = kNotConverted;
	OSStatus status;

	long actualCount = 0;
	long bufferSize = sizeof(Str32) * kMaxZonesOnTryOne;
	
	mBuffer = (char *)malloc( bufferSize );
	if ( mBuffer == NULL )
		return eMemoryAllocError;

	// go get the local zone list
	if ( !AreWeCanceled() )
	{
		status = ZIPGetZoneList( LOOKUP_CURRENT, mBuffer, bufferSize, &actualCount );
		if ( status == -1 )
		{
			strcpy( zoneName, kNoZoneLabel );
			result = kConvertedToLocal;
		}
		else
			result = kMustSearchZoneNameAppleTalk;
	}

	return result;
};

#include "serverlist.h"

#include <NSLSemaphore.h>

// NOTE: dumping threads is a questionable practice that is being done to improve performance.
//
// The kMaxSemaphoreQueueSize constant should be high enough that no threads are dumped even
// if a client asks for a lot of services (http, https, ftp, news, nntp, radminx, afp, nfs, LaserWriter,
// Macintosh Manager == 10 services). Thread dumping is generally safe with the NSL UI dialog because
// the column view guarantees that the user is looking at the contents of the last set of searches. If a client
// implements a list-view, it could cause problems. I've chosen 15 as the threshold because
// normal cases will work.

#define kMaxSemaphoreQueueSize	15

static NSLSemaphore *sNSLSemaphoreB = NULL;
static short sWaitCount = 0;
static Boolean sStartDumpingThreads = false;

int CNBPServiceLookupThread::NBPGetServerList(
    char *service,
    char *curr_zone,
    struct NBPNameAndAddress *buffer,
    long *actualCount )
{
    at_nbptuple_t *tuple_buffer = NULL;
    at_entity_t entity;
    at_retry_t retry;
    int i;
    char len;
    long entryCount = 0;
    struct NBPNameAndAddress *currEntry = buffer;
    long maxCount;
    int error = 0;
    at_nbptuple_t *tuple;
    char *cptr;
    
    if ( sNSLSemaphoreB == NULL ) {
        sNSLSemaphoreB = new NSLSemaphore(1);
        if ( sNSLSemaphoreB == NULL ) {
            DBGLOG("sNSLSemaphoreB is NULL\n");
            return -1;
        }
    }
    
    maxCount = *actualCount;		// max number of entries that will fit in the buffer (ie the returned CString buffer)

    try
    {
        tuple_buffer = (at_nbptuple_t *) malloc( sizeof(at_nbptuple_t) * maxCount );
        if ( tuple_buffer == NULL ) {
            DBGLOG( "NBPGetServerList, out of memory\n" );
            throw(-1);
        }
        
        *actualCount = 0;
    
        if (error = nbp_make_entity(&entity, "=", service, curr_zone) != 0) {
            DBGLOG( "nbp_make_entity returned error %d\n", error);
            throw(error);
        }
    
        if ( sWaitCount > kMaxSemaphoreQueueSize )
        {
            sStartDumpingThreads = true;
            sNSLSemaphoreB->Wait();
            sStartDumpingThreads = false;
            sNSLSemaphoreB->Signal();
        }
        
        sWaitCount++;
        sNSLSemaphoreB->Wait();
        
        if ( !sStartDumpingThreads && !AreWeCanceled() )
        {
            // preflight to see if we're going to get data
            retry.interval = 1;
            retry.retries = 1;
            retry.backoff = 0x01;
            
            DBGLOG("NBP begin test run, service=%s\n", service);
            entryCount = nbp_lookup (&entity, &tuple_buffer[0], 1, &retry);
            DBGLOG("NBP end test run, entryCount=%ld\n", entryCount);
            
            // do the real lookup
            if ( entryCount > 0 && !sStartDumpingThreads && !AreWeCanceled() )
            {
                retry.interval = 1;
                retry.retries = 2;
                retry.backoff = 0x00;
                
                DBGLOG("NBP begin real run\n");
                entryCount = nbp_lookup (&entity, &tuple_buffer[0], maxCount, &retry);
                DBGLOG("NBP end real run\n");
            }
        }
        
        if (AreWeCanceled())
            DBGLOG("NBP canceled\n");
        
        sNSLSemaphoreB->Signal();
        sWaitCount--;
        
        if (entryCount < 0)
        {
            DBGLOG( "nbp_lookup returned error %ld\n", entryCount);
            throw (-1);
        }
        
        tuple = &tuple_buffer[0];
        for (i = 0; i < entryCount; i++, tuple++)
        {
            len = tuple->enu_entity.object.len;
            if ( len > 33 )
            {
                len = 33;
                DBGLOG( "NBPGetServerList: found len > 33\n" );
            }
            cptr = currEntry->name;

            if (strncpy ((char*) cptr, (char*)tuple->enu_entity.object.str, len) <= 0 )
            {
                throw (-1);
            }
            *(cptr + len) = 0x00;
            
            currEntry->atalkAddress.net =  tuple->enu_addr.net;
            currEntry->atalkAddress.node =  tuple->enu_addr.node;
            currEntry->atalkAddress.socket =  tuple->enu_addr.socket;
    
            currEntry++;
        }

        qsort (buffer, entryCount, sizeof (struct NBPNameAndAddress), my_strcmp2);

        *actualCount = entryCount;
    }
    catch( int inError )
    {
        error = inError;
    }
    
    free ( tuple_buffer );
    
    return(error);
}
