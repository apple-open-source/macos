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
 * CacheAgent.m
 *
 * Cache server for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/system_log.h>
#import "Root.h"
#import "Thread.h"
#import "CacheAgent.h"
#import "Config.h"
#import "LUPrivate.h"
#import "LUCachedDictionary.h"
#import <arpa/inet.h>
#import <stdio.h>
#import <NetInfo/dsutil.h>

#define CUserName			 0
#define CUserNumber			 1
#define CGroupName 			 2
#define CGroupNumber 		 3
#define CHostName 			 4
#define CHostIPAddress		 5
#define CHostENAddress		 6
#define CNetworkName		 7
#define CNetworkIPAddress	 8
#define CServiceName		 9
#define CServiceNumber		10
#define CProtocolName		11
#define CProtocolNumber		12
#define CRpcName			13
#define CRpcNumber			14
#define CMountName			15
#define CPrinterName		16
#define CBootparamName		17
#define CBootpName			18
#define CBootpIPAddress		19
#define CBootpENAddress		20
#define CAliasName			21
#define CNetgroupName		22

LUCategory cacheCategory[] =
{
	LUCategoryUser,
	LUCategoryUser,
	LUCategoryGroup,
	LUCategoryGroup,
	LUCategoryHost,
	LUCategoryHost,
	LUCategoryHost,
	LUCategoryNetwork,
	LUCategoryNetwork,
	LUCategoryService,
	LUCategoryService,
	LUCategoryProtocol,
	LUCategoryProtocol,
	LUCategoryRpc,
	LUCategoryRpc,
	LUCategoryMount,
	LUCategoryPrinter,
	LUCategoryBootparam,
	LUCategoryBootp,
	LUCategoryBootp,
	LUCategoryBootp,
	LUCategoryAlias,
	LUCategoryNetgroup
};

char *cacheName[] =
{
	"user (by name)",
	"user (by uid)",	
	"group (by name)",	
	"group (by gid)",	
	"host (by name)",	
	"host (by Internet address)",	
	"host (by Ethernet address)",	
	"network (by name)",	
	"network (by Internet address)",	
	"service (by name)",	
	"service (by number)",	
	"protocol (by name)",	
	"protocol (by number)",	
	"rpc (by name)",	
	"protocol (by number)",	
	"mount",	
	"printer",	
	"bootparam (by name)",	
	"bootparam (by Internet address)",	
	"bootparam (by Ethernet address)",	
	"bootp",	
	"alias",	
	"netgroup"
};

int cacheForCategory[] =
{
	CUserName,
	CGroupName,
	CHostName,
	CNetworkName,
	CServiceName,
	CProtocolName,
	CRpcName,
	CMountName,
	CPrinterName,
	CBootparamName,
	CBootpName,
	CAliasName,
	-1,
	-1,
	CNetgroupName,
	-1,
	-1
};

#define forever for(;;)

static CacheAgent *_sharedCacheAgent = nil;

static BOOL shutdown_CacheAgent = NO;

@implementation CacheAgent

