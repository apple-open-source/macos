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
 *  @header mDNSServiceLookupThread
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "CNSLHeaders.h"
#include "mDNSServiceLookupThread.h"
#include "mDNSPlugin.h"
#include "CNSLTimingUtils.h"

const CFStringRef	kColonSlashSlashSAFE_CFSTR = CFSTR("://");
const CFStringRef	kDNSServiceLookupThreadSAFE_CFSTR = CFSTR("dsAttrTypeStandard:ServiceType");

static void ServiceBrowserCallBack(CFNetServiceBrowserRef browser, CFOptionFlags flags, CFTypeRef domainOrEntity, CFStreamError* error, void* info) ;
CFStringRef CopyServiceBrowserDescription( const void* info );
void CancelSearchBrowse(CFRunLoopTimerRef timer, void *info);

mDNSServiceLookupThread::mDNSServiceLookupThread( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep )
    : CNSLServiceLookupThread( parentPlugin, serviceType, nodeDirRep )
{
	DBGLOG( "mDNSServiceLookupThread::mDNSServiceLookupThread: 0x%x\n", (int)this);
	mSearchingBrowserRef = NULL;
	mRunLoopRef = NULL;
	mLastResult = 0;
}

mDNSServiceLookupThread::~mDNSServiceLookupThread()
{
	DBGLOG( "mDNSServiceLookupThread::~mDNSServiceLookupThread: 0x%x\n", (int)this );

	if ( mSearchingBrowserRef )
	{
		CFNetServiceBrowserInvalidate( mSearchingBrowserRef );
		CFRelease(mSearchingBrowserRef);
	}

	mSearchingBrowserRef = NULL;
	
	mRunLoopRef = NULL;
}

void mDNSServiceLookupThread::Cancel( void )
{
	DBGLOG( "mDNSServiceLookupThread::Cancel: 0x%x\n", (int)this );
	if ( mSearchingBrowserRef )
	{
		CFNetServiceBrowserInvalidate( mSearchingBrowserRef );
		CFRelease(mSearchingBrowserRef);
	}
	
	mSearchingBrowserRef = NULL;
	
	CFRunLoopStop( mRunLoopRef );
}

void* mDNSServiceLookupThread::Run( void )
{
	DBGLOG( "mDNSServiceLookupThread::Run: 0x%x\n", (int)this );
    
    mRunLoopRef = CFRunLoopGetCurrent();
    
    if ( AreWeCanceled() )
    {
        DBGLOG( "CDNSServiceLookupThread::Run, we were canceled before we even started: 0x%x\n", (int)this );
    }
    else 
    {
        sInt32	status = StartServiceLookup( GetNodeName(), GetServiceTypeRef() );
    
        if ( status )
            DBGLOG( "mDNSServiceLookupThread::Run, mDNSGetListOfServicesWithCallback returned error: %ld: 0x%x\n", (int)this, status );
        else
            CFRunLoopRun();
    }
    
    DBGLOG( "mDNSServiceLookupThread::Run, finished: 0x%x\n", (int)this );
    
    return NULL;
}

