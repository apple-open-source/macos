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

@implementation LUAgent

- (LUAgent *)init
{
	[super init];
	didInit = YES;
	return self;
}

- (const char *)categoryName:(LUCategory)cat
{
	switch (cat)
	{
		case LUCategoryUser: return "user";
		case LUCategoryGroup: return "group";
		case LUCategoryHost: return "host";
		case LUCategoryNetwork: return "network";
		case LUCategoryService: return "service";
		case LUCategoryProtocol: return "protocol";
		case LUCategoryRpc: return "rpc";
		case LUCategoryMount: return "mount";
		case LUCategoryPrinter: return "printer";
		case LUCategoryBootparam: return "bootparam";
		case LUCategoryBootp: return "bootp";
		case LUCategoryAlias: return "alias";
		case LUCategoryNetDomain: return "netdomain";
		case LUCategoryEthernet: return "ethernet";
		case LUCategoryNetgroup: return "netgroup";
		case LUCategoryInitgroups: return "initgroup";
		case LUCategoryHostServices: return "hostservice";
		default: return NULL;
	}

	return NULL;
}

- (const char *)categoryPathname:(LUCategory)cat
{
	switch (cat)
	{
		case LUCategoryUser: return "users";
		case LUCategoryGroup: return "groups";
		case LUCategoryHost: return "hosts";
		case LUCategoryNetwork: return "networks";
		case LUCategoryService: return "services";
		case LUCategoryProtocol: return "protocols";
		case LUCategoryRpc: return "rpcs";
		case LUCategoryMount: return "mounts";
		case LUCategoryPrinter: return "printers";
		case LUCategoryBootparam: return "bootparams";
		case LUCategoryBootp: return "bootp";
		case LUCategoryAlias: return "aliases";
		case LUCategoryNetDomain: return "netdomains";
		case LUCategoryEthernet: return "ethernets";
		case LUCategoryNetgroup: return "netgroups";
		case LUCategoryInitgroups: return "initgroups";
		case LUCategoryHostServices: return "hostservices";
		default: return NULL;
	}

	return NULL;
}

