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
 * LUAgent.m
 *
 * Generic client for lookupd
 *
 * This is an (almost) abstract superclass for specific 
 * lookup clients such as NetInfo, DNS, or NIS.
 *
 * The implementation maintains statistics.
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "LUAgent.h"
#import "LUGlobal.h"
#import "Config.h"
#import "LUCachedDictionary.h"
#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <NetInfo/dsutil.h>

extern struct ether_addr *ether_aton(char *);

char *categoryName[] =
{
	"user",
	"group",
	"host",
	"network",
	"service",
	"protocol",
	"rpc",
	"mount",
	"printer",
	"bootparam",
	"bootp",
	"alias",
	"netdomain",
	"ethernet",
	"netgroup",
	"initgroup",
	"hostservice"
};

char *categoryPathname[] =
{
	"users",
	"groups",
	"hosts",
	"networks",
	"services",
	"protocols",
	"rpcs",
	"mounts",
	"printers",
	"bootparams",
	"bootp",
	"aliases",
	"netdomains",
	"ethernets",
	"netgroups",
	"initgroups",
	"hostservices"
};

@implementation LUAgent

- (LUAgent *)init
{
	if (didInit) return self;

	[super init];

	generation = [configManager generation];
	configurationArray = [configManager config];

	didInit = YES;

	serviceName = malloc(strlen([[self class] name]) + 1);
	sprintf(serviceName, "%s", [[self class] name]);

	return self;
}

- (LUAgent *)initWithArg:(char *)arg
{
	if (didInit) return self;

	[super init];

	generation = [configManager generation];
	configurationArray = [configManager config];

	didInit = YES;

	if (arg == NULL)
	{
		serviceName = malloc(strlen([[self class] name]) + 1);
		sprintf(serviceName, "%s", [[self class] name]);
		return self;
	}

	serviceName = malloc(strlen([[self class] name]) + strlen(arg) + 2);
	sprintf(serviceName, "%s:%s", [[self class] name], arg);

	return self;
}

- (void)dealloc
{
	[configurationArray release];
	free(serviceName);
	[super dealloc];
}

+ (const char *)categoryName:(LUCategory)cat
{
	if (cat >= NCATEGORIES) return NULL;
	return categoryName[cat];
}

+ (const char *)categoryPathname:(LUCategory)cat
{
	if (cat >= NCATEGORIES) return NULL;
	return categoryPathname[cat];
}

+ (int)categoryWithName:(char *)name
{
	if (streq(name, "user")) return LUCategoryUser;
	if (streq(name, "group")) return LUCategoryGroup;
	if (streq(name, "host")) return LUCategoryHost;
	if (streq(name, "network")) return LUCategoryNetwork;
	if (streq(name, "service")) return LUCategoryService;
	if (streq(name, "protocol")) return LUCategoryProtocol;
	if (streq(name, "rpc")) return LUCategoryRpc;
	if (streq(name, "mount")) return LUCategoryMount;
	if (streq(name, "printer")) return LUCategoryPrinter;
	if (streq(name, "bootparam")) return LUCategoryBootparam;
	if (streq(name, "bootp")) return LUCategoryBootp;
	if (streq(name, "alias")) return LUCategoryAlias;
	if (streq(name, "netdomain")) return LUCategoryNetDomain;
	if (streq(name, "ethernet")) return LUCategoryEthernet;
	if (streq(name, "netgroup")) return LUCategoryNetgroup;
	if (streq(name, "initgroup")) return LUCategoryInitgroups;
	if (streq(name, "hostservice")) return LUCategoryHostServices;
	return -1;
}

+ (char **)variationsOfEthernetAddress:(char *)addr
{
	char **etherAddrs = NULL;
	char e[6][3], str[64];
	struct ether_addr *ether;
	int i, j, bit;

	if (addr == NULL) return NULL;

	ether = ether_aton(addr);
	if (ether == NULL) return NULL;

	for (i = 0; i < 64; i++)
	{
		for (j = 0, bit = 1; j < 6; j++, bit *= 2)
		{
			if ((i & bit) && (ether->ether_addr_octet[j] <= 15))
			{
				sprintf(e[j], "0%x", ether->ether_addr_octet[j]);
			}
			else
			{
				sprintf(e[j], "%x", ether->ether_addr_octet[j]);
			}
		}
		sprintf(str, "%s:%s:%s:%s:%s:%s", e[0],e[1],e[2],e[3],e[4],e[5]);
		if (listIndex(str, etherAddrs) == IndexNull)
		{
			etherAddrs = appendString(str, etherAddrs);
		}
	}

	return etherAddrs;
}

+ (char *)canonicalEthernetAddress:(char *)addr
{
	char e[6][3];
	static char str[64];
	struct ether_addr *ether;
	int i, bit;

	ether = NULL;
	if (addr != NULL) ether = ether_aton(addr);

	if (ether == NULL)
	{
		sprintf(str, "00:00:00:00:00:00");
		return str;
	}

	for (i = 0, bit = 1; i < 6; i++, bit *= 2)
	{
		if ((i & bit) && (ether->ether_addr_octet[i] <= 15))
		{
			sprintf(e[i], "0%x", ether->ether_addr_octet[i]);
		}
		else
		{
			sprintf(e[i], "%x", ether->ether_addr_octet[i]);
		}
	}

	sprintf(str, "%s:%s:%s:%s:%s:%s", e[0],e[1],e[2],e[3],e[4],e[5]);
	return str;
}

