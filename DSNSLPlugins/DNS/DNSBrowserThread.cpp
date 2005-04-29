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
 *  @header DNSBrowserThread
 *  Currently, we are going to have this thread just monitor the local. domain (local) and keep track of things that are
 *  registered or not.
 */
 
#include "mDNSPlugin.h"
#include "DNSBrowserThread.h"
#include "CNSLTimingUtils.h"

const CFStringRef	kDNSBrowserThreadSAFE_CFSTR = CFSTR("DNSBrowserThread");
const CFStringRef	kCDNSNotifierCopyDesctriptionCallbackSAFE_CFSTR = CFSTR("CDNSNotifierCopyDesctriptionCallback");

static void BrowserCallBack(CFNetServiceBrowserRef browser, CFOptionFlags flags, CFTypeRef domainOrEntity, CFStreamError* error, void* info);
CFStringRef CopyBrowserDescription( const void* info );

CFStringRef CDNSNotifierCopyDesctriptionCallback( const void *item )
{
    return kCDNSNotifierCopyDesctriptionCallbackSAFE_CFSTR;
}

Boolean CDNSNotifierEqualCallback( const void *item1, const void *item2 )
{
    return item1 == item2;
}

void CancelBrowse(CFRunLoopTimerRef timer, void *info) 
{
    DNSBrowserThread* 		searchingBrowser = (DNSBrowserThread*)info;
    
    DBGLOG("CancelBrowse called\n" );
    searchingBrowser->Cancel();
}

DNSBrowserThread::DNSBrowserThread( mDNSPlugin* parentPlugin )
{
    mParentPlugin = parentPlugin;
    mRunLoopRef = 0;
    mCanceled = false;
	mDomainSearchingBrowserRef = NULL;
	mLocalDomainSearchingBrowserRef = NULL;
}

DNSBrowserThread::~DNSBrowserThread()
{
    mParentPlugin = NULL;
    mRunLoopRef = 0;

	if ( mDomainSearchingBrowserRef )
	{
		CFNetServiceBrowserUnscheduleFromRunLoop( mDomainSearchingBrowserRef, mRunLoopRef, kCFRunLoopDefaultMode );
		CFNetServiceBrowserInvalidate( mDomainSearchingBrowserRef );
		CFRelease( mDomainSearchingBrowserRef );
		mDomainSearchingBrowserRef = NULL;
	}

	if ( mLocalDomainSearchingBrowserRef )
	{
		CFNetServiceBrowserUnscheduleFromRunLoop( mLocalDomainSearchingBrowserRef, mRunLoopRef, kCFRunLoopDefaultMode );
		CFNetServiceBrowserInvalidate( mLocalDomainSearchingBrowserRef );
		CFRelease( mLocalDomainSearchingBrowserRef );
		mLocalDomainSearchingBrowserRef = NULL;
	}
}

void DNSBrowserThread::Cancel( void )
{
    mCanceled = true;
    
	if ( mDomainSearchingBrowserRef )
	{
		CFNetServiceBrowserUnscheduleFromRunLoop( mDomainSearchingBrowserRef, mRunLoopRef, kCFRunLoopDefaultMode );
		CFNetServiceBrowserInvalidate( mDomainSearchingBrowserRef );
		CFRelease( mDomainSearchingBrowserRef );
		mDomainSearchingBrowserRef = NULL;
	}

	if ( mLocalDomainSearchingBrowserRef )
	{
		CFNetServiceBrowserUnscheduleFromRunLoop( mLocalDomainSearchingBrowserRef, mRunLoopRef, kCFRunLoopDefaultMode );
		CFNetServiceBrowserInvalidate( mLocalDomainSearchingBrowserRef );
		CFRelease( mLocalDomainSearchingBrowserRef );
		mLocalDomainSearchingBrowserRef = NULL;
	}
}

void DNSBrowserThread::Initialize( CFRunLoopRef idleRunLoopRef )
{
	CFArrayCallBacks	callBack;
    
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = CDNSNotifierCopyDesctriptionCallback;
    callBack.equal = CDNSNotifierEqualCallback;

	mRunLoopRef = idleRunLoopRef;
}

