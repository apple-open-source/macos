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

#import "keychain/ot/OTLocalCuttlefishReset.h"

#import "keychain/ot/ObjCImprovements.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "utilities/debugging.h"

@interface OTLocalResetOperation ()
@property OTOperationDependencies* deps;

@property NSOperation* finishedOp;
@end

@implementation OTLocalResetOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithDependencies:(OTOperationDependencies *)deps
                       intendedState:(OctagonState *)intendedState
                          errorState:(OctagonState *)errorState
{
    if((self = [super init])) {
        _intendedState = intendedState;
        _nextState = errorState;

        _deps = deps;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon-local-reset", "Resetting local cuttlefish");

    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    WEAKIFY(self);
    [self.deps.cuttlefishXPCWrapper localResetWithContainer:self.deps.containerName
                                                    context:self.deps.contextID
                                                      reply:^(NSError * _Nullable error) {
            STRONGIFY(self);
            if(error) {
                secnotice("octagon", "Unable to reset local cuttlefish for (%@,%@): %@", self.deps.containerName, self.deps.contextID, error);
                self.error = error;
            } else {
                secnotice("octagon", "Successfully reset local cuttlefish");

                NSError* localError = nil;
                [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
                    metadata.trustState = OTAccountMetadataClassC_TrustState_UNKNOWN;
                    metadata.peerID = nil;
                    metadata.syncingPolicy = nil;

                    // Don't touch the CDP or account states; those can carry over

                    return metadata;
                } error:&localError];

                if(localError) {
                    secnotice("octagon", "Error resetting local account state: %@", localError);

                } else {
                    secnotice("octagon", "Successfully reset local account state");
                    self.nextState = self.intendedState;
                }
            }

            [self runBeforeGroupFinished:self.finishedOp];
        }];
}

@end

#endif // OCTAGON
