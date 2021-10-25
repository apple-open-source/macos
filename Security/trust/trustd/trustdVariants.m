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

#import <Foundation/Foundation.h>
#import <os/variant_private.h>
#import <utilities/debugging.h>
#import "trustdVariants.h"

#if !TARGET_OS_BRIDGE
#import <MobileAsset/MAAsset.h>
#import <MobileAsset/MAAssetQuery.h>
#endif // !TARGET_OS_BRIDGE

bool TrustdVariantHasCertificatesBundle() {
#if TARGET_OS_BRIDGE
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        secnotice("trustd", "variant does not have certificates bundle");
    });
    return false;
#else
    return true;
#endif
}

bool TrustdVariantAllowsAnalytics() {
#if TARGET_OS_SIMULATOR || TARGET_OS_BRIDGE
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        secnotice("trustd", "variant does not allow analytics");
    });
    return false;
#else
    return TrustdVariantAllowsFileWrite();
#endif
}

bool TrustdVariantAllowsKeychain() {
#if TARGET_OS_BRIDGE
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        secnotice("trustd", "variant does not allow keychain");
    });
    return false;
#else
    return TrustdVariantAllowsFileWrite();
#endif
}

bool TrustdVariantAllowsFileWrite() {
    bool result = !os_variant_uses_ephemeral_storage("com.apple.security");
#if TARGET_OS_BRIDGE
    result = false;
#endif
    if (!result) {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            secnotice("trustd", "variant does not allow file writes");
        });
    }
    return result;
}

bool TrustdVariantAllowsNetwork() {
    // <rdar://32728029>
#if TARGET_OS_BRIDGE || TARGET_OS_WATCH
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        secnotice("trustd", "variant does not allow network");
    });
    return false;
#else
    return true;
#endif
}

bool TrustdVariantAllowsMobileAsset() {
    BOOL result = NO;
    if (TrustdVariantHasCertificatesBundle() && TrustdVariantAllowsFileWrite()) {
#if !TARGET_OS_BRIDGE
        /* MobileAsset.framework isn't mastered into the BaseSystem. Check that the MA classes are linked. */
        static dispatch_once_t onceToken;
        static BOOL classesAvailable = YES;
        dispatch_once(&onceToken, ^{
            if (![ASAssetQuery class] || ![ASAsset class] || ![MAAssetQuery class] || ![MAAsset class]) {
                secnotice("OTATrust", "Weak-linked MobileAsset framework missing.");
                classesAvailable = NO;
            }
        });
        if (classesAvailable) {
            result = YES;
        }
#endif
    }
    if (!result) {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            secnotice("trustd", "variant does not allow MobileAsset");
        });
    }
    return result;
}

bool TrustdVariantLowMemoryDevice() {
#if TARGET_OS_WATCH
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        secnotice("trustd", "low-memory variant");
    });
    return true;
#else
    return false;
#endif
}
