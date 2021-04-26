/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecPolicyInternal.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>

#include "../TestMacroConversions.h"
#include "../TrustEvaluationTestHelpers.h"
#include "TrustFrameworkTestCase.h"

@interface PolicyInterfaceTests : TrustFrameworkTestCase
@end

@implementation PolicyInterfaceTests

- (void)testCreateWithProperties
{
    const void *keys[] = { kSecPolicyName, kSecPolicyClient };
    const void *values[] = { CFSTR("www.google.com"), kCFBooleanFalse };
    CFDictionaryRef properties = CFDictionaryCreate(NULL, keys, values,
            array_size(keys),
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    SecPolicyRef policy = SecPolicyCreateWithProperties(kSecPolicyAppleSSL, properties);
    isnt(policy, NULL, "SecPolicyCreateWithProperties");
    CFReleaseSafe(properties);
}

- (void)testCopyProperties
{
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("www.google.com"));
    CFDictionaryRef properties = NULL;
    isnt(properties = SecPolicyCopyProperties(policy), NULL, "copy policy properties");
    CFTypeRef value = NULL;
    is(CFDictionaryGetValueIfPresent(properties, kSecPolicyName, (const void **)&value) &&
        kCFCompareEqualTo == CFStringCompare((CFStringRef)value, CFSTR("www.google.com"), 0),
        true, "has policy name");
    is(CFDictionaryGetValueIfPresent(properties, kSecPolicyOid, (const void **)&value) &&
        CFEqual(value, kSecPolicyAppleSSL) , true, "has SSL policy");
    CFReleaseSafe(properties);
}

- (void)testSetSHA256Pins
{
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    CFDictionaryRef options = SecPolicyGetOptions(policy);
    XCTAssertEqual(CFDictionaryGetValue(options, kSecPolicyCheckLeafSPKISHA256), NULL);
    XCTAssertEqual(CFDictionaryGetValue(options, kSecPolicyCheckCAspkiSHA256), NULL);

    NSArray *pins = @[ ];
    SecPolicySetSHA256Pins(policy, (__bridge CFArrayRef)pins, (__bridge CFArrayRef)pins);
    XCTAssertEqualObjects((__bridge NSArray *)CFDictionaryGetValue(options, kSecPolicyCheckLeafSPKISHA256), pins);
    XCTAssertEqualObjects((__bridge NSArray *)CFDictionaryGetValue(options, kSecPolicyCheckCAspkiSHA256), pins);

    SecPolicySetSHA256Pins(policy, NULL, (__bridge CFArrayRef)pins);
    XCTAssertEqual(CFDictionaryGetValue(options, kSecPolicyCheckLeafSPKISHA256), NULL);
    XCTAssertEqualObjects((__bridge NSArray *)CFDictionaryGetValue(options, kSecPolicyCheckCAspkiSHA256), pins);

    SecPolicySetSHA256Pins(policy, (__bridge CFArrayRef)pins, NULL);
    XCTAssertEqualObjects((__bridge NSArray *)CFDictionaryGetValue(options, kSecPolicyCheckLeafSPKISHA256), pins);
    XCTAssertEqual(CFDictionaryGetValue(options, kSecPolicyCheckCAspkiSHA256), NULL);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    SecPolicySetSHA256Pins(NULL, NULL, NULL);
    XCTAssertEqualObjects((__bridge NSArray *)CFDictionaryGetValue(options, kSecPolicyCheckLeafSPKISHA256), pins);
    XCTAssertEqual(CFDictionaryGetValue(options, kSecPolicyCheckCAspkiSHA256), NULL);
#pragma clang diagnostic pop
}

@end
