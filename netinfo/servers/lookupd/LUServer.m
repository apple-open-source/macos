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
#import "Config.h"
#import "Dyna.h"
#import <NetInfo/dsutil.h>
#import <netinfo/ni.h>
#import <string.h>
#import <stdlib.h>
#import <stdio.h>
#import <sys/types.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>

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

- (char **)lookupOrderForCategory:(LUCategory)cat
{
	LUDictionary *cdict;
	char **order;

	cdict = [configManager configForCategory:cat fromConfig:configurationArray];
	if (cdict != nil)
	{
		order = [cdict valuesForKey:"LookupOrder"];
		if (order != NULL) return order;
	}

	cdict = [configManager configGlobal:configurationArray];
	if (cdict == nil) return NULL;

	order = [cdict valuesForKey:"LookupOrder"];
	return order;
}

- (LUServer *)init
{
	[super init];

	agentList = [[LUArray alloc] init];
	[agentList setBanner:"LUServer agent list"];

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

- (LUAgent *)agentNamed:(char *)name
{
	id agentClass;
	char *arg, *cname, *cserv;
	int i, len;
	id agent;

	if (name == NULL) return nil;

	arg = strchr(name, ':');
	if (arg != NULL) arg += 1;

	cname = [LUAgent canonicalAgentName:name];
	if (cname == NULL) return nil;

	cserv = [LUAgent canonicalServiceName:name];
	if (cserv == NULL)
	{
		free(cname);
		return nil;
	}

	len = [agentList count];
	for (i = 0; i < len; i++)
	{
		agent = [agentList objectAtIndex:i];
		if (streq([agent serviceName], cserv))
		{
			free(cname);
			free(cserv);
			return agent;
		}
	}

	agent = nil;
	agentClass = [controller agentClassNamed:cname];
	free(cname);
	free(cserv);
	if (agentClass == nil)
	{
		agent = [[Dyna alloc] initWithArg:name];
	}
	else
	{
		agent = [[agentClass alloc] initWithArg:arg];
	}

	if (agent == nil) return nil;

	[agentList addObject:agent];
	[agent release];

	system_log(LOG_DEBUG, "LUServer 0x%08x added agent 0x%08x (%s)",
		(int)self, (int)agent, [agent serviceName]);

	return agent;
}

- (void)dealloc
{
	int i, len;
	LUAgent *agent;

	if (agentList != nil)
	{
		len = [agentList count];
		for (i = len - 1; i >= 0; i--)
		{
			agent = [agentList objectAtIndex:i];
			system_log(LOG_DEBUG, "%d: server 0x%08x released agent 0x%08x (%s)", i, (int)self, (int)agent, [agent serviceName]);
			[agentList removeObject:agent];
		}

		[agentList release];
	}

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

	if (info == NULL) return;

	/* total for this info system */
	[self addTime:t hit:found forKey:info];

	/* total for this method in this info system */
	sprintf(key, "%s %s", info, method);
	[self addTime:t hit:found forKey:key];
}

- (LUDictionary *)stamp:(LUDictionary *)item
	key:(char *)key
	agent:(LUAgent *)agent
 	category:(LUCategory)cat
{
	BOOL cacheEnabled;
	char scratch[256];
	const char *sname;

	if (item == nil) return nil;

	cacheEnabled = [cacheAgent cacheIsEnabledForCategory:cat];
	[item setCategory:cat];

	sname = [agent shortName];
	if (sname == NULL) return item;

	if (strcmp(sname, "Cache"))
	{
		if (cat == LUCategoryBootp)
		{
			sprintf(scratch, "%s: %s %s (%s / %s)",
				sname,
				[LUAgent categoryName:cat],
				[item valueForKey:"name"],
				[item valueForKey:"en_address"],
				[item valueForKey:"ip_address"]);
		}
		else
		{
			sprintf(scratch, "%s: %s %s",
				sname,
				[LUAgent categoryName:cat],
				[item valueForKey:"name"]);
		}
		[item setBanner:scratch];
	}

	if (cacheEnabled && (strcmp(sname, "Cache")))
	{
		[cacheAgent addObject:item key:key category:cat];
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
	char **lookupOrder;
	const char *sname;
	LUArray *all;
	LUArray *sub;
	LUAgent *agent;
	LUDictionary *stamp;
	LUDictionary *item;
	int i, len;
	int j, sublen;
	BOOL cacheEnabled;
	char scratch[256], caller[256];
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;

	if (cat >= NCATEGORIES) return nil;

	if (statistics_enabled)
	{
		gettimeofday(&allStart, (struct timezone *)NULL);

		sprintf(caller, "all %s", [LUAgent categoryName:cat]);
		currentCall = caller;
	}

	cacheEnabled = [cacheAgent cacheIsEnabledForCategory:cat];
	if (cacheEnabled)
	{
		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);
			currentAgent = cacheAgent;
			state = ServerStateQuerying;
		}

		all = [cacheAgent allItemsWithCategory:cat];

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
			[self recordSearch:caller infoSystem:"Cache" time:sysTime hit:(all != nil)];
		}

		if (all != nil)
		{
			if (statistics_enabled)
			{
				allTime = milliseconds_since(allStart);
				[self recordCall:caller time:allTime hit:YES];
				currentCall = NULL;
			}
			return all;
		}
	}

	all = [[LUArray alloc] init];

	lookupOrder = [self lookupOrderForCategory:cat];
	len = listLength(lookupOrder);
	agent = nil;
	for (i = 0; i < len; i++)
	{
		agent = [self agentNamed:lookupOrder[i]];
		if (agent == nil) continue;
		sname = [agent shortName];
		if ((sname != NULL) && (streq(sname, "Cache"))) continue;

		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);

			currentAgent = agent;
			state = ServerStateQuerying;
		}

		sub = [agent allItemsWithCategory:cat];

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;

			sysTime = milliseconds_since(sysStart);
	
			[self recordSearch:caller infoSystem:sname time:sysTime hit:(sub != nil)];
		}
	
		if (sub != nil)
		{
			/* Merge validation info from this agent into "all" array */
			sublen = [sub validationStampCount];
			for (j = 0; j < sublen; j++)
			{
				stamp = [sub validationStampAtIndex:j];
				if ([stamp isNegative]) continue;

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

	if (statistics_enabled)
	{
		allTime = milliseconds_since(allStart);
		[self recordCall:caller time:allTime hit:([all count] != 0)];
		currentCall = NULL;
	}

	if ([all count] == 0)
	{
		[all release];
		return nil;
	}

	sprintf(scratch, "LUServer: all %s", [LUAgent categoryName:cat]);
	[all setBanner:scratch];

	if (cacheEnabled) [cacheAgent addArray:all];
	return all;
}

- (LUArray *)query:(LUDictionary *)pattern
{
	char **lookupOrder;
	char *catname;
	const char *sname;
	LUArray *all, *list;
	LUArray *sub;
	LUAgent *agent;
	LUDictionary *item;
	LUCategory cat;
	int i, len;
	int j, sublen;
	BOOL isnumber;
	char caller[256], *pagent;
	struct timeval listStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int listTime;
	unsigned int where;

	if (pattern == nil) return nil;

	where = [pattern indexForKey:"_lookup_category"];
	if (where == IndexNull) return nil;

	catname = [pattern valueAtIndex:where];
	if (catname == NULL) return nil;

	/* Backward compatibility for clients that do direct "query" calls */
	cat = -1;
	isnumber = YES;
	len = strlen(catname);
	for (i = 0; (i < len) && isnumber; i++)
	{
		if ((catname[i] < '0') || (catname[i] > '9')) isnumber = NO;
	}

	if (isnumber) cat = atoi(catname);
	else cat = [LUAgent categoryWithName:catname];

	if (cat > NCATEGORIES) return nil;

	if (statistics_enabled)
	{
		gettimeofday(&listStart, (struct timezone *)NULL);

		sprintf(caller, "query");
		currentCall = caller;
	}

	pagent = NULL;
	where = [pattern indexForKey:"_lookup_agent"];
	if (where != IndexNull) pagent = [pattern valueAtIndex:where];

	if ((pagent != NULL) && (!strcmp(pagent, "Cache")))
	{
		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);
			currentAgent = cacheAgent;
			state = ServerStateQuerying;
		}

		list = nil;

		all = [cacheAgent allItemsWithCategory:cat];
		if (all != nil)
		{
			list = [all filter:pattern];
			[all release];
		}

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
			[self recordSearch:caller infoSystem:"Cache" time:sysTime hit:(list != nil)];

			listTime = milliseconds_since(listStart);
			[self recordCall:caller time:listTime hit:(list != nil)];
			currentCall = NULL;
		}

		return list;
	}

	all = [[LUArray alloc] init];

	lookupOrder = [self lookupOrderForCategory:cat];
	len = listLength(lookupOrder);
	agent = nil;
	for (i = 0; i < len; i++)
	{
		agent = [self agentNamed:lookupOrder[i]];
		if (agent == nil) continue;
		sname = [agent shortName];
		if ((sname != NULL) && (streq(sname, "Cache"))) continue;
		if ((pagent != NULL) && strcmp(sname, pagent)) continue;

		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);

			currentAgent = agent;
			state = ServerStateQuerying;
		}

		sub = [agent query:pattern category:cat];

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
			[self recordSearch:caller infoSystem:sname time:sysTime hit:(sub != nil)];
		}

		if (sub != nil)
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

	if (statistics_enabled)
	{
		listTime = milliseconds_since(listStart);
		[self recordCall:caller time:listTime hit:([all count] != 0)];
		currentCall = NULL;
	}

	if ([all count] == 0)
	{
		[all release];
		return nil;
	}

	return all;
}

