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
 * FFAgent.m
 *
 * Flat File agent for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "FFAgent.h"
#import <stdio.h>
#import <sys/types.h>
#import <sys/param.h>
#import <sys/stat.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <string.h>
#import <stdlib.h>
#import <NetInfo/dsutil.h>

#define BUFSIZE 8192

extern char *nettoa(unsigned long);

static FFAgent *_sharedFFAgent = nil;

@implementation FFAgent

- (FFAgent *)init
{
	if (didInit) return self;

	[super init];

	stats = [[LUDictionary alloc] init];
	[stats setBanner:"FFAgent statistics"];
	[stats setValue:"Flat_File" forKey:"information_system"];
	etcDir = copyString("/etc");
	parser = [[FFParser alloc] init];

	threadLock = [[Lock alloc] init];

	return self;
}

+ (FFAgent *)alloc
{
	if (_sharedFFAgent != nil)
	{
		[_sharedFFAgent retain];
		return _sharedFFAgent;
	}

	_sharedFFAgent = [super alloc];
	_sharedFFAgent = [_sharedFFAgent init];
	if (_sharedFFAgent == nil) return nil;

	return _sharedFFAgent;
}

- (char *)getLineFromFile:(FILE *)fp category:(LUCategory)cat
{
	char s[BUFSIZE];
	char c;
	char *out;
	BOOL getNextLine;
	int len;

    s[0] = '\0';

    fgets(s, BUFSIZE, fp);
    if (s == NULL || s[0] == '\0') return NULL;

	if (s[0] == '#')
	{
		out = copyString("#");
		return out;
	}

	len = strlen(s) - 1;
	s[len] = '\0';

	out = copyString(s);

	/* only printcap, bootparams, and aliases can continue on multiple lines */
	if ((cat != LUCategoryPrinter) &&
		(cat != LUCategoryBootparam) &&
		(cat != LUCategoryAlias))
	{
		return out;
	}

	if (cat == LUCategoryAlias)
	{
		/* alias continues if next line starts with whitespace */
		c = getc(fp);
		while ((c == ' ') || (c == '\t'))
		{
			fgets(s, BUFSIZE, fp);
			if (s == NULL || s[0] == '\0') return out;

			len = strlen(s) - 1;
			s[len] = '\0';
			out = concatString(out, s);

			c = getc(fp);
		}

		/* hit next line - unread a character */
		if (c != EOF) fseek(fp, -1, SEEK_CUR);

		return out;
	}

	/* printcap and bootparams continue if last char is a backslash */
	getNextLine = out[len - 1] == '\\';
	if (getNextLine) out[--len] = '\0';

	while (getNextLine)
	{
		fgets(s, BUFSIZE, fp);
		if (s == NULL || s[0] == '\0') return out;

		len = strlen(s) - 1;
		s[len] = '\0';
		
		getNextLine = s[len - 1] == '\\';
		if (getNextLine) s[--len] = '\0';

		out = concatString(out, s);
	}

	return out;
}

- (LUDictionary *)itemWithKeys:(char **)keys
	values:(char **)values
	category:(LUCategory)cat
	file:(char *)fname
{
	FILE *fp;
	char *line;
	LUDictionary *item = nil;
	char **vals;
	char fpath[MAXPATHLEN];
	int i, len;
	BOOL match;
	struct stat st;
	long ts;

	ts = 0;

	if (etcDir != NULL)
	{
		sprintf(fpath, "%s/%s", etcDir, fname);

		if (stat(fpath, &st) < 0) return nil;
		ts = st.st_mtime;
	}

	[threadLock lock]; // locked [[[[[

	if (etcDir != NULL) fp = fopen(fpath, "r");
	else fp = stdin;

	if (fp == NULL)
	{
		[threadLock unlock]; // ] unlocked
		return nil;
	}

	/* bootptab entries start after a "%%" line */
	if (cat == LUCategoryBootp)
	{
		while (NULL != (line = [self getLineFromFile:fp category:cat]))
		{
			if (!strncmp(line, "%%", 2)) break;
			freeString(line);
			line = NULL;
		}
		if (line == NULL)
		{
			fclose(fp);
			[threadLock unlock]; // ] unlocked
			return nil;
		}

		freeString(line);
		line = NULL;
	}

	len = listLength(keys);
	if (listLength(values) != len)
	{
		fclose(fp);
		[threadLock unlock]; // ] unlocked
		return nil;
	}

	while (NULL != (line = [self getLineFromFile:fp category:cat]))
	{
		if (line[0] == '#') 
		{
			freeString(line);
			line = NULL;
			continue;
		}

		item = [parser parse_A:line category:cat];

		freeString(line);
		line = NULL;

		if (item == nil) continue;

		match = YES;

		for (i = 0; (i < len) && match; i++)
		{
			vals = [item valuesForKey:keys[i]];
			if (vals == NULL)
			{
				match = NO;
				continue;
			}

			if (listIndex(values[i], vals) == IndexNull)
			{
				match = NO;
				continue;
			}
		}

		if (match)
		{
			fclose(fp);
			[threadLock unlock]; // ] unlocked

			return item;
		}

		[item release];
		item = nil;
	}

	fclose(fp);
	[threadLock unlock]; // ] unlocked
	return nil;
}

