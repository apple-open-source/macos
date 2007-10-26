/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

#ifndef _SCNETWORKCONNECTIONPRIVATE_H
#define _SCNETWORKCONNECTIONPRIVATE_H

#include <AvailabilityMacros.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCNetworkConfigurationPrivate.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1050


typedef const struct __SCUserPreferencesRef * SCUserPreferencesRef;


__BEGIN_DECLS


#pragma mark -
#pragma mark SCNetworkConnection SPIs


CFArrayRef /* of SCNetworkServiceRef's */
SCNetworkConnectionCopyAvailableServices	(SCNetworkSetRef		set)			AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

SCNetworkConnectionRef
SCNetworkConnectionCreateWithService		(CFAllocatorRef			allocator,
						 SCNetworkServiceRef		service,
						 SCNetworkConnectionCallBack	callout,
						 SCNetworkConnectionContext	*context)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

SCNetworkServiceRef
SCNetworkConnectionGetService			(SCNetworkConnectionRef		connection)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

CFArrayRef /* of SCUserPreferencesRef's */
SCNetworkConnectionCopyAllUserPreferences	(SCNetworkConnectionRef		connection)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

SCUserPreferencesRef
SCNetworkConnectionCopyCurrentUserPreferences	(SCNetworkConnectionRef		connection)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

SCUserPreferencesRef
SCNetworkConnectionCreateUserPreferences	(SCNetworkConnectionRef		connection)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;


#pragma mark -
#pragma mark SCUserPreferences SPIs


Boolean
SCUserPreferencesRemove				(SCUserPreferencesRef		userPreferences)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCUserPreferencesSetCurrent			(SCUserPreferencesRef		userPreferences)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

CFStringRef
SCUserPreferencesCopyName			(SCUserPreferencesRef		userPreferences)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

CFTypeID
SCUserPreferencesGetTypeID			(void)							AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

CFStringRef
SCUserPreferencesGetUniqueID			(SCUserPreferencesRef		userPreferences)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCUserPreferencesIsForced			(SCUserPreferencesRef		userPreferences)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCUserPreferencesSetName			(SCUserPreferencesRef		userPreferences,
						 CFStringRef			newName)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCNetworkConnectionSelectService		(CFDictionaryRef		selectionOptions,
						 SCNetworkServiceRef		*service,
						 SCUserPreferencesRef		*userPreferences)	AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCNetworkConnectionStartWithUserPreferences	(SCNetworkConnectionRef		connection,
						 SCUserPreferencesRef		userPreferences,
						 Boolean			linger)			AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

CFDictionaryRef
SCUserPreferencesCopyInterfaceConfiguration	(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCUserPreferencesSetInterfaceConfiguration	(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 CFDictionaryRef		newOptions)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

CFDictionaryRef
SCUserPreferencesCopyExtendedInterfaceConfiguration
						(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 CFStringRef			extendedType)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCUserPreferencesSetExtendedInterfaceConfiguration
						(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 CFStringRef			extendedType,
						 CFDictionaryRef		newOptions)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;


#pragma mark -
#pragma mark SCUserPreferences + SCNetworkInterface Password SPIs


Boolean
SCUserPreferencesCheckInterfacePassword		(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 SCNetworkInterfacePasswordType	passwordType)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

CFDataRef
SCUserPreferencesCopyInterfacePassword		(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 SCNetworkInterfacePasswordType	passwordType)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCUserPreferencesRemoveInterfacePassword	(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 SCNetworkInterfacePasswordType	passwordType)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

Boolean
SCUserPreferencesSetInterfacePassword		(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 SCNetworkInterfacePasswordType	passwordType,
						 CFDataRef			password,
						 CFDictionaryRef		options)		AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

__END_DECLS

#endif	/* MAC_OS_X_VERSION_MAX_ALLOWED >= 1050 */

#endif /* _SCNETWORKCONNECTIONPRIVATE_H */