- (void)ageCache:(unsigned int)n
{
	int i, len;
	int expired, expireAll, expireInitGroups, expireRootInitgroups;
	time_t age;
	time_t ttl;
	LUDictionary *item;
	LUArray *all;
	LUCategory cat;
	LUCache *cache;

	if (n >= NCACHE) return;
	
	cat = cacheCategory[n];

	if (cat >= NCATEGORIES) return;
 
	cache = cacheStore[n].cache;

	expired = 0;
	expireAll = 0;
	expireInitGroups = 0;
	expireRootInitgroups = 0;
	
	len = [cache count];
	for (i = len - 1; i >= 0; i--)
	{
		item = [cache objectAtIndex:i];
		if (item == nil) continue;

		ttl = [item timeToLive];
		age = [item age];

		if (age > ttl)
		{
			[cache removeObject:item];
			expired++;
		}
	}

	if (cat == LUCategoryGroup)
	{
		all = allStore[LUCategoryInitgroups].all;
		if (all != nil)
		{
			if (expired > 0)
				expireInitGroups++;
			else if ([all validationStampCount] == 0)
				expireInitGroups++;
			else
			{
				item = [all validationStampAtIndex:0];
				ttl = [item timeToLive];
				age = [item age];

				if (age > ttl) expireInitGroups++;
			}
		}

		all = rootInitGroups;
		if (all != nil)
		{
			if (expired > 0)
				expireRootInitgroups++;
			else if ([all validationStampCount] == 0)
				expireRootInitgroups++;
			else
			{
				item = [all validationStampAtIndex:0];
				ttl = [item timeToLive];
				age = [item age];

				if (age > ttl) expireRootInitgroups++;
			}
		}
	}

	if (expireInitGroups)
	{
		[allStore[LUCategoryInitgroups].all release];
		allStore[LUCategoryInitgroups].all = nil;
		if (initgroupsUserName != NULL) freeString(initgroupsUserName);
		initgroupsUserName = NULL;
	}

	if (expireRootInitgroups)
	{
		[rootInitGroups release];
		rootInitGroups = nil;
	}

	if (allStore[cat].all != nil)
	{
		if (expired > 0)
			expireAll++;
		else if ([allStore[cat].all validationStampCount] == 0)
			expireAll++;
		else
		{
			item = [allStore[cat].all validationStampAtIndex:0];
			ttl = [item timeToLive];
			age = [item age];

			if (age > ttl) expireAll++;
		}
	}

	if (expireAll)
	{
		[allStore[cat].all release];
		allStore[cat].all = nil;
	}
	
	if (expired > 0)
	{
		system_log(LOG_DEBUG, "expired %d object%s in %s cache",
			expired, (expired == 1) ? "" : "s", cacheName[n]);
	}

	if (expireAll > 0)
	{
		system_log(LOG_DEBUG, "expired all %s cache", [LUAgent categoryName:cat]);
	}

	if (expireInitGroups > 0)
	{
		system_log(LOG_DEBUG, "expired all groups for user cache");
	}

	if (expireRootInitgroups > 0)
	{
		system_log(LOG_DEBUG, "expired all groups for root cache");
	}
}

- (time_t)minTimeToLive
{
	int i;
	time_t min;
 
	min = cacheStore[0].ttl;
	for (i = 1; i < NCACHE; i++)
		if (cacheStore[i].ttl < min) min = cacheStore[i].ttl;
	return min;
}

- (void)sweepCache
{
	int i;
	struct timeval now;
	unsigned int delta;

	gettimeofday(&now, NULL);
	delta = now.tv_sec - lastSweep;

	if (delta < sweepTime) return;
	lastSweep = now.tv_sec;

	syslock_lock(cacheLock);
	for (i = 0; i < NCACHE; i++) [self ageCache:i];
	syslock_unlock(cacheLock);
}

/*
 * Object creation, initilizations, and general stuff
 */

- (LUAgent *)initWithArg:(char *)arg
{
	return [self init];
}

- (void)setCacheIsValidated:(BOOL)validate forCategory:(LUCategory)cat
{
	int i;

	for (i = 0; i < NCACHE; i++)
	{
		if (cacheCategory[i] == cat)
			cacheStore[i].validate = validate;
	}

	allStore[(unsigned int)cat].validate = validate;
}

- (void)setCapacity:(unsigned int)max forCategory:(LUCategory)cat
{
	int i;

	for (i = 0; i < NCACHE; i++)
	{
		if (cacheCategory[i] == cat)
			cacheStore[i].capacity = max;
	}
}

- (void)setTimeToLive:(time_t)timeout forCategory:(LUCategory)cat
{
	int i;

	for (i = 0; i < NCACHE; i++)
	{
		if (cacheCategory[i] == cat)
			cacheStore[i].ttl = timeout;
	}
}

- (time_t)timeToLiveForCategory:(LUCategory)cat
{
	int n;

	n = cacheForCategory[cat];
	if (n < 0) return 0;
	return cacheStore[n].ttl;
}