- (LUDictionary *)itemWithKey:(char *)aKey
	value:(char *)aVal
	category:(LUCategory)cat
	file:(char *)fname
{
	LUDictionary *item;
	char **k = NULL;
	char **v = NULL;

	k = appendString(aKey, k);
	v = appendString(aVal, v);

	item = [self itemWithKeys:k values:v category:(LUCategory)cat file:fname];

	freeList(k);
	k = NULL;

	freeList(v);
	v = NULL;

	return item;
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat file:(char *)fname
{
	FILE *fp;
	char *line;
	LUDictionary *item;
	LUArray *all;
	char fpath[MAXPATHLEN];
	struct stat st;
	long ts;

	ts = 0;

	if (etcDir != NULL)
	{
		sprintf(fpath, "%s/%s", etcDir, fname);

		if (stat(fpath, &st) < 0) return nil;
		ts = st.st_mtime;
	}

	[threadLock lock]; // locked [[[[[

	if (etcDir != NULL) fp = fopen(fpath, "r");
	else fp = stdin;

	if (fp == NULL)
	{
		[threadLock unlock]; // ] unlocked
		return nil;
	}

	/* bootptab entries start after a "%%" line */
	if (cat == LUCategoryBootp)
	{
		while (NULL != (line = [self getLineFromFile:fp category:cat]))
		{
			if (!strncmp(line, "%%", 2)) break;
			freeString(line);
			line = NULL;
		}
		if (line == NULL)
		{
			fclose(fp);
			[threadLock unlock]; // ] unlocked
			return nil;
		}

		freeString(line);
		line = NULL;
	}

	all = [[LUArray alloc] init];

	while (NULL != (line = [self getLineFromFile:fp category:cat]))
	{
		if (line[0] == '#')
		{
			freeString(line);
			line = NULL;
			continue;
		}

		item = [parser parse_A:line category:cat];

		freeString(line);
		line = NULL;
		if (item == nil) continue;
		[all addObject:item];
		[item release];
	}

	fclose(fp);
	[threadLock unlock]; // ] unlocked
	return all;
}

- (const char *)name
{
	return "Flat_File";
}

- (const char *)shortName
{
	return "FFAgent";
}

- (void)dealloc
{
	if (stats != nil) [stats release];
	freeString(etcDir);
	etcDir = NULL;
	if (parser != nil) [parser release];
	if (threadLock != nil) [threadLock free];

	[super dealloc];

	_sharedFFAgent = nil;
}

- (LUDictionary *)statistics
{
	return stats;
}

- (void)resetStatistics
{
	if (stats != nil) [stats release];
	stats = [[LUDictionary alloc] init];
	[stats setBanner:"FFAgent statistics"];
	[stats setValue:"Flat_File" forKey:"information_system"];
}

- (void)setDirectory:(char *)dir
{
	freeString(etcDir);
	etcDir = NULL;
	if (dir != NULL) etcDir = copyString(dir);
}

- (LUDictionary *)userWithName:(char *)name
{
	return [self itemWithKey:"name" value:name 
		category:LUCategoryUser file:"master.passwd"];
}

- (LUDictionary *)userWithNumber:(int *)number
{
	char str[32];

	sprintf(str, "%d", *number);
	return [self itemWithKey:"uid" value:str
		category:LUCategoryUser file:"master.passwd"];
}

- (LUArray *)allUsers
{
	return [self allItemsWithCategory:LUCategoryUser file:"master.passwd"];
}

- (LUDictionary *)groupWithName:(char *)name
{
	return [self itemWithKey:"name" value:name
		category:LUCategoryGroup file:"group"];
}

- (LUDictionary *)groupWithNumber:(int *)number
{
	char str[32];

	sprintf(str, "%d", *number);
	return [self itemWithKey:"gid" value:str
		category:LUCategoryGroup file:"group"];
}

- (LUArray *)allGroups
{
	return [self allItemsWithCategory:LUCategoryGroup file:"group"];
}

- (LUArray *)allGroupsWithUser:(char *)name
{
	LUArray *allWithUser;
	LUArray *all;
	LUDictionary *user;
	LUDictionary *group;
	char **vals;
	int i, len, nvals;
	char fpath[MAXPATHLEN];
	struct stat st;
	long ts;

	all = [self allGroups];
	if (all == nil) return nil;

	len = [all count];
	if (len == 0)
	{
		[all release];
		return nil;
	}

	allWithUser = [[LUArray alloc] init];

	/* first get the user's default group(s) */
	sprintf(fpath, "%s/master.passwd", etcDir);
	if (stat(fpath, &st) < 0) ts = 0;
	else ts = st.st_mtime;

	user = [self userWithName:name];
	if (user != nil)
	{
		vals = [user valuesForKey:"gid"];
		if (vals != NULL)
		{
			nvals = [user countForKey:"gid"];
			if (nvals < 0) nvals = 0;

			for (i = 0; i < nvals; i++)
			{
				group = [self itemWithKey:"gid" value:vals[i]
					category:LUCategoryGroup file:"group"];

				if (group == nil) continue;

				if ([allWithUser containsObject:group])
				{
					[group release];
					continue;
				}
				[allWithUser addObject:group];
				[group release];
			}
		}
		[user release];
	}

	/* get groups with this user as a member */
	sprintf(fpath, "%s/group", etcDir);
	if (stat(fpath, &st) < 0) ts = 0;
	else ts = st.st_mtime;

	for (i = 0; i < len; i ++)
	{
		group = [all objectAtIndex:i];
		vals = [group valuesForKey:"users"];
		if (vals == NULL) continue;
		if (listIndex(name, vals) == IndexNull)
			continue;

		if ([allWithUser containsObject:group])
			continue;

		[allWithUser addObject:group];
	}

	[all release];

	len = [allWithUser count];
	if (len == 0)
	{
		[allWithUser release];
		allWithUser = nil;
	}

	return allWithUser;
}

- (LUDictionary *)hostWithName:(char *)name
{
	return [self itemWithKey:"name" value:name
		category:LUCategoryHost file:"hosts"];
}

- (LUDictionary *)hostWithInternetAddress:(struct in_addr *)addr
{
	char str[32];

	sprintf(str, "%s", inet_ntoa(*addr));
	return [self itemWithKey:"ip_address" value:str
		category:LUCategoryHost file:"hosts"];
}

- (LUDictionary *)hostWithEthernetAddress:(struct ether_addr *)addr
{
	char **etherAddrs = NULL;
	LUDictionary *ether;
	LUDictionary *host;
	int i, len;

	/* Try all possible variations on leading zeros in the address */
	etherAddrs = [self variationsOfEthernetAddress:addr];
	len = listLength(etherAddrs);
	for (i = 0; i < len; i++)
	{
		ether = [self itemWithKey:"en_address" value:etherAddrs[i]
			category:LUCategoryEthernet file:"ethers"];

		if (ether != nil)
		{
			host = [self hostWithName: [ether valueForKey:"name"]];
			[ether release];
			freeList(etherAddrs);
			etherAddrs = NULL;
			return host;
		}
		[ether release];
	}
	freeList(etherAddrs);
	etherAddrs = NULL;
	return nil;
}

- (LUArray *)allHosts
{
	return [self allItemsWithCategory:LUCategoryHost file:"hosts"];
}

- (LUDictionary *)networkWithName:(char *)name
{
	return [self itemWithKey:"name" value:name
		category:LUCategoryNetwork file:"networks"];
}

- (LUDictionary *)networkWithInternetAddress:(struct in_addr *)addr
{
	char str[32];

	sprintf(str, "%s", nettoa(addr->s_addr));
	return [self itemWithKey:"address" value:str
		category:LUCategoryNetwork file:"networks"];
}

- (LUArray *)allNetworks
{
	return [self allItemsWithCategory:LUCategoryNetwork file:"networks"];
}

- (LUDictionary *)serviceWithName:(char *)name
	protocol:(char *)prot
{
	LUDictionary *item;
	char **k = NULL;
	char **v = NULL;

	k = appendString("name", k);
	v = appendString(name, v);
	if (prot != NULL)
	{
		k = appendString("protocol", k);
		v = appendString(prot, v);
	}
	
	item = [self itemWithKeys:k values:v
		category:LUCategoryService file:"services"];

	freeList(k);
	k = NULL;

	freeList(v);
	v = NULL;

	return item;
}

- (LUDictionary *)serviceWithNumber:(int *)number
	protocol:(char *)prot
{
	char str[32];
	LUDictionary *item;
	char **k = NULL;
	char **v = NULL;

	sprintf(str, "%d", *number);

	k = appendString("port", k);
	v = appendString(str, v);
	if (prot != NULL)
	{
		k = appendString("protocol", k);
		v = appendString(prot, v);
	}
	
	item = [self itemWithKeys:k values:v
		category:LUCategoryService file:"services"];

	freeList(k);
	k = NULL;

	freeList(v);
	v = NULL;

	return item;
}

- (LUArray *)allServices
{
	return [self allItemsWithCategory:LUCategoryService file:"services"];
}

- (LUDictionary *)protocolWithName:(char *)name
{
	return [self itemWithKey:"name" value:name
		category:LUCategoryProtocol file:"protocols"];
}

- (LUDictionary *)protocolWithNumber:(int *)number
{
	char str[32];

	sprintf(str, "%d", *number);
	return [self itemWithKey:"number" value:str
		category:LUCategoryProtocol file:"protocols"];
}

- (LUArray *)allProtocols 
{
	return [self allItemsWithCategory:LUCategoryProtocol file:"protocols"];
}

- (LUDictionary *)rpcWithName:(char *)name
{
	return [self itemWithKey:"name" value:name
		category:LUCategoryRpc file:"rpc"];
}

- (LUDictionary *)rpcWithNumber:(int *)number
{
	char str[32];

	sprintf(str, "%d", *number);
	return [self itemWithKey:"number" value:str
		category:LUCategoryRpc file:"rpc"];
}

- (LUArray *)allRpcs
{
	return [self allItemsWithCategory:LUCategoryRpc file:"rpc"];
}

- (LUDictionary *)mountWithName:(char *)name
{
	return [self itemWithKey:"name" value:name
		category:LUCategoryMount file:"fstab"];
}

- (LUArray *)allMounts
{
	return [self allItemsWithCategory:LUCategoryMount file:"fstab"];
}

- (LUDictionary *)printerWithName:(char *)name
{
	return [self itemWithKey:"name" value:name
		category:LUCategoryPrinter file:"printcap"];
}

- (LUArray *)allPrinters
{
	return [self allItemsWithCategory:LUCategoryPrinter file:"printcap"];
}

- (LUDictionary *)bootparamsWithName:(char *)name
{
	return [self itemWithKey:"name" value:name
		category:LUCategoryBootparam file:"bootparams"];
}

- (LUArray *)allBootparams
{
	return [self allItemsWithCategory:LUCategoryBootparam file:"bootparams"];
}

- (LUDictionary *)bootpWithInternetAddress:(struct in_addr *)addr
{
	char str[32];

	sprintf(str, "%s", nettoa(addr->s_addr));
	return [self itemWithKey:"ip_address" value:str
		category:LUCategoryBootp file:"bootptab"];
}

- (LUDictionary *)bootpWithEthernetAddress:(struct ether_addr *)addr
{
	char **etherAddrs = NULL;
	LUDictionary *bootp;
	int i, len;

	/* Try all possible variations on leading zeros in the address */
	etherAddrs = [self variationsOfEthernetAddress:addr];
	len = listLength(etherAddrs);
	for (i = 0; i < len; i++)
	{
		bootp = [self itemWithKey:"en_address" value:etherAddrs[i]
			category:LUCategoryBootp file:"bootptab"];

		if (bootp != nil)
		{
			freeList(etherAddrs);
			etherAddrs = NULL;
			return bootp;
		}
	}
	freeList(etherAddrs);
	etherAddrs = NULL;
	return nil;
}

- (LUDictionary *)aliasWithName:(char *)name
{
	LUDictionary *alias;

	alias = [self itemWithKey:"name" value:name
		category:LUCategoryAlias file:"aliases"];
	if (alias == nil) return nil;
	[alias setValue:"1" forKey:"alias_local"];
	return alias;
}

- (LUArray *)allAliases
{
	LUArray *all;
	int i, len;

	all = [self allItemsWithCategory:LUCategoryAlias file:"aliases"];
	if (all == nil) return nil;

	len = [all count];
	for (i = 0; i < len; i++)
	{
		[[all objectAtIndex:i] setValue:"1" forKey:"alias_local"];
	}

	return all;
}

- (LUDictionary *)netgroupWithName:(char *)name
{
	return [self itemWithKey:"name" value:name
		category:LUCategoryNetgroup file:"netgroup"];
}

- (LUArray *)allNetgroups
{
	return [self allItemsWithCategory:LUCategoryNetgroup file:"netgroup"];
}

- (BOOL)inNetgroup:(char *)group
	host:(char *)host
	user:(char *)user
	domain:(char *)domain
{
	return NO;
}

@end

