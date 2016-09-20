/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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


#import <Foundation/Foundation.h>
#import <Security/SecItemPriv.h>

#import <corecrypto/ccrng_system.h>
#import <corecrypto/ccsha1.h>
#import <corecrypto/ccsha2.h>
#import <corecrypto/ccec.h>
#import <corecrypto/ccecies.h>

#include "shared_regressions.h"

static struct ccrng_system_state ccrng_system_state_seckey_ies_test;

static void test_ies_run(CFStringRef keyType, id keySize, SecKeyAlgorithm algorithm) {
    NSError *error = nil;
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (__bridge id)keyType, (id)kSecAttrKeySizeInBits: keySize}, (void *)&error));
    ok(privateKey, "key properly generated");
    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privateKey));
    ok(publicKey, "public key retrieved");

    ok(SecKeyIsAlgorithmSupported((SecKeyRef)privateKey, kSecKeyOperationTypeDecrypt, algorithm),
       "%@ supported for decryption", algorithm);
    ok(SecKeyIsAlgorithmSupported((SecKeyRef)publicKey, kSecKeyOperationTypeEncrypt, algorithm),
       "%@ supported for encryption", algorithm);
    ok(!SecKeyIsAlgorithmSupported((SecKeyRef)publicKey, kSecKeyOperationTypeDecrypt, algorithm),
       "%@ not supported for decryption - pubkey", algorithm);
    ok(!SecKeyIsAlgorithmSupported((SecKeyRef)privateKey, kSecKeyOperationTypeEncrypt, algorithm),
       "%@ not supported for encryption - privKey", algorithm);

    NSData *message = [NSData dataWithBytes:"hello" length:5];
    error = nil;
    NSData *encrypted = CFBridgingRelease(SecKeyCreateEncryptedData((SecKeyRef)publicKey, algorithm, (CFDataRef)message, (void *)&error));
    ok(encrypted, "message encrypted");

    error = nil;
    NSData *decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privateKey, algorithm, (CFDataRef)encrypted, (void *)&error));
    ok(decrypted, "encrypted message decrypted");
    ok([decrypted isEqual:message], "decrypted message is equal as original one (original:%@ decrypted:%@)", message, decrypted);

    // Modify encrypted message and verify that it cannot be decrypted.
    NSMutableData *badEncrypted = [NSMutableData dataWithData:encrypted];
    UInt8 *badEncryptedBuffer = badEncrypted.mutableBytes;
    badEncryptedBuffer[badEncrypted.length - 8] ^= 0xff;

    error = nil;
    decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privateKey, algorithm, (CFDataRef)badEncrypted, (void *)&error));
    ok(decrypted == nil, "broken encrypted message failed to decrypt (tag breakage)");

    badEncrypted = [NSMutableData dataWithData:encrypted];
    badEncryptedBuffer = badEncrypted.mutableBytes;
    badEncryptedBuffer[badEncrypted.length - 20] ^= 0xff;

    error = nil;
    decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privateKey, algorithm, (CFDataRef)badEncrypted, (void *)&error));
    ok(decrypted == nil, "broken encrypted message failed to decrypt (ciphertext breakage)");
    
    badEncrypted = [NSMutableData dataWithData:encrypted];
    badEncryptedBuffer = badEncrypted.mutableBytes;
    badEncryptedBuffer[0] ^= 0xff;

    error = nil;
    decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privateKey, algorithm, (CFDataRef)badEncrypted, (void *)&error));
    ok(decrypted == nil, "broken encrypted message failed to decrypt (pubkey intro breakage)");

    badEncrypted = [NSMutableData dataWithData:encrypted];
    badEncryptedBuffer = badEncrypted.mutableBytes;
    badEncryptedBuffer[1] ^= 0xff;

    error = nil;
    decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privateKey, algorithm, (CFDataRef)badEncrypted, (void *)&error));
    ok(decrypted == nil, "broken encrypted message failed to decrypt (pubkey data breakage)");
}
static const int TestCountIESRun = 13;

static void test_ecies() {
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(192), kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(192), kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(192), kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(192), kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(192), kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM);

    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(256), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA1AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(256), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA224AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(256), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(256), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA384AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(256), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA512AESGCM);

    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(521), kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(521), kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(521), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(521), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA384AESGCM);
    test_ies_run(kSecAttrKeyTypeECSECPrimeRandom, @(521), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA512AESGCM);
}
static const int TestCountECIES = TestCountIESRun * 5 * 3;

