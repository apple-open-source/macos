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
 * LUCache.h
 *
 * Cache abstraction.  Cached objects may have multiple keys.
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "Root.h"
#import <sys/types.h>

typedef struct
{
	char *key;
	id obj;
} lu_node;

	
@interface LUCache : Root
{
	lu_node *node;
	unsigned int count;
}

- (unsigned int)count;

- (void)setObject:(id)obj forKey:(char *)key;
- (void)setObject:(id)obj forKeys:(char **)keys;

- (id)objectForKey:(char *)key;
- (id)objectAtIndex:(unsigned int)where;
- (BOOL)containsObject:(id)obj;

- (void)removeObject:(id)obj;
- (void)removeOldestObject;
- (void)empty;
@end

@interface Object (CachedObject)
- (BOOL)isEqual:(id)anObject;
@end
