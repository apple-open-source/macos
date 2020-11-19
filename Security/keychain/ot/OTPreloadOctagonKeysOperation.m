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

#import <SecurityFoundation/SecurityFoundation.h>
#import "keychain/ot/OTPreloadOctagonKeysOperation.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OctagonCKKSPeerAdapter.h"
#import "utilities/debugging.h"
#import <Security/SecKey.h>
#import <Security/SecKeyPriv.h>

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/securityd/SOSCloudCircleServer.h"

@interface OTPreloadOctagonKeysOperation ()
@property OTOperationDependencies* deps;

@property NSOperation* finishOp;
@end

@implementation OTPreloadOctagonKeysOperation
@synthesize intendedState = _intendedState;

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
    secnotice("octagon-preload-keys", "Beginning operation that preloads the SOSAccount with newly created Octagon Keys");

    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];

    if(!self.deps.sosAdapter.sosEnabled) {
        self.error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorSOSAdapter userInfo:@{NSLocalizedDescriptionKey : @"sos adapter not enabled"}];
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    NSError* fetchSelfPeersError = nil;
    CKKSSelves *selfPeers = [self.deps.octagonAdapter fetchSelfPeers:&fetchSelfPeersError];
    if((!selfPeers) || fetchSelfPeersError) {
        secnotice("octagon-preload-keys", "failed to retrieve self peers: %@", fetchSelfPeersError);
        self.error = fetchSelfPeersError;
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    id<CKKSSelfPeer> currentSelfPeer = selfPeers.currentSelf;
    if(currentSelfPeer == nil) {
        secnotice("octagon-preload-keys", "failed to retrieve current self");
        self.error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorOctagonAdapter userInfo: @{ NSLocalizedDescriptionKey : @"failed to retrieve current self"}];
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    NSError* updateError = nil;
    BOOL ret = [self.deps.sosAdapter preloadOctagonKeySetOnAccount:currentSelfPeer error:&updateError];
    if(!ret) {
        self.error = updateError;
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    self.nextState = self.intendedState;
    [self runBeforeGroupFinished:self.finishOp];
}

@end

#endif // OCTAGON
