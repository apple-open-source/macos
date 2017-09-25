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

#import <Foundation/Foundation.h>

#import "TPSigningKey.h"
#import "TPTypes.h"

NS_ASSUME_NONNULL_BEGIN

/*!
 A voucher is a record signed by a "sponsor" peer to say that
 a "beneficiary" peer is trusted.
 
 The signature is not checked when an TPVoucher instance is
 constructed, because the sponsor's signing key might not be
 available at that time. 
 
 This class is a value type -- its members are immutable and
 instances with identical contents are interchangeable.
 It overrides isEqual and hash, so that two instances with
 identical contents will compare as equal.
 */
@interface TPVoucher : NSObject

/*!
 Can return nil with error if [trustSigningKey signatureForData:error:] errors.
 */
+ (nullable instancetype)voucherWithBeneficiaryID:(NSString *)beneficiaryID
                                        sponsorID:(NSString *)sponsorID
                                            clock:(TPCounter)clock
                                  trustSigningKey:(id<TPSigningKey>)trustSigningKey
                                            error:(NSError **)error;

// Returns nil if data cannot be deserialized to a dictionary
// or that dictionary does not contain the expected keys and value types.
// This method performs no signature checking; that should be done later,
// when the sponsor's trustSigningKey is available.
+ (nullable instancetype)voucherWithPList:(NSData *)voucherInfoPList
                                      sig:(NSData *)voucherInfoSig;

- (BOOL)isEqualToVoucher:(TPVoucher *)other;

@property (nonatomic, readonly) NSString *beneficiaryID;
@property (nonatomic, readonly) NSString *sponsorID;
@property (nonatomic, readonly) TPCounter clock;
@property (nonatomic, readonly) NSData *voucherInfoPList;
@property (nonatomic, readonly) NSData *voucherInfoSig;

@end

NS_ASSUME_NONNULL_END
