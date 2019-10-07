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

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSItem.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSTLKShare.h"

#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFKey.h>

NS_ASSUME_NONNULL_BEGIN

@interface CKKSTLKShareRecord : CKKSCKRecordHolder
@property CKKSTLKShare* share;

// Passthroughs to the underlying share
@property (readonly) NSString* tlkUUID;

@property (readonly) NSString* senderPeerID;

@property (readonly) NSInteger epoch;
@property (readonly) NSInteger poisoned;

@property (readonly, nullable) NSData* wrappedTLK;
@property (readonly, nullable) NSData* signature;

- (instancetype)init NS_UNAVAILABLE;

- (CKKSKey* _Nullable)recoverTLK:(id<CKKSSelfPeer>)recoverer trustedPeers:(NSSet<id<CKKSPeer>>*)peers error:(NSError**)error;

+ (CKKSTLKShareRecord* _Nullable)share:(CKKSKey*)key
                              as:(id<CKKSSelfPeer>)sender
                              to:(id<CKKSPeer>)receiver
                           epoch:(NSInteger)epoch
                        poisoned:(NSInteger)poisoned
                           error:(NSError**)error;

- (bool)signatureVerifiesWithPeerSet:(NSSet<id<CKKSPeer>>*)peerSet error:(NSError**)error;

- (NSData*)dataForSigning;

// Database loading
+ (instancetype _Nullable)fromDatabase:(NSString*)uuid
                        receiverPeerID:(NSString*)receiverPeerID
                          senderPeerID:(NSString*)senderPeerID
                                zoneID:(CKRecordZoneID*)zoneID
                                 error:(NSError* __autoreleasing*)error;
+ (instancetype _Nullable)tryFromDatabase:(NSString*)uuid
                           receiverPeerID:(NSString*)receiverPeerID
                             senderPeerID:(NSString*)senderPeerID
                                   zoneID:(CKRecordZoneID*)zoneID
                                    error:(NSError**)error;
+ (NSArray<CKKSTLKShareRecord*>*)allFor:(NSString*)receiverPeerID
                          keyUUID:(NSString*)uuid
                           zoneID:(CKRecordZoneID*)zoneID
                            error:(NSError* __autoreleasing*)error;
+ (NSArray<CKKSTLKShareRecord*>*)allForUUID:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError**)error;
+ (NSArray<CKKSTLKShareRecord*>*)allInZone:(CKRecordZoneID*)zoneID error:(NSError**)error;
+ (instancetype _Nullable)tryFromDatabaseFromCKRecordID:(CKRecordID*)recordID error:(NSError**)error;

// Returns a prefix that all every CKKSTLKShare CKRecord will have
+ (NSString*)ckrecordPrefix;

// For tests
- (CKKSKey* _Nullable)unwrapUsing:(id<CKKSSelfPeer>)localPeer error:(NSError**)error;
- (NSData* _Nullable)signRecord:(SFECKeyPair*)signingKey error:(NSError**)error;
- (bool)verifySignature:(NSData*)signature verifyingPeer:(id<CKKSPeer>)peer error:(NSError**)error;
@end

NS_ASSUME_NONNULL_END

#endif  // OCTAGON
