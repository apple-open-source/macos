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
 * LUServer.m
 *
 * Lookup server for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/system_log.h>
#import <NetInfo/syslock.h>
#import "LUServer.h"
#import "LUCachedDictionary.h"
#import "LUPrivate.h"
#import "Controller.h"
#import <NetInfo/dsutil.h>
#import <string.h>
#import <stdlib.h>
#import <stdio.h>

#define MaxNetgroupRecursion 5
#define XDRSIZE 8192

#define MICROSECONDS 1000000
#define MILLISECONDS 1000

static unsigned int
milliseconds_since(struct timeval t)
{
	struct timeval now, delta;
	unsigned int millisec;

	gettimeofday(&now, NULL);

	delta.tv_sec = now.tv_sec - t.tv_sec;
	
	if (t.tv_usec > now.tv_usec)
	{
		now.tv_usec += 1000000;
		delta.tv_sec -= 1;
	}

	delta.tv_usec = now.tv_usec - t.tv_usec;

	millisec = ((delta.tv_sec * 1000000) + delta.tv_usec) / 1000;
	return millisec;
}

@implementation LUServer

+ (LUServer *)alloc
{
	id s;

	s = [super alloc];

	system_log(LOG_DEBUG, "Allocated LUServer 0x%08x\n", (int)s);

	return s;
}

- (LUServer *)init
{
	int i;
	char str[128];

	[super init];

	agentClassList = [[LUArray alloc] init];
	[agentClassList setBanner:"LUServer agent class list"];

	agentList = [[LUArray alloc] init];
	[agentList setBanner:"LUServer agent list"];

	for (i = 0; i < NCATEGORIES; i++)
	{
		order[i] = [[LUArray alloc] init];
		sprintf(str, "Lookup order for category %s", [LUAgent categoryName:i]);
		[order[i] setBanner:str];
	}

	[agentClassList addObject:[CacheAgent class]];
	[agentList addObject:cacheAgent];

	system_log(LOG_DEBUG, "LUServer 0x%08x added agent 0x%08x (%s) retain count = %d",
		(int)self, (int)cacheAgent, [cacheAgent shortName],
		[cacheAgent retainCount]);

	ooBufferSize = 0;
	ooBufferOffset = 0;
	ooBuffer = NULL;

	idle = YES;
	state = ServerStateIdle;
	currentAgent = nil;
	currentCall = NULL;

	return self;
}

- (BOOL)isIdle
{
	return idle;
}

/*
 * Called on check out / check in.
 */
- (void)setIsIdle:(BOOL)yn
{
	Thread *t;

	t = [Thread currentThread];

	if (yn)
	{
		if (t != myThread)
		{
			system_log(LOG_ERR, "Thread %s attempted to idle server %x owned by thread %s", [t name], (unsigned int)self, [myThread name]);
			return;
		}

		idle = YES;
		state = ServerStateIdle;
		myThread = nil;

		if (ooBufferSize > 0)
		{
			free(ooBuffer);
			ooBuffer = NULL;
			ooBufferSize = 0;
			ooBufferOffset = 0;
		}
	}
	else
	{
		if (myThread != nil)
		{
			system_log(LOG_ERR, "Thread %s attempted to use server %x owned by thread %s", [t name], (unsigned int)self, [myThread name]);
			return;
		}

		idle = NO;
		state = ServerStateActive;
		myThread = t;
	}
}

- (LUAgent *)agentForSystem:(id)systemClass
{
	int i, len;
	LUAgent *agent;

	len = [agentClassList count];
	for (i = 0; i < len; i++)
	{
		if ([agentClassList objectAtIndex:i] == systemClass)
		{
			return [[agentList objectAtIndex:i] retain];
		}
	}

	agent = [[systemClass alloc] init];
	if (agent == nil) return nil;

	[agentClassList addObject:systemClass];
	[agentList addObject:agent];

	system_log(LOG_DEBUG, "LUServer 0x%08x added agent 0x%08x (%s) retain count = %d",
		(int)self, (int)agent, [agent shortName], [agent retainCount]);

	return agent;
}

/*
 * luOrder must be an array of Class objects
 */
