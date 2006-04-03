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
 * MemoryWatchdog.m
 *
 * Keeps an array of object pointers.  Used to keep track of all
 * allocated lookupd objects.  Useful for finding leaks.
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "MemoryWatchdog.h"
#import "CacheAgent.h"
#import <string.h>

@implementation MemoryWatchdog

- (BOOL)objectInCache:(id)obj
{
	if (cacheAgent == nil) return NO;
	return [cacheAgent containsObject:obj];
}

- (id)init
{
	[super init];

	listLock = syslock_new(1);

	list = [[LUArray alloc] init];
	[list setBanner:"Memory list"];

	[Root setWatchdog:self];
	
	return self;
}

- (void)dealloc
{
	[Root setWatchdog:nil];
	[list release];
	syslock_free(listLock);
	[super dealloc];
}

- (void)showMemory:(FILE *)f
{
	int i, len, size, totalsize;
	id obj;
	char cachecode;
	BOOL valid;

	syslock_lock(listLock);

	totalsize = 0;
	len = [list count];

	fprintf(f, "%d object%s in memory\n\n", len, (len == 1) ? "" : "s");
	for (i = 0; i < len; i++)
	{
		obj = [list objectAtIndex:i];
		size = [obj memorySize];
		totalsize += size;

		cachecode = ' ';
		if ([self objectInCache:obj])
		{
			valid = NO;
			if ([obj isMemberOf:[LUArray class]])
			{
				valid = [cacheAgent isArrayValid:obj];
			}
			else
			{
				valid = [cacheAgent isValid:obj];
			}
			if (valid) cachecode = '*';
			else cachecode = '.';
		}

		fprintf(f, "%5d %2d %c 0x%08x %8d %s",
			i, [obj retainCount] - 1,
			cachecode,
			(unsigned int)obj, size, [obj banner]);
		fprintf(f, "\n");

	}

	fprintf(f, "Total Size = %d\n", totalsize);
	syslock_unlock(listLock);
}

- (void)printObject:(int)where file:(FILE *)f
{
	syslock_lock(listLock);
	if (where >= [list count])
	{
		syslock_unlock(listLock);
		return;
	}

	[[list objectAtIndex:where] print:f];
	syslock_unlock(listLock);
}

- (void)addObject:(id)anObject
{
	syslock_lock(listLock);
	[list addObject:anObject];
	syslock_unlock(listLock);
}

- (void)removeObject:(id)anObject;
{
	syslock_lock(listLock);
	[list removeObject:anObject];
	syslock_unlock(listLock);
}

- (unsigned int)totalMemory
{
	int i, len, size, totalsize;
	id obj;

	syslock_lock(listLock);

	totalsize = 0;
	len = [list count];

	for (i = 0; i < len; i++)
	{
		obj = [list objectAtIndex:i];
		size = [obj memorySize];
		totalsize += size;
	}
	
	syslock_unlock(listLock);

	return totalsize;
}

@end
