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
#import "OTBottledPeer.h"
#import "OTBottledPeerRecord.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTBottledPeerSigned : NSObject
@property (nonatomic, readonly) OTBottledPeer*  bp;
@property (nonatomic, readonly) NSData*         signatureUsingEscrowKey;
@property (nonatomic, readonly) NSData*         signatureUsingPeerKey;
@property (nonatomic, readonly) NSData*         escrowSigningSPKI;

- (instancetype) init NS_UNAVAILABLE;

// Create signatures
- (nullable instancetype) initWithBottledPeer:(OTBottledPeer*)bp
                           escrowedSigningKey:(SFECKeyPair *)escrowedSigningKey
                               peerSigningKey:(SFECKeyPair *)peerSigningKey
                                        error:(NSError**)error;

// Verify signatures, or return nil
- (nullable instancetype) initWithBottledPeer:(OTBottledPeer*)bp
                         signatureUsingEscrow:(NSData*)signatureUsingEscrow
                        signatureUsingPeerKey:(NSData*)signatureUsingPeerKey
                        escrowedSigningPubKey:(SFECPublicKey *)escrowedSigningPubKey
                                        error:(NSError**)error;

// Convenience wrapper, verifies signatures
- (nullable instancetype) initWithBottledPeerRecord:(OTBottledPeerRecord *)record
                                         escrowKeys:(OTEscrowKeys *)escrowKeys
                                              error:(NSError**)error;

- (OTBottledPeerRecord *)asRecord:(NSString*)escrowRecordID;
+ (BOOL) verifyBottleSignature:(NSData*)data signature:(NSData*)signature key:(_SFECPublicKey*) pubKey error:(NSError**)error;

@end

NS_ASSUME_NONNULL_END

#endif
