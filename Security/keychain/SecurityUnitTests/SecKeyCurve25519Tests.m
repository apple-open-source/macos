/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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
 * The Original Cod
 e and all software distributed under the License are
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
#include <corecrypto/ccec.h>
#include <corecrypto/ccec25519.h>
#include <corecrypto/ccec25519_priv.h>
#include <corecrypto/ccsha2.h>
#include <libDER/oids.h>


@interface SecKeyCurve25519Tests : XCTestCase
@end

@implementation SecKeyCurve25519Tests

- (void)verifySignFunctionalityWithPrivateKey:(SecKeyRef)privatekey
                                    publicKey:(SecKeyRef)pubkey // optional
                             invalidPublicKey:(SecKeyRef)invalidpubkey // optional
{
    XCTAssert(privatekey, @"no private key provided");
    NSData *message = [@"message" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *signature = nil;
    NSError *error = nil;
    SecKeyAlgorithm algorithm = NULL;
    int err = -1;

    // sign data
    algorithm = kSecKeyAlgorithmEdDSASignatureMessageCurve25519SHA512;
    signature = CFBridgingRelease(SecKeyCreateSignature(privatekey, algorithm, (CFDataRef)message, (void *)&error));
    XCTAssertNotNil(signature, @"Signing failed: %@", error);
    XCTAssert(signature.length == sizeof(ccec25519signature), @"Invalid signature length: %u", (unsigned)signature.length);

    // copy out public key
    id localPubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privatekey));
    XCTAssert(localPubKey, @"Failed to export pubkey");
    NSData *pubKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)localPubKey, (void *)&error));
    XCTAssert(pubKeyData && pubKeyData.length == sizeof(ccec25519pubkey), @"Failed to export pubkey data: %@", error);
    
    // verify the signature with CoreCrypto
    if (signature) {
        err = cced25519_verify(ccsha512_di(), message.length, message.bytes, signature.bytes, pubKeyData.bytes);
        XCTAssert(err == 0, @"Signature verification failed with CoreCrypto, err: %d", err);
    }
    
    // verify the signature with SecKey
    Boolean valid = SecKeyVerifySignature((SecKeyRef)localPubKey, algorithm, (CFDataRef)message, (CFDataRef)signature, (void*)&error);
    XCTAssertTrue(valid, @"Signature verification failed with SecKey");
    
    NSMutableData *invalidSignature = [signature mutableCopy];
    ((uint8_t*)invalidSignature.mutableBytes)[0] += 1; // make it invalid
    
    // verify the invalid signature with CoreCrypto
    err = cced25519_verify(ccsha512_di(), message.length, message.bytes, invalidSignature.bytes, pubKeyData.bytes);
    XCTAssert(err != 0, @"Signature verification succeeded unexpectedly with CoreCrypto, err: %d", err);
    
    // verify the signature with SecKey
    valid = SecKeyVerifySignature((SecKeyRef)localPubKey, algorithm, (CFDataRef)message, (CFDataRef)invalidSignature, (void*)&error);
    XCTAssertFalse(valid, @"Signature verification succeeded unexpectadly with SecKey");
    
    if (pubkey) {
        // verify the signature with the testing key
        valid = SecKeyVerifySignature(pubkey, algorithm, (CFDataRef)message, (CFDataRef)signature, (void*)&error);
        XCTAssertTrue(valid, @"Signature verification failed with testing SecKey");
    }
    if (invalidpubkey) {
        // verify the signature with invalid testing key
        valid = SecKeyVerifySignature(invalidpubkey, algorithm, (CFDataRef)message, (CFDataRef)signature, (void*)&error);
        XCTAssertFalse(valid, @"Signature verification succeeded unexpectadly with SecKey");
    }
}

