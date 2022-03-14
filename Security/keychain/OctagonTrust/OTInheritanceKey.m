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

#if __OBJC2__

#import "keychain/OctagonTrust/OTInheritanceKey.h"
#import "keychain/OctagonTrust/categories/OTInheritanceKey+Test.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#include <corecrypto/ccaes.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccwrap.h>

NSErrorDomain const OTInheritanceKeyErrorDomain = @"com.apple.security.OctagonTrust.OTInheritanceKey";

@implementation OTInheritanceKey

- (BOOL)generateWrappingWithError:(NSError**)error {
    NSMutableData *wrappingKey = [NSMutableData dataWithLength:32];
    ccrng_generate(ccrng(NULL), wrappingKey.length, wrappingKey.mutableBytes);
    
    size_t wrapped_size = ccwrap_wrapped_size(_recoveryKeyData.length);
    NSMutableData *wrapped = [NSMutableData dataWithLength:wrapped_size];
    const struct ccmode_ecb *aes_ecb = ccaes_ecb_encrypt_mode();
    ccecb_ctx_decl(aes_ecb->size, ecb_ctx);
    int ret = ccecb_init(aes_ecb, ecb_ctx, wrappingKey.length, wrappingKey.bytes);
    if (ret != 0) {
        if (error != nil) {
            *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                         code:OTInheritanceKeyErrorEcbInitFailed
                                  description:[NSString stringWithFormat:@"ccecb_init failed: %d", ret]];
        }
        return NO;
    }

    ret = ccwrap_auth_encrypt(aes_ecb,
                              ecb_ctx,
                              _recoveryKeyData.length,
                              _recoveryKeyData.bytes,
                              &wrapped_size,
                              wrapped.mutableBytes);
    ccecb_ctx_clear(aes_ecb->size, ecb_ctx);
    if (ret != CCERR_OK) {
        if (error != nil) {
            *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                         code:OTInheritanceKeyErrorCCWrapAuthEncrypt
                                  description:[NSString stringWithFormat:@"ccwrap_auth_encrypt: %d", ret]];
        }
        return NO;
    }

    _wrappingKeyData = wrappingKey;
    _wrappingKeyString = [OTInheritanceKey printableWithData:_wrappingKeyData checksumSize:3 error:error];
    if (_wrappingKeyString == nil) {
        return NO;
    }

    _wrappedKeyData = wrapped;
    _wrappedKeyString = [OTInheritanceKey printableWithData:_wrappedKeyData checksumSize:3 error:error];
    if (_wrappedKeyString == nil) {
        return NO;
    }
    return YES;
}

// See SecPasswordGenerate.c
static const unsigned char printableChars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

+ (NSString*_Nullable)base32:(const unsigned char*)d len:(size_t)inlen {
    unsigned char out[8];
    size_t outlen;
    
    if (inlen == 0) {
        return nil;
    } else {
        out[0] = printableChars[d[0] >> 3];
        if (inlen == 1) {
            out[1] = printableChars[(d[0] & 7) << 2];
            outlen = 2;
        } else {
            out[1] = printableChars[((d[0] & 7) << 2) | (d[1] >> 6)];
            out[2] = printableChars[(d[1] >> 1) & 0x1F];
            if (inlen == 2) {
                out[3] = printableChars[(d[1] & 1) << 4];
                outlen = 4;
            } else {
                out[3] = printableChars[((d[1] & 1) << 4) | (d[2] >> 4)];
                if (inlen == 3) {
                    out[4] = printableChars[(d[2] & 0xF) << 1];
                    outlen = 5;
                } else {
                    out[4] = printableChars[((d[2] & 0xF) << 1) | (d[3] >> 7)];
                    out[5] = printableChars[(d[3] >> 2) & 0x1F];
                    if (inlen == 4) {
                        out[6] = printableChars[(d[3] & 3) << 3];
                        outlen = 7;
                    } else if (inlen == 5) {
                        out[6] = printableChars[((d[3] & 3) << 3) | (d[4] >> 5)];
                        out[7] = printableChars[d[4] & 0x1F];
                        outlen = 8;
                    } else {
                        return nil;
                    }
                }
            }
        }
    }
    return [[NSString alloc] initWithBytes:out length:outlen encoding:NSUTF8StringEncoding];
}

