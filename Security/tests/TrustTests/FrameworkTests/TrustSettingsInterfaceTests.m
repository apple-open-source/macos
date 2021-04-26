/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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
 *
 */


#import <XCTest/XCTest.h>
#include "OSX/utilities/SecCFWrappers.h"
#include <Security/SecTrustSettings.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecFramework.h>

#include "../TestMacroConversions.h"
#include "TrustFrameworkTestCase.h"

@interface TrustSettingsInterfaceTests : TrustFrameworkTestCase
@end

@implementation TrustSettingsInterfaceTests

#if TARGET_OS_OSX
- (void)testCopySystemAnchors {
    CFArrayRef certArray;
    ok_status(SecTrustCopyAnchorCertificates(&certArray), "copy anchors");
    CFReleaseSafe(certArray);
    ok_status(SecTrustSettingsCopyCertificates(kSecTrustSettingsDomainSystem, &certArray), "copy certificates");
    CFReleaseSafe(certArray);
}
#endif

#if !TARGET_OS_BRIDGE
- (void)testSetCTExceptions {
    CFErrorRef error = NULL;
    const CFStringRef TrustTestsAppID = CFSTR("com.apple.trusttests");
    CFDictionaryRef copiedExceptions = NULL;

    /* Verify no exceptions set */
    is(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, NULL), NULL, "no exceptions set");
    if (copiedExceptions) {
        /* If we're starting out with exceptions set, a lot of the following will also fail, so just skip them */
        CFReleaseNull(copiedExceptions);
        return;
    }

    /* Set exceptions with specified AppID */
    NSDictionary *exceptions1 = @{
                                  (__bridge NSString*)kSecCTExceptionsDomainsKey: @[@"test.apple.com", @".test.apple.com"],
                                  };
    ok(SecTrustStoreSetCTExceptions(TrustTestsAppID, (__bridge CFDictionaryRef)exceptions1, &error),
       "failed to set exceptions for SecurityTests: %@", error);

    /* Copy all exceptions (with only one set) */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, &error),
       "failed to copy all exceptions: %@", error);
    ok([exceptions1 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Copy this app's exceptions */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(TrustTestsAppID, &error),
       "failed to copy SecurityTests' exceptions: %@", error);
    ok([exceptions1 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Set different exceptions with implied AppID */
    NSDictionary *exceptions2 = @{
                                  (__bridge NSString*)kSecCTExceptionsDomainsKey: @[@".test.apple.com"],
                                  };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions2, &error),
       "failed to set exceptions for this app: %@", error);

    /* Ensure exceptions are replaced for SecurityTests */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(TrustTestsAppID, &error),
       "failed to copy SecurityTests' exceptions: %@", error);
    ok([exceptions2 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Set exceptions with bad inputs */
    NSDictionary *badExceptions = @{
                                    (__bridge NSString*)kSecCTExceptionsDomainsKey: @[@"test.apple.com", @".test.apple.com"],
                                    @"not a key": @"not a value",
                                    };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with unknown key");
    if (error) {
        is(CFErrorGetCode(error), errSecParam, "bad input produced unxpected error code: %ld", (long)CFErrorGetCode(error));
    } else {
        fail("expected failure to set NULL exceptions");
    }
    CFReleaseNull(error);

    /* Remove exceptions */
    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error),
       "failed to set empty array exceptions for this app: %@", error);
    is(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, NULL), NULL, "no exceptions set");
}

- (NSData *)random
{
    uint8_t random[32];
    (void)SecRandomCopyBytes(kSecRandomDefault, sizeof(random), random);
    return [[NSData alloc] initWithBytes:random length:sizeof(random)];
}

- (void)testSetTransparentConnections {
    CFErrorRef error = NULL;
    const CFStringRef TrustTestsAppID = CFSTR("com.apple.trusttests");
    CFArrayRef copiedPins = NULL;

    /* Verify no pins set */
    copiedPins = SecTrustStoreCopyTransparentConnectionPins(NULL, NULL);
    XCTAssertEqual(copiedPins, NULL);
    if (copiedPins) {
        /* If we're startign out with pins set, a lot of the following will also fail, so just skip them */
        CFReleaseNull(copiedPins);
        return;
    }

    /* Set pin with specified AppID */
    NSArray *pin1 = @[@{
        (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
        (__bridge NSString*)kSecTrustStoreSPKIHashKey : [self random]
    }];
    /* Set pin with specified AppID */
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(TrustTestsAppID, (__bridge CFArrayRef)pin1, &error),
              "failed to set pins: %@", error);

    /* Copy all pins (with only one set) */
    XCTAssertNotEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(NULL, &error),
                      "failed to copy all pins: %@", error);
    XCTAssertEqualObjects(pin1, (__bridge NSArray*)copiedPins);
    CFReleaseNull(copiedPins);

    /* Copy this app's pins */
    XCTAssertNotEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(TrustTestsAppID, &error),
                      "failed to copy this app's pins: %@", error);
    XCTAssertEqualObjects(pin1, (__bridge NSArray*)copiedPins);
    CFReleaseNull(copiedPins);

    /* Set a different pin with implied AppID and ensure pins are replaced */
    NSArray *pin2 = @[@{
        (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
        (__bridge NSString*)kSecTrustStoreSPKIHashKey : [self random]
    }];
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)pin2, &error),
              "failed to set pins: %@", error);
    XCTAssertNotEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(TrustTestsAppID, &error),
                      "failed to copy this app's pins: %@", error);
    XCTAssertEqualObjects(pin2, (__bridge NSArray*)copiedPins);
    CFReleaseNull(copiedPins);

    /* Set exceptions with bad inputs */
    NSArray *badPins = @[@{
         (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
         @"not a key" : @"not a value"
    }];
    XCTAssertFalse(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)badPins, &error));
    if (error) {
        is(CFErrorGetCode(error), errSecParam, "bad input produced unxpected error code: %ld", (long)CFErrorGetCode(error));
    } else {
        fail("expected failure to set NULL pins");
    }
    CFReleaseNull(error);

    /* Reset remaining pins */
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(TrustTestsAppID, NULL, &error),
              "failed to reset pins: %@", error);
    XCTAssertEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(NULL, &error),
                   "failed to copy all pins: %@", error);
    CFReleaseNull(copiedPins);
}
#else // TARGET_OS_BRIDGE
- (void)testSkipTests
{
    XCTAssert(true);
}
#endif

@end
