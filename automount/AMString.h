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
#ifndef __STRING_H__
#define __STRING_H__

#import "RRObject.h"
#import "Array.h"

#define IndexNull (unsigned int)-1

@interface String : RRObject
{
	char *val;
	int len;
}

+ (String *)uniqueString:(char *)s;
+ (String *)concatStrings:(String *)s :(String *)t;

- (String *)initWithChars:(char *)s;
- (String *)initWithInteger:(int)n;
- (char *)value;
- (int)intValue;
- (unsigned int)length;
- (String *)prefix:(char)c;
- (String *)postfix:(char)c;
- (String *)presuffix:(char)c;
- (String *)suffix:(char)c;
- (String *)lowerCase;
- (String *)upperCase;
- (char *)copyValue;
- (int)compare:(String *)s;
- (BOOL)equal:(String *)s;
- (char *)scan:(char)c pos:(int *)pl;

- (Array *)explode:(char)c;
- (String *)explode:(char)c pos:(unsigned int *)where;

@end

#endif __STRING_H__
