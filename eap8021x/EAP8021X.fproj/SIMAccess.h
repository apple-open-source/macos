
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

#define SIM_KC_SIZE		8
#define SIM_SRES_SIZE		4
#define SIM_RAND_SIZE		16

/*
 * Function: SIMProcessRAND
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
SIMProcessRAND(const uint8_t * rand_p, int count,
	       uint8_t * kc_p, uint8_t * sres_p);

#endif _EAP8021X_SIMACCESS_H

