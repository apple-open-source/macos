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
@property NSString* containerName;
@property NSString* contextID;
@property CuttlefishXPCWrapper* cuttlefishXPCWrapper;
@property NSOperation* finishedOp;
@end

@implementation OTLocalResetOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)init:(NSString*)containerName
           contextID:(NSString*)contextID
       intendedState:(OctagonState*)intendedState
          errorState:(OctagonState*)errorState
cuttlefishXPCWrapper:(CuttlefishXPCWrapper*)cuttlefishXPCWrapper
{
    if((self = [super init])) {
        _intendedState = intendedState;
        _nextState = errorState;

        _containerName = containerName;
        _contextID = contextID;
        _cuttlefishXPCWrapper = cuttlefishXPCWrapper;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon-local-reset", "Resetting local cuttlefish");

    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    WEAKIFY(self);
    [self.cuttlefishXPCWrapper localResetWithContainer:self.containerName
                                               context:self.contextID
                                                 reply:^(NSError * _Nullable error) {
            STRONGIFY(self);
            if(error) {
                secnotice("octagon", "Unable to reset local cuttlefish for (%@,%@): %@", self.containerName, self.contextID, error);
                self.error = error;
            } else {
                secnotice("octagon", "Successfully reset local cuttlefish");
                self.nextState = self.intendedState;
            }

            [self runBeforeGroupFinished:self.finishedOp];
        }];
}

@end

#endif // OCTAGON