sInt32 DNSBrowserThread::StartNodeLookups( Boolean onlyLookForRegistrationDomains )
{
    sInt32				siResult			= eDSNoErr;

    while (!mRunLoopRef)
    {
        DBGLOG("DNSBrowserThread::StartNodeLookups, waiting for mRunLoopRef\n");
        if ( mCanceled )
            return siResult;
            
        SmartSleep(100000);
    }
    
	if ( mLocalDomainSearchingBrowserRef && onlyLookForRegistrationDomains )
	{
		CFNetServiceBrowserUnscheduleFromRunLoop( mLocalDomainSearchingBrowserRef, mRunLoopRef, kCFRunLoopDefaultMode );
		CFNetServiceBrowserInvalidate( mLocalDomainSearchingBrowserRef );
		CFRelease( mLocalDomainSearchingBrowserRef );
		mLocalDomainSearchingBrowserRef = NULL;
	}
	else if ( mDomainSearchingBrowserRef && !onlyLookForRegistrationDomains )
	{
		CFNetServiceBrowserUnscheduleFromRunLoop( mDomainSearchingBrowserRef, mRunLoopRef, kCFRunLoopDefaultMode );
		CFNetServiceBrowserInvalidate( mDomainSearchingBrowserRef );
		CFRelease( mDomainSearchingBrowserRef );
		mDomainSearchingBrowserRef = NULL;
	}
		
    CFStreamError 				error = {(CFStreamErrorDomain)0, 0};
    CFNetServiceClientContext 	c = {0, this, NULL, NULL, CopyBrowserDescription};
    CFNetServiceBrowserRef 		searchingBrowser = CFNetServiceBrowserCreate(NULL, BrowserCallBack, &c);
    
	if ( searchingBrowser )
	{
		if ( onlyLookForRegistrationDomains )
			mLocalDomainSearchingBrowserRef = searchingBrowser;
		else
			mDomainSearchingBrowserRef = searchingBrowser;

		CFNetServiceBrowserScheduleWithRunLoop(searchingBrowser, mRunLoopRef, kCFRunLoopDefaultMode);
		
		CFNetServiceBrowserSearchForDomains(searchingBrowser, onlyLookForRegistrationDomains, &error);
		
		if (error.error)
		{
			DBGLOG( "mDNSServiceLookupThread::StartNodeLookups, CFNetServiceBrowserSearchForDomains returned (%d, %ld)\n", error.domain, error.error);
			siResult = error.error;
		}
		else
			DBGLOG( "mDNSServiceLookupThread::StartNodeLookups, CFNetServiceBrowserSearchForDomains returned status ok\n");		
    }
	
    return siResult;
}

sInt32 DNSBrowserThread::StartServiceLookup( CFStringRef domain, CFStringRef serviceType )
{
    sInt32				siResult			= eDSNoErr;

    DBGLOG("StartServiceLookup called\n" );
    if ( getenv( "NSLDEBUG" ) )
    {
        CFShow(domain);
        CFShow(serviceType);
    }
    
    while (!mRunLoopRef)
    {
        DBGLOG("StartServiceLookup, waiting for mRunLoopRef\n");
        SmartSleep(500000);
    }
    
    CFStreamError 				error = {(CFStreamErrorDomain)0, 0};
    CFNetServiceClientContext 	c = {0, this, NULL, NULL, CopyBrowserDescription};
    CFNetServiceBrowserRef 		searchingBrowser = CFNetServiceBrowserCreate(NULL, BrowserCallBack, &c);

    DBGLOG("Run StartServiceLookup called, searchingBrowser:%ld, mRunLoopRef:%ld\n", (UInt32)searchingBrowser, (UInt32)mRunLoopRef );
    CFNetServiceBrowserScheduleWithRunLoop(searchingBrowser, mRunLoopRef, kCFRunLoopDefaultMode);
    
    DBGLOG("Run StartServiceLookup calling, CFNetServiceBrowserSearchForServices\n" );
    CFNetServiceBrowserSearchForServices(searchingBrowser, domain, serviceType, &error);
    DBGLOG("Run StartServiceLookup returning from CFNetServiceBrowserSearchForServices\n" );
    
    if (error.error)
    {
        DBGLOG( "Got an error starting service search (%d, %ld)\n", error.domain, error.error);
        siResult = error.error;
    }
    
    return siResult;
}