-(void)verifyEncryptionFunctionalityWithKey:(id)privateKey algorithm:(SecKeyAlgorithm)algorithm {
    NSError *error = nil;
    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privateKey));
    XCTAssert(publicKey, "public key retrieved");

    XCTAssert(SecKeyIsAlgorithmSupported((SecKeyRef)privateKey, kSecKeyOperationTypeDecrypt, algorithm),
       "%@ supported for decryption", algorithm);
    XCTAssert(SecKeyIsAlgorithmSupported((SecKeyRef)publicKey, kSecKeyOperationTypeEncrypt, algorithm),
       "%@ supported for encryption", algorithm);
    XCTAssert(!SecKeyIsAlgorithmSupported((SecKeyRef)publicKey, kSecKeyOperationTypeDecrypt, algorithm),
       "%@ not supported for decryption - pubkey", algorithm);
    XCTAssert(!SecKeyIsAlgorithmSupported((SecKeyRef)privateKey, kSecKeyOperationTypeEncrypt, algorithm),
       "%@ not supported for encryption - privKey", algorithm);

    NSData *message = [NSData dataWithBytes:"hello" length:5];
    NSDictionary *sharedInfo = @{(id)kSecKeyKeyExchangeParameterSharedInfo :[NSData dataWithBytes:"shared" length:6]};
    error = nil;
    NSData *encrypted = CFBridgingRelease(SecKeyCreateEncryptedData((SecKeyRef)publicKey, algorithm, (CFDataRef)message, (void *)&error));
    NSData *encryptedSI = CFBridgingRelease(SecKeyCreateEncryptedDataWithParameters((__bridge SecKeyRef)publicKey, algorithm, (__bridge CFDataRef)message, (__bridge CFDictionaryRef)sharedInfo, (void *)&error));
    XCTAssert(encrypted, "message encrypted");
    XCTAssert(encryptedSI, "message encrypted");

    error = nil;
    NSData *decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privateKey, algorithm, (CFDataRef)encrypted, (void *)&error));
    XCTAssert(decrypted, "encrypted message decrypted");
    XCTAssert([decrypted isEqual:message], "decrypted message is equal as original one (original:%@ decrypted:%@)", message, decrypted);
    NSData *decryptedSI = CFBridgingRelease(SecKeyCreateDecryptedDataWithParameters((__bridge SecKeyRef)privateKey, algorithm, (__bridge CFDataRef)encryptedSI, (__bridge CFDictionaryRef)sharedInfo, (void *)&error));
    XCTAssert(decryptedSI, "encrypted-with-sharedinfo message decrypted: %@", error);
    XCTAssert([decryptedSI isEqual:message], "decrypted-with-sharedinfo message is equal as original one (original:%@ decrypted:%@)", message, decryptedSI);

    // Modify encrypted message and verify that it cannot be decrypted.
    NSMutableData *badEncrypted = [NSMutableData dataWithData:encrypted];
    UInt8 *badEncryptedBuffer = badEncrypted.mutableBytes;
    badEncryptedBuffer[badEncrypted.length - 8] ^= 0xff;

    error = nil;
    decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privateKey, algorithm, (CFDataRef)badEncrypted, (void *)&error));
    XCTAssert(decrypted == nil, "broken encrypted message failed to decrypt (tag breakage)");

    badEncrypted = [NSMutableData dataWithData:encrypted];
    badEncryptedBuffer = badEncrypted.mutableBytes;
    badEncryptedBuffer[badEncrypted.length - 20] ^= 0xff;

    error = nil;
    decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privateKey, algorithm, (CFDataRef)badEncrypted, (void *)&error));
    XCTAssert(decrypted == nil, "broken encrypted message failed to decrypt (ciphertext breakage)");
    
    badEncrypted = [NSMutableData dataWithData:encrypted];
    badEncryptedBuffer = badEncrypted.mutableBytes;
    badEncryptedBuffer[0] ^= 0xff;

    error = nil;
    decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privateKey, algorithm, (CFDataRef)badEncrypted, (void *)&error));
    XCTAssert(decrypted == nil, "broken encrypted message failed to decrypt (pubkey intro breakage)");

    badEncrypted = [NSMutableData dataWithData:encrypted];
    badEncryptedBuffer = badEncrypted.mutableBytes;
    badEncryptedBuffer[1] ^= 0xff;

    error = nil;
    decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privateKey, algorithm, (CFDataRef)badEncrypted, (void *)&error));
    XCTAssert(decrypted == nil, "broken encrypted message failed to decrypt (pubkey data breakage)");
}

- (void)testInvalidSignParam {
    NSData *signature = nil;
    NSError *error = nil;
    SecKeyAlgorithm algorithm;
    NSData *message = [@"message" dataUsingEncoding:NSUTF8StringEncoding];

    // generate private key
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeEd25519, (id)kSecAttrKeySizeInBits: @256, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssertNotNil(privateKey, @"key generation failed: %@", error);

    // sign data with invalid aglorithm
    algorithm = kSecKeyAlgorithmECDSASignatureMessageX962SHA512;
    signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privateKey, algorithm, (CFDataRef)message, (void *)&error));
    XCTAssertNil(signature, @"Signing succeeded unexpectadly");
    
    // generate private key
    privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeX25519, (id)kSecAttrKeySizeInBits: @256, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssertNotNil(privateKey, @"key generation failed: %@", error);

    // sign data with invalid aglorithm
    algorithm = kSecKeyAlgorithmEdDSASignatureMessageCurve25519SHA512;
    signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privateKey, algorithm, (CFDataRef)message, (void *)&error));
    XCTAssertNil(signature, @"Signing succeeded unexpectadly");
}

