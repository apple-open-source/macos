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
 * FFParser.m
 *
 * Flat File data parser for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <NetInfo/config.h>
#import "FFParser.h"
#import <NetInfo/dsutil.h>
#import <string.h>

@implementation FFParser

- (char **)tokensFromLine:(const char *)data separator:(const char *)sep
{
	return [self tokensFromLine:data separator:sep stopAtPound:NO];
}

- (char **)tokensFromLine:(const char *)data separator:(const char *)sep
	stopAtPound:(BOOL)trail
{
	char **tokens = NULL;
	const char *p;
	int i, j, len;
	char buf[4096];
	BOOL scanning;

	if (data == NULL) return NULL;
	if (sep == NULL)
	{
		tokens = appendString((char *)data, tokens);
		return tokens;
	}

	len = strlen(sep);

	p = data;

	while (p[0] != '\0')
	{
		/* skip leading white space */
		while ((p[0] == ' ') || (p[0] == '\t') || (p[0] == '\n')) p++;

		/* check for end of line */
		if ((trail == YES && p[0] == '#') || p[0] == '\0')
			break;

		/* copy data */
		i = 0;
		scanning = YES;
		for (j = 0; (j < len) && scanning; j++)
		{
			if (p[0] == sep[j] || (p[0] == '\0')) scanning = NO;
		}

		while (scanning)
		{
			buf[i++] = p[0];
			p++;
			for (j = 0; (j < len) && scanning; j++)
			{
				if (p[0] == sep[j] || (p[0] == '\0')) scanning = NO;
			}
		}
	
		/* back over trailing whitespace */
		i--;
		while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
		buf[++i] = '\0';
	
		tokens = appendString(buf, tokens);

		/* check for end of line */
		if (p[0] == '\0') break;

		/* skip separator */
		scanning = YES;
		for (j = 0; (j < len) && scanning; j++)
		{
			if (p[0] == sep[j])
			{
				p++;
				scanning = NO;
			}
		}

		if ((!scanning) && p[0] == '\0')
		{
			/* line ended at a separator - add a null member */
			tokens = appendString("", tokens);
			return tokens;
		}
	}
	return tokens;
}

- (char **)netgroupTokensFromLine:(const char *)data
{
	char **tokens = NULL;
	const char *p;
	int i, j, len;
	char buf[4096], sep[3];
	BOOL scanning;
	BOOL paren;

	if (data == NULL) return NULL;
	strcpy(sep," \t");
	len = 2;

	p = data;

	while (p[0] != '\0')
	{
		/* skip leading white space */
		while ((p[0] == ' ') || (p[0] == '\t') || (p[0] == '\n')) p++;

		/* check for end of line */
		if (p[0] == '\0') break;

		/* copy data */
		i = 0;
		scanning = YES;
		for (j = 0; (j < len) && scanning; j++)
		{
			if (p[0] == sep[j] || (p[0] == '\0')) scanning = NO;
		}

		paren = NO;
		if (p[0] == '(')
		{
			paren = YES;
			p++;
		}

		while (scanning)
		{
			if (p[0] == '\0') return NULL;
			buf[i++] = p[0];
			p++;
			if (paren)
			{
				if (p[0] == ')') scanning = NO;
			}
			else
			{
				for (j = 0; (j < len) && scanning; j++)
				{
					if ((p[0] == sep[j]) || (p[0] == '\0')) scanning = NO;
				}					
			}
		}

		if (paren)
		{
			paren = NO;
			if (p[0] == ')') p++;
		}

		/* back over trailing whitespace */
		i--;
		while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
		buf[++i] = '\0';
	
		tokens = appendString(buf, tokens);

		/* check for end of line */
		if (p[0] == '\0') break;

		/* skip separator */
		scanning = YES;
		for (j = 0; (j < len) && scanning; j++)
		{
			if (p[0] == sep[j])
			{
				p++;
				scanning = NO;
			}
		}
	}
	return tokens;
}

