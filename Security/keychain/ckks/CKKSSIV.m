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

#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>

#import <AssertMacros.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSData_Private.h>

#import "CKKSSIV.h"

#include <CommonCrypto/CommonCrypto.h>
#include <CommonCrypto/CommonRandom.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccmode_siv.h>

#import "keychain/categories/NSError+UsefulConstructors.h"

@implementation CKKSBaseAESSIVKey
- (instancetype)init {
    if(self = [super init]) {
        self->size = CKKSWrappedKeySize;
        [self zeroKey];
    }
    return self;
}
- (instancetype)initWithBytes:(uint8_t *)bytes len:(size_t)len {
    if(self = [super init]) {
        if(len <= CKKSWrappedKeySize) {
            self->size = len;
            memcpy(self->key, bytes, self->size);
        }
    }
    return self;
}
- (instancetype)initWithBase64:(NSString*)base64bytes {
    if(self = [super init]) {
        NSData* data = [[NSData alloc] initWithBase64EncodedString: base64bytes options:0];

        if(!data) {
            return nil;
        }

        if(data.length <= CKKSWrappedKeySize) {
            self->size = data.length;
            memcpy(self->key, data.bytes, self->size);
        }
    }
    return self;
}

- (BOOL)isEqual: (id) object {
    if(![object isKindOfClass:[CKKSBaseAESSIVKey class]]) {
        return NO;
    }

    CKKSBaseAESSIVKey* obj = (CKKSBaseAESSIVKey*) object;
    if (self->size == obj->size && 0 == memcmp(self->key, obj->key, self->size)) {
        return YES;
    } else {
        return NO;
    }
}

- (void)dealloc {
    [self zeroKey];
}

- (void)zeroKey {
    memset_s(self->key, self->size, 0x00, CKKSWrappedKeySize);
}

- (instancetype)copyWithZone:(NSZone *)zone {
    return [[[self class] allocWithZone:zone] initWithBytes:self->key len:self->size];
}

@end