- (LUDictionary *)allGroupsWithUser:(char *)name
{
	char **lookupOrder;
	LUDictionary *all;
	LUDictionary *sub;
	LUAgent *agent;
	int i, len;
	BOOL cacheEnabled;
	char scratch[256];
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;

	if (statistics_enabled)
	{
		gettimeofday(&allStart, (struct timezone *)NULL);
		currentCall = "initgroups";
	}

	if (name == NULL)
	{
		if (statistics_enabled)
		{
			[self recordSearch:currentCall infoSystem:"Failed" time:0 hit:YES];
			[self recordCall:currentCall time:0 hit:NO];
			currentCall = NULL;
		}
		return nil;
	}

	cacheEnabled = [cacheAgent cacheIsEnabledForCategory:LUCategoryInitgroups];
	if (cacheEnabled)
	{
		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);
			currentAgent = cacheAgent;
			state = ServerStateQuerying;
		}

		all = [cacheAgent allGroupsWithUser:name];

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
			[self recordSearch:currentCall infoSystem:"Cache" time:sysTime hit:(all != nil)];
		}

		if (all != nil)
		{
			if (statistics_enabled)
			{
				allTime = milliseconds_since(allStart);
				[self recordCall:currentCall time:allTime hit:YES];
				currentCall = NULL;
			}
			return all;
		}
	}

	all = [[LUDictionary alloc] init];
	[all setValue:name forKey:"name"];

	lookupOrder = [self lookupOrderForCategory:LUCategoryUser];
	len = listLength(lookupOrder);
	agent = nil;
	for (i = 0; i < len; i++)
	{
		agent = [self agentNamed:lookupOrder[i]];
		if (agent == nil) continue;
		if (streq([agent shortName], "Cache")) continue;

		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);
			currentAgent = agent;
			state = ServerStateQuerying;
		}

		sub = [agent allGroupsWithUser:name];

		if (statistics_enabled)
		{
			state = ServerStateActive;
				currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
			[self recordSearch:currentCall infoSystem:[agent shortName] time:sysTime hit:(sub != nil)];
		}

		if (sub != nil)
		{
			[all mergeKey:"gid" from:sub];
			[sub release];
		}
	}

	if (statistics_enabled)
	{
		allTime = milliseconds_since(allStart);
		[self recordCall:currentCall time:allTime hit:([all count] != 0)];
		currentCall = NULL;
	}

	sprintf(scratch, "LUServer: all groups with user %s", name);
	[all setBanner:scratch];

	if (cacheEnabled) [cacheAgent setInitgroups:all forUser:name];
	return [self stamp:all key:"name" agent:self category:LUCategoryInitgroups];
}