- (void)setTimeToLive:(time_t)ttl forArray:(LUArray *)array
{
	LUDictionary *stamp;
	int i, len;

	if (array == nil) return;

	len = [array validationStampCount];
	for (i = 0; i < len; i++)
	{
		stamp = [array validationStampAtIndex:i];
		if (stamp != nil) [stamp setTimeToLive:ttl];
	}
}

- (void)setCacheIsEnabled:(BOOL)enabled forCategory:(LUCategory)cat
{
	int i;

	for (i = 0; i < NCACHE; i++)
	{
		if (cacheCategory[i] == cat)
			cacheStore[i].enabled = enabled;
	}

	allStore[(unsigned int)cat].enabled = enabled;
}

- (CacheAgent *)init
{
	int i, j;
	char str[128], **order;
	struct timeval now;
	LUDictionary *global, *config;
	BOOL gValidation, cValidation, gEnable, cEnable;
	unsigned int gMax, cMax;
	time_t gTTL, cTTL;

	if (didInit) return self;

	[super init];

	gettimeofday(&now, NULL);
	lastSweep = now.tv_sec;
	sweepTime = [self minTimeToLive];
	if (sweepTime < 60) sweepTime = 60;
	cacheLock = syslock_new(1);

	for (i = 0; i < NCACHE; i++)
	{
		cacheStore[i].cache = [[LUCache alloc] init];
		sprintf(str, "%s cache store", cacheName[i]);
		[cacheStore[i].cache setBanner:str];
		cacheStore[i].capacity = (unsigned int)-1;
		cacheStore[i].ttl = 43200;
		cacheStore[i].validate = YES;
		cacheStore[i].enabled = NO;
	}

	for (i = 0; i < NCATEGORIES; i++)
	{
		allStore[i].all = nil;
		allStore[i].validate = YES;
		allStore[i].enabled = NO;
	}

	rootInitGroups = nil;

	initgroupsUserName = NULL;

	global = [configManager configGlobal:configurationArray];

	gValidation = [configManager boolForKey:"ValidateCache" dict:global default:YES];
	gMax = [configManager intForKey:"CacheCapacity" dict:global default:-1];
	if (gMax == 0) gMax = (unsigned int)-1;
	gTTL = (time_t)[configManager intForKey:"TimeToLive" dict:global default:43200];

	gEnable = NO;
	order = [global valuesForKey:"LookupOrder"];
	if (order != NULL)
	{
		for (i = 0; order[i] != NULL; i++)
		{
			if (streq(order[i], "Cache") || streq(order[i], "CacheAgent"))
				gEnable = YES;
		}
	}

	for (i = 0; i < NCATEGORIES; i++)
	{	
		config = [configManager configForCategory:i fromConfig:configurationArray];
		cValidation = [configManager boolForKey:"ValidateCache" dict:config default:gValidation];
		cMax = [configManager intForKey:"CacheCapacity" dict:config default:gMax];
		if (cMax == 0) cMax = (unsigned int)-1;
		cTTL = (time_t)[configManager intForKey:"TimeToLive" dict:config default:gTTL];

		cEnable = gEnable;
		order = [config valuesForKey:"LookupOrder"];
		if (order != NULL)
		{
			cEnable = NO;
			for (j = 0; order[j] != NULL; j++)
			{
				if (streq(order[j], "Cache") || streq(order[j], "Cache"))
					cEnable = YES;
			}
		}

		[self setCacheIsValidated:cValidation forCategory:(LUCategory)i];
		[self setCacheIsEnabled:cEnable forCategory:(LUCategory)i];
		[self setCapacity:cMax forCategory:(LUCategory)i];
		[self setTimeToLive:cTTL forCategory:(LUCategory)i];
	}

	return self;
}

+ (CacheAgent *)alloc
{
	if (_sharedCacheAgent != nil)
	{
		[_sharedCacheAgent retain];
		return _sharedCacheAgent;
	}

	_sharedCacheAgent = [super alloc];
	_sharedCacheAgent = [_sharedCacheAgent init];
	if (_sharedCacheAgent == nil) return nil;

	system_log(LOG_DEBUG, "Allocated CacheAgent 0x%08x\n", (int)_sharedCacheAgent);

	return _sharedCacheAgent;
}

