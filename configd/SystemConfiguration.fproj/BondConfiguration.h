/*
 * Copyright (c) 2004-2006 Apple Computer, Inc. All rights reserved.
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

#ifndef _BONDCONFIGURATION_H
#define _BONDCONFIGURATION_H

/*!
	@header BONDCONFIGURATION
*/

#include <AvailabilityMacros.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCNetworkConfiguration.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1040

typedef const struct __BondInterface *		BondInterfaceRef;

typedef const struct __BondPreferences *	BondPreferencesRef;

typedef const struct __BondStatus *		BondStatusRef;


__BEGIN_DECLS

// ----------

Boolean
IsBondSupported				(CFStringRef		device)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// e.g. "en0", "en1", ...

// ----------

CFTypeID
BondInterfaceGetTypeID			(void)						AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

CFStringRef
BondInterfaceGetInterface		(BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// returns "bond0", "bond1", ...

CFArrayRef /* of CFStringRef's */
BondInterfaceGetDevices			(BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

CFDictionaryRef
BondInterfaceGetOptions			(BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// e.g. UserDefinedName, ...

// ----------

CFTypeID
BondPreferencesGetTypeID		(void)						AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

BondPreferencesRef
BondPreferencesCreate			(CFAllocatorRef		allocator)		AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

CFArrayRef /* of BondInterfaceRef's */
BondPreferencesCopyInterfaces		(BondPreferencesRef	prefs)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

BondInterfaceRef
BondPreferencesCreateInterface		(BondPreferencesRef	prefs)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

Boolean
BondPreferencesRemoveInterface		(BondPreferencesRef	prefs,
					 BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

Boolean
BondPreferencesAddDevice		(BondPreferencesRef	prefs,
					 BondInterfaceRef	bond,
					 CFStringRef		device)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// e.g. "en0", "en1", ...

Boolean
BondPreferencesRemoveDevice		(BondPreferencesRef	prefs,
					 BondInterfaceRef	bond,
					 CFStringRef		device)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// e.g. "en0", "en1", ...

Boolean
BondPreferencesSetOptions		(BondPreferencesRef	prefs,
					 BondInterfaceRef	bond,
					 CFDictionaryRef	newOptions)		AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

Boolean
BondPreferencesCommitChanges		(BondPreferencesRef	prefs)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

Boolean
BondPreferencesApplyChanges		(BondPreferencesRef	prefs)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

// ----------

CFTypeID
BondStatusGetTypeID			(void)						AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

BondStatusRef
BondInterfaceCopyStatus			(BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

CFArrayRef
BondStatusGetDevices			(BondStatusRef		bondStatus)		AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

CFDictionaryRef
BondStatusGetInterfaceStatus		(BondStatusRef		bondStatus)		AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

CFDictionaryRef
BondStatusGetDeviceStatus		(BondStatusRef		bondStatus,
					 CFStringRef		device)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

__END_DECLS

#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1040 */

#endif /* _BONDCONFIGURATION_H */
