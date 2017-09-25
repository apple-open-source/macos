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

#import "TPPeerStableInfo.h"
#import "TPUtils.h"

static const NSString *kClock = @"clock";
static const NSString *kPolicyVersion = @"policyVersion";
static const NSString *kPolicyHash = @"policyHash";
static const NSString *kPolicySecrets = @"policySecrets";


@interface TPPeerStableInfo ()

@property (nonatomic, strong) NSDictionary *dict;
@property (nonatomic, assign) TPCounter clock;
@property (nonatomic, assign) TPCounter policyVersion;
@property (nonatomic, strong) NSString *policyHash;
@property (nonatomic, strong) NSDictionary<NSString*,NSData*> *policySecrets;
@property (nonatomic, strong) NSData *stableInfoPList;
@property (nonatomic, strong) NSData *stableInfoSig;

@end


@implementation TPPeerStableInfo

+ (instancetype)stableInfoWithDict:(NSDictionary *)dict
                             clock:(TPCounter)clock
                     policyVersion:(TPCounter)policyVersion
                        policyHash:(NSString *)policyHash
                     policySecrets:(NSDictionary<NSString*,NSData*> *)policySecrets
                   trustSigningKey:(id<TPSigningKey>)trustSigningKey
                             error:(NSError **)error
{
    NSMutableDictionary *mutDict = [NSMutableDictionary dictionaryWithDictionary:dict];
    mutDict[kClock] = @(clock);
    mutDict[kPolicyVersion] = @(policyVersion);
    mutDict[kPolicyHash] = policyHash;
    mutDict[kPolicySecrets] = policySecrets;
    
    NSData *data = [TPUtils serializedPListWithDictionary:mutDict];
    NSData *sig = [trustSigningKey signatureForData:data withError:error];
    if (nil == sig) {
        return nil;
    }
    TPPeerStableInfo *info = [self stableInfoWithPListData:data stableInfoSig:sig];;
    assert(info);
    return info;
}

+ (instancetype)stableInfoWithPListData:(NSData *)stableInfoPList
                          stableInfoSig:(NSData *)stableInfoSig
{
    id dict = [NSPropertyListSerialization propertyListWithData:stableInfoPList
                                                        options:NSPropertyListImmutable
                                                         format:nil
                                                          error:NULL];
    if (![dict isKindOfClass:[NSDictionary class]]) {
        return nil;
    }

    TPPeerStableInfo* info = [[TPPeerStableInfo alloc] init];

    if (![dict[kClock] isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    info.clock = [dict[kClock] unsignedLongLongValue];

    if (![dict[kPolicyVersion] isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    info.policyVersion = [dict[kPolicyVersion] unsignedLongLongValue];

    if (![dict[kPolicyHash] isKindOfClass:[NSString class]]) {
        return nil;
    }
    info.policyHash = dict[kPolicyHash];

    if ([dict[kPolicySecrets] isKindOfClass:[NSDictionary class]]) {
        NSDictionary *secrets = dict[kPolicySecrets];
        for (id name in secrets) {
            NSAssert([name isKindOfClass:[NSString class]], @"plist keys must be strings");
            if (![secrets[name] isKindOfClass:[NSData class]]) {
                return nil;
            }
        }
        info.policySecrets = secrets;
    } else if (nil == dict[kPolicySecrets]) {
        info.policySecrets = @{};
    } else {
        return nil;
    }
    
    info.dict = dict;
    info.stableInfoPList = [stableInfoPList copy];
    info.stableInfoSig = [stableInfoSig copy];
    
    return info;
}

- (BOOL)isEqualToPeerStableInfo:(TPPeerStableInfo *)other
{
    if (other == self) {
        return YES;
    }
    return [self.stableInfoPList isEqualToData:other.stableInfoPList]
        && [self.stableInfoSig isEqualToData:other.stableInfoSig];
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object
{
    if (self == object) {
        return YES;
    }
    if (![object isKindOfClass:[TPPeerStableInfo class]]) {
        return NO;
    }
    return [self isEqualToPeerStableInfo:object];
}

@end
