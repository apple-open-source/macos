/*
 * Copyright (c) 2012 Apple Inc. All rights reserved.
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

#ifndef __EAP8021X_EAPSIMAKAPERSISTENTSTATE_H__
#define __EAP8021X_EAPSIMAKAPERSISTENTSTATE_H__

#include <CoreFoundation/CFString.h>
#include "EAPSIMAKA.h"

/* 
 * Modification History
 *
 * October 19, 2012	Dieter Siegmund (dieter@apple)
 * - created
 */

typedef struct EAPSIMAKAPersistentState *	EAPSIMAKAPersistentStateRef;

uint8_t *
EAPSIMAKAPersistentStateGetMasterKey(EAPSIMAKAPersistentStateRef persist);

int
EAPSIMAKAPersistentStateGetMasterKeySize(EAPSIMAKAPersistentStateRef persist);

CFStringRef
EAPSIMAKAPersistentStateGetIMSI(EAPSIMAKAPersistentStateRef persist);

CFStringRef
EAPSIMAKAPersistentStateGetPseudonym(EAPSIMAKAPersistentStateRef persist);

void
EAPSIMAKAPersistentStateSetPseudonym(EAPSIMAKAPersistentStateRef persist,
				     CFStringRef pseudonym);

CFStringRef
EAPSIMAKAPersistentStateGetReauthID(EAPSIMAKAPersistentStateRef persist);

void
EAPSIMAKAPersistentStateSetReauthID(EAPSIMAKAPersistentStateRef persist,
				    CFStringRef reauth_id);

uint16_t
EAPSIMAKAPersistentStateGetCounter(EAPSIMAKAPersistentStateRef persist);

void
EAPSIMAKAPersistentStateSetCounter(EAPSIMAKAPersistentStateRef persist,
				   uint16_t counter);
EAPSIMAKAPersistentStateRef
EAPSIMAKAPersistentStateCreate(EAPType type, int master_key_size,
			       CFStringRef imsi,
			       EAPSIMAKAAttributeType identity_type);
void
EAPSIMAKAPersistentStateSave(EAPSIMAKAPersistentStateRef persist,
			     Boolean master_key_valid,
			     CFStringRef ssid);
void
EAPSIMAKAPersistentStateRelease(EAPSIMAKAPersistentStateRef persist);

void
EAPSIMAKAPersistentStateForgetSSID(CFStringRef ssid);

#endif /* __EAP8021X_EAPSIMAKAPERSISTENTSTATE_H__ */