static void test_rsawrap() {
    test_ies_run(kSecAttrKeyTypeRSA, @(1024), kSecKeyAlgorithmRSAEncryptionOAEPSHA1AESGCM);
    test_ies_run(kSecAttrKeyTypeRSA, @(1024), kSecKeyAlgorithmRSAEncryptionOAEPSHA224AESGCM);
    test_ies_run(kSecAttrKeyTypeRSA, @(1024), kSecKeyAlgorithmRSAEncryptionOAEPSHA256AESGCM);
    test_ies_run(kSecAttrKeyTypeRSA, @(1024), kSecKeyAlgorithmRSAEncryptionOAEPSHA384AESGCM);
    test_ies_run(kSecAttrKeyTypeRSA, @(4096), kSecKeyAlgorithmRSAEncryptionOAEPSHA512AESGCM);
}
static const int TestCountRSAWRAP = TestCountIESRun * 5;

static void test_ies_against_corecrypto(id keySize, ccec_const_cp_t cp, const struct ccdigest_info *di, uint32_t ccKeySize, SecKeyAlgorithm algorithm) {
    // Generate SecKey and import it as corecrypto fullkey.
    NSError *error = nil;
    NSDictionary *params = @{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: keySize,
                             (id)kSecAttrNoLegacy: @YES};
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    ok(privKey != nil, "create key (error %@)", error);
    error = nil;
    NSData *privKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privKey, (void *)&error));
    ok(privKey != nil, "export key (error %@)", error);
    ccec_full_ctx_decl_cp(cp, fullkey);
    ok(ccec_x963_import_priv(cp, privKeyData.length, privKeyData.bytes, fullkey) == 0, "error importing cc ec key");

    // SecKey encrypt -> cc decrypt.
    static const UInt8 knownPlaintext[] = "KNOWN PLAINTEXT";
    NSData *plaintext = [NSData dataWithBytes:knownPlaintext length:sizeof(knownPlaintext)];
    error = nil;
    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    NSData *ciphertext = CFBridgingRelease(SecKeyCreateEncryptedData((SecKeyRef)publicKey, algorithm, (CFDataRef)plaintext, (void *)&error));
    ok(ciphertext != nil, "encrypt data with SecKey (error %@)", error);
    struct ccecies_gcm ecies_dec;
    ccecies_decrypt_gcm_setup(&ecies_dec, di, ccaes_gcm_decrypt_mode(), ccKeySize, 16, ECIES_EPH_PUBKEY_IN_SHAREDINFO1 | ECIES_EXPORT_PUB_STANDARD);
    size_t decryptedLength = plaintext.length;
    NSMutableData *decrypted = [NSMutableData dataWithLength:decryptedLength];
    if (ciphertext != nil) {
        ok(ccecies_decrypt_gcm(fullkey, &ecies_dec, ciphertext.length, ciphertext.bytes, 0, NULL, 0, NULL,
                               &decryptedLength, decrypted.mutableBytes) == 0, "decrypt data with cc failed");
    }
    ok(decryptedLength = plaintext.length);
    ok([plaintext isEqualToData:decrypted], "cc decrypted data are the same");

    // cc encrypt -> SecKey decrypt
    struct ccecies_gcm ecies_enc;
    ccecies_encrypt_gcm_setup(&ecies_enc, di, (struct ccrng_state *)&ccrng_system_state_seckey_ies_test,
                              ccaes_gcm_encrypt_mode(), ccKeySize, 16, ECIES_EPH_PUBKEY_IN_SHAREDINFO1 | ECIES_EXPORT_PUB_STANDARD);
    size_t encryptedLength = ccecies_encrypt_gcm_ciphertext_size(fullkey, &ecies_enc, sizeof(knownPlaintext));
    NSMutableData *encrypted = [NSMutableData dataWithLength:encryptedLength];
    ok(ccecies_encrypt_gcm(fullkey, &ecies_enc, sizeof(knownPlaintext), knownPlaintext, 0, NULL, 0, NULL, &encryptedLength, encrypted.mutableBytes) == 0, "encrypt data with cc failed");
    error = nil;
    NSData *decryptedPlaintext = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privKey, algorithm, (CFDataRef)encrypted, (void *)&error));
    ok(decryptedPlaintext != nil, "decrypt data with SecKey (error %@)", error);
    ok([plaintext isEqualToData:decryptedPlaintext], "SecKey decrypted data are the same");
}
static const int TestCountIESAgainstCoreCryptoRun = 10;

