/*
 * Copyright (c) 2021-2024 Apple Inc. All Rights Reserved.
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

// For functions related to feature flags used in clients/frameworks and servers/daemons

#include "featureflags.h"

#include <stdatomic.h>
#include <dispatch/dispatch.h>
#include <os/feature_private.h>
#include <os/variant_private.h>
#include <security_utilities/debugging.h>


// feature flag for supporting system keychain on non-edu-mode iOS

typedef enum {
    SystemKeychainAlways_DEFAULT,
    SystemKeychainAlways_OVERRIDE_TRUE,
    SystemKeychainAlways_OVERRIDE_FALSE,
} SystemKeychainAlwaysSupported;

static SystemKeychainAlwaysSupported gSystemKeychainAlwaysSupported = SystemKeychainAlways_DEFAULT;

bool _SecSystemKeychainAlwaysIsEnabled(void)
{
    if (gSystemKeychainAlwaysSupported != SystemKeychainAlways_DEFAULT) {
        return gSystemKeychainAlwaysSupported == SystemKeychainAlways_OVERRIDE_TRUE;
    }

    static bool ffSystemKeychainAlwaysSupported = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#if TARGET_OS_OSX
        ffSystemKeychainAlwaysSupported = true;
        secnotice("keychain", "Enabling System Keychain Always due to platform");
#else
        ffSystemKeychainAlwaysSupported = os_feature_enabled(Security, SecSystemKeychainAlwaysSupported);
        secnotice("keychain", "System Keychain Always Supported set via feature flag to %s", ffSystemKeychainAlwaysSupported ? "enabled" : "disabled");
#endif
    });

    return ffSystemKeychainAlwaysSupported;
}

void _SecSystemKeychainAlwaysOverride(bool value)
{
    gSystemKeychainAlwaysSupported = value ? SystemKeychainAlways_OVERRIDE_TRUE : SystemKeychainAlways_OVERRIDE_FALSE;
    secnotice("keychain", "System Keychain Always Supported overridden to %s", value ? "enabled" : "disabled");
}

void _SecSystemKeychainAlwaysClearOverride(void)
{
    gSystemKeychainAlwaysSupported = SystemKeychainAlways_DEFAULT;
    secnotice("keychain", "System Keychain Always Supported override removed");
}
