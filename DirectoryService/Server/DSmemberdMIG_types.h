/*
 * Copyright (c) 2004-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _DSMEMBERDMIG_TYPES_H_
#define _DSMEMBERDMIG_TYPES_H_

#include <sys/types.h>
#include <sys/kauth.h>
#include <stdint.h>

/* create a standard name that can be used by other clients */
#ifndef kDSStdMachDSMembershipPortName
	#define kDSStdMachDSMembershipPortName "com.apple.system.DirectoryService.membership_v1"
#endif

#ifndef MAX_MIG_INLINE_DATA
#define MAX_MIG_INLINE_DATA 16384
#endif

typedef struct kauth_identity_extlookup kauth_identity_extlookup;
typedef char* mstring;

typedef struct StatBlock
{
	uint64_t fTotalUpTime;
	
	uint64_t fTotalCallsHandled;
	uint64_t fAverageuSecPerCall;
	
	uint64_t fCacheHits;
	uint64_t fCacheMisses;
	
	uint64_t fTotalRecordLookups;
	uint64_t fNumFailedRecordLookups;
	uint64_t fAverageuSecPerRecordLookup;
	
	uint64_t fTotalMembershipSearches;
	uint64_t fAverageuSecPerMembershipSearch;
	
	uint64_t fTotalLegacySearches;
	uint64_t fAverageuSecPerLegacySearch;
	
	uint64_t fTotalGUIDMemberSearches;
	uint64_t fAverageuSecPerGUIDMemberSearch;
	
	uint64_t fTotalNestedMemberSearches;
	uint64_t fAverageuSecPerNestedMemberSearch;
} StatBlock;

typedef uint32_t GIDArray[16];
typedef uint32_t *GIDList;

#endif
