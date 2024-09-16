/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#include <TargetConditionals.h>
#if TARGET_OS_IOS
#include <MobileGestalt.h>
#endif

#ifndef TARGET_OS_XR
#define TARGET_OS_XR 0
#endif

#import <os/feature_private.h>

#import "keychain/ot/OTConstants.h"
#import "utilities/debugging.h"

NSErrorDomain const OctagonErrorDomain = @"com.apple.security.octagon";

NSString* OTDefaultContext = @"defaultContext";
NSString* OTDefaultsDomain = @"com.apple.security.octagon";

NSString* OTProtocolPairing = @"OctagonPairing";
NSString* OTProtocolPiggybacking = @"OctagonPiggybacking";

const char * OTTrustStatusChangeNotification = "com.apple.security.octagon.trust-status-change";

NSString* const CuttlefishErrorDomain = @"CuttlefishError";
NSString* const CuttlefishErrorRetryAfterKey = @"retryafter";

NSString* OTEscrowRecordPrefix = @"com.apple.icdp.record.";

NSString* const TrustedPeersHelperRecoveryKeySetErrorDomain = @"com.apple.security.trustedpeers.RecoveryKeySetError";
NSString* const TrustedPeersHelperErrorDomain = @"com.apple.security.trustedpeers.container";

static bool OctagonSOSFeatureIsEnabledOverrideSet = false;
static bool OctagonSOSFeatureIsEnabledOverride = false;


bool OctagonIsSOSFeatureEnabled(void)
{
    if(OctagonSOSFeatureIsEnabledOverrideSet) {
        secnotice("octagon", "SOS Feature is %@ (overridden)", OctagonSOSFeatureIsEnabledOverride ? @"enabled" : @"disabled");
        return OctagonSOSFeatureIsEnabledOverride;
    }

    static bool sosEnabled = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sosEnabled = os_feature_enabled(Security, EnableSecureObjectSync);
        secnotice("octagon", "SOS Feature is %@ (via feature flags)", sosEnabled ? @"enabled" : @"disabled");
    });

    return sosEnabled;
}

bool OctagonPlatformSupportsSOS(void)
{
    return OctagonIsSOSFeatureEnabled();
}

void OctagonSetSOSFeatureEnabled(bool value)
{
    OctagonSOSFeatureIsEnabledOverrideSet = true;
    OctagonSOSFeatureIsEnabledOverride = value;
}

typedef enum {
    OctagonSupportsPersonaMultiuser_DEFAULT,
    OctagonSupportsPersonaMultiuser_OVERRIDE_TRUE,
    OctagonSupportsPersonaMultiuser_OVERRIDE_FALSE,
} OctagonSupportsPersonaMultiuserStatus;

static OctagonSupportsPersonaMultiuserStatus gOctagonSupportsPersonaMultiuserStatus = OctagonSupportsPersonaMultiuser_DEFAULT;

bool OctagonSupportsPersonaMultiuser(void)
{
    if (gOctagonSupportsPersonaMultiuserStatus != OctagonSupportsPersonaMultiuser_DEFAULT) {
        return gOctagonSupportsPersonaMultiuserStatus == OctagonSupportsPersonaMultiuser_OVERRIDE_TRUE;
    }

    static bool ffOctagonSupportsPersonaMultiuserStatus = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // To enable, run this and reboot: $ ffctl Security/OctagonSupportsPersonaMultiuser=1
        ffOctagonSupportsPersonaMultiuserStatus = os_feature_enabled(Security, OctagonSupportsPersonaMultiuser);
        secnotice("octagon", "OctagonSupportsMultiuser is %s", ffOctagonSupportsPersonaMultiuserStatus ? "enabled" : "disabled");
    });

    return ffOctagonSupportsPersonaMultiuserStatus;
}

void OctagonSetSupportsPersonaMultiuser(bool value)
{
    gOctagonSupportsPersonaMultiuserStatus = value ? OctagonSupportsPersonaMultiuser_OVERRIDE_TRUE : OctagonSupportsPersonaMultiuser_OVERRIDE_FALSE;
    secnotice("octagon", "OctagonSupportsMultiuser overridden to %s", value ? "enabled" : "disabled");
}