static ssize_t alphaIndex(unsigned char c)
{
    const char *p = strchr((const char *)printableChars, c);
    if (p == NULL) {
        return -1;
    } else {
        return (const unsigned char *)p - printableChars;
    }
}

+ (NSData*_Nullable)unbase32:(const unsigned char*)s len:(size_t)inlen {
    unsigned char out[5];
    size_t outlen;
    
    switch (inlen) {
    case 2:
        outlen = 1;
        break;
    case 4:
        outlen = 2;
        break;
    case 5:
        outlen = 3;
        break;
    case 7:
        outlen = 4;
        break;
    case 8:
        outlen = 5;
        break;
    default:
        return nil;
    }

    ssize_t index[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    for (size_t i = 0; i < inlen; ++i) {
        index[i] = alphaIndex(s[i]);
        if (index[i] < 0) {
            return nil;
        }
    }

    out[0] = (index[0] << 3) | (index[1] >> 2);
    if (inlen >= 4) {
        out[1] = (index[1] << 6) | (index[2] << 1) | (index[3] >> 4);
        if (inlen >= 5) {
            out[2] = ((index[3] & 0xF) << 4) | (index[4] >> 1);
            if (inlen >= 7) {
                out[3] = ((index[4] & 1) << 7) | (index[5] << 2) | (index[6] >> 3);
                if (inlen == 8) {
                    out[4] = ((index[6] & 0x7) << 5) | index[7];
                }
            }
        }
    }
    return [NSData dataWithBytes:out length:outlen];
}

+ (NSString* _Nullable)printableWithData:(NSData*)data checksumSize:(size_t)checksumSize error:(NSError**)error {
    NSMutableData *d = [NSMutableData dataWithCapacity:([data length] + checksumSize)];
    uint8_t digest[CCSHA256_OUTPUT_SIZE];
    ccdigest(ccsha256_di(), [data length], [data bytes], digest);
    [d appendData:data];
    if (checksumSize > CCSHA256_OUTPUT_SIZE) {
        if (error != nil) {
            *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                         code:OTInheritanceKeyErrorBadChecksumSize
                                  description:[NSString stringWithFormat:@"checksumSize (%zu) too large (%zu)",
                                                        checksumSize,
                                                        CCSHA256_OUTPUT_SIZE]];
        }
        return nil;
    }
    [d appendData:[NSData dataWithBytes:digest length:checksumSize]];

    const unsigned char *bytes = [d bytes];
    size_t length = [d length];
    NSMutableString *out = [NSMutableString stringWithCapacity:0];
    for (size_t i = 0; i < length; i += 5) {
        size_t chunkLen = MIN(5, length - i);
        const unsigned char *chunk = bytes + i;
        NSString* encodedChunk = [OTInheritanceKey base32:chunk len:chunkLen];
        if (encodedChunk == nil) {
            if (error != nil) {
                *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                             code:OTInheritanceKeyErrorInternal
                                      description:[NSString stringWithFormat:@"bad chunkLen (%zu)", chunkLen]];
            }
            return nil;
        }
        if (encodedChunk.length >= 5) {
            [out appendString:[encodedChunk substringToIndex:4]];
            [out appendString:@"-"];
            [out appendString:[encodedChunk substringFromIndex:4]];
        } else {
            [out appendString:encodedChunk];
        }
        if (i + 5 < length) {
            [out appendString:@"-"];
        }
    }
    return out;
}

