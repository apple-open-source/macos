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
 * NIAgent.m
 *
 * NetInfo lookup agent for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/system_log.h>
#import "NIAgent.h"
#import "Config.h"
#import "LUPrivate.h"
#import <NetInfo/dsutil.h>
#import <stdlib.h>
#import <stdio.h>
#import <string.h>

static NIAgent *_sharedNIAgent = nil;
static LUArray *_domainStore = nil;
static char **_domainNames;
static unsigned long timeout = 0;

#define NI_TIMEOUT 30

@implementation NIAgent

/* Domain cache is maintained by the class */
+initialize
{
	if (_domainStore == nil)
	{
		_domainStore = [[LUArray alloc] init];
		[_domainStore setBanner:"NIAgent domain store"];
		_domainNames = NULL;
	}
	return self;
}

+ (ni_status)findDirectory:(char *)path domain:(void **)dom nidir:(ni_id *)nid
{
	void *d, *p;
	ni_id n, root;
	ni_status status;
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	root.nii_object = 0;

	*dom = NULL;
	nid->nii_object = NI_INDEX_NULL;
	nid->nii_instance = NI_INDEX_NULL;
	
	syslock_lock(rpcLock);
	d = ni_connect(&addr, "local");
	syslock_unlock(rpcLock);

	if (d == NULL) return NI_FAILED;
	
	syslock_lock(rpcLock);	
	status = ni_self(d, &root);
	syslock_unlock(rpcLock);

	if (status != NI_OK) return status;

	ni_setabort(d, 1);
	ni_setreadtimeout(d, NI_TIMEOUT);

	while (d != NULL)
	{
		syslock_lock(rpcLock);	
		status = ni_pathsearch(d, &n, path);
		syslock_unlock(rpcLock);

		if (status == NI_OK)
		{
			*dom = d;
			*nid = n;
			return NI_OK;
		}
	
		syslock_lock(rpcLock);	
		status = ni_open(d, "..", &p);
		if (status == NI_OK) status = ni_self(p, &root);
		syslock_unlock(rpcLock);
		ni_free(d);
		d = NULL;

		if (status == NI_OK) d = p;
	}
	
	return NI_NODIR;
}

+ (LUNIDomain *)domainWithName:(char *)domainName
{
	LUNIDomain *d;
	int i, len;

	len = listLength(_domainNames);
	for (i = 0; i < len; i++)
	{
		if (streq(domainName, _domainNames[i]))
			return [[_domainStore objectAtIndex:i] retain];
	}

	d = [[LUNIDomain alloc] initWithDomainNamed:domainName];
	if (d != nil)
	{
		[_domainStore addObject:d];
		_domainNames = appendString(domainName, _domainNames);
	}

	return d;
}

+ (void)releaseDomainStore
{
	[_domainStore release];
	_domainStore = nil;
	freeList(_domainNames);
	_domainNames = NULL;
}

- (void)_setupChain:(LUArray *)c fromConfig:(LUDictionary *)config
{
	char **order;
	LUNIDomain *d, *p;
	int i, len;

	/* Set up DomainOrder */
	if (config == nil)
	{
		order = NULL;
		len = 0;
	}
	else
	{
		order = [config valuesForKey:"DomainOrder"];
		if (order == NULL) len = 0;
		else len = listLength(order);
	}

	if (len == 0)
	{
		/* Only default to standard lookup for global config */
		if (c != globalChain) return;

		/* use plain local->root order */
		d = [NIAgent domainWithName:"."];

		while (d != nil)
		{
			[c addObject:d];
			p = [[d parent] retain];
			[d release];

			d = p;
			if (d != nil)
			{
				[_domainStore addObject:d];
				_domainNames = appendString((char *)[d name], _domainNames);
			}
		}

	}
	else
	{
		for (i = 0; i < len; i++)
		{
			d = [NIAgent domainWithName:order[i]];
			[c addObject:d];
			[d release];
		}
	}
}

