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
#import <NetInfo/dsutil.h>
#import <stdlib.h>

@implementation LUArray

- (void)print
{
	[self print:stdout];
}

- (void)print:(FILE *)f
{
	int i;

	fprintf(f, "[\n");
	for (i = 0; i < count; i++)
	{
		if ([obj[i] respondsTo:@selector(print:)]) [obj[i] print:f];
		else fprintf(f, "object 0x%x\n", (int)obj[i]);
	}
	fprintf(f, "]\n");
}

- (LUArray *)init
{
	[super init];
	obj = NULL;
	retainCount = 1;
	count = 0;

	return self;
}

- (void)dealloc
{
	[self releaseObjects];
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

- (void)releaseObjects
{
	int i;

	for (i = 0; i < count; i++)
		[obj[i] release];

	count = 0;

	if (obj != NULL) free(obj);
	obj = NULL;
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

- (id)objectAtIndex:(unsigned int)where
{
	if (where == IndexNull) return nil;
	if (where >= count) return nil;

	return obj[where];
}

@end