sInt32 mDNSServiceLookupThread::StartServiceLookup( CFStringRef domainRef, CFStringRef serviceType )
{
    sInt32		status = 0;
    
    DBGLOG("mDNSServiceLookupThread::StartServiceLookup called: 0x%x\n", (int)this );
    if ( getenv( "NSLDEBUG" ) )
    {
        CFShow(domainRef);
        CFShow(serviceType);
    }
    
    while (!mRunLoopRef)
    {
        DBGLOG("mDNSServiceLookupThread::StartServiceLookup, waiting for mRunLoopRef: 0x%x\n", (int)this);
        
        if ( mCanceled )
            return status;
            
        SmartSleep(1*USEC_PER_SEC);
    }
        
    CFStringRef		modDomainRef = NULL;
    CFStringRef		modTypeRef = NULL;
    
    if ( CFStringCompare( domainRef, kEmptySAFE_CFSTR, 0 ) != kCFCompareEqualTo && !CFStringHasSuffix( domainRef, kDotSAFE_CFSTR ) )
    {
        // we need to pass fully qualified domains (i.e. local. not local)
        modDomainRef = CFStringCreateMutableCopy( NULL, 0, domainRef );
        CFStringAppendCString( (CFMutableStringRef)modDomainRef, ".", kCFStringEncodingUTF8 );
    }
        
    if ( !CFStringHasSuffix( serviceType, kDotUnderscoreTCPSAFE_CFSTR ) )
    {
        // need to convert this to the appropriate DNS style.  I.E. _afp._tcp. not afp
        modTypeRef = CFStringCreateMutableCopy( NULL, 0, kUnderscoreSAFE_CFSTR );
        CFStringAppend( (CFMutableStringRef)modTypeRef, serviceType );
        CFStringAppend( (CFMutableStringRef)modTypeRef, kDotUnderscoreTCPSAFE_CFSTR );
    }
    
    CFStreamError 				error = {(CFStreamErrorDomain)0, 0};
    CFNetServiceClientContext 	c = {0, this, NULL, NULL, CopyServiceBrowserDescription};
	
	CFNetServiceBrowserRef 		searchingBrowser = CFNetServiceBrowserCreate(NULL, ServiceBrowserCallBack, &c);

    DBGLOG("Run mDNSServiceLookupThread::StartServiceLookup called, searchingBrowser:%ld, mRunLoopRef:%ld\n", (UInt32)searchingBrowser, (UInt32)mRunLoopRef );
	
	if ( searchingBrowser )
	{
		mSearchingBrowserRef = searchingBrowser;
		CFNetServiceBrowserScheduleWithRunLoop(searchingBrowser, mRunLoopRef, kCFRunLoopDefaultMode);
		
		DBGLOG("Run mDNSServiceLookupThread::StartServiceLookup calling, CFNetServiceBrowserSearchForServices: 0x%x\n", (int)this );
		CFNetServiceBrowserSearchForServices(searchingBrowser, (modDomainRef)?modDomainRef:domainRef, (modTypeRef)?modTypeRef:serviceType, &error);
	}
	
    if ( modDomainRef )
        CFRelease( modDomainRef );
    
	if ( modTypeRef )
		CFRelease( modTypeRef );
		
		CFRunLoopTimerRef 			timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + kMaxTimeToWaitBetweenServices, 0, 0, 0, CancelSearchBrowse, (CFRunLoopTimerContext*)&c);
	CFRunLoopAddTimer(mRunLoopRef, timer, kCFRunLoopDefaultMode);
	
	CFRelease( timer );
	
    if (error.error)
    {
        DBGLOG( "Got an error starting service search (%d, %ld)\n", error.domain, error.error);
        status = error.error;
    }
    else
		DBGLOG("Run mDNSServiceLookupThread::StartServiceLookup calling, CFNetServiceBrowserSearchForServices: 0x%x\n", (int)this );
	
    return status;
}

void mDNSServiceLookupThread::AddResult( CNSLResult* newResult )
{
	DBGLOG( "mDNSServiceLookupThread::AddResult: 0x%x\n", (int)this );
	
	CNSLServiceLookupThread::AddResult( newResult );
		
	mLastResult = CFAbsoluteTimeGetCurrent();
}

Boolean mDNSServiceLookupThread::IsSearchTimedOut( void )
{
    if ( mLastResult + kMaxTimeToWaitBetweenServices < CFAbsoluteTimeGetCurrent() )
        return true;
    else
        return false;
}

