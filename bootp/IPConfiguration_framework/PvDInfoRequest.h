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

#ifndef PvDInfoRequest_h
#define PvDInfoRequest_h

#import <CoreFoundation/CoreFoundation.h>

CF_ASSUME_NONNULL_BEGIN

typedef struct __PvDInfoRequest * PvDInfoRequestRef;

typedef CF_ENUM(uint32_t, PvDInfoRequestState) {
	kPvDInfoRequestStateIdle = 0,
	kPvDInfoRequestStateScheduled = 1,
	kPvDInfoRequestStateObtained = 2,
	kPvDInfoRequestStateFailed = 3
};

/*
 * PvDInfoRequestRef is a CFRuntime object.
 * Must be released with CFRelease().
 */
PvDInfoRequestRef
PvDInfoRequestCreate(CFStringRef pvdid, CFArrayRef prefixes,
		     const char * ifname, uint64_t ms_delay);

void
PvDInfoRequestSetCompletionHandler(PvDInfoRequestRef request,
				   dispatch_block_t completion,
				   dispatch_queue_t queue);

void
PvDInfoRequestCancel(PvDInfoRequestRef request);

void
PvDInfoRequestResume(PvDInfoRequestRef request);

PvDInfoRequestState
PvDInfoRequestGetCompletionStatus(PvDInfoRequestRef request);

CFDictionaryRef
PvDInfoRequestCopyAdditionalInformation(PvDInfoRequestRef request);

CF_ASSUME_NONNULL_END

#endif /* PvDInfoRequest_h */
