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
 * LUServer.h
 *
 * Lookup server for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "LUAgent.h"
#import "CacheAgent.h"
#import "LUGlobal.h"
#import "LUDictionary.h"
#import "LUArray.h"
#import "Thread.h"

#define ServerStateIdle 0
#define ServerStateActive 1
#define ServerStateQuerying 2

@interface LUServer : LUAgent
{
	LUArray *agentList;
	LUAgent *currentAgent;
	char *currentCall;
	Thread *myThread;
	BOOL idle;
	unsigned long state;
}

- (BOOL)isIdle;
- (void)setIsIdle:(BOOL)yn;

- (unsigned long)state;
- (LUAgent *)currentAgent;
- (char *)currentCall;

- (LUAgent *)agentNamed:(char *)name;

- (BOOL)isNetwareEnabled;
- (BOOL)isSecurityEnabledForOption:(char *)option;

- (LUDictionary *)dns_proxy:(LUDictionary *)dict;

- (LUArray *)getaddrinfo:(LUDictionary *)dict;
- (LUDictionary *)getnameinfo:(LUDictionary *)dict;

@end

