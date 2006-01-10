/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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

#define forever for(;;)

const char *ccatname = "ughnsprmPbBadeNiS";

static CacheAgent *_sharedCacheAgent = nil;

@implementation CacheAgent

void
cache_object_release(void *d)
{
	id obj;

	if (d == NULL) return;
	obj = d;

	[obj release];
}

void
cache_object_retain(void *d)
{
	id obj;

	if (d == NULL) return;
	obj = d;

	[obj retain];
}

- (time_t)minTimeToLive
{
	int i;
	time_t min;
 
	min = cacheParams[0].ttl;
	for (i = 1; i < NCATEGORIES; i++) if (cacheParams[i].ttl < min) min = cacheParams[i].ttl;
	return min;
}

- (void)sweepCache
{
	time_t now;
	unsigned int delta;

	now = time(NULL);
	delta = now - lastSweep;

	if (delta < sweepTime) return;
	lastSweep = now;

	syslock_lock(cacheLock);
	cache_sweep(cache);
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
	cacheParams[cat].validate = validate;
}

- (void)setTimeToLive:(time_t)timeout forCategory:(LUCategory)cat
{
	cacheParams[cat].ttl = timeout;
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
	cacheParams[cat].enabled = enabled;
}

- (CacheAgent *)init
{
	int i, j;
	char **order;
	time_t now;
	LUDictionary *global, *config;
	BOOL gValidation, cValidation, gEnable, cEnable;
	uint32_t gMax, cMax, cBits;
	time_t gTTL, cTTL;

	if (didInit) return self;

	[super init];

	now= time(NULL);
	lastSweep = now;
	sweepTime = [self minTimeToLive];
	if (sweepTime < 60) sweepTime = 60;
	cacheLock = syslock_new(1);

	for (i = 0; i < NCATEGORIES; i++)
	{
		cacheParams[i].ttl = 43200;
		cacheParams[i].validate = YES;
		cacheParams[i].enabled = NO;
	}

	global = [configManager configGlobal:configurationArray];
	cBits = [configManager intForKey:"CacheBits" dict:global default:13];
	if (cBits > 16) cBits = 16;

	cache_size = 1 << cBits;

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
		[self setTimeToLive:cTTL forCategory:(LUCategory)i];
	}

	cache = cache_new(cache_size, 0);
	cache_set_retain_callback(cache, cache_object_retain);
	cache_set_release_callback(cache, cache_object_release);
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

	system_log(LOG_DEBUG, "Allocated CacheAgent 0x%08x", (int)_sharedCacheAgent);

	return _sharedCacheAgent;
}

