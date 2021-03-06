/*
 * Copyright (c) 2006-2019 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPKEYCHAINUTIL_H
#define _EAP8021X_EAPKEYCHAINUTIL_H

/*
 * EAPKeychainUtil.h
 * - routines to deal with keychain passwords
 */

/* 
 * Modification History
 *
 * May 10, 2006		Dieter Siegmund (dieter@apple)
 * - created
 * December 4, 2009	Dieter Siegmund (dieter@apple)
 * - updated to use CFDictionary for passing values
 */

#include <TargetConditionals.h>
#include <os/availability.h>
#include <CoreFoundation/CoreFoundation.h>
#if ! TARGET_OS_IPHONE
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#endif /* ! TARGET_OS_IPHONE */

typedef CFTypeRef EAPSecAccessRef;

/*
 * Keys relevant to the values dict.
 */
#if ! TARGET_OS_IPHONE

/*
 * Note:
 *   These two properties are only consulted when doing a Create
 */
const CFStringRef kEAPSecKeychainPropTrustedApplications; /* CFArray[SecTrustedApplication] */
const CFStringRef kEAPSecKeychainPropAllowRootAccess;	  /* CFBoolean */

/*
 * In the Set APIs, specifying a value of kCFNull will remove any of the
 * following properties.
 */
const CFStringRef kEAPSecKeychainPropLabel;	/* CFData */
const CFStringRef kEAPSecKeychainPropDescription; /* CFData */
const CFStringRef kEAPSecKeychainPropAccount; 	/* CFData */
const CFStringRef kEAPSecKeychainPropPassword; 	/* CFData */

#endif /* ! TARGET_OS_IPHONE */

OSStatus
EAPSecKeychainPasswordItemCreateWithAccess(SecKeychainRef keychain,
					   EAPSecAccessRef access,
					   CFStringRef unique_id_str,
					   CFDataRef label,
					   CFDataRef description,
					   CFDataRef user,
					   CFDataRef password);
OSStatus
EAPSecKeychainPasswordItemCreateUniqueWithAccess(SecKeychainRef keychain,
						 EAPSecAccessRef access,
						 CFDataRef label,
						 CFDataRef description,
						 CFDataRef user,
						 CFDataRef password,
						 CFStringRef * unique_id_str);
OSStatus
EAPSecKeychainPasswordItemCreate(SecKeychainRef keychain,
				 CFStringRef unique_id_str,
				 CFDictionaryRef values) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);
OSStatus
EAPSecKeychainPasswordItemCreateUnique(SecKeychainRef keychain,
				       CFDictionaryRef values,
				       CFStringRef * req_unique_id) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);
OSStatus
EAPSecKeychainPasswordItemSet(SecKeychainRef keychain,
			      CFStringRef unique_id_str,
			      CFDataRef password);
OSStatus
EAPSecKeychainPasswordItemSet2(SecKeychainRef keychain,
			       CFStringRef unique_id_str,
			       CFDictionaryRef values) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);
OSStatus
EAPSecKeychainPasswordItemCopy(SecKeychainRef keychain,
			       CFStringRef unique_id_str,
			       CFDataRef * ret_password);
OSStatus
EAPSecKeychainPasswordItemCopy2(SecKeychainRef keychain,
				CFStringRef unique_id_str,
				CFArrayRef keys,
				CFDictionaryRef * ret_values) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);
OSStatus
EAPSecKeychainPasswordItemRemove(SecKeychainRef keychain,
				 CFStringRef unique_id_str);
#endif /* _EAP8021X_EAPKEYCHAINUTIL_H */

