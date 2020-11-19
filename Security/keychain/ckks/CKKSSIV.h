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

NS_ASSUME_NONNULL_BEGIN

// For AES-SIV 512.

#define CKKSKeySize (512 / 8)
#define CKKSWrappedKeySize (CKKSKeySize + 16)

@interface CKKSBaseAESSIVKey : NSObject <NSCopying>
{
   @package
    uint8_t key[CKKSWrappedKeySize];  // subclasses can use less than the whole buffer, and set key to be precise
    size_t size;
}
- (instancetype)init;
- (instancetype)initWithBytes:(uint8_t*)bytes len:(size_t)len;
- (void)zeroKey;
- (instancetype)copyWithZone:(NSZone* _Nullable)zone;

// Mostly for testing.
- (instancetype)initWithBase64:(NSString*)base64bytes;
- (BOOL)isEqual:(id _Nullable)object;
@end

@interface CKKSWrappedAESSIVKey : CKKSBaseAESSIVKey <NSSecureCoding>
- (instancetype)initWithData:(NSData*)data;
- (NSData*)wrappedData;
- (NSString*)base64WrappedKey;

// Almost certainly not a valid key; use if you need a placeholder
+ (CKKSWrappedAESSIVKey*)zeroedKey;
@end

@interface CKKSAESSIVKey : CKKSBaseAESSIVKey
+ (instancetype _Nullable)randomKey:(NSError*__autoreleasing*)error;

- (CKKSWrappedAESSIVKey* _Nullable)wrapAESKey:(CKKSAESSIVKey*)keyToWrap
                                        error:(NSError* __autoreleasing*)error;

- (CKKSAESSIVKey* _Nullable)unwrapAESKey:(CKKSWrappedAESSIVKey*)keyToUnwrap
                                   error:(NSError* __autoreleasing*)error;

// Encrypt and decrypt data into buffers. Adds a nonce for ciphertext protection.
- (NSData* _Nullable)encryptData:(NSData*)plaintext
               authenticatedData:(NSDictionary<NSString*, NSData*>* _Nullable)ad
                           error:(NSError* __autoreleasing*)error;
- (NSData* _Nullable)decryptData:(NSData*)ciphertext
               authenticatedData:(NSDictionary<NSString*, NSData*>* _Nullable)ad
                           error:(NSError* __autoreleasing*)error;

// Please only call this if you're storing this key to the keychain, or sending it to a peer.
- (NSData*)keyMaterial;

@end

NS_ASSUME_NONNULL_END

#endif  // OCTAGON