- (LUDictionary *)allNetgroupsWithName:(char *)name
{
	LUDictionary *group;
	char **lookupOrder;
	LUDictionary *item;
	LUAgent *agent;
	int i, len;
	char scratch[256];
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;
	BOOL allFound, found;

	if (name == NULL) return nil;

	if (statistics_enabled)
	{
		gettimeofday(&allStart, (struct timezone *)NULL);
		currentCall = "netgroup name";
	}

	lookupOrder = [self lookupOrderForCategory:LUCategoryNetgroup];
	len = listLength(lookupOrder);
	if (len == 0)
	{
		currentCall = NULL;
		return nil;
	}
	
	group = [[LUDictionary alloc] init];
	[group setValue:name forKey:"name"];
	sprintf(scratch, "LUServer: netgroup %s", name);
	[group setBanner:scratch];

	allFound = NO;

	for (i = 0; i < len; i++)
	{
		agent = [self agentNamed:lookupOrder[i]];
		if (agent == nil) continue;
		if (streq([agent shortName], "Cache")) continue;

		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);
			currentAgent = agent;
			state = ServerStateQuerying;
		}

		found = NO;
		item = [agent netgroupWithName:name];
		if (item != nil)
		{
			found = YES;
			allFound = YES;
			[group mergeKey:"hosts" from:item];
			[group mergeKey:"users" from:item];
			[group mergeKey:"domains" from:item];
			[item release];
		}

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
			[self recordSearch:currentCall infoSystem:[agent shortName] time:sysTime hit:found];
		}
	}

	if (statistics_enabled)
	{
		allTime = milliseconds_since(allStart);
		[self recordCall:currentCall time:allTime hit:allFound];
		currentCall = NULL;
	}

	if (!allFound)
	{
		[group release];
		return nil;
	}

	return group;
}

