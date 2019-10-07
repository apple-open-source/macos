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

#include <SecurityFoundation/SFKey.h>
NS_ASSUME_NONNULL_BEGIN

@interface OTIdentity : NSObject

@property (nonatomic, readonly) NSString*     peerID;
@property (nonatomic, readonly) NSString*     spID;
@property (nonatomic, readonly) SFECKeyPair*  peerSigningKey;
@property (nonatomic, readonly) SFECKeyPair*  peerEncryptionKey;


- (instancetype) initWithPeerID:(nullable NSString*)peerID
                           spID:(nullable NSString*)spID
                 peerSigningKey:(SFECKeyPair*)peerSigningKey
              peerEncryptionkey:(SFECKeyPair*)peerEncryptionKey
                          error:(NSError**)error;

+ (nullable instancetype) currentIdentityFromSOS:(NSError**)error;

-(BOOL)isEqual:(OTIdentity* _Nullable)identity;


+(BOOL) storeOctagonIdentityIntoKeychain:(_SFECKeyPair *)restoredSigningKey
                   restoredEncryptionKey:(_SFECKeyPair *)restoredEncryptionKey
                 escrowSigningPubKeyHash:(NSString *)escrowSigningPubKeyHash
                          restoredPeerID:(NSString *)peerID
                                   error:(NSError**)error;

@end

NS_ASSUME_NONNULL_END
#endif

