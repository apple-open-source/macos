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

#import "keychain/ot/OTResetOperation.h"

#import "keychain/ot/ObjCImprovements.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"

@interface OTResetOperation ()
@property NSString* containerName;
@property NSString* contextID;
@property CuttlefishXPCWrapper* cuttlefishXPCWrapper;

// Since we're making callback based async calls, use this operation trick to hold off the ending of this operation
@property NSOperation* finishedOp;
@end

@implementation OTResetOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)init:(NSString*)containerName
           contextID:(NSString*)contextID
              reason:(CuttlefishResetReason)reason
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
        _resetReason = reason;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon-authkit", "Attempting to reset octagon");

    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    WEAKIFY(self);
    [self.cuttlefishXPCWrapper resetWithContainer:self.containerName
                                          context:self.contextID
                                      resetReason:self.resetReason
                                            reply:^(NSError * _Nullable error) {
            STRONGIFY(self);
            [[CKKSAnalytics logger] logResultForEvent:OctagonEventReset hardFailure:true result:error];
        
            if(error) {
                secnotice("octagon", "Unable to reset for (%@,%@): %@", self.containerName, self.contextID, error);
                self.error = error;
            } else {
                secnotice("octagon", "Successfully reset Octagon");
                self.nextState = self.intendedState;
            }

            [self runBeforeGroupFinished:self.finishedOp];
        }];
}

@end

#endif // OCTAGON