- (void)setLookupOrder:(LUArray *)luOrder
{
	int i, j, len;
	LUAgent *agent;
	BOOL enabled;

	for (i = 0; i < NCATEGORIES; i++) [order[i] releaseObjects];

	if (luOrder == nil)
	{
		for (i = 0; i < NCATEGORIES; i++)
			[cacheAgent setCacheIsEnabled:NO forCategory:(LUCategory)i];
		return;
	}

	len = [luOrder count];
	enabled = NO;

	for (i = 0; i < len; i++)
	{
		agent = [self agentForSystem:[luOrder objectAtIndex:i]];
		if (agent != nil)
		{
			for (j = 0; j < NCATEGORIES; j++) [order[j] addObject:agent];
			if ([agent isMemberOf:[CacheAgent class]]) enabled = YES;
			[agent release];
		}
	}

	for (i = 0; i < NCATEGORIES; i++)
		[cacheAgent setCacheIsEnabled:enabled forCategory:(LUCategory)i];
}

- (void)setLookupOrder:(LUArray *)luOrder forCategory:(LUCategory)cat
{
	int i, len, n;
	LUAgent *agent;
	BOOL enabled;

	n = (unsigned int)cat;
	[order[n] releaseObjects];

	if (luOrder == nil)
	{
		[cacheAgent setCacheIsEnabled:NO forCategory:cat];
		return;
	}

	len = [luOrder count];
	enabled = NO;
	for (i = 0; i < len; i++)
	{
		agent = [self agentForSystem:[luOrder objectAtIndex:i]];
		if (agent != nil)
		{
			[order[n] addObject:agent];
			if ([agent isMemberOf:[CacheAgent class]]) enabled = YES;
			[agent release];
		}
	}

	[cacheAgent setCacheIsEnabled:enabled forCategory:cat];
} 

- (void)copyToOOBuffer:(char *)src size:(unsigned long)len
{
	long avail, delta;

	if (ooBufferSize == 0)
	{
		ooBufferSize = XDRSIZE * ((len / XDRSIZE) + 1);
		ooBuffer = malloc(ooBufferSize);
		ooBufferOffset = 0;
	}
	else
	{
		avail = ooBufferSize - ooBufferOffset;

		if (len > avail) 
		{
			delta = XDRSIZE * (((len - avail) / XDRSIZE) + 1);
			ooBufferSize += delta;
			ooBuffer = realloc(ooBuffer, ooBufferSize);
		}
	}

	memmove(ooBuffer + ooBufferOffset, src, len);
	ooBufferOffset += len;
}

- (char *)ooBuffer
{
	return ooBuffer;
}

- (int)ooBufferLength
{
	return (int)ooBufferOffset;
}

- (void)dealloc
{
	int i, len;
	LUAgent *agent;

	for (i = 0; i < NCATEGORIES; i++)
	{
		if (order[i] != nil) [order[i] release];
	}

	if (agentClassList != nil) [agentClassList release];

	if (agentList != nil)
	{
		len = [agentList count];
		for (i = len - 1; i >= 0; i--)
		{
			agent = [agentList objectAtIndex:i];
			system_log(LOG_DEBUG, "%d: server 0x%08x releasing agent 0x%08x (%s) with retain count = %d", i, (int)self, (int)agent, [agent shortName], [agent retainCount]);
			[agentList removeObject:agent];
		}

		[agentList release];
	}

	free(ooBuffer);

	system_log(LOG_DEBUG, "Deallocated LUServer 0x%08x\n", (int)self);

	[super dealloc];
}

- (void)addTime:(unsigned int)t hit:(BOOL)found forKey:(const char *)key
{
	unsigned long calls, time, hits;
	char str[128], *v;

	if (key == NULL) return;

	calls = 0;
	time = 0;
	hits = 0;

	syslock_lock(statsLock);

	v = [statistics valueForKey:(char *)key];
	if (v != NULL) sscanf(v, "%lu %lu %lu", &calls, &hits, &time);

	calls += 1;
	if (found) hits += 1;
	time += t;

	sprintf(str, "%lu %lu %lu", calls, hits, time);
	[statistics setValue:str forKey:(char *)key];

	syslock_unlock(statsLock);
}

- (void)recordCall:(char *)method time:(unsigned int)t hit:(BOOL)found
{
	/* total of all calls */
	[self addTime:t hit:found forKey:"total"];

	/* total calls of this lookup method */
	[self addTime:t hit:found forKey:method];
}

- (void)recordSearch:(char *)method
	infoSystem:(const char *)info
	time:(unsigned int)t
	hit:(BOOL)found
{
	char key[256];

	/* total for this info system */
	[self addTime:t hit:found forKey:info];

	/* total for this method in this info system */
	sprintf(key, "%s %s", info, method);
	[self addTime:t hit:found forKey:key];
}

