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
 * Root.h
 *
 * Reference-counted object class
 *
 * Copyright (c) 1995, NeXT Computer Inc. All rights reserved.
 * Written by Marc Majka
 */
 
#import <objc/objc.h>
#import <objc/Object.h>
#import <NetInfo/config.h>
#import <stdio.h>

@interface Root : Object
{
	int rootLock;
	unsigned int refCount;
	char *banner;
}

+ (id)retain;
+ (void)release;
+ (void)setWatchdog:(id)rover;

+ (void)print;
+ (void)print:(FILE *)f;

- (id)retain;
- (unsigned int)refCount;
- (unsigned int)retainCount;
- (void)release;
- (void)dealloc;

- (void)setBanner:(char *)str;
- (char *)banner;

- (void)print;
- (void)print:(FILE *)f;

- (unsigned int)memorySize;

@end
