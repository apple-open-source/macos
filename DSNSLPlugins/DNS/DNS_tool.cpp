/*
 *  DNS_tool.cpp
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Thu Mar 07 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>

#define DBGLOG(format, args...)	fprintf( stderr, format , ## args )

void PrintHelpInfo( void );
void DoDomainLookup( void );

static void BrowserCallBack(CFNetServiceBrowserRef browser, CFOptionFlags flags, CFTypeRef domainOrEntity, CFStreamError* error, void* info);
void CancelBrowse(CFRunLoopTimerRef timer, void *info);

void StartNodeLookups( Boolean onlyLookForRegistrationDomains );
void StartServiceLookup( CFStringRef domain, CFStringRef serviceType );
static void PerformRegister(char mode);
void CancelRegister(CFRunLoopTimerRef timer, void *info);
static void RegisterEntityCallBack(CFNetServiceRef theEntity, CFStreamError* error, void* info);

CFRunLoopRef		mRunLoopRef;
CFNetServiceRef 	mRegisteredEntity = NULL;

#define kMaxArgs		6		// [-b] [-r address] | [-a] [-s serviceType]
int main(int argc, char *argv[])
{
    OSStatus		status = 0;
    
//    PerformRegister( 'A' );
    
    mRunLoopRef = CFRunLoopGetCurrent();
    CFRunLoopTimerContext 		c = {0, NULL, NULL, NULL, NULL};
    CFRunLoopTimerRef 			timer = CFRunLoopTimerCreate(NULL, 1.0e20, 0, 0, 0, CancelBrowse, (CFRunLoopTimerContext*)&c);
    CFRunLoopAddTimer(mRunLoopRef, timer, kCFRunLoopDefaultMode);

//    DoDomainLookup();
    StartServiceLookup( CFSTR("local.arpa."), CFSTR("_afp._tcp.") );
    CFRunLoopRun();
/*    if ( argc > kMaxArgs || argc <= 1 )
    {
        PrintHelpInfo();
        return -1;
    }
    
    for ( int i=1; i<argc; i++ )		// skip past [0]
    {
        if ( strcmp(argv[i], "-d" ) == 0 )
        {
            SLPSetProperty( "com.apple.slp.logAll", "true" );
        }
    }
*/    
    return status; 
}

void PrintHelpInfo( void )
{
    fprintf( stderr,
            "Usage: slpda_netdetective [-l <da_address>] | [-a] [-s <serviceType>]\n"
            "  -l <da_address> is the address of a directory agent you wish to lookup\n"
            "  -a lookup all directory agents on the network\n"
            "  -s <serviceType is an optional parameter to query the DA(s) about registered services\n" );
}

void DoDomainLookup( void )
{
    mRunLoopRef = 0;
    mRunLoopRef = CFRunLoopGetCurrent();
    StartNodeLookups( true );
    
    StartNodeLookups( false );
}




CFStringRef CDNSNotifierCopyDesctriptionCallback( const void *item )
{
    return CFSTR("Blah");
}

Boolean CDNSNotifierEqualCallback( const void *item1, const void *item2 )
{
    return item1 == item2;
}

void Cancel( void )
{
    if ( mRunLoopRef )
        CFRunLoopStop( mRunLoopRef );
}

void* Run( void )
{
    // We just want to set up and do a CFRunLoop
    DBGLOG("Run called, just going to start up a CFRunLoop\n" );
    mRunLoopRef = CFRunLoopGetCurrent();
    
    while (1)
        CFRunLoopRun();
    
    DBGLOG("Run, CFRunLoop finished - exiting thread\n" );
    
    return NULL;
}

void StartNodeLookups( Boolean onlyLookForRegistrationDomains )
{
    while (!mRunLoopRef)
        DBGLOG("StartNodeLookups, waiting for mRunLoopRef\n");
        
    CFStreamError 				error = {(CFStreamErrorDomain)0, 0};
    CFNetServiceClientContext 	c = {0, NULL, NULL, NULL, NULL};
    CFRunLoopTimerRef 			timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 10, 0, 0, 0, CancelBrowse, (CFRunLoopTimerContext*)&c);
    CFNetServiceBrowserRef 		searchingBrowser = CFNetServiceBrowserCreate(NULL, BrowserCallBack, &c);
    
    CFNetServiceBrowserScheduleWithRunLoop(searchingBrowser, mRunLoopRef, kCFRunLoopDefaultMode);
    
    CFNetServiceBrowserSearchForDomains(searchingBrowser, onlyLookForRegistrationDomains, &error);
    
//    CFRunLoopAddTimer(mRunLoopRef, timer, kCFRunLoopDefaultMode);    
    
    if (error.error)
        DBGLOG( "Got an error starting domain search (%d, %ld)\n", error.domain, error.error);
}

void StartServiceLookup( CFStringRef domain, CFStringRef serviceType )
{
    DBGLOG("StartServiceLookup called\n" );
    if ( getenv( "NSLDEBUG" ) )
    {
        CFShow(domain);
        CFShow(serviceType);
    }
    
    while (!mRunLoopRef)
        DBGLOG("StartServiceLookup, waiting for mRunLoopRef\n");
        
    CFStreamError 				error = {(CFStreamErrorDomain)0, 0};
    CFNetServiceClientContext 	c = {0, NULL, NULL, NULL, NULL};
    CFRunLoopTimerRef 			timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 10, 0, 0, 0, CancelBrowse, (CFRunLoopTimerContext*)&c);
    CFNetServiceBrowserRef 		searchingBrowser = CFNetServiceBrowserCreate(NULL, BrowserCallBack, &c);

    DBGLOG("Run StartServiceLookup called, searchingBrowser:%ld, mRunLoopRef:%ld\n", searchingBrowser, mRunLoopRef );
    CFNetServiceBrowserScheduleWithRunLoop(searchingBrowser, mRunLoopRef, kCFRunLoopDefaultMode);
    
    DBGLOG("Run StartServiceLookup calling, CFNetServiceBrowserSearchForServices\n" );
    CFNetServiceBrowserSearchForServices(searchingBrowser, domain, serviceType, &error);
    DBGLOG("Run StartServiceLookup returning from CFNetServiceBrowserSearchForServices\n" );
    
