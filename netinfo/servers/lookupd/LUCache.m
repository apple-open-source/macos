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
 * LUCache.m
 *
 * Cache abstraction.  Cached objects may have multiple keys.
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "LUCache.h"
#import "LUPrivate.h"
#import "LUDictionary.h"
#import "LUCachedDictionary.h"
#import <NetInfo/dsutil.h>
#import <stdlib.h>
#import <string.h>
#import <stdio.h>

@implementation LUCache

- (LUCache *)init
{
	[super init];
	count = 0;
	node = (lu_node *)malloc(sizeof(lu_node) * (count + 1));
	node[count].key = NULL;
	node[count].obj = nil;
	return self;
}

- (void)dealloc
{
	[self empty];
	free(node);
	[super dealloc];
}

- (void)empty
{
	int i;

	for (i = 0; i < count; i++)
	{
		[node[i].obj release];
		freeString(node[i].key);
	}

	free(node);

	count = 0;
	node = (lu_node *)malloc(sizeof(lu_node) * (count + 1));
	node[count].key = NULL;
	node[count].obj = nil;
}

- (void)print:(FILE *)f
{
	int i;

	for (i = 0; i < count; i++)
	{
		fprintf(f, "0x%08x \"%s\" (%s)\n", (unsigned int)node[i].obj, node[i].key, [node[i].obj banner]);
	}
}

- (void)removeObject:(id)obj
{
	int i, j;
	BOOL shrank;

	if (obj == nil) return;

	shrank = NO;
	for (i = count-1; i >= 0; i--)
	{
		if (node[i].obj == obj)
		{
			freeString(node[i].key);
			node[i].key = NULL;
			[node[i].obj release];
			for (j = i + 1; j < count; j++) node[j - 1] = node[j];
			count--;
			shrank = YES;
		}
	}

	if (shrank)
	{
		node = (lu_node *)realloc(node, (sizeof(lu_node) * (count + 1)));
		node[count].key = NULL;
		node[count].obj = nil;
	}
}	

- (unsigned int)indexForKey:(char *)key
{
	unsigned int top, bot, mid, range;
	int comp;

	if (count == 0) return IndexNull;
	top = count - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		comp = strcmp(key, node[mid].key);
		if (comp == 0) return mid;
		else if (comp < 0) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	if (strcmp(key, node[top].key) == 0) return top;
	if (strcmp(key, node[bot].key) == 0) return bot;
	return IndexNull;
}

- (unsigned int)addKey:(char *)key
{
	unsigned int top, bot, mid, range;
	int i, comp;

	if (count == 0)
	{
		count++;
		node = (lu_node *)realloc(node, sizeof(lu_node) * (count + 1));
		node[0].key = copyString(key);
		node[0].obj = nil;
		node[count].key = NULL;
		node[count].obj = nil;
		return 0;
	}

	top = count - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		comp = strcmp(key, node[mid].key);
		if (comp == 0) return mid;
		else if (comp < 0) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	if (strcmp(key, node[top].key) == 0) return top;
	if (strcmp(key, node[bot].key) == 0) return bot;

	if (strcmp(key, node[bot].key) < 0) mid = bot;
	else if (strcmp(key, node[top].key) > 0) mid = top + 1;
	else mid = top;

	count++;
	node = (lu_node *)realloc(node, sizeof(lu_node) * (count + 1));
	for (i = count; i > mid; i--) node[i] = node[i - 1];
	node[mid].key = copyString(key);
	node[mid].obj = nil;

	return mid;
}

- (unsigned int)count
{
	return count;
}

- (char *)keyAtIndex:(unsigned int)where
{
	if (where > count) return NULL;
	return node[where].key;
}

- (void)setObject:(id)obj forKey:(char *)key;
{
	unsigned int where;

	if (obj == nil) return;
	if (key == NULL) return;
	
	where = [self indexForKey:key];
	if (where != IndexNull)
	{
		/* this key is already in cache */

		if ([obj isEqual:node[where].obj])
		{
			/* duplicate.  Just update access time */
			[obj resetAge];
			return;
		}

		/* preserve existing object with this key */
		return;
	}

	where = [self addKey:key];
	[obj resetAge];
	node[where].obj = [obj retain];
	return;
}

- (void)setObject:(id)obj forKeys:(char **)keys
{
	int i;

	if (keys == NULL) return;
	if (obj == nil) return;
	
	for (i = 0; keys[i] != NULL; i++)
		[self setObject:obj forKey:keys[i]];
}

- (void)removeOldestObject
{
	id oldest;
	time_t old, age;
	int i;

	if (count == 0) return;

	oldest = node[0].obj;
	old = [oldest age];

	for (i = 1; i < count; i++)
	{
		age = [node[i].obj age];
		if (age < old)
		{
			oldest = node[i].obj;
			old = age;
		}
	}

	[self removeObject:oldest];
}

- (id)objectForKey:(char *)key;
{
	unsigned int where;

	if (key == NULL) return nil;
	where = [self indexForKey:key];
	if (where == IndexNull) return nil;
	if (where >= count) return nil;

	return node[where].obj;
}
	
- (id)objectAtIndex:(unsigned int)where
{
	if (where == IndexNull) return nil;
	if (where >= count) return nil;

	return node[where].obj;
}

- (BOOL)containsObject:(id)obj;
{
	unsigned int i;

	if (obj == nil) return NO;

	for (i = 0; i < count; i++)
		if (node[i].obj == obj) return YES;

	return NO;
}

- (unsigned int)memorySize
{
	unsigned int size, i;

	size = [super memorySize];

	size += 8;

	for (i = 0; i < count; i++)
	{
		size += 4;
		if (node[i].key != NULL) size += (strlen(node[i].key) + 1);
	}

	return size;
}

@end
