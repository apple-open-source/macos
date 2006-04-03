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
 * LUArray.m
 *
 * Simple array of objects.
 *	
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "LUArray.h"
#import "LUDictionary.h"
#import <NetInfo/dsutil.h>
#import "LUPrivate.h"
#import <stdlib.h>
#import <string.h>

static char *
dsmetadatatostring(dsdata *d)
{
	char *s;

	if (d == NULL) return NULL;
	s = malloc(d->length + 2);
	memmove(s + 1, d->data, d->length);
	s[0] = '_';
	s[d->length + 1] = '\0';
	return s;
}

LUArray *
dsrecordToArray(dsrecord *rl)
{
	LUArray *a;
	dsrecord *r;
	LUDictionary *dict;
	unsigned int where;
	int i, j;
	char *k;
	struct timeval now;

	if (rl == NULL) return nil;
	r = rl;

	gettimeofday(&now, (struct timezone *)NULL);

	a = [[LUArray alloc] init];

	while (r != NULL)
	{
		dict = [[LUDictionary alloc] initWithTime:now];

		for (i = 0; i < r->count; i++)
		{
			where = [dict addKey:dsdata_to_cstring(r->attribute[i]->key)];
			for (j = 0; j < r->attribute[i]->count; j++)
			{
				[dict addValue:dsdata_to_cstring(r->attribute[i]->value[j]) atIndex:where];
			}
		}

		for (i = 0; i < r->meta_count; i++)
		{
			k = dsmetadatatostring(r->meta_attribute[i]->key);
			where = [dict addKey:k];
			free(k);
			for (j = 0; j < r->meta_attribute[i]->count; j++)
			{
				[dict addValue:dsdata_to_cstring(r->meta_attribute[i]->value[j]) atIndex:where];
			}
		}

		[a addObject:dict];
		[dict release];
		r = r->next;
	}

	return a;
}

@implementation LUArray

- (void)print:(FILE *)f
{
	int i;

	fprintf(f, "Array: \"%s\"\n", [self banner]);
	if (validationStampCount > 0)
	{
		fprintf(f, "--> %d validation stamp%s\n", validationStampCount, (validationStampCount == 1) ? "" : "s");
		fprintf(f, "[\n");
		for (i = 0; i < validationStampCount; i++) [validationStamps[i] print:f];
		fprintf(f, "]\n");
		fprintf(f, "<-- %d validation stamp%s\n\n", validationStampCount, (validationStampCount == 1) ? "" : "s");
	}

	fprintf(f, "==> %d object%s\n", count, (count == 1) ? "" : "s");
	fprintf(f, "[\n");
	for (i = 0; i < count; i++) [obj[i] print:f];
	fprintf(f, "]\n");
	fprintf(f, "<== %d object%s\n\n", count, (count == 1) ? "" : "s");
}

- (LUArray *)init
{
	char str[64];

	[super init];
	obj = NULL;

	count = 0;
	validationStamps = NULL;
	validationStampCount = 0;

	sprintf(str, "A-0x%08x", (int)self);
	[self setBanner:str];

	return self;
}

- (void)dealloc
{
	[self releaseObjects];
	[self releaseValidationStamps];
	[super dealloc];
}

- (void)releaseObjects
{
	int i;

	for (i = 0; i < count; i++)
		[obj[i] release];

	count = 0;

	if (obj != NULL) free(obj);
	obj = NULL;
}

- (void)releaseValidationStamps
{
	int i;

	for (i = 0; i < validationStampCount; i++)
		[validationStamps[i] release];
	if (validationStamps != NULL) free(validationStamps);
	validationStamps = NULL;
	validationStampCount = 0;
}

- (unsigned int)indexForObject:(id)anObject
{
	int i;

	if (anObject == nil) return IndexNull;

	for (i = 0; i < count; i++)
	{
		if (anObject == obj[i]) return i;
	}
	return IndexNull;
}

- (BOOL)containsObject:(id)anObject
{
	return ([self indexForObject:anObject] != IndexNull);
}

- (unsigned int)count
{
	return count;
}

- (void)addObject:(id)anObject
{
	if (anObject == nil) return;
	if (count == 0) obj = (id *)malloc(sizeof(id));
	else obj = (id *)realloc(obj, (count + 1) * sizeof(id));
	obj[count] = anObject;
	[anObject retain];
	count++;
}

- (void)insertObject:(id)anObject atIndex:(unsigned int)where
{
	int i;

	if (anObject == nil) return;
	if (where >= count)
	{
		[self addObject:anObject];
		return;
	}

	if (count == 0) obj = (id *)malloc(sizeof(id));
	else obj = (id *)realloc(obj, (count + 1) * sizeof(id));

	count++;
	for (i = count - 1; i > where; i--) obj[i] = obj[i - 1];
	obj[where] = anObject;
	[anObject retain];
}

- (void)removeObject:(id)anObject
{
	if (anObject == nil) return;
	[self removeObjectAtIndex:[self indexForObject:anObject]];
}

- (void)removeObjectAtIndex:(unsigned int)where
{
	int i;
	id anObject;

	if (where == IndexNull) return;
	if (where >= count) return;

	anObject = obj[where];

	count--;
	for (i = where; i < count; i++) obj[i] = obj[i + 1];
	obj = (id *)realloc(obj, count * sizeof(id));

	[anObject release];
}

- (void)replaceObjectAtIndex:(unsigned int)where withObject:(id)anObject
{
	if (where == IndexNull) return;
	if (where >= count) return;
	if (anObject == nil) return;
	[obj[where] release];
	obj[where] = anObject;
}

- (id)objectAtIndex:(unsigned int)where
{
	if (where == IndexNull) return nil;
	if (where >= count) return nil;

	return obj[where];
}

- (void)addValidationStamp:(id)anObject
{
	if (anObject == nil) return;
	if (validationStampCount == 0)
		validationStamps = (id *)malloc(sizeof(id));
	else
	validationStamps = (id *)realloc(validationStamps,
		(validationStampCount + 1) * sizeof(id));
	validationStamps[validationStampCount] = anObject;
	[anObject retain];
	validationStampCount++;
}

- (unsigned int)validationStampCount
{
	return validationStampCount;
}

- (id)validationStampAtIndex:(unsigned int)where
{
	if (where >= validationStampCount) return nil;
	return validationStamps[where];
}

- (LUArray *)filter:(id)pattern
{
	LUArray *list;
	LUDictionary *item;
	int i, len;

	if (pattern == nil) return [self retain];

	list = [[LUArray alloc] init];
	len = count;
	for (i = 0; i < len; i++)
	{
		item = [self objectAtIndex:i];
		if ([item match:pattern]) [list addObject:item];
	}

	if ([list count] == 0)
	{
		[list release];
		return nil;
	}

	return list;
}

- (unsigned int)memorySize
{
	unsigned int size;

	size = [super memorySize];
	size += 8;
	size += (4 * count);
	size += (4 * validationStampCount);
	return size;
}

@end
