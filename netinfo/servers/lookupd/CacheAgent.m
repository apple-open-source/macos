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
#import "LUServer.h"
#import "Config.h"
#import "LUPrivate.h"
#import "LUCachedDictionary.h"
#import <arpa/inet.h>
#import <stdio.h>
#import <string.h>
#import <NetInfo/dsutil.h>

#define CUserName			 0
#define CUserNumber			 1
#define CGroupName 			 2
#define CGroupNumber 		 3
#define CIPV4NodeName		 4
#define CIPV4NodeAddr		 5
#define CIPV4NodeMAC			 6
#define CIPV6NodeName 		 7
#define CIPV6NodeAddr		 8
#define CNetworkName			 9
#define CNetworkIPAddress	10
#define CServiceName			11
#define CServiceNumber		12
#define CProtocolName		13
#define CProtocolNumber		14
#define CRpcName				15
#define CRpcNumber			16
#define CMountName			17
#define CPrinterName			18
#define CBootparamName		19
#define CBootpName			20
#define CBootpIPAddress		21
#define CBootpENAddress		22
#define CAliasName			23
#define CNetgroupName		24
#define CInitgroupName		25

LUCategory cacheCategory[] =
{
	LUCategoryUser,
	LUCategoryUser,
	LUCategoryGroup,
	LUCategoryGroup,
	LUCategoryHost,
	LUCategoryHost,
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
	LUCategoryNetgroup,
	LUCategoryInitgroups
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
	"host (IPV6 by name)",
	"host (by IPV6 address)",
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
	"netgroup",
	"grouplist"
};

int cacheForCategory[] =
{
	CUserName,
	CGroupName,
	CIPV4NodeName,
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
	CInitgroupName,
	-1
};

#define forever for(;;)

static CacheAgent *_sharedCacheAgent = nil;

@implementation CacheAgent

- (void)ageCache:(unsigned int)n
{
	int i, len;
	int expired, expireAll;
	time_t age;
	time_t ttl;
	LUDictionary *item;
	LUCategory cat;
	LUCache *cache;
	LUArray *exlist;

	if (n >= NCACHE) return;

	cat = cacheCategory[n];

	if (cat >= NCATEGORIES) return;
 
	cache = cacheStore[n].cache;

	exlist = [[LUArray alloc] init];

	expired = 0;
	expireAll = 0;

	len = [cache count];
	for (i = 0; i < len; i++)
	{
		item = [cache objectAtIndex:i];
		if (item == nil) continue;

		ttl = [item timeToLive];
		age = [item age];

		if ((age > ttl) && (![exlist containsObject:item]))
		{
			[exlist addObject:item];
			expired++;
		}
	}

	len = [exlist count];
	for (i = 0; i < len; i++)
	{
		item = [exlist objectAtIndex:i];
		[cache removeObject:item];
	}

	[exlist release];

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
				if (streq(order[j], "Cache") || streq(order[j], "CacheAgent"))
					cEnable = YES;
			}
		}

		[self setCacheIsValidated:cValidation forCategory:(LUCategory)i];
		[self setCacheIsEnabled:cEnable forCategory:(LUCategory)i];
		[self setCapacity:cMax forCategory:(LUCategory)i];
		[self setTimeToLive:cTTL forCategory:(LUCategory)i];
	}

	cserver = (LUServer *)[[LUServer alloc] init];

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
	LUServer *s;

	for (i = 0; i < NCACHE; i++)
	{
		if (cacheStore[i].cache != nil) [cacheStore[i].cache release];
	}

	for (i = 0; i < NCATEGORIES; i++)
	{
		if (allStore[i].all != nil) [allStore[i].all release];
	}

	syslock_free(cacheLock);

	s = (LUServer *)cserver;
	[s release];

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
	LUServer *s;
	char *name;

	if (item == nil) return NO;

	name = [item valueForKey:"_lookup_agent"];
	if (name == NULL) return NO;

	s = (LUServer *)cserver;
	agent = [s agentNamed:name];
	if (agent == nil) return NO;

	return [agent isValid:item];
}

