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

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFEncryptionOperation.h>

typedef NS_ENUM(NSUInteger, SecCKKSTLKShareVersion) {
    SecCKKSTLKShareVersion0 = 0,  // Signature is over all fields except (signature) and (receiverPublicKey)
                                  // Unknown fields in the CKRecord will be appended to the end, in sorted order based on column ID
};

#define SecCKKSTLKShareCurrentVersion SecCKKSTLKShareVersion0


@interface CKKSTLKShare : CKKSCKRecordHolder
@property SFEllipticCurve curve;
@property SecCKKSTLKShareVersion version;

@property NSString* tlkUUID;

@property id<CKKSPeer> receiver;
@property NSString* senderPeerID;

@property NSInteger epoch;
@property NSInteger poisoned;

@property NSData* wrappedTLK;
@property NSData* signature;

-(instancetype)init NS_UNAVAILABLE;

- (CKKSKey*)recoverTLK:(id<CKKSSelfPeer>)recoverer
          trustedPeers:(NSSet<id<CKKSPeer>>*)peers
                 error:(NSError* __autoreleasing *)error;

+ (CKKSTLKShare*)share:(CKKSKey*)key
                    as:(id<CKKSSelfPeer>)sender
                    to:(id<CKKSPeer>)receiver
                 epoch:(NSInteger)epoch
              poisoned:(NSInteger)poisoned
                 error:(NSError* __autoreleasing *)error;

// Database loading
+ (instancetype)fromDatabase:(NSString*)uuid
              receiverPeerID:(NSString*)receiverPeerID
                senderPeerID:(NSString*)senderPeerID
                      zoneID:(CKRecordZoneID*)zoneID
                       error:(NSError * __autoreleasing *)error;
+ (instancetype)tryFromDatabase:(NSString*)uuid
                 receiverPeerID:(NSString*)receiverPeerID
                   senderPeerID:(NSString*)senderPeerID
                         zoneID:(CKRecordZoneID*)zoneID
                          error:(NSError * __autoreleasing *)error;
+ (NSArray<CKKSTLKShare*>*)allFor:(NSString*)receiverPeerID
                          keyUUID:(NSString*)uuid
                           zoneID:(CKRecordZoneID*)zoneID
                            error:(NSError * __autoreleasing *)error;
+ (NSArray<CKKSTLKShare*>*)allForUUID:(NSString*)uuid
                               zoneID:(CKRecordZoneID*)zoneID
                                error:(NSError * __autoreleasing *)error;
+ (NSArray<CKKSTLKShare*>*)allInZone:(CKRecordZoneID*)zoneID
                               error:(NSError * __autoreleasing *)error;
+ (instancetype)tryFromDatabaseFromCKRecordID:(CKRecordID*)recordID
                                        error:(NSError * __autoreleasing *)error;

// Returns a prefix that all every CKKSTLKShare CKRecord will have
+ (NSString*)ckrecordPrefix;

// For tests
- (CKKSKey*)unwrapUsing:(id<CKKSSelfPeer>)localPeer error:(NSError * __autoreleasing *)error;
- (NSData*)signRecord:(SFECKeyPair*)signingKey error:(NSError* __autoreleasing *)error;
- (bool)verifySignature:(NSData*)signature verifyingPeer:(id<CKKSPeer>)peer error:(NSError* __autoreleasing *)error;
- (NSData*)dataForSigning;
@end

#endif // OCTAGON