//    CFRunLoopAddTimer(mRunLoopRef, timer, kCFRunLoopDefaultMode);
    
    if (error.error)
        DBGLOG( "Got an error starting service search (%d, %ld)\n", error.domain, error.error);
}

static void
PerformRegister(char mode) 
{
	CFStreamError error = {(CFStreamErrorDomain)0, 0};
	CFNetServiceRef entity;
	
	entity = CFNetServiceCreate(NULL, CFSTR("local.arpa."), CFSTR("MyType"), CFSTR("MyName"), 80);
	
	if (mode == 'A') {
		CFNetServiceClientContext c = {0, 0, NULL, NULL, NULL};
        CFRunLoopTimerRef timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + 10, 0, 0, 0, CancelRegister, (CFRunLoopTimerContext*)&c);
		CFNetServiceSetClient(entity, RegisterEntityCallBack, &c);
		CFNetServiceScheduleWithRunLoop(entity, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        mRegisteredEntity = entity;
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
	}
	
	if (CFNetServiceRegister(entity, &error))
		DBGLOG("CFNetServiceRegister returned TRUE!\n");
	else
		DBGLOG("CFNetServiceRegister returned FALSE (%d, %ld).\n", error.domain, error.error);
}

void CancelServiceLookup( )
{
    DBGLOG("CancelServiceLookup called\n" );
//    CFRunLoopStop(mRunLoopRef);
}

static void RegisterEntityCallBack(CFNetServiceRef theEntity, CFStreamError* error, void* info)
{

	switch ((int)info) 
    {
		case 0:
			// Register
			DBGLOG( "Register returned FALSE (%d, %ld).\n", error->domain, error->error);
//			CFRunLoopStop(CFRunLoopGetCurrent());
			break;
			
		case 1:
			// Resolve
			{
				CFStringRef description = CFNetServiceGetName(theEntity);
				CFStringRef type = CFNetServiceGetType(theEntity);
				CFStringRef domain = CFNetServiceGetDomain(theEntity);
				CFArrayRef addressing = CFNetServiceGetAddressing(theEntity);
				CFDataRef data = NULL;
                
                if ( addressing  )
                    data = (CFDataRef)CFArrayGetValueAtIndex(addressing, 0);

				DBGLOG( "Resolve returned (%d, %ld).\n", error->domain, error->error);
				
				if ( data )
                    DBGLOG(	"Address resolved to %s:%d\n",
                            inet_ntoa(((struct sockaddr_in*)CFDataGetBytePtr(data))->sin_addr),
                            ntohs(((struct sockaddr_in*)CFDataGetBytePtr(data))->sin_port));
			}
			
//			CFRunLoopStop(CFRunLoopGetCurrent());
			break;
			
		default:
			DBGLOG( "Received bad info pointer.\n");
			break;
	}

}


void CancelRegister(CFRunLoopTimerRef timer, void *info) 
{
    CFNetServiceCancel(mRegisteredEntity);
}



static void BrowserCallBack(CFNetServiceBrowserRef browser, CFOptionFlags flags, CFTypeRef domainOrEntity, CFStreamError* error, void* info) 
{
//    DNSBrowserThread*	browserThread = (DNSBrowserThread*)info;

    DBGLOG("*****BrowserCallBack called*****\n" );
    
    if (error->error) 
    {
        if ((error->domain == kCFStreamErrorDomainNetServices) && (error->error == kCFNetServicesErrorCancel))
        {
//            browserThread->CancelServiceLookup();
            CancelServiceLookup();
        }
        else
            DBGLOG( "Browser #%d received error (%d, %ld).\n", (int)info, error->domain, error->error);
    }
	else if (flags & kCFNetServiceFlagIsDomain) 
    {
        DBGLOG( "Browser received a %s%s domain notification.  Domain is:\n",
                (flags & kCFNetServiceFlagRemove) ? "remove" : "add",
                (flags & kCFNetServiceFlagIsRegistrationDomain) ? " registration" : "");
    }
    else
    {
        DBGLOG( "Browser received %s service.  Service info:\n",
                (flags & kCFNetServiceFlagRemove) ? "remove" : "add");
    }
    
    if ( (flags & kCFNetServiceFlagMoreComing) )
    {
        DBGLOG( "BrowserCallBack kCFNetServiceFlagMoreComing:\n" );
//        browserThread->CancelServiceLookup();
//        CancelServiceLookup();
    }
    else
        DBGLOG( "BrowserCallBack no more stuff coming\n" );
}


void CancelBrowse(CFRunLoopTimerRef timer, void *info) 
{
    CFNetServiceBrowserRef 		searchingBrowser = (CFNetServiceBrowserRef)info;
    
    DBGLOG("CancelBrowse called\n" );
//    CFNetServiceBrowserStopSearch(searchingBrowser, NULL);
}







