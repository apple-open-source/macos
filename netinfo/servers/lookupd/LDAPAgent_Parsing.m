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
 * LDAPAgent_Parsing.m
 * LDAP agent entry parsers
 * Copyright (C) 1997 Luke Howard. All rights reserved.
 * Luke Howard, March 1997.
 */

#import "LDAPAgent.h"
#import "LDAPAgent_Parsing.h"
#import "LDAPAttributes.h"
#import "LULDAPDictionary.h"

#import "FFParser.h"

#import <string.h>
#import <sys/param.h>
#import <stdlib.h>

#define ReturnIfFalse(b)  do {					\
				if (b == NO) return (nil);	\
			} while (0)


@implementation LDAPAgent (Parsing) 

- (LUDictionary *)parseUser:(LULDAPDictionary *)user
{
	BOOL b;

	/* Mandatory attributes. */
	b = [user bindAttribute:OID_UID toKey:"name"];
	ReturnIfFalse(b);

	b = [user bindAttribute:OID_UIDNUMBER toKey:"uid"];
	ReturnIfFalse(b);

	b = [user bindAttribute:OID_GECOS toKey:"realname"];
	if (b == NO)
	{
		b = [user bindAttribute:OID_CN toKey:"realname"];
	}
	ReturnIfFalse(b);

	b = [user bindAttribute:OID_GIDNUMBER toKey:"gid"];
	ReturnIfFalse(b);

	b = [user bindAttribute:OID_HOMEDIRECTORY toKey:"home"];
	ReturnIfFalse(b);

	/* Non-mandatory attributes. */
	if ([user bindAttributeCrypted:OID_USERPASSWORD toKey:"passwd"] == NO)
	{
		[user setValue:"*" forKey:"passwd"];
	}

	if ([user bindAttribute:OID_LOGINSHELL toKey:"shell"] == NO)
	{
		[user setValue:"" forKey:"shell"];
	}

	return user;
}

- (LUDictionary *)parseGroup:(LULDAPDictionary *)group
{
	BOOL b;

	b = [group bindAttribute:OID_CN toKey:"name"];
	ReturnIfFalse(b);

	if ([group bindAttributeCrypted:OID_USERPASSWORD toKey:"passwd"] == NO)
	{
		[group setValue:"*" forKey:"passwd"];
	}

	b = [group bindAttribute:OID_GIDNUMBER toKey:"gid"];
	ReturnIfFalse(b);

	b = [group bindAttribute:OID_MEMBERUID toKey:"users"];
	ReturnIfFalse(b);

	return group;
}

- (LUDictionary *)parseHost:(LULDAPDictionary *)host
{
	BOOL b;

	b = [host bindRdnToName:OID_CN];
	ReturnIfFalse(b);

	(void) [host bindAttribute:OID_CN toKey:"name"
		exceptValue:[host valueForKey:"name"]];

	b = [host bindAttribute:OID_IPHOSTNUMBER toKey:"ip_address"];
	ReturnIfFalse(b);

	b = [host bindAttribute:OID_MACADDRESS toKey:"en_address"];

	return host;
}

- (LUDictionary *)parseNetwork:(LULDAPDictionary *)network
{
	BOOL b;

	b = [network bindRdnToName:OID_CN];
	ReturnIfFalse(b);

	(void) [network bindAttribute:OID_CN toKey:"name"
		exceptValue:[network valueForKey:"name"]];

	b = [network bindAttribute:OID_IPNETWORKNUMBER toKey:"address"];
	ReturnIfFalse(b);

	return network;
}

- (LUDictionary *)parseService:(LULDAPDictionary *)service
{
	BOOL b;

	b = [service bindRdnToName:OID_CN];
	ReturnIfFalse(b);
	
	(void) [service bindAttribute:OID_CN toKey:"name"
		exceptValue:[service valueForKey: "name"]];

	b = [service bindAttribute:OID_IPSERVICEPORT toKey:"port"];
	ReturnIfFalse(b);

	b = [service bindAttribute:OID_IPSERVICEPROTOCOL toKey:"protocol"];
	ReturnIfFalse(b);

	return service;
}

