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

#import "keychain/ot/OTEpochOperation.h"
#import "keychain/ot/OTCuttlefishContext.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@implementation OTEpochOperation
@synthesize intendedState = _intendedState;

- (instancetype)initForCuttlefishContext:(OTCuttlefishContext*)context
                           intendedState:(OctagonState*)intendedState
                              errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _intendedState = intendedState;
        _nextState = errorState;
        self.cuttlefishContext = context;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "retrieving epoch");

    NSOperation* finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:finishOp];

    OTCuttlefishContext* strongCuttlefishContext = self.cuttlefishContext;

    WEAKIFY(self);

    [[strongCuttlefishContext.cuttlefishXPCConnection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        STRONGIFY(self);
        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
        self.error = error;
        [self runBeforeGroupFinished:finishOp];

    }]  fetchEgoEpochWithContainer:strongCuttlefishContext.containerName
     context:strongCuttlefishContext.contextID
     reply:^(uint64_t epoch, NSError* _Nullable error) {
         STRONGIFY(self);
         if(error) {
             secerror("octagon: Error getting epoch: %@", error);
             self.error = error;
         } else {
             self.epoch = epoch;
             self.nextState = self.intendedState;
         }
         [self runBeforeGroupFinished:finishOp];

     }];
}

@end

#endif // OCTAGON