- (LUDictionary *)stamp:(LUDictionary *)item
	agent:(LUAgent *)agent
 	category:(LUCategory)cat
{
	BOOL cacheEnabled;
	char scratch[256];

	if (item == nil) return nil;

	cacheEnabled = [cacheAgent cacheIsEnabledForCategory:cat];
	[item setCategory:cat];

	if (strcmp([agent shortName], "Cache"))
	{
		if (cat == LUCategoryBootp)
		{
			sprintf(scratch, "%s: %s %s (%s / %s)",
				[agent shortName],
				[LUAgent categoryName:cat],
				[item valueForKey:"name"],
				[item valueForKey:"en_address"],
				[item valueForKey:"ip_address"]);
		}
		else
		{
			sprintf(scratch, "%s: %s %s",
				[agent shortName],
				[LUAgent categoryName:cat],
				[item valueForKey:"name"]);
		}
		[item setBanner:scratch];
	}

	if (cacheEnabled && (strcmp([agent shortName], "Cache")))
	{
		[cacheAgent addObject:item];
	}

	if ([item isNegative])
	{
		[item release];
		return nil;
	}

	return item;
}

- (unsigned long)state
{
	return state;
}

- (LUAgent *)currentAgent
{
	return currentAgent;
}

- (char *)currentCall
{
	return currentCall;
}

static char *
appendDomainName(char *h, char *d)
{
	int len;
	char *q;

	if (h == NULL) return NULL;
	if (d == NULL) return copyString(h);

	len = strlen(h) + strlen(d) + 2;
	q = malloc(len);
	sprintf(q, "%s.%s", h, d);
	return q;
}

/*
 * Given a host name, returns a list of possible variations
 * based on our DNS domain name / DNS domain search list.
 */
- (char **)hostNameList:(char *)host
{
	char **l, **dns;
	char *p, *s;
	int i, len;

	if (host == NULL) return NULL;

	/* Bail out if we are shutting down (prevents a call to controller) */
	if (shutting_down) return NULL;

	l = NULL;
	l = appendString(host, l);

	dns = [controller dnsSearchList];

	/* If no DNS, list is just (host) */
	if (dns == NULL) return l;

	len = listLength(dns);

	p = strchr(host, '.');
	if (p == NULL)
	{
		/*
		 * Unqualified host name.
		 * Return (host, host.<dns[0]>, host.<dns[1]>, ...)
		 */		 
		for (i = 0; i < len; i++)
		{
			s = appendDomainName(host, dns[i]);
			if (s == NULL) continue;
			l = appendString(s, l);
			free(s);
		}
		return l;
	}

	/*
	 * Hostname is qualified.
	 * If domain is in dns search list, we return (host.domain, host).
	 * Otherwise, return (host.domain).
	 */
	for (i = 0; i < len; i++)
	{
		if (streq(p+1, dns[i]))
		{
			/* Strip domain name, append host to list */
			*p = '\0';
			l = appendString(host, l);
			*p = '.';
			return l;
		}
	}
			
	return l;
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	LUArray *lookupOrder;
	LUArray *all;
	LUArray *sub;
	LUAgent *agent;
	LUDictionary *stamp;
	LUDictionary *item;
	int i, len;
	int j, sublen;
	BOOL cacheEnabled, found;
	char scratch[256], caller[256];
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;

	if (cat >= NCATEGORIES) return nil;
	gettimeofday(&allStart, (struct timezone *)NULL);

	sprintf(caller, "all %s", [LUAgent categoryName:cat]);
	currentCall = caller;
	
	cacheEnabled = [cacheAgent cacheIsEnabledForCategory:cat];
	if (cacheEnabled)
	{
		gettimeofday(&sysStart, (struct timezone *)NULL);
		currentAgent = cacheAgent;
		state = ServerStateQuerying;
		all = [cacheAgent allItemsWithCategory:cat];
		state = ServerStateActive;
		currentAgent = nil;
		sysTime = milliseconds_since(sysStart);
		found = (all != nil);
		[self recordSearch:caller infoSystem:"Cache" time:sysTime hit:found];

		if (found)
		{
			allTime = milliseconds_since(allStart);
			[self recordCall:caller time:allTime hit:found];
			currentCall = NULL;
			return all;
		}
	}

	all = [[LUArray alloc] init];

	lookupOrder = order[(unsigned int)cat];
	len = [lookupOrder count];
	agent = nil;
	for (i = 0; i < len; i++)
	{
		agent = [lookupOrder objectAtIndex:i];
		if (streq([agent shortName], "Cache")) continue;

		gettimeofday(&sysStart, (struct timezone *)NULL);

		currentAgent = agent;
		state = ServerStateQuerying;
		sub = [agent allItemsWithCategory:cat];
		state = ServerStateActive;
		currentAgent = nil;

		sysTime = milliseconds_since(sysStart);
		found = (sub != nil);
		[self recordSearch:caller infoSystem:[agent shortName] time:sysTime hit:found];

		if (found)
		{
			/* Merge validation info from this agent into "all" array */
			sublen = [sub validationStampCount];
			for (j = 0; j < sublen; j++)
			{
				stamp = [sub validationStampAtIndex:j];
				[stamp setCategory:cat];
				[all addValidationStamp:stamp];
			}

			sublen = [sub count];
			for (j = 0; j < sublen; j++)
			{
				item = [sub objectAtIndex:j];
				[item setCategory:cat];
				[all addObject:item];
			}

			[sub release];
		}
	}

	allTime = milliseconds_since(allStart);
	found = ([all count] != 0);
	[self recordCall:caller time:allTime hit:found];

	if (!found)
	{
		[all release];
		currentCall = NULL;
		return nil;
	}

	sprintf(scratch, "LUServer: all %s", [LUAgent categoryName:cat]);
	[all setBanner:scratch];

	if (cacheEnabled) [cacheAgent addArray:all];
	currentCall = NULL;
	return all;
}

