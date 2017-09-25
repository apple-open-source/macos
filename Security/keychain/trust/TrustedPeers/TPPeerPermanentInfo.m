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

#import "TPPeerPermanentInfo.h"
#import "TPUtils.h"

static const NSString *kMachineID = @"machineID";
static const NSString *kModelID = @"modelID";
static const NSString *kEpoch = @"epoch";
static const NSString *kTrustSigningKey = @"trustSigningKey";


@interface TPPeerPermanentInfo ()

@property (nonatomic, strong) NSString* machineID;
@property (nonatomic, strong) NSString* modelID;
@property (nonatomic, assign) TPCounter epoch;
@property (nonatomic, strong) id<TPSigningKey> trustSigningKey;
@property (nonatomic, strong) NSData *permanentInfoPList;
@property (nonatomic, strong) NSData *permanentInfoSig;
@property (nonatomic, strong) NSString *peerID;

@end


@implementation TPPeerPermanentInfo

+ (instancetype)permanentInfoWithMachineID:(NSString *)machineID
                                   modelID:(NSString *)modelID
                                     epoch:(TPCounter)epoch
                           trustSigningKey:(id<TPSigningKey>)trustSigningKey
                            peerIDHashAlgo:(TPHashAlgo)peerIDHashAlgo
                                     error:(NSError **)error
{
    TPPeerPermanentInfo* info = [[TPPeerPermanentInfo alloc] init];
    info.machineID = [machineID copy];
    info.modelID = [modelID copy];
    info.epoch = epoch;
    info.trustSigningKey = trustSigningKey;

    NSDictionary *dict = @{
                           kMachineID: machineID,
                           kModelID: modelID,
                           kEpoch: @(epoch),
                           kTrustSigningKey: [trustSigningKey publicKey]
                           };
    NSData *data = [TPUtils serializedPListWithDictionary:dict];
    NSData *sig = [trustSigningKey signatureForData:data withError:error];
    if (nil == sig) {
        return nil;
    }
    info.permanentInfoPList = data;
    info.permanentInfoSig = sig;
    info.peerID = [TPPeerPermanentInfo peerIDForPermanentInfoPList:data
                                                  permanentInfoSig:sig
                                                    peerIDHashAlgo:peerIDHashAlgo];
    return info;
}

+ (NSString *)peerIDForPermanentInfoPList:(NSData *)permanentInfoPList
                         permanentInfoSig:(NSData *)permanentInfoSig
                           peerIDHashAlgo:(TPHashAlgo)peerIDHashAlgo

{
    TPHashBuilder *hasher = [[TPHashBuilder alloc] initWithAlgo:peerIDHashAlgo];
    [hasher updateWithData:permanentInfoPList];
    [hasher updateWithData:permanentInfoSig];
    return [hasher finalHash];
}

+ (instancetype)permanentInfoWithPeerID:(NSString *)peerID
                     permanentInfoPList:(NSData *)permanentInfoPList
                       permanentInfoSig:(NSData *)permanentInfoSig
                             keyFactory:(id<TPSigningKeyFactory>)keyFactory
{
    id obj = [NSPropertyListSerialization propertyListWithData:permanentInfoPList
                                                        options:NSPropertyListImmutable
                                                         format:nil
                                                          error:NULL];
    if (![obj isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    NSDictionary *dict = obj;
    
    TPPeerPermanentInfo *info = [[TPPeerPermanentInfo alloc] init];
    info.peerID = peerID;
    info.permanentInfoPList = permanentInfoPList;
    info.permanentInfoSig = permanentInfoSig;

    if (![dict[kMachineID] isKindOfClass:[NSString class]]) {
        return nil;
    }
    info.machineID = dict[kMachineID];
    
    if (![dict[kModelID] isKindOfClass:[NSString class]]) {
        return nil;
    }
    info.modelID = dict[kModelID];
    
    if (![dict[kEpoch] isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    info.epoch = [dict[kEpoch] unsignedLongLongValue];
    
    if (![dict[kTrustSigningKey] isKindOfClass:[NSData class]]) {
        return nil;
    }
    info.trustSigningKey = [keyFactory keyWithPublicKeyData:dict[kTrustSigningKey]];
    if (nil == info.trustSigningKey) {
        return nil;
    }
    if (![info.trustSigningKey checkSignature:permanentInfoSig matchesData:permanentInfoPList]) {
        return nil;
    }
    
    // check peerID is hash of (permanentInfoPList + permanentInfoSig)
    TPHashAlgo algo = [TPHashBuilder algoOfHash:peerID];
    if (algo == kTPHashAlgoUnknown) {
        return nil;
    }
    NSString* checkHash = [TPPeerPermanentInfo peerIDForPermanentInfoPList:info.permanentInfoPList
                                                  permanentInfoSig:info.permanentInfoSig
                                                    peerIDHashAlgo:algo];
    if (![checkHash isEqualToString:peerID]) {
        return nil;
    }
    
    return info;
}

@end