- (LUDictionary *)parse_A:(char *)data category:(LUCategory)cat
{
	if (data == NULL) return nil;
	if (data[0] == '#') return nil;

	switch (cat)
	{
		case LUCategoryUser: return [self parseUser_A:data];
		case LUCategoryUser_43: return [self parseUser:data];
		case LUCategoryGroup: return [self parseGroup:data];
		case LUCategoryHost: return [self parseHost:data];
		case LUCategoryNetwork: return [self parseNetwork:data];
		case LUCategoryService: return [self parseService:data];
		case LUCategoryProtocol: return [self parseProtocol:data];
		case LUCategoryRpc: return [self parseRpc:data];
		case LUCategoryMount: return [self parseMount:data];
		case LUCategoryPrinter: return [self parsePrinter:data];
		case LUCategoryBootparam: return [self parseBootparam:data];
		case LUCategoryBootp: return [self parseBootp:data];
		case LUCategoryAlias: return [self parseAlias:data];
		case LUCategoryNetDomain: return [self parseNetgroup:data];
		case LUCategoryEthernet: return [self parseEthernet:data];
		case LUCategoryNetgroup: return [self parseNetgroup:data];
		default: return nil;
	}

	return nil;
}

- (LUDictionary *)parse:(char *)data category:(LUCategory)cat
{
	if (data == NULL) return nil;
	if (data[0] == '#') return nil;

	switch (cat)
	{
		case LUCategoryUser: return [self parseUser:data];
		case LUCategoryGroup: return [self parseGroup:data];
		case LUCategoryHost: return [self parseHost:data];
		case LUCategoryNetwork: return [self parseNetwork:data];
		case LUCategoryService: return [self parseService:data];
		case LUCategoryProtocol: return [self parseProtocol:data];
		case LUCategoryRpc: return [self parseRpc:data];
		case LUCategoryMount: return [self parseMount:data];
		case LUCategoryPrinter: return [self parsePrinter:data];
		case LUCategoryBootparam: return [self parseBootparam:data];
		case LUCategoryBootp: return [self parseBootp:data];
		case LUCategoryAlias: return [self parseAlias:data];
		case LUCategoryNetDomain: return [self parseNetgroup:data];
		case LUCategoryEthernet: return [self parseEthernet:data];
		case LUCategoryNetgroup: return [self parseNetgroup:data];
		default: return nil;
	}

	return nil;
}

- (LUDictionary *)parseMagicCookieUser:(char **)tokens
{
	freeList(tokens);
	tokens = NULL;
	return nil;
}

- (LUDictionary *)parseUser_A:(char *)data
{
	LUDictionary *item;
	char **tokens;

	if (data == NULL) return nil;

	tokens = [self tokensFromLine:data separator:":"];
	if (listLength(tokens) == 0)
	{
		freeList(tokens);
		return nil;
	}

	if (tokens[0][0] == '+')
	{
		return [self parseMagicCookieUser:tokens];
	}

	if (listLength(tokens) != 10)
	{
		freeList(tokens);
		return nil;
	}

	item = [[LUDictionary alloc] init];

	[item setValue:tokens[0] forKey:"name"];
	[item setValue:tokens[1] forKey:"passwd"];
	[item setValue:tokens[2] forKey:"uid"];
	[item setValue:tokens[3] forKey:"gid"];
	[item setValue:tokens[4] forKey:"class"];
	[item setValue:tokens[5] forKey:"change"];
	[item setValue:tokens[6] forKey:"expire"];
	[item setValue:tokens[7] forKey:"realname"];
	[item setValue:tokens[8] forKey:"home"];
	[item setValue:tokens[9] forKey:"shell"];
	[item setValue:tokens[0] forKey:"_writers_passwd"];

	freeList(tokens);
	tokens = NULL;

	return item;
}

- (LUDictionary *)parseUser:(char *)data
{
	/* For compatibility with YP, support 4.3 style passwd files. */
	
	LUDictionary *item;
	char **tokens;

	if (data == NULL) return nil;

	tokens = [self tokensFromLine:data separator:":"];
	if (listLength(tokens) == 0)
	{
		freeList(tokens);
		return nil;
	}

	if (tokens[0][0] == '+')
	{
		return [self parseMagicCookieUser:tokens];
	}

	if (listLength(tokens) != 7)
	{
		freeList(tokens);
		return nil;
	}

	item = [[LUDictionary alloc] init];

	[item setValue:tokens[0] forKey:"name"];
	[item setValue:tokens[1] forKey:"passwd"];
	[item setValue:tokens[2] forKey:"uid"];
	[item setValue:tokens[3] forKey:"gid"];
	[item setValue:tokens[4] forKey:"realname"];
	[item setValue:tokens[5] forKey:"home"];
	[item setValue:tokens[6] forKey:"shell"];

	[item setValue:"" forKey:"class"];
	[item setValue:"0" forKey:"change"];
	[item setValue:"0" forKey:"expire"];

	[item setValue:tokens[0] forKey:"_writers_passwd"];

	freeList(tokens);
	tokens = NULL;

	return item;
}

