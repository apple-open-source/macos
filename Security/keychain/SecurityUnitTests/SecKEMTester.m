/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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

#import "SecKEMTester.h"
#import "SecKeyPriv.h"
#import "SecItemPriv.h"

#import <corecrypto/cckem.h>
#import <corecrypto/cckyber.h>
#import <corecrypto/ccmlkem.h>

#import <XCTest/XCTest.h>

#import <Foundation/Foundation.h>

@implementation SecKEMTesterConfig

- (instancetype)initWithKeyType:(id)keyType
                        keySize:(id)keySize
                      algorithm:(SecKeyAlgorithm)algorithm
                      cckemInfo:(const struct cckem_info *)kemInfo {
    self = [super init];
    if (self) {
        _keyType = keyType;
        _keySize = keySize;
        _algorithm = algorithm;
        _kemInfo = kemInfo;
    }
    return self;
}

+ (instancetype)kyber768Config {
    return [[SecKEMTesterConfig alloc] initWithKeyType:(id)kSecAttrKeyTypeKyber
                                               keySize:(id)kSecAttrKeySizeKyber768
                                             algorithm:kSecKeyAlgorithmKEMKyber
                                             cckemInfo:cckem_kyber768()];
}

+ (instancetype)kyber1024Config {
    return [[SecKEMTesterConfig alloc] initWithKeyType:(id)kSecAttrKeyTypeKyber
                                               keySize:(id)kSecAttrKeySizeKyber1024
                                             algorithm:kSecKeyAlgorithmKEMKyber
                                             cckemInfo:cckem_kyber1024()];
}

+ (instancetype)mlkem768Config {
    return [[SecKEMTesterConfig alloc] initWithKeyType:(id)kSecAttrKeyTypeMLKEM
                                               keySize:(id)kSecAttrKeySizeMLKEM768
                                             algorithm:kSecKeyAlgorithmKEMMLKEM
                                             cckemInfo:cckem_mlkem768()];
}

+ (instancetype)mlkem1024Config {
    return [[SecKEMTesterConfig alloc] initWithKeyType:(id)kSecAttrKeyTypeMLKEM
                                               keySize:(id)kSecAttrKeySizeMLKEM1024
                                             algorithm:kSecKeyAlgorithmKEMMLKEM
                                             cckemInfo:cckem_mlkem1024()];
}

@end

@implementation SecKEMTester

+ (void)verifyKEMTestGenKeysWithConfig:(SecKEMTesterConfig *)config {
    NSError *error;
    NSDictionary *params = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: config.keyType,
        (id)kSecAttrKeySizeInBits: config.keySize,
    };
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(privKey, @"generate key: %@", error);

    NSDictionary *privAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privKey));
    XCTAssertNotNil(privAttrs, @"getting privKey attrs");
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeyType], config.keyType);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPrivate);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeySizeInBits], config.keySize);
    XCTAssertEqual(SecKeyGetBlockSize((SecKeyRef)privKey), cckem_privkey_nbytes_info(config.kemInfo));
    XCTAssertEqual(SecKeyIsAlgorithmSupported((SecKeyRef)privKey, kSecKeyOperationTypeDecapsulate, config.algorithm), YES);

    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    XCTAssertNotNil(pubKey, "getting pubKey from privKey");

    NSDictionary *pubAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)pubKey));
    XCTAssertNotNil(pubAttrs, @"getting pubKey attrs");
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeyType], config.keyType);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPublic);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeySizeInBits], config.keySize);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrApplicationLabel], pubAttrs[(id)kSecAttrApplicationLabel]);
    XCTAssertEqual(SecKeyGetBlockSize((SecKeyRef)pubKey), cckem_pubkey_nbytes_info(config.kemInfo));
    XCTAssertEqual(SecKeyIsAlgorithmSupported((SecKeyRef)pubKey, kSecKeyOperationTypeEncapsulate, config.algorithm), YES);

    NSData *privKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privKey, (void *)&error));
    XCTAssertNotNil(privKeyData);
    params = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: config.keyType,
        (id)kSecAttrKeySizeInBits: config.keySize,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
    };
    id privKey2 = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privKeyData, (CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(privKey2, @"import privKey from data: %@", error);
    XCTAssertEqualObjects(privKey, privKey2, @"original and reimported privkey are the same");

    NSDictionary *privAttrs2 = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privKey2));
    XCTAssertNotNil(privAttrs2, @"getting privKey attrs");
    XCTAssertEqualObjects(privAttrs2[(id)kSecAttrKeyType], config.keyType);
    XCTAssertEqualObjects(privAttrs2[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPrivate);
    XCTAssertEqualObjects(privAttrs2[(id)kSecAttrKeySizeInBits], config.keySize);
    XCTAssertEqualObjects(privAttrs, privAttrs2);

    NSData *pubKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)pubKey, (void *)&error));
    XCTAssertNotNil(pubKeyData);
    params = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: config.keyType,
        (id)kSecAttrKeySizeInBits: config.keySize,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
    };
    id pubKey2 = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)pubKeyData, (CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(pubKey2, @"import pubKey from data: %@", error);
    XCTAssertEqualObjects(pubKey, pubKey2, @"original and reimported privkey are the same");
}

