/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

#include <utilities/SecCFWrappers.h>

@interface SecCFWrappersTests : XCTestCase

@end

@implementation SecCFWrappersTests

- (void)testCreateStringByTrimmingCharactersInSet {
    NSCharacterSet *asciiDigits = [NSCharacterSet characterSetWithRange:NSMakeRange('0', '9')];

    NSString *noDigits = CFBridgingRelease(CFStringCreateByTrimmingCharactersInSet((__bridge CFStringRef)@"keys", (__bridge CFCharacterSetRef)asciiDigits));
    XCTAssertEqualObjects(noDigits, @"keys", "Should return string without digits as-is");

    NSString *leadingDigits = CFBridgingRelease(CFStringCreateByTrimmingCharactersInSet((__bridge CFStringRef)@"12keys", (__bridge CFCharacterSetRef)asciiDigits));
    XCTAssertEqualObjects(leadingDigits, @"keys", "Should trim leading digits");

    NSString *trailingDigits = CFBridgingRelease(CFStringCreateByTrimmingCharactersInSet((__bridge CFStringRef)@"keys34", (__bridge CFCharacterSetRef)asciiDigits));
    XCTAssertEqualObjects(trailingDigits, @"keys", "Should trim trailing digits");

    NSString *leadingAndTrailingDigits = CFBridgingRelease(CFStringCreateByTrimmingCharactersInSet((__bridge CFStringRef)@"12keys34", (__bridge CFCharacterSetRef)asciiDigits));
    XCTAssertEqualObjects(leadingAndTrailingDigits, @"keys", "Should trim leading and trailing digits");

    NSString *allDigits = CFBridgingRelease(CFStringCreateByTrimmingCharactersInSet((__bridge CFStringRef)@"1234", (__bridge CFCharacterSetRef)asciiDigits));
    XCTAssertEqual(allDigits.length, 0, "Should return empty string for all digits");
}

@end
