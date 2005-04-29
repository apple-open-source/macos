/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCNetworkConfiguration.h>


typedef const struct __BondInterface *		BondInterfaceRef;

typedef const struct __BondPreferences *	BondPreferencesRef;

typedef const struct __BondStatus *		BondStatusRef;


enum {
	kSCBondStatusOK			= 0,	/* enabled, active, running, ... */
	kSCBondStatusLinkInvalid	= 1,	/* The link state was not valid (i.e. down, half-duplex, wrong speed) */
	kSCBondStatusNoPartner		= 2,	/* The port on the switch that the device is connected doesn't seem to have 802.3ad Link Aggregation enabled */
	kSCBondStatusNotInActiveGroup	= 3,	/* We're talking to a partner, but the link aggregation group is different from the one that's active */
	kSCBondStatusUnknown		= 999	/* Non-specific failure */
};

extern const CFStringRef kSCBondStatusDeviceAggregationStatus	/* CFNumber */		AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern const CFStringRef kSCBondStatusDeviceCollecting		/* CFNumber (0 or 1) */	AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern const CFStringRef kSCBondStatusDeviceDistributing	/* CFNumber (0 or 1) */	AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;


__BEGIN_DECLS

// ----------

extern const CFStringRef kSCNetworkInterfaceTypeBOND;

Boolean
SCNetworkInterfaceSupportsBonding	(SCNetworkInterfaceRef	interface)		AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

SCNetworkInterfaceRef
SCNetworkInterfaceCreateWithBond	(BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

// ----------

Boolean
IsBondSupported				(CFStringRef		device)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;	// e.g. "en0", "en1", ...

// ----------

CFTypeID
BondInterfaceGetTypeID			(void)						AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

CFStringRef
BondInterfaceGetInterface		(BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;	// returns "bond0", "bond1", ...

CFArrayRef /* of CFStringRef's */
BondInterfaceGetDevices			(BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

CFDictionaryRef
BondInterfaceGetOptions			(BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;	// e.g. UserDefinedName, ...

// ----------

CFTypeID
BondPreferencesGetTypeID		(void)						AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

BondPreferencesRef
BondPreferencesCreate			(CFAllocatorRef		allocator)		AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

CFArrayRef /* of BondInterfaceRef's */
BondPreferencesCopyInterfaces		(BondPreferencesRef	prefs)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

BondInterfaceRef
BondPreferencesCreateInterface		(BondPreferencesRef	prefs)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

Boolean
BondPreferencesRemoveInterface		(BondPreferencesRef	prefs,
					 BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

Boolean
BondPreferencesAddDevice		(BondPreferencesRef	prefs,
					 BondInterfaceRef	bond,
					 CFStringRef		device)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;	// e.g. "en0", "en1", ...

Boolean
BondPreferencesRemoveDevice		(BondPreferencesRef	prefs,
					 BondInterfaceRef	bond,
					 CFStringRef		device)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;	// e.g. "en0", "en1", ...

Boolean
BondPreferencesSetOptions		(BondPreferencesRef	prefs,
					 BondInterfaceRef	bond,
					 CFDictionaryRef	newOptions)		AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

Boolean
BondPreferencesCommitChanges		(BondPreferencesRef	prefs)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

Boolean
BondPreferencesApplyChanges		(BondPreferencesRef	prefs)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

// ----------

CFTypeID
BondStatusGetTypeID			(void)						AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

BondStatusRef
BondInterfaceCopyStatus			(BondInterfaceRef	bond)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

CFArrayRef
BondStatusGetDevices			(BondStatusRef		bondStatus)		AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

CFDictionaryRef
BondStatusGetInterfaceStatus		(BondStatusRef		bondStatus)		AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

CFDictionaryRef
BondStatusGetDeviceStatus		(BondStatusRef		bondStatus,
					 CFStringRef		device)			AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

__END_DECLS

#endif /* _BONDCONFIGURATION_H */
