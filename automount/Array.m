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
#import "Array.h"
#import <stdlib.h>

@implementation Array

- (id)init
{
	[super init];
	obj = NULL;
	count = 0;

	return self;
}

- (void)dealloc
{
	[self releaseObjects];
	[super dealloc];
}

- (void)releaseObjects
{
	int i;

	for (i = 0; i < count; i++) [obj[i] release];
	free(obj);
	count = 0;
	obj = NULL;
}

- (unsigned int)indexForObject:(id)anObject
{
	int i;

	if (anObject == nil) return (unsigned int)-1;

	for (i = 0; i < count; i++)
	{
		if (anObject == obj[i]) return i;
	}
	return (unsigned int)-1;
}

- (BOOL)containsObject:(id)anObject
{
	return ([self indexForObject:anObject] != (unsigned int)-1);
}

- (unsigned int)count
{
	return count;
}

- (void)addObject:(id)anObject
{
	if (anObject == nil) return;
	if (count == 0)
		obj = (id *)malloc(sizeof(id));
	else
		obj = (id *)realloc(obj, (count + 1) * sizeof(id));

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

	count++;
	obj = (id *)realloc(obj, count * sizeof(id));
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

	if (where == (unsigned int)-1) return;
	if (where >= count) return;

	[obj[where] release];

	for (i = where; i < count; i++) obj[i] = obj[i + 1];
	count--;
	if (count > 0)
	{
		obj = (id *)realloc(obj, count * sizeof(id));
	} else {
		free(obj);
		obj = nil;
	};
}

- (void)replaceObjectAtIndex:(unsigned int)where withObject:(id)anObject
{
	if (where == (unsigned int)-1) return;
	if (where >= count) return;
	if (anObject == nil) return;

	[obj[where] release];

	[anObject retain];

	obj[where] = anObject;
}

- (id)objectAtIndex:(unsigned int)where
{
	if (where == (unsigned int)-1) return nil;
	if (where >= count) return nil;

	return obj[where];
}

@end