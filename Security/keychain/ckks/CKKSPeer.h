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

#import <Foundation/Foundation.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>

NS_ASSUME_NONNULL_BEGIN

// ==== Peer protocols ====

@protocol CKKSPeer <NSObject>
@property (readonly) NSString* peerID;
@property (nullable, readonly) SFECPublicKey* publicEncryptionKey;
@property (nullable, readonly) SFECPublicKey* publicSigningKey;

// Not exactly isEqual, since this only compares peerID
- (bool)matchesPeer:(id<CKKSPeer>)peer;
@end

@protocol CKKSSelfPeer <CKKSPeer>
@property (readonly) SFECKeyPair* encryptionKey;
@property (readonly) SFECKeyPair* signingKey;
@end

// ==== Peer collection protocols ====

@interface CKKSSelves : NSObject
@property id<CKKSSelfPeer> currentSelf;
@property (nullable) NSSet<id<CKKSSelfPeer>>* allSelves;
- (instancetype)initWithCurrent:(id<CKKSSelfPeer>)selfPeer allSelves:(NSSet<id<CKKSSelfPeer>>* _Nullable)allSelves;
@end

// ==== Peer handler protocols ====

@protocol CKKSPeerUpdateListener;

@protocol CKKSPeerProvider <NSObject>
- (CKKSSelves* _Nullable)fetchSelfPeers:(NSError* _Nullable __autoreleasing* _Nullable)error;
- (NSSet<id<CKKSPeer>>* _Nullable)fetchTrustedPeers:(NSError* _Nullable __autoreleasing* _Nullable)error;
// Trusted peers should include self peers

- (void)registerForPeerChangeUpdates:(id<CKKSPeerUpdateListener>)listener;
@end

// A CKKSPeerUpdateListener wants to be notified when a CKKSPeerProvider has new information
@protocol CKKSPeerUpdateListener <NSObject>
- (void)selfPeerChanged;
- (void)trustedPeerSetChanged;
@end

extern NSString* const CKKSSOSPeerPrefix;

// These should be replaced by Octagon peers, when those exist
@interface CKKSSOSPeer : NSObject <CKKSPeer>
@property (readonly) NSString* peerID;
@property (nullable, readonly) SFECPublicKey* publicEncryptionKey;
@property (nullable, readonly) SFECPublicKey* publicSigningKey;

- (instancetype)initWithSOSPeerID:(NSString*)syncingPeerID
              encryptionPublicKey:(SFECPublicKey* _Nullable)encryptionKey
                 signingPublicKey:(SFECPublicKey* _Nullable)signingKey;
@end

@interface CKKSSOSSelfPeer : NSObject <CKKSPeer, CKKSSelfPeer>
@property (readonly) NSString* peerID;
@property (readonly) SFECPublicKey* publicEncryptionKey;
@property (readonly) SFECPublicKey* publicSigningKey;

@property SFECKeyPair* encryptionKey;
@property SFECKeyPair* signingKey;

- (instancetype)initWithSOSPeerID:(NSString*)syncingPeerID
                    encryptionKey:(SFECKeyPair*)encryptionKey
                       signingKey:(SFECKeyPair*)signingKey;
@end

NS_ASSUME_NONNULL_END
#endif  // OCTAGON