- (LUArray *)query:(LUDictionary *)pattern
{
	LUArray *lookupOrder;
	LUArray *all, *list;
	LUArray *sub;
	LUAgent *agent;
	LUDictionary *item;
	LUCategory cat;
	int i, len;
	int j, sublen;
	BOOL found;
	char caller[256], *pagent;
	struct timeval listStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int listTime;
	unsigned int where;

	if (pattern == nil) return nil;

	where = [pattern indexForKey:"_lookup_category"];
	if (where == IndexNull) return nil;

	cat = [LUAgent categoryWithName:[pattern valueAtIndex:where]];
	if (cat > NCATEGORIES) return nil;

	pagent = NULL;
	where = [pattern indexForKey:"_lookup_agent"];
	if (where != IndexNull) pagent = [pattern valueAtIndex:where];

	gettimeofday(&listStart, (struct timezone *)NULL);

	sprintf(caller, "query");
	currentCall = caller;
	
	if ((pagent != NULL) && (!strcmp(pagent, "Cache")))
	{
		gettimeofday(&sysStart, (struct timezone *)NULL);
		currentAgent = cacheAgent;
		state = ServerStateQuerying;

		list = nil;

		all = [cacheAgent allItemsWithCategory:cat];
		if (all != nil)
		{
			list = [all filter:pattern];
			[all release];
		}

		state = ServerStateActive;
		currentAgent = nil;
		sysTime = milliseconds_since(sysStart);
		found = (list != nil);
		[self recordSearch:caller infoSystem:"Cache" time:sysTime hit:found];

		listTime = milliseconds_since(listStart);
		[self recordCall:caller time:listTime hit:found];
		currentCall = NULL;
		return list;
	}

	all = [[LUArray alloc] init];

	lookupOrder = order[(unsigned int)cat];
	len = [lookupOrder count];
	agent = nil;
	for (i = 0; i < len; i++)
	{
		agent = [lookupOrder objectAtIndex:i];
		if (streq([agent shortName], "Cache")) continue;
		if ((pagent != NULL) && strcmp([agent shortName], pagent)) continue;

		gettimeofday(&sysStart, (struct timezone *)NULL);

		currentAgent = agent;
		state = ServerStateQuerying;
		sub = [agent query:pattern category:cat];
		state = ServerStateActive;
		currentAgent = nil;

		sysTime = milliseconds_since(sysStart);
		found = (sub != nil);
		[self recordSearch:caller infoSystem:[agent shortName] time:sysTime hit:found];

		if (found)
		{
			sublen = [sub count];
			for (j = 0; j < sublen; j++)
			{
				item = [sub objectAtIndex:j];
				[item setCategory:cat];
				[all addObject:item];
			}

			[sub release];
		}
	}

	listTime = milliseconds_since(listStart);
	found = ([all count] != 0);
	[self recordCall:caller time:listTime hit:found];

	if (!found)
	{
		[all release];
		currentCall = NULL;
		return nil;
	}

	currentCall = NULL;
	return all;
}

/*
 * Data lookup done here!
 */
