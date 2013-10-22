
/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_SIMACCESS_H
#define _EAP8021X_SIMACCESS_H


/* 
 * Modification History
 *
 * January 15, 2009	Dieter Siegmund (dieter@apple.com)
 * - created
 */

/*
 * SIMAccess.h
 * - API's to access the SIM
 */

#include <stdint.h>
#include <stdbool.h>
#include <CoreFoundation/CFString.h>
#include "EAPSIMAKA.h"

CFStringRef
SIMCopyIMSI(void);

CFStringRef
SIMCopyRealm(void);

/*
 * Function: SIMAuthenticateGSM
 * Purpose:
 *   Communicate with SIM to retrieve the (SRES, Kc) pairs for the given
 *   set of RANDs.
 * Parameters:
 *   rand_p		input buffer containing RANDs;
 *			size must be at least 'count' * SIM_RAND_SIZE
 *   count		the number of values in rand_p, kc_p, and sres_p
 *   kc_p		output buffer to return Kc values;
 *			size must be at least 'count' * SIM_KC_SIZE
 *   sres_p		output buffer to return SRES values;
 * 			size must be at least 'count' * SIM_SRES_SIZE
 * Returns:
 *   TRUE if RANDS were processed and kc_p and sres_p were filled in,
 *   FALSE on failure.
 */
bool
SIMAuthenticateGSM(const uint8_t * rand_p, int count,
		   uint8_t * kc_p, uint8_t * sres_p);

typedef struct {
    CFDataRef	ck;
    CFDataRef	ik;
    CFDataRef	res;
    CFDataRef	auts;
} AKAAuthResults, * AKAAuthResultsRef;

void
AKAAuthResultsSetCK(AKAAuthResultsRef results, CFDataRef ck);

void
AKAAuthResultsSetIK(AKAAuthResultsRef results, CFDataRef ik);

void
AKAAuthResultsSetRES(AKAAuthResultsRef results, CFDataRef res);

void
AKAAuthResultsSetAUTS(AKAAuthResultsRef results, CFDataRef auts);

void
AKAAuthResultsInit(AKAAuthResultsRef results);

void
AKAAuthResultsRelease(AKAAuthResultsRef results);

/*
 * Function: SIMAuthenticateAKA
 * Purpose:
 *   Run the AKA algorithms on the AT_RAND data.
 *
 * Returns:
 *   FALSE if the request could not be completed (SIM unavailable).
 *
 *   TRUE if results are available:
 *   - if authentication was successful, AKAAuthResultsRef contains non-NULL
 *     res, ck, and ik values.
 *   - if there's a sync failure, AKAAuthResultsRef will contain non-NULL
 *     auts value.
 *   - otherwise, there was an auth reject.
 */
bool
SIMAuthenticateAKA(CFDataRef rand, CFDataRef autn, AKAAuthResultsRef results);

#endif /* _EAP8021X_SIMACCESS_H */

