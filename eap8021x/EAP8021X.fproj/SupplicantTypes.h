/*
 * Copyright (c) 2002 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_SUPPLICANTTYPES_H
#define _EAP8021X_SUPPLICANTTYPES_H

#include <stdint.h>

enum {
    kSupplicantStateDisconnected = 0,
    kSupplicantStateConnecting = 1,
    kSupplicantStateAcquired = 2,
    kSupplicantStateAuthenticating = 3,
    kSupplicantStateAuthenticated = 4,
    kSupplicantStateHeld = 5,
    kSupplicantStateLogoff = 6,
    kSupplicantStateInactive = 7,
    kSupplicantStateNoAuthenticator = 8,
    kSupplicantStateFirst = kSupplicantStateDisconnected,
    kSupplicantStateLast = kSupplicantStateNoAuthenticator
};

typedef uint32_t SupplicantState;
						
static __inline__ const char *
SupplicantStateString(SupplicantState state)
{
    static const char * str[] = {
	"Disconnected",
	"Connecting",
	"Acquired",
	"Authenticating",
	"Authenticated",
	"Held",
	"Logoff",
	"Inactive",
	"No Authenticator"
    };

    if (state >= kSupplicantStateFirst
	&& state <= kSupplicantStateLast) {
	return (str[state]);
    }
    return ("<unknown>");
}

#endif /* _EAP8021X_SUPPLICANTTYPES_H */