- (void)dealloc
{
	int i;

	shutdown_CacheAgent = YES;

	for (i = 0; i < NCACHE; i++)
	{
		if (cacheStore[i].cache != nil) [cacheStore[i].cache release];
	}

	for (i = 0; i < NCATEGORIES; i++)
	{
		if (allStore[i].all != nil) [allStore[i].all release];
	}

	[rootInitGroups release];

	if (initgroupsUserName != NULL) freeString(initgroupsUserName);
	initgroupsUserName = NULL;

	syslock_free(cacheLock);
	
	system_log(LOG_DEBUG, "Deallocated CacheAgent 0x%08x\n", (int)self);

	[super dealloc];

	_sharedCacheAgent = nil;
}

- (const char *)shortName
{
	return "Cache";
}

- (BOOL)isValid:(LUDictionary *)item
{
	id agent;

	if (item == nil) return NO;
		
	agent = [item agent];
	if (agent == nil) return NO;

	return [agent isValid: item];
}

- (LUDictionary *)postProcess:(LUDictionary *)item
	cache:(unsigned int)n
	key:(char *)key
{
	time_t age, ttl;
	unsigned int hits;

	if (item == nil) return nil;
	ttl = [item timeToLive];

	if (cacheStore[n].validate)
	{
		if (![self isValid:item])
		{
			[cacheStore[n].cache removeObject:item];
			return nil;
		}
	}

	age = [item age];
	if (age > ttl)
	{
		[self removeObject:item];
		return nil;
	}

	hits = [item cacheHit];
	if (cacheStore[n].validate) [item resetAge];

	/* Retain the object here.  Caller must release. */
	[item retain];
	return item;
}

- (BOOL)isArrayValid:(LUArray *)array
{
	unsigned int i, len;
	time_t age;
	LUDictionary *stamp;
	LUAgent *agent;

	if (array == nil) return NO;
	len = [array validationStampCount];
	if (len == 0) return NO;

	for (i = 0; i < len; i++)
	{
		stamp = [array validationStampAtIndex:i];
		if (stamp == nil) return NO;
		age = [stamp age];
		if (age > [stamp timeToLive]) return NO;

		agent = [stamp agent];
		if (agent == nil) return NO;
		if (![agent isValid:stamp]) return NO;
	}
	return YES;
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	LUArray *all;

	if (cat > NCATEGORIES) return nil;

	syslock_lock(cacheLock);

	all = allStore[(unsigned int)cat].all;
	if (all == nil)
	{
		syslock_unlock(cacheLock);
		return nil;
	}

	/* Retain the array here.  Caller must release */
	if (!allStore[(unsigned int)cat].validate)
	{
		[all retain];
		syslock_unlock(cacheLock);
		return all;
	}

	if ([self isArrayValid:all])
	{
		[all retain];
		syslock_unlock(cacheLock);
		return all;
	}

	[all release];
	allStore[(unsigned int)cat].all = nil;

	syslock_unlock(cacheLock);
	return nil;
}

- (void)addArray:(LUArray *)array
{
	LUDictionary *stamp;
	LUCategory cat;
	time_t ttl;

	if (array == nil) return;

	stamp = [array validationStampAtIndex:0];
	if (stamp == nil) return;
	cat = [stamp category];
	if (cat >= NCATEGORIES) return;

	/* initgroups arrays are handled by setInitgroupsForUser: */
	if (cat == LUCategoryInitgroups) return;

	ttl = [self timeToLiveForCategory:cat];
	[self setTimeToLive:ttl forArray:array];

	syslock_lock(cacheLock);
	if (allStore[cat].all != nil) [allStore[cat].all release];
	allStore[cat].all = [array retain];
	syslock_unlock(cacheLock);
}