- (LUArray *)allGroupsWithUser:(char *)name
{
	LUArray *lookupOrder;
	LUArray *all;
	LUArray *sub;
	LUAgent *agent;
	LUDictionary *stamp;
	int i, len;
	int j, sublen;
	BOOL cacheEnabled, found;
	char scratch[256];
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;

	currentCall = "initgroups";

	if (name == NULL)
	{
		[self recordSearch:currentCall infoSystem:"Failed" time:0 hit:YES];
		[self recordCall:currentCall time:0 hit:NO];
		currentCall = NULL;
		return nil;
	}

	gettimeofday(&allStart, (struct timezone *)NULL);

	cacheEnabled = [cacheAgent cacheIsEnabledForCategory:LUCategoryInitgroups];
	if (cacheEnabled)
	{
		gettimeofday(&sysStart, (struct timezone *)NULL);
		currentAgent = cacheAgent;
		state = ServerStateQuerying;
		all = [cacheAgent initgroupsForUser:name];
		state = ServerStateActive;
		currentAgent = nil;
		sysTime = milliseconds_since(sysStart);
		found = (all != nil);
		[self recordSearch:currentCall infoSystem:"Cache"
			time:sysTime hit:found];

		if (found)
		{
			allTime = milliseconds_since(allStart);
			[self recordCall:currentCall time:allTime hit:found];
			currentCall = NULL;
			return all;
		}
	}

	all = [[LUArray alloc] init];

	lookupOrder = order[(unsigned int)LUCategoryUser];
	len = [lookupOrder count];
	agent = nil;
	for (i = 0; i < len; i++)
	{
		agent = [lookupOrder objectAtIndex:i];
		if (streq([agent shortName], "Cache")) continue;

		gettimeofday(&sysStart, (struct timezone *)NULL);
		currentAgent = agent;
		state = ServerStateQuerying;
		sub = [agent allGroupsWithUser:name];
		state = ServerStateActive;
		currentAgent = nil;
		sysTime = milliseconds_since(sysStart);
		found = (sub != nil);
		[self recordSearch:currentCall infoSystem:[agent shortName]
			time:sysTime hit:found];

		if (found)
		{
			/* Merge validation info from this agent into "all" array */
			sublen = [sub validationStampCount];
			for (j = 0; j < sublen; j++)
			{
				stamp = [sub validationStampAtIndex:j];
				[stamp setCategory:LUCategoryInitgroups];
				[all addValidationStamp:stamp];
			}

			sublen = [sub count];
			for (j = 0; j < sublen; j++)
			{
				[all addObject:[sub objectAtIndex:j]];
			}

			[sub release];
		}
	}

	allTime = milliseconds_since(allStart);
	found = ([all count] != 0);
	[self recordCall:currentCall time:allTime hit:found];

	if (!found)
	{
		[all release];
		currentCall = NULL;
		return nil;
	}

	sprintf(scratch, "LUServer: all groups with user %s", name);
	[all setBanner:scratch];

	if (cacheEnabled) [cacheAgent setInitgroups:all forUser:name];
	currentCall = NULL;
	return all;
}

/*
 * Data lookup done here!
 */
- (LUArray *)allNetgroupsWithName:(char *)name
{
	LUArray *all;
	LUArray *lookupOrder;
	LUDictionary *item;
	LUAgent *agent;
	int i, len;
	char scratch[256];
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;
	BOOL found;

	currentCall = "netgroup name";
	if (name == NULL)
	{
		[self recordSearch:currentCall infoSystem:"Failed" time:0 hit:YES];
		[self recordCall:currentCall time:0 hit:NO];
		currentCall = NULL;
		return nil;
	}

	lookupOrder = order[(unsigned int)LUCategoryNetgroup];
	len = [lookupOrder count];
	if (len == 0)
	{
		currentCall = NULL;
		return nil;
	}
	
	all = [[LUArray alloc] init];

	gettimeofday(&allStart, (struct timezone *)NULL);

	for (i = 0; i < len; i++)
	{
		agent = [lookupOrder objectAtIndex:i];
		gettimeofday(&sysStart, (struct timezone *)NULL);
		currentAgent = agent;
		state = ServerStateQuerying;
		item = [agent itemWithKey:"name" value:name category:LUCategoryNetgroup];
		state = ServerStateActive;
		currentAgent = nil;
		sysTime = milliseconds_since(sysStart);
		found = (item != nil);
		[self recordSearch:currentCall infoSystem:[agent shortName]
			time:sysTime hit:found];

		if (found)
		{
			[all addObject:item];
			[item release];
		}
	}

	allTime = milliseconds_since(allStart);
	found = ([all count] != 0);
	[self recordCall:currentCall time:allTime hit:found];

	if (!found)
	{
		[all release];
		currentCall = NULL;
		return nil;
	}

	sprintf(scratch, "LUServer: all netgroup %s", name);
	[all setBanner:scratch];

	currentCall = NULL;
	return all;
}

