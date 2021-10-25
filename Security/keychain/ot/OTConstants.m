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

static bool OctagonSOSFeatureIsEnabledOverrideSet = false;
static bool OctagonSOSFeatureIsEnabledOverride = false;

static bool SecKVSOnCloudKitIsEnabledOverrideSet = false;
static bool SecKVSOnCloudKitIsEnabledOverride = false;

static bool SecErrorNestedErrorCappingIsEnabledOverrideSet = false;
static bool SecErrorNestedErrorCappingIsEnabledOverride = false;

static bool SecKeychainStaticPersistentRefsEnabledOverrideSet = false;
static bool SecKeychainStaticPersistentRefsEnabledOverride = false;

static bool OctagonOverridePlatformSOS = false;
static bool OctagonPlatformSOSOverrideValue = false;

bool OctagonPlatformSupportsSOS(void)
{
    if(OctagonOverridePlatformSOS) {
        return OctagonPlatformSOSOverrideValue ? YES : NO;
    }
    
#if TARGET_OS_OSX
    return true;
#elif TARGET_OS_IOS
    static bool isSOSCapable = false;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // Only iPhones, iPads, and iPods support SOS.
        CFStringRef deviceClass = MGCopyAnswer(kMGQDeviceClass, NULL);

        isSOSCapable = deviceClass && (CFEqual(deviceClass, kMGDeviceClassiPhone) ||
                                       CFEqual(deviceClass, kMGDeviceClassiPad) ||
                                       CFEqual(deviceClass, kMGDeviceClassiPod));

        if(deviceClass) {
            CFRelease(deviceClass);
        } else {
            secerror("octagon: Unable to determine device class. Guessing SOS status as Not Supported");
            isSOSCapable = false;
        }

        secnotice("octagon", "SOS is %@ on this platform" , isSOSCapable ? @"supported" : @"not supported");
    });

    return isSOSCapable;
#else
    return false;
#endif
}

void OctagonSetPlatformSupportsSOS(bool value)
{
    OctagonPlatformSOSOverrideValue = value;
    OctagonOverridePlatformSOS = YES;
}


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

void OctagonSetSOSFeatureEnabled(bool value)
{
    OctagonSOSFeatureIsEnabledOverrideSet = true;
    OctagonSOSFeatureIsEnabledOverride = value;
}

//feature flag for checking kvs on cloudkit enablement
bool SecKVSOnCloudKitIsEnabled(void)
{
    if(SecKVSOnCloudKitIsEnabledOverrideSet) {
        secnotice("octagon", "KVS on CloudKit is %@ (overridden)", SecKVSOnCloudKitIsEnabledOverride ? @"enabled" : @"disabled");
        return SecKVSOnCloudKitIsEnabledOverride;
    }

    static bool kvsOnCloudKitEnabled = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        kvsOnCloudKitEnabled = os_feature_enabled(KVS, KVSOnCloudKitForAll);
        secnotice("octagon", "KVS on CloudKit is %@ (via feature flags)", kvsOnCloudKitEnabled ? @"enabled" : @"disabled");
    });

    return kvsOnCloudKitEnabled;
}

void SecKVSOnCloudKitSetOverrideIsEnabled(bool value)
{
    SecKVSOnCloudKitIsEnabledOverrideSet = true;
    SecKVSOnCloudKitIsEnabledOverride = value;
}

//feature flag for checking whether or not we should cap the number of nested errors
bool SecErrorIsNestedErrorCappingEnabled(void)
{
    if(SecErrorNestedErrorCappingIsEnabledOverrideSet) {
        secnotice("octagon", "SecError Nested Error Capping is %@ (overridden)", SecErrorNestedErrorCappingIsEnabledOverride ? @"enabled" : @"disabled");
        return SecErrorNestedErrorCappingIsEnabledOverride;
    }

    static bool errorCappingEnabled = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        errorCappingEnabled = os_feature_enabled(Security, SecErrorNestedErrorCapping);
        secnotice("octagon", "SecError Nested Error Capping is %@ (via feature flags)", errorCappingEnabled ? @"enabled" : @"disabled");
    });

    return errorCappingEnabled;
}

void SecErrorSetOverrideNestedErrorCappingIsEnabled(bool value)
{
    SecErrorNestedErrorCappingIsEnabledOverrideSet = true;
    SecErrorNestedErrorCappingIsEnabledOverride = value;
}


//feature flag for checking if static persistent refs are enabled
bool SecKeychainIsStaticPersistentRefsEnabled(void)
{
    if(SecKeychainStaticPersistentRefsEnabledOverrideSet) {
        secnotice("octagon", "Static Persistent Refs are %@ (overridden)", SecKeychainStaticPersistentRefsEnabledOverride ? @"enabled" : @"disabled");
        return SecKeychainStaticPersistentRefsEnabledOverride;
    }

    static bool staticPersistentRefsEnabled = true;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        staticPersistentRefsEnabled = os_feature_enabled(Security, SecKeychainStaticPersistentRefs);
        secnotice("octagon", "Static Persistent Refs are %@ (via feature flags)", staticPersistentRefsEnabled ? @"enabled" : @"disabled");
    });

    return staticPersistentRefsEnabled;
}

void SecKeychainSetOverrideStaticPersistentRefsIsEnabled(bool value)
{
    SecKeychainStaticPersistentRefsEnabledOverrideSet = true;
    SecKeychainStaticPersistentRefsEnabledOverride = value;
}
