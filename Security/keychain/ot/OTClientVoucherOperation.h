/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#if OCTAGON

#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "OTDeviceInformation.h"

@class OTCuttlefishContext;

NS_ASSUME_NONNULL_BEGIN

@interface OTClientVoucherOperation : CKKSGroupOperation <OctagonStateTransitionOperationProtocol>
@property OctagonState* nextState;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                          deviceInfo:(nonnull OTDeviceInformation *)deviceInfo
                              peerID:(nonnull NSString *)peerID
                       permanentInfo:(nonnull NSData *)permanentInfo
                    permanentInfoSig:(nonnull NSData *)permanentInfoSig
                          stableInfo:(nonnull NSData *)stableInfo
                       stableInfoSig:(nonnull NSData *)stableInfoSig;

@property (weak) OTCuttlefishContext* cuttlefishContext;
@property (nonatomic) NSString* peerID;
@property (nonatomic) NSData* permanentInfo;
@property (nonatomic) NSData* permanentInfoSig;
@property (nonatomic) NSData* stableInfo;
@property (nonatomic) NSData* stableInfoSig;
@property OTDeviceInformation* deviceInfo;

@property (nonatomic) NSData* voucher;
@property (nonatomic) NSData* voucherSig;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