- (LUDictionary *)groupWithKey:(char *)key value:(char *)val
{
	LUArray *all;
	char **lookupOrder;
	LUDictionary *item;
	int i, len;
	char scratch[256];
	char str[1024];
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;
	BOOL cacheEnabled;
	LUDictionary *q;

	if (key == NULL) return nil;
	if (val == NULL) return nil;

	if (statistics_enabled)
	{
		gettimeofday(&allStart, (struct timezone *)NULL);
		sprintf(str, "%s %s", [LUAgent categoryName:LUCategoryGroup], key);
		currentCall = str;
	}

	lookupOrder = [self lookupOrderForCategory:LUCategoryGroup];
	item = nil;
	len = listLength(lookupOrder);

	cacheEnabled = [cacheAgent cacheIsEnabledForCategory:LUCategoryGroup];
	if (cacheEnabled)
	{
		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);
			currentAgent = cacheAgent;
			state = ServerStateQuerying;
		}

		item = [cacheAgent itemWithKey:key value:val category:LUCategoryGroup];

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
			[self recordSearch:currentCall infoSystem:"Cache" time:sysTime hit:(item != nil)];
		}

		if (item != nil)
		{
			if (statistics_enabled)
			{
				allTime = milliseconds_since(allStart);
				[self recordCall:currentCall time:allTime hit:YES];
				currentCall = NULL;
			}
			return item;
		}
	}

	if (statistics_enabled) currentCall = NULL;

	q = [[LUDictionary alloc] init];
	[q setValue:val forKey:key];
	sprintf(scratch, "%u", LUCategoryGroup);
	[q setValue:scratch forKey:"_lookup_category"];

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

	if (cacheEnabled) [cacheAgent addObject:item key:key category:LUCategoryGroup];

	return item;
}

