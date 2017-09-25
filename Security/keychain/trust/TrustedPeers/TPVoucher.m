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

#import "TPVoucher.h"
#import "TPUtils.h"

static const NSString *kBeneficiaryID = @"beneficiaryID";
static const NSString *kSponsorID = @"sponsorID";
static const NSString *kClock = @"clock";


@interface TPVoucher ()
@property (nonatomic, strong) NSString *beneficiaryID;
@property (nonatomic, strong) NSString *sponsorID;
@property (nonatomic, assign) TPCounter clock;
@property (nonatomic, strong) NSData *voucherInfoPList;
@property (nonatomic, strong) NSData *voucherInfoSig;
@end


@implementation TPVoucher

+ (instancetype)voucherWithBeneficiaryID:(NSString *)beneficiaryID
                               sponsorID:(NSString *)sponsorID
                                   clock:(TPCounter)clock
                         trustSigningKey:(id<TPSigningKey>)trustSigningKey
                                   error:(NSError **)error
{
    NSDictionary *dict = @{
                           kBeneficiaryID: beneficiaryID,
                           kSponsorID: sponsorID,
                           kClock: @(clock)
                           };
    NSData *data = [TPUtils serializedPListWithDictionary:dict];
    NSData *sig = [trustSigningKey signatureForData:data withError:error];
    if (nil == sig) {
        return nil;
    }
    
    TPVoucher *voucher = [[TPVoucher alloc] init];
    voucher.beneficiaryID = [beneficiaryID copy];
    voucher.sponsorID = [sponsorID copy];
    voucher.clock = clock;
    voucher.voucherInfoPList = data;
    voucher.voucherInfoSig = sig;
    return voucher;
}

+ (instancetype)voucherWithPList:(NSData *)voucherInfoPList
                             sig:(NSData *)voucherInfoSig
{
    TPVoucher *voucher = [[TPVoucher alloc] init];
    voucher.voucherInfoPList = [voucherInfoPList copy];
    voucher.voucherInfoSig = [voucherInfoSig copy];
    
    id dict = [NSPropertyListSerialization propertyListWithData:voucherInfoPList
                                                        options:NSPropertyListImmutable
                                                         format:nil
                                                          error:NULL];
    if (![dict isKindOfClass:[NSDictionary class]]) {
        return nil;
    }

    if (![dict[kBeneficiaryID] isKindOfClass:[NSString class]]) {
        return nil;
    }
    voucher.beneficiaryID = dict[kBeneficiaryID];
    
    if (![dict[kSponsorID] isKindOfClass:[NSString class]]) {
        return nil;
    }
    voucher.sponsorID = dict[kSponsorID];
    
    if (![dict[kClock] isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    voucher.clock = [dict[kClock] unsignedLongLongValue];
    
    return voucher;
}

- (BOOL)isEqualToVoucher:(TPVoucher *)other
{
    if (other == self) {
        return YES;
    }
    return [self.voucherInfoPList isEqualToData:other.voucherInfoPList]
        && [self.voucherInfoSig isEqualToData:other.voucherInfoSig];
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object
{
    if (self == object) {
        return YES;
    }
    if (![object isKindOfClass:[TPVoucher class]]) {
        return NO;
    }
    return [self isEqualToVoucher:object];
}

- (NSUInteger)hash
{
    return [self.voucherInfoPList hash];
}

@end
