/*
 * Copyright(c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1(the
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

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * November 9, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCPreferencesInternal.h"

Boolean
SCPreferencesRemoveValue(SCPreferencesRef session, CFStringRef key)
{
	SCPreferencesPrivateRef	sessionPrivate	= (SCPreferencesPrivateRef)session;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCPreferencesRemoveValue:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  key = %@"), key);

	sessionPrivate->accessed = TRUE;

	if (!CFDictionaryContainsKey(sessionPrivate->prefs, key)) {
		_SCErrorSet(kSCStatusNoKey);
		return FALSE;
	}

	CFDictionaryRemoveValue(sessionPrivate->prefs, key);
	sessionPrivate->changed  = TRUE;
	return TRUE;
}
