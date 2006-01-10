/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import "Root.h"
#import <NetInfo/syslock.h>
#import <NetInfo/DynaAPI.h>
#import <stdint.h>

#define DefaultName "lookup daemon v2"

/* RPC lock */
extern syslock *rpcLock;

/* statistics directory lock */
#define StatsLockName "Stats_lock"
extern syslock *statsLock;

/* shared CacheAgent */
extern id cacheAgent;

/* Configuration Manager (Config class) */
extern id configManager;

/* statistics LUDictionary */
extern id statistics;

extern BOOL shutting_down;
extern BOOL debug_enabled;
extern BOOL trace_enabled;
extern BOOL agent_debug_enabled;
extern BOOL statistics_enabled;
extern BOOL coredump_enabled;
extern BOOL aaaa_cutoff_enabled;

/* preferences for getaddrinfo */
#define GAI_UNSET 0
#define GAI_P 1
#define GAI_4 2
#define GAI_6 3
#define GAI_S46 4
#define GAI_S64 5

extern uint32_t gai_pref;
extern uint32_t gai_wait;
