//
//  CKKSMockOctagonAdapter.h
//  Security
//
//  Created by Love Hörnquist Åstrand on 6/19/19.
//

#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSListenerCollection.h"
#import "keychain/ot/OctagonCKKSPeerAdapter.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSMockOctagonPeer : NSObject <CKKSPeer, CKKSRemotePeerProtocol>

@property NSString* peerID;
@property (nullable) SFECPublicKey* publicEncryptionKey;
@property (nullable) SFECPublicKey* publicSigningKey;
@property (nullable) NSSet<NSString*>* viewList;

- (instancetype)initWithOctagonPeerID:(NSString*)syncingPeerID
                  publicEncryptionKey:(SFECPublicKey* _Nullable)publicEncryptionKey
                     publicSigningKey:(SFECPublicKey* _Nullable)publicSigningKey
                             viewList:(NSSet<NSString*>* _Nullable)viewList;
@end

@interface CKKSMockOctagonAdapter : NSObject <CKKSPeerProvider>
@property CKKSListenerCollection* peerChangeListeners;
@property OctagonSelfPeer* selfOTPeer;
@property (nullable) NSSet<NSString*>* selfViewList;

@property NSMutableSet<id<CKKSRemotePeerProtocol>>* trustedPeers;
@property (readonly) NSString* providerID;

- (instancetype)initWithSelfPeer:(OctagonSelfPeer*)selfPeer
                    trustedPeers:(NSSet<id<CKKSRemotePeerProtocol>>*)trustedPeers
                       essential:(BOOL)essential;

@end


NS_ASSUME_NONNULL_END
