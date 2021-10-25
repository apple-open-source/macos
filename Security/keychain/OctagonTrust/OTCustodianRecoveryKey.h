/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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


#ifndef OTCustodianRecoveryKey_h
#define OTCustodianRecoveryKey_h

#if __OBJC2__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// All the information related to the custodian recovery key.
@interface OTCustodianRecoveryKey : NSObject <NSSecureCoding>

- (nullable instancetype)initWithUUID:(NSUUID*)uuid recoveryString:(NSString*)recoveryString error:(NSError**)error;
- (nullable instancetype)initWithWrappedKey:(NSData*)wrappedKey wrappingKey:(NSData*)wrappingKey uuid:(NSUUID*)uuid error:(NSError**)error;

- (BOOL)isEqualToCustodianRecoveryKey:(OTCustodianRecoveryKey*)other;
- (BOOL)isEqual:(nullable id)object;

@property (strong, nonatomic, readonly) NSUUID *uuid;               // Unique identifier for each CRK
@property (strong, nonatomic, readonly) NSData *wrappingKey;        // Key to encrypt recoveryString -- to create two parts both needed to reassemble.
@property (strong, nonatomic, readonly) NSData *wrappedKey;         // The recoveryString encrypted by wrappingKey (IV + ciphertext) -- in AES GCM.
@property (strong, nonatomic, readonly) NSString *recoveryString;   // random string that is used to derive encryptionKey and signingKey.
@end

NS_ASSUME_NONNULL_END

#endif /* OBJC2 */

#endif /* OTCustodianRecoveryKey_h */