- (LUDictionary *)netgroupWithName:(char *)name
{
	LUDictionary *item;
	BOOL cacheEnabled;
	struct timeval sysStart;
	unsigned int sysTime;

	if (name == NULL) return nil;

	cacheEnabled = [cacheAgent cacheIsEnabledForCategory:LUCategoryNetgroup];
	if (cacheEnabled)
	{
		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);
			currentAgent = cacheAgent;
			state = ServerStateQuerying;
		}

		item = [cacheAgent itemWithKey:"name" value:name category:LUCategoryNetgroup];

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
			[self recordSearch:currentCall infoSystem:"Cache" time:sysTime hit:(item != nil)];
		}

		if (item != nil)
		{
			if (statistics_enabled)
			{
				[self recordCall:currentCall time:sysTime hit:YES];
				currentCall = NULL;
			}

			return item;
		}
	}

	if (statistics_enabled) currentCall = NULL;

	item = [self allNetgroupsWithName:name];
	if (item == nil) return nil;

	if (cacheEnabled) [cacheAgent addObject:item key:name category:LUCategoryNetgroup];
	return item;
}

/* 
 * This is the search routine for itemWithKey:value:category
 */
- (LUDictionary *)findItemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	char **lookupOrder;
	const char *sname;
	LUDictionary *item;
	LUAgent *agent;
	int i, j, len, nether;
	char **etherAddrs;
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;
	char str[1024];
	BOOL tryRealName, isEtherAddr;
	struct in_addr a4;
	struct in6_addr a6;
	char paddr[64];

	sprintf(str, "%s %s", [LUAgent categoryName:cat], key);
	currentCall = str;

	lookupOrder = [self lookupOrderForCategory:cat];
	item = nil;
	len = listLength(lookupOrder);
	tryRealName = NO;
	if ((cat == LUCategoryUser) && (streq(key, "name"))) tryRealName = YES;

	isEtherAddr = NO;
	if (streq(key, "en_address")) isEtherAddr = YES;

	/*
	 * Convert addresses to canonical form.
	 * if inet_aton, inet_pton, or inet_ntop can't deal with them
	 * we leave the address alone - user may have some private idea
	 * of addresses.
	 */
	if (cat == LUCategoryHost)
	{
		if (streq(key, "ip_address"))
		{
			if (inet_aton(val, &a4) == 1)
			{
				if (inet_ntop(AF_INET, &a4, paddr, 64) != NULL) val = paddr;
			}
		}
		else if (streq(key, "ipv6_address"))
		{
			if (inet_pton(AF_INET6, val, &a6) == 1)
			{
				if (inet_ntop(AF_INET6, &a6, paddr, 64) != NULL) val = paddr;
			}
		}
	}

	if (statistics_enabled) gettimeofday(&allStart, (struct timezone *)NULL);

	for (i = 0; i < len; i++)
	{
		agent = [self agentNamed:lookupOrder[i]];
		if (agent == nil) continue;

		if (statistics_enabled) 
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);

			currentAgent = agent;
			state = ServerStateQuerying;
		}

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
			/*
			 * N.B. we omit NI from this search since it handles the
			 * name / realname case itself.  
			 */
			if (tryRealName && (item == nil) && strcmp([agent shortName], "NI"))
			{
				item = [agent itemWithKey:"realname" value:val category:cat];
			}
		}

		if (statistics_enabled) 
		{
			state = ServerStateActive;
			currentAgent = nil;
	
			sysTime = milliseconds_since(sysStart);
			sname = [agent shortName];
			[self recordSearch:currentCall infoSystem:sname time:sysTime hit:(item != nil)];
		}

		if (item != nil)
		{
			if (statistics_enabled)
			{
				allTime = milliseconds_since(allStart);
				[self recordCall:currentCall time:allTime hit:YES];
				currentCall = NULL;
			}

			return [self stamp:item key:key agent:agent category:cat];
		}
	}

	if (statistics_enabled)
	{
		allTime = milliseconds_since(allStart);
		[self recordSearch:currentCall infoSystem:"Failed" time:allTime hit:YES];
		[self recordCall:currentCall time:allTime hit:NO];

		currentCall = NULL;
	}

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
 * Essentially the same as gethostbyname, but we continue
 * searching until we find a record with an ipv6_address attribute.
 */
