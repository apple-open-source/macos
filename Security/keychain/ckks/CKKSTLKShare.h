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
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFKey.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainBackedKey.h"
#import "keychain/ckks/CKKSPeer.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, SecCKKSTLKShareVersion) {
    SecCKKSTLKShareVersion0 = 0,  // Signature is over all fields except (signature) and (receiverPublicKey)
    // Unknown fields in the CKRecord will be appended to the end, in sorted order based on column ID
};

#define SecCKKSTLKShareCurrentVersion SecCKKSTLKShareVersion0

// Note that a CKKSTLKShare attempts to be forward-compatible with newly-signed fields
// To use this functionality, pass in a CKRecord to its interfaces. If it has extra data,
// that data will be signed or its signature verified.

@interface CKKSTLKShare : NSObject <NSCopying, NSSecureCoding>
@property SFEllipticCurve curve;
@property SecCKKSTLKShareVersion version;

@property NSString* tlkUUID;

@property NSString* receiverPeerID;
@property NSData* receiverPublicEncryptionKeySPKI;

@property NSString* senderPeerID;

@property NSInteger epoch;
@property NSInteger poisoned;

@property (nullable) NSData* wrappedTLK;
@property (nullable) NSData* signature;

@property CKRecordZoneID* zoneID;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)init:(CKKSKeychainBackedKey*)key
              sender:(id<CKKSSelfPeer>)sender
            receiver:(id<CKKSPeer>)receiver
               curve:(SFEllipticCurve)curve
             version:(SecCKKSTLKShareVersion)version
               epoch:(NSInteger)epoch
            poisoned:(NSInteger)poisoned
              zoneID:(CKRecordZoneID*)zoneID;
- (instancetype)initForKey:(NSString*)tlkUUID
              senderPeerID:(NSString*)senderPeerID
            recieverPeerID:(NSString*)receiverPeerID
  receiverEncPublicKeySPKI:(NSData* _Nullable)publicKeySPKI
                     curve:(SFEllipticCurve)curve
                   version:(SecCKKSTLKShareVersion)version
                     epoch:(NSInteger)epoch
                  poisoned:(NSInteger)poisoned
                wrappedKey:(NSData*)wrappedKey
                 signature:(NSData*)signature
                    zoneID:(CKRecordZoneID*)zoneID;

- (CKKSKeychainBackedKey* _Nullable)recoverTLK:(id<CKKSSelfPeer>)recoverer
                                  trustedPeers:(NSSet<id<CKKSPeer>>*)peers
                                      ckrecord:(CKRecord* _Nullable)ckrecord
                                         error:(NSError* __autoreleasing*)error;

+ (CKKSTLKShare* _Nullable)share:(CKKSKeychainBackedKey*)key
                              as:(id<CKKSSelfPeer>)sender
                              to:(id<CKKSPeer>)receiver
                           epoch:(NSInteger)epoch
                        poisoned:(NSInteger)poisoned
                           error:(NSError**)error;

- (bool)signatureVerifiesWithPeerSet:(NSSet<id<CKKSPeer>>*)peerSet
                            ckrecord:(CKRecord* _Nullable)ckrecord
                               error:(NSError**)error;

// For tests
- (CKKSKeychainBackedKey* _Nullable)unwrapUsing:(id<CKKSSelfPeer>)localPeer
                                          error:(NSError**)error;

- (NSData* _Nullable)signRecord:(SFECKeyPair*)signingKey
                       ckrecord:(CKRecord* _Nullable)ckrecord
                          error:(NSError**)error;

- (bool)verifySignature:(NSData*)signature
          verifyingPeer:(id<CKKSPeer>)peer
               ckrecord:(CKRecord* _Nullable)ckrecord
                  error:(NSError**)error;

// Pass in a CKRecord for forward-compatible signatures
- (NSData*)dataForSigning:(CKRecord* _Nullable)record;
@end

NS_ASSUME_NONNULL_END

#endif  // OCTAGON