@implementation CKKSWrappedAESSIVKey
- (instancetype)init {
    if(self = [super init]) {
        self->size = CKKSWrappedKeySize;
    }
    return self;
}
- (instancetype)initWithBytes:(uint8_t *)bytes len:(size_t)len {
    if(len != CKKSWrappedKeySize) {
        @throw [NSException
                exceptionWithName:@"WrongKeySizeException"
                reason:[NSString stringWithFormat: @"length (%lu) was not %d", (unsigned long)len, CKKSWrappedKeySize]
                userInfo:nil];
    }
    if(self = [super initWithBytes: bytes len: len]) {
    }
    return self;
}
- (instancetype)initWithBase64:(NSString*)base64bytes {
    if(self = [super initWithBase64: base64bytes]) {
        if(self->size != CKKSWrappedKeySize) {
            @throw [NSException
                    exceptionWithName:@"WrongKeySizeException"
                    reason:[NSString stringWithFormat: @"length (%lu) was not %d", (unsigned long)self->size, CKKSWrappedKeySize]
                    userInfo:nil];
        }
    }
    return self;
}
- (instancetype)initWithData: (NSData*) data {
    if(data.length != CKKSWrappedKeySize) {
        @throw [NSException
                exceptionWithName:@"WrongKeySizeException"
                reason:[NSString stringWithFormat: @"length (%lu) was not %d", (unsigned long) data.length, CKKSWrappedKeySize]
                userInfo:nil];
    }
    if(self = [super initWithBytes: (uint8_t*) data.bytes len: data.length]) {
    }
    return self;
}
- (NSData*) wrappedData {
    return [[NSData alloc] initWithBytes:self->key length:self->size];
}
- (NSString*) base64WrappedKey {
    return [[self wrappedData] base64EncodedStringWithOptions:0];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder {
    [coder encodeBytes:self->key length:self->size forKey:@"wrappedkey"];
}

- (nullable instancetype)initWithCoder:(nonnull NSCoder *)decoder {
    if ((self = [super init])) {
        NSUInteger len = 0;
        const uint8_t * bytes = [decoder decodeBytesForKey:@"wrappedkey" returnedLength:&len];

        if(bytes) {
            memcpy(self->key, bytes, (size_t) len <= CKKSWrappedKeySize ? len : CKKSWrappedKeySize);
        }
    }
    return self;
}

+ (CKKSWrappedAESSIVKey*)zeroedKey
{
    NSData* zeroedData = [NSMutableData dataWithLength:CKKSWrappedKeySize];
    return [[CKKSWrappedAESSIVKey alloc] initWithData:zeroedData];
}

@end

@implementation CKKSAESSIVKey
- (instancetype)init {
    if(self = [super init]) {
        self->size = CKKSKeySize;
    }
    return self;
}
- (instancetype)initWithBytes:(uint8_t *)bytes len:(size_t)len {
    if(len != CKKSKeySize) {
        @throw [NSException
                exceptionWithName:@"WrongKeySizeException"
                reason:[NSString stringWithFormat: @"length (%lu) was not %d", (unsigned long)len, CKKSKeySize]
                userInfo:nil];
    }
    if(self = [super initWithBytes: bytes len: len]) {
    }
    return self;
}
- (instancetype)initWithBase64:(NSString*)base64bytes {
    if(self = [super initWithBase64: base64bytes]) {
        if(self->size != CKKSKeySize) {
            @throw [NSException
                    exceptionWithName:@"WrongKeySizeException"
                    reason:[NSString stringWithFormat: @"length (%lu) was not %d", (unsigned long)self->size, CKKSKeySize]
                    userInfo:nil];
        }
    }
    return self;
}

+ (instancetype _Nullable)randomKey:(NSError* __autoreleasing *)error
{
    CKKSAESSIVKey* key = [[CKKSAESSIVKey alloc] init];

    CCRNGStatus status = CCRandomGenerateBytes(key->key, key->size);
    if(status != kCCSuccess) {
        if(error) {
            *error = [NSError errorWithDomain:@"corecrypto"
                                         code:status
                                  description:[NSString stringWithFormat: @"CCRandomGenerateBytes failed with %d", status]];
        }
        return nil;
    }

    return key;
}

- (CKKSWrappedAESSIVKey*)wrapAESKey: (CKKSAESSIVKey*) keyToWrap error: (NSError * __autoreleasing *) error {
    NSError* localerror = nil;
    bool success = false;

    if(!keyToWrap) {
        localerror = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecParam
                                  description:@"No key given"];
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    CKKSWrappedAESSIVKey* wrappedKey = nil;
    uint8_t buffer[CKKSWrappedKeySize] = {};

    size_t ciphertextLength = ccsiv_ciphertext_size(ccaes_siv_encrypt_mode(), CKKSKeySize);
    require_action_quiet(ciphertextLength == CKKSWrappedKeySize, out, localerror = [NSError errorWithDomain:NSOSStatusErrorDomain
                                                                                                       code:errSecParam
                                                                                                   userInfo:@{NSLocalizedDescriptionKey: @"wrapped key size does not match key size"}]);

    success = [self doSIV: ccaes_siv_encrypt_mode()
                    nonce: nil
                     text: [NSData _newZeroingDataWithBytes:(void*) (keyToWrap->key) length:keyToWrap->size]
                   buffer: buffer
             bufferLength: sizeof(buffer)
        authenticatedData: nil
                    error: error];
    require_quiet(success, out);

    wrappedKey = [[CKKSWrappedAESSIVKey alloc] initWithBytes:buffer len:sizeof(buffer)];
out:
    memset_s(buffer, sizeof(buffer), 0x00, CKKSKeySize);
    if(error && localerror != nil) {
        *error = localerror;
    }
    return wrappedKey;
}

- (CKKSAESSIVKey*)unwrapAESKey: (CKKSWrappedAESSIVKey*) keyToUnwrap error: (NSError * __autoreleasing *) error {
    NSError* localerror = nil;
    bool success = false;

    CKKSAESSIVKey* unwrappedKey = nil;
    uint8_t buffer[CKKSKeySize] = {};

    size_t plaintextLength = ccsiv_plaintext_size(ccaes_siv_decrypt_mode(), CKKSWrappedKeySize);
    require_action_quiet(plaintextLength == CKKSKeySize, out, localerror = [NSError errorWithDomain:NSOSStatusErrorDomain
                                                                                               code:errSecParam
                                                                                           userInfo:@{NSLocalizedDescriptionKey: @"unwrapped key size does not match key size"}]);

    success = [self doSIV: ccaes_siv_decrypt_mode()
                    nonce: nil
                     text: [[NSData alloc] initWithBytesNoCopy: (void*) (keyToUnwrap->key) length: keyToUnwrap->size freeWhenDone: NO]
                   buffer: buffer bufferLength:sizeof(buffer)
        authenticatedData: nil
                    error: error];

    require_quiet(success, out);

    unwrappedKey = [[CKKSAESSIVKey alloc] initWithBytes: buffer len:sizeof(buffer)];
out:
    memset_s(buffer, sizeof(buffer), 0x00, CKKSKeySize);
    if(error && localerror != nil) {
        *error = localerror;
    }
    return unwrappedKey;
}


- (NSData*)encryptData: (NSData*) plaintext authenticatedData: (NSDictionary<NSString*, NSData*>*) ad error: (NSError * __autoreleasing *) error {
    size_t nonceLength = (128/8);
    size_t ciphertextLength = 0;

    NSMutableData* buffer = nil;
    NSMutableData* nonce = nil;

    bool success = false;

    const struct ccmode_siv* mode = ccaes_siv_encrypt_mode();

    // alloc space for nonce and ciphertext.
    nonce = [[NSMutableData alloc] initWithLength: nonceLength];
    CCRNGStatus status = CCRandomGenerateBytes(nonce.mutableBytes, nonce.length);
    if(status != kCCSuccess) {
        if(error) {
            *error = [NSError errorWithDomain:@"CommonCrypto"
                                         code:status
                                     userInfo:@{NSLocalizedDescriptionKey: @"IV generation failed"}];
        }
        return nil;
    }

    ciphertextLength = ccsiv_ciphertext_size(mode, plaintext.length);
    buffer = [[NSMutableData alloc] initWithLength: ciphertextLength];

    success = [self doSIV: mode
                    nonce: nonce
                     text: plaintext
                   buffer: buffer.mutableBytes
             bufferLength: buffer.length
        authenticatedData: ad error: error];

    if(!success) {
        return nil;
    }

    NSMutableData* ret = [[NSMutableData alloc] init];
    [ret appendData: nonce];
    [ret appendData: buffer];
    return ret;
}

- (NSData*)decryptData: (NSData*) ciphertext authenticatedData: (NSDictionary<NSString*, NSData*>*) ad error: (NSError * __autoreleasing *) error {
    size_t nonceLength = (128/8);
    size_t ciphertextLength = 0;
    size_t plaintextLength = 0;

    NSMutableData* plaintext = nil;
    NSData* nonce = nil;
    NSData* text = nil;

    bool success = false;

    const struct ccmode_siv* mode = ccaes_siv_decrypt_mode();

    // compute sizes.
    nonceLength = (128/8);
    if(ciphertext.length <= nonceLength) {
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:4
                                     userInfo:@{NSLocalizedDescriptionKey: @"ciphertext too short"}];
        }
        return nil;
    }

    ciphertextLength = ciphertext.length - (nonceLength);

    // pointer arithmetic. tsk tsk.
    nonce = [[NSData alloc] initWithBytesNoCopy: (void*) ciphertext.bytes length: nonceLength freeWhenDone: NO];
    text = [[NSData alloc] initWithBytesNoCopy: (void*) (ciphertext.bytes + nonceLength) length: ciphertextLength freeWhenDone: NO];

    // alloc space for plaintext
    plaintextLength = ccsiv_plaintext_size(mode, ciphertextLength);
    plaintext = [[NSMutableData alloc] initWithLength: plaintextLength];

    success = [self doSIV: mode
                    nonce: nonce
                     text: text
                   buffer: plaintext.mutableBytes
             bufferLength: plaintext.length
        authenticatedData: ad error: error];

    if(!success) {
        return nil;
    }

    return plaintext;
}