static void test_against_corecrypto() {

    // Tests ifdefed-out by this define fail because of
    // <rdar://problem/26855537> ccecies is broken when AES keysize > KDF hash output size
#define CC_HAS_BUG_26855537 1

    test_ies_against_corecrypto(@(192), ccec_cp_192(), ccsha1_di(), 16, kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM);
    test_ies_against_corecrypto(@(192), ccec_cp_192(), ccsha224_di(), 16, kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM);
    test_ies_against_corecrypto(@(192), ccec_cp_192(), ccsha256_di(), 16, kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM);
    test_ies_against_corecrypto(@(192), ccec_cp_192(), ccsha384_di(), 16, kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM);
    test_ies_against_corecrypto(@(192), ccec_cp_192(), ccsha512_di(), 16, kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM);
    test_ies_against_corecrypto(@(256), ccec_cp_256(), ccsha1_di(), 16, kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM);
    test_ies_against_corecrypto(@(256), ccec_cp_256(), ccsha224_di(), 16, kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM);
    test_ies_against_corecrypto(@(256), ccec_cp_256(), ccsha256_di(), 16, kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM);
    test_ies_against_corecrypto(@(256), ccec_cp_256(), ccsha384_di(), 16, kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM);
    test_ies_against_corecrypto(@(256), ccec_cp_256(), ccsha512_di(), 16, kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM);
#if !CC_HAS_BUG_26855537
    test_ies_against_corecrypto(@(384), ccec_cp_384(), ccsha1_di(), 32, kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM);
    test_ies_against_corecrypto(@(384), ccec_cp_384(), ccsha224_di(), 32, kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM);
#endif
    test_ies_against_corecrypto(@(384), ccec_cp_384(), ccsha256_di(), 32, kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM);
    test_ies_against_corecrypto(@(384), ccec_cp_384(), ccsha384_di(), 32, kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM);
    test_ies_against_corecrypto(@(384), ccec_cp_384(), ccsha512_di(), 32, kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM);
#if !CC_HAS_BUG_26855537
    test_ies_against_corecrypto(@(521), ccec_cp_521(), ccsha1_di(), 32, kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM);
    test_ies_against_corecrypto(@(521), ccec_cp_521(), ccsha224_di(), 32, kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM);
#endif
    test_ies_against_corecrypto(@(521), ccec_cp_521(), ccsha256_di(), 32, kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM);
    test_ies_against_corecrypto(@(521), ccec_cp_521(), ccsha384_di(), 32, kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM);
    test_ies_against_corecrypto(@(521), ccec_cp_521(), ccsha512_di(), 32, kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM);
}
#if !CC_HAS_BUG_26855537
static const int TestCountAgainstCoreCrypto = TestCountIESAgainstCoreCryptoRun * 20;
#else
static const int TestCountAgainstCoreCrypto = TestCountIESAgainstCoreCryptoRun * 16;
#endif

static void test_ies_known_ciphertext(CFStringRef keyType, id keySize, SecKeyAlgorithm algorithm, NSString *privKeyData, NSString *ciphertext) {
#define GENERATE_VECTORS 0

    NSError *error = nil;
#if GENERATE_VECTORS
    NSDictionary *params = @{(id)kSecAttrKeyType: (__bridge id)keyType, (id)kSecAttrKeySizeInBits: keySize, (id)kSecAttrNoLegacy: @YES};
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    ok(privKey != nil, "generate key (error %@)", error);
#else
    NSDictionary *params = @{(id)kSecAttrKeyType: (__bridge id)keyType, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate};
    id privKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)[[NSData alloc] initWithBase64EncodedString:privKeyData options:0], (CFDictionaryRef)params, (void *)&error));
    ok(privKey != nil, "import key (error %@)", error);
#endif

    static const UInt8 knownPlaintext[] = "KNOWN PLAINTEXT";
    NSData *plaintext = [NSData dataWithBytes:knownPlaintext length:sizeof(knownPlaintext)];
    error = nil;
#if GENERATE_VECTORS
    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    NSData *ciphertextData = CFBridgingRelease(SecKeyCreateEncryptedData((SecKeyRef)publicKey, algorithm, (CFDataRef)plaintext, (void *)&error));
    error = nil;
    NSData *keyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privKey, (void *)&error));
    printf("\n@\"%s\", @\"%s\");\n",
           [keyData base64EncodedStringWithOptions:0].UTF8String,
           [ciphertextData base64EncodedStringWithOptions:0].UTF8String);
#else
    NSData *ciphertextData = [[NSData alloc] initWithBase64EncodedString:ciphertext options:0];
    NSData *decrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privKey, algorithm, (CFDataRef)ciphertextData, (void *)&error));
    ok(decrypted != nil, "decrypt known ciphertext (error %@)", error);
    ok([decrypted isEqual:plaintext], "known ciphertext decrypts to known plaintext (%@: %@)", plaintext, decrypted);
#endif
}
static const int TestCountTestIESKnownCipherText = 3;