- (LUDictionary *)parseMagicCookieGroup:(char **)tokens
{
	return nil;
}

- (LUDictionary *)parseGroup:(char *)data
{
	LUDictionary *item;
	char **users;
	char **tokens;

	if (data == NULL) return nil;

	tokens = [self tokensFromLine:data separator:":"];
	if (listLength(tokens) == 0)
	{
		freeList(tokens);
		return nil;
	}

	if (tokens[0][0] == '+')
	{
		return [self parseMagicCookieGroup:tokens];
	}

	if (listLength(tokens) < 3)
	{
		freeList(tokens);
		return nil;
	}

	item = [[LUDictionary alloc] init];

	[item setValue:tokens[0] forKey:"name"];
	[item setValue:tokens[1] forKey:"passwd"];
	[item setValue:tokens[2] forKey:"gid"];

	if (listLength(tokens) < 4)
	{
		[item setValue:"" forKey:"users"];
	}
	else
	{
		users = [self tokensFromLine:tokens[3] separator:","];
		[item setValues:users forKey:"users"];
		freeList(users);
		users = NULL;
	}

	freeList(tokens);
	tokens = NULL;

	return item;
}

- (LUDictionary *)parseHost:(char *)data
{
	LUDictionary *item;
	char **tokens;
	int len;

	if (data == NULL) return nil;

	tokens = [self tokensFromLine:data separator:" \t" stopAtPound:YES];
	len = listLength(tokens);
	if (len < 2)
	{
		freeList(tokens);
		return nil;
	}

	item = [[LUDictionary alloc] init];

	[item setValue:tokens[0] forKey:"ip_address"];
	[item setValues:tokens+1 forKey:"name"];

	freeList(tokens);
	tokens = NULL;

	return item;
}

- (LUDictionary *)parseNNA:(char *)data nKey:(char *)aKey
{
	LUDictionary *item;
	char **tokens;
	int len;

	if (data == NULL) return nil;

	tokens = [self tokensFromLine:data separator:" \t" stopAtPound:YES];
	len = listLength(tokens);
	if (len < 2)
	{
		freeList(tokens);
		return nil;
	}

	item = [[LUDictionary alloc] init];

	[item setValue:tokens[0] forKey:"name"];
	[item setValue:tokens[1] forKey:aKey];
	[item addValues:tokens+2 forKey:"name"];

	freeList(tokens);
	tokens = NULL;

	return item;
}

- (LUDictionary *)parseNetwork:(char *)data
{
	return [self parseNNA:data nKey:"address"];
}

- (LUDictionary *)parseService:(char *)data
{
	LUDictionary *item;
	char *port;
	char *proto;

	item = [self parseNNA:data nKey:"protport"];
	if (item == nil) return nil;

	port = prefix([item valueForKey:"protport"], '/');
	if (port == NULL)
	{
		[item release];
		return nil;
	}

	proto = postfix([item valueForKey:"protport"], '/');
	if (proto == NULL)
	{
		freeString(port);
		port = NULL;
		[item release];
		return nil;
	}

	[item setValue:port forKey:"port"];
	[item setValue:proto forKey:"protocol"];
	freeString(port);
	port = NULL;
	freeString(proto);
	proto = NULL;

	[item removeKey:"protport"];

	return item;
}

- (LUDictionary *)parseProtocol:(char *)data
{
	return [self parseNNA:data nKey:"number"];
}

- (LUDictionary *)parseRpc:(char *)data
{
	return [self parseNNA:data nKey:"number"];
}

- (LUDictionary *)parseMount:(char *)data
{
	LUDictionary *item;
	char **val;
	char **tokens;

	if (data == NULL) return nil;

	tokens = [self tokensFromLine:data separator:" \t"];
	if (listLength(tokens) < 6)
	{
		freeList(tokens);
		return nil;
	}

	item = [[LUDictionary alloc] init];

	[item setValue:tokens[0] forKey:"name"];
	[item setValue:tokens[1] forKey:"dir"];
	[item setValue:tokens[2] forKey:"vfstype"];

	val = [self tokensFromLine:tokens[3] separator:","];
	[item setValues:val forKey:"opts"];

	freeList(val);
	val = NULL;

	[item setValue:tokens[4] forKey:"dump_freq"];
	[item setValue:tokens[5] forKey:"passno"];

	freeList(tokens);
	tokens = NULL;

	return item;
}