- (LUDictionary *)postProcess:(LUDictionary *)item
	cache:(unsigned int)n
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

	if (array == nil) return NO;
	len = [array validationStampCount];
	if (len == 0) return NO;

	for (i = 0; i < len; i++)
	{
		stamp = [array validationStampAtIndex:i];
		if (stamp == nil) return NO;
		age = [stamp age];
		if (age > [stamp timeToLive]) return NO;
		if (![self isValid:stamp]) return NO;
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

	ttl = [self timeToLiveForCategory:cat];
	[self setTimeToLive:ttl forArray:array];

	syslock_lock(cacheLock);
	if (allStore[cat].all != nil) [allStore[cat].all release];
	allStore[cat].all = [array retain];
	syslock_unlock(cacheLock);
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
			if (streq(key, "name")) return CIPV4NodeName;
			if (streq(key, "namev6")) return CIPV6NodeName;
			if (streq(key, "ip_address")) return CIPV4NodeAddr;
			if (streq(key, "ipv6_address")) return CIPV6NodeAddr;
			if (streq(key, "en_address")) return CIPV4NodeMAC;
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

		case LUCategoryInitgroups: return CInitgroupName;

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
	if (!(cacheNum == CIPV4NodeMAC || cacheNum == CBootpENAddress)) return;
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
	numbers = [item valuesForKey:"port"];
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
	nnumbers = [item countForKey:"port"];
	if (nnumbers < 0) nnumbers = 0;

	nprotocols = [item countForKey:"protocol"];
	if (nprotocols < 0) nprotocols = 0;

	[item setTimeToLive:cacheStore[CServiceName].ttl];
	[item setCacheHits:0];

	for (i = 0; i < nnames; i++)
	{
		[nameCache setObject:item forKey:names[i]];

		for (j = 0; j < nprotocols; j++)
		{
			sprintf(str, "%s/%s", names[i], protocols[j]);
			[nameCache setObject:item forKey:str];
		}
	}

	for (i = 0; i < nnumbers; i++)
	{
		sprintf(str, "%s", numbers[i]);
		[numberCache setObject:item forKey:str];

		for (j = 0; j < nprotocols; j++)
		{
			sprintf(str, "%s/%s", numbers[i], protocols[j]);
			[numberCache setObject:item forKey:str];
		}
	}
}

- (void)addObject:(LUDictionary *)item key:(char *)key category:(LUCategory)cat
{
	if (item == nil) return;
	if (key == NULL) return;

	syslock_lock(cacheLock);

	switch (cat)
	{
		case LUCategoryUser:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CUserName key:"name"];
				[self addObject:item category:cat toCache:CUserName key:"realname"];
			}
			else if (streq(key, "uid"))
			{
				[self addObject:item category:cat toCache:CUserNumber key:"uid"];
			}
			break;
		case LUCategoryGroup:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CGroupName key:"name"];
			}
			else if (streq(key, "gid"))
			{
				[self addObject:item category:cat toCache:CGroupNumber key:"gid"];
			}
			break;
		case LUCategoryHost:
			if (streq(key, "name"))
			{
				if ([item valuesForKey:"ip_address"] != NULL)
				{
					[self addObject:item category:cat toCache:CIPV4NodeName key:"name"];
					[self addEthernetObject:item category:cat toCache:CIPV4NodeMAC];
				}
				if ([item valuesForKey:"ipv6_address"] != NULL)
				{
					[self addObject:item category:cat toCache:CIPV6NodeName key:"name"];
				}
			}
			else if (streq(key, "ip_address"))
			{
				[self addObject:item category:cat toCache:CIPV4NodeAddr key:"ip_address"];
				[self addEthernetObject:item category:cat toCache:CIPV4NodeMAC];
			}
			else if (streq(key, "ipv6_address"))
			{
				[self addObject:item category:cat toCache:CIPV6NodeAddr key:"ipv6_address"];
			}
			break;
		case LUCategoryNetwork:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CNetworkName key:"name"];
			}
			else if (streq(key, "address"))
			{
				[self addObject:item category:cat toCache:CNetworkIPAddress key:"address"];
			}
			break;
		case LUCategoryService:
			[self addService:item];
			break;
		case LUCategoryProtocol:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CProtocolName key:"name"];
			}
			else if (streq(key, "number"))
			{
				[self addObject:item category:cat toCache:CProtocolNumber key:"number"];
			}
			break;
		case LUCategoryRpc:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CRpcName key:"name"];
			}
			else if (streq(key, "number"))
			{
				[self addObject:item category:cat toCache:CRpcNumber key:"number"];
			}
			break;
		case LUCategoryMount:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CMountName key:"name"];
			}
			break;
		case LUCategoryPrinter:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CPrinterName key:"name"];
			}
			break;
		case LUCategoryBootparam:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CBootparamName key:"name"];
			}
			break;
		case LUCategoryBootp:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CBootpName key:"name"];
				[self addEthernetObject:item category:cat toCache:CBootpENAddress];
			}
			else if (streq(key, "ip_address"))
			{
				[self addObject:item category:cat toCache:CBootpIPAddress key:"ip_address"];
				[self addEthernetObject:item category:cat toCache:CBootpENAddress];
			}
			break;
		case LUCategoryAlias:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CAliasName key:"name"];
			}
			break;
		case LUCategoryNetgroup:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CNetgroupName key:"name"];
			}
			break;
		case LUCategoryInitgroups:
			if (streq(key, "name"))
			{
				[self addObject:item category:cat toCache:CInitgroupName key:"name"];
			}
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
			[cacheStore[CUserName].cache removeObject:item];
			[cacheStore[CUserNumber].cache removeObject:item];
			break;
		case LUCategoryGroup:
			[cacheStore[CGroupName].cache removeObject:item];
			[cacheStore[CGroupNumber].cache removeObject:item];
			break;
		case LUCategoryHost:
			[cacheStore[CIPV4NodeName].cache removeObject:item];
			[cacheStore[CIPV4NodeAddr].cache removeObject:item];
			[cacheStore[CIPV6NodeName].cache removeObject:item];
			[cacheStore[CIPV6NodeAddr].cache removeObject:item];
			[cacheStore[CIPV4NodeMAC].cache removeObject:item];
			break;
		case LUCategoryNetwork:
			[cacheStore[CNetworkName].cache removeObject:item];
			[cacheStore[CNetworkIPAddress].cache removeObject:item];
			break;
		case LUCategoryService:
			[cacheStore[CServiceName].cache removeObject:item];
			[cacheStore[CServiceNumber].cache removeObject:item];
			break;
		case LUCategoryProtocol:
			[cacheStore[CProtocolName].cache removeObject:item];
			[cacheStore[CProtocolNumber].cache removeObject:item];
			break;
		case LUCategoryRpc:
			[cacheStore[CRpcName].cache removeObject:item];
			[cacheStore[CRpcNumber].cache removeObject:item];
			break;
		case LUCategoryMount:
			[cacheStore[CMountName].cache removeObject:item];
			break;
		case LUCategoryPrinter:
			[cacheStore[CPrinterName].cache removeObject:item];
			break;
		case LUCategoryBootparam:
			[cacheStore[CBootparamName].cache removeObject:item];
			break;
		case LUCategoryBootp:
			[cacheStore[CBootpName].cache removeObject:item];
			[cacheStore[CBootpIPAddress].cache removeObject:item];
			[cacheStore[CBootpENAddress].cache removeObject:item];
			break;
		case LUCategoryAlias:
			[cacheStore[CAliasName].cache removeObject:item];
			break;
		case LUCategoryNetgroup:
			[cacheStore[CNetgroupName].cache removeObject:item];
			break;
		case LUCategoryInitgroups:
			[cacheStore[CInitgroupName].cache removeObject:item];
			break;
		default: break;
	}

	syslock_unlock(cacheLock);
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
	}

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

