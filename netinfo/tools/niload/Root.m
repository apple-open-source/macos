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

extern void spin_unlock(int *p);
extern void spin_lock(int *p);

static int classRefCount = 0;

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

- (Root *)init
{
	[super init];

	rootLock = 0;
	refCount = 1;

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

	return self;
}

- (void)release
{
	BOOL releaseMe = NO;

	spin_lock(&rootLock);
	refCount--;	
	if (refCount == 0) releaseMe = YES;
	spin_unlock(&rootLock);

	if (releaseMe) [self dealloc];
}

- (void)dealloc
{
	[super free];
}

@end
