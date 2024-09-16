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
 *
 */

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecPolicyInternal.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <Foundation/NSJSONSerialization.h>
#include "SecJWS.h"
#include "TrustFrameworkTestCase.h"

@interface SecJWSEncoder (Private)
- (BOOL) appendPaddedToData:(NSMutableData *)data ptr:(const uint8_t *)ptr len:(size_t)len expected:(size_t)expLen;
@end

@interface CertificateRequestTests : TrustFrameworkTestCase
@end

@implementation CertificateRequestTests

static void test_jws_signature(void) {
    // create an encoder, which generates a default signing key pair
    SecJWSEncoder *encoder = [[SecJWSEncoder alloc] init];
    NSMutableDictionary *payload = [NSMutableDictionary dictionary];
    payload[@"termsOfServiceAgreed"] = @(YES); // fixed payload item
    NSString *url = @"https://localhost:14000/dir"; // fixed url
    NSString *nonce = [[NSUUID UUID] UUIDString]; // one-time nonce
    NSError *encodeError = NULL;
    NSString *jwsString = [encoder encodedJWSWithPayload:payload kid:nil nonce:nonce url:url error:&encodeError];
    // verify we successfully encoded and signed the JSON
    XCTAssertNotEqual(jwsString, NULL);

    // convert the JSON into a dictionary
    NSError *decodeError = NULL;
    NSData *jwsData = [jwsString dataUsingEncoding:NSUTF8StringEncoding];
    XCTAssertNotEqual(jwsData, NULL);
    NSDictionary *jwsDict = [NSJSONSerialization JSONObjectWithData:jwsData options:0 error:&decodeError];
    XCTAssertNotEqual(jwsDict, NULL);

    // verify we can make a compact encoded string from its components
    NSString *jwsCompactString = [NSString stringWithFormat:@"%@.%@.%@", jwsDict[@"protected"], jwsDict[@"payload"], jwsDict[@"signature"]];
    XCTAssertNotEqual(jwsCompactString, NULL);

    // test that the compact encoded string decodes successfully
    SecJWSDecoder *decoder = [[SecJWSDecoder alloc] initWithJWSCompactEncodedString:jwsCompactString keyID:nil publicKey:encoder.publicKey];
    // %%% check verificationError when SecJWSDecoder has been completed
    // XCTAssertEqual(decoder.verificationError, NULL);

    // test that the signature is the expected size (rdar://116576444)
    XCTAssertEqual([decoder.signature length], 64);
}

- (void)testJWSSignatures {
    // generate and verify JWS signatures
    // (each invocation uses a different key and nonce)
    CFIndex count = 8; // number of signatures to generate
    for (CFIndex i = 0; i < count; i++) {
        test_jws_signature();
    }
}

- (void)testZeroPadding {
    SecJWSEncoder *encoder = [[SecJWSEncoder alloc] init];
    NSMutableData *data = NULL;
    BOOL result = false;

    const uint8_t needsZeroPaddingTo32[] = {
             0x80,0xad,0x23,0x45,0x56,0x12,0xac,0xfe,0xfa,0xce,0x29,0x34,0xfb,0xfa,0x19,
        0x40,0xcc,0xad,0x23,0x45,0x56,0x12,0xac,0xfe,0xfa,0xce,0x29,0x34,0xfb,0xff,0x01
    };
    data = [NSMutableData dataWithCapacity:0];
    result = [encoder appendPaddedToData:data ptr:needsZeroPaddingTo32 len:sizeof(needsZeroPaddingTo32) expected:32];
    XCTAssertEqual(result, YES);
    XCTAssertEqual([data length], 32);

    const uint8_t needsZeroRemovalTo32[] = {
        0x00,0x00,
        0x32,0xcc,0xad,0x23,0x45,0x56,0x12,0xac,0xfe,0xfa,0xce,0x29,0x34,0xfb,0xfa,0x19,
        0x40,0xcc,0xad,0x23,0x45,0x56,0x12,0xac,0xfe,0xfa,0xce,0x29,0x34,0xfb,0xff,0x01
    };
    data = [NSMutableData dataWithCapacity:0];
    result = [encoder appendPaddedToData:data ptr:needsZeroRemovalTo32 len:sizeof(needsZeroRemovalTo32) expected:32];
    XCTAssertEqual(result, YES);
    XCTAssertEqual([data length], 32);

    const uint8_t needsNoPaddingIs32[] = {
        0x00,0x82,0x32,0xcc,0xad,0x23,0x45,0x56,0x12,0xac,0xfe,0xfa,0xce,0x29,0x34,0xfb,
        0x40,0xcc,0xad,0x23,0x45,0x56,0x12,0xac,0xfe,0xfa,0xce,0x29,0x34,0xfb,0xff,0x01
    };
    data = [NSMutableData dataWithCapacity:0];
    result = [encoder appendPaddedToData:data ptr:needsNoPaddingIs32 len:sizeof(needsNoPaddingIs32) expected:32];
    XCTAssertEqual(result, YES);
    XCTAssertEqual([data length], 32);

    const uint8_t cannotBeMadeValid32[] = {
        0x00,0x00,0xaa,0xaa,
        0x00,0x82,0x32,0xcc,0xad,0x23,0x45,0x56,0x12,0xac,0xfe,0xfa,0xce,0x29,0x34,0xfb,
        0x40,0xcc,0xad,0x23,0x45,0x56,0x12,0xac,0xfe,0xfa,0xce,0x29,0x34,0xfb,0xff,0x01
    };
    data = [NSMutableData dataWithCapacity:0];
    result = [encoder appendPaddedToData:data ptr:cannotBeMadeValid32 len:sizeof(cannotBeMadeValid32) expected:32];
    XCTAssertEqual(result, NO); // this case should fail
}

@end