+ (NSData* _Nullable)parseBase32:(NSString*)in checksumSize:(size_t)checksumSize error:(NSError**)error {
    NSString *noDashes = [in stringByReplacingOccurrencesOfString:@"-" withString:@""];

    size_t length = [noDashes length];
    NSMutableData *acc = [NSMutableData dataWithLength:0];
    const unsigned char *s = (const unsigned char*)[noDashes UTF8String];

    for (size_t i = 0; i < length; i += 8) {
        size_t chunkSize = MIN(8, length - i);
        NSData *chunk = [OTInheritanceKey unbase32:(s + i) len:chunkSize];
        if (chunk == nil) {
            if (error != nil) {
                *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                             code:OTInheritanceKeyErrorCannotParseBase32
                                      description:[NSString stringWithFormat:@"Cannot parse %.*s",
                                                            (int)chunkSize, s + i]];
            }
            return nil;
        }
        [acc appendData:chunk];
    }
    if ([acc length] < checksumSize) {
        if (error != nil) {
            *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                         code:OTInheritanceKeyErrorBadChecksumSize
                                  description:[NSString stringWithFormat:@"Length (%zu) shorter than checksumsize (%zu)",
                                                        [acc length],
                                                        checksumSize]];
        }
        return nil;
    }
    uint8_t digest[CCSHA256_OUTPUT_SIZE];
    const size_t checksumOff = [acc length] - checksumSize;
    ccdigest(ccsha256_di(), checksumOff, [acc bytes], digest);
    if (checksumSize > CCSHA256_OUTPUT_SIZE) {
        if (error != nil) {
            *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                         code:OTInheritanceKeyErrorBadChecksumSize
                                  description:[NSString stringWithFormat:@"checksumsize (%zu) too long (expected %zu)",
                                                        checksumSize,
                                                        CCSHA256_OUTPUT_SIZE]];
        }
        return nil;
    }
    if (memcmp([acc bytes] + checksumOff, digest, checksumSize) != 0) {
        if (error != nil) {
            *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                         code:OTInheritanceKeyErrorChecksumMismatch
                                  description:[NSString stringWithFormat:@"Checksum does not match"]];
        }
        return nil;
    }
    return [NSData dataWithBytes:[acc bytes] length:checksumOff];
}

- (nullable instancetype)initWithUUID:(NSUUID*)uuid error:(NSError**)error {
    if ((self = [super init]) ) {
        _uuid = uuid;
        unsigned char buf_claim_token[CLAIM_TOKEN_BYTES];
        int ret = SecRandomCopyBytes(kSecRandomDefault, sizeof(buf_claim_token), buf_claim_token);
        if (ret != errSecSuccess) {
            memset_s(buf_claim_token, sizeof(buf_claim_token), 0, sizeof(buf_claim_token));
            if (error != nil) {
                *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                             code:OTInheritanceKeyErrorSecRandom
                                      description:[NSString stringWithFormat:@"SecRandomCopyBytes: %d", ret]];
            }
            return nil;
        }
        _claimTokenData = [NSData dataWithBytes:buf_claim_token length:sizeof(buf_claim_token)];
        memset_s(buf_claim_token, sizeof(buf_claim_token), 0, sizeof(buf_claim_token));
        _claimTokenString = [OTInheritanceKey printableWithData:_claimTokenData checksumSize:4 error:error];
        if (_claimTokenString == nil) {
            return nil;
        }

        unsigned char buf_recovery_key[RECOVERY_KEY_BYTES];
        ret = SecRandomCopyBytes(kSecRandomDefault, sizeof(buf_recovery_key), buf_recovery_key);
        if (ret != errSecSuccess) {
            memset_s(buf_recovery_key, sizeof(buf_recovery_key), 0, sizeof(buf_recovery_key));
            if (error != nil) {
                *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                             code:OTInheritanceKeyErrorSecRandom
                                      description:[NSString stringWithFormat:@"SecRandomCopyBytes: %d", ret]];
            }
            return nil;
        }
        _recoveryKeyData = [NSData dataWithBytes:buf_recovery_key length:sizeof(buf_recovery_key)];
        memset_s(buf_recovery_key, sizeof(buf_recovery_key), 0, sizeof(buf_recovery_key));

        if (![self generateWrappingWithError:error]) {
            return nil;
        }
    }
    return self;
}