+ (void)verifyKEMTestEncapsulateWithConfig:(SecKEMTesterConfig *)config {
    NSError *error;
    NSDictionary *params = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: config.keyType,
        (id)kSecAttrKeySizeInBits: config.keySize,
    };
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(privKey, @"generate key: %@", error);
    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((__bridge SecKeyRef)privKey));
    XCTAssertNotNil(pubKey);

    CFDataRef cfSharedKey = NULL;

    // Try encapsulation with a private key, expect failure
    NSData *encapsulatedKey = CFBridgingRelease(SecKeyCreateEncapsulatedKey((__bridge SecKeyRef)privKey, config.algorithm, &cfSharedKey, (void *)&error));
    XCTAssertNil(encapsulatedKey, @"unexpectedly succeeded encpsulating using privKey");

    // Try encapsulation with public key, expect success
    encapsulatedKey = CFBridgingRelease(SecKeyCreateEncapsulatedKey((__bridge SecKeyRef)pubKey, config.algorithm, &cfSharedKey, (void *)&error));
    NSData *sharedKey = CFBridgingRelease(cfSharedKey);
    XCTAssertNotNil(sharedKey);
    XCTAssertNotNil(encapsulatedKey);

    NSData *privKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privKey, (void *)&error));
    XCTAssertNotNil(privKeyData, @"privKey export: %@", error);

    cckem_full_ctx_decl(config.kemInfo, ctx);
    cckem_full_ctx_init(ctx, config.kemInfo);
    size_t privSize = cckem_privkey_nbytes_info(config.kemInfo);
    int err = cckem_import_privkey(config.kemInfo, privSize, privKeyData.bytes + privKeyData.length - privSize, ctx);
    XCTAssertEqual(err, CCERR_OK, @"import privKey into CC");

    NSMutableData *sharedKeyFromCC = [NSMutableData dataWithLength:cckem_shared_key_nbytes_info(config.kemInfo)];
    err = cckem_decapsulate(ctx, encapsulatedKey.length, encapsulatedKey.bytes, sharedKeyFromCC.length, sharedKeyFromCC.mutableBytes);
    XCTAssertEqual(err, CCERR_OK, @"decapsulate with CC");

    XCTAssertEqualObjects(sharedKey, sharedKeyFromCC, @"secret key must be identical on both sides");

    id ccExportedPrivKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privKeyData, (CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)config.keyType,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
    }, (void *)&error));
    XCTAssertNotNil(ccExportedPrivKey, @"import CC exported key into SecKey: %@", error);

    NSData *decapsulatedSharedKey = CFBridgingRelease(SecKeyCreateDecapsulatedKey((__bridge SecKeyRef)ccExportedPrivKey, config.algorithm, (__bridge CFDataRef)encapsulatedKey, (void *)&error));

    XCTAssertEqualObjects(decapsulatedSharedKey, sharedKeyFromCC, @"shared key must be identical on both sides");
    XCTAssertEqualObjects(sharedKey, decapsulatedSharedKey, @"shared key must be identical on both sides");
}

+ (void)verifyKEMTestDecapsulateWithConfig:(SecKEMTesterConfig *)config {
    cckem_full_ctx_decl(config.kemInfo, ctx);
    cckem_full_ctx_init(ctx, config.kemInfo);
    int err = cckem_generate_key(ctx, ccrng(NULL));
    XCTAssertEqual(err, CCERR_OK, @"generate key on CC side");

    NSMutableData *sk = [NSMutableData dataWithLength:cckem_shared_key_nbytes_info(config.kemInfo)];
    NSMutableData *ek = [NSMutableData dataWithLength:cckem_encapsulated_key_nbytes_info(config.kemInfo)];
    err = cckem_encapsulate(cckem_public_ctx(ctx),
                            ek.length,
                            ek.mutableBytes,
                            sk.length,
                            sk.mutableBytes,
                            ccrng(NULL));
    XCTAssertEqual(err, CCERR_OK, @"encapsulate on CC side");

    size_t pubSize = cckem_pubkey_nbytes_info(config.kemInfo);
    size_t privSize = cckem_privkey_nbytes_info(config.kemInfo);
    NSMutableData *privKeyData = [NSMutableData dataWithLength:pubSize + privSize];
    err = cckem_export_privkey(ctx, &privSize, privKeyData.mutableBytes + pubSize);
    XCTAssertEqual(err, CCERR_OK, @"export privKey from CC side");

    NSError *error;
    id privKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privKeyData, (CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)config.keyType,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
    }, (void *)&error));
    XCTAssertNotNil(privKey, @"import CC exported key into SecKey: %@", error);

    NSData *sk2 = CFBridgingRelease(SecKeyCreateDecapsulatedKey((__bridge SecKeyRef)privKey, config.algorithm, (__bridge CFDataRef)ek, (void *)&error));
    XCTAssertNotNil(sk2, @"decapsulate key using SecKey: %@", error);

    XCTAssertEqualObjects(sk, sk2, @"both shared keys must be equal");
}

@end
