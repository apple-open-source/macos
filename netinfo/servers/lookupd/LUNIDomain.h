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
 * LUNIDomain.h
 *
 * NetInfo domain for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "LUDictionary.h"
#import "LUArray.h"
#import "LUAgent.h"
#import <sys/types.h>
#import <sys/time.h>

@interface LUNIDomain : LUAgent
{
	void *ni;
	LUNIDomain *parent;
	BOOL iAmRoot;
	BOOL mustSetChecksumPassword;
	int isLocal;
	char *myDomainName;
	char *masterHostName;
	char *masterTag;
	char *currentServer;
	char *currentServerHostName;
	char *currentServerAddress;
	char *currentServerTag;
	unsigned long currentServerIPAddr;
	BOOL mustSetMaxChecksumAge;
	unsigned long lastChecksum;
	time_t maxChecksumAge;
	struct timeval lastChecksumFetch;

	char **userKeys;
	char **groupKeys;
	char **hostKeys;
	char **networkKeys;
	char **serviceKeys;
	char **protocolKeys;
	char **rpcKeys;
	char **mountKeys;
	char **bootparamKeys;
	char **aliasKeys;
}

+ (void *)handleForName:(char *)name;

- (LUNIDomain *)initWithDomainNamed:(char *)domainName;
- (LUNIDomain *)parent;
- (BOOL)isRootDomain;
- (BOOL)isLocalDomain;
- (char *)nameForChild:(LUNIDomain *)child;
- (const char *)name;
- (char *)masterHostName;
- (char *)masterTag;
- (char *)currentServer;
- (char *)currentServerHostName;
- (char *)currentServerAddress;
- (char *)currentServerTag;
- (void)setTimeout:(unsigned long)t;
- (void)setMaxChecksumAge:(time_t)age;
- (unsigned long)checksum;
- (unsigned long)currentChecksum;

- (LUDictionary *)readDirectory:(unsigned long)d
	selectedKeys:(char **)keyList;
- (LUDictionary *)readDirectoryName:(char *)name
	selectedKeys:(char **)keyList;

- (LUDictionary *)entityForCategory:(LUCategory)cat
	key:(char *)aKey
	value:(char *)aVal;
- (LUDictionary *)entityForCategory:(LUCategory)cat
	key:(char *)aKey
	value:(char *)aVal
	selectedKeys:(char **)keyList;
- (LUArray *)allEntitiesForCategory:(LUCategory)cat
	selectedKeys:(char **)keyList;
- (LUArray *)allEntitiesForCategory:(LUCategory)cat
	key:(char *)aKey
	value:(char *)aVal
	selectedKeys:(char **)keyList;

- (BOOL)isSecurityEnabledForOption:(char *)option;
- (BOOL)checkNetwareEnabled;
- (LUDictionary *)netgroupWithName:(char *)name;

@end