- (LUDictionary *)ipv6NodeWithName:(char *)name
{
	char **lookupOrder;
	const char *sname;
	LUDictionary *item;
	LUAgent *agent;
	int i, len;
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;
	BOOL found;

	if (name == NULL) return nil;

	if (statistics_enabled)
	{
		gettimeofday(&allStart, (struct timezone *)NULL);
		currentCall = "ipv6 node name";
	}

	lookupOrder = [self lookupOrderForCategory:LUCategoryHost];
	item = nil;
	len = listLength(lookupOrder);
	for (i = 0; i < len; i++)
	{
		agent = [self agentNamed:lookupOrder[i]];
		if (agent == nil) continue;
		sname = [agent shortName];

		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);
			currentAgent = agent;
			state = ServerStateQuerying;
		}

		item = nil;
		if (streq(sname, "Cache")) item = [agent itemWithKey:"namev6" value:name category:LUCategoryHost];
		else if (streq(sname, "DNS")) item = [agent itemWithKey:"namev6" value:name category:LUCategoryHost];
		else item = [agent itemWithKey:"name" value:name category:LUCategoryHost];

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
		}

		found = NO;
		if (item != nil)
		{
			/* Check for ipv6_address attribute */
			if ([item valueForKey:"ipv6_address"] == NULL)
			{
				[item release];
				item = nil;
			}
			else found = YES;
		}

		if (statistics_enabled) [self recordSearch:currentCall infoSystem:sname time:sysTime hit:found];

		if (found)
		{
			if (statistics_enabled)
			{
				allTime = milliseconds_since(allStart);
				[self recordCall:currentCall time:allTime hit:found];
				currentCall = NULL;
			}
			return [self stamp:item key:"name" agent:agent category:LUCategoryHost];
		}
	}

	if (statistics_enabled)
	{
		allTime = milliseconds_since(allStart);
		[self recordSearch:currentCall infoSystem:"Failed" time:allTime hit:YES];
		[self recordCall:currentCall time:allTime hit:NO];
		currentCall = NULL;
	}

	return nil;
}

- (LUDictionary *)serviceWithName:(char *)name
	protocol:(char *)prot
{
	char **lookupOrder;
	const char *sname;
	LUDictionary *item;
	LUAgent *agent;
	int i, len;
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;

	if (name == NULL) return nil;

	if (statistics_enabled)
	{
		gettimeofday(&allStart, (struct timezone *)NULL);
		currentCall = "service name";
	}

	lookupOrder = [self lookupOrderForCategory:LUCategoryService];
	item = nil;
	len = listLength(lookupOrder);
	for (i = 0; i < len; i++)
	{
		agent = [self agentNamed:lookupOrder[i]];
		if (agent == nil) continue;
		sname = [agent shortName];

		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);
			currentAgent = agent;
			state = ServerStateQuerying;
		}

		item = [agent serviceWithName:name protocol:prot];

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
			[self recordSearch:currentCall infoSystem:sname time:sysTime hit:(item != nil)];
		}

		if (item != nil)
		{
			if (statistics_enabled)
			{
				allTime = milliseconds_since(allStart);
				[self recordCall:currentCall time:allTime hit:YES];
				currentCall = NULL;
			}
			return [self stamp:item key:"name" agent:agent category:LUCategoryService];
		}
	}

	if (statistics_enabled)
	{
		allTime = milliseconds_since(allStart);
		[self recordSearch:currentCall infoSystem:"Failed" time:allTime hit:YES];
		[self recordCall:currentCall time:allTime hit:NO];
		currentCall = NULL;
	}

	return nil;
}

