/*
 * Copyright(c) 2000-2003 Apple Computer, Inc. All rights reserved.
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
SCPreferencesAddValue(SCPreferencesRef session, CFStringRef key, CFPropertyListRef value)
{
	SCPreferencesPrivateRef	sessionPrivate	= (SCPreferencesPrivateRef)session;

	if (_sc_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("SCPreferencesAddValue:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  key   = %@"), key);
		SCLog(TRUE, LOG_DEBUG, CFSTR("  value = %@"), value);
	}

	sessionPrivate->accessed = TRUE;

	if (CFDictionaryContainsKey(sessionPrivate->prefs, key)) {
		_SCErrorSet(kSCStatusKeyExists);
		return FALSE;
	}

	CFDictionaryAddValue(sessionPrivate->prefs, key, value);
	sessionPrivate->changed  = TRUE;
	return TRUE;
}
