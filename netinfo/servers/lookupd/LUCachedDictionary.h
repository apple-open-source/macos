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
 * LUCachedDictionary.h
 *
 * Category that extends LUDictionary for caching.
 * Method definitions are in LUDictionary.m
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "LUDictionary.h"
#import "LUGlobal.h"
#import <time.h>

@interface LUDictionary (LUCachedDictionary)

- (void)setCategory:(LUCategory)category;
- (LUCategory)category;
- (unsigned int)cacheHits;
- (void)setCacheHits:(unsigned int)n;
- (unsigned int)cacheHit;
- (time_t)timeToLive;
- (void)setTimeToLive:(time_t)seconds;
- (time_t)age;
- (time_t)dob;
- (void)resetAge;

@end
