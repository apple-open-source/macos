#ifndef __NSL_VNODE_H__
#define __NSL_VNODE_H__

#import "NSLMap.h"
#import "AMVnode.h"
#import "NSLUtil.h"
#import <CoreServices/CoreServices.h>

@class String;

@interface NSLVnode : Vnode
{
	unsigned long generation;
	String *apparentName;
    NetworkObjectType NSLObjectType;
	union {
		NSLNeighborhood neighborhood;
		NSLService service;
	} NSLObject;
	BOOL fixedEntry;
	BOOL beingPopulated;
	BOOL havePopulated;
	BOOL neighborhoodSearchStarted;
	BOOL neighborhoodSearchComplete;
	SearchContext neighborhoodSearchContext;
	struct SearchResultList neighborhoodSearchResults;
	BOOL servicesSearchStarted;
	BOOL servicesSearchComplete;
	SearchContext servicesSearchContext;
	struct SearchResultList servicesSearchResults;
	struct timeval lastNotification;
	struct timeval lastSeen;
	struct timeval ErrorTime;
    struct timeval lastUpdate;
	unsigned long currentContentGeneration;
}

- (NSLVnode *)init;

- (String *)apparentName;
- (void)setApparentName:(String *)n;

- (NSLVnode *)newNeighborhoodWithName:(String *)newNeighborhoodname neighborhood:(NSLNeighborhood)neighborhood;
- (NSLVnode *)newServiceWithName:(String *)newServiceName service:(NSLService)service serviceURL:(char *)serverURL;
- (NSLVnode *)newSymlinkWithName:(String *)newSymlink target:(char *)target;

- (void)triggerDeferredNotifications:(SearchContext *)searchContext;

- (void)invalidateRecursively:(BOOL)invalidateDescendants notifyFinder:(BOOL)notifyFinder;

- (unsigned long)getGeneration;
- (void)setGeneration:(unsigned long)newGeneration;

- (NetworkObjectType)getobjectType;
- (void)setNSLObject:(const void *)object type:(NetworkObjectType)objecttype;
- (void)freeNSLObject;

- (NSLNeighborhood)getNSLNeighborhood;

- (NSLService)getNSLService;

- (void)stopSearchesInProgress;

- (BOOL)fixedEntry;
- (void)setFixedEntry:(BOOL)newFixedEntryStatus;

- (BOOL)processSearchResults:(SearchContext *)searchContext;

@end

#endif __NSL_VNODE_H__
