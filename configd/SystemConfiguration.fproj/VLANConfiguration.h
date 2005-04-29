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

#ifndef _VLANCONFIGURATION_H
#define _VLANCONFIGURATION_H

/*!
	@header VLANConfiguration
*/

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>


typedef const struct __VLANInterface *		VLANInterfaceRef;

typedef const struct __VLANPreferences *	VLANPreferencesRef;


__BEGIN_DECLS

// ----------

Boolean
IsVLANSupported			(CFStringRef		device);	// e.g. "en0", "en1", ...

// ----------

CFTypeID
VLANInterfaceGetTypeID		(void);

CFStringRef
VLANInterfaceGetInterface	(VLANInterfaceRef	vlan);	// returns "vlan0", "vlan1", ...

CFStringRef
VLANInterfaceGetDevice		(VLANInterfaceRef	vlan);	// returns "en0", "en1, ...

CFNumberRef
VLANInterfaceGetTag		(VLANInterfaceRef	vlan);	// returns 1 <= tag <= 4094

CFDictionaryRef
VLANInterfaceGetOptions		(VLANInterfaceRef	vlan);	// e.g. UserDefinedName, ...

// ----------

CFTypeID
VLANPreferencesGetTypeID	(void);

VLANPreferencesRef
VLANPreferencesCreate		(CFAllocatorRef		allocator);

CFArrayRef /* of VLANInterfaceRef's */
VLANPreferencesCopyInterfaces	(VLANPreferencesRef	prefs);

VLANInterfaceRef
VLANPreferencesAddInterface	(VLANPreferencesRef	prefs,
				 CFStringRef		device,		// e.g. "en0", "en1", ...
				 CFNumberRef		tag,		// e.g. 1 <= tag <= 4094
				 CFDictionaryRef	options);	// e.g. UserDefinedName, ...

Boolean
VLANPreferencesUpdateInterface	(VLANPreferencesRef	prefs,
				 VLANInterfaceRef	vlan,
				 CFStringRef		newDevice,
				 CFNumberRef		newTag,
				 CFDictionaryRef	newOptions);

Boolean
VLANPreferencesRemoveInterface	(VLANPreferencesRef	prefs,
				 VLANInterfaceRef	vlan);

Boolean
VLANPreferencesCommitChanges	(VLANPreferencesRef	prefs);

Boolean
VLANPreferencesApplyChanges	(VLANPreferencesRef	prefs);

// ----------

__END_DECLS

#endif /* _VLANCONFIGURATION_H */