- (LUDictionary *)groupWithKey:(char *)key value:(char *)val
{
	LUArray *all;
	LUArray *lookupOrder;
	LUDictionary *item;
	int i, len;
	char scratch[256];
	char str[1024];
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;
	BOOL found, cacheEnabled;
	LUDictionary *q;

	if (key == NULL) return nil;
	if (val == NULL) return nil;

	sprintf(str, "%s %s", [LUAgent categoryName:LUCategoryGroup], key);
	currentCall = str;

	lookupOrder = order[(unsigned int)LUCategoryGroup];
	item = nil;
	len = [lookupOrder count];

	gettimeofday(&allStart, (struct timezone *)NULL);

	cacheEnabled = [cacheAgent cacheIsEnabledForCategory:LUCategoryGroup];
	if (cacheEnabled)
	{
		gettimeofday(&sysStart, (struct timezone *)NULL);
		currentAgent = cacheAgent;
		state = ServerStateQuerying;
		item = [cacheAgent itemWithKey:key value:val category:LUCategoryGroup];
		state = ServerStateActive;
		currentAgent = nil;
		sysTime = milliseconds_since(sysStart);
		found = (item != nil);
		[self recordSearch:currentCall infoSystem:"Cache" time:sysTime hit:found];

		if (found)
		{
			allTime = milliseconds_since(allStart);
			[self recordCall:currentCall time:allTime hit:found];
			currentCall = NULL;
			return item;
		}
	}

	q = [[LUDictionary alloc] init];
	[q setValue:val forKey:key];
	[q setValue:"group" forKey:"_lookup_category"];

	all = [self query:q];
	[q release];

	if (all == nil) return nil;

	item = [[LUDictionary alloc] init];
	[item setCategory:LUCategoryGroup];
	sprintf(scratch, "LUServer: group %s %s", key, val);
	[item setBanner:scratch];

	len = [all count];
	for (i = 0; i < len; i++)
	{
		[item mergeKey:"name" from:[all objectAtIndex:i]];
		[item mergeKey:"gid" from:[all objectAtIndex:i]];
		[item mergeKey:"users" from:[all objectAtIndex:i]];
	}

	[all release];

	if (cacheEnabled) [cacheAgent addObject:item];
	return item;
}

- (LUDictionary *)netgroupWithName:(char *)name
{
	LUArray *all;
	LUDictionary *group;
	int i, len;
	char scratch[256];

	if (name == NULL) return nil;

	all = [self allNetgroupsWithName:name];
	if (all == nil) return nil;

	group = [[LUDictionary alloc] init];
	sprintf(scratch, "LUServer: netgroup %s", name);
	[group setBanner:scratch];

	[group setValue:name forKey:"name"];

	len = [all count];
	for (i = 0; i < len; i++)
		[self mergeNetgroup:[all objectAtIndex:i] into:group];

	[all release];

	return group;
}

/* 
 * This is the search routine for itemWithKey:value:category
 */
- (LUDictionary *)findItemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	LUArray *lookupOrder;
	LUDictionary *item;
	LUAgent *agent;
	int i, j, len, nether;
	char **etherAddrs;
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;
	char str[1024];
	BOOL tryRealName, isEtherAddr, found;

	sprintf(str, "%s %s", [LUAgent categoryName:cat], key);
	currentCall = str;

	lookupOrder = order[(unsigned int)cat];
	item = nil;
	len = [lookupOrder count];
	tryRealName = NO;
	if ((cat == LUCategoryUser) && (streq(key, "name"))) tryRealName = YES;

	isEtherAddr = NO;
	if (streq(key, "en_address")) isEtherAddr = YES;

	gettimeofday(&allStart, (struct timezone *)NULL);

	for (i = 0; i < len; i++)
	{
		agent = [lookupOrder objectAtIndex:i];

		gettimeofday(&sysStart, (struct timezone *)NULL);

		currentAgent = agent;
		state = ServerStateQuerying;
		if (isEtherAddr)
		{
			/* Try all possible variations on leading zeros in the address */
			etherAddrs = [LUAgent variationsOfEthernetAddress:val];
			nether = listLength(etherAddrs);
			for (j = 0; j < nether; j++)
			{
				item = [agent itemWithKey:key value:etherAddrs[j] category:cat];
				if (item != nil) break;
			}

			freeList(etherAddrs);
		}
		else
		{
			item = [agent itemWithKey:key value:val category:cat];
			if (tryRealName && (item == nil))
			{
				item = [agent itemWithKey:"realname" value:val category:cat];
			}
		}
		state = ServerStateActive;
		currentAgent = nil;
	
		sysTime = milliseconds_since(sysStart);
		found = (item != nil);
		[self recordSearch:currentCall infoSystem:[agent shortName] time:sysTime hit:found];

		if (found)
		{
			allTime = milliseconds_since(allStart);
			[self recordCall:currentCall time:allTime hit:found];
			currentCall = NULL;
			return [self stamp:item agent:agent category:cat];
		}
	}

	allTime = milliseconds_since(allStart);
	[self recordSearch:currentCall infoSystem:"Failed" time:allTime hit:YES];
	[self recordCall:currentCall time:allTime hit:NO];

	currentCall = NULL;
	return nil;
}

