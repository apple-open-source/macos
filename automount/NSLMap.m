/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import "NSLMap.h"
#import "Controller.h"
#import "NSLVnode.h"
#import "AMString.h"
#import "automount.h"
#import "NSLUtil.h"
#import "log.h"
#import <stdio.h>
#import <string.h>
#import <unistd.h>
#import <syslog.h>
#import <arpa/inet.h>	/* for inet_ntoa */
#import <CoreServices/CoreServices.h>
#import <DNSServiceDiscovery/DNSServiceDiscovery.h>

#define RESOLVENEWBROWSERREPLIES 0
#define DNSTYPE "_afpovertcp._tcp."
#define DNSDOMAIN ""

#define INVALIDATE_ON_RENDEZVOUS_CHANGES 0
#define NOTIFICATION_REQUEST_PORT_NAME "com.apple.automount.notification_requests.%d"
#define MAXMESSAGEPORTNAMEATTEMPTS 25

extern CFRunLoopRef gMainRunLoop;

#if INVALIDATE_ON_RENDEZVOUS_CHANGES

void
MyHandleMachMessage ( CFMachPortRef port, void * msg, CFIndex size, void * info )
{
    DNSServiceDiscovery_handleReply(msg);
}

void ResolveReply (
                    struct sockaddr 	*interface,
                    struct sockaddr 	*address,
                    char 		*txtRecord,
                    int 		flags,
                    void		*context
                    )
{
    sys_msg(debug, LOG_DEBUG, "ResolveReply: address port = %d, family = %d, address = %s",
                                ((struct sockaddr_in *)address)->sin_port,
                                ((struct sockaddr_in *)address)->sin_family,
                                inet_ntoa(((struct in_addr)((struct sockaddr_in *)address)->sin_addr)));
    return;
}


void DNSBrowseReply (
                   DNSServiceBrowserReplyResultType 	resultType,		// One of DNSServiceBrowserReplyResultType
                   const char  	*replyName,
                   const char  	*replyType,
                   const char  	*replyDomain,
                   DNSServiceDiscoveryReplyFlags 	flags,			// DNS Service Discovery reply flags information
                   void	*context
                   )
{
    NSLMap *map = (NSLMap *)context;
    
    if (resultType == DNSServiceBrowserReplyAddInstance) {
#if RESOLVENEWBROWSERREPLIES
        // we have a find, let's resolve it
        CFMachPortRef           cfMachPort;
        CFMachPortContext       context;
        Boolean                 shouldFreeInfo;
        dns_service_discovery_ref 	dns_client;
        mach_port_t			port = NULL;
        CFRunLoopSourceRef		rls;
#endif
        sys_msg(debug, LOG_DEBUG, "DNSBrowseReply: Someone showed up with the name = %s, domain = %s, type = %s", replyName, replyDomain, replyType);
    
#if RESOLVENEWBROWSERREPLIES
        context.version                 = 1;
        context.info                    = 0;
        context.retain                  = NULL;
        context.release                 = NULL;
        context.copyDescription 	    = NULL;
    
        // start an enumerator on the local server
        dns_client = DNSServiceResolverResolve
            (
            replyName,
            replyType,
            replyDomain,
            ResolveReply,
            nil
            );
		
        if (dns_client) port = DNSServiceDiscoveryMachPort(dns_client);
    
        if (port) {
            cfMachPort = CFMachPortCreateWithPort ( kCFAllocatorDefault, port, ( CFMachPortCallBack ) MyHandleMachMessage,&context,&shouldFreeInfo );
    
            /* Create and add a run loop source for the port */
            rls = CFMachPortCreateRunLoopSource(NULL, cfMachPort, 0);
            CFRunLoopAddSource(gMainRunLoop, rls, kCFRunLoopDefaultMode);
            CFRelease(rls);
        } else {
            sys_msg(debug, LOG_DEBUG, " DNSBrowseReply: Could not obtain client port");
            return;
        }
#endif
    } else if (resultType == DNSServiceBrowserReplyRemoveInstance) {
        sys_msg(debug, LOG_DEBUG, "DNSBrowseReply: Someone went away with the name = %s, domain = %s, type = %s", replyName, replyDomain, replyType);
    } else {
        sys_msg(debug, LOG_DEBUG, "DNSBrowseReply: unknown resultType (%d)?!", resultType);
    };

    [((NSLVnode *)[map root]) invalidateRecursively:YES notifyFinder:YES];
    
    return;
}


