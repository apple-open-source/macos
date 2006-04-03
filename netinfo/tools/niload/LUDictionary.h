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
 * LUDictionary.h
 *
 * Property list / dictionary abstraction.
 * 
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */
 
#import <objc/objc.h>
#import <objc/Object.h>
#import <stdio.h>
#import <netinfo/ni.h>

typedef struct {
	char *key;
	unsigned int len;
	char **val;
} lu_property;

@interface LUDictionary : Object
{
	unsigned int retainCount;
	lu_property *prop;
	unsigned int count;
}

- (id)retain;
- (void)release;

- (unsigned int)count;

- (void)setValue:(char *)val forKey:(char *)key;
- (void)setValues:(char **)vals forKey:(char *)key;
- (void)addValue:(char *)val forKey:(char *)key;
- (void)addValues:(char **)vals forKey:(char *)key;
- (char *)valueForKey:(char *)key;
- (char **)valuesForKey:(char *)key;

- (void)removeKey:(char *)key;

- (void)print;
- (void)print:(FILE *)f;

- (ni_proplist *)niProplist;

@end
