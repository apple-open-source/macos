/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef PvDInfoContext_h
#define PvDInfoContext_h

#import <CoreFoundation/CoreFoundation.h>

CF_ASSUME_NONNULL_BEGIN

#define kPvDInfoClientRefetchSamePvDIDMinWaitSeconds 10

typedef struct {
	CFStringRef pvdid;
	CFArrayRef _Nullable ipv6_prefixes;
	const char * if_name;
	uint16_t sequence_nr;
	bool status_ok;
	CFDictionaryRef _Nullable additional_info;
	CFDateRef last_fetched_date;
	CFDateRef effective_expiration_date;
} PvDInfoContext;

void
PvDInfoContextFlush(PvDInfoContext * ret_context, bool persist_failure);

CFStringRef
PvDInfoContextGetPvDID(PvDInfoContext * ret_context);

void
PvDInfoContextSetPvDID(PvDInfoContext * ret_context, CFStringRef pvdid);

CFArrayRef
PvDInfoContextGetPrefixes(PvDInfoContext * ret_context);

void
PvDInfoContextSetPrefixes(PvDInfoContext * ret_context,
			  CFArrayRef _Nullable prefixes);

const char *
PvDInfoContextGetInterfaceName(PvDInfoContext * ret_context);

void
PvDInfoContextSetInterfaceName(PvDInfoContext * ret_context,
			       const char * if_name);

uint16_t
PvDInfoContextGetSequenceNumber(PvDInfoContext * ret_context);

void
PvDInfoContextSetSequenceNumber(PvDInfoContext * ret_context, uint16_t seqnr);

bool
PvDInfoContextIsOk(PvDInfoContext * ret_context);

void
PvDInfoContextSetIsOk(PvDInfoContext * ret_context, bool ok);

CFDictionaryRef
PvDInfoContextGetAdditionalInformation(PvDInfoContext * ret_context);

void
PvDInfoContextSetAdditionalInformation(PvDInfoContext * ret_context,
				       CFDictionaryRef _Nullable additional_info);

bool
PvDInfoContextCanRefetch(PvDInfoContext * ret_context);

bool
PvDInfoContextFetchAllowed(PvDInfoContext * ret_context);

void
PvDInfoContextSetLastFetchedDateToNow(PvDInfoContext * ret_context);

CFAbsoluteTime
PvDInfoContextGetEffectiveExpirationTime(PvDInfoContext * ret_context);

void
PvDInfoContextCalculateEffectiveExpiration(PvDInfoContext * ret_context);

CF_ASSUME_NONNULL_END

#endif /* PvDInfoContext_h */