- (BOOL)unwrapWithError:(NSError**)error {
    if (_wrappingKeyData.length != 32) {
        if (error != nil) {
            *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                         code:OTInheritanceKeyErrorBadWrappingKeyLength
                                  description:[NSString stringWithFormat:@"wrong wrapping key length: %u",
                                                        (unsigned)_wrappingKeyData.length]];
        }
        return NO;
    }

    if (_wrappedKeyData.length != 72) {
        if (error != nil) {
            *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                         code:OTInheritanceKeyErrorBadWrappedKeyLength
                                  description:[NSString stringWithFormat:@"wrong wrapped key length: %u",
                                                        (unsigned)_wrappedKeyData.length]];
        }
        return NO;
    }

    size_t unwrapped_size = ccwrap_unwrapped_size(_wrappedKeyData.length);
    NSMutableData *unwrapped = [NSMutableData dataWithLength:unwrapped_size];
    const struct ccmode_ecb *aes_ecb = ccaes_ecb_decrypt_mode();
    ccecb_ctx_decl(aes_ecb->size, ecb_ctx);
    int ret = ccecb_init(aes_ecb, ecb_ctx, _wrappingKeyData.length, _wrappingKeyData.bytes);
    if (ret != 0) {
        if (error != nil) {
            *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                         code:OTInheritanceKeyErrorEcbInitFailed
                                  description:[NSString stringWithFormat:@"ccecb_init failed: %d", ret]];
        }
        return NO;
    }

    ret = ccwrap_auth_decrypt(aes_ecb,
                              ecb_ctx,
                              _wrappedKeyData.length,
                              _wrappedKeyData.bytes,
                              &unwrapped_size,
                              unwrapped.mutableBytes);
    ccecb_ctx_clear(aes_ecb->size, ecb_ctx);
    if (ret != CCERR_OK) {
        if (error != nil) {
            *error = [NSError errorWithDomain:OTInheritanceKeyErrorDomain
                                         code:OTInheritanceKeyErrorCCWrapAuthDecrypt
                                  description:[NSString stringWithFormat:@"ccwrap_auth_decrypt: %d", ret]];
        }
        return NO;
    }

    _recoveryKeyData = unwrapped;
    return YES;
}

- (nullable instancetype)initWithWrappedKeyData:(NSData*)wrappedKeyData wrappingKeyData:(NSData*)wrappingKeyData uuid:(NSUUID*)uuid error:(NSError**)error {
    if ((self = [super init])) {
        _uuid = uuid;
        _wrappedKeyData = wrappedKeyData;
        _wrappingKeyData = wrappingKeyData;

        if (![self unwrapWithError:error]) {
            return nil;
        }
        _wrappingKeyString = [OTInheritanceKey printableWithData:_wrappingKeyData checksumSize:3 error:error];
        if (_wrappingKeyString == nil) {
            return nil;
        }
        _wrappedKeyString = [OTInheritanceKey printableWithData:_wrappedKeyData checksumSize:3 error:error];
        if (_wrappedKeyString == nil) {
            return nil;
        }
        _claimTokenData = nil;
        _claimTokenString = nil;
    }
    return self;
}

- (nullable instancetype)initWithWrappedKeyData:(NSData*)wrappedKeyData wrappingKeyString:(NSString*)wrappingKeyString uuid:(NSUUID*)uuid error:(NSError**)error {
    NSData *wrappingKeyData = [OTInheritanceKey parseBase32:wrappingKeyString checksumSize:3 error:error];
    if (wrappingKeyData == nil) {
        return nil;
    }
    return [self initWithWrappedKeyData:wrappedKeyData wrappingKeyData:wrappingKeyData uuid:uuid error:error];
}