- (LUDictionary *)parsePrinter:(LULDAPDictionary *)printer
{
	BOOL b;

	/*
	 * This is based on "Adding a TCP/IP Printer to a
	 * NEXTSTEP computer network" from the Summer 1993
	 * issue of NEXTSTEP In Focus (and help from Kurt
	 * Werle).
	 */

	b = [printer bindRdnToName:OID_CN];
	ReturnIfFalse(b);

	/*
	 * lp needs to be present in the directory,
	 * but doesn't need a value.
	 */
	[printer addKey:"lp"];

	/*
	 * lo gives the printer a resource lock. Its
	 * value must be "lock".
	 */
	[printer setValue:"lock" forKey:"lo"];

	/*
	 * rm is the name of the remote printer's server
	 */

	b = [printer bindAttribute:OID_LPRHOST toKey:"rm"];
	ReturnIfFalse(b);

	/*
	 * rp is the remote printer's name
	 */
	b = [printer bindAttribute:OID_LPRQUEUE toKey:"rp"];
	ReturnIfFalse(b);

	/*
	 * ty is the printer's type.
	 */
	b = [printer bindAttribute:OID_LPRTYPE toKey:"ty"];
	ReturnIfFalse(b);

	/*
	 * _nxfinalform causes PostScript comments to be
	 * interpreted correctly.
	 */
	[printer addKey:"_nxfinalform"];

	(void) [printer bindAttribute:OID_DESCRIPTION toKey:"note"];
	
	return printer;
}

- (LUDictionary *)parseProtocol:(LULDAPDictionary *)protocol
{
	BOOL b;

	b = [protocol bindRdnToName:OID_CN];
	ReturnIfFalse(b);
	
	b = [protocol bindAttribute:OID_IPPROTOCOLNUMBER toKey:"number"];
	ReturnIfFalse(b);

	(void) [protocol bindAttribute:OID_CN toKey:"name"
		exceptValue:[protocol valueForKey: "name"]];

	return protocol;
}

- (LUDictionary *)parseRpc:(LULDAPDictionary *)rpc
{
	BOOL b;

	b = [rpc bindRdnToName:OID_CN];
	ReturnIfFalse(b);
	
	b = [rpc bindAttribute:OID_ONCRPCNUMBER toKey:"number"];
	ReturnIfFalse(b);

	(void) [rpc bindAttribute:OID_CN toKey:"name"
		exceptValue:[rpc valueForKey: "name"]];

	return rpc;
}

- (LUDictionary *)parseMount:(LULDAPDictionary *)mount
{
	BOOL b;

	b = [mount bindAttribute:OID_CN toKey:"name"];
	ReturnIfFalse(b);

	b = [mount bindAttribute:OID_MOUNTDIRECTORY toKey:"dir"];
	ReturnIfFalse(b);

	b = [mount bindAttribute:OID_MOUNTTYPE toKey:"type"];
	ReturnIfFalse(b);

	b = [mount bindAttribute:OID_MOUNTOPTION toKey:"opts"];
	ReturnIfFalse(b);

	b = [mount bindAttribute:OID_MOUNTDUMPFREQUENCY toKey:"dump_freq"];
	ReturnIfFalse(b);

	b = [mount bindAttribute:OID_MOUNTPASSNUMBER toKey:"passno"];
	ReturnIfFalse(b);

	return mount;
}

- (LUDictionary *)parseBootparam:(LULDAPDictionary *)bootparam
{
	BOOL b;

	b = [bootparam bindRdnToName:OID_CN];
	ReturnIfFalse(b);

	(void) [bootparam bindAttribute:OID_CN toKey:"name"
		exceptValue:[bootparam valueForKey: "name"]];

	b = [bootparam bindAttribute:OID_BOOTPARAMETER toKey:"bootparam"];
	ReturnIfFalse(b);

	return bootparam;
}

