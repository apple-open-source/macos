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

+ (char *)canonicalAgentName:(char *)name
{
	char *arg, *colon, *cname;
	int len;

	if (name == NULL) return NULL;

	arg = NULL;
	colon = strchr(name, ':');
	if (colon != NULL)
	{
		*colon = '\0';
		arg = colon + 1;
	}

	cname = NULL;
	len = strlen(name);
	if ((name[0] == '/') || (name[0] == '.'))
	{
		cname = copyString(name);
	}
	else if (len > 5)
	{
		if (streq(name + (len - 5), "Agent"))
		{
			cname = copyString(name);
		}
	}

	if (cname == NULL)
	{
		cname = malloc(len + 6);
		sprintf(cname, "%sAgent", name);
	}

	if (colon != NULL) *colon = ':';

	return cname;
}

+ (char *)canonicalServiceName:(char *)name
{
	char *arg, *colon, *cname, *cserv;
	int len;

	if (name == NULL) return NULL;

	arg = NULL;
	colon = strchr(name, ':');
	if (colon != NULL)
	{
		*colon = '\0';
		arg = colon + 1;
	}

	cname = NULL;
	len = strlen(name);
	if ((name[0] == '/') || (name[0] == '.'))
	{
		cname = copyString(name);
	}
	else if (len > 5)
	{
		if (streq(name + (len - 5), "Agent"))
		{
			cname = copyString(name);
		}
	}

	if (cname == NULL)
	{
		cname = malloc(len + 6);
		sprintf(cname, "%sAgent", name);
	}

	if (colon != NULL) *colon = ':';

	cserv = NULL;
	if (arg == NULL)
	{
		cserv = copyString(cname);
	}
	else 
	{
		cserv = malloc(strlen(cname) + strlen(arg) + 2);
		sprintf(cserv, "%s:%s", cname, arg);
	}

	free(cname);

	return cserv;
}

- (LUAgent *)init
{
	if (didInit) return self;

	[super init];

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

- (const char *)serviceName
{
	return serviceName;
}

- (const char *)shortName
{
	return serviceName;
}

- (BOOL)inNetgroup:(char *)group
	host:(char *)host
	user:(char *)user
	domain:(char *)domain
{
	return NO;
}

- (LUDictionary *)statistics
{
	return nil;
}

- (void)resetStatistics
{}

- (LUDictionary *)ipv6NodeWithName:(char *)name
{
	return nil;
}

- (LUDictionary *)allGroupsWithUser:(char *)name
{
	LUDictionary *q, *u, *item;
	LUArray *all;
	int i, len;
	char str[16];

	if (name == NULL) return nil;

	q = [[LUDictionary alloc] init];
	sprintf(str, "%u", LUCategoryGroup);
	[q setValue:str forKey:"_lookup_category"];
	[q setValue:name forKey:"users"];

	all = [self query:q];
	[q release];

	u = [self itemWithKey:"name" value:name category:LUCategoryUser];

	if ((all == nil) && (u == nil)) return nil;

	item = [[LUDictionary alloc] initTimeStamped];
	[item setValue:name forKey:"name"];

	if (u != nil)
	{
		[item mergeKey:"gid" from:u];
		[u release];
	}

	if (all != nil)
	{
		len = [all count];
		for (i = 0; i < len; i++)
		{
			[item mergeKey:"gid" from:[all objectAtIndex:i]];
		}
		[all release];
	}

	return item;
}

- (LUDictionary *)netgroupWithName:(char *)name
{
	LUDictionary *q;
	LUDictionary *item;
	LUArray *all;
	char str[16], **p;
	int i, len;
	BOOL found;

	if (name == NULL) return nil;

	item = [[LUDictionary alloc] initTimeStamped];
	[item setValue:name forKey:"name"];
	found = NO;

	q = [[LUDictionary alloc] init];
	sprintf(str, "%u", LUCategoryHost);
	[q setValue:str forKey:"_lookup_category"];
	[q setValue:name forKey:"netgroups"];

	all = [self query:q];
	[q release];

	if (all != nil)
	{
		len = [all count];
		for (i = 0; i < len; i++)
		{
			p = [[all objectAtIndex:i] valuesForKey:"name"];
			if (p != NULL)
			{
				[item mergeValues:p forKey:"hosts"];
				found = YES;
			}
		}

		[all release];
	}

	q = [[LUDictionary alloc] init];
	sprintf(str, "%u", LUCategoryUser);
	[q setValue:str forKey:"_lookup_category"];
	[q setValue:name forKey:"netgroups"];

	all = [self query:q];
	[q release];

	if (all != nil)
	{
		len = [all count];
		for (i = 0; i < len; i++)
		{
			p = [[all objectAtIndex:i] valuesForKey:"name"];
			if (p != NULL)
			{
				[item mergeValues:p forKey:"users"];
				found = YES;
			}
		}

		[all release];
	}

	q = [[LUDictionary alloc] init];
	sprintf(str, "%u", LUCategoryNetDomain);
	[q setValue:str forKey:"_lookup_category"];
	[q setValue:name forKey:"netgroups"];

	all = [self query:q];
	[q release];

	if (all != nil)
	{
		len = [all count];
		for (i = 0; i < len; i++)
		{
			p = [[all objectAtIndex:i] valuesForKey:"name"];
			if (p != NULL)
			{
				[item mergeValues:p forKey:"domains"];
				found = YES;
			}
		}

		[all release];
	}
	
	if (!found)
	{
		[item release];
		return nil;
	}

	return item;
}

- (LUDictionary *)serviceWithName:(char *)name protocol:(char *)prot
{
	LUDictionary *q;
	LUArray *all;
	char str[16];

	if (name == NULL) return nil;

	q = [[LUDictionary alloc] init];
	sprintf(str, "%u", LUCategoryService);
	[q setValue:str forKey:"_lookup_category"];
	[q setValue:"YES" forKey:"_lookup_single"];
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
	sprintf(str, "%u", LUCategoryService);
	[q setValue:str forKey:"_lookup_category"];
	[q setValue:"YES" forKey:"_lookup_single"];
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

	cat = atoi([pattern valueAtIndex:where]);
	if (cat >= NCATEGORIES) return nil;

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
	char str[16];

	if (key == NULL) return nil;
	if (cat >= NCATEGORIES) return nil;

	q = [[LUDictionary alloc] init];
	sprintf(str, "%u", cat);
	[q setValue:str forKey:"_lookup_category"];
	[q setValue:"YES" forKey:"_lookup_single"];
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
