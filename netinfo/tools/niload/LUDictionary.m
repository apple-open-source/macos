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
 * LUDictionary.m
 *
 * Property list / dictionary abstraction.
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "LUDictionary.h"
#import <NetInfo/dsutil.h>
#import <NetInfo/nilib2.h>
#import <stdlib.h>
#import <string.h>
#import <sys/types.h>
#import <sys/time.h>

@implementation LUDictionary

- (LUDictionary *)init
{
	[super init];
	count = 0;
	prop = (lu_property *)malloc(sizeof(lu_property) * (count + 1));
	retainCount = 1;

	return self;
}

- (void)dealloc
{
	int i;

	for (i = 0; i < count; i++)
	{
		freeList(prop[i].val);
		prop[i].val = NULL;
		freeString(prop[i].key);
		prop[i].key = NULL;
		prop[i].len = 0;
	}
	if (prop != NULL) free(prop);
	prop = NULL;

	[super free];
}

- (id)retain
{
	retainCount++;

	return self;
}

- (void)release
{
	retainCount--;	
	if (retainCount == 0) [self dealloc];
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
		comp = strcmp(key, prop[mid].key);
		if (comp == 0) return mid;
		else if (comp < 0) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	if (strcmp(key, prop[top].key) == 0) return top;
	if (strcmp(key, prop[bot].key) == 0) return bot;
	return IndexNull;
}

- (unsigned int)addKey:(char *)key
{
	unsigned int top, bot, mid, range;
	int comp, i;

	if (count == 0)
	{
		count++;
		prop = (lu_property *)realloc(prop, sizeof(lu_property) * (count + 1));
		prop[0].key = copyString(key);
		prop[0].val = (char **)malloc(sizeof(char *));
		prop[0].val[0] = NULL;
		prop[0].len = 0;
		return 0;
	}

	top = count - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		comp = strcmp(key, prop[mid].key);
		if (comp == 0) return mid;
		else if (comp < 0) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	if (strcmp(key, prop[top].key) == 0) return top;
	if (strcmp(key, prop[bot].key) == 0) return bot;

	if (strcmp(key, prop[bot].key) < 0) mid = bot;
	else if (strcmp(key, prop[top].key) > 0) mid = top + 1;
	else mid = top;

	count++;
	prop = (lu_property *)realloc(prop, sizeof(lu_property) * (count + 1));
	for (i = count; i > mid; i--) prop[i] = prop[i - 1];
	prop[mid].key = copyString(key);
	prop[mid].val = (char **)malloc(sizeof(char *));
	prop[mid].val[0] = NULL;
	prop[mid].len = 0;

	return mid;
}

- (unsigned int)count
{
	return count;
}

- (void)removeValuesAtIndex:(unsigned int)where
{
	if (where == IndexNull) return;
	if (where >= count) return;

	if (prop[where].len > 0)
	{
		freeList(prop[where].val);
		prop[count].val = (char **)malloc(sizeof(char *));
		prop[count].val[0] = NULL;
		prop[where].len = 0;
	}
}

- (void)setValue:(char *)val forKey:(char *)key
{
	unsigned int where, l;

	if (key == NULL) return;

	where = [self addKey:key];

	if (val == NULL) return;
	if (where == IndexNull) return;
	if (where >= count) return;

	if (prop[where].len == 1)
	{
		if (val == NULL)
		{
			[self removeValuesAtIndex:where];
			return;
		}
		free(prop[where].val[0]);
		prop[where].val[0] = copyString(val);
		return;
	}

	[self removeValuesAtIndex:where];

	l = prop[where].len;
	prop[where].val = (char **)realloc(prop[where].val, sizeof(char *) * (l + 1 + 1));

	prop[where].val[l] = copyString(val);
	prop[where].val[l + 1] = NULL;
	prop[where].len = l + 1;
}
	
- (void)setValues:(char **)vals forKey:(char *)key
{
	unsigned int where;
	int i, n;

	if (key == NULL) return;
	
	where = [self addKey:key];

	if (where == IndexNull) return;
	if (where >= count) return;

	n = prop[where].len;
	[self removeValuesAtIndex:where];

	if (vals == NULL) return;

	free(prop[where].val);
	for (n = 0; vals[n] != NULL; n++);
	prop[where].val = (char **)malloc((n + 1) * sizeof(char *));
	for (i = 0; i < n; i++) prop[where].val[i] = copyString(vals[i]);
	prop[where].val[n] = NULL;

	prop[where].len = n;
}

- (void)addValues:(char **)vals forKey:(char *)key
{
	unsigned int where;
	int i, l, n;

	if (key == NULL) return;
	
	where = [self addKey:key];
	if (where == IndexNull) return;
	if (where >= count) return;
	if (vals == NULL) return;

	l = prop[where].len;
	for (n = 0; vals[n] != NULL; n++);
	prop[where].val =
		(char **)realloc(prop[where].val, sizeof(char *) * (l + n + 1));

	for (i = 0; i < n; i++) prop[where].val[l + i] = copyString(vals[i]);
	prop[where].val[l + n] = NULL;
	prop[where].len = l + n;
}

- (void)removeKey:(char *)key
{
	unsigned int i, where;

	if (key == NULL) return;
	
	where = [self indexForKey:key];
	if (where == IndexNull) return;
	if (where >= count) return;

	freeList(prop[where].val);
	freeString(prop[where].key);
	prop[where].key = NULL;

	for (i = where + 1; i < count; i++) prop[i - 1] = prop[i];
	count--;
	prop = (lu_property *)realloc(prop, (sizeof(lu_property) * (count + 1)));
}

- (void)addValue:(char *)val forKey:(char *)key
{
	unsigned int where;
	int l;

	if (key == NULL) return;
	if (val == NULL) return;
	
	where = [self addKey:key];

	if (where == IndexNull) return;
	if (where >= count) return;
	if (val == NULL) return;

	l = prop[where].len;
	prop[where].val = (char **)realloc(prop[where].val, sizeof(char *) * (l + 1 + 1));

	prop[where].val[l] = copyString(val);
	prop[where].val[l + 1] = NULL;
	prop[where].len = l + 1;
}

- (char **)valuesForKey:(char *)key
{
	unsigned int where;

	if (key == NULL) return NULL;
	where = [self indexForKey:key];
	if (where == IndexNull) return NULL;
	if (where >= count) return NULL;
	if (prop[where].len == 0) return NULL;
	return prop[where].val;
}

- (char *)valueForKey:(char *)key
{
	unsigned int where;

	if (key == NULL) return NULL;
	where = [self indexForKey:key];
	if (where == IndexNull) return NULL;
	if (where >= count) return NULL;
	if (prop[where].len == 0) return NULL;
	return prop[where].val[0];
}

- (void)print
{
	[self print:stdout];
}

- (void)print:(FILE *)f
{
	unsigned int i, j, len;
	char **p;
 
	for (i = 0; i < count; i++)
	{
		fprintf(f, "%s:",  prop[i].key);
		p = prop[i].val;
		len = prop[i].len;
		for (j = 0; j < len; j++) fprintf(f, " %s", p[j]);
		fprintf(f, "\n");
	}

	fprintf(f, "\n");
}

- (ni_proplist *)niProplist
{
	int i, j;
	ni_proplist *pl;

	pl = (ni_proplist *)malloc(sizeof(ni_proplist));
	NI_INIT(pl);

	for (i = 0; i < count; i++)
	{
		nipl_createprop(pl, prop[i].key);

		for (j = 0; j < prop[i].len; j++)
		{
			nipl_appendprop(pl, prop[i].key, prop[i].val[j]);
		}
	}

	return pl;
}

@end
