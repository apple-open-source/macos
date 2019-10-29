/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/OTVouchWithRecoveryKeyOperation.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTVouchWithRecoveryKeyOperation ()
@property OTOperationDependencies* deps;

@property NSOperation* finishOp;
@end

@implementation OTVouchWithRecoveryKeyOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                         recoveryKey:(NSString*)recoveryKey
{
    if((self = [super init])) {
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;

        _recoveryKey = recoveryKey;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "creating voucher using a recovery key");

    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];

    NSString* salt = nil;

    if(self.salt != nil) {
        secnotice("octagon", "using passed in altdsid, altdsid is: %@", self.salt);
        salt = self.salt;
    } else{
        if(self.deps.authKitAdapter.primaryiCloudAccountAltDSID){
            secnotice("octagon", "using auth kit adapter, altdsid is: %@", self.deps.authKitAdapter.primaryiCloudAccountAltDSID);
            salt = self.deps.authKitAdapter.primaryiCloudAccountAltDSID;
        }
        else {
            NSError* accountError = nil;
            OTAccountMetadataClassC* account = [self.deps.stateHolder loadOrCreateAccountMetadata:&accountError];

            if(account && !accountError) {
                secnotice("octagon", "retrieved account, altdsid is: %@", account.altDSID);
                salt = account.altDSID;
            }
            if(accountError || !account){
                secerror("failed to rerieve account object: %@", accountError);
            }
        }
    }

    WEAKIFY(self);

    // After a vouch, we also want to acquire all TLKs that the bottled peer might have had
    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithDependencies:self.deps];
    [self runBeforeGroupFinished:fetchKeysOp];

    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"recovery-tlks"
                                                            withBlock:^{
                                                                STRONGIFY(self);
                                                                [self proceedWithKeys:fetchKeysOp.viewKeySets tlkShares:fetchKeysOp.tlkShares salt:salt];
                                                            }];

    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets tlkShares:(NSArray<CKKSTLKShare*>*)tlkShares salt:(NSString*)salt
{
    WEAKIFY(self);

    [self.deps.cuttlefishXPCWrapper vouchWithRecoveryKeyWithContainer:self.deps.containerName
                                                              context:self.deps.contextID
                                                          recoveryKey:self.recoveryKey
                                                                 salt:salt
                                                            tlkShares:tlkShares
                                                                reply:^(NSData * _Nullable voucher, NSData * _Nullable voucherSig, NSError * _Nullable error) {
            STRONGIFY(self);
            if(error){
                [[CKKSAnalytics logger] logResultForEvent:OctagonEventVoucherWithRecoveryKey hardFailure:true result:error];
                secerror("octagon: Error preparing voucher using recovery key: %@", error);
                self.error = error;
                [self runBeforeGroupFinished:self.finishOp];
                return;
            }
            self.voucher = voucher;
            self.voucherSig = voucherSig;
            self.nextState = self.intendedState;
            [self runBeforeGroupFinished:self.finishOp];
        }];
}

@end

#endif // OCTAGON