// Does NOT check buffer size. Make sure you get it right for the mode you're requesting!
- (bool)doSIV: (const struct ccmode_siv*) mode nonce: (NSData*) nonce text: (NSData*) text buffer: (uint8_t*) buffer bufferLength: (size_t) bufferLength authenticatedData: (NSDictionary<NSString*, NSData*>*) ad error: (NSError * __autoreleasing *) error {

    NSArray<NSString*>* adKeys = nil;
    NSError* localerror = nil;
    int status = 0;

    ccsiv_ctx_decl(mode->size, ctx);

    require_action_quiet(mode, out, localerror = [NSError errorWithDomain:NSOSStatusErrorDomain
                                                                     code:1
                                                                 userInfo:@{NSLocalizedDescriptionKey: @"no mode given"}]);

    status = ccsiv_init(mode, ctx, self->size, self->key);
    require_action_quiet(status == 0, out, localerror = [NSError errorWithDomain:@"corecrypto"
                                                                            code:status
                                                                        userInfo:@{NSLocalizedDescriptionKey: @"could not ccsiv_init"}]);

    if(nonce) {
        status = ccsiv_set_nonce(mode, ctx, nonce.length, nonce.bytes);
        require_action_quiet(status == 0, out, localerror = [NSError errorWithDomain:@"corecrypto"
                                                                                code:status
                                                                            userInfo:@{NSLocalizedDescriptionKey: @"could not ccsiv_set_nonce"}]);
    }

    // Add authenticated data, sorted by Key Order
    adKeys = [[ad allKeys] sortedArrayUsingSelector:@selector(compare:)];
    for(NSString* adKey in adKeys) {
        NSData* adValue = [ad objectForKey: adKey];
        status = ccsiv_aad(mode, ctx, adValue.length, adValue.bytes);
        require_action_quiet(status == 0, out, localerror = [NSError errorWithDomain:@"corecrypto"
                                                                                code:status
                                                                            userInfo:@{NSLocalizedDescriptionKey: @"could not ccsiv_aad"}]);
    }

    // Actually go.
    status = ccsiv_crypt(mode, ctx, text.length, text.bytes, buffer);
    require_action_quiet(status == 0, out, localerror = [NSError errorWithDomain:@"corecrypto"
                                                                            code:status
                                                                        userInfo:@{NSLocalizedDescriptionKey: @"could not ccsiv_crypt"}]);
out:
    ccsiv_ctx_clear(mode->size, ctx);

    if(error) {
        *error = localerror;
    }
    return localerror == NULL;
}

- (NSData*)keyMaterial
{
    return [NSData _newZeroingDataWithBytes:self->key length:self->size];
}


@end

#endif // OCTAGON