- (NIAgent *)init
{
	int i;
	LUDictionary *config;
	char str[256];

	if (didInit) return self;
	
	[super init];

	stats = [[LUDictionary alloc] init];
	[stats setBanner:"NIAgent statistics"];
	[stats setValue:"NetInfo" forKey:"information_system"];
	threadLock = syslock_new(1);

	config = [configManager configGlobal];
	timeout = [configManager intForKey:"Timeout" dict:config default:30];
	if (config != nil) [config release];

	config = [configManager configForAgent:"NIAgent"];
	timeout = [configManager intForKey:"Timeout" dict:config default:timeout];

	/* Set DomainOrder */
	globalChain = [[LUArray alloc] init];
	[globalChain setBanner:"NIAgent global lookup chain"];

	[self _setupChain:globalChain fromConfig:config];
	if (config != nil) [config release];

	for (i = 0; i < NCATEGORIES; i++)
	{
		config = [configManager configForAgent:"NIAgent" category:(LUCategory)i];
		chain[i] = [[LUArray alloc] init];
		sprintf(str, "NIAgent %s lookup chain", [LUAgent categoryPathname:(LUCategory)i]);
		[chain[i] setBanner:str];
		[self _setupChain:chain[i] fromConfig:config];
		if (config != nil) [config release];
	}

	return self;
}

+ (NIAgent *)alloc
{
	if (_sharedNIAgent != nil)
	{
		[_sharedNIAgent retain];
		return _sharedNIAgent;
	}

	_sharedNIAgent = [super alloc];
	[_sharedNIAgent init];
	if (_sharedNIAgent == nil) return nil;

	system_log(LOG_DEBUG, "Allocated NIAgent 0x%08x\n", (int)_sharedNIAgent);

	return _sharedNIAgent;
}

- (void)dealloc
{
	int i;

	for (i = 0; i < NCATEGORIES; i++)
		if (chain[i] != nil) [chain[i] release];
	if (globalChain != nil) [globalChain release];
	if (stats != nil) [stats release];
	if (threadLock != NULL) syslock_free(threadLock);
	threadLock = NULL;
	
	system_log(LOG_DEBUG, "Deallocated NIAgent 0x%08x\n", (int)self);

	[super dealloc];
}

- (const char *)serviceName
{
	return "NetInfo";
}

- (const char *)shortName
{
	return "NI";
}

- (void)setMaxChecksumAge:(time_t)age
{
	int i, j, len;

	for (j = 0; j < NCATEGORIES; j++)
	{
		len = [chain[j] count];
		for (i = 0; i < len; i++)
			[[chain[j] objectAtIndex:i] setMaxChecksumAge:age];
	}
}


- (unsigned int)indexOfDomain:(LUNIDomain *)domain
{
	unsigned int i, len;

	if (domain == nil) return IndexNull;

	len = [_domainStore count];
	for (i = 0; i < len; i++)
	{
		if (domain == [_domainStore objectAtIndex:i]) return i;
	}

	return IndexNull;
}

- (LUNIDomain *)domainAtIndex:(unsigned int)where
{
	if (where >= listLength(_domainNames)) return nil;
	return [_domainStore objectAtIndex:where];
}
	
- (LUDictionary *)statistics
{
	LUNIDomain *d;
	int i, len;
	char key[256];

	syslock_lock(threadLock);
	len = listLength(_domainNames);
	for (i = 0; i < len; i++)
	{
		d = [_domainStore objectAtIndex:i];
		sprintf(key, "%d_domain", i);
		[stats setValue:(char *)_domainNames[i] forKey:key];
		sprintf(key, "%d_server", i);
		[stats setValue:[d currentServer] forKey:key];
	}

	syslock_unlock(threadLock);
	return stats;
}