- (LUArray *)initgroupsForUser:(char *)name
{
	LUArray *all;

	syslock_lock(cacheLock);

	if (streq(name, "root"))
	{
		if (rootInitGroups == nil)
		{
			syslock_unlock(cacheLock);
			return nil;
		}

		if (!allStore[(unsigned int)LUCategoryInitgroups].validate)
		{
			[rootInitGroups retain];
			syslock_unlock(cacheLock);
			return rootInitGroups;
		}

		if ([self isArrayValid:rootInitGroups])
		{
			[rootInitGroups retain];
			syslock_unlock(cacheLock);
			return rootInitGroups;
		}

		[rootInitGroups release];
		rootInitGroups = nil;
		syslock_unlock(cacheLock);
		return nil;
	}

	if ((initgroupsUserName == NULL) || (name == NULL))
	{
		syslock_unlock(cacheLock);
		return nil;
	}

	if (strcmp(name, initgroupsUserName))
	{
		syslock_unlock(cacheLock);
		return nil;
	}

	all = allStore[(unsigned int)LUCategoryInitgroups].all;
	if (all == nil)
	{
		syslock_unlock(cacheLock);
		return nil;
	}

	/* Retain the array here.  Caller must release */
	if (!allStore[(unsigned int)LUCategoryInitgroups].validate)
	{
		[all retain];
		syslock_unlock(cacheLock);
		return all;
	}

	if ([self isArrayValid:all])
	{
		[all retain];
		syslock_unlock(cacheLock);
		return all;
	}

	[all release];
	allStore[(unsigned int)LUCategoryInitgroups].all = nil;

	syslock_unlock(cacheLock);
	return nil;
}

- (LUArray *)allGroupsWithUser:(char *)name
{
	return [self initgroupsForUser:name];
}

- (void)setInitgroups:(LUArray *)groups forUser:(char *)name
{
	time_t ttl;

	if (name == NULL) return;

	ttl = [self timeToLiveForCategory:LUCategoryGroup];
	[self setTimeToLive:ttl forArray:groups];

	syslock_lock(cacheLock);

	if (streq(name, "root"))
	{
		if (rootInitGroups != nil) [rootInitGroups release];
		rootInitGroups = [groups retain];
		syslock_unlock(cacheLock);
		return;
	}

	if (initgroupsUserName != NULL) freeString(initgroupsUserName);
	initgroupsUserName = copyString(name);

	if (allStore[(unsigned int)LUCategoryInitgroups].all != nil)
	{
		[allStore[(unsigned int)LUCategoryInitgroups].all release];
	}
	allStore[(unsigned int)LUCategoryInitgroups].all = [groups retain];

	syslock_unlock(cacheLock);
}

- (LUDictionary *)cache:(unsigned int)n itemWithKey:(char *)key category:(LUCategory)cat
{
	LUDictionary *item;
 
	item = [cacheStore[n].cache objectForKey:key];
	item = [self postProcess:item cache:n key:key];

	return item;
}

/*
 * Utilities
 */

+ (int)cacheForKey:(char *)key category:(LUCategory)cat
{
	switch (cat)
	{
		case LUCategoryUser:
			if (streq(key, "name")) return CUserName;
			if (streq(key, "uid")) return CUserNumber;
			return -1;

		case LUCategoryGroup:
			if (streq(key, "name")) return CGroupName;
			if (streq(key, "gid")) return CGroupNumber;
			return -1;

		case LUCategoryHost:
			if (streq(key, "name")) return CHostName;
			if (streq(key, "ip_address")) return CHostIPAddress;
			if (streq(key, "en_address")) return CHostENAddress;
			return -1;

		case LUCategoryNetwork:
			if (streq(key, "name")) return CNetworkName;
			if (streq(key, "address")) return CNetworkIPAddress;
			return -1;

		case LUCategoryService:
			if (streq(key, "name")) return CServiceName;
			if (streq(key, "number")) return CServiceNumber;
			return -1;

		case LUCategoryProtocol:
			if (streq(key, "name")) return CProtocolName;
			if (streq(key, "number")) return CProtocolNumber;
			return -1;

		case LUCategoryRpc:
			if (streq(key, "name")) return CRpcName;
			if (streq(key, "number")) return CRpcNumber;
			return -1;

		case LUCategoryMount:
			if (streq(key, "name")) return CMountName;
			return -1;

		case LUCategoryPrinter:
			if (streq(key, "name")) return CPrinterName;
			return -1;

		case LUCategoryBootparam:
			if (streq(key, "name")) return CBootparamName;
			return -1;

		case LUCategoryBootp:
			if (streq(key, "name")) return CBootpName;
			if (streq(key, "ip_address")) return CBootpIPAddress;
			if (streq(key, "en_address")) return CBootpENAddress;
			return -1;

		case LUCategoryAlias:
			if (streq(key, "name")) return CAliasName;
			return -1;

		case LUCategoryNetgroup:
			if (streq(key, "name")) return CNetgroupName;
			return -1;

		default:
			return -1;
	}

	return -1;
}