static void BrowserCallBack(CFNetServiceBrowserRef browser, CFOptionFlags flags, CFTypeRef domainOrEntity, CFStreamError* error, void* info) 
{
    DNSBrowserThread*	browserThread = (DNSBrowserThread*)info;

    DBGLOG("BrowserCallBack called\n" );
    
    if (error->error) 
    {
        DBGLOG( "Browser #%d received error (%d, %ld).\n", (int)info, error->domain, error->error);
    }
	else if (flags & kCFNetServiceFlagIsDomain) 
    {
        if ( flags & kCFNetServiceFlagIsRegistrationDomain )
        {
            if ( CFGetTypeID(domainOrEntity) == CFStringGetTypeID() )
            {
                if ( CFStringHasSuffix( (CFStringRef)domainOrEntity, kDotSAFE_CFSTR ) )
                {
                    CFMutableStringRef	modifiedStringRef = CFStringCreateMutableCopy( NULL, 0, (CFStringRef)domainOrEntity );
                    CFStringDelete( modifiedStringRef, CFRangeMake(CFStringGetLength(modifiedStringRef)-1, 1) );
                    
                    if ( flags & kCFNetServiceFlagRemove )
                        browserThread->GetParentPlugin()->RemoveNode( modifiedStringRef );
                    else
                        browserThread->GetParentPlugin()->AddNode( modifiedStringRef, true );	// local node

                    CFRelease( modifiedStringRef );
                }
                else
                {
                    if ( flags & kCFNetServiceFlagRemove )
                        browserThread->GetParentPlugin()->RemoveNode( (CFStringRef)domainOrEntity );
                    else
                        browserThread->GetParentPlugin()->AddNode( (CFStringRef)domainOrEntity, true );	// local node                    
                }
            }
            else
                DBGLOG( "Received registration domain but it isn't a CFStringRef - CFGetTypeID:%ld\n", (UInt32)CFGetTypeID(domainOrEntity) );
        }
        else
        {
            if ( CFGetTypeID(domainOrEntity) == CFStringGetTypeID() )
            {
                if ( CFStringHasSuffix( (CFStringRef)domainOrEntity, kDotSAFE_CFSTR ) )
                {
                    CFMutableStringRef	modifiedStringRef = CFStringCreateMutableCopy( NULL, 0, (CFStringRef)domainOrEntity );
                    CFStringDelete( modifiedStringRef, CFRangeMake(CFStringGetLength(modifiedStringRef)-1, 1) );
                    
                    if ( flags & kCFNetServiceFlagRemove )
                        browserThread->GetParentPlugin()->RemoveNode( modifiedStringRef );
                    else
                        browserThread->GetParentPlugin()->AddNode( modifiedStringRef, false );
                        
                    CFRelease( modifiedStringRef );
                }
                else
                {
                    if ( flags & kCFNetServiceFlagRemove )
                        browserThread->GetParentPlugin()->RemoveNode( (CFStringRef)domainOrEntity );
                    else
                        browserThread->GetParentPlugin()->AddNode( (CFStringRef)domainOrEntity, false );                   
                }
            }
            else
                DBGLOG( "Received domain but it isn't a CFStringRef - CFGetTypeID:%ld\n", CFGetTypeID(domainOrEntity) );
        }
        
        DBGLOG( "Browser received %s%s domain.  Domain is:\n",
                (flags & kCFNetServiceFlagRemove) ? "remove" : "add",
                (flags & kCFNetServiceFlagIsRegistrationDomain) ? " registration" : "");

        if ( getenv("NSLDEBUG") )
            CFShow(domainOrEntity);
    }
    else
    {
        if ( IsNSLDebuggingEnabled() )
        {
            DBGLOG( "Browser received %s service.  Service info:\n",
                    (flags & kCFNetServiceFlagRemove) ? "remove" : "add");
        }
        
        if ( !(flags & kCFNetServiceFlagMoreComing) )
        {
            // this search is done as far as our clients are concerned...
        }
    }
}

CFStringRef CopyBrowserDescription( const void* info )
{
    DBGLOG( "CopyBrowserDescription called\n" );
    
    CFStringRef		description = CFStringCreateCopy( NULL, kDNSBrowserThreadSAFE_CFSTR );
    return description;
}
