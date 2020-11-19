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

#import <utilities/debugging.h>

#import "keychain/ot/OTClientVoucherOperation.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTClientVoucherOperation ()
@property OTOperationDependencies* operationDependencies;
@property NSOperation* finishedOp;
@end

@implementation OTClientVoucherOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                          deviceInfo:(nonnull OTDeviceInformation *)deviceInfo
                              peerID:(nonnull NSString *)peerID
                       permanentInfo:(nonnull NSData *)permanentInfo
                    permanentInfoSig:(nonnull NSData *)permanentInfoSig
                          stableInfo:(nonnull NSData *)stableInfo
                       stableInfoSig:(nonnull NSData *)stableInfoSig
{
    if((self = [super init])) {
        _intendedState = intendedState;
        _nextState = errorState;

        _operationDependencies = dependencies;

        self.peerID = peerID;
        self.permanentInfo = permanentInfo;
        self.permanentInfoSig = permanentInfoSig;
        self.stableInfo = stableInfo;
        self.stableInfoSig = stableInfoSig;
        self.deviceInfo = deviceInfo;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "creating voucher");

    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    WEAKIFY(self);

    // Acquire the CKKS TLKs to pass in
    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithDependencies:self.operationDependencies
                                                                                     refetchNeeded:NO];
    [self runBeforeGroupFinished:fetchKeysOp];

    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"vouch-with-keys"
                                                            withBlock:^{
                                                                STRONGIFY(self);
                                                                [self proceedWithKeys:fetchKeysOp.viewKeySets];
                                                            }];

    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets
{
    WEAKIFY(self);

    secnotice("octagon", "vouching with %d keysets", (int)viewKeySets.count);

    [self.operationDependencies.cuttlefishXPCWrapper vouchWithContainer:self.deviceInfo.containerName
                                                                context:self.deviceInfo.contextID
                                                                 peerID:self.peerID
                                                          permanentInfo:self.permanentInfo
                                                       permanentInfoSig:self.permanentInfoSig
                                                             stableInfo:self.stableInfo
                                                          stableInfoSig:self.stableInfoSig
                                                               ckksKeys:viewKeySets
                                                                  reply:^(NSData * _Nullable voucher,
                                                                          NSData * _Nullable voucherSig,
                                                                          NSError * _Nullable error)
         {
             STRONGIFY(self);
             if(error){
                 secerror("octagon: Error preparing voucher: %@", error);
                 self.error = error;
             }else{
                 self.voucher = voucher;
                 self.voucherSig = voucherSig;
                 self.nextState = self.intendedState;
             }
             [self runBeforeGroupFinished:self.finishedOp];
         }];
}

@end

#endif // OCTAGON
