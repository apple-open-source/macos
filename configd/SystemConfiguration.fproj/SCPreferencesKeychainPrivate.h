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

#ifndef	_SCPREFERENCESKEYCHAINPRIVATE_H
#define	_SCPREFERENCESKEYCHAINPRIVATE_H

/*
 * SCPreferencesKeychain.h
 * - routines to deal with keychain passwords
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPreferences.h>
#include <Security/Security.h>

#include <AvailabilityMacros.h>
#include <sys/cdefs.h>

#pragma mark -
#pragma mark Keychain helper APIs

#define	kSCKeychainOptionsAllowRoot		CFSTR("AllowRoot")		// CFBoolean, allow uid==0 applications
#define kSCKeychainOptionsAllowedExecutables	CFSTR("AllowedExecutables")	// CFArray[CFURL]

__BEGIN_DECLS

SecKeychainRef
_SCSecKeychainCopySystemKeychain		(void);

CFDataRef
_SCSecKeychainPasswordItemCopy			(SecKeychainRef		keychain,
						 CFStringRef		unique_id);

Boolean
_SCSecKeychainPasswordItemExists		(SecKeychainRef		keychain,
						 CFStringRef		unique_id);

Boolean
_SCSecKeychainPasswordItemRemove		(SecKeychainRef		keychain,
						 CFStringRef		unique_id);

Boolean
_SCSecKeychainPasswordItemSet			(SecKeychainRef		keychain,
						 CFStringRef		unique_id,
						 CFStringRef		label,
						 CFStringRef		description,
						 CFStringRef		account,
						 CFDataRef		password,
						 CFDictionaryRef	options);


#pragma mark -
#pragma mark "System" Keychain APIs (w/SCPreferences)


CFDataRef
_SCPreferencesSystemKeychainPasswordItemCopy	(SCPreferencesRef	prefs,
						 CFStringRef		unique_id);

Boolean
_SCPreferencesSystemKeychainPasswordItemExists	(SCPreferencesRef	prefs,
						 CFStringRef		unique_id);

Boolean
_SCPreferencesSystemKeychainPasswordItemRemove	(SCPreferencesRef	prefs,
						 CFStringRef		unique_id);

Boolean
_SCPreferencesSystemKeychainPasswordItemSet	(SCPreferencesRef	prefs,
						 CFStringRef		unique_id,
						 CFStringRef		label,
						 CFStringRef		description,
						 CFStringRef		account,
						 CFDataRef		password,
						 CFDictionaryRef	options);

__END_DECLS

#endif	// _SCPREFERENCESKEYCHAINPRIVATE_H

