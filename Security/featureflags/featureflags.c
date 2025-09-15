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
#include <Security/SecInternalReleasePriv.h>


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

static void _SecTrustShowFeatureStatus(const char* feature, bool status) {
    secnotice("trustd", "%s is %s (via feature flags)",
              feature, status ? "enabled" : "disabled");
}

bool _SecTrustQWACValidationEnabled(void)
{
    /* NOTE: This feature flags are referenced by string in unit tests.
     * If you're here cleaning up, please remove it from the tests as well. */
    static bool QWACValidationEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        QWACValidationEnabled = os_feature_enabled(Security, QWACValidation);
        _SecTrustShowFeatureStatus("QWACValidation", QWACValidationEnabled);
    });
    return QWACValidationEnabled;
}

bool _SecTrustStoreRootConstraintsEnabled(void)
{
    /* NOTE: This feature flags are referenced by string in unit tests.
     * If you're here cleaning up, please remove it from the tests as well. */
    static bool RootConstraintsEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        RootConstraintsEnabled = os_feature_enabled(Security, RootConstraints);
        _SecTrustShowFeatureStatus("RootConstraints", RootConstraintsEnabled);
    });
    return RootConstraintsEnabled;
}

bool _SecProtectLoginKeychainWithDP(void)
{
    static bool ffProtectLoginKeychainWithDP = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // To enable, run this & reboot: ffctl Security/ProtectLoginKeychainWithDP=1
        ffProtectLoginKeychainWithDP = os_feature_enabled(Security, ProtectLoginKeychainWithDP);
        secnotice("dp_login", "ff is %s", ffProtectLoginKeychainWithDP ? "enabled" : "disabled");
    });
    return ffProtectLoginKeychainWithDP;
}

/* NOTE: Do NOT remove this flag -- turning it off is an escape hatch
 * for internal users when anchor migrations are managed poorly. */
bool _SecTrustEarlyAnchorExpirationEnabled(void)
{
    if (SecIsInternalRelease()) {
        static bool EarlyAnchorExpirationEnabled = false;
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            EarlyAnchorExpirationEnabled = os_feature_enabled(Security, EarlyAnchorExpiration);
            _SecTrustShowFeatureStatus("EarlyAnchorExpiration", EarlyAnchorExpirationEnabled);
        });
        return EarlyAnchorExpirationEnabled;
    }
    return false;
}


typedef enum {
    SecDbVerboseDatabaseLogging_DEFAULT,
    SecDbVerboseDatabaseLogging_OVERRIDE_TRUE,
    SecDbVerboseDatabaseLogging_OVERRIDE_FALSE,
} SecDbVerboseDatabaseLoggingFlag;

static SecDbVerboseDatabaseLoggingFlag gSecDbVerboseDatabaseLoggingFlag = SecDbVerboseDatabaseLogging_DEFAULT;

bool _SecDebVerboseDatabaseLoggingIsEnabled(void)
{
    if (gSecDbVerboseDatabaseLoggingFlag != SecDbVerboseDatabaseLogging_DEFAULT) {
        return gSecDbVerboseDatabaseLoggingFlag == SecDbVerboseDatabaseLogging_OVERRIDE_TRUE;
    }

    static bool ffSecDbVerboseDatabaseLoggingFlag = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        ffSecDbVerboseDatabaseLoggingFlag = os_feature_enabled(Security, SecDbVerboseDatabaseLogging);
    });

    return ffSecDbVerboseDatabaseLoggingFlag;
}

void _SecDebVerboseDatabaseLoggingSetOverride(bool value)
{
    gSecDbVerboseDatabaseLoggingFlag = value ? SecDbVerboseDatabaseLogging_OVERRIDE_TRUE : SecDbVerboseDatabaseLogging_OVERRIDE_FALSE;
    secnotice("keychain", "Verbose Databse Logging overridden to %s", value ? "enabled" : "disabled");
}

void _SecDebVerboseDatabaseLoggingClearOverride(void)
{
    gSecDbVerboseDatabaseLoggingFlag = SecDbVerboseDatabaseLogging_DEFAULT;
    secnotice("keychain", "Verbose Databse Logging override removed");
}

