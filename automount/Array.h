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
#ifndef __ARRAY_H__
#define __ARRAY_H__

#import "RRObject.h"

@interface Array : RRObject
{
	id *obj;
	unsigned int count;
}

- (unsigned int)count;
- (id)objectAtIndex:(unsigned int)where;
- (void)insertObject:(id)anObject atIndex:(unsigned int)where;
- (void)replaceObjectAtIndex:(unsigned int)where withObject:(id)anObject;

- (void)removeObjectAtIndex:(unsigned int)where;
- (void)addObject:(id)anObject;

- (void)removeObject:(id)anObject;
- (BOOL)containsObject:(id)anObject;

- (unsigned int)indexForObject:(id)anObject;
- (void)releaseObjects;

@end

#endif __ARRAY_H__