- (void)preferBootpAddress:(char *)addr
	key:(char *)key
	target:(char *)tkey
	dict:(LUDictionary *)dict
{
	char **kVals, **tVals;
	char *t;
	int i, target, kLen, tLen, tLast;

	kVals = [dict valuesForKey:key];
	tVals = [dict valuesForKey:tkey];

	tLen = listLength(tVals);
	if (tLen == 0) return;

	kLen = listLength(kVals);
	if (kLen == 0) return;

	tLast = tLen - 1;
	target = 0;

	for (i = 0; i < kLen; i++)
	{
		if (i == tLast) break;

		if (streq(addr, kVals[i]))
		{
			target = i;
			break;
		}
	}

	[dict removeKey:key];
	[dict setValue:addr forKey:key];

	t = copyString(tVals[target]);
	[dict removeKey:tkey];
	[dict setValue:t forKey:tkey];
	freeString(t);
}

/*
 * Find an item.  This method handles some special cases.
 * It calls findItemWithKey:value:category to do the work.
 */
- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	LUDictionary *item;

	if ((key == NULL) || (val == NULL) || (cat > NCATEGORIES))
	{
		return nil;
	}

	if (cat == LUCategoryGroup)
	{
		return [self groupWithKey:key value:val];
	}

	if (cat == LUCategoryNetgroup)
	{
		return [self netgroupWithName:val];
	}

	if (streq(key, "en_address")) val = [LUAgent canonicalEthernetAddress:val];
	
	item = [self findItemWithKey:key value:val category:cat];
	if ((cat == LUCategoryBootp) && (item != nil))
	{
		if (streq(key, "ip_address"))
		{
			/* only return a directory if there is an en_address */
			if ([item valuesForKey:"en_address"] == NULL)
			{
				[item release];
				return nil;
			}

			[self preferBootpAddress:val key:key target:"en_address" dict:item];
		}
		else if (streq(key, "en_address"))
		{
			[self preferBootpAddress:val key:key target:"ip_address" dict:item];
		}
	}

	return item;
}

/*
 ****************  Lookup routines (API) start here  ****************
 */

- (BOOL)inNetgroup:(char *)group
	host:(char *)host
	user:(char *)user
	domain:(char *)domain
	level:(int)level
{
	LUDictionary *g;
	int i, len;
	char **members;
	char *name;

	if (level > MaxNetgroupRecursion)
	{
		system_log(LOG_ERR, "netgroups nested more than %d levels",
			MaxNetgroupRecursion);
		return NO;
	}

	if (group == NULL) return NO;
	g = [self itemWithKey:"name" value:group category:LUCategoryNetgroup];
	if (g == nil) return NO;

	if (host != NULL)
	{
		name = host;
		members = [g valuesForKey:"hosts"];
	}
	else if (user != NULL)
	{
		name = user;
		members = [g valuesForKey:"users"];
	}
	else if (domain != NULL)
	{
		name = domain;
		members = [g valuesForKey:"domains"];
	}
	else
	{
		[g release];
		return NO;
	}

	if ((listIndex("*", members) != IndexNull) ||
		(listIndex(name, members) != IndexNull))
	{
		[g release];
		return YES;
	}
	
	members = [g valuesForKey:"netgroups"];
	len = [g countForKey:"netgroups"];
	for (i = 0; i < len; i++)
	{
		if ([self inNetgroup:members[i] host:host user:user domain:domain
			level:level+1])
		{
			[g release];
			return YES;
		}
	}

	[g release];
	return NO;
}

