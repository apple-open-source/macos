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

#import "keychain/ot/OTJoinSOSAfterCKKSFetchOperation.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTOperationDependencies.h"
#include "keychain/SecureObjectSync/SOSAccount.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTWaitOnPriorityViews.h"

@interface OTJoinSOSAfterCKKSFetchOperation ()
@property OTOperationDependencies* operationDependencies;
@property NSOperation* finishedOp;
@end

@implementation OTJoinSOSAfterCKKSFetchOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _intendedState = intendedState;
        _nextState = errorState;
        _operationDependencies = dependencies;
    }
    return self;
}

- (void)groupStart
{
    if(!self.operationDependencies.sosAdapter.sosEnabled) {
        secnotice("octagon-sos", "SOS not enabled on this platform?");
        self.nextState = self.intendedState;
        return;
    }
    
    secnotice("octagon-sos", "joining SOS");
    
    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];
    
    OTWaitOnPriorityViews* op = [[OTWaitOnPriorityViews alloc] initWithDependencies:self.operationDependencies];
    
    [op timeout:10*NSEC_PER_SEC];
    
    [self runBeforeGroupFinished:op];
    
    WEAKIFY(self);

    CKKSResultOperation* proceedAfterFetch = [CKKSResultOperation named:@"join-sos-after-fetch"
                                                              withBlock:^{
        STRONGIFY(self);
        [self proceedAfterFetch];
    }];
   
    [proceedAfterFetch addDependency:op];
    [self runBeforeGroupFinished:proceedAfterFetch];
}

- (void)proceedAfterFetch
{
    NSError* restoreError = nil;
    bool restoreResult = [self.operationDependencies.sosAdapter joinAfterRestore:&restoreError];
    
    if (restoreError && restoreError.code == kSOSErrorPrivateKeyAbsent && [restoreError.domain isEqualToString:(id)kSOSErrorDomain]) {
        self.error = restoreError;
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }
    
    NSError* circleStatusError = nil;
    SOSCCStatus sosCircleStatus = [self.operationDependencies.sosAdapter circleStatus:&circleStatusError];
    if ((circleStatusError && circleStatusError.code == kSOSErrorPrivateKeyAbsent && [circleStatusError.domain isEqualToString:(id)kSOSErrorDomain])
        || sosCircleStatus == kSOSCCError) {
        secnotice("octagon-sos", "Error fetching circle status: %d, error:%@", sosCircleStatus, circleStatusError);
        self.error = circleStatusError;
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }
    
    if (!restoreResult || restoreError || (sosCircleStatus == kSOSCCRequestPending || sosCircleStatus == kSOSCCNotInCircle)) {
        NSError* resetToOfferingError = nil;
        bool successfulReset = [self.operationDependencies.sosAdapter resetToOffering:&resetToOfferingError];
        
        secnotice("octagon-sos", "SOSCCResetToOffering complete: %d %@", successfulReset, resetToOfferingError);
        if (!successfulReset || resetToOfferingError) {
            self.error = resetToOfferingError;
            [self runBeforeGroupFinished:self.finishedOp];
            return;
        }
    }
    self.nextState = self.intendedState;
    
    [self runBeforeGroupFinished:self.finishedOp];
}

@end

#endif // OCTAGON