- (LUDictionary *)serviceWithNumber:(int *)number
	protocol:(char *)prot
{
	char **lookupOrder;
	const char *sname;
	LUDictionary *item;
	LUAgent *agent;
	int i, len;
	struct timeval allStart;
	struct timeval sysStart;
	unsigned int sysTime;
	unsigned int allTime;

	if (number == NULL) return nil;

	if (statistics_enabled)
	{
		gettimeofday(&allStart, (struct timezone *)NULL);
		currentCall = "service number";
	}

	lookupOrder = [self lookupOrderForCategory:LUCategoryService];
	item = nil;
	len = listLength(lookupOrder);
	for (i = 0; i < len; i++)
	{
		agent = [self agentNamed:lookupOrder[i]];
		if (agent == nil) continue;
		sname = [agent shortName];

		if (statistics_enabled)
		{
			gettimeofday(&sysStart, (struct timezone *)NULL);
			currentAgent = agent;
			state = ServerStateQuerying;
		}

		item = [agent serviceWithNumber:number protocol:prot];

		if (statistics_enabled)
		{
			state = ServerStateActive;
			currentAgent = nil;
			sysTime = milliseconds_since(sysStart);
			[self recordSearch:currentCall infoSystem:sname time:sysTime hit:(item != nil)];
		}

		if (item != nil)
		{
			if (statistics_enabled)
			{
				allTime = milliseconds_since(allStart);
				[self recordCall:currentCall time:allTime hit:YES];
				currentCall = NULL;
			}

			return [self stamp:item key:"number" agent:agent category:LUCategoryService];
		}
	}

	if (statistics_enabled)
	{
		allTime = milliseconds_since(allStart);
		[self recordSearch:currentCall infoSystem:"Failed" time:allTime hit:YES];
		[self recordCall:currentCall time:allTime hit:NO];
		currentCall = NULL;
	}

	return nil;
}

- (BOOL)isSecurityEnabledForOption:(char *)option
{
	ni_id dir;
	void *d, *p;
	ni_status status;
	unsigned long i;
	ni_namelist nl;

	if (option == NULL) return NO;

	dir.nii_object = 0;
	d = NULL;

	syslock_lock(rpcLock);
	status = ni_open(NULL, ".", &d);
	syslock_unlock(rpcLock);
	if (status != NI_OK) return NO;

	while (d != NULL)
	{
		NI_INIT(&nl);

		syslock_lock(rpcLock);
		ni_setreadtimeout(d, 10);
		ni_setabort(d, 1);
		status = ni_lookupprop(d, &dir, "security_options", &nl);
		syslock_unlock(rpcLock);

		if (status == NI_OK)
		{
			for (i = 0; i < nl.ni_namelist_len; i++)
			{
				if (streq(nl.ni_namelist_val[i], option) || streq(nl.ni_namelist_val[i], "all"))
				{
					ni_namelist_free(&nl);
					syslock_lock(rpcLock);
					ni_free(d);
					syslock_unlock(rpcLock);
					return YES;
				}
			}

			ni_namelist_free(&nl);
		}

		syslock_lock(rpcLock);
		status = ni_open(d, "..", &p);
		ni_free(d);
		syslock_unlock(rpcLock);

		d = NULL;
		if (status == NI_OK) d = p;
	}

	return NO;
}

- (BOOL)isNetwareEnabled
{
	return NO;
}

@end
