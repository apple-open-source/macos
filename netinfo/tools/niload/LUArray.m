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

- (void)setBanner:(char *)str
{
	if (banner != NULL) freeString(banner);
	banner = NULL;
	if (str != NULL) banner = copyString(str);
}

- (char *)banner
{
	return banner;
}

- (void)print
{
	[self print:stdout];
}

- (void)print:(FILE *)f
{
	int i;
	fprintf(f, "Array: \"%s\" (%d objects)\n", banner, count);
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
	char str[64];

	[super init];
	obj = NULL;
	retainCount = 1;
	count = 0;
	validationStamps = NULL;
	validationStampCount = 0;
	lock = [[Lock alloc] initThreadLock];

	sprintf(str, "A-0x%x", (int)self);
	banner = copyString(str);

	return self;
}

- (unsigned int)retainCount
{
	return retainCount;
}

- (id)retain
{
	[lock lock];
	retainCount++;
	[lock unlock];

	return self;
}

- (void)release
{
	BOOL pleaseReleaseMe;

	pleaseReleaseMe = NO;

	[lock lock];
	retainCount--;	
	if (retainCount == 0) pleaseReleaseMe = YES;
	[lock unlock];

	if (pleaseReleaseMe) [self dealloc];
}

- (void)dealloc
{
	[self releaseObjects];
	[self releaseValidationStamps];
	freeString(banner);
	banner = NULL;
	[lock free];
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
	for (i = count; i > where; i--) obj[i] = obj[i - 1];
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

	if (where == IndexNull) return;
	if (where >= count) return;

	[obj[where] release];

	count--;
	for (i = where; i < count; i++) obj[i] = obj[i + 1];
	obj = (id *)realloc(obj, count * sizeof(id));
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

@end
