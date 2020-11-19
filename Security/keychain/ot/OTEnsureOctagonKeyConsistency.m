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
#import "keychain/ot/OTEnsureOctagonKeyConsistency.h"
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

@interface OTEnsureOctagonKeyConsistency ()
@property OTOperationDependencies* deps;

@property NSOperation* finishOp;
@end

@implementation OTEnsureOctagonKeyConsistency
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
    secnotice("octagon-sos", "Beginning ensuring Octagon keys are set properly in SOS");

    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];

    if(!self.deps.sosAdapter.sosEnabled) {
        self.error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorSOSAdapter userInfo:@{NSLocalizedDescriptionKey : @"sos adapter not enabled"}];
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }
    NSError* sosSelfFetchError = nil;
    id<CKKSSelfPeer> sosSelf = [self.deps.sosAdapter currentSOSSelf:&sosSelfFetchError];

    if(!sosSelf || sosSelfFetchError) {
        secnotice("octagon-sos", "Failed to get the current SOS self: %@", sosSelfFetchError);
        self.error = sosSelfFetchError;
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    secnotice("octagon", "Fetched SOS Self! Fetching Octagon Adapter now.");

    NSError* getEgoPeerError = nil;
    NSString* octagonPeerID = [self.deps.stateHolder getEgoPeerID:&getEgoPeerError];
    if(getEgoPeerError) {
        secnotice("octagon", "failed to get peer id: %@", getEgoPeerError);
        self.error = getEgoPeerError;
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    OctagonCKKSPeerAdapter* octagonAdapter = [[OctagonCKKSPeerAdapter alloc] initWithPeerID:octagonPeerID
                                                                              containerName:self.deps.containerName
                                                                                  contextID:self.deps.contextID
                                                                              cuttlefishXPC:self.deps.cuttlefishXPCWrapper];

    secnotice("octagon", "Fetched SOS Self! Fetching Octagon Adapter now: %@", octagonAdapter);

    NSError* fetchSelfPeersError = nil;
    CKKSSelves *selfPeers = [octagonAdapter fetchSelfPeers:&fetchSelfPeersError];

    if((!selfPeers) || fetchSelfPeersError) {
        secnotice("octagon", "failed to retrieve self peers: %@", fetchSelfPeersError);
        self.error = fetchSelfPeersError;
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    id<CKKSSelfPeer> currentSelfPeer = selfPeers.currentSelf;
    if(currentSelfPeer == nil) {
        secnotice("octagon", "failed to retrieve current self");
        self.error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorOctagonAdapter userInfo: @{ NSLocalizedDescriptionKey : @"failed to retrieve current self"}];
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    NSData* octagonSigningKeyData = currentSelfPeer.publicSigningKey.keyData;
    NSData* octagonEncryptionKeyData = currentSelfPeer.publicEncryptionKey.keyData;
    NSData* sosSigningKeyData = sosSelf.publicSigningKey.keyData;
    NSData* sosEncryptionKeyData = sosSelf.publicEncryptionKey.keyData;

    if(![octagonSigningKeyData isEqualToData:sosSigningKeyData] || ![octagonEncryptionKeyData isEqualToData:sosEncryptionKeyData]) {
        secnotice("octagon", "SOS and Octagon signing keys do NOT match! updating SOS");
        NSError* updateError = nil;
        BOOL ret = [self.deps.sosAdapter updateOctagonKeySetWithAccount:currentSelfPeer error:&updateError];
        if(!ret) {
            self.error = updateError;
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }
    } else {
        secnotice("octagon", "SOS and Octagon keys match!");
    }
    self.nextState = self.intendedState;
    [self runBeforeGroupFinished:self.finishOp];
}

@end

#endif // OCTAGON
