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
 * LDAPAgent_Parsing.h
 * Header for LDAP entry parsers
 * Copyright (C) 1997 Luke Howard. All rights reserved.
 * Luke Howard, March 1997.
 */

#import "LDAPAgent.h"

@class LULDAPDictionary, LDAPAgent, FFParser;

@interface LDAPAgent (Parsing)

/*
 * These methods mutate the dictionaries by binding
 * LDAP attributes to dictionary keys, suitable for
 * returning to lookupd.
 */
- (LUDictionary *)parse:(LULDAPDictionary *)data category:(LUCategory)cat;
- (LUDictionary *)parseUser:(LULDAPDictionary *)user;
- (LUDictionary *)parseGroup:(LULDAPDictionary *)group;
- (LUDictionary *)parseHost:(LULDAPDictionary *)host;
- (LUDictionary *)parseNetwork:(LULDAPDictionary *)network;
- (LUDictionary *)parseService:(LULDAPDictionary *)service;
- (LUDictionary *)parseProtocol:(LULDAPDictionary *)protocol;
- (LUDictionary *)parseRpc:(LULDAPDictionary *)rpc;
- (LUDictionary *)parseMount:(LULDAPDictionary *)mount;
- (LUDictionary *)parsePrinter:(LULDAPDictionary *)printer;
- (LUDictionary *)parseBootparam:(LULDAPDictionary *)bootparam;
- (LUDictionary *)parseBootp:(LULDAPDictionary *)bootp;
- (LUDictionary *)parseAlias:(LULDAPDictionary *)alias;
- (LUDictionary *)parseNetgroup:(LULDAPDictionary *)netgroup;
@end