- (nullable instancetype)initWithWrappedKeyString:(NSString*)wrappedKeyString wrappingKeyData:(NSData*)wrappingKeyData uuid:(NSUUID*)uuid error:(NSError**)error {
    NSData *wrappedKeyData = [OTInheritanceKey parseBase32:wrappedKeyString checksumSize:3 error:error];
    if (wrappedKeyData == nil) {
        return nil;
    }
    return [self initWithWrappedKeyData:wrappedKeyData wrappingKeyData:wrappingKeyData uuid:uuid error:error];
}

- (BOOL)isEqualToOTInheritanceKey:(OTInheritanceKey *)other {
    if (self == other) {
        return YES;
    }
    return [self.uuid isEqual:other.uuid] &&
        [self.wrappingKeyData isEqualToData:other.wrappingKeyData] &&
        [self.wrappingKeyString isEqualToString:other.wrappingKeyString] &&
        [self.wrappedKeyData isEqualToData:other.wrappedKeyData] &&
        [self.wrappedKeyString isEqualToString:other.wrappedKeyString] &&
        [self.claimTokenData isEqualToData:other.claimTokenData] &&
        [self.claimTokenString isEqualToString:other.claimTokenString] &&
        [self.recoveryKeyData isEqualToData:other.recoveryKeyData];
}

- (BOOL)isEqual:(nullable id)object {
    if (self == object) {
        return YES;
    }
    if (![object isKindOfClass:[OTInheritanceKey class]]) {
        return NO;
    }
    return [self isEqualToOTInheritanceKey:object];
}

- (BOOL)isRecoveryKeyEqual:(OTInheritanceKey*)other {
    if (self == other) {
        return YES;
    }
    return [self.uuid isEqual:other.uuid] &&
        [self.wrappingKeyData isEqualToData:other.wrappingKeyData] &&
        [self.wrappingKeyString isEqualToString:other.wrappingKeyString] &&
        [self.wrappedKeyData isEqualToData:other.wrappedKeyData] &&
        [self.wrappedKeyString isEqualToString:other.wrappedKeyString] &&
        [self.recoveryKeyData isEqualToData:other.recoveryKeyData];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
    if (self = [super init]) {
        _uuid = [coder decodeObjectOfClass:[NSUUID class] forKey:@"uuid"];
        _wrappingKeyData = [coder decodeObjectOfClass:[NSData class] forKey:@"wrappingKeyData"];
        _wrappedKeyData = [coder decodeObjectOfClass:[NSData class] forKey:@"wrappedKeyData"];
        _wrappingKeyString = [coder decodeObjectOfClass:[NSString class] forKey:@"wrappingKeyString"];
        _wrappedKeyString = [coder decodeObjectOfClass:[NSString class] forKey:@"wrappedKeyString"];
        _claimTokenData = [coder decodeObjectOfClass:[NSData class] forKey:@"claimTokenData"];
        _claimTokenString = [coder decodeObjectOfClass:[NSString class] forKey:@"claimTokenString"];
        _recoveryKeyData = [coder decodeObjectOfClass:[NSData class] forKey:@"recoveryKeyData"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
    [coder encodeObject:_uuid forKey:@"uuid"];
    [coder encodeObject:_wrappingKeyData forKey:@"wrappingKeyData"];
    [coder encodeObject:_wrappedKeyData forKey:@"wrappedKeyData"];
    [coder encodeObject:_wrappingKeyString forKey:@"wrappingKeyString"];
    [coder encodeObject:_wrappedKeyString forKey:@"wrappedKeyString"];
    [coder encodeObject:_claimTokenData forKey:@"claimTokenData"];
    [coder encodeObject:_claimTokenString forKey:@"claimTokenString"];
    [coder encodeObject:_recoveryKeyData forKey:@"recoveryKeyData"];
}

@end

#endif /* OBJC2 */
