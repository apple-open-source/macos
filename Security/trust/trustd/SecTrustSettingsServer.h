/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

/*!
    @header SecTrustSettingsServer
    Interface to server-managed trust settings. This functionality
    previously was part of ocspd and is now managed by trustd.
*/

#ifndef _SECURITY_SECTRUSTSETTINGSSERVER_H_
#define _SECURITY_SECTRUSTSETTINGSSERVER_H_

#include "Security/SecTrustSettings.h"
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CFString.h>
#include <sys/types.h>

__BEGIN_DECLS

void SecTrustSettingsSetDataBlock(uid_t uid, CFStringRef _Nonnull domain, CFDataRef _Nullable auth, CFDataRef _Nullable settings, void (^ _Nonnull completed)(bool result, CFErrorRef _Nullable error));

bool SecTrustSettingsSetData(uid_t uid, CFStringRef _Nonnull domain, CFDataRef _Nullable authExternalForm, CFDataRef _Nullable trustSettings, CFErrorRef _Nonnull * _Nullable error);
bool SecTrustSettingsCopyData(uid_t uid, CFStringRef _Nonnull domain, CFDataRef * _Nonnull CF_RETURNS_RETAINED trustSettings, CFErrorRef _Nonnull * _Nullable error);

__END_DECLS

#endif /* !_SECURITY_SECTRUSTSETTINGSSERVER_H_ */
