/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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
#ifndef _IOKIT_PWRMGT_IOSYSTEMCONFIGURATION_H
#define _IOKIT_PWRMGT_IOSYSTEMCONFIGURATION_H

// Get the SystemConfiguration header and hide its APIs
#define kSCCompAnyRegex __hide_kSCCompAnyRegex
#define kSCDynamicStoreDomainState __hide_kSCDynamicStoreDomainState

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPreferencesPrivate.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <TargetConditionals.h>

#undef kSCCompAnyRegex
#undef kSCDynamicStoreDomainState

__private_extern__ CFStringRef kSCCompAnyRegex;
__private_extern__ CFStringRef kSCDynamicStoreDomainState;

__private_extern__ int SCError();

__private_extern__ Boolean
SCDynamicStoreAddWatchedKey	(SCDynamicStoreRef		store,
				 CFStringRef			key,
				 Boolean			isRegex);
__private_extern__ CFDictionaryRef
SCDynamicStoreCopyMultiple	(
				    SCDynamicStoreRef		store,
				    CFArrayRef			keys,
				    CFArrayRef			patterns
				);
__private_extern__ CFPropertyListRef
SCDynamicStoreCopyValue		(
				    SCDynamicStoreRef		store,
				    CFStringRef			key
				);
__private_extern__ SCDynamicStoreRef
SCDynamicStoreCreate		(
				    CFAllocatorRef		allocator,
				    CFStringRef			name,
				    SCDynamicStoreCallBack	callout,
				    SCDynamicStoreContext	*context
				);
__private_extern__ CFRunLoopSourceRef
SCDynamicStoreCreateRunLoopSource(
				    CFAllocatorRef		allocator,
				    SCDynamicStoreRef		store,
				    CFIndex			order
				);
__private_extern__ CFStringRef
SCDynamicStoreKeyCreate		(
				    CFAllocatorRef		allocator,
				    CFStringRef			fmt,
				    ...
				);
__private_extern__ CFStringRef
SCDynamicStoreKeyCreatePreferences(
				    CFAllocatorRef		allocator,
				    CFStringRef			prefsID,
				    SCPreferencesKeyType	keyType
				);
__private_extern__ Boolean
SCDynamicStoreSetNotificationKeys(
				    SCDynamicStoreRef		store,
				    CFArrayRef			keys,
				    CFArrayRef			patterns
				);
__private_extern__ Boolean
SCDynamicStoreSetValue		(
				    SCDynamicStoreRef		store,
				    CFStringRef			key,
				    CFPropertyListRef		value
				);
__private_extern__ Boolean
SCDynamicStoreNotifyValue		(
					SCDynamicStoreRef		store,
					CFStringRef			key
					);
__private_extern__ Boolean
SCPreferencesApplyChanges	(
				    SCPreferencesRef		prefs
				);
__private_extern__ Boolean
SCPreferencesCommitChanges	(
				    SCPreferencesRef		prefs
				);
__private_extern__ SCPreferencesRef
SCPreferencesCreate		(
				    CFAllocatorRef		allocator,
				    CFStringRef			name,
				    CFStringRef			prefsID
				);

#if TARGET_OS_EMBEDDED
__private_extern__ SCPreferencesRef
SCPreferencesCreateWithAuthorization	(
					CFAllocatorRef		allocator,
					CFStringRef		name,
					CFStringRef		prefsID,
					AuthorizationRef	authorization
					);
#endif /* TARGET_OS_EMBEDDED */

__private_extern__ CFPropertyListRef
SCPreferencesGetValue		(
				    SCPreferencesRef		prefs,
				    CFStringRef			key
				);
__private_extern__ Boolean
SCPreferencesLock		(
				    SCPreferencesRef		prefs,
				    Boolean			wait
				);
__private_extern__ Boolean
SCPreferencesRemoveValue	(
				    SCPreferencesRef		prefs,
				    CFStringRef			key
				);
__private_extern__ Boolean
SCPreferencesSetValue		(
				    SCPreferencesRef		prefs,
				    CFStringRef			key,
				    CFPropertyListRef		value
				);
__private_extern__ Boolean
SCPreferencesUnlock		(
				    SCPreferencesRef		prefs
				);


#endif // _IOKIT_PWRMGT_IOSYSTEMCONFIGURATION_H

