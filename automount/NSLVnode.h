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
    NetworkObjectType NSLObjectType;
	union {
		NSLNeighborhood neighborhood;
		NSLService service;
	} NSLObject;
	BOOL fixedEntry;
	BOOL havePopulated;
	BOOL beingPopulated;
	struct timeval ErrorTime;
    struct timeval lastUpdate;
	unsigned long currentContentGeneration;
}

- (NSLVnode *)init;

- (NSLVnode *)newNeighborhoodWithName:(String *)newNeighborhoodname neighborhood:(NSLNeighborhood)neighborhood;
- (NSLVnode *)newServiceWithName:(String *)newServiceName service:(NSLService)service serviceURL:(char *)serverURL;
- (NSLVnode *)newSymlinkWithName:(String *)newSymlink target:(char *)target;

- (void)invalidateRecursively:(BOOL)invalidateDescendants;

- (unsigned long)getGeneration;
- (void)setGeneration:(unsigned long)newGeneration;

- (NetworkObjectType)getobjectType;
- (void)setNSLObject:(const void *)object type:(NetworkObjectType)objecttype;
- (void)freeNSLObject;

- (NSLNeighborhood)getNSLNeighborhood;

- (NSLService)getNSLService;

- (BOOL)fixedEntry;
- (void)setFixedEntry:(BOOL)newFixedEntryStatus;

@end

#endif __NSL_VNODE_H__