void watch4mDNSNotifications(NSLMap *map)
{
    CFMachPortRef           cfMachPort;
    CFMachPortContext       context;
    Boolean                 shouldFreeInfo;
    mach_port_t			port;
    CFRunLoopSourceRef		rls;
    dns_service_discovery_ref 	browse_client;

    context.version                 = 1;
    context.info                    = 0;
    context.retain                  = NULL;
    context.release                 = NULL;
    context.copyDescription 	    = NULL;

    // start an enumerator on the local server
#if 0
	sys_msg(debug, LOG_DEBUG, "watch4mDNSNotifications: Creating DNS service browser; type = '%s', domain = '%s'...", DNSTYPE, DNSDOMAIN);
#endif
    browse_client = DNSServiceBrowserCreate
        (
        DNSTYPE,
        DNSDOMAIN,
        DNSBrowseReply,
        map
        );

    port = DNSServiceDiscoveryMachPort(browse_client);

    if (port) {
        cfMachPort = CFMachPortCreateWithPort ( kCFAllocatorDefault, port, ( CFMachPortCallBack ) MyHandleMachMessage,&context,&shouldFreeInfo );

        /* Create and add a run loop source for the port */
        rls = CFMachPortCreateRunLoopSource(NULL, cfMachPort, 0);
        CFRunLoopAddSource(gMainRunLoop, rls, kCFRunLoopDefaultMode);
        CFRelease(rls);
    } else {
        sys_msg(debug, LOG_DEBUG, "watch4mDNSNotifications: Could not obtain client port");
        return;
    }
}

#endif	/* INVALIDATE_ON_RENDEZVOUS_CHANGES */

CFDataRef NotificationRequestHandler(CFMessagePortRef local, SInt32 msgid, CFDataRef data, void *info)
{
	if (data == NULL) {
		sys_msg(debug, LOG_ERR, "NotificationRequestHandler received NULL data pointer?");
	} else {
		sys_msg(debug_nsl, LOG_DEBUG,
					"Got request for notification: msgid = %d, data = 0x%08lx, info = 0x%08lx...",
					msgid, data, info);
	};
	
	FNNotifyByPath(CFDataGetBytePtr(data), kFNDirectoryModifiedMessage, kNilOptions);
	
	return NULL;		/* No response necessary (or expected) */
}

