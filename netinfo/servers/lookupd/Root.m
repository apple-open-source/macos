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
 * Root.m
 *
 * Reference-counted Object class.
 * This is used as the superclass for all lookupd objects.
 *
 * Written by Marc Majka
 */

#import "Root.h"
#import <string.h>
#import <stdlib.h>
#import <NetInfo/dsutil.h>

extern void spin_unlock(int *p);
extern void spin_lock(int *p);

static int classRefCount = 0;
static id watchdog = nil;

#import "MemoryWatchdog.h"

//#define _ALLOC_DEBUG_
//#define _RETAIN_DEBUG_
//#define _INIT_DEBUG_
//#define _DEALLOC_DEBUG_


@implementation Root

+ (id)retain
{
	classRefCount++;
	return self;
}

+ (void)release;
{
	classRefCount--;
}

+ (void)setWatchdog:(id)rover
{
	watchdog = rover;
}

+ (id)alloc
{
	id obj;

	obj = [super alloc];
#ifdef _ALLOC_DEBUG_
	fprintf(stderr, "+alloc   0x%08x   %s\n", (unsigned int)obj, [[self class] name]);
#endif
	return obj;
}

- (Root *)init
{
	char str[128];

	[super init];

	rootLock = 0;
	refCount = 1;

	sprintf(str, "%s", [[self class] name]);
#ifdef _INIT_DEBUG_
	fprintf(stderr, "-init    0x%08x   %s\n", (unsigned int)self, str);
#endif
	banner = copyString(str);

	if (watchdog != nil) [watchdog addObject:self];

	return self;
}

- (unsigned int)refCount
{
	return refCount;
}

- (unsigned int)retainCount
{
	return refCount;
}

- (id)retain
{
	spin_lock(&rootLock);
	refCount++;
	spin_unlock(&rootLock);
	
#ifdef _RETAIN_DEBUG_
	fprintf(stderr, "-retain 0x%08x %u", (unsigned int)self, refCount);
	if (banner != NULL) fprintf(stderr, "   %s", banner);
	fprintf(stderr, "\n");
#endif

	return self;
}

- (void)release
{
	BOOL pleaseReleaseMe = NO;
	BOOL callOffTheDog = NO;

	spin_lock(&rootLock);
	refCount--;
	if ((watchdog != nil) && (refCount == 1)) callOffTheDog = YES;
	else if (refCount == 0) pleaseReleaseMe = YES;
	spin_unlock(&rootLock);

#ifdef _RETAIN_DEBUG_
	fprintf(stderr, "-release 0x%08x %u", (unsigned int)self, refCount);
	if (banner != NULL) fprintf(stderr, "   %s", banner);
	fprintf(stderr, "\n");
#endif

	if (callOffTheDog) [watchdog removeObject:self];
	if (pleaseReleaseMe)
	{
#ifdef _RETAIN_DEBUG_
		fprintf(stderr, "-release -> dealloc 0x%08x\n", (unsigned int)self);
#endif
		[self dealloc];
#ifdef _RETAIN_DEBUG_
		fprintf(stderr, "-release <- dealloc 0x%08x\n", (unsigned int)self);
#endif
	}
}

- (void)dealloc
{
#ifdef _DEALLOC_DEBUG_
	fprintf(stderr, "-dealloc 0x%08x", (unsigned int)self);
	if (banner != NULL) fprintf(stderr, "   %s", banner);
	fprintf(stderr, "\n");
#endif
	if (banner != NULL) free(banner);

	rootLock = 0xdeadbeef;
	refCount = 0xdeadbeef;
	banner = (char *)0xdeadbeef;
	
	[super free];
}

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
	if (banner == NULL) fprintf(f, "%s 0x%08x\n", [[self class] name], (int)self);
	else fprintf(f, "%s\n", banner);
}

+ (void)print
{
	[self print:stdout];
}

+ (void)print:(FILE *)f
{
	fprintf(f, "Class %s\n", [self name]);
}

- (unsigned int)memorySize
{
	if (banner == NULL) return 12;
	return (13 + strlen(banner));
}

@end
