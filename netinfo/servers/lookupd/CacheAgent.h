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

/*
 * CacheAgent.h
 *
 * Cache server for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <time.h>
#import "LUGlobal.h"
#import "LUPrivate.h"
#import "LUCache.h"
#import "LUAgent.h"
#import <NetInfo/syslock.h>

@interface CacheAgent : LUAgent
{
	struct {
		LUArray *all;
		syslock *lock;
	} rootInitGroups;

	struct {
		LUCache *cache;
		unsigned int capacity;
		time_t ttl;
		time_t delta;
		unsigned int freq;
		BOOL validate;
		BOOL enabled;
	} cacheStore[NCACHE];

	struct {
		LUArray *all;
		BOOL validate;
		BOOL enabled;
		syslock *lock;
	} allStore[NCATEGORIES];

	char *initgroupsUserName;
	LUDictionary *stats;

	unsigned int lastSweep;
	unsigned int sweepTime;
}

+ (int)cacheForKey:(char *)key category:(LUCategory)cat;

- (void)setCapacity:(unsigned int)max forCategory:(LUCategory)cat;
- (unsigned int)capacityForCategory:(LUCategory)cat;

- (void)setTimeToLive:(time_t)timeout forCategory:(LUCategory)cat;
- (time_t)timeToLiveForCategory:(LUCategory)cat;

- (void)setCacheIsValidated:(BOOL)validate forCategory:(LUCategory)cat;
- (BOOL)cacheIsValidatedForCategory:(LUCategory)cat;

- (void)setCacheIsEnabled:(BOOL)enabled forCategory:(LUCategory)cat;
- (BOOL)cacheIsEnabledForCategory:(LUCategory)cat;

- (void)addTimeToLive:(time_t)delta
	afterCacheHits:(unsigned int)freq
	forCategory:(LUCategory)cat;

- (time_t)cacheTimeToLiveDeltaForCategory:(LUCategory)cat;
- (unsigned int)cacheTimeToLiveFrequencyForCategory:(LUCategory)cat;

- (void)addObject:(LUDictionary *)item;
- (void)removeObject:(LUDictionary *)item;

- (void)addArray:(LUArray *)array;

- (LUArray *)initgroupsForUser:(char *)name;
- (void)setInitgroups:(LUArray *)groups forUser:(char *)name;

- (void)flushCache;
- (void)flushCacheForCategory:(LUCategory)cat;

- (BOOL)containsObject:(id)obj;
- (void)sweepCache;

@end