- (void)dealloc
{
	LUServer *s;

	cache_free(cache);
	syslock_free(cacheLock);

	s = (LUServer *)cserver;
	[s release];

	system_log(LOG_DEBUG, "Deallocated CacheAgent 0x%08x", (int)self);

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

- (LUDictionary *)postProcess:(LUDictionary *)item category:(LUCategory)cat
{
	unsigned int hits;

	if (item == nil) return nil;

	if (cacheParams[cat].validate)
	{
		if (![self isValid:item])
		{
			cache_delete_datum(cache, item);
			return nil;
		}
	}

	hits = [item cacheHit];

	/* Retain the object here.  Caller must release. */
	[item retain];
	return item;
}

- (BOOL)isArrayValid:(LUArray *)array
{
	unsigned int i, len;
	LUDictionary *stamp;

	if (array == nil) return NO;
	len = [array validationStampCount];
	if (len == 0) return NO;

	for (i = 0; i < len; i++)
	{
		stamp = [array validationStampAtIndex:i];
		if (stamp == nil) return NO;
		if (![self isValid:stamp]) return NO;
	}
	return YES;
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	LUArray *all;
	char *str;

	if (cat > NCATEGORIES) return nil;

	syslock_lock(cacheLock);

	asprintf(&str, "*:%s", [LUAgent categoryPathname:cat]);
	all = cache_find_reset(cache, str);

	if (all == nil)
	{
		free(str);
		syslock_unlock(cacheLock);
		return nil;
	}

	/* Retain the array here.  Caller must release */
	if (!cacheParams[cat].validate)
	{
		free(str);
		[all retain];
		syslock_unlock(cacheLock);
		return all;
	}

	if ([self isArrayValid:all])
	{
		free(str);
		[all retain];
		syslock_unlock(cacheLock);
		return all;
	}

	cache_delete(cache, str);
	free(str);

	syslock_unlock(cacheLock);
	return nil;
}

- (void)addArray:(LUArray *)array
{
	LUDictionary *stamp;
	LUCategory cat;
	time_t ttl;
	char *str;

	if (array == nil) return;

	stamp = [array validationStampAtIndex:0];
	if (stamp == nil) return;
	cat = [stamp category];
	if (cat >= NCATEGORIES) return;

	ttl = cacheParams[cat].ttl;
	[self setTimeToLive:ttl forArray:array];

	asprintf(&str, "*:%s", [LUAgent categoryPathname:cat]);
	syslock_lock(cacheLock);
	cache_insert_ttl(cache, str, array, ttl);
	syslock_unlock(cacheLock);
	free(str);
}

/*
 * Utilities
 */

/*
 * Add objects to cache 
 * key is ccatname + hc.
 * hc values are:
 * n - name
 * r - realname
 * u - uid
 * g - gid
 * e - en_address
 * i - ip_address
 * a - address (network)
 * p - port (service)
 * 6 - ipv6_address
 * # - number (protocol, rpc)
 * v - namev6 (ipv6-only host)
 */

- (void)addObject:(LUDictionary *)item category:(LUCategory)cat key:(char *)keyName
{
	char **values, *ce, *str, hc;
	int i, len, canon_en;

	if (item == nil) return;
	if (!cacheParams[cat].enabled) return;
	if (keyName == NULL) return;

	canon_en = 0;
	if (streq(keyName, "en_address")) canon_en = 1;

	hc = keyName[0];
	if (streq(keyName, "ipv6_address")) hc = '6';
	else if (streq(keyName, "number")) hc = '#';
	else if (streq(keyName, "namev6")) hc = 'v';

	values = NULL;
	len = 0;

	if (hc == 'v')
	{
		values = [item valuesForKey:"name"];
		len = [item countForKey:"name"];
	}
	else
	{
		values = [item valuesForKey:keyName];
		len = [item countForKey:keyName];
	}

	if ((values == NULL) || (len == 0)) return;

	[item setTimeToLive:cacheParams[cat].ttl];
	[item setCacheHits:0];

	for (i = 0; i < len; i++)
	{
		str = NULL;

		if (canon_en != 0)
		{
			ce = [LUAgent canonicalEthernetAddress:values[i]];
			if (ce == NULL) continue;
			asprintf(&str, "%c%c:%s", ccatname[cat], hc, ce);
			free(ce);
		}
		else
		{
			asprintf(&str, "%c%c:%s", ccatname[cat], hc, values[i]);
		}

		if (str == NULL) continue;

		cache_insert_ttl(cache, str, item, cacheParams[cat].ttl);
		free(str);
	}
}

- (void)addService:(LUDictionary *)item
{
	char **names;
	char **numbers;
	char **protocols;
	int j, nnames, nnumbers;
	int i, nprotocols;
	char str[256];
	time_t now;
	uint32_t ttl;

	if (item == nil) return;
	if (!cacheParams[LUCategoryService].enabled) return;

	names = [item valuesForKey:"name"];
	numbers = [item valuesForKey:"port"];
	protocols = [item valuesForKey:"protocol"];

	if (protocols == NULL) return;

	if (names == NULL) nnames = 0;
	else nnames = [item countForKey:"name"];
	if (nnames < 0) nnames = 0;

	if (numbers == NULL) nnumbers = 0;
	nnumbers = [item countForKey:"port"];
	if (nnumbers < 0) nnumbers = 0;

	nprotocols = [item countForKey:"protocol"];
	if (nprotocols < 0) nprotocols = 0;

	ttl = cacheParams[LUCategoryService].ttl;
	[item setTimeToLive:ttl];
	[item setCacheHits:0];
	now = time(NULL);

	for (i = 0; i < nnames; i++)
	{
		sprintf(str, "sn:%s", names[i]);
		cache_insert_ttl_time(cache, str, item, ttl, now);
	}

	for (i = 0; i < nnumbers; i++)
	{
		sprintf(str, "sp:%s", numbers[i]);
		cache_insert_ttl_time(cache, str, item, ttl, now);
	}

	for (i = 0; i < nprotocols; i++)
	{
		for (j = 0; j < nnames; j++)
		{
			sprintf(str, "sn:%s/%s", names[j], protocols[i]);
			cache_insert_ttl_time(cache, str, item, ttl, now);
		}

		for (j = 0; j < nnumbers; j++)
		{
			sprintf(str, "sp:%s/%s", numbers[j], protocols[i]);
			cache_insert_ttl_time(cache, str, item, ttl, now);
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
		{
			if (streq(key, "name"))
			{
				[self addObject:item category:cat key:"name"];
				[self addObject:item category:cat key:"realname"];
			}
			else if (streq(key, "uid"))
			{
				[self addObject:item category:cat key:"uid"];
			}
			break;
		}

		case LUCategoryGroup:
		{
			if (streq(key, "name"))
			{
				[self addObject:item category:cat key:"name"];
			}
			else if (streq(key, "gid"))
			{
				[self addObject:item category:cat key:"gid"];
			}
			break;
		}

		case LUCategoryHost:
		{
			if ((streq(key, "name")) || (streq(key, "namev46")))
			{
				if ([item valuesForKey:"ip_address"] != NULL)
				{
					[self addObject:item category:cat key:"name"];
					[self addObject:item category:cat key:"en_address"];
				}
				if ([item valuesForKey:"ipv6_address"] != NULL)
				{
					[self addObject:item category:cat key:"namev6"];
				}
			}
			else if (streq(key, "namev6"))
			{
				if ([item valuesForKey:"ipv6_address"] != NULL)
				{
					[self addObject:item category:cat key:"namev6"];
				}
			}
			else if (streq(key, "ip_address"))
			{
				[self addObject:item category:cat key:"ip_address"];
				[self addObject:item category:cat key:"en_address"];
			}
			else if (streq(key, "ipv6_address"))
			{
				[self addObject:item category:cat key:"ipv6_address"];
			}
			break;
		}

		case LUCategoryNetwork:
		{
			if (streq(key, "name"))
			{
				[self addObject:item category:cat key:"name"];
			}
			else if (streq(key, "address"))
			{
				[self addObject:item category:cat key:"address"];
			}
			break;
		}

		case LUCategoryService:
		{
			[self addService:item];
			break;
		}

		case LUCategoryBootp:
		{
			if (streq(key, "name"))
			{
				[self addObject:item category:cat key:"name"];
				[self addObject:item category:cat key:"en_address"];
			}
			else if (streq(key, "ip_address"))
			{
				[self addObject:item category:cat key:"ip_address"];
				[self addObject:item category:cat key:"en_address"];
			}
			break;
		}

		case LUCategoryProtocol:
		case LUCategoryRpc:
		{
			if (streq(key, "name"))
			{
				[self addObject:item category:cat key:"name"];
			}
			else if (streq(key, "number"))
			{
				[self addObject:item category:cat key:"number"];
			}
			break;
		}

		case LUCategoryMount:
		case LUCategoryPrinter:
		case LUCategoryBootparam:
		case LUCategoryAlias:
		case LUCategoryNetgroup:
		case LUCategoryInitgroups:
		{
			if (streq(key, "name"))
			{
				[self addObject:item category:cat key:"name"];
			}
			break;
		}

		default: break;
	}

	syslock_unlock(cacheLock);
}

/*
 * Remove objects from cache
 */

- (void)removeObject:(LUDictionary *)item
{
	if (item == nil) return;

	syslock_lock(cacheLock);
	cache_delete_datum(cache, item);
	syslock_unlock(cacheLock);
}

- (void)flushCache
{
	syslock_lock(cacheLock);
	cache_free(cache);
	cache = cache_new(cache_size, 0);
	cache_set_retain_callback(cache, cache_object_retain);
	cache_set_release_callback(cache, cache_object_release);
	syslock_unlock(cacheLock);
}

- (BOOL)cacheIsEnabledForCategory:(LUCategory)cat
{
	if (cat > NCATEGORIES) return NO;
	return cacheParams[cat].enabled;
}

- (BOOL)containsObject:(id)obj
{
	syslock_lock(cacheLock);

	if (cache_contains_datum(cache, obj))
	{
		syslock_unlock(cacheLock);
		return YES;
	}

	syslock_unlock(cacheLock);

	return NO;
}

- (LUDictionary *)serviceWithName:(char *)name protocol:(char *)prot
{
	LUDictionary *item;
	char *str;

	str = NULL;
	if (prot == NULL) asprintf(&str, "sn:%s", name);
	else asprintf(&str, "sn:%s/%s", name, prot);
	if (str == NULL) return nil;

	syslock_lock(cacheLock);
	item = cache_find_reset(cache, str);
	free(str);
	item = [self postProcess:item category:LUCategoryService];
	syslock_unlock(cacheLock);

	return item;
}

- (LUDictionary *)serviceWithNumber:(int *)number protocol:(char *)prot
{
	LUDictionary *item;
	char *str;

	str = NULL;
	if (prot == NULL) asprintf(&str, "sp:%u", *number);
	else asprintf(&str, "sp:%u/%s", *number, prot);
	if (str == NULL) return nil;

	syslock_lock(cacheLock);
	item = cache_find_reset(cache, str);
	free(str);
	item = [self postProcess:item category:LUCategoryService];
	syslock_unlock(cacheLock);

	return item;
}

- (LUDictionary *)itemWithKey:(char *)key value:(char *)val category:(LUCategory)cat
{
	LUDictionary *item;
	char *str, hc;

	hc = key[0];
	if (streq(key, "ipv6_address")) hc = '6';
	else if (streq(key, "number")) hc = '#';
	else if (streq(key, "namev6")) hc = 'v';

	asprintf(&str, "%c%c:%s", ccatname[cat], hc, val);
	if (str == NULL) return nil;

	syslock_lock(cacheLock);
	item = cache_find_reset(cache, str);
	free(str);
	item = [self postProcess:item category:cat];
	syslock_unlock(cacheLock);

	return item;
}

- (unsigned int)memorySize
{
	unsigned int size;

	size = [super memorySize];

	size += (NCATEGORIES * (sizeof(time_t) + 2 * sizeof(BOOL)));
	size += 24;
	if (cacheLock != NULL) size += sizeof(syslock);

	return size;
}

- (LUDictionary *)allGroupsWithUser:(char *)name
{
	return [self itemWithKey:"name" value:name category:LUCategoryInitgroups];
}

- (void)setInitgroups:(LUDictionary *)item forUser:(char *)name
{
	syslock_lock(cacheLock);
	[self addObject:item category:LUCategoryInitgroups key:"name"];
	syslock_unlock(cacheLock);
}

- (void)print:(FILE *)f
{
	int i;

	syslock_lock(cacheLock);

	fprintf(f, "Cache Agent\n");
	fprintf(f, "    category        ttl validated enabled\n");

	for (i = 0; i < NCATEGORIES; i++)
	{
		fprintf(f, "%12s %10u %s %s\n",
			[LUAgent categoryName:i], (unsigned int)cacheParams[i].ttl,
			cacheParams[i].validate ? "YES      " : "NO       ",
			cacheParams[i].enabled ? "YES" : "NO");
	}

	fprintf(f, "\n");
	cache_print(cache, f);

	syslock_unlock(cacheLock);
}

@end
