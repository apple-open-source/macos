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

@protocol CKKSRemotePeerProtocol <CKKSPeer>
- (BOOL)shouldHaveView:(NSString*)viewName;
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

extern NSString* const CKKSSOSPeerPrefix;

@interface CKKSActualPeer : NSObject <CKKSPeer, CKKSRemotePeerProtocol, NSSecureCoding>
@property (readonly) NSString* peerID;
@property (nullable, readonly) SFECPublicKey* publicEncryptionKey;
@property (nullable, readonly) SFECPublicKey* publicSigningKey;
@property (nullable, readonly) NSSet<NSString*>* viewList;

- (instancetype)initWithPeerID:(NSString*)syncingPeerID
           encryptionPublicKey:(SFECPublicKey* _Nullable)encryptionKey
              signingPublicKey:(SFECPublicKey* _Nullable)signingKey
                      viewList:(NSSet<NSString*>* _Nullable)viewList;
@end

@protocol CKKSSOSPeerProtocol <NSObject, CKKSRemotePeerProtocol>
@end

@interface CKKSSOSPeer : NSObject <CKKSPeer, CKKSSOSPeerProtocol, CKKSRemotePeerProtocol, NSSecureCoding>
- (instancetype)initWithSOSPeerID:(NSString*)syncingPeerID
              encryptionPublicKey:(SFECPublicKey* _Nullable)encryptionKey
                 signingPublicKey:(SFECPublicKey* _Nullable)signingKey
                         viewList:(NSSet<NSString*>* _Nullable)viewList;
@end

@interface CKKSSOSSelfPeer : NSObject <CKKSPeer, CKKSSOSPeerProtocol, CKKSRemotePeerProtocol, CKKSSelfPeer>
@property (readonly) NSString* peerID;
@property (nullable, readonly) NSSet<NSString*>* viewList;

@property (readonly) SFECPublicKey* publicEncryptionKey;
@property (readonly) SFECPublicKey* publicSigningKey;

@property SFECKeyPair* encryptionKey;
@property SFECKeyPair* signingKey;

- (instancetype)initWithSOSPeerID:(NSString*)syncingPeerID
                    encryptionKey:(SFECKeyPair*)encryptionKey
                       signingKey:(SFECKeyPair*)signingKey
                         viewList:(NSSet<NSString*>* _Nullable)viewList;
@end

NSSet<Class>* CKKSPeerClasses(void);

NS_ASSUME_NONNULL_END
#endif  // OCTAGON
