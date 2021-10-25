/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/OTPreflightVouchWithCustodianRecoveryKeyOperation.h"
#import "keychain/ot/OTStates.h"

#import "keychain/OctagonTrust/OTCustodianRecoveryKey.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTPreflightVouchWithCustodianRecoveryKeyOperation ()
@property OTOperationDependencies* deps;

@property NSString* salt;

@property NSOperation* finishOp;

@property TrustedPeersHelperCustodianRecoveryKey *tphcrk;

@end

@implementation OTPreflightVouchWithCustodianRecoveryKeyOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                              tphcrk:(TrustedPeersHelperCustodianRecoveryKey*)tphcrk
{
    if((self = [super init])) {
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;

        _tphcrk = tphcrk;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "creating voucher using a custodian recovery key");

    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];

    // First, let's preflight the vouch (to receive a policy and view set to use for TLK fetching
    WEAKIFY(self);
    [self.deps.cuttlefishXPCWrapper preflightVouchWithCustodianRecoveryKeyWithContainer:self.deps.containerName
                                                                                context:self.deps.contextID
                                                                                    crk:self.tphcrk
                                                                                  reply:^(NSString * _Nullable recoveryKeyID,
                                                                                          TPSyncingPolicy* _Nullable peerSyncingPolicy,
                                                                                          NSError * _Nullable error) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventPreflightVouchWithCustodianRecoveryKey hardFailure:true result:error];

        if(error || !recoveryKeyID) {
            secerror("octagon: Error preflighting voucher using custodian recovery key: %@", error);
            self.error = error;
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }

        secnotice("octagon", "Preflight Custodian Recovery key ID %@ looks good to go", recoveryKeyID);

        self.nextState = self.intendedState;
        [self runBeforeGroupFinished:self.finishOp];
    }];
}

@end

#endif // OCTAGON
