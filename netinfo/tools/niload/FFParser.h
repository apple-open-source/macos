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
 * FFParser.h
 *
 * Flat File data parser for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <objc/objc.h>
#import <objc/Object.h>
#import "LUDictionary.h"
#import "LUGlobal.h"

@interface FFParser : Object
{
}

- (char **)tokensFromLine:(const char *)data separator:(const char *)sep;
- (char **)tokensFromLine:(const char *)data separator:(const char *)sep
	stopAtPound:(BOOL)trail;
- (LUDictionary *)parse:(char *)data category:(LUCategory)cat;
- (LUDictionary *)parse_A:(char *)data category:(LUCategory)cat;
- (LUDictionary *)parseUser:(char *)data;
- (LUDictionary *)parseUser_A:(char *)data;
- (LUDictionary *)parseGroup:(char *)data;
- (LUDictionary *)parseHost:(char *)data;
- (LUDictionary *)parseNetwork:(char *)data;
- (LUDictionary *)parseService:(char *)data;
- (LUDictionary *)parseProtocol:(char *)data;
- (LUDictionary *)parseRpc:(char *)data;
- (LUDictionary *)parseMount:(char *)data;
- (LUDictionary *)parsePrinter:(char *)data;
- (LUDictionary *)parseBootparam:(char *)data;
- (LUDictionary *)parseBootp:(char *)data;
- (LUDictionary *)parseAlias:(char *)data;
- (LUDictionary *)parseNetgroup:(char *)data;
- (LUDictionary *)parseEthernet:(char *)data;

@end