- (LUDictionary *)parseBootp:(LULDAPDictionary *)bootp
{
	BOOL b;

	b = [bootp bindRdnToName:OID_CN];
	ReturnIfFalse(b);

	(void) [bootp bindAttribute:OID_CN toKey:"name"
		exceptValue:[bootp valueForKey: "name"]];

	b = [bootp bindAttribute:OID_MACADDRESS toKey:"en_address"];
	ReturnIfFalse(b);

	// we can't expect these attributes to be there, although they may be.
	b = [bootp bindAttribute:OID_BOOTFILE toKey:"bootfile"];
//	ReturnIfFalse(b);

	b = [bootp bindAttribute:OID_IPHOSTNUMBER toKey:"ip_address"];
//	ReturnIfFalse(b);


	return bootp;
}

- (LUDictionary *)parseAlias:(LULDAPDictionary *)alias
{
	BOOL b;

	b = [alias bindAttribute:OID_CN toKey:"name"];
	ReturnIfFalse(b);

	b = [alias bindAttribute:OID_MAIL toKey:"members"];
	ReturnIfFalse(b);

	return alias;
}

- (LUDictionary *)parseNetgroup:(LULDAPDictionary *)netgroup
{
	char **triples, **t;
	char *ngname;
	int ngnamelen;

	if ([netgroup bindRdnToName:OID_CN] == NO)
	{
		return nil;
	}

	ngname = [netgroup valueForKey:"name"];
	ngnamelen = strlen(ngname);
	(void) [netgroup bindAttribute:OID_MEMBERNISNETGROUP toKey:"netgroups"];
	triples = [netgroup entryValuesForAttribute:OID_NISNETGROUPTRIPLE];
	if (triples != NULL)
	{
		for (t = triples; *t != NULL; t++)
		{
			LUDictionary *p;
			char *ng = malloc(ngnamelen + strlen(*t) + 3);
			sprintf(ng, "%s %s", ngname, *t);
 
			p = [parser parseNetgroup:ng];
			[self mergeNetgroup:p into:netgroup];
			[p release];
			free(ng);
		}
	}
	return netgroup;
}

- (LUDictionary *)parse:(LULDAPDictionary *)data category:(LUCategory)cat
{
	LUDictionary *item;
	char scratch[256];

	switch (cat)
	{
		case LUCategoryUser:
			item = [self parseUser:data];
			break;
		case LUCategoryGroup:
			item = [self parseGroup:data];
			break;
		case LUCategoryHost:
			item = [self parseHost:data];
			break;
		case LUCategoryNetwork:
			item = [self parseNetwork:data];
			break;
		case LUCategoryService:
			item = [self parseService:data];
			break;
		case LUCategoryProtocol:
			item = [self parseProtocol:data];
			break;
		case LUCategoryRpc:
			item = [self parseRpc:data];
			break;
		case LUCategoryMount: 
			item = [self parseMount:data];
			break;
		case LUCategoryPrinter: 
			item = [self parsePrinter:data];
			break;
		case LUCategoryBootparam: 
			item = [self parseBootparam:data];
			break;
		case LUCategoryBootp: 
			item = [self parseBootp:data];
			break;
		case LUCategoryAlias: 
			item = [self parseAlias:data];
			break;
		case LUCategoryNetgroup: 
			item = [self parseNetgroup:data];
			break;
		case LUCategoryInitgroups:
			item = [self parseGroup:data];
			break;
		default: 
			item = nil;
	}

	if (item != nil)
	{	
		sprintf(scratch, "LDAPAgent: %s %s",
			[LUAgent categoryName:cat], [item valueForKey:"name"]);
		[item setBanner:scratch];
	}
	
	return item;
}

@end

