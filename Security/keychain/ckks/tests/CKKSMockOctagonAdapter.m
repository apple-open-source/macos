/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import <CloudKit/CloudKit.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <SecurityFoundation/SFDigestOperation.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import "keychain/ot/OctagonCKKSPeerAdapter.h"

#import "keychain/ckks/CKKSListenerCollection.h"
#import "keychain/ckks/tests/CKKSMockOctagonAdapter.h"

@implementation CKKSMockOctagonPeer

@synthesize publicSigningKey = _publicSigningKey;
@synthesize publicEncryptionKey = _publicEncryptionKey;


- (instancetype)initWithOctagonPeerID:(NSString*)syncingPeerID
                  publicEncryptionKey:(SFECPublicKey* _Nullable)publicEncryptionKey
                     publicSigningKey:(SFECPublicKey* _Nullable)publicSigningKey
                             viewList:(NSSet<NSString*>* _Nullable)viewList
{
    if((self = [super init])) {
        _peerID = syncingPeerID;
        _publicEncryptionKey = publicEncryptionKey;
        _publicSigningKey = publicSigningKey;
        _viewList = viewList;
    }
    return self;
}

- (bool)matchesPeer:(nonnull id<CKKSPeer>)peer {
    NSString* otherPeerID = peer.peerID;

    if(self.peerID == nil && otherPeerID == nil) {
        return true;
    }

    return [self.peerID isEqualToString:otherPeerID];
}

- (BOOL)shouldHaveView:(nonnull NSString *)viewName {
    return [self.viewList containsObject: viewName];
}

@end


@implementation CKKSMockOctagonAdapter
@synthesize essential = _essential;

- (instancetype)initWithSelfPeer:(OctagonSelfPeer*)selfPeer
                    trustedPeers:(NSSet<id<CKKSRemotePeerProtocol>>*)trustedPeers
                       essential:(BOOL)essential
{
    if((self = [super init])) {
        _essential = essential;

        _peerChangeListeners = [[CKKSListenerCollection alloc] initWithName:@"ckks-mock-sos"];

        _selfOTPeer = selfPeer;
        _trustedPeers = [trustedPeers mutableCopy];
    }
    return self;
}

- (NSString*)providerID
{
    return [NSString stringWithFormat:@"[CKKSMockOctagonAdapter: %@]", self.selfOTPeer.peerID];
}

- (CKKSSelves * _Nullable)fetchSelfPeers:(NSError * _Nullable __autoreleasing * _Nullable)error {
    return [[CKKSSelves alloc] initWithCurrent:self.selfOTPeer allSelves:nil];
}

- (NSSet<id<CKKSRemotePeerProtocol>> * _Nullable)fetchTrustedPeers:(NSError * _Nullable __autoreleasing * _Nullable)error {
    // include the self peer
    CKKSMockOctagonPeer *s = [[CKKSMockOctagonPeer alloc] initWithOctagonPeerID:self.selfOTPeer.peerID
                                                            publicEncryptionKey:self.selfOTPeer.publicEncryptionKey
                                                               publicSigningKey:self.selfOTPeer.publicSigningKey
                                                                       viewList:self.selfViewList];
    return [self.trustedPeers setByAddingObject: s];
}

- (void)registerForPeerChangeUpdates:(nonnull id<CKKSPeerUpdateListener>)listener {
    [self.peerChangeListeners registerListener:listener];
}

- (void)sendSelfPeerChangedUpdate {
    [self.peerChangeListeners iterateListeners: ^(id<CKKSPeerUpdateListener> listener) {
        [listener selfPeerChanged:self];
    }];
}

- (void)sendTrustedPeerSetChangedUpdate {
    [self.peerChangeListeners iterateListeners: ^(id<CKKSPeerUpdateListener> listener) {
        [listener trustedPeerSetChanged:self];
    }];
}

@end

#endif
