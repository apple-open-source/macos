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

#import "SecKEMTester.h"
#import <XCTest/XCTest.h>

@interface SecKeyKyberTests : XCTestCase
@end

/// Tests the non-SEP Kyber keys (backed by corecrypto)
@implementation SecKeyKyberTests

- (void)testKyberGenKeys768 {
    [SecKEMTester verifyKEMTestGenKeysWithConfig:SecKEMTesterConfig.kyber768Config];
}

- (void)testKyberGenKeys1024 {
    [SecKEMTester verifyKEMTestGenKeysWithConfig:SecKEMTesterConfig.kyber1024Config];
}

- (void)testKyberEncapsulateWithSecKey768 {
    [SecKEMTester verifyKEMTestEncapsulateWithConfig:SecKEMTesterConfig.kyber768Config];
}

- (void)testKyberEncapsulateWithSecKey1024 {
    [SecKEMTester verifyKEMTestEncapsulateWithConfig:SecKEMTesterConfig.kyber1024Config];
}

- (void)testKyberDecapsulateWithSecKey768 {
    [SecKEMTester verifyKEMTestDecapsulateWithConfig:SecKEMTesterConfig.kyber768Config];
}

- (void)testKyberDecapsulateWithSecKey1024 {
    [SecKEMTester verifyKEMTestDecapsulateWithConfig:SecKEMTesterConfig.kyber1024Config];
}

@end