void PrepareForNotificationRequests(NSLMap *map)
{
	CFMessagePortContext notificationContext = { 1, NULL, NULL, NULL, NULL };
	CFMessagePortRef localNotificationMessagePort = NULL;
	CFMessagePortRef remoteNotificationMessagePort = NULL;
    CFRunLoopSourceRef rls;
	char messagePortName[sizeof(NOTIFICATION_REQUEST_PORT_NAME) + 16];
	int n;
	CFStringRef messagePortNameString;
	
	notificationContext.info = map;
	for (n = 1; n <= MAXMESSAGEPORTNAMEATTEMPTS; ++n) {
		sprintf(messagePortName, NOTIFICATION_REQUEST_PORT_NAME, n);
		sys_msg(debug, LOG_DEBUG, "PrepareForNotificationRequests: trying notification port name %s...", messagePortName);
		messagePortNameString = CFStringCreateWithCString(kCFAllocatorDefault, messagePortName, kCFStringEncodingMacRoman);
		if (messagePortNameString == NULL) {
			sys_msg(debug, LOG_ERR, "Could not create local message port name; proceeding without Finder notification...");
			goto No_Finder_Notification;
		} else {
			localNotificationMessagePort = CFMessagePortCreateLocal(kCFAllocatorDefault,
																	messagePortNameString,
																	NotificationRequestHandler,
																	&notificationContext,
																	NULL);
			if (localNotificationMessagePort) break;
		};
		CFRelease(messagePortNameString);
	};
	if (localNotificationMessagePort == NULL) {
		sys_msg(debug, LOG_ERR, "Could not create local message port for notification requests; proceeding without Finder notification...");
		goto Std_Exit;
	};
	
	remoteNotificationMessagePort = CFMessagePortCreateRemote(kCFAllocatorDefault, messagePortNameString);
	if (remoteNotificationMessagePort == NULL) {
		sys_msg(debug, LOG_ERR, "Could not create remote message port for notification requests; proceeding without Finder notification...");
		goto No_Finder_Notification;
	};
	
	rls = CFMessagePortCreateRunLoopSource(kCFAllocatorDefault, localNotificationMessagePort, 0);
	if (rls == NULL) {
		sys_msg(debug, LOG_ERR, "Could not create run loop source for notification requests; proceeding without Finder notification...");
		goto No_Finder_Notification;
	};
	
	[map setNotificationMessagePort:remoteNotificationMessagePort];
	
	CFRunLoopAddSource(gMainRunLoop, rls, kCFRunLoopDefaultMode);
	CFRelease(rls);
	CFRelease(messagePortNameString);
	goto Std_Exit;

No_Finder_Notification:
	if (messagePortNameString) CFRelease(messagePortNameString);
	if (remoteNotificationMessagePort) CFRelease(remoteNotificationMessagePort);
	if (localNotificationMessagePort) CFRelease(localNotificationMessagePort);

Std_Exit:
	return;
}

@implementation NSLMap

- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds mountdirectory:(String *)mnt
{
	[super init];
	clientRefInvalid = YES;
	pthread_mutex_init(&searches.searchListLock, NULL);
    TAILQ_INIT(&searches.searchesInProgress);
    TAILQ_INIT(&searches.searchesCompleted);
	searches.searchCompleted = NO;
	notificationMessagePort = NULL;

	[self setName:ds];

	if (mnt)
	{
		[self setMountDirectory:mnt];
	} else {
		[self setMountDirectory:[controller mountDirectory]];
	};
	root = [[NSLVnode alloc] init];
	[root setMap:self];
	[(NSLVnode *)root setApparentName:dir];
	[root setMounted:NO];
	[root setServer:nil];
	[root setMode:00755];
	[(NSLVnode *)root setNSLObject:NSLMakeNewNeighborhood( "", NULL ) type:kNetworkNeighborhood];

	if (p != nil) [p addChild:root];
	[controller registerVnode:root];

#if INVALIDATE_ON_RENDEZVOUS_CHANGES
	// create a pthread which can run the CFRunLoop and register for mDNS notifications
	watch4mDNSNotifications(self);
#endif
	
	/* Set up to receive notification requests when map contents change: */
	PrepareForNotificationRequests(self);

	NSMap_list_record.mle_map = self;
	LIST_INSERT_HEAD(&gNSLMapList, &NSMap_list_record, mle_link);

	/*
	 * SIGHUP causes maps to be deallocated and reallocated, which loses
	 * all symlinks that have since been created.  So, just ignore SIGHUP.
	 * Note: do not use -nsl maps with other maps in the same automount
	 * process.
	 */
	signal(SIGHUP, SIG_IGN);

	return self;
}

- (void)timeout
{
	/* Do nothing */
}



- (void)dealloc {
	/*
	 * Don't bother trying to close the NSL connection at shutdown: likely as not,
	 * DirectoryServices itself is shutting down and the call would just hang...
	 */
    if ((!clientRefInvalid) && (!gTerminating)) NSLCloseNavigationAPI( [self getNSLClientRef] );
	
	[super dealloc];
}

- (NSLClientRef)getNSLClientRef
{
	if (clientRefInvalid) {
		OSStatus returnStatus;
		NSLClientRef newNSLClientRef;
		
		// Open NSL and get ourselves a valid client ref:
		returnStatus = NSLXOpenNavigationAPI( kNSLXDefaultProtocols | kNSLXEnableOpenEndedSearches, &newNSLClientRef );
		if (returnStatus) {
			sys_msg(debug, LOG_ERR, "Cannot open NSL navigation API (error = %ld)", returnStatus);
			return 0;
		};
		[self setNSLClientRef:newNSLClientRef];
	};
	
	return clientRef;
}

