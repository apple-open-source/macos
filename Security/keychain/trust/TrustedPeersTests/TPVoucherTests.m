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
#import "TPDummySigningKey.h"

@interface TPVoucherTests : XCTestCase

@end

@implementation TPVoucherTests

- (void)testRoundTrip
{
    NSData *keyData = [@"key" dataUsingEncoding:NSUTF8StringEncoding];
    id<TPSigningKey> key = [[TPDummySigningKey alloc] initWithPublicKeyData:keyData];

    TPVoucher *voucher1 = [TPVoucher voucherWithBeneficiaryID:@"B"
                                                    sponsorID:@"A"
                                                        clock:1
                                              trustSigningKey:key
                                                        error:NULL];
    TPVoucher *voucher1b = [TPVoucher voucherWithPList:voucher1.voucherInfoPList
                                                  sig:voucher1.voucherInfoSig];
    
    XCTAssertEqualObjects(voucher1, voucher1b);
    XCTAssertEqual([voucher1 hash], [voucher1b hash]);
    XCTAssert([voucher1 isEqual:voucher1]);
    XCTAssert([voucher1 isEqualToVoucher:voucher1]);
    XCTAssert(![voucher1 isEqual:@"foo"]);

    TPVoucher *voucher2 = [TPVoucher voucherWithBeneficiaryID:@"C"
                                                    sponsorID:@"A"
                                                        clock:1
                                              trustSigningKey:key
                                                        error:NULL];
    XCTAssertNotEqualObjects(voucher1, voucher2);
}

- (void)testMalformed
{
    NSData *data = [@"foo" dataUsingEncoding:NSUTF8StringEncoding];
    XCTAssertNil([TPVoucher voucherWithPList:data sig:data]);
    
    data = [TPUtils serializedPListWithDictionary:@{
                                                    @"beneficiaryID": @[],
                                                    @"sponsorID": @"A",
                                                    @"clock": @1
                                                    }];
    XCTAssertNil([TPVoucher voucherWithPList:data sig:data]);

    data = [TPUtils serializedPListWithDictionary:@{
                                                    @"beneficiaryID": @"B",
                                                    @"sponsorID": @7,
                                                    @"clock": @1
                                                    }];
    XCTAssertNil([TPVoucher voucherWithPList:data sig:data]);
    
    data = [TPUtils serializedPListWithDictionary:@{
                                                    @"beneficiaryID": @"B",
                                                    @"sponsorID": @"A",
                                                    @"clock": @"foo"
                                                    }];
    XCTAssertNil([TPVoucher voucherWithPList:data sig:data]);
}

- (void)testCannotSign
{
    NSData *keyData = [@"key" dataUsingEncoding:NSUTF8StringEncoding];
    TPDummySigningKey *key = [[TPDummySigningKey alloc] initWithPublicKeyData:keyData];
    key.privateKeyIsAvailable = NO;

    NSError *error = nil;
    TPVoucher *voucher = [TPVoucher voucherWithBeneficiaryID:@"B"
                                                   sponsorID:@"A"
                                                       clock:1
                                             trustSigningKey:key
                                                       error:&error];
    XCTAssertNil(voucher);
}

@end
