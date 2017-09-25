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

#import <XCTest/XCTest.h>
#import <TrustedPeers/TrustedPeers.h>

@interface TPCircleTests : XCTestCase

@end

@implementation TPCircleTests

- (void)testCircleIDChecks {
    NSArray *included = @[@"A", @"B"];
    NSArray *excluded = @[@"C", @"D"];
    TPCircle *circle1 = [TPCircle circleWithIncludedPeerIDs:included excludedPeerIDs:excluded];
    TPCircle *circle2 = [TPCircle circleWithID:circle1.circleID includedPeerIDs:included excludedPeerIDs:excluded];
    XCTAssertEqual([circle1 hash], [circle2 hash]);
    XCTAssertEqualObjects(circle1, circle2);
    XCTAssert([circle1 isEqual:circle1]);
    XCTAssertEqualObjects(circle1, circle2);
    XCTAssertNotEqualObjects(circle1, @"foo");
    
    // (Feel free to change the format of the description output, this is just for test coverage.)
    XCTAssertEqualObjects([circle1 description], @"{ in: [A B] ex: [C D] }");
    
    // Misuse circle1.circleID here, trying to construct a different circle with nil excludedPeerIDs:
    TPCircle *circle3 = [TPCircle circleWithID:circle1.circleID includedPeerIDs:included excludedPeerIDs:nil];
    XCTAssertNil(circle3);
}

@end