- (void)setNSLClientRef:(NSLClientRef)newNSLClientRef
{
	clientRef = newNSLClientRef;
	clientRefInvalid = NO;
}

- (SearchList *)searchList
{
	return &searches;
}

- (void)recordSearchInProgress:(SearchContext *)searchContext
{
	pthread_mutex_lock(&searches.searchListLock);
	TAILQ_INSERT_TAIL(&searches.searchesInProgress, searchContext, sc_link);
	pthread_mutex_unlock(&searches.searchListLock);
}

- (void)deleteSearchInProgress:(SearchContext *)searchContext
{
	pthread_mutex_lock(&searches.searchListLock);
	TAILQ_REMOVE(&searches.searchesInProgress, searchContext, sc_link);
	pthread_mutex_unlock(&searches.searchListLock);
}

- (void)processNewSearchResults;
{
	SearchContext *searchContext;
	BOOL directoryChanged;
	
	pthread_mutex_lock(&searches.searchListLock);
search_loop:
	TAILQ_FOREACH(searchContext, &searches.searchesInProgress, sc_link) {
		if (!TAILQ_EMPTY(&searchContext->results->contentsFound) &&
			((searchContext->searchFlags & kSearchResultsBeingProcessed) == 0)) {
			searchContext->searchFlags |= kSearchResultsBeingProcessed;
			pthread_mutex_unlock(&searches.searchListLock);
			directoryChanged = [searchContext->parent_vnode processSearchResults:searchContext];
			pthread_mutex_lock(&searches.searchListLock);
			searchContext->searchFlags &= ~kSearchResultsBeingProcessed;
			goto search_loop;
		};
	
		if (searchContext->searchState == kSearchComplete) {
			sys_msg(debug, LOG_DEBUG, "NSLVnode.processSearchResults: searchState is kSearchComplete.");
			TAILQ_REMOVE(&searches.searchesInProgress, searchContext, sc_link);
			TAILQ_INSERT_TAIL(&searches.searchesCompleted, searchContext, sc_link);
			searches.searchCompleted = TRUE;
		};
	};
	pthread_mutex_unlock(&searches.searchListLock);
}

- (void)cleanupSearchContextList
{
	pthread_mutex_lock(&searches.searchListLock);
	if (searches.searchCompleted) {
		while (!TAILQ_EMPTY(&searches.searchesCompleted)) {
			SearchContextPtr targetSearchContext = TAILQ_FIRST(&searches.searchesCompleted);
			if (!gTerminating) (void)NSLDeleteRequest(targetSearchContext->searchRef);
			TAILQ_REMOVE(&searches.searchesCompleted, targetSearchContext, sc_link);
		};
		searches.searchCompleted = FALSE;
	}
	pthread_mutex_unlock(&searches.searchListLock);
}

- (void)triggerDeferredNotifications
{
	SearchContext *searchContext;
	
	pthread_mutex_lock(&searches.searchListLock);
	TAILQ_FOREACH(searchContext, &searches.searchesInProgress, sc_link) {
		[searchContext->parent_vnode triggerDeferredNotifications:searchContext];

		if ((searchContext->searchState == kSearchComplete) &&
			((searchContext->searchFlags & kSearchResultsBeingProcessed) == 0)) {
			sys_msg(debug, LOG_DEBUG, "NSLVnode.processSearchResults: searchState is kSearchComplete.");
			TAILQ_REMOVE(&searches.searchesInProgress, searchContext, sc_link);
			TAILQ_INSERT_TAIL(&searches.searchesCompleted, searchContext, sc_link);
			searches.searchCompleted = TRUE;
		};
	};
	pthread_mutex_unlock(&searches.searchListLock);
}

- (CFMessagePortRef)notificationMessagePort
{
	return notificationMessagePort;
}

- (void)setNotificationMessagePort:(CFMessagePortRef)newNotificationMessagePort
{
	notificationMessagePort = newNotificationMessagePort;
}

@end