- (void)testSignEd25519 {
    NSError *error = nil;
    
    // generate private key
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeEd25519, (id)kSecAttrKeySizeInBits: @256, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssertNotNil(privateKey, @"key generation failed: %@", error);
    
    // verify signing functionality
    [self verifySignFunctionalityWithPrivateKey:(SecKeyRef)privateKey publicKey:NULL invalidPublicKey:NULL];
}

- (void)testExportImportEd25519PublicKey {
    NSError *error = nil;

    // generate private key
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeEd25519, (id)kSecAttrKeySizeInBits: @256, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssertNotNil(privateKey, @"key generation failed: %@", error);
    
    // copy out public key
    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privateKey));
    XCTAssert(pubKey, @"Failed to export pubkey");
    NSData *pubKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)pubKey, (void *)&error));
    XCTAssert(pubKeyData && pubKeyData.length == sizeof(ccec25519pubkey), @"Failed to export pubkey data: %@", error);

    // import public key
    id importedPubKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)pubKeyData, (CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeEd25519, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssert(importedPubKey, @"Failed to import public key: %@", error);
    
    // import invalid public key
    NSMutableData *invalidPubKeyData = [pubKeyData mutableCopy];
    ((uint8_t*)invalidPubKeyData.mutableBytes)[0] += 1; // make it invalid

    id importedInvalidPubKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)invalidPubKeyData, (CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeEd25519, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssert(importedInvalidPubKey, @"Failed to import public key: %@", error);

    // verify signing functionality
    [self verifySignFunctionalityWithPrivateKey:(SecKeyRef)privateKey
                                      publicKey:(SecKeyRef)importedPubKey
                               invalidPublicKey:(SecKeyRef)importedInvalidPubKey];
}

- (void)testExportImportEd25519PrivateKey {
    NSError *error = nil;

    // generate private key
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeEd25519, (id)kSecAttrKeySizeInBits: @256, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssertNotNil(privateKey, @"key generation failed: %@", error);

    // copy out public key
    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privateKey));
    XCTAssert(pubKey, @"Failed to export pubkey");

    // copy out private key
    NSData *privateKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privateKey, (void *)&error));
    XCTAssert(privateKeyData && privateKeyData.length == sizeof(ccec25519secretkey), @"Failed to export private key data: %@", error);
    
    // import private key
    id importedPrivateKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privateKeyData, (CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeEd25519, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssert(importedPrivateKey, @"Failed to import private key: %@", error);
    
    // verify signing functionality with imported private key and original public key
    [self verifySignFunctionalityWithPrivateKey:(SecKeyRef)importedPrivateKey
                                      publicKey:(SecKeyRef)pubKey
                               invalidPublicKey:NULL];
}

- (void)testEncryptX25519 {
    // generate private key
    NSError *error = nil;
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeX25519, (id)kSecAttrKeySizeInBits: @256, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssertNotNil(privateKey, @"key generation failed: %@", error);

    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM];
    
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA224AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA384AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA512AESGCM];
}

- (void)testEncryptEC {
    // generate private key
    NSError *error = nil;
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssertNotNil(privateKey, @"key generation failed: %@", error);

    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM];
    
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA224AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA384AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA512AESGCM];
}

- (void)testSingEd25519CoreCrypto {
    NSData *message = [@"message" dataUsingEncoding:NSUTF8StringEncoding];

    struct ccrng_state *rng = ccrng(NULL);
    const struct ccdigest_info *di = ccsha512_di();
    int err = 0;

    ccec25519secretkey privatekey = {};
    ccec25519pubkey pubkeyTemp = {};

    err = cced25519_make_key_pair(di, rng, pubkeyTemp, privatekey);
    XCTAssertTrue(err == 0, @"cced25519_make_key_pair() failed, err=%d", err);

    ccec25519pubkey pubkey = {};
    err = cced25519_make_pub_with_rng(di, rng, pubkey, privatekey);
    XCTAssertTrue(err == 0, @"Failed to generate public key from private using CoreCrypto, err=%d", err);

    ccec25519signature signature;
    const uint8_t *msg = message.bytes;
    size_t msgLen = message.length;
    
    err = cced25519_sign_with_rng(di, rng, signature, msgLen, msg, pubkey, privatekey);
    XCTAssertTrue(err == 0, @"Failed to sign using CoreCrypto, err=%d", err);
}