- (void)freeSpace:(unsigned int)n inCache:(unsigned int)cacheNum
{
	unsigned int i, size, avail;

	size = [cacheStore[cacheNum].cache count];
	avail = cacheStore[cacheNum].capacity - size;

	for (i = avail; i < n; i++)
		[cacheStore[cacheNum].cache removeOldestObject];
}

/*
 * Add objects to cache 
 */

- (void)addObject:(LUDictionary *)item
	category:(LUCategory)cat
	toCache:(unsigned int)cacheNum
	key:(char *)keyName
{
	char **values;

	if (item == nil) return;
	if (cacheNum >= NCACHE) return;
	if (!cacheStore[cacheNum].enabled) return;
	if (keyName == NULL) return;

	values = [item valuesForKey:keyName];
	if (values == NULL) return;

	[self freeSpace:1 inCache:cacheNum];
	[item setTimeToLive:cacheStore[cacheNum].ttl];
	[item setCacheHits:0];
	[cacheStore[cacheNum].cache setObject:item forKeys:values];
}

- (void)addEthernetObject:(LUDictionary *)item
	category:(LUCategory)cat
	toCache:(unsigned int)cacheNum
{
	char **values;
	int i, len;

	if (item == nil) return;
	if (!(cacheNum == CHostENAddress || cacheNum == CBootpENAddress)) return;
	if (!cacheStore[cacheNum].enabled) return;

	values = [item valuesForKey:"en_address"];
	if (values == NULL) return;

	[self freeSpace:1 inCache:cacheNum];
	[item setTimeToLive:cacheStore[cacheNum].ttl];
	[item setCacheHits:0];

	len = [item countForKey:"en_address"];
	if (len < 0) len = 0;
	for (i = 0; i < len; i++)
	{
		[cacheStore[cacheNum].cache setObject:item
			forKey:[LUAgent canonicalEthernetAddress:values[i]]];
	}
}

- (void)addService:(LUDictionary *)item
{
	char **names;
	char **numbers;
	char **protocols;
	int j, nnames, nnumbers;
	int i, nprotocols;
	LUCache *nameCache;
	LUCache *numberCache;
	char str[256];

	if (item == nil) return;
	if (!cacheStore[CServiceName].enabled) return;

	names = [item valuesForKey:"name"];
	numbers = [item valuesForKey:"number"];
	protocols = [item valuesForKey:"protocol"];

	if (protocols == NULL) return;

	nameCache = cacheStore[CServiceName].cache;
	if (nameCache == nil) return;

	numberCache = cacheStore[CServiceNumber].cache;
	if (numberCache == nil) return;

	[self freeSpace:1 inCache:CServiceName];
	[self freeSpace:1 inCache:CServiceNumber];

	if (names == NULL) nnames = 0;
	else nnames = [item countForKey:"name"];
	if (nnames < 0) nnames = 0;

	if (numbers == NULL) nnumbers = 0;
	nnumbers = [item countForKey:"number"];
	if (nnumbers < 0) nnumbers = 0;

	nprotocols = [item countForKey:"protocol"];
	if (nprotocols < 0) nprotocols = 0;

	[item setTimeToLive:cacheStore[CServiceName].ttl];
	[item setCacheHits:0];
	[nameCache setObject:item forKeys:names];

	for (i = 0; i < nprotocols; i++)
	{
		for (j = 0; j < nnames; j++)
		{
			sprintf(str, "%s/%s", names[j], protocols[i]);
			[nameCache setObject:item forKey:str];
		}

		for (j = 0; j < nnumbers; j++)
		{
			sprintf(str, "%s/%s", numbers[j], protocols[i]);
			[numberCache setObject:item forKey:str];
		}
	}
}