void OctagonClearSupportsPersonaMultiuserOverride(void)
{
    gOctagonSupportsPersonaMultiuserStatus = OctagonSupportsPersonaMultiuser_DEFAULT;
    secnotice("octagon", "OctagonSupportsMultiuser override removed");
}

typedef enum {
    DEFER_SOS_FROM_SIGNIN_DEFAULT,
    DEFER_SOS_FROM_SIGNIN_OVERRIDE_TRUE,
    DEFER_SOS_FROM_SIGNIN_OVERRIDE_FALSE,
} DeferSOSFromSignInStatus;

static DeferSOSFromSignInStatus gDeferSOSFromSignInStatus = DEFER_SOS_FROM_SIGNIN_DEFAULT;

bool SOSCompatibilityModeEnabled(void)
{
    if (gDeferSOSFromSignInStatus != DEFER_SOS_FROM_SIGNIN_DEFAULT) {
        return gDeferSOSFromSignInStatus == DEFER_SOS_FROM_SIGNIN_OVERRIDE_TRUE;
    }

    static bool ffDeferSOSFromSignInStatus = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // To enable, run this and reboot: $ ffctl CoreCDP/DeferSOSFromSignIn=FeatureComplete
        ffDeferSOSFromSignInStatus = os_feature_enabled(CoreCDP, DeferSOSFromSignIn);
        secnotice("octagon", "DeferSOSFromSignIn is %s", ffDeferSOSFromSignInStatus ? "enabled" : "disabled");
    });

    return ffDeferSOSFromSignInStatus;
}

void SetSOSCompatibilityMode(bool value)
{
    gDeferSOSFromSignInStatus = value ? DEFER_SOS_FROM_SIGNIN_OVERRIDE_TRUE : DEFER_SOS_FROM_SIGNIN_OVERRIDE_FALSE;
    secnotice("octagon", "DeferSOSFromSignIn overridden to %s", value ? "enabled" : "disabled");
}

void ClearSOSCompatibilityModeOverride(void)
{
    gDeferSOSFromSignInStatus = DEFER_SOS_FROM_SIGNIN_DEFAULT;
    secnotice("octagon", "DeferSOSFromSignIn override removed");
}

typedef enum {
    ROLL_OCTAGON_IDENTITY_DEFAULT,
    ROLL_OCTAGON_IDENTITY_ENABLED,
    ROLL_OCTAGON_IDENTITY_DISABLED,
} RollOctagonIdentityEnabled;

static RollOctagonIdentityEnabled gRollOctagonIdentityEnabled = ROLL_OCTAGON_IDENTITY_DEFAULT;

bool IsRollOctagonIdentityEnabled(void)
{
    if (gRollOctagonIdentityEnabled != ROLL_OCTAGON_IDENTITY_DEFAULT) {
        return gRollOctagonIdentityEnabled == ROLL_OCTAGON_IDENTITY_ENABLED;
    }

    static bool ffRollOctagonIdentityEnabled = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // To enable, run this and reboot: $ ffctl Security/RollIdentityOnMIDRotation=FeatureComplete
        ffRollOctagonIdentityEnabled = os_feature_enabled(Security, RollIdentityOnMIDRotation);
        secnotice("octagon", "RollIdentityOnMIDRotation is %s", ffRollOctagonIdentityEnabled ? "enabled" : "disabled");
    });

    return ffRollOctagonIdentityEnabled;
}

void SetRollOctagonIdentityEnabled(bool value)
{
    gRollOctagonIdentityEnabled = value ? ROLL_OCTAGON_IDENTITY_ENABLED : ROLL_OCTAGON_IDENTITY_DISABLED;
    secnotice("octagon", "RollIdentityOnMIDRotation overridden to %s", value ? "enabled" : "disabled");
}

void ClearRollOctagonIdentityEnabledOverride(void)
{
    gRollOctagonIdentityEnabled = ROLL_OCTAGON_IDENTITY_DEFAULT;
    secnotice("octagon", "RollIdentityOnMIDRotation override removed");
}
