/*
 * Copyright (c) 2012, 2013 Apple Computer, Inc. All rights reserved.
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

#ifndef __API_SUPPORT__
#define __API_SUPPORT__

#include <CoreFoundation/CoreFoundation.h>
#include <net/pfkeyv2.h>
#include "racoon_types.h"
#include <sys/socket.h>
#include <SNIPSecIKEDefinitions.h>
#include <SNIPSecIKE.h>

typedef uint32_t InternalSessionRef;
typedef uint32_t InternalItemRef;

/* IKE API Types */
typedef InternalSessionRef InternalIKESARef;
#define kInternalIKESARefInvalid 0

Boolean ASHasValidSessions (void);

InternalIKESARef ASIKECreate (CFDictionaryRef ikeData, CFDictionaryRef childData);
Boolean ASIKEDispose(InternalIKESARef ref, Boolean *blockForResponse);

#endif
