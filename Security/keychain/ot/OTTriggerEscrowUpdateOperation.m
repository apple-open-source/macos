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

#import "keychain/ot/OTTriggerEscrowUpdateOperation.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"

@interface OTTriggerEscrowUpdateOperation ()
@property OTOperationDependencies* deps;
@property NSOperation* finishedOp;
@end

@implementation OTTriggerEscrowUpdateOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                           errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "Triggering escrow update");

    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];


    NSError* error = nil;
    id<SecEscrowRequestable> request = [self.deps.escrowRequestClass request:&error];
    if(!request || error) {
        secnotice("octagon-sos", "Unable to acquire a EscrowRequest object: %@", error);
        [self runBeforeGroupFinished:self.finishedOp];
        self.error = error;
        return;
    }

    [request triggerEscrowUpdate:@"octagon-sos" error:&error];
    [[CKKSAnalytics logger] logResultForEvent:OctagonEventUpgradeSilentEscrow hardFailure:true result:error];

    if(error) {
        secnotice("octagon-sos", "Unable to request silent escrow update: %@", error);
        self.error = error;
    } else{
        secnotice("octagon-sos", "Requested silent escrow update");
        self.nextState = self.intendedState;
    }
    [self runBeforeGroupFinished:self.finishedOp];
}

@end

#endif // OCTAGON

