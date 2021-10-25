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

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTRemoveCustodianRecoveryKeyOperation.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTRemoveCustodianRecoveryKeyOperation ()
@property OTOperationDependencies* deps;
@property NSUUID* uuid;
@property NSOperation* finishOp;
@end

@implementation OTRemoveCustodianRecoveryKeyOperation

- (instancetype)initWithUUID:(NSUUID *)uuid dependencies:(OTOperationDependencies*)dependencies
{
    if((self = [super init])) {
        _uuid = uuid;
        _deps = dependencies;
    }
    return self;
}

- (void)groupStart
{
    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];
    
    WEAKIFY(self);
    [self.deps.cuttlefishXPCWrapper removeCustodianRecoveryKeyWithContainer:self.deps.containerName
                                                                    context:self.deps.contextID
                                                                       uuid:self.uuid
                                                                      reply:^(NSError * _Nullable error) {
            STRONGIFY(self);
            [[CKKSAnalytics logger] logResultForEvent:OctagonEventCustodianRecoveryKey hardFailure:true result:error];
            if(error){
                secerror("octagon: Error removing custodian recovery key: %@", error);
                self.error = error;
                [self runBeforeGroupFinished:self.finishOp];
            } else {
                secnotice("octagon", "successfully removed custodian recovery key");
                [self runBeforeGroupFinished:self.finishOp];
            }
        }];
}

@end

#endif // OCTAGON
