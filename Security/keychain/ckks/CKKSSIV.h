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
- (instancetype)copyWithZone:(NSZone*)zone;

// Mostly for testing.
- (instancetype)initWithBase64:(NSString*)base64bytes;
- (BOOL)isEqual:(id)object;
@end

@interface CKKSWrappedAESSIVKey : CKKSBaseAESSIVKey
- (instancetype)initWithData:(NSData*)data;
- (NSData*)wrappedData;
- (NSString*)base64WrappedKey;
@end

@interface CKKSAESSIVKey : CKKSBaseAESSIVKey
+ (instancetype)randomKey;

- (CKKSWrappedAESSIVKey*)wrapAESKey:(CKKSAESSIVKey*)keyToWrap error:(NSError* __autoreleasing*)error;
- (CKKSAESSIVKey*)unwrapAESKey:(CKKSWrappedAESSIVKey*)keyToUnwrap error:(NSError* __autoreleasing*)error;

// Encrypt and decrypt data into buffers. Adds a nonce for ciphertext protection.
- (NSData*)encryptData:(NSData*)plaintext
     authenticatedData:(NSDictionary<NSString*, NSData*>*)ad
                 error:(NSError* __autoreleasing*)error;
- (NSData*)decryptData:(NSData*)ciphertext
     authenticatedData:(NSDictionary<NSString*, NSData*>*)ad
                 error:(NSError* __autoreleasing*)error;

@end

#endif  // OCTAGON
