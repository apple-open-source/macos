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
#import <stdio.h>
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
	[super init];
	didInit = YES;
	return self;
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
{return NULL;}

- (const char *)shortName
{return NULL;}

- (BOOL)isValid:(LUDictionary *)item
{return NO;}

- (LUDictionary *)serviceWithName:(char *)name protocol:(char *)prot
{return nil;}

- (LUDictionary *)serviceWithNumber:(int *)number protocol:(char *)prot
{return nil;}

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
{return nil;}

- (LUDictionary *)hostsWithService:(char *)name protocol:(char *)protocol
{return nil;}

- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{return nil;}

- (LUArray *)allItemsWithCategory:(LUCategory)cat
{return nil;}

- (LUArray *)query:(LUDictionary *)pattern
{return nil;}

- (LUArray *)query:(LUDictionary *)pattern category:(LUCategory)cat
{return nil;}

@end