static void test_known_ciphertext() {
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(192), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA1AESGCM, @"BJID1fgBoH7L1eHwBcJmZcP4oeuV0xDkeEEO3RdodExpqy5UQWf64FIfRRRbZKSA9cATBLVT0HIl2HXcDMYoX0kE9l8fA/G18w==", @"BMUM+ykbSAfK3zVjoqMpruH00UJWvtC05GITlPuqByGu7loVSl3LdOw8bmkHQB+yx6CdBuq2LGao1BqGP7rNYQRiVDrdA/klICsCm/HADu6a");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(192), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA224AESGCM, @"BGvXnhpVlguy834KfbQUn/HHJVz1mE2q1TOI/c5VdryxFdTSIHzO1OntV7rHVqoKhMXvgmtE88aKr0E6dDESlTmGUXlXvD7AoQ==", @"BL8xaAaQBe3+4GR8e6BO+wupb4dNAr6NgjdqH7C1p55LKhJyEP2QQJo1YHXadXpqdSvh9wWBWiz4NDgMeQx8F5Vst6FWKDuzgQKY1QS8Zp0z");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(192), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM, @"BPyRwvWxZr+tJsxRigfSQflyr1sq+CqUsh68E2JqNK33ud5J34OdI0dGSWe7/i4JF7l0a8piYVr4pkH2pTIPtYmwFAOGGtrzyA==", @"BAPIogml3kN6qJ7Lqdmah+C9LtxnGQ+npz1dj1wqf8UmCRScyjvB3Ir87vGLpOJTWNb7fWth+9FieyAyn9AtK9uRE+JH9x2mu2dmfVq+q48z");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(192), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA384AESGCM, @"BBXZUianpQozaYCBT4DzWfhg/+UqvW1BS4i17RIVvXZuYhXhfuYpCdgwfVc7NqJretuT/Bti6noQ6g1P+yj7BJF+i9pPwSnj5Q==", @"BEPRcNwrzSQ8fGzdQlhKaLmfUcuosUkZGfdta/cwTOpoREIrn/ax4BoNpNibuGVhgUMdWNMIg9Z519o0v/VZFKtRk++9cFQSDjE1SIw3NyOy");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(192), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA512AESGCM, @"BETnUivZ4Qg+x7F8mQZaAyh7rmb15O49EKwBIxodfx1BykjVXWBqC64odYkWcOUc3w2LZ/9/klr0VGvF9UC+5w9fKufyMVQdvg==", @"BEj8oriYbbcfX+y/RW+aHp47LMSChJy3TY5GU2Ld4vMP6M8y4AyB7mIZTLJLYQPOoMam/CGqpEAUhtJlhYxugQMzqJHr9ySEsgp89vsgM/X+");

    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(256), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA1AESGCM, @"BOAqiwAFzjxO7xaYE/6+ywCRap/xxiBCxuUj4A0kzSQtu+0BJ0R71djlM+wtYPwff3+g7e52a8cQSioBciTrMjWCTZuTbhHqsMOdqsmKN/pLEwK9uI3FA6grFbSODA1u0g==", @"BIqvCZdbwbSmS4+hB8vbDdIDJxaUGBmcaOPoRpaTY8MWofTlcqc/OGB0AjbuzpJ7cqODphJ+B6LlsrK0SPogUyj7zT01PA9gwWau/BXb8/1JQa7zTeIJ1vsF7gSrTcMWxA==");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(256), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA224AESGCM, @"BPYROmZkeW3Zc9Qc8TzTZ27YO9+IGHjj78DNmZ3gwNjZtEW+knhI3Z3G/UNwaI3jCgezA3ZyPgIfizUTOqe4fYRIwy867Zn8BHpSxjzEG7Sk3zu/CgA7ObNVJQ6dJWu4PQ==", @"BCiu9fyMXqWXojfd0XWynB9CQwUSNedfdn1AC7OlrZaqH9IyUrgcyajdFaRCksBWt6lmUQx4Ypv48mOqvBcPXpX1TAr4NHkHK4cWXbBzaxesrYn3zOlUhZYvzB5WnWTorg==");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(256), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM, @"BINzTFiGIp6G4fpmFCP/KHot+ek+PUYvYp69iT8lHdJNwRbJir1Zr4i73A0vdKoKMn3jdp/8oQ5WnSE+PsFruYahsHI2tFyhPjGYEhUmFrwaA/2WQynh77dAqSiKhg0l3w==", @"BE7IWFWccaIl7n7Kffg5Q+vkJMJFVsHP8C5C/APzW6gjUQynJGRoG4Vw4UEsFTi/yfUQYTSV7Vv18CXE/3IecGAILuLpcDaSxSwzFqrwmqCcSqSRbYSat+8HuaKq19Cq4A==");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(256), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA384AESGCM, @"BE73i2R6RS7M83M2JGD+JKCR1Z8AST87PFRNLLIfIXoKhH6BFsVgUmuOhz+o450teB4rig57yn9nzDkBRTTn+QAp3g6zGzIE1CBuxSSWVcJNiUD8APCb6kJj7Y/95ekTHg==", @"BKl/2vl0v3BqnLYTMvjszf2hB48GwIrSqqBQU01p/RsAPk3zPb1QG1nq0/bL5mFyIbsQUq8jhAvx8YbXDG47e6qv44Q2nalZqvry55daTUyVvrHvZ+xM6qceFzLFUHYGLw==");

    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(256), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA512AESGCM, @"BMnc5IlDIZJKns6G8ALvQqnaprPuhMkNy04G7rZmOD5w7O2OlViw+aDZr667+xIoQ/zqpMBLIA8ujun6mMQdgx2Wvtj3LPhvfbnHQnuTM60RDfA2BQ+Xj4WCQ4KzcaUElw==", @"BP997mVpTGiPFJp+2CKm2cWgAg/bUToX9UcJlGuasGxFdDqb6a3WZ0HYyujiwxihdSUOEcAdExU3RJdWTfFHDeXDAB7Qdf4WdjiWfyPO29kSWAWbJnHaC9JKC4UdTjTPBA==");


    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(521), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA1AESGCM, @"BAD3+GLa92qTtnxZjAYCnkX+LqlwrDKD+jMvq8JYYNFk3rYBT7QKs8ddleTb1ScK732C/NtNTS6yBuqh1/GLLIH0+QCE4D+B3tk+gpgx7yc0Yy+RMTxPpdfX8h4holqwo81TlvLaYpriEmD+8fSFNJuOzc30COFJhpEa70aRkBOtXJbd0AFbq3fvwbC1epWVeTq5d73ZEuiFv5CJ6NIxBsdBFy0RvFm0I+hgqUXyk4gILgsNP6WdWQc1SFqHh6PvbmXO5f6Jzw==", @"BABKMqx3GstC3HtOo2FyOJ1TP0/v+CxRcKawwESkC2SQ7bvhkJGClfPpO57ZIUBULNzY04d+ZmrZ6JX9mp/qB2DtSAA9WGEm/dAawdzOrwwPbD55fi/BaUhP7HF/2x/5ST4WyU5oFTGc3c2vvTnHLLEKKUk13d9ybl8inWPWlNZolb5NNna7MrzX45H8IcRljg6xWntSVqzeglzB4BkRrmjqVbeJ");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(521), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA224AESGCM, @"BAAKStHnPj7CcT+M0wIAsCR89AzaD5h5Mvn8fMPgdqyWZjPuHOL1QaInfSytINx/w88AXLmdEVZucY/R1QB8Gj+5KQFvVYcxFH6rfyHiel00khFdiV0hiNKDQIgGOj3EtkMW2SBnawj2E9ku3m6AhR6p9XcnrwiORhqujH5VQsohJkWMCABNScNDTe19wGskuup11oTKnDNIucSrpggRJsldGymnyV4O74vFsl+8Kq3kfxpVKClU39tqAtC9hxduBRolHwfscg==", @"BAEPiWaky7NoDWFoNRot4xVsxxWP64hbwLM9xyIvYDWyQjXRXHaP4weUYV0n8Cw5aUzvDyON4ik9ouRlLId0SX8SYQCGvopjiBWyA9ieu0LKLxYvMYJCrZUshXmmpHdB25gVZ8vv9sZyE5ggA0IXVl7IxMJTLirbIcfO8EvMPAao/3z0rAx09Yht2iITOoNupE/gSbg/UY3ijuhccFd0bBaLlj9u");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(521), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM, @"BAHaS9E1bgFRlYB555Or19pwj9wOdMP1ETONjgS8MQ4NiOmkOaus4HMcqpiTfCruIAIjBJ0jfuM8j2TdTR8GdPEg3QHHXOav+tPlhfj8y+E/6dNAz0jLHO16wjcGjpq0MJcDMlHS7SKPh8j/GD2KwNya9LefD+VAM62yqRuaFFJihpBV5QHbLDN1AIjqhQO0nm2fRgW0Hff7EcFJXNHkl7ULby2sViPZx1+qN+LcAt4BoZVayiQVxIWX+p2l6nesBcChF+2Fvw==", @"BAFJqyEg7XOScWdtbgVTvmzsYbunFKhiDMJINwPp5tq0QSOiSkh7Cz91+SkUrlCN7Um6bPJmpRjksd9ThR2GlWiiPACFnhhcLmSlLkS1vG3H6kutOJLISBvru05qzXqhDAoda4lDh6jrqppAGUkOgk6lwSJq9w023fmxCxpPtth41Vfq7U2hxik8N81ZdeqiYALerJdXhGVmv8YVaZis2hdzGHLc");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(521), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA384AESGCM, @"BAHR8zVCUzKJi3yc7QJCpxIdsVUJ+N2tB9P0CnMWZ9gVt6bO+UCrGsS6pon0f+nERY+RlPfTyTlrBBGp24a2wWhs2wFaZf2Ng1eKcRFaXPgJozAdz+vOGy1UVK3PxhDkhusdDGyDu6d72UNAicyESa6OCl+f9uicZA5ANaDb0djGr2o67AHQNb41IMcZ+CQKW7XdoMBHT+tLCSZCw+7ZQ1mnHD9NaJbnaxI0qgI03RxXKFj4mqNKjxxzugl9rUfFF0HYJ1KANA==", @"BAFQj+wieG249oNkP5kzC0xTdgtQkd0tTwNmVxYarD05WIjWeaE9VgyfqPVHNyntNA7fEfpLjWy6d5WQGBdFwXxxiAHAqDXeNyNLL0MSzKPmyQNy4TAGjUmGXt/Nq2LafQgKSiWLagSvyaICgk7BBa2z9NvVr4ZcvpreUDzLTrl/+J1WRfR9/78vM3uLfFyTEJwKg6MH3fQRr8RWxcza9AQXspbD");
    test_ies_known_ciphertext(kSecAttrKeyTypeECSECPrimeRandom, @(521), kSecKeyAlgorithmECIESEncryptionCofactorX963SHA512AESGCM, @"BAD56NOl02CAXGSBYKT9JMmMQNtYkaRjw/DP29GWe8X70zTNzTBRgQQzqtCmT2c10VRNxzR5bCAM47fR8HyK4OsV4wEes8IDR68JQC7+epF7cdx+m+EGhaHlB2N99MCEZx9NQni6/5LBMItVY7LSQUskKjqslG90vlIZZxn87EzbJ9MvHgFFgeZeUj9SXcbCu6/+ysgXAtldaGyFwXwzI6CrN84Pd72TR/6me1GZjm8pamJ0p507ntdISB5SBFcAQpNieDYxbg==", @"BAABJeJ2D+Yy0aA1/FKoUF4eBR5kr3JnM8iXDEjIZ47EWixF1nde4ehg7iZpbihxv/rDl7jdJYOW9LqXdqoF/2JO7gCrNSbqSkveG6UdsgfalnAd/tAYvz26Ct6vQiGSJJLvHcnss8thgQrOVQaf/NAoxgxIM0dxUmJE3B2EJxb5sKedzCje0XttQQWvEW1t3Ni606AVAhZ3Psiud23qf/qgtJxK");

    test_ies_known_ciphertext(kSecAttrKeyTypeRSA, @(1024), kSecKeyAlgorithmRSAEncryptionOAEPSHA1AESGCM, @"MIICXQIBAAKBgQC0QZAuyNFNspGakQvFdgwkswN+eG6y2IKefIGbcrhGRvs4LJUp0Z+AjTR5ir6Mm7KeRaFMlwybz1eW0HXEJX86jtCGkB4T9iKW06HOevMqp5p/gOSMroGkX374UMdX5WIsI/qPXxHSp0OdoUC1vsAO99UYGitiYuiyzVm/XONKBQIDAQABAoGAFEkUlXnwDr27zm3oVI6BC1hCBRI/PaKeqytOgqnvEamcoqkRwvpePYn75SRiEVIsUHd8PDGmL5qukWsK9cn/0+xm3BsoB5uUPRUmXRtAJjSSZ3bkhxuGOiLuPpFBlpKvcDk6sUaNQPPel2TsmRpn2PLbyEfPuQH6PmzpePc49rkCQQD2zJgfzRl/PU2d/Z29YoWS/FtxLi0aOq2BcoPacwvaGr6/hfuQk0RiycwoJrXFGe5YfmPtPeg216gj4SteCIdbAkEAuvnk4ZsjquWiHL0UkbGjdXYhVQR+oazisfn7u+gNmjFGE8HBa0YiTXhW9mfvZSX/2sKuxljXqz94VF1qb0uSHwJAeH2Sl5QOqqxHRKcZZ+i9xfEmw68Dnhaftt8tuG0KkEUWc3L6Sq9bZ8+VuNSNUdlDIDk0mBLtWDkZgcrg3VvUmQJBALHi8/YMUnfR91TeM0aVuc0T8YxgNVX+FMN88RoKIX7UaDZ9vVYhKJuJ5TqTEbiq2Wu4ku1UMwPS49ln8s4mGMMCQQDHm3uYC1Okv31eeeJRY2w9ZbVARpMTT+nTCxd0xAxo7y+HPk+gJYWywJlxI3kvO7fl4EgpC0w+0oHPsehwHp3a", @"ItQD/o7D7hFGQaw6ODUWmjH/IhkldnH7/22cIf3vML3iR1T/gDq318oauBKqRUwF0WiVNr/OODlLV+OMiAVFV8oaao9p7ympLFTUv797N84FkftYDckU4qUItyGIqf3YJbe56G9s8Tc+/ztFDguS9B+coGh1jr7lba+XMtkmBJu0GLUxlPxaAEtEclSmTv1mGXn1CEieGTtQyo5ByzI5dA==");
    test_ies_known_ciphertext(kSecAttrKeyTypeRSA, @(1024), kSecKeyAlgorithmRSAEncryptionOAEPSHA224AESGCM, @"MIICXAIBAAKBgQC5dl6b7suTbgA0vWFM7EtgqBLmIksuUwTspm7c/Ecfw2goKGH0D1ABprGKh5nop1oP8hUuMkszzxd5mNAO0ayUvsRhAU11okZkcmO05EKOfs6kqrw9E039bBoy4X/Ps5J5SmkkoigPg2Uh+ZK5+88gNRoXXYUPqGdX7AJ0heFIcwIDAQABAoGANf1yDFXneKtIrxHEjhap2OEE427vUPSFFflbg8SDVglWPH6JCXodseVbgPb5xKNXUhYIuXKVtubeMB1e0DmU0hbGXgYRsfe7wB+7LawPCyMc5K/jnH6hY14mdngFROcbNV1sdMt4Sfo6xvvVVMLDkREbBj+oDauoFSX3TFqQQ0kCQQD0UzUW/NSy0jcWmUUIOCRhvC1yNZWPkBNkq264QUfuz181wfZQx64MM34ECLFQU3XNlVte7ZzpGu0K7DcVHmAJAkEAwlManQJu7uhOq2rTmBugovIQNjvzKB+nF3Ma/1zbSsns6cAn87Btj9i1S0VWgEkMLknX+BfWGY9FEpuNfSbLmwJAaXwvNLCOCHKYFCqyUj0jAAtlt6SI4QW8Sb92Oxj8PI/NtID8np3HeD8XDhjOrTaLauosG80M7NuSMiAQHA8UOQJBAIpjRICsSvQ43E2XNjkM88kXOhRlfTUF1akNgBx7tG/+fYm6Hrmc22mlmvaP2pphaxtL21IDJ6XsMfSecpS+HCECQEcBOTs79wMov4bC42zpG2UCjTLps3Vhik6T5Z8ZE9vml8q1/poKgfYkssn8o9L/f3XZSwVzW1yTaaAjS855rps=", @"QsbG+lUASE1YjhFWgHhx9HkNlnJ9LbWV1fRZBHrxb6nNoJUiAoT4MYjVKrcDdBwmPl2Mg4xyKRePY3pJzc86G1obv76LOpRoaaQkg+hm0e/klGF/qJPBWCqpf3PndfUEHsakGxMQ2bJFL3u9wSQTWFiR1lbdeWADClK4oA5zNZBj1K9Tn1sXhXB/uRvfR3O7n4/1seLbhmyv9nMPIMZ8sQ==");
    test_ies_known_ciphertext(kSecAttrKeyTypeRSA, @(1024), kSecKeyAlgorithmRSAEncryptionOAEPSHA256AESGCM, @"MIICWwIBAAKBgQCgWxqK/7sFc5D6QkVpCGkJ1ObSAlgqVcfaFWE8ftSa+70Hfb7AjjuU9RtkVglZXOM4r6VGv2an8B50XoJXJMe+UxFokzLOZhJ7cKOKk4o7QR0s3nDc8n/bzfQJ10hor0sv9kQTYUOXAqqCkC5XuHwrBtPzCz9qrRVGKGmwilvzhwIDAQABAoGAGYKKGp0WlFuaBOirCyED42+uWAV4dM+tr04hvb0oNw704kQI/Fqu3te5XQEU6VWjbilUf93uHP765qSsU+RYLs3JhZZ/KLGT0RBKXsBRFt5tk2n3XN8UqrSrAAhg2qT0I4a9mOM7dL+ZNBA+yYLamBT4hC22i44o20PLR7pTOn0CQQDbWTYtwY/xUYE6CkroWTGKHt5cyQn13WGYBJ5Ac7XkyiSteluQVZz8XAxxiF+DieehdzJbyRlgMLeqC3ZlUA+9AkEAuyZw4wDcrcPIBHKqH7CPi0R4ey2+ZCWZV24477uZYtQXdDi93WL4ev+ShbC+HahzQ11F2K76eqwrV+gXYJAykwJACu2B3nuzMSGPX5Xdr3+qESiCiXrWjTIvR4SLYcih+jj75MygvSsWvBfV4t4ZbBM1v/yRPLNjGUC0FbumdVusvQJARikGHwP+tyHzhT9bad/uIF698CfY/YBe+Tj4HV+uBC/QzyBKhYmJ78qKKpZ033d8Jp/8BFysyHptEVqQEQJeAwJACPdcu84kIC9myg+VULqQ8z8/R8a6YrVQM+G3dtas/o43v9tB/NlDTP4dFoQpdOP5EuH9YJwp2ttlDoO8+uG8MQ==", @"f6HqrDnncmx5z394bT+HHP4TiTh5249+SkDJYzRD9CrMDfPJuM3i8QmH9kGXFHnGZ2Y5dngEzOrlqqLQyVxTgnYUrHDrkNVz3mckzqlnDh2TifAC/laxGs3h9fQJT1oXmhIfGQqh94PFnFQiQVPVLKU1K9bSvdU6gVwmhkoakWGMHuC5aA1MX/trvUVfP2qE2ffvscPJP5pQdpXl5jLtHw==");
    test_ies_known_ciphertext(kSecAttrKeyTypeRSA, @(1024), kSecKeyAlgorithmRSAEncryptionOAEPSHA384AESGCM, @"MIICXAIBAAKBgQDM7GnrRS4rJK9wGYELIefw0q33S7YMHlbxcrY5JcQl8Waka0mZ0Di6Yd5jlTHuz5wA8gs9019nqaN5F9n5U/Px9wCLXbMyQ4kOk94smRpC6v+GL/IVCLZgV6GPRh3KKLWu3LzLKvqNJ2YKTb2CnwvU3jCdzG/+WkJw3nzfZZfyFQIDAQABAoGAF1a1mX3/jBpZgMLm14W9DMhx18Bfs3GhJU6TQl7yv/+GWSN+9m2oiFGtKlpLnY83jUQD077HFt9TJu94e9T761av5WJVCthYynMhoPCykHRTgEQV9iYl6nES1VHai3ko3cInyhd6N6/EE2UO5Me8r0AOO4WjeQpgfcUfTwnSHeECQQDpafQxjoA6Ht8FLgZn23xP6eI6kq6c+K8dC/1Y8TAIiOUJd7zqzMRN4zYuAvFvIclUq2oitSida1Tq527fNHEHAkEA4MC0ur1ZgBkli4es1cFXyvdM/ja28t9r2CyJQoMf5dtVYgiLq1Co8/9KWCxfkhfm4a8PT8Q1kkIdIYGKjXupAwJAXmnSMZ1vdpL4KPM1+hqIzRZQwNqGMM5SntAzuR9OC5W79zlsvBj5qnumdbQRDp++/TWc588ZT5uTrLTSXwyqTwJAUL0Jb7gLyde+xBQWQ6e0GSaj0wLmz/Lw8/Rzzp/6OoGDd7coLX/JYfXIyEoQfxP1DgfsUTRkJkl324yEsHTG7wJBAKXnHl6SkuWC0C4s8th+gGzxNkAMB7gRUA2BbTM/9v559J2D6AsO7XLR2BhJNdkIot55lMwRsFbwimyDh70aQbk=", @"c7YwdZPmJu7zdciEupcViD60m9/zJdadmRcucAMksqXyKJBFUyTht0tDktz8NQe6NBoPRzBNloKRnaVI2yu8Sgq+m83oNrpzWkXb1CV82NnA4A4K/+MUL/+guG8/7lfNv+2YLO9Bs+FjfHCwRK+9lDZBRdzQkH2y3QHb0RCMzZ1UgbXrGkVQ2OxPb6zMn+19L23LYy5434JM7vFJ5AVv1g==");
    test_ies_known_ciphertext(kSecAttrKeyTypeRSA, @(4096), kSecKeyAlgorithmRSAEncryptionOAEPSHA512AESGCM, @"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", @"Kl3UWEzylpSG0R5m4hY/1S/ZlOdgKhQkG/d0O+WfpVcEmhF8LtjqS8VX5BfhK2u2H07qPxy6HB5XoP488JHzvmoimTFMkghhPY9dzAkuUNvCdSOcSMMeh+1KDIJHxV13YkmTzlf3dUsytwDfSZeRbZ/TLIoav9LbROKhNP4POL4jkKoSFoq0wBXoM6RebDZ+RI69igshJ2WJCAF52hvIiBl56e/6HHFRuuD+WyOJh82p/yCSWN5s4nCdS4urOPxYVQNpBRc9qSJsDiLf9nUBXccoaL7sHjZTPO/tCv13SZWYt1S+DaEl4GqdJ3OPRmYh2YPTObmIJ6vhZ2TgOwWxNjx9DBmh4Xxnf6EcuDvDpHlY0gQYWL9uKGNVCBwMi6jQv5BRsMeiUf6WwZCud7M6EsuaqVaj9JOQQgUScXIPx/b2OzmIRjJpGIMDghN7lnlHuIKz9ED5WoK5Gn9Ypp48PfnBgL6y538pn919VPfTCQ0afGEWsSoFfwl8PcMUkBGhNRRA8SY0LnJuF7raC3eOyQHld+C5Yvg/WW+cePKJjeDWot28ltBRobtQ5EgXOUatjjNMj+1lLZoFNm8eKdE1NKoA0nXCghX5g5Pt/J4mGNEp25T8TCM0Aoqou30sMN/a2NQHFCOvIBJeLP/0hD31Zig6FZZyiFQ3BAURlsqgbGP4O6y6Z98dFsf7eqXAOxSACj/GWad8pfQ3p9tPjADU8Q==");
}
static const int TestCountKnownCipherText = TestCountTestIESKnownCipherText * 5 * 4;

static const int TestCount = TestCountECIES + TestCountRSAWRAP + TestCountAgainstCoreCrypto + TestCountKnownCipherText;

int si_44_seckey_ies(int argc, char *const *argv) {
    plan_tests(TestCount);
    
    @autoreleasepool {
        ccrng_system_init(&ccrng_system_state_seckey_ies_test);

        test_ecies();
        test_rsawrap();
        test_against_corecrypto();
        test_known_ciphertext();
    }
    
    return 0;
}