- (BOOL)inNetgroup:(char *)group
	host:(char *)host
	user:(char *)user
	domain:(char *)domain
{
	char **nameList;
	int i, len;
	BOOL yn;

	nameList = [self hostNameList:host];
	len = listLength(nameList);

	for (i = 0; i < len; i++)
	{
		yn = [self inNetgroup:group host:nameList[i] user:user domain:domain level:0];
		if (yn == YES)
		{
			freeList(nameList);
			return YES;
		}
	}

	freeList(nameList);
	return NO;
}

/*
 * Data lookup done here!
 */
- (LUDictionary *)serviceWithName:(char *)name
	protocol:(char *)prot
{
	LUArray *lookupOrder;
	LUDictionary *item;
	LUAgent *agent;
	int i, len;
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;
	BOOL found;

	currentCall = "service name";
	if (name == NULL)
	{
		[self recordSearch:currentCall infoSystem:"Failed" time:0 hit:YES];
		[self recordCall:currentCall time:0 hit:NO];
		currentCall = NULL;
		return nil;
	}

	gettimeofday(&allStart, (struct timezone *)NULL);
	lookupOrder = order[(unsigned int)LUCategoryService];
	item = nil;
	len = [lookupOrder count];
	for (i = 0; i < len; i++)
	{
		agent = [lookupOrder objectAtIndex:i];
		gettimeofday(&sysStart, (struct timezone *)NULL);
		currentAgent = agent;
		state = ServerStateQuerying;
		item = [agent serviceWithName:name protocol:prot];
		state = ServerStateActive;
		currentAgent = nil;
		sysTime = milliseconds_since(sysStart);
		found = (item != nil);
		[self recordSearch:currentCall infoSystem:[agent shortName]
			time:sysTime hit:found];

		if (found)
		{
			allTime = milliseconds_since(allStart);
			[self recordCall:currentCall time:allTime hit:found];
			currentCall = NULL;
			return [self stamp:item agent:agent category:LUCategoryService];
		}
	}
	allTime = milliseconds_since(allStart);
	[self recordSearch:currentCall infoSystem:"Failed" time:allTime hit:YES];
	[self recordCall:currentCall time:allTime hit:NO];
	currentCall = NULL;
	return nil;
}

/*
 * Data lookup done here!
 */
- (LUDictionary *)serviceWithNumber:(int *)number
	protocol:(char *)prot
{
	LUArray *lookupOrder;
	LUDictionary *item;
	LUAgent *agent;
	int i, len;
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;
	BOOL found;

	currentCall = "service number";
	if (number == NULL)
	{
		[self recordSearch:currentCall infoSystem:"Failed" time:0 hit:YES];
		[self recordCall:currentCall time:0 hit:NO];
		currentCall = NULL;
		return nil;
	}

	gettimeofday(&allStart, (struct timezone *)NULL);
	lookupOrder = order[(unsigned int)LUCategoryService];
	item = nil;
	len = [lookupOrder count];
	for (i = 0; i < len; i++)
	{
		agent = [lookupOrder objectAtIndex:i];
		gettimeofday(&sysStart, (struct timezone *)NULL);
		currentAgent = agent;
		state = ServerStateQuerying;
		item = [agent serviceWithNumber:number protocol:prot];
		state = ServerStateActive;
		currentAgent = nil;
		sysTime = milliseconds_since(sysStart);
		found = (item != nil);
		[self recordSearch:currentCall infoSystem:[agent shortName]
			time:sysTime hit:found];

		if (found)
		{
			allTime = milliseconds_since(allStart);
			[self recordCall:currentCall time:allTime hit:found];
			currentCall = NULL;
			return [self stamp:item agent:agent category:LUCategoryService];
		}
	}
	allTime = milliseconds_since(allStart);
	[self recordSearch:currentCall infoSystem:"Failed" time:allTime hit:YES];
	[self recordCall:currentCall time:allTime hit:NO];
	currentCall = NULL;
	return nil;
}

/*
 * Custom lookups 
 *
 * Data lookup done here!
 */
- (BOOL)isSecurityEnabledForOption:(char *)option
{
	BOOL status;
	NIAgent *ni;

	ni = (NIAgent *)[self agentForSystem:[NIAgent class]];
	status = [ni isSecurityEnabledForOption:option];
	[ni release];

	return status;
}

/*
 * Data lookup done here!
 */
- (BOOL)isNetwareEnabled
{
	BOOL status;
	NIAgent *ni;

	ni = (NIAgent *)[self agentForSystem:[NIAgent class]];
	status = [ni isNetwareEnabled];
	[ni release];

	return status;
}

@end