/*
 * merge values from netgroup b into a
 */
- (void)mergeNetgroup:(LUDictionary *)b into:(LUDictionary *)a
{
	if (a == nil || b == nil) return;
	
	[a mergeKey:"name" from:b];
	[a mergeKey:"netgroups" from:b];
	[a mergeKey:"hosts" from:b];
	[a mergeKey:"users" from:b];
	[a mergeKey:"domains" from:b];
}

- (const char *)serviceName
{return serviceName;}

- (const char *)shortName
{return NULL;}

- (BOOL)isValid:(LUDictionary *)item
{return NO;}

- (BOOL)isArrayValid:(LUArray *)array
{
	unsigned int i, len;
	time_t age;
	LUDictionary *stamp;
	LUAgent *agent;

	if (array == nil) return NO;

	len = [array validationStampCount];
	if (len == 0) return YES;

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

- (BOOL)isStale
{
	return (generation != [configManager generation]);
}

- (BOOL)inNetgroup:(char *)group
	host:(char *)host
	user:(char *)user
	domain:(char *)domain
{return NO;}

- (LUDictionary *)statistics
{return nil;}

- (void)resetStatistics
{}

- (LUArray *)allGroupsWithUser:(char *)name
{
	LUDictionary *q, *u, *g;
	LUArray *all;

	if (name == NULL) return nil;

	g = nil;
	u = [self itemWithKey:"name" value:name category:LUCategoryUser];
	if (u != nil)
	{
		g = [self itemWithKey:"gid" value:[u valueForKey:"gid"] category:LUCategoryGroup];
		[u release];
	}

	q = [[LUDictionary alloc] init];
	[q setValue:"group" forKey:"_lookup_category"];
	[q setValue:name forKey:"users"];

	all = [self query:q];
	[q release];

	if (all == nil)
	{
		if (g == nil) return nil;
		all = [[LUArray alloc] init];
	}

	if (g != nil) 
	{
		if (![all containsObject:g]) [all addObject:g];
		[g release];
	}

	return all;
}

- (LUDictionary *)serviceWithName:(char *)name protocol:(char *)prot
{
	LUDictionary *q;
	LUArray *all;

	if (name == NULL) return nil;

	q = [[LUDictionary alloc] init];
	[q setValue:"service" forKey:"_lookup_category"];
	[q setValue:name forKey:"name"];
	if (prot != NULL) [q setValue:prot forKey:"protocol"];

	all = [self query:q];
	[q release];

	if (all == nil) return nil;

	q = [[all objectAtIndex:0] retain];
	[all release];
	return q;
}

- (LUDictionary *)serviceWithNumber:(int *)number protocol:(char *)prot
{
	LUDictionary *q;
	LUArray *all;
	char str[64];

	if (number == NULL) return nil;

	q = [[LUDictionary alloc] init];
	[q setValue:"service" forKey:"_lookup_category"];
	sprintf(str, "%u", *number);
	[q setValue:str forKey:"port"];
	if (prot != NULL) [q setValue:prot forKey:"protocol"];

	all = [self query:q];
	[q release];

	if (all == nil) return nil;

	q = [[all objectAtIndex:0] retain];
	[all release];
	return q;
}

/*
 * Agents may override this method for better performance.
 * Note that it just calls query:category, so that's a better method to override.
 */
- (LUArray *)query:(LUDictionary *)pattern
{
	unsigned int where;
	LUCategory cat;

	if (pattern == nil) return nil;

	where = [pattern indexForKey:"_lookup_category"];
	if (where == IndexNull) return nil;

	cat = [LUAgent categoryWithName:[pattern valueAtIndex:where]];
	if (cat > NCATEGORIES) return nil;

	return [self query:pattern category:cat];
}

/*
 * Agents may override this method for better performance.
 */
- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	LUDictionary *q;
	LUDictionary *item;
	LUArray *all;

	if (key == NULL) return nil;

	q = [[LUDictionary alloc] init];
	[q setValue:categoryName[cat] forKey:"_lookup_category"];
	[q setValue:val forKey:key];

	all = [self query:q];
	[q release];

	if (all == nil) return nil;
	if ([all count] == 0)
	{
		[all release];
		return nil;
	}

	item = [all objectAtIndex:0];
	[item retain];
	[all release];

	return item;
}

/*
 * Agents may override this method for better performance.
 */
- (LUArray *)query:(LUDictionary *)pattern category:(LUCategory)cat
{
	LUArray *all, *list;
	int i, len;

	all = [self allItemsWithCategory:cat];

	if (pattern == nil) return all;
	if ([pattern count] == 0) return all;

	list = nil;
	if (all != nil)
	{
		list = [all filter:pattern];
		if (list != nil)
		{
			len = [all validationStampCount];
			for (i = 0; i < len; i++)
			{
				[list addValidationStamp:[all validationStampAtIndex:i]];
			}
		}
		[all release];
	}

	return list;
}

/*
 * Agents must override this method.
 */
- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	return nil;
}

@end
