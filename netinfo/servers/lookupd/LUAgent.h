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
 * LUAgent.h
 *
 * Generic client for lookupd
 *
 * This is an abstract superclass for specific 
 * lookup clients such as NetInfo, DNS, or NIS.
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "Root.h"
#import <sys/types.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <net/if.h>
#import <net/if_arp.h>
#import <net/etherdefs.h>
#import "LUDictionary.h"
#import "LUArray.h"
#import "LUGlobal.h"

@interface LUAgent : Root
{
	BOOL didInit;
	unsigned long generation;
	char *serviceName;
	LUArray *configurationArray;
}

+ (const char *)categoryName:(LUCategory)cat;
+ (const char *)categoryPathname:(LUCategory)cat;
+ (int)categoryWithName:(char *)name;

+ (char **)variationsOfEthernetAddress:(char *)addr;
+ (char *)canonicalEthernetAddress:(char *)addr;

- (LUAgent *)initWithArg:(char *)arg;

- (void)mergeNetgroup:(LUDictionary *)b into:(LUDictionary *)a;

- (const char *)serviceName;
- (const char *)shortName;

- (LUDictionary *)statistics;
- (void)resetStatistics;

- (BOOL)isStale;

- (BOOL)isValid:(LUDictionary *)item;
- (BOOL)isArrayValid:(LUArray *)array;

- (LUDictionary *)serviceWithName:(char *)name
	protocol:(char *)prot;
- (LUDictionary *)serviceWithNumber:(int *)number
	protocol:(char *)prot;

- (BOOL)inNetgroup:(char *)group
	host:(char *)host
	user:(char *)user
	domain:(char *)domain;

- (LUArray *)allGroupsWithUser:(char *)name;

- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat;

- (LUArray *)query:(LUDictionary *)pattern;
- (LUArray *)query:(LUDictionary *)pattern category:(LUCategory)cat;

- (LUArray *)allItemsWithCategory:(LUCategory)cat;

@end
