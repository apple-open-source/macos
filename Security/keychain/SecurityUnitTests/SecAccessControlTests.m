/*
 * Copyright (c) 2024 Apple Inc. All Rights Reserved.
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

#import <XCTest/XCTest.h>
#import <Security/SecAccessControlPriv.h>


@interface SecAccessControlTests : XCTestCase

@end

@implementation SecAccessControlTests

- (void)testExplicitBind {
    NSError *error;
    id sac = CFBridgingRelease(SecAccessControlCreate(kCFAllocatorDefault, (void *)&error));
    XCTAssertNotNil(sac, @"Failed to create an empty ACL: %@", error);
    XCTAssert(SecAccessControlAddConstraintForOperation((__bridge SecAccessControlRef)sac, CFSTR("op"), (__bridge SecAccessConstraintRef)@{}, (void *)&error), @"Failed to add dict constraint: %@", error);

    XCTAssertFalse(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"ACL is not expected to be bound");
    SecAccessControlSetBound((__bridge SecAccessControlRef)sac, true);
    XCTAssertTrue(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"ACL is expected to be bound");
    SecAccessControlSetBound((__bridge SecAccessControlRef)sac, false);
    XCTAssertFalse(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"ACL is not expected to be bound");
}

- (void)testImplicitBound {
    @autoreleasepool {
        NSError *error;
        id sac = CFBridgingRelease(SecAccessControlCreate(kCFAllocatorDefault, (void *)&error));
        XCTAssertNotNil(sac, @"Failed to create an empty ACL: %@", error);

        XCTAssertTrue(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"%@ is expected to be implicitly bound", sac);
    }

    @autoreleasepool {
        NSError *error;
        id sac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAfterFirstUnlock, 0, (void *)&error));
        XCTAssertNotNil(sac, @"Failed to create ACL: %@", error);

        XCTAssertTrue(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"%@ is expected to be implicitly bound", sac);
    }

    @autoreleasepool {
        NSError *error;
        id sac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAfterFirstUnlock, kSecAccessControlPrivateKeyUsage, (void *)&error));
        XCTAssertNotNil(sac, @"Failed to create ACL: %@", error);

        XCTAssertTrue(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"%@ is expected to be implicitly bound", sac);
    }

    @autoreleasepool {
        NSError *error;
        id sac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAfterFirstUnlock, kSecAccessControlApplicationPassword, (void *)&error));
        XCTAssertNotNil(sac, @"Failed to create ACL: %@", error);

        XCTAssertFalse(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"%@ is not expected to be implicitly bound", sac);
    }

    @autoreleasepool {
        NSError *error;
        id sac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAfterFirstUnlock, kSecAccessControlPrivateKeyUsage | kSecAccessControlApplicationPassword, (void *)&error));
        XCTAssertNotNil(sac, @"Failed to create ACL: %@", error);

        XCTAssertFalse(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"%@ is not expected to be implicitly bound", sac);
    }

    @autoreleasepool {
        NSError *error;
        id sac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAfterFirstUnlock, kSecAccessControlBiometryAny, (void *)&error));
        XCTAssertNotNil(sac, @"Failed to create ACL: %@", error);

        XCTAssertFalse(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"%@ is not expected to be implicitly bound", sac);
    }

    @autoreleasepool {
        NSError *error;
        id sac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAfterFirstUnlock, kSecAccessControlPrivateKeyUsage | kSecAccessControlBiometryAny, (void *)&error));
        XCTAssertNotNil(sac, @"Failed to create ACL: %@", error);

        XCTAssertFalse(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"%@ is not expected to be implicitly bound", sac);
    }

    @autoreleasepool {
        NSError *error;
        id sac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAfterFirstUnlock, kSecAccessControlUserPresence, (void *)&error));
        XCTAssertNotNil(sac, @"Failed to create ACL: %@", error);

        XCTAssertFalse(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"%@ is not expected to be implicitly bound", sac);
    }

    @autoreleasepool {
        NSError *error;
        id sac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAfterFirstUnlock, kSecAccessControlPrivateKeyUsage | kSecAccessControlUserPresence, (void *)&error));
        XCTAssertNotNil(sac, @"Failed to create ACL: %@", error);

        XCTAssertFalse(SecAccessControlIsBound((__bridge SecAccessControlRef)sac), @"%@ is not expected to be implicitly bound", sac);
    }
}

@end