- (void)addObject:(LUDictionary *)item
{
	LUCategory cat;

	if (item == nil) return;

	syslock_lock(cacheLock);

	cat = [item category];
	switch (cat)
	{
		case LUCategoryUser:
			[self addObject:item category:cat toCache:CUserName key:"name"];
			[self addObject:item category:cat toCache:CUserName key:"realname"];
			[self addObject:item category:cat toCache:CUserNumber key:"uid"];
			break;
		case LUCategoryGroup:
			[self addObject:item category:cat toCache:CGroupName key:"name"];
			[self addObject:item category:cat toCache:CGroupNumber key:"gid"];
			break;
		case LUCategoryHost:
			[self addObject:item category:cat toCache:CHostName key:"name"];
			[self addObject:item category:cat toCache:CHostIPAddress key:"ip_address"];
			[self addEthernetObject:item category:cat toCache:CHostENAddress];
			break;
		case LUCategoryNetwork:
			[self addObject:item category:cat toCache:CNetworkName key:"name"];
			[self addObject:item category:cat toCache:CNetworkIPAddress key:"address"];
			break;
		case LUCategoryService:
			[self addService:item];
			break;
		case LUCategoryProtocol:
			[self addObject:item category:cat toCache:CProtocolName key:"name"];
			[self addObject:item category:cat toCache:CProtocolNumber key:"number"];
			break;
		case LUCategoryRpc:
			[self addObject:item category:cat toCache:CRpcName key:"name"];
			[self addObject:item category:cat toCache:CRpcNumber key:"number"];
			break;
		case LUCategoryMount:
			[self addObject:item category:cat toCache:CMountName key:"name"];
			break;
		case LUCategoryPrinter:
			[self addObject:item category:cat toCache:CPrinterName key:"name"];
			break;
		case LUCategoryBootparam:
			[self addObject:item category:cat toCache:CBootparamName key:"name"];
			break;
		case LUCategoryBootp:
			[self addObject:item category:cat toCache:CBootpName key:"name"];
			[self addObject:item category:cat toCache:CBootpIPAddress key:"ip_address"];
			[self addEthernetObject:item category:cat toCache:CBootpENAddress];
			break;
		case LUCategoryAlias:
			[self addObject:item category:cat toCache:CAliasName key:"name"];
			break;
		case LUCategoryNetgroup:
			[self addObject:item category:cat toCache:CNetgroupName key:"name"];
			break;
		default: break;
	}

	syslock_unlock(cacheLock);
}

/*
 * Remove objects from cache
 */

