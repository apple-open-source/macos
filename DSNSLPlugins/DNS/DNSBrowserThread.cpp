/*
 *  DNSBrowserThread.cpp
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Tue Feb 26 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

/*
    Currently, we are going to have this thread just monitor the local. domain (local) and keep track of things that are
    registered or not.
*/
#include "mDNSPlugin.h"
#include "DNSBrowserThread.h"
static void BrowserCallBack(CFNetServiceBrowserRef browser, CFOptionFlags flags, CFTypeRef domainOrEntity, CFStreamError* error, void* info);
CFStringRef CopyBrowserDescription( void* info );

CFStringRef CDNSNotifierCopyDesctriptionCallback( const void *item )
{
    return CFSTR("Blah");
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
//    : DSLThread()
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
		CFNetServiceBrowserInvalidate( mDomainSearchingBrowserRef );
		CFRelease( mDomainSearchingBrowserRef );
		mDomainSearchingBrowserRef = NULL;
	}

	if ( mLocalDomainSearchingBrowserRef )
	{
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
		CFNetServiceBrowserInvalidate( mDomainSearchingBrowserRef );
		CFRelease( mDomainSearchingBrowserRef );
		mDomainSearchingBrowserRef = NULL;
	}

	if ( mLocalDomainSearchingBrowserRef )
	{
		CFNetServiceBrowserInvalidate( mLocalDomainSearchingBrowserRef );
		CFRelease( mLocalDomainSearchingBrowserRef );
		mLocalDomainSearchingBrowserRef = NULL;
	}

/*    if ( mRunLoopRef )
        CFRunLoopStop( mRunLoopRef );
	mRunLoopRef = NULL;
*/
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

/*
void* DNSBrowserThread::Resume( void )
{
    // We just want to set up and do a CFRunLoop
    DBGLOG("DNSBrowserThread::Run called, just going to start up a CFRunLoop\n" );
    mRunLoopRef = CFRunLoopGetCurrent();
    
    CFRunLoopTimerContext 	c = {0, this, NULL, NULL, NULL};
    CFRunLoopTimerRef 			timer = CFRunLoopTimerCreate(NULL, 1.0e20, 0, 0, 0, CancelBrowse, (CFRunLoopTimerContext*)&c);
    CFRunLoopAddTimer(mRunLoopRef, timer, kCFRunLoopDefaultMode);
    
    CFRunLoopRun();
    
    DBGLOG("DNSBrowserThread::Run, CFRunLoop finished - exiting thread\n" );
    
    return NULL;
}
*/
sInt32 DNSBrowserThread::StartNodeLookups( Boolean onlyLookForRegistrationDomains )
{
    sInt32				siResult			= eDSNoErr;

    while (!mRunLoopRef)
    {
        DBGLOG("DNSBrowserThread::StartNodeLookups, waiting for mRunLoopRef\n");
        if ( mCanceled )
            return siResult;
            
        usleep(100000);
    }
    
	if ( mLocalDomainSearchingBrowserRef && onlyLookForRegistrationDomains )
	{
		CFNetServiceBrowserInvalidate( mLocalDomainSearchingBrowserRef );
		CFRelease( mLocalDomainSearchingBrowserRef );
		mLocalDomainSearchingBrowserRef = NULL;
	}
	else if ( mDomainSearchingBrowserRef && !onlyLookForRegistrationDomains )
	{
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
			DBGLOG( "mDNSServiceLookupThread::StartServiceLookup, CFNetServiceBrowserSearchForDomains returned (%d, %ld)\n", error.domain, error.error);
			siResult = error.error;
		}
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
        usleep(500000);
    }
        
    CFStreamError 				error = {(CFStreamErrorDomain)0, 0};
    CFNetServiceClientContext 	c = {0, this, NULL, NULL, CopyBrowserDescription};
//    CFRunLoopTimerRef 			timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 10, 0, 0, 0, CancelBrowse, (CFRunLoopTimerContext*)&c);
    CFNetServiceBrowserRef 		searchingBrowser = CFNetServiceBrowserCreate(NULL, BrowserCallBack, &c);
    
//    CFArrayAppendValue( mListOfSearches, searchingBrowser );

    DBGLOG("Run StartServiceLookup called, searchingBrowser:%ld, mRunLoopRef:%ld\n", (UInt32)searchingBrowser, (UInt32)mRunLoopRef );
    CFNetServiceBrowserScheduleWithRunLoop(searchingBrowser, mRunLoopRef, kCFRunLoopDefaultMode);
    
    DBGLOG("Run StartServiceLookup calling, CFNetServiceBrowserSearchForServices\n" );
    CFNetServiceBrowserSearchForServices(searchingBrowser, domain, serviceType, &error);
    DBGLOG("Run StartServiceLookup returning from CFNetServiceBrowserSearchForServices\n" );
    
//    CFRunLoopAddTimer(mRunLoopRef, timer, kCFRunLoopDefaultMode);
    
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
                if ( CFStringHasSuffix( (CFStringRef)domainOrEntity, CFSTR(".") ) )
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
                if ( CFStringHasSuffix( (CFStringRef)domainOrEntity, CFSTR(".") ) )
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
        if ( getenv("NSLDEBUG") )
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

CFStringRef CopyBrowserDescription( void* info )
{
//    DNSBrowserThread*	browserThread = (DNSBrowserThread*)info;
    DBGLOG( "CopyBrowserDescription called\n" );
    
    CFStringRef		description = CFStringCreateCopy( NULL, CFSTR("DNSBrowserThread") );
    return description;
}


