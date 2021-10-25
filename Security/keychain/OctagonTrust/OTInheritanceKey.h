/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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


#ifndef OTInheritanceKey_h
#define OTInheritanceKey_h

#if __OBJC2__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSErrorDomain const OTInheritanceKeyErrorDomain;

typedef NS_ENUM(NSInteger, OTInheritanceKeyErrorCode) {
    OTInheritanceKeyErrorCannotParseBase32 = 1,
    OTInheritanceKeyErrorChecksumMismatch = 2,
    OTInheritanceKeyErrorBadChecksumSize = 3,
    OTInheritanceKeyErrorCCWrapAuthEncrypt = 4,
    OTInheritanceKeyErrorCCWrapAuthDecrypt = 5,
    OTInheritanceKeyErrorInternal = 6,
    OTInheritanceKeyErrorSecRandom = 7,
    OTInheritanceKeyErrorBadWrappingKeyLength = 8,
    OTInheritanceKeyErrorEcbInitFailed = 9,
    OTInheritanceKeyErrorBadWrappedKeyLength = 10,
};

// All the information related to an inheritance key
@interface OTInheritanceKey : NSObject <NSSecureCoding>

- (nullable instancetype)initWithUUID:(NSUUID*)uuid error:(NSError**)error;
- (nullable instancetype)initWithWrappedKeyData:(NSData*)wrappedKeyData wrappingKeyData:(NSData*)wrappingKeyData uuid:(NSUUID*)uuid error:(NSError**)error;
- (nullable instancetype)initWithWrappedKeyData:(NSData*)wrappedKeyData wrappingKeyString:(NSString*)wrappingKeyString uuid:(NSUUID*)uuid error:(NSError**)error;
- (nullable instancetype)initWithWrappedKeyString:(NSString*)wrappedKeyString wrappingKeyData:(NSData*)wrappingKeyData uuid:(NSUUID*)uuid error:(NSError**)error;

- (BOOL)isEqualToOTInheritanceKey:(OTInheritanceKey*)other;
- (BOOL)isEqual:(nullable id)object;

- (BOOL)isRecoveryKeyEqual:(OTInheritanceKey*)other;

@property (strong, nonatomic, readonly) NSUUID *uuid;               // Unique identifier for each inheritance key

@property (strong, nonatomic, readonly) NSData *wrappingKeyData;    // Key to encrypt IK. (stored by beneficiary)
@property (strong, nonatomic, readonly) NSString *wrappingKeyString; // Key to encrypt IK. (stored by beneficiary)

@property (strong, nonatomic, readonly) NSData *wrappedKeyData;     // IK encrypted with wrapping. (part stored at Apple)
@property (strong, nonatomic, readonly) NSString *wrappedKeyString;  // IK encrypted with wrapping. (part stored at Apple)

@property (strong, nonatomic, readonly, nullable) NSData   *claimTokenData;    // claim token used on web/idms (in binary)
@property (strong, nonatomic, readonly, nullable) NSString *claimTokenString;  // claim token used on web/idms (in printable form)

@property (strong, nonatomic, readonly) NSData   *recoveryKeyData;   // recovery key (in binary)

// In version 0:
// recoveryKey is 512 bits / 64 bytes
// wrappingKeyData: 256-bit AES key / 32 bytes
// wrappedKeyData consists of AES-KW (RFC 5649) or recoveryKey with wrappingKey, length 64+8 = 72 bytes

// the _String versions are icloud RK-style base32 (see alphabet in
// OTInheritanceKey) -- with enough checksum digits to round up to a
// multiple of 4 characters.

enum { CLAIM_TOKEN_BYTES = 16 };
enum { RECOVERY_KEY_BYTES = 64 };

@end

NS_ASSUME_NONNULL_END

#endif /* OBJC2 */

#endif /* OTInheritanceKey_h */