- (void)removeObject:(LUDictionary *)item
{
	LUCategory cat;

	if (item == nil) return;

	cat = [item category];
	syslock_lock(cacheLock);

	switch (cat)
	{
		case LUCategoryUser:
			[cacheStore	[CUserName].cache removeObject:item];
			[cacheStore	[CUserNumber].cache removeObject:item];
			break;
		case LUCategoryGroup:
			[cacheStore	[CGroupName].cache removeObject:item];
			[cacheStore	[CGroupNumber].cache removeObject:item];
			break;
		case LUCategoryHost:
			[cacheStore	[CHostName].cache removeObject:item];
			[cacheStore	[CHostIPAddress].cache removeObject:item];
			[cacheStore	[CHostENAddress].cache removeObject:item];
			break;
		case LUCategoryNetwork:
			[cacheStore	[CNetworkName].cache removeObject:item];
			[cacheStore	[CNetworkIPAddress].cache removeObject:item];
			break;
		case LUCategoryService:
			[cacheStore	[CServiceName].cache removeObject:item];
			[cacheStore	[CServiceNumber].cache removeObject:item];
			break;
		case LUCategoryProtocol:
			[cacheStore	[CProtocolName].cache removeObject:item];
			[cacheStore	[CProtocolNumber].cache removeObject:item];
			break;
		case LUCategoryRpc:
			[cacheStore	[CRpcName].cache removeObject:item];
			[cacheStore	[CRpcNumber].cache removeObject:item];
			break;
		case LUCategoryMount:
			[cacheStore	[CMountName].cache removeObject:item];
			break;
		case LUCategoryPrinter:
			[cacheStore	[CPrinterName].cache removeObject:item];
			break;
		case LUCategoryBootparam:
			[cacheStore	[CBootparamName].cache removeObject:item];
			break;
		case LUCategoryBootp:
			[cacheStore	[CBootpName].cache removeObject:item];
			[cacheStore	[CBootpIPAddress].cache removeObject:item];
			[cacheStore	[CBootpENAddress].cache removeObject:item];
			break;
		case LUCategoryAlias:
			[cacheStore	[CAliasName].cache removeObject:item];
			break;
		case LUCategoryNetgroup:
			[cacheStore	[CNetgroupName].cache removeObject:item];
			break;
		default: break;
	}

	syslock_unlock(cacheLock);
}

- (void)reset
{
	syslock_lock(cacheLock);
	[configurationArray release];
	generation = [configManager generation];
	configurationArray = [configManager config];
	syslock_unlock(cacheLock);

	[self flushCache];
}

- (void)flushCache
{
	int i;

	syslock_lock(cacheLock);

	for (i = 0; i < NCACHE; i++) [cacheStore[i].cache empty];

	for (i = 0; i < NCATEGORIES; i++)
	{
		[allStore[i].all release];
		allStore[i].all = nil;

		if (i == LUCategoryInitgroups)
		{
			if (initgroupsUserName != NULL) freeString(initgroupsUserName);
			initgroupsUserName = NULL;
		}
	}

	if (rootInitGroups != nil) [rootInitGroups release];
	rootInitGroups = nil;

	syslock_unlock(cacheLock);
}

- (void)flushCacheForCategory:(LUCategory)cat
{
	unsigned int i;

	syslock_lock(cacheLock);

	for (i = 0; i < NCACHE; i++)
	{
		if (cacheCategory[i] == cat) [cacheStore[i].cache empty];
	}

	i = (unsigned int)cat;
	if (allStore[i].all != nil)
	{
		[allStore[i].all release];
		allStore[i].all = nil;
	}

	if (cat == LUCategoryInitgroups)
	{
		if (rootInitGroups != nil) [rootInitGroups release];
		rootInitGroups = nil;
	}

	syslock_unlock(cacheLock);
}

/*
 * Cache management
 */

- (BOOL)cacheIsEnabledForCategory:(LUCategory)cat
{
	if (cat > NCATEGORIES) return NO;
	return allStore[(unsigned int)cat].enabled;
}

- (BOOL)containsObject:(id)obj
{
	int i;

	syslock_lock(cacheLock);

	if ([obj isMemberOf:[LUArray class]])
	{
		for (i = 0; i < NCATEGORIES; i++)
		{
			if (obj == allStore[i].all)
			{
				syslock_unlock(cacheLock);
				return YES;
			}
		}

		if (obj == rootInitGroups)
		{
			syslock_unlock(cacheLock);
			return YES;
		}

		syslock_unlock(cacheLock);
		return NO;
	}

	for (i = 0; i < NCACHE; i++)
	{
		if ([cacheStore[i].cache containsObject:obj])
		{
			syslock_unlock(cacheLock);
			return YES;
		}
	}

	syslock_unlock(cacheLock);

	return NO;
}

- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	int n;
	LUDictionary *item;

	n = [CacheAgent cacheForKey:key category:cat];
	if (n < 0 || n > NCACHE) return nil;

	syslock_lock(cacheLock);
	item = [self cache:n itemWithKey:val category:cat];
	syslock_unlock(cacheLock);

	return item;
}

@end