static void ServiceBrowserCallBack(CFNetServiceBrowserRef browser, CFOptionFlags flags, CFTypeRef domainOrEntity, CFStreamError* error, void* info) 
{
    mDNSServiceLookupThread*	browserThread = (mDNSServiceLookupThread*)info;

    DBGLOG("ServiceBrowserCallBack called, flags:%d\n", flags );
    
	do {
		if (error->error) 
		{
			if ((error->domain == kCFStreamErrorDomainNetServices) && (error->error == kCFNetServicesErrorCancel) && browserThread && !browserThread->AreWeCanceled())
			{
				DBGLOG("ServiceBrowserCallBack kCFNetServicesErrorCancel\n" );
				browserThread->Cancel();
			}
			else
				DBGLOG( "Browser #%d received error (%d, %ld).\n", (int)info, error->domain, error->error);
		}
		else
		{
			DBGLOG( "Browser received %s service.  Service info:\n", (flags & kCFNetServiceFlagRemove) ? "remove" : "add");
			
			CFMutableDictionaryRef			foundService = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			
			if ( foundService )
			{
				CFStringRef			nameRef = CFNetServiceGetName((CFNetServiceRef)domainOrEntity);
				CFStringRef			domainRef = CFNetServiceGetDomain((CFNetServiceRef)domainOrEntity);
				CFMutableStringRef	modifiedDomainRef = NULL;
				
				if ( !nameRef || !domainRef )
				{
					CFRelease( foundService );
					break;
				}
			
                if ( CFStringHasSuffix( (CFStringRef)domainRef, kDotSAFE_CFSTR ) )
                {
                    modifiedDomainRef = CFStringCreateMutableCopy( NULL, 0, (CFStringRef)domainRef );
                    CFStringDelete( modifiedDomainRef, CFRangeMake(CFStringGetLength(modifiedDomainRef)-1, 1) );

					CFDictionaryAddValue( foundService, kDS1AttrLocationSAFE_CFSTR, modifiedDomainRef );
                }
				else
					CFDictionaryAddValue( foundService, kDS1AttrLocationSAFE_CFSTR, domainRef );

				CFDictionaryAddValue( foundService, kDSNAttrRecordNameSAFE_CFSTR, nameRef );
		
				CFMutableStringRef		serviceType = CFStringCreateMutableCopy( NULL, 0, CFNetServiceGetType((CFNetServiceRef)domainOrEntity) );
				
				if ( !serviceType )
				{
					CFRelease( foundService );
					break;
				}
				
				if ( CFStringHasSuffix( serviceType, kDotUnderscoreTCPSAFE_CFSTR ) )
				{
					CFRange					charsToDelete = CFStringFind( serviceType, kDotUnderscoreTCPSAFE_CFSTR, 0 );
					CFStringDelete( serviceType, charsToDelete );
				}
				
				if ( CFStringHasPrefix( serviceType, kUnderscoreSAFE_CFSTR ) )
					CFStringDelete( serviceType, CFRangeMake(0,1) );
		
				CFStringRef		convertedServiceTypeRef = browserThread->GetParentPlugin()->CreateRecTypeFromNativeType( serviceType );
		
				if ( !convertedServiceTypeRef )
				{
					CFRelease( serviceType );
					CFRelease( foundService );
					break;
				}
	
				CFDictionaryAddValue( foundService, kDS1AttrServiceTypeSAFE_CFSTR, convertedServiceTypeRef );
		
				CFStringRef				dnsURL = NULL;
				CFStringRef				hostNameRef = CreateRFC1034HostLabelFromUTF8Name( nameRef );
				
				if ( hostNameRef )
				{				
					if ( CFStringCompare( convertedServiceTypeRef, kDSStdRecordTypeAFPServerSAFE_CFSTR, 0 ) == kCFCompareEqualTo )
					{
						// special case afpovertcp to be afp
						dnsURL = CFStringCreateWithFormat( NULL, NULL, CFSTR("afp://%@.%@"), hostNameRef, modifiedDomainRef );
					}
					else
						dnsURL = CFStringCreateWithFormat( NULL, NULL, CFSTR("%@://%@.%@"), serviceType, hostNameRef, modifiedDomainRef );
				}
				
				if ( dnsURL )
				{
					CFDictionaryAddValue( foundService, kDSNAttrURLSAFE_CFSTR, dnsURL );
				}
				
				if ( modifiedDomainRef )
					CFRelease( modifiedDomainRef );
				modifiedDomainRef = NULL;
				
				if ( convertedServiceTypeRef )
					CFRelease( convertedServiceTypeRef );
				convertedServiceTypeRef = NULL;
				
				if ( serviceType )
					CFRelease( serviceType );
				serviceType = NULL;

				if ( dnsURL )
					CFRelease( dnsURL );
				dnsURL = NULL;
				
				if ( hostNameRef )
					CFRelease( hostNameRef );
				hostNameRef = NULL;
				
				CFStringRef		protocolSpecificInfo = CFNetServiceGetProtocolSpecificInformation( (CFNetServiceRef)domainOrEntity );
				
				if ( protocolSpecificInfo )
				{
					if ( getenv("NSLDEBUG") )
					{
						DBGLOG( "ServiceBrowserCallBack, retrieved protocolSpecificInfo\n" );
						CFShow( protocolSpecificInfo );
					}
					
					CFDictionaryAddValue( foundService, kDNSTextRecordSAFE_CFSTR, protocolSpecificInfo );			
					CFDictionaryAddValue( foundService, kDS1AttrCommentSAFE_CFSTR, protocolSpecificInfo );			
				}
				else
					DBGLOG( "ServiceBrowserCallBack, no protocolSpecificInfo\n" );
		
				// now we will try and get the address/port information if it is available.  If it isn't we'll just leave it out and
				// let the client resolve this at a later time if they wish.
				CFArrayRef		addressResults = CFNetServiceGetAddressing( (CFNetServiceRef)domainOrEntity );
				
		/*        if ( !addressResults )
				{
					CFStreamError	resolveError;
			
					if ( CFNetServiceResolve( (CFNetServiceRef)domainOrEntity, &resolveError ) )
						DBGLOG("ServiceBrowserCallBack, CFNetServiceResolve returned true\n");
					else
						DBGLOG("ServiceBrowserCallBack, CFNetServiceResolve returned false with error (%d,%ld)\n", resolveError.domain, resolveError.error);
			
				}
		*/        
				if ( addressResults )
				{
					CFIndex 	numAddressResults = CFArrayGetCount(addressResults);
			
					for ( CFIndex i=0; i<numAddressResults; i++ )
					{
						CFDataRef			sockAddrRef = (CFDataRef)CFArrayGetValueAtIndex( addressResults, i );
						struct	sockaddr	sockHdr;
						
						if ( sockAddrRef )
						{
							CFDataGetBytes( sockAddrRef, CFRangeMake(0, sizeof(sockHdr)), (UInt8*)&sockHdr );
							// now get the appropriate data...
							switch ( sockHdr.sa_family )
							{
								case AF_INET:
								{
									struct sockaddr_in		address;
									char					addressString[16];
									char					addressPort[7];
			
									CFDataGetBytes( sockAddrRef, CFRangeMake(0, sizeof(address)), (UInt8*)&address );
		
									const u_char* p = (const u_char*)&address.sin_addr;
									snprintf(addressString, sizeof(addressString), "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
									
									if (ntohs( address.sin_port) != 0)
									{
										snprintf(addressPort, sizeof(addressPort), ".%d", ntohs(address.sin_port));
									}
									
									DBGLOG( "Address resolves to %s\n", addressString );
									DBGLOG( "Port resolves to %s\n", addressPort );
								}
								break;
								
								case AF_INET6:
			{
									DBGLOG("ServiceBrowserCallBack, received IPv6 type that we don't handle!\n");
								}
								break;
								
								default:
									DBGLOG("ServiceBrowserCallBack, received unkown sockaddr family! (%d)\n", sockHdr.sa_family);
								break;
							}
						}
						else
							DBGLOG("ServiceBrowserCallBack, we couldn't get the addressing info from the addressResults!\n");
					}
				}
				else
					DBGLOG("ServiceBrowserCallBack, there wasn't any addressing information available without resolution\n");
	
				CNSLResult* newResult = new CNSLResult( foundService );
				
//#define LOG_CF_NOTIFY
#ifdef LOG_CF_NOTIFY
#warning "LOG_CF_NOTIFY is defined DO NOT SUBMIT"
{
	char		newItemName[1024] = {0,};
	CFStringRef	newItemNameRef = NULL;
	char		newItemType[256] = {0,};
	CFStringRef	newItemTypeRef = NULL;
	
	newItemNameRef = (CFStringRef)CFDictionaryGetValue( foundService, kDSNAttrRecordNameSAFE_CFSTR );
	
	newItemTypeRef = (CFStringRef)CFDictionaryGetValue( foundService, kDS1AttrServiceTypeSAFE_CFSTR );

	if ( newItemNameRef )
		CFStringGetCString( newItemNameRef, newItemName, sizeof(newItemName), kCFStringEncodingUTF8 );

	if ( newItemTypeRef )
		CFStringGetCString( newItemTypeRef, newItemType, sizeof(newItemType), kCFStringEncodingUTF8 );

	syslog( LOG_ALERT, "[NSL] Bonjour DS plugin (0x%x) got a %s notification for result [%s - %s]\n", browserThread, (flags & kCFNetServiceFlagRemove) ? "delete" : "add", newItemName, newItemType );

}
#endif
				if ( !browserThread->AreWeCanceled() )
				{
					browserThread->AddResult( newResult );
				}
#ifdef LOG_CF_NOTIFY
else
	syslog( LOG_ALERT, "[NSL] Bonjour DS plugin ignoring notification as the browserThread was canceled!\n" );
#endif
			}
			else
				DBGLOG("ServiceBrowserCallBack, CFDictionaryCreateMutable failed!\n");

			if ( foundService )
				CFRelease( foundService );
		}
		
		if ( flags & kCFNetServiceFlagMoreComing )
		{
			DBGLOG("ServiceBrowserCallBack, kCFNetServiceFlagMoreComing is set\n");
		}
		else
		{
			DBGLOG("ServiceBrowserCallBack, kCFNetServiceFlagMoreComing is not set\n");
	
			if ( browserThread && !browserThread->AreWeCanceled() && browserThread->IsSearchTimedOut() )
			{
				DBGLOG("ServiceBrowserCallBack, calling browserThread->Cancel()\n");
				browserThread->Cancel();
			}
		}
	} while (false);
}

CFStringRef CopyServiceBrowserDescription( const void* info )
{
    DBGLOG( "CopyServiceBrowserDescription called\n" );
    
    CFStringRef		description = CFStringCreateCopy( NULL, kDNSServiceLookupThreadSAFE_CFSTR );
    return description;
}

void CancelSearchBrowse(CFRunLoopTimerRef timer, void *info) 
{
    mDNSServiceLookupThread*	browserThread = (mDNSServiceLookupThread*)info;
    
    DBGLOG("CancelSearchBrowse called\n" );
    if ( !browserThread->AreWeCanceled() )
        browserThread->Cancel();
}

