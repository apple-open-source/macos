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

#import "OTEscrowKeys.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTBottledPeer : NSObject

@property (nonatomic, readonly) NSString* peerID;
@property (nonatomic, readonly) NSString* spID;
@property (nonatomic, readonly) SFECKeyPair* peerSigningKey;
@property (nonatomic, readonly) SFECKeyPair* peerEncryptionKey;
@property (nonatomic, readonly) NSData* data;

// Given a peer's details including private key material, and
// the keys generated from the escrow secret, encrypt the peer private keys,
// make a bottled peer object and serialize it into data.
- (nullable instancetype) initWithPeerID:(NSString * _Nullable)peerID
                                    spID:(NSString * _Nullable)spID
                          peerSigningKey:(SFECKeyPair *)peerSigningKey
                       peerEncryptionKey:(SFECKeyPair *)peerEncryptionKey
                              escrowKeys:(OTEscrowKeys *)escrowKeys
                                   error:(NSError**)error;

// Deserialize a bottle and decrypt the contents (peer keys)
// using the keys generated from the escrow secret.
- (nullable instancetype) initWithData:(NSData *)data
                            escrowKeys:(OTEscrowKeys *)escrowKeys
                                 error:(NSError**)error;

@end
NS_ASSUME_NONNULL_END

#endif
