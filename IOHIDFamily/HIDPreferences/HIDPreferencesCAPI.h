
/*
*
* @APPLE_LICENSE_HEADER_START@
*
* Copyright (c) 2019 Apple Computer, Inc.  All Rights Reserved.
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

#pragma once

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

#if defined(__cplusplus)
extern "C" {
#endif
    
CF_ASSUME_NONNULL_BEGIN

/*! These APIs are XPC wrapper around corresponding CFPreferences API. https://developer.apple.com/documentation/corefoundation/preferences_utilities
 * XPC HID Preferences APIs are currently available for macOS only .
 */
void HIDPreferencesSet(CFStringRef key, CFTypeRef __nullable value, CFStringRef user, CFStringRef host, CFStringRef domain);

void HIDPreferencesSetMultiple(CFDictionaryRef __nullable keysToSet , CFArrayRef __nullable keysToRemove, CFStringRef user, CFStringRef host, CFStringRef domain);

CFTypeRef __nullable HIDPreferencesCopy(CFStringRef key, CFStringRef user, CFStringRef host, CFStringRef domain);

CFDictionaryRef __nullable HIDPreferencesCopyMultiple(CFArrayRef __nullable keys, CFStringRef user, CFStringRef host, CFStringRef domain);

void HIDPreferencesSynchronize(CFStringRef user, CFStringRef host, CFStringRef domain);

CFTypeRef __nullable HIDPreferencesCopyDomain(CFStringRef key, CFStringRef domain);

void HIDPreferencesSetDomain(CFStringRef key,  CFTypeRef __nullable value, CFStringRef domain);

CF_ASSUME_NONNULL_END

#ifdef __cplusplus
}
#endif

__END_DECLS

