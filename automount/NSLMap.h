#ifndef __NSL_MAP_H__
#define _NSL_MAP_H__

#import <sys/types.h>
#import <sys/queue.h>
#import <pthread.h>

#include <CoreServices/CoreServices.h>
#include <CoreServices/CoreServicesPriv.h>

#import "AMMap.h"
#import "NSLUtil.h"
#import "automount.h"

@class NSLVnode;

typedef struct {
	pthread_mutex_t searchListLock;
	TAILQ_HEAD(SearchListHead, _searchContext) searchesInProgress;
	TAILQ_HEAD(CompletedListHead, _searchContext) searchesCompleted;
	BOOL searchCompleted;
} SearchList;

@interface NSLMap : Map
{
	struct NSLMapListEntry NSMap_list_record;
	BOOL clientRefInvalid;
    NSLClientRef clientRef;
	SearchList searches;
	CFMessagePortRef notificationMessagePort;
}

- (NSLClientRef)getNSLClientRef;
- (void)setNSLClientRef:(NSLClientRef)newNSLClientRef;

- (SearchList *)searchList;
- (void)recordSearchInProgress:(SearchContext *)searchContext;
- (void)deleteSearchInProgress:(SearchContext *)searchContext;
- (void)processNewSearchResults;
- (void)cleanupSearchContextList;

- (void)triggerDeferredNotifications;

- (CFMessagePortRef)notificationMessagePort;
- (void)setNotificationMessagePort:(CFMessagePortRef)newNotificationMessagePort;

@end

#endif __NSL_MAP_H__
