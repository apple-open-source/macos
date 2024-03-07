/*
 * Copyright (c) 2021-2023 Apple Inc. All Rights Reserved.
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

typedef enum : int {
    KCSharingChangeTrackingEnabledState_DEFAULT,
    KCSharingChangeTrackingEnabledState_OVERRIDE_TRUE,
    KCSharingChangeTrackingEnabledState_OVERRIDE_FALSE,
} KCSharingChangeTrackingEnabledState;

static _Atomic(KCSharingChangeTrackingEnabledState) gSharingChangeTrackingEnabled = KCSharingChangeTrackingEnabledState_DEFAULT;

bool KCSharingIsChangeTrackingEnabled(void)
{
    KCSharingChangeTrackingEnabledState currentState = atomic_load_explicit(&gSharingChangeTrackingEnabled, memory_order_acquire);
    if (currentState != KCSharingChangeTrackingEnabledState_DEFAULT) {
        return currentState == KCSharingChangeTrackingEnabledState_OVERRIDE_TRUE;
    }

    static bool ffSharingAutomaticSyncing = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        ffSharingAutomaticSyncing = os_feature_enabled(Security, KCSharingAutomaticSyncing);
    });

    return ffSharingAutomaticSyncing;
}

void KCSharingSetChangeTrackingEnabled(bool enabled)
{
    KCSharingChangeTrackingEnabledState newState = enabled ? KCSharingChangeTrackingEnabledState_OVERRIDE_TRUE : KCSharingChangeTrackingEnabledState_OVERRIDE_FALSE;
    atomic_store_explicit(&gSharingChangeTrackingEnabled, newState, memory_order_release);
}

void KCSharingClearChangeTrackingEnabledOverride(void)
{
    atomic_store_explicit(&gSharingChangeTrackingEnabled, KCSharingChangeTrackingEnabledState_DEFAULT, memory_order_release);
}

bool _SecTrustSettingsUseXPCEnabled(void)
{
    static bool useXPCEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        useXPCEnabled = os_feature_enabled(Security, TrustSettingsUseXPC);
        secnotice("trustsettings", "TrustSettingsUseXPC is %s (via feature flags)",
                  useXPCEnabled ? "enabled" : "disabled");
    });
    return useXPCEnabled;
}

bool _SecTrustSettingsUseTrustStoreEnabled(void)
{
    static bool useTrustStoreEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        useTrustStoreEnabled = os_feature_enabled(Security, TrustSettingsUseTrustStore);
        secnotice("trustsettings", "TrustSettingsUseTrustStore is %s (via feature flags)",
                  useTrustStoreEnabled ? "enabled" : "disabled");
    });
    return useTrustStoreEnabled;
}

bool _SecTrustStoreUsesUUIDEnabled(void)
{
    static bool useTrustStoreUUIDEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        useTrustStoreUUIDEnabled = os_feature_enabled(Security, TrustStoreUsesUUID);
        secnotice("trustsettings", "TrustStoreUsesUUID is %s (via feature flags)",
                  useTrustStoreUUIDEnabled ? "enabled" : "disabled");
    });
    return useTrustStoreUUIDEnabled;
}