- (LUDictionary *)serviceWithName:(char *)name protocol:(char *)prot
{
	LUDictionary *item;
	char *str;

	if (name == NULL) return nil;

	str = NULL;
	if (prot == NULL) asprintf(&str, "%s", name);
	else asprintf(&str, "%s/%s", name, prot);
	if (str == NULL) return nil;

	syslock_lock(cacheLock);
	item = [cacheStore[CServiceName].cache objectForKey:str];
	item = [self postProcess:item cache:CServiceName];
	syslock_unlock(cacheLock);

	free(str);

	return item;
}

- (LUDictionary *)serviceWithNumber:(int *)number protocol:(char *)prot
{
	LUDictionary *item;
	char *str;
	
	if (number == NULL) return nil;
	
	str = NULL;
	if (prot == NULL) asprintf(&str, "%d", *number);
	else asprintf(&str, "%d/%s", *number, prot);
	if (str == NULL) return nil;
	
	syslock_lock(cacheLock);
	item = [cacheStore[CServiceNumber].cache objectForKey:str];
	item = [self postProcess:item cache:CServiceNumber];
	syslock_unlock(cacheLock);
	
	free(str);
	
	return item;
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
	item = [cacheStore[n].cache objectForKey:val];
	item = [self postProcess:item cache:n];
	syslock_unlock(cacheLock);

	return item;
}

- (unsigned int)memorySize
{
	unsigned int size;

	size = [super memorySize];

	size += 24;
	if (cacheLock != NULL) size += sizeof(syslock);

	size += (NCACHE * 20);
	size += (NCATEGORIES * 12);

	return size;
}

- (LUDictionary *)allGroupsWithUser:(char *)name
{
	return [self itemWithKey:"name" value:name category:LUCategoryInitgroups];
}

- (void)setInitgroups:(LUDictionary *)item forUser:(char *)name
{
	syslock_lock(cacheLock);
	[self addObject:item category:LUCategoryInitgroups toCache:CInitgroupName key:"name"];
	syslock_unlock(cacheLock);
}

@end
