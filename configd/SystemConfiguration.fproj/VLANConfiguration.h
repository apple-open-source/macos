/*
 * Copyright (c) 2003-2006 Apple Computer, Inc. All rights reserved.
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

#ifndef _VLANCONFIGURATION_H
#define _VLANCONFIGURATION_H

#include <AvailabilityMacros.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030

/*!
	@header VLANConfiguration
*/

typedef const struct __VLANInterface *		VLANInterfaceRef;

typedef const struct __VLANPreferences *	VLANPreferencesRef;


__BEGIN_DECLS

// ----------

Boolean
IsVLANSupported			(CFStringRef		device)		AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// e.g. "en0", "en1", ...

// ----------

CFTypeID
VLANInterfaceGetTypeID		(void)					AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

CFStringRef
VLANInterfaceGetInterface	(VLANInterfaceRef	vlan)		AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// returns "vlan0", "vlan1", ...

CFStringRef
VLANInterfaceGetDevice		(VLANInterfaceRef	vlan)		AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// returns "en0", "en1, ...

CFNumberRef
VLANInterfaceGetTag		(VLANInterfaceRef	vlan)		AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// returns 1 <= tag <= 4094

CFDictionaryRef
VLANInterfaceGetOptions		(VLANInterfaceRef	vlan)		AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// e.g. UserDefinedName, ...

// ----------

CFTypeID
VLANPreferencesGetTypeID	(void)					AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

VLANPreferencesRef
VLANPreferencesCreate		(CFAllocatorRef		allocator)	AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

CFArrayRef /* of VLANInterfaceRef's */
VLANPreferencesCopyInterfaces	(VLANPreferencesRef	prefs)		AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

VLANInterfaceRef
VLANPreferencesAddInterface	(VLANPreferencesRef	prefs,
				 CFStringRef		device,		// e.g. "en0", "en1", ...
				 CFNumberRef		tag,		// e.g. 1 <= tag <= 4094
				 CFDictionaryRef	options)	AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;	// e.g. UserDefinedName, ...

Boolean
VLANPreferencesUpdateInterface	(VLANPreferencesRef	prefs,
				 VLANInterfaceRef	vlan,
				 CFStringRef		newDevice,
				 CFNumberRef		newTag,
				 CFDictionaryRef	newOptions)	AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

Boolean
VLANPreferencesRemoveInterface	(VLANPreferencesRef	prefs,
				 VLANInterfaceRef	vlan)		AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

Boolean
VLANPreferencesCommitChanges	(VLANPreferencesRef	prefs)		AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

Boolean
VLANPreferencesApplyChanges	(VLANPreferencesRef	prefs)		AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_5;

// ----------

__END_DECLS

#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1030 */

#endif /* _VLANCONFIGURATION_H */
