/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef USER_API_DEFINES
#define USER_API_DEFINES
#import <sys/types.h>
#import <sys/kauth.h>

typedef struct kauth_identity_extlookup kauth_identity_extlookup;
typedef char* string;

typedef struct StatBlock
{
	uint32_t fTotalUpTime;
	uint32_t fTotalCallsHandled;
	uint32_t fAverageuSecPerCall;
	uint32_t fCacheHits;
	uint32_t fCacheMisses;
	uint32_t fTotalRecordLookups;
	uint32_t fNumFailedRecordLookups;
	uint32_t fAverageuSecPerRecordLookup;
	uint32_t fTotalMembershipSearches;
	uint32_t fAverageuSecPerMembershipSearch;
	uint32_t fTotalLegacySearches;
	uint32_t fAverageuSecPerLegacySearch;
	uint32_t fTotalGUIDMemberSearches;
	uint32_t fAverageuSecPerGUIDMemberSearch;
	uint32_t fTotalNestedMemberSearches;
	uint32_t fAverageuSecPerNestedMemberSearch;
} StatBlock;

typedef uint32_t GIDArray[16];

#endif