- (void)resetStatistics
{
	if (stats != nil) [stats release];
	stats = [[LUDictionary alloc] init];
	[stats setBanner:"NIAgent statistics"];
	[stats setValue:"NetInfo" forKey:"information_system"];
}

- (LUDictionary *)stamp:(LUDictionary *)item
	domain:(LUNIDomain *)d
{
	char str[32];

	[item setAgent:self];
	[item setValue:"NetInfo" forKey:"_lookup_info_system"];
	[item setValue:(char *)[d name] forKey:"_lookup_NI_domain"];
	[item setValue:[d currentServer] forKey:"_lookup_NI_server"];
	sprintf(str, "%u", [self indexOfDomain:d]);
	[item setValue:str forKey:"_lookup_NI_index"];
	sprintf(str, "%lu", [d currentChecksum]);
	[item setValue:str forKey:"_lookup_NI_checksum"];
	return item;
}

- (void)allStamp:(LUArray *)all
	domain:(LUNIDomain *)d
	addToList:(LUArray *)list
{
	int i, len;
	char *dname;
	char *sname;
	char index[32], csum[32];
	LUDictionary * item;

	if (all == nil) return;

	dname = copyString((char *)[d name]);
	sname = copyString([d currentServer]);
	sprintf(index, "%u", [self indexOfDomain:d]);
	sprintf(csum, "%lu", [d currentChecksum]);

	len = [all count];
	for (i = 0; i < len; i++)
	{
		item = [all objectAtIndex:i];
		[item setAgent:self];
		[item setValue:"NetInfo" forKey:"_lookup_info_system"];
		[item setValue:dname forKey:"_lookup_NI_domain"];
		[item setValue:sname forKey:"_lookup_NI_server"];
		[item setValue:index forKey:"_lookup_NI_index"];
		[item setValue:csum forKey:"_lookup_NI_checksum"];

		[list addObject:item];
	}
	freeString(dname);
	dname = NULL;
	freeString(sname);
	sname = NULL;
}

- (BOOL)isValid:(LUDictionary *)item
{
	unsigned long oldsum, newsum;
	char *c;
	LUNIDomain *d;

	if (item == nil) return NO;
	c = [item valueForKey:"_lookup_NI_checksum"];
	if (c == NULL) return NO;
	sscanf(c, "%lu", &oldsum);

	c = [item valueForKey:"_lookup_NI_index"];
	if (c == NULL) return NO;
	d = [self domainAtIndex:atoi(c)];
	if (d == nil) return NO;

	syslock_lock(threadLock);
	newsum = [d checksum];
	syslock_unlock(threadLock);

	if (oldsum != newsum) return NO;
	return YES;
}

/*
 * These methods do NetInfo lookups on behalf of all calls
 */

- (LUDictionary *)netgroupWithName:(char *)name
{
	LUDictionary *item;
	LUDictionary *itemInDomain;
	LUNIDomain *d;
	BOOL found;
	unsigned int i, len;
	LUArray *lookupChain;

	found = NO;
	item = [[LUDictionary alloc] init];

	syslock_lock(threadLock);
	len = [chain[(int)LUCategoryNetgroup] count];
	if (len > 0)
	{
		lookupChain = chain[(int)LUCategoryNetgroup];
	}
	else
	{
		lookupChain = globalChain;
		len = [lookupChain count];
	}

	for (i = 0; i < len; i++)
	{
		d = [lookupChain objectAtIndex:i];
		itemInDomain = [d netgroupWithName:name];
		if (itemInDomain != nil)
		{
			found = YES;
			[self mergeNetgroup:itemInDomain into:item];
			[itemInDomain release];
		}
	}

	syslock_unlock(threadLock);

	if (found) return item;
	[item release];
	return nil;
}

- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	LUDictionary *item = nil;
	LUNIDomain *d;
	unsigned int i, len;
	LUArray *lookupChain;

	if (cat == LUCategoryNetgroup)
	{
		return [self netgroupWithName:val];
	}

	syslock_lock(threadLock);
	len = [chain[(int)cat] count];
	if (len > 0)
	{
		lookupChain = chain[(int)cat];
	}
	else
	{
		lookupChain = globalChain;
		len = [lookupChain count];
	}

	for (i = 0; i < len; i++)
	{
		d = [lookupChain objectAtIndex:i];

		item = [d itemWithKey:key value:val category:cat];

		if (item != nil)
		{
			[self stamp:item domain:d];
			syslock_unlock(threadLock);
			return item;
		}
	}

	syslock_unlock(threadLock);
	return nil;
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	LUArray *all;
	LUArray *allInDomain;
	LUNIDomain *d;
	LUDictionary *vstamp;
	unsigned int i, len;
	char scratch[256];
	LUArray *lookupChain;

	all = [[LUArray alloc] init];
	sprintf(scratch, "NIAgent: all %s", [LUAgent categoryName:cat]);
	[all setBanner:scratch];

	syslock_lock(threadLock);
	len = [chain[(int)cat] count];
	if (len > 0)
	{
		lookupChain = chain[(int)cat];
	}
	else
	{
		lookupChain = globalChain;
		len = [lookupChain count];
	}

	for (i = 0; i < len; i++)
	{
		d = [lookupChain objectAtIndex:i];

		vstamp = [[LUDictionary alloc] init];
		[vstamp setBanner:"NIAgent validation stamp"];
		[all addValidationStamp:[self stamp:vstamp domain:d]];
		[vstamp release];

		allInDomain = [d allItemsWithCategory:cat];

		[self allStamp:allInDomain domain:d addToList:all];
		if (allInDomain != nil) [allInDomain release];
	}
	syslock_unlock(threadLock);
	return all;
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat key:(char *)key value:(char *)val
{
	LUArray *all;
	LUArray *allInDomain;
	LUNIDomain *d;
	unsigned int i, j, n, len;
	LUArray *lookupChain;

	all = [[LUArray alloc] init];

	syslock_lock(threadLock);
	len = [chain[(int)cat] count];
	if (len > 0)
	{
		lookupChain = chain[(int)cat];
	}
	else
	{
		lookupChain = globalChain;
		len = [lookupChain count];
	}

	for (i = 0; i < len; i++)
	{
		d = [lookupChain objectAtIndex:i];
		allInDomain = [d allEntitiesForCategory:cat key:key value:val selectedKeys:NULL];
		if (allInDomain == nil) continue;

		n = [allInDomain count];
		for (j = 0; j < n; j++)
		{
			[all addObject:[allInDomain objectAtIndex:j]];
		}
		[allInDomain release];
	}
	syslock_unlock(threadLock);
	return all;
}

- (LUArray *)query:(LUDictionary *)pattern category:(LUCategory)cat
{
	LUArray *all, *list;
	char *key, *val;
	unsigned int where, len;

	if (pattern == nil) return [self allItemsWithCategory:cat];
	len = [pattern count];
	if (len == 0) return [self allItemsWithCategory:cat];

	/*
	 * Find a key with a non-null value.
	 * Ignore "_lookup_*" keys.
	 */
	
	key = NULL;
	val = NULL;
	where = 0;

	while (key == NULL)
	{
		key = [pattern keyAtIndex:where];
		if (strncmp(key, "_lookup_", 8))
		{
			/* A "real" key.  Check the value. */
			val = [pattern valueAtIndex:where];
			if (val != NULL) break;
		}

		key = NULL;
		where++;
		if (where >= len) break;
	}
	if (key == NULL) return [self allItemsWithCategory:cat];

	all = [self allItemsWithCategory:cat key:key value:val];
	
	list = nil;
	if (all != nil)
	{
		list = [all filter:pattern];
		[all release];
	}

	return list;
}

