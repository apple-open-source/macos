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
#import "vfs_sysctl.h"
#import <fcntl.h>
#import <stdio.h>
#import <string.h>
#import <unistd.h>
#import <syslog.h>
#import <arpa/inet.h>	/* for inet_ntoa */
#import <CoreServices/CoreServices.h>

#define NOTIFICATION_REQUEST_PORT_NAME "com.apple.automount.notification_requests.%d"
#define MAXMESSAGEPORTNAMEATTEMPTS 25

#define DEBUG_CHECK_QUEUEING 0

extern CFRunLoopRef gMainRunLoop;



CFDataRef NotificationRequestHandler(CFMessagePortRef local, SInt32 msgid, CFDataRef data, void *info)
{
	const char *targetPath;
	int fd;
	
	if (data == NULL) {
		sys_msg(debug, LOG_ERR, "NotificationRequestHandler received NULL data pointer?");
	} else {
		sys_msg(debug_nsl, LOG_DEBUG,
					"Got request for notification: msgid = %d, data = 0x%08lx, info = 0x%08lx...",
					msgid, data, info);
	};
	
	sys_msg(debug_nsl, LOG_DEBUG, "FNNotifyByPath('%s', ...", CFDataGetBytePtr(data) ? (char *)CFDataGetBytePtr(data) : "[all]");
	targetPath = CFDataGetBytePtr(data);
	
	/* Before generating the notificaiton call open(2) on the target here
	   solely to prompt the NFS client on this machine to flush it attribute
	   cache to make sure when the Finder asks for the current mod. date
	   it's guaranteed an up-to-date answer:
	 */
	fd = open(targetPath, 0, 0);
	if (fd >= 0) close(fd);
	
	FNNotifyByPath(targetPath, kFNDirectoryModifiedMessage, kNilOptions);
	
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

- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds mountdirectory:(String *)mnt mountedon:(String *)mnton
{
	[super initWithParent:p directory:dir from:ds mountdirectory:mnt withRootVnodeClass:[NSLVnode class]];
	[(NSLVnode *)[self root] setApparentName:dir];
	[[self root] setMode:00755];

	clientRefInvalid = YES;
	pthread_mutex_init(&searches.searchListLock, NULL);
    TAILQ_INIT(&searches.searchesInProgress);
    TAILQ_INIT(&searches.searchesCompleted);
	searches.searchCompleted = NO;
	notificationMessagePort = NULL;

	/*
	 * If this is /Network, set the flag that causes us to censor
	 * host and neighborhood names that correspond to "well-known"
	 * names in /Network such as /Network/Library.
	 */
	if (mnton != nil && strcmp([mnton value], "/Network") == 0)
		[(NSLVnode *)[self root] setCensorContents:YES];

	[(NSLVnode *)[self root] setNSLObject:NSLMakeNewNeighborhood( "", NULL ) type:kNetworkNeighborhood];
	
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

- (unsigned int)didAutoMount
{
	unsigned int result;
	
	result = [super didAutoMount];
	if (result == 0) {
		[[self root] deferContentGeneration];
	};
	
	return result;
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
	
#if DEBUG_CHECK_QUEUEING
	if ( ! TAILQ_EMPTY(&searches.searchesInProgress) ) {
		sys_msg(debug, LOG_DEBUG, "[NSLMap dealloc]: searchesInProgress is not empty ('%s')?", [[TAILQ_FIRST(&searches.searchesInProgress)->parent_vnode name] value]);
	}
	if ( ! TAILQ_EMPTY(&searches.searchesCompleted) ) {
		sys_msg(debug, LOG_DEBUG, "[NSLMap dealloc]: searchesCompleted is not empty ('%s')?", [[TAILQ_FIRST(&searches.searchesCompleted)->parent_vnode name] value]);
	}
#endif
	
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

#if DEBUG_CHECK_QUEUEING

struct searchlist_head {
	struct _searchContext *tqh_first;
	struct _searchContext **tqh_last;
};

static void CheckSearchListConsistency(struct searchlist_head *theListHead, char *listName)
{
	struct _searchContext **previousContextLink = &theListHead->tqh_first;
	struct _searchContext *currentContext = theListHead->tqh_first;
	
	while (currentContext) {
		if ( currentContext->sc_link.tqe_prev != previousContextLink ) {
			sys_msg(debug, LOG_ERR, "Hey?! 'prev' link of search context 0x%08lx for neighborhood '%s' on %s is not expected %08lx?",
								currentContext, [[currentContext->parent_vnode name] value], listName, previousContextLink);
		}
		
		if ( currentContext->sc_link.tqe_next == currentContext ) {
			sys_msg(debug, LOG_ERR, "Hey?! 'next' link of search context 0x%08lx for neighborhood '%s' on %s is circular?",
								currentContext, [[currentContext->parent_vnode name] value], listName);
		}
		
		if ( (currentContext->sc_link.tqe_next == NULL) && (theListHead->tqh_last != &(currentContext->sc_link.tqe_next)) ) {
			sys_msg(debug, LOG_ERR, "Hey?! search context 0x%08lx for neighborhood '%s' is last on %s but 'last' points to 0x%08lx?",
								currentContext, [[currentContext->parent_vnode name] value],
								listName,
								theListHead->tqh_last);
		}
		
		previousContextLink = &currentContext->sc_link.tqe_next;
		currentContext = currentContext->sc_link.tqe_next;
	}
	
	if ( theListHead->tqh_last != previousContextLink) {
		sys_msg(debug, LOG_ERR, "Hey?! 'last' pointer of %s search list is 0x%08lx but should be %08lx?",
							listName, theListHead->tqh_last, previousContextLink);
	}
}
#endif

- (void)recordSearchInProgress:(SearchContext *)searchContext
{
	pthread_mutex_lock(&searches.searchListLock);
	
#if DEBUG_CHECK_QUEUEING
	CheckSearchListConsistency((struct searchlist_head *)&searches.searchesInProgress, "searchesInProgress");
	CheckSearchListConsistency((struct searchlist_head *)&searches.searchesCompleted, "searchesCompleted");
	
	sys_msg(debug, LOG_INFO, "recordSearchInProgress: Adding 0x%08lx ('%s'): searchesInProgress head: first = 0x%08lx ('%s'), last = 0x%08lx ('%s')",
						searchContext, [[searchContext->parent_vnode name] value],
						searches.searchesInProgress.tqh_first,
							searches.searchesInProgress.tqh_first ? [[searches.searchesInProgress.tqh_first->parent_vnode name] value] : "(null)",
						searches.searchesInProgress.tqh_last,
#if 0
						(searches.searchesInProgress.tqh_last != &searches.searchesInProgress.tqh_first) ?
							[[TAILQ_LAST(&searches.searchesInProgress, SearchListHead)->parent_vnode name] value] : "(empty)"
#else
						"[not shown]"
#endif
			);
#endif
							
	TAILQ_INSERT_TAIL(&searches.searchesInProgress, searchContext, sc_link);
	
#if DEBUG_CHECK_QUEUEING
	sys_msg(debug, LOG_INFO, "recordSearchInProgress: After adding search context 0x%08lx ('%s'), next = 0x%08lx, prev = 0x%08lx",
						searchContext, [[searchContext->parent_vnode name] value], searchContext->sc_link.tqe_next, searchContext->sc_link.tqe_prev);
	sys_msg(debug, LOG_INFO, "recordSearchInProgress: searches.searchesInProgress head: first = 0x%08lx ('%s'), last = 0x%08lx ('%s')",
						searches.searchesInProgress.tqh_first,
							searches.searchesInProgress.tqh_first ? [[searches.searchesInProgress.tqh_first->parent_vnode name] value] : "(null)",
						searches.searchesInProgress.tqh_last,
#if 0
						(searches.searchesInProgress.tqh_last != &searches.searchesInProgress.tqh_first) ?
							[[TAILQ_LAST(&searches.searchesInProgress, SearchListHead)->parent_vnode name] value] : "(empty)"
#else
						"[not shown]"
#endif
			);
	
	CheckSearchListConsistency((struct searchlist_head *)&searches.searchesInProgress, "searchesInProgress");
	CheckSearchListConsistency((struct searchlist_head *)&searches.searchesCompleted, "searchesCompleted");
#endif
	
	pthread_mutex_unlock(&searches.searchListLock);
}

- (void)deleteSearchInProgress:(SearchContext *)searchContext
{
#if DEBUG_CHECK_QUEUEING
	struct _searchContext *savedNext, **savedPrev;
#endif
	
	pthread_mutex_lock(&searches.searchListLock);
	
#if DEBUG_CHECK_QUEUEING
	CheckSearchListConsistency((struct searchlist_head *)&searches.searchesInProgress, "searchesInProgress");
	CheckSearchListConsistency((struct searchlist_head *)&searches.searchesCompleted, "searchesCompleted");
	
	savedNext = searchContext->sc_link.tqe_next;
	savedPrev = searchContext->sc_link.tqe_prev;

	sys_msg(debug, LOG_INFO, "deleteSearchInProgress: Deleting search context 0x%08x ('%s'): next = 0x%08lx, prev = 0x%08lx",
						searchContext, [[searchContext->parent_vnode name] value],
						searchContext->sc_link.tqe_next, searchContext->sc_link.tqe_prev);
	sys_msg(debug, LOG_INFO, "deleteSearchInProgress: searches.searchesInProgress head: first = 0x%08lx ('%s'), last = 0x%08lx ('%s')",
						searches.searchesInProgress.tqh_first,
							searches.searchesInProgress.tqh_first ? [[searches.searchesInProgress.tqh_first->parent_vnode name] value] : "(null)",
						searches.searchesInProgress.tqh_last,
#if 0
						(searches.searchesInProgress.tqh_last != &searches.searchesInProgress.tqh_first) ?
							[[TAILQ_LAST(&searches.searchesInProgress, SearchListHead)->parent_vnode name] value] : "(empty)"
#else
						"[not shown]"
#endif
			);
#endif

	if ( searchContext->searchFlags & kSearchAwaitingCleanup ) {
		TAILQ_REMOVE(&searches.searchesCompleted, searchContext, sc_link);
		searchContext->searchFlags &= ~kSearchAwaitingCleanup;
	} else {
		TAILQ_REMOVE(&searches.searchesInProgress, searchContext, sc_link);
	}
	
#if DEBUG_CHECK_QUEUEING
	sys_msg(debug, LOG_INFO, "deleteSearchInProgress: searches.searchesInProgress head: first = 0x%08lx ('%s'), last = 0x%08lx ('%s')",
						searches.searchesInProgress.tqh_first,
							searches.searchesInProgress.tqh_first ? [[searches.searchesInProgress.tqh_first->parent_vnode name] value] : "(null)",
						searches.searchesInProgress.tqh_last,
#if 0
						(searches.searchesInProgress.tqh_last != &searches.searchesInProgress.tqh_first) ?
							[[TAILQ_LAST(&searches.searchesInProgress, SearchListHead)->parent_vnode name] value] : "(empty)"
#else
						"[not shown]"
#endif
			);
	
	CheckSearchListConsistency((struct searchlist_head *)&searches.searchesInProgress, "searchesInProgress");
	CheckSearchListConsistency((struct searchlist_head *)&searches.searchesCompleted, "searchesCompleted");
#endif
	
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
			searchContext->searchFlags |= kSearchAwaitingCleanup;
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
			targetSearchContext->searchFlags &= ~kSearchAwaitingCleanup;
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
			searchContext->searchFlags |= kSearchAwaitingCleanup;
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
