/*
 *  mDNSNodeLookupThread.cpp
 *  DSSMBPlugIn
 *
 *  Created by imlucid on Wed Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <CoreServices/CoreServices.h>
#include "mDNSNodeLookupThread.h"

static void BrowserCallBack(CFNetServiceBrowserRef browser, CFOptionFlags flags, CFTypeRef domainOrEntity, CFStreamError* error, void* info);
void CancelNodeBrowse(CFRunLoopTimerRef timer, void *info);

mDNSNodeLookupThread::mDNSNodeLookupThread( CNSLPlugin* parentPlugin, const char* parentDomain )
    : CNSLNodeLookupThread( parentPlugin )
{
	DBGLOG( "mDNSNodeLookupThread::mDNSNodeLookupThread\n" );
    
    mParentDomain = NULL;
    
    if ( parentDomain )
    {
        mParentDomain = (char*)malloc(strlen(parentDomain) + 1);
        strcpy( mParentDomain, parentDomain );
    }
    
    mRunLoopRef = 0;
    mSearchingBrowser = 0;
}

mDNSNodeLookupThread::~mDNSNodeLookupThread()
{
	DBGLOG( "mDNSNodeLookupThread::~mDNSNodeLookupThread\n" );
    
    if ( mSearchingBrowser )
        CFNetServiceBrowserInvalidate( mSearchingBrowser );
        
    if ( mParentDomain )
        free( mParentDomain );
    
    mParentDomain = NULL;
}

void* mDNSNodeLookupThread::Run( void )
{
	DBGLOG( "mDNSNodeLookupThread::Run\n" );
    
    mRunLoopRef = CFRunLoopGetCurrent();

    CFStreamError 				error = {kCFStreamErrorDomainMacOSStatus, 0};
    CFNetServiceClientContext 	c = {0, this, NULL, NULL, NULL};
    CFRunLoopTimerRef 			timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 10, 0, 0, 0, CancelNodeBrowse, (CFRunLoopTimerContext*)&c);
    mSearchingBrowser = CFNetServiceBrowserCreate(NULL, BrowserCallBack, &c);
    
    CFNetServiceBrowserScheduleWithRunLoop(mSearchingBrowser, mRunLoopRef, kCFRunLoopDefaultMode);
    
    CFNetServiceBrowserSearchForDomains(mSearchingBrowser, true, &error);
    CFNetServiceBrowserSearchForDomains(mSearchingBrowser, false, &error);
    
    CFRunLoopAddTimer(mRunLoopRef, timer, kCFRunLoopDefaultMode);    
    
    if (error.error)
        DBGLOG( "Got an error starting domain search (%d, %ld)\n", error.domain, error.error);

    CFRunLoopRun();

    return NULL;
}

void mDNSNodeLookupThread::Cancel( void )
{
    if ( mRunLoopRef )
    {
        CFStreamError 				error = {kCFStreamErrorDomainMacOSStatus, 0};
        
        CFNetServiceBrowserStopSearch( GetBrowserRef(), &error );

        if (error.error) 
        	DBGLOG( "mDNSNodeLookupThread::Cancel got an error: %ld from CFNetServiceBrowserStopSearch\n", error.error );

        CFRunLoopStop( mRunLoopRef );
    }
}

static void BrowserCallBack(CFNetServiceBrowserRef browser, CFOptionFlags flags, CFTypeRef domainOrEntity, CFStreamError* error, void* info) 
{
    mDNSNodeLookupThread*		lookupThread = (mDNSNodeLookupThread*)info;
    
    if (error->error) 
    {
        if ((error->domain == kCFStreamErrorDomainNetServices) && (error->error == kCFNetServicesErrorCancel))
            lookupThread->Cancel();
        else
            DBGLOG( "Browser #%d received error (%d, %ld).\n", (int)info, error->domain, error->error);
    }
	else if (flags & kCFNetServiceFlagIsDomain || flags & kCFNetServiceFlagIsRegistrationDomain) 
    {
        DBGLOG( "Browser received %s%s domain.  Domain is:\n",
                (flags & kCFNetServiceFlagRemove) ? "remove" : "add",
                (flags & kCFNetServiceFlagIsRegistrationDomain) ? " registration" : "");
    }
    else
    {
        DBGLOG( "Browser received %s service.  Service info:\n",
                (flags & kCFNetServiceFlagRemove) ? "remove" : "add");
    }
    
    if ( !(flags & kCFNetServiceFlagMoreComing) )
        lookupThread->Cancel();
}


void CancelNodeBrowse(CFRunLoopTimerRef timer, void *info) 
{
    mDNSNodeLookupThread*		lookupThread = (mDNSNodeLookupThread*)info;
    CFNetServiceBrowserRef 		searchingBrowser = lookupThread->GetBrowserRef();
    
    CFNetServiceBrowserStopSearch(searchingBrowser, NULL);
}