- (LUArray *)allGroupsWithUser:(char *)name
{
	LUArray *all;
	LUArray *allInDomain;
	LUNIDomain *d;
	LUDictionary *vstamp;
	unsigned int i, len;
	unsigned int j, jlen;
	LUArray *lookupChain;

	all = [[LUArray alloc] init];
	syslock_lock(threadLock);
	len = [chain[(int)LUCategoryInitgroups] count];
	if (len > 0)
	{
		lookupChain = chain[(int)LUCategoryInitgroups];
	}
	else
	{
		lookupChain = globalChain;
		len = [lookupChain count];
	}

	for (i = 0; i < len; i++)
	{
		d = [lookupChain objectAtIndex:i];

		vstamp = [[LUDictionary alloc] init];
		[vstamp setBanner:"NIAgent validation stamp"];
		[all addValidationStamp:[self stamp:vstamp domain:d]];
		[vstamp release];

		allInDomain = [d allGroupsWithUser:name];
		jlen = 0;
		if (allInDomain != nil) jlen = [allInDomain count];
		for (j = 0; j < jlen; j++)
		{
			[all addObject:
				[self stamp:[allInDomain objectAtIndex:j] domain:d]];
		}
		if (allInDomain != nil) [allInDomain release];
	}
	syslock_unlock(threadLock);
	return all;
}

- (LUDictionary *)serviceWithName:(char *)name
	protocol:(char *)prot
{
	LUDictionary *item = nil;
	LUNIDomain *d;
	unsigned int i, len;
	LUArray *lookupChain;

	syslock_lock(threadLock);
	len = [chain[(int)LUCategoryService] count];
	if (len > 0)
	{
		lookupChain = chain[(int)LUCategoryService];
	}
	else
	{
		lookupChain = globalChain;
		len = [lookupChain count];
	}

	for (i = 0; i < len; i++)
	{
		d = [lookupChain objectAtIndex:i];
		item = [d serviceWithName:name protocol:prot];
		if (item != nil)
		{
			[self stamp:item domain:d];
			syslock_unlock(threadLock);
			return item;
		}
	}

	syslock_unlock(threadLock);
	return nil;
}

- (LUDictionary *)serviceWithNumber:(int *)number
	protocol:(char *)prot
{
	LUDictionary *item = nil;
	LUNIDomain *d;
	unsigned int i, len;
	LUArray *lookupChain;

	syslock_lock(threadLock);
	len = [chain[(int)LUCategoryService] count];
	if (len > 0)
	{
		lookupChain = chain[(int)LUCategoryService];
	}
	else
	{
		lookupChain = globalChain;
		len = [lookupChain count];
	}

	for (i = 0; i < len; i++)
	{
		d = [lookupChain objectAtIndex:i];
		item = [d serviceWithNumber:number protocol:prot];
		if (item != nil)
		{
			[self stamp:item domain:d];
			syslock_unlock(threadLock);
			return item;
		}
	}

	syslock_unlock(threadLock);
	return nil;
}

/*
 * Custom lookups 
 */
- (BOOL)isSecurityEnabledForOption:(char *)option
{
	LUNIDomain *d;
	unsigned int i, len;

	syslock_lock(threadLock);
	len = [globalChain count];
	for (i = 0; i < len; i++)
	{
		d = [globalChain objectAtIndex:i];
		if ([d isSecurityEnabledForOption:option])
		{
			syslock_unlock(threadLock);
			return YES;
		}
	}

	syslock_unlock(threadLock);
	return NO;
}

- (BOOL)isNetwareEnabled
{
	LUNIDomain *d;
	unsigned int i, len;

	syslock_lock(threadLock);
	len = [globalChain count];
	for (i = 0; i < len; i++)
	{
		d = [globalChain objectAtIndex:i];
		if ([d checkNetwareEnabled])
		{
			syslock_unlock(threadLock);
			return YES;
		}
	}

	syslock_unlock(threadLock);
	return NO;
}

@end