- (LUDictionary *)parsePB:(char *)data sep:(char)c
{
	char **options;
	char **opt;
	char t[2];
	int i, len;
	LUDictionary *item;

	if (data == NULL) return nil;

	item = [[LUDictionary alloc] init];

	t[0] = c;
	t[1] = '\0';
	options = explode(data, t);

	len = listLength(options);
	if (len < 1)
	{
		freeList(options);
		return nil;
	}

	[item setValue:options[0] forKey:"name"];

	for (i = 1; i < len; i++)
	{
		opt = explode(options[i], "=");
		if (listLength(opt) == 2) [item setValue:opt[1] forKey:opt[0]];
		freeList(opt);
		opt = NULL;
	}

	freeList(options);
	options = NULL;

	return item;
}

- (LUDictionary *)parsePrinter:(char *)data
{
	return [self parsePB:data sep:':'];
}

- (LUDictionary *)parseBootparam:(char *)data
{
	return [self parsePB:data sep:'\t'];
}

- (LUDictionary *)parseBootp:(char *)data
{
	LUDictionary *item;
	char **tokens;

	if (data == NULL) return nil;

	tokens = [self tokensFromLine:data separator:" \t"];
	if (listLength(tokens) < 5)
	{
		freeList(tokens);
		return nil;
	}

	item = [[LUDictionary alloc] init];

	[item setValue:tokens[0] forKey:"name"];
	[item setValue:tokens[1] forKey:"htype"];
	[item setValue:tokens[2] forKey:"en_address"];
	[item setValue:tokens[3] forKey:"ip_address"];
	[item setValue:tokens[4] forKey:"bootfile"];

	freeList(tokens);
	tokens = NULL;

	return item;
}

- (LUDictionary *)parseAlias:(char *)data
{
	LUDictionary *item;
	char **members;
	char **tokens;

	if (data == NULL) return nil;

	tokens = [self tokensFromLine:data separator:":"];
	if (listLength(tokens) < 2)
	{
		freeList(tokens);
		return nil;
	}

	item = [[LUDictionary alloc] init];

	[item setValue:tokens[0] forKey:"name"];

	members = [self tokensFromLine:tokens[1] separator:","];
	[item setValues:members forKey:"members"];

	freeList(members);
	members = NULL;

	freeList(tokens);
	tokens = NULL;

	return item;
}

- (LUDictionary *)parseEthernet:(char *)data
{
	LUDictionary *item;
	char **tokens;

	if (data == NULL) return nil;

	tokens = [self tokensFromLine:data separator:" \t" stopAtPound:YES];
	if (listLength(tokens) < 2)
	{
		freeList(tokens);
		return nil;
	}

	item = [[LUDictionary alloc] init];

	[item setValue:tokens[0] forKey:"en_address"];
	[item setValue:tokens[1] forKey:"name"];

	freeList(tokens);
	tokens = NULL;

	return item;
}

- (LUDictionary *)parseNetgroup:(char *)data
{
	LUDictionary *item;
	char **val;
	char **tokens;
	int i, len;

	if (data == NULL) return nil;

	tokens = [self netgroupTokensFromLine:data];
	if (tokens == NULL) return nil;

	len = listLength(tokens);
	if (len < 1)
	{
		freeList(tokens);
		return nil;
	}

	item = [[LUDictionary alloc] init];

	[item setValue:tokens[0] forKey:"name"];

	for (i = 1; i < len; i++)
	{
		val = [self tokensFromLine:tokens[i] separator:","];
		if (listLength(val) == 1)
		{
			[item addValue:val[0] forKey:"netgroups"];
			freeList(val);
			val = NULL;
			continue;
		}

		if (listLength(val) != 3)
		{
			[item release];
			freeList(tokens);
			tokens = NULL;
			freeList(val);
			val = NULL;
			return nil;
		}

		if (val[0][0] != '\0') [item addValue:val[0] forKey:"hosts"];
		if (val[1][0] != '\0') [item addValue:val[1] forKey:"users"];
		if (val[2][0] != '\0') [item addValue:val[2] forKey:"domains"];

		freeList(val);
		val = NULL;
	}

	freeList(tokens);
	tokens = NULL;

	return item;
}

@end