- (void)testDHX255519CoreCrypto {
    struct ccrng_state *rng = ccrng(NULL);
    int err = 0;

    ccec25519secretkey privateKey = {};
    ccec25519pubkey pubKey = {};
    err = cccurve25519_make_key_pair(rng, pubKey, privateKey);
    XCTAssert(0 == err, @"cccurve25519_make_key_pair() failed, err = %d", err);

    ccec25519secretkey ephemeralPrivateKey = {};
    ccec25519pubkey ephemeralPubKey = {};
    err = cccurve25519_make_key_pair(rng, ephemeralPubKey, ephemeralPrivateKey);
    XCTAssert(0 == err, @"cccurve25519_make_key_pair() failed, err = %d", err);

    ccec25519key sh1;
    err = cccurve25519_with_rng(rng, sh1, ephemeralPrivateKey, pubKey);
    XCTAssert(0 == err, @"cccurve25519_with_rng() failed, err = %d", err);

    ccec25519key sh2;
    err = cccurve25519_with_rng(rng, sh2, privateKey, ephemeralPubKey);
    XCTAssert(0 == err, @"cccurve25519_with_rng() failed, err = %d", err);

    XCTAssert(0 == memcmp(sh1, sh2, sizeof(sh1)), @"DH secrets do not match");
}

- (void)testSignEd25519CTK {
    NSError *error = nil;
    
    // generate private key
    NSDictionary *params = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeEd25519,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
    };
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(privateKey, @"SEP key generation failed: %@", error);
    
    // verify signing functionality
    [self verifySignFunctionalityWithPrivateKey:(SecKeyRef)privateKey publicKey:NULL invalidPublicKey:NULL];
}

- (void)testEncryptX25519CTK {
    // generate private key
    NSError *error = nil;
    NSDictionary *params = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeX25519,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
    };
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(privateKey, @"SEP key generation failed: %@", error);

    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM];
    
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA224AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA384AESGCM];
    [self verifyEncryptionFunctionalityWithKey:privateKey algorithm:kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA512AESGCM];
}

- (void)testSPKIForEd25519 {
#ifndef LIBDER_HAS_EDDSA
    // guard for rdar://106052612
    XCTSkip();
#endif
    /* Verify decode/encode with externally generated test data */
    const uint8_t _ed25519_spki[] = {
        0x30,0x2A,0x30,0x05,0x06,0x03,0x2B,0x65,0x70,0x03,0x21,0x00,0x0A,0x41,0x0C,0x8F,
        0xE4,0x91,0x2E,0x36,0x52,0xB6,0x1D,0xD2,0x22,0xB1,0xB4,0xD7,0x77,0x32,0x61,0x53,
        0x7D,0x7E,0xBA,0xD5,0x9D,0xF6,0xCD,0x33,0x62,0x2A,0x69,0x3E,
    };
    NSData *expected_spki = [NSData dataWithBytes:_ed25519_spki length:sizeof(_ed25519_spki)];
    id key = CFBridgingRelease(SecKeyCreateFromSubjectPublicKeyInfoData(NULL, (__bridge CFDataRef)expected_spki));
    XCTAssertNotNil(key, @"SecKeyCreate from SPKI failed");

    NSData *spki = CFBridgingRelease(SecKeyCopySubjectPublicKeyInfo((__bridge SecKeyRef)key));
    XCTAssertNotNil(spki, @"SecKeyCopy SPKI failed");
    XCTAssertEqualObjects(spki, expected_spki, @"Failed to generate the same spki");

    /* can we encode/decode against ourselves */
    NSError *error = nil;
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeEd25519, (id)kSecAttrKeySizeInBits: @256, (id)kSecUseDataProtectionKeychain : @YES}, (void *)&error));
    XCTAssertNotNil(privateKey, @"key generation failed: %@", error);

    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((__bridge SecKeyRef)privateKey));
    XCTAssertNotNil(publicKey, @"public key computation failed");

    spki = CFBridgingRelease(SecKeyCopySubjectPublicKeyInfo((__bridge SecKeyRef)publicKey));
    XCTAssertNotNil(spki, @"failed to create spki from public key");

    id decodedKey = CFBridgingRelease(SecKeyCreateFromSubjectPublicKeyInfoData(NULL, (__bridge CFDataRef)spki));
    XCTAssertNotNil(decodedKey, @"failed to create key from spki");
    XCTAssertEqualObjects(publicKey, decodedKey, @"failed to get same public key back from spki");
}

@end
