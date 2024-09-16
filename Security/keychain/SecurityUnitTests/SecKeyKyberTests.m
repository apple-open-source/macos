/*
 * Copyright (c) 2024 Apple Inc. All Rights Reserved.
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

#import <XCTest/XCTest.h>
#import <Security/SecKeyPriv.h>
#import <Security/SecItemPriv.h>
#import <corecrypto/cckem.h>
#import <corecrypto/ccrng.h>
#import <corecrypto/cckyber.h>
#include <libDER/oids.h>


@interface SecKeyKyberTests : XCTestCase
@end

@implementation SecKeyKyberTests

- (void)workerTestGenKeysSizeType:(id)kyberTypeSize {
    NSError *error;
    NSDictionary *params = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeKyber,
        (id)kSecAttrKeySizeInBits: kyberTypeSize,
    };
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(privKey, @"generate key: %@", error);

    NSDictionary *privAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privKey));
    XCTAssertNotNil(privAttrs, @"getting privKey attrs");
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeyType], (id)kSecAttrKeyTypeKyber);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPrivate);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeySizeInBits], kyberTypeSize);

    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    XCTAssertNotNil(pubKey, "getting pubKey from privKey");

    NSDictionary *pubAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)pubKey));
    XCTAssertNotNil(pubAttrs, @"getting pubKey attrs");
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeyType], (id)kSecAttrKeyTypeKyber);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPublic);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeySizeInBits], kyberTypeSize);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrApplicationLabel], pubAttrs[(id)kSecAttrApplicationLabel]);

    NSData *privKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privKey, (void *)&error));
    XCTAssertNotNil(privKeyData);
    params = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeKyber,
        (id)kSecAttrKeySizeInBits: kyberTypeSize,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
    };
    id privKey2 = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privKeyData, (CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(privKey2, @"import privKey from data: %@", error);
    XCTAssertEqualObjects(privKey, privKey2, @"original and reimported privkey are the same");

    NSDictionary *privAttrs2 = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privKey2));
    XCTAssertNotNil(privAttrs2, @"getting privKey attrs");
    XCTAssertEqualObjects(privAttrs2[(id)kSecAttrKeyType], (id)kSecAttrKeyTypeKyber);
    XCTAssertEqualObjects(privAttrs2[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPrivate);
    XCTAssertEqualObjects(privAttrs2[(id)kSecAttrKeySizeInBits], kyberTypeSize);
    XCTAssertEqualObjects(privAttrs, privAttrs2);

    NSData *pubKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)pubKey, (void *)&error));
    XCTAssertNotNil(pubKeyData);
    params = @{ 
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeKyber,
        (id)kSecAttrKeySizeInBits: kyberTypeSize,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
    };
    id pubKey2 = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)pubKeyData, (CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(pubKey2, @"import pubKey from data: %@", error);
    XCTAssertEqualObjects(pubKey, pubKey2, @"original and reimported privkey are the same");
}

- (void)testGenKeys768 {
    [self workerTestGenKeysSizeType:(id)kSecAttrKeySizeKyber768];
}

- (void)testGenKeys1024 {
    [self workerTestGenKeysSizeType:(id)kSecAttrKeySizeKyber1024];
}

- (void)workerTestEncapsulateWithSecKeySizeType:(id)kyberTypeSize info:(const struct cckem_info *)info {
    NSError *error;
    NSDictionary *params = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeKyber,
        (id)kSecAttrKeySizeInBits: kyberTypeSize,
    };
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(privKey, @"generate key: %@", error);
    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((__bridge SecKeyRef)privKey));
    XCTAssertNotNil(pubKey);

    CFDataRef cfEK = NULL;
    NSData *sk = CFBridgingRelease(SecKeyCreateEncapsulatedKey((__bridge SecKeyRef)privKey, kSecKeyAlgorithmKEMKyber, &cfEK, (void *)&error));
    XCTAssertNil(sk, @"unexpectedly succeeded encpsulating using privKey");
    sk = CFBridgingRelease(SecKeyCreateEncapsulatedKey((__bridge SecKeyRef)pubKey, kSecKeyAlgorithmKEMKyber, &cfEK, (void *)&error));
    XCTAssertNotNil(sk);
    NSData *ek = CFBridgingRelease(cfEK);
    XCTAssertNotNil(ek);

    NSData *privKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privKey, (void *)&error));
    XCTAssertNotNil(privKeyData, @"privKey export: %@", error);

    cckem_full_ctx_decl(info, ctx);
    cckem_full_ctx_init(ctx, info);
    size_t privSize = cckem_privkey_nbytes_info(info);
    int err = cckem_import_privkey(info, privSize, privKeyData.bytes + privKeyData.length - privSize, ctx);
    XCTAssertEqual(err, CCERR_OK, @"import privKey into CC");

    NSMutableData *sk2 = [NSMutableData dataWithLength:cckem_shared_key_nbytes_info(info)];
    err = cckem_decapsulate(ctx, ek.length, ek.bytes, sk2.length, sk2.mutableBytes);
    XCTAssertEqual(err, CCERR_OK, @"decapsulate with CC");

    XCTAssertEqualObjects(sk, sk2, @"secret key must be identical on both sides");
}

- (void)testEncapsulateWithSecKey768 {
    [self workerTestEncapsulateWithSecKeySizeType:(id)kSecAttrKeySizeKyber768 info:cckem_kyber768()];
}

- (void)testEncapsulateWithSecKey1024 {
    [self workerTestEncapsulateWithSecKeySizeType:(id)kSecAttrKeySizeKyber1024 info:cckem_kyber1024()];
}

- (void)workerTestDecapsulateWithSecKeySize:(size_t)keySize info:(const struct cckem_info *)info {
    cckem_full_ctx_decl(info, ctx);
    cckem_full_ctx_init(ctx, info);
    int err = cckem_generate_key(ctx, ccrng(NULL));
    XCTAssertEqual(err, CCERR_OK, @"generate key on CC side");

    NSMutableData *sk = [NSMutableData dataWithLength:cckem_shared_key_nbytes_info(info)];
    NSMutableData *ek = [NSMutableData dataWithLength:cckem_encapsulated_key_nbytes_info(info)];
    err = cckem_encapsulate(cckem_public_ctx(ctx), ek.length, ek.mutableBytes, sk.length, sk.mutableBytes, ccrng(NULL));
    XCTAssertEqual(err, CCERR_OK, @"encapsulate on CC side");

    size_t pubSize = cckem_pubkey_nbytes_info(info);
    size_t privSize = cckem_privkey_nbytes_info(info);
    NSMutableData *privKeyData = [NSMutableData dataWithLength:pubSize + privSize];
    err = cckem_export_privkey(ctx, &privSize, privKeyData.mutableBytes + pubSize);
    XCTAssertEqual(err, CCERR_OK, @"export privKey from CC side");

    NSError *error;
    id privKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privKeyData, (CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeKyber,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
    }, (void *)&error));
    XCTAssertNotNil(privKey, @"import CC exported key into SecKey: %@", error);

    NSData *sk2 = CFBridgingRelease(SecKeyCreateDecapsulatedKey((__bridge SecKeyRef)privKey, kSecKeyAlgorithmKEMKyber, (__bridge CFDataRef)ek, (void *)&error));
    XCTAssertNotNil(sk2, @"decapsulate key using SecKey: %@", error);

    XCTAssertEqualObjects(sk, sk2, @"both shared keys must be equal");
}

- (void)testDecapsulateWithSecKey768 {
    [self workerTestDecapsulateWithSecKeySize:768 info:cckem_kyber768()];
}

- (void)testDecapsulateWithSecKey1024 {
    [self workerTestDecapsulateWithSecKeySize:1024 info:cckem_kyber1024()];
}

@end