- (char **)variationsOfEthernetAddress:(void *)addr
{
	char **etherAddrs = NULL;
	char e[6][3], str[64];
	struct ether_addr *ether, etherZero;
	int i, j, bit;

	if (addr == NULL)
	{
		bzero(&etherZero, sizeof(struct ether_addr));
		ether = &etherZero;
	}
	else
	{
		ether = (struct ether_addr *)addr;
	}

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

- (char *)canonicalEthernetAddress:(void *)addr
{
	char e[6][3];
	static char str[64];
	struct ether_addr *ether;
	int i, bit;

	if (addr == NULL)
	{
		sprintf(str, "00:00:00:00:00:00");
		return str;
	}

	ether = (struct ether_addr *)addr;

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
	char **alist;
	char **blist;
	char **new = NULL;
	int i, len, n;

	if (a == nil || b == nil) return;
	
	/* merge names */
	alist = [a valuesForKey:"name"];
	blist = [b valuesForKey:"name"];

	len = [b countForKey:"name"];
	if (len < 0) len = 0;

	n = 0;
	for (i = 0; i < len; i++)
	{
		if (listIndex(blist[i], alist) == IndexNull)
		{
			new = appendString(blist[i], new);
			n++;
		}
	}
	if (n > 0)
	{
		[a addValues:new forKey:"name" count:n];
		freeList(new);
		new = NULL;
	}

	/* merge netgroup members */
	alist = [a valuesForKey:"netgroups"];
	blist = [b valuesForKey:"netgroups"];

	len = [b countForKey:"netgroups"];
	if (len < 0) len = 0;

	n = 0;
	for (i = 0; i < len; i++)
	{
		if (listIndex(blist[i], alist) == IndexNull)
		{
			new = appendString(blist[i], new);
			n++;
		}
	}
	if (n > 0)
	{
		[a addValues:new forKey:"netgroups" count:n];
		freeList(new);
		new = NULL;
	}

	/* merge hosts */
	alist = [a valuesForKey:"hosts"];
	blist = [b valuesForKey:"hosts"];

	len = [b countForKey:"hosts"];
	if (len < 0) len = 0;

	n = 0;
	for (i = 0; i < len; i++)
	{
		if (listIndex(blist[i], alist) == IndexNull)
		{
			new = appendString(blist[i], new);
			n++;
		}
	}
	if (n > 0)
	{
		[a addValues:new forKey:"hosts" count:n];
		freeList(new);
		new = NULL;
	}

	/* merge users */
	alist = [a valuesForKey:"users"];
	blist = [b valuesForKey:"users"];

	len = [b countForKey:"users"];
	if (len < 0) len = 0;

	n = 0;
	for (i = 0; i < len; i++)
	{
		if (listIndex(blist[i], alist) == IndexNull)
		{
			new = appendString(blist[i], new);
			n++;
		}
	}
	if (n > 0)
	{
		[a addValues:new forKey:"users" count:n];
		freeList(new);
		new = NULL;
	}

	/* merge domains */
	alist = [a valuesForKey:"domains"];
	blist = [b valuesForKey:"domains"];

	len = [b countForKey:"domains"];
	if (len < 0) len = 0;

	n = 0;
	for (i = 0; i < len; i++)
	{
		if (listIndex(blist[i], alist) == IndexNull)
		{
			new = appendString(blist[i], new);
			n++;
		}
	}
	if (n > 0)
	{
		[a addValues:new forKey:"domains" count:n];
		freeList(new);
		new = NULL;
	}
}

- (const char *)name
{return NULL;}

- (const char *)shortName
{return NULL;}

- (BOOL)isValid:(LUDictionary *)item
{return NO;}

- (LUDictionary *)userWithName:(char *)name
{return nil;}

- (LUDictionary *)userWithNumber:(int *)number
{return nil;}
- (LUArray *)allUsers {return nil;}

- (LUDictionary *)groupWithName:(char *)name
{return nil;}

- (LUDictionary *)groupWithNumber:(int *)number
{return nil;}

- (LUArray *)allGroups
{return nil;}

- (LUArray *)allGroupsWithUser:(char *)name
{return nil;}

- (LUDictionary *)hostWithName:(char *)name
{return nil;}

- (LUDictionary *)hostWithInternetAddress:(struct in_addr *)addr
{return nil;}

- (LUDictionary *)hostWithEthernetAddress:(struct ether_addr *)addr
{return nil;}

- (LUArray *)allHosts
{return nil;}

- (LUDictionary *)networkWithName:(char *)name
{return nil;}

- (LUDictionary *)networkWithInternetAddress:(struct in_addr *)addr
{return nil;}

- (LUArray *)allNetworks
{return nil;}

- (LUDictionary *)serviceWithName:(char *)name protocol:(char *)prot
{return nil;}

- (LUDictionary *)serviceWithNumber:(int *)number protocol:(char *)prot
{return nil;}

- (LUArray *)allServices
{return nil;}

- (LUDictionary *)protocolWithName:(char *)name
{return nil;}

- (LUDictionary *)protocolWithNumber:(int *)number
{return nil;}

- (LUArray *)allProtocols 
{return nil;}

- (LUDictionary *)rpcWithName:(char *)name
{return nil;}

- (LUDictionary *)rpcWithNumber:(int *)number
{return nil;}

- (LUArray *)allRpcs
{return nil;}

- (LUDictionary *)mountWithName:(char *)name
{return nil;}

- (LUArray *)allMounts
{return nil;}

- (LUDictionary *)printerWithName:(char *)name
{return nil;}

- (LUArray *)allPrinters
{return nil;}

- (LUDictionary *)bootparamsWithName:(char *)name
{return nil;}

- (LUArray *)allBootparams
{return nil;}

- (LUDictionary *)bootpWithInternetAddress:(struct in_addr *)addr
{return nil;}

- (LUDictionary *)bootpWithEthernetAddress:(struct ether_addr *)addr
{return nil;}

- (LUDictionary *)aliasWithName:(char *)name
{return nil;}

- (LUArray *)allAliases
{return nil;}

- (LUDictionary *)netgroupWithName:(char *)name
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

- (LUDictionary *)hostsWithService:(char *)name protocol:(char *)protocol
{return nil;}

@end
