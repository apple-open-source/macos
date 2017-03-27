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

#import <Security/SecKeyPriv.h>
#import <Security/oidsalg.h>

#include "shared_regressions.h"

static NSData *decryptAndUnpad(SecKeyRef privateKey, SecKeyAlgorithm algorithm, NSData *ciphertext, NSError **error) {
    NSData *plaintext = CFBridgingRelease(SecKeyCreateDecryptedData(privateKey, algorithm, (CFDataRef)ciphertext, (void *)error));
    if (plaintext != nil && [(__bridge id)algorithm isEqual:(id)kSecKeyAlgorithmRSAEncryptionRaw]) {
        NSRange range = NSMakeRange(0, plaintext.length);
        while (((const UInt8 *)plaintext.bytes)[range.location] == 0x00 && range.location < plaintext.length) {
            range.length--;
            range.location++;
        }
        plaintext = [plaintext subdataWithRange:range];
    }
    return plaintext;
}

static void test_encrypt_run(SecKeyRef privateKey, SecKeyRef publicKey, SecKeyRef iosPrivateKey, SecKeyRef iosPublicKey, SecKeyAlgorithm algorithm) {
    NSData *original = [NSData dataWithBytes:"encrypt" length:7], *plaintext;
    NSError *error;

    error = nil;
    NSData *ciphertext = CFBridgingRelease(SecKeyCreateEncryptedData(publicKey, algorithm, (CFDataRef)original, (void *)&error));
    ok(ciphertext != nil, "RSA encrypt (native) succeeded (error: %@, key %@)", error, publicKey);

    error = nil;
    NSData *iosCiphertext = CFBridgingRelease(SecKeyCreateEncryptedData(iosPublicKey, algorithm, (CFDataRef)original, (void *)&error));
    ok(iosCiphertext != nil, "RSA encrypt (native) succeeded (error: %@, key %@)", error, iosPublicKey);

    error = nil;
    plaintext = decryptAndUnpad(privateKey, algorithm, ciphertext, &error);
    ok(plaintext != nil, "RSA decrypt (native) succeeded (error: %@, key %@)", error, privateKey);
    ok([plaintext isEqual:original], "(native -> native) plaintext equals original (%@ : %@)", original, plaintext);

    error = nil;
    plaintext = decryptAndUnpad(privateKey, algorithm, iosCiphertext, &error);
    ok(plaintext != nil, "RSA decrypt (native) succeeded (error: %@, key %@)", error, privateKey);
    ok([plaintext isEqual:original], "(ios -> native) plaintext equals original (%@ : %@)", original, plaintext);

    error = nil;
    plaintext = decryptAndUnpad(iosPrivateKey, algorithm, ciphertext, &error);
    ok(plaintext != nil, "RSA decrypt (ios) succeeded (error: %@, key %@)", error, privateKey);
    ok([plaintext isEqual:original], "(native -> ios) plaintext equals original (%@ : %@)", original, plaintext);

    error = nil;
    plaintext = decryptAndUnpad(iosPrivateKey, algorithm, iosCiphertext, &error);
    ok(plaintext != nil, "RSA decrypt (ios) succeeded (error: %@, key %@)", error, privateKey);
    ok([plaintext isEqual:original], "(ios -> ios) plaintext equals original (%@ : %@)", original, plaintext);
}
static const int TestCountEncryptRun = 10;

static void test_encrypt_keypair_run(int keySizeInBits, NSArray *algorithms, NSArray *failAlgorithms) {
    NSError *error;
    NSDictionary *params = @{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA, (id)kSecAttrKeySizeInBits: @(keySizeInBits)};

    error = nil;
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    ok(privateKey != nil, "generate private key (error %@)", error);

    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privateKey));
    ok(publicKey != nil, "get public key");

    NSData *data = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privateKey, NULL));
    NSDictionary *attrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privateKey));
    error = nil;
    id iosPrivateKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)data, (CFDictionaryRef)attrs, (void *)&error));
    ok(iosPrivateKey != nil, "get private key created from data");

    data = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)publicKey, NULL));
    attrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)publicKey));
    error = nil;
    id iosPublicKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)data, (CFDictionaryRef)attrs, (void *)&error));
    ok(iosPublicKey != nil, "get public key created from data");

    for (id algorithm in algorithms) {
        test_encrypt_run((__bridge SecKeyRef)privateKey, (__bridge SecKeyRef)publicKey,
                         (__bridge SecKeyRef)iosPrivateKey, (__bridge SecKeyRef)iosPublicKey,
                         (__bridge SecKeyAlgorithm)algorithm);
    }

    for (id algorithm in failAlgorithms) {
        error = nil;
        NSData *data = CFBridgingRelease(SecKeyCreateEncryptedData((SecKeyRef)publicKey, (SecKeyAlgorithm)algorithm, (CFDataRef)[NSData data], (void *)&error));
        ok(data == nil && error.code == errSecParam, "incorrect algorithm refused");
    }
}
static const int TestCountEncryptKeypairRun = 4;

static void test_encryption() {
    test_encrypt_keypair_run(1024,
                             @[
                               (id)kSecKeyAlgorithmRSAEncryptionRaw,
                               (id)kSecKeyAlgorithmRSAEncryptionPKCS1,
                               (id)kSecKeyAlgorithmRSAEncryptionOAEPSHA1,
                               (id)kSecKeyAlgorithmRSAEncryptionOAEPSHA224,
                               (id)kSecKeyAlgorithmRSAEncryptionOAEPSHA256,
                               (id)kSecKeyAlgorithmRSAEncryptionOAEPSHA384,
                               ],
                             @[
                               (id)kSecKeyAlgorithmRSAEncryptionOAEPSHA512,
                               ]);

    test_encrypt_keypair_run(2048,
                             @[
                               (id)kSecKeyAlgorithmRSAEncryptionRaw,
                               (id)kSecKeyAlgorithmRSAEncryptionPKCS1,
                               (id)kSecKeyAlgorithmRSAEncryptionOAEPSHA1,
                               (id)kSecKeyAlgorithmRSAEncryptionOAEPSHA224,
                               (id)kSecKeyAlgorithmRSAEncryptionOAEPSHA256,
                               (id)kSecKeyAlgorithmRSAEncryptionOAEPSHA384,
                               (id)kSecKeyAlgorithmRSAEncryptionOAEPSHA512,
                               ],
                             @[
                               ]);
}
static const int TestCountEncryption =
TestCountEncryptKeypairRun + (TestCountEncryptRun * 6) + (1 * 1) +
TestCountEncryptKeypairRun + (TestCountEncryptRun * 7) + (1 * 0);

static void test_bad_input(NSInteger keySizeInBits, NSInteger inputSize, SecKeyAlgorithm algorithm) {
    NSError *error;
    NSDictionary *params = @{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA, (id)kSecAttrKeySizeInBits: @(keySizeInBits)};

    error = nil;
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    ok(privateKey != nil, "generate private key (error %@)", error);
    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privateKey));

    NSData *input, *output;

    error = nil;
    input = [NSMutableData dataWithLength:inputSize];
    output = CFBridgingRelease(SecKeyCreateEncryptedData((SecKeyRef)publicKey, algorithm, (CFDataRef)input, (void *)&error));
    ok(output, "encryption succeeds at the border size %d (key=%dbytes, %@)", (int)input.length, (int)keySizeInBits / 8, algorithm);
    is((NSInteger)output.length, keySizeInBits / 8, "Unexpected output block size");

    input = [NSMutableData dataWithLength:inputSize + 1];
    output = CFBridgingRelease(SecKeyCreateEncryptedData((SecKeyRef)publicKey, algorithm, (CFDataRef)input, (void *)&error));
    ok(output == nil, "encryption did not fail for border size %d (key=%dbytes, output=%dbytes, %@)", (int)input.length, (int)keySizeInBits / 8, (int)output.length, algorithm);
    is_status((OSStatus)error.code, errSecParam, "Fails with errSecParam for too long input (%@)", algorithm);
}
static const int TestCountBadInputSizeStep = 5;

static void test_bad_input_size() {
    test_bad_input(1024, 128, kSecKeyAlgorithmRSAEncryptionRaw);
    test_bad_input(2048, 256, kSecKeyAlgorithmRSAEncryptionRaw);
    test_bad_input(1024, 128 - 11, kSecKeyAlgorithmRSAEncryptionPKCS1);
    test_bad_input(2048, 256 - 11, kSecKeyAlgorithmRSAEncryptionPKCS1);
    test_bad_input(1024, 128 - 42, kSecKeyAlgorithmRSAEncryptionOAEPSHA1);
    test_bad_input(2048, 256 - 42, kSecKeyAlgorithmRSAEncryptionOAEPSHA1);
    test_bad_input(1024, 128 - 66, kSecKeyAlgorithmRSAEncryptionOAEPSHA256);
    test_bad_input(2048, 256 - 66, kSecKeyAlgorithmRSAEncryptionOAEPSHA256);
}
static const int TestCountBadInputSize = TestCountBadInputSizeStep * 8;

static void test_bad_signature() {
    NSDictionary *params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA, (id)kSecAttrKeySizeInBits: @2048 };
    NSError *error;
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    ok(privateKey, "Generate RSA-2048 temporary key, err %@", error);
    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privateKey));
    ok(publicKey, "Get public key from private key");

#if TARGET_OS_IPHONE
    SecKeyAlgorithm algorithm = kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1;
#else
    SecKeyAlgorithm algorithm = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1;
#endif

    char digest[20] = "digest";
    NSData *digestData = [NSData dataWithBytes:digest length:sizeof(digest)];
    NSData *signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privateKey, algorithm, (CFDataRef)digestData, (void *)&error));
    ok(signature, "Sign digest, err %@", error);

    bool result = SecKeyVerifySignature((SecKeyRef)publicKey, algorithm, (CFDataRef)digestData, (CFDataRef)signature, (void *)&error);
    ok(result, "Verify signature, err %@", error);

    OSStatus status = SecKeyRawVerify((SecKeyRef)publicKey, kSecPaddingPKCS1SHA1, (const uint8_t *)digest, sizeof(digest), signature.bytes, signature.length);
    ok_status(status, "Raw verify correct signature");

    status = SecKeyRawVerify((SecKeyRef)publicKey, kSecPaddingPKCS1SHA1, (const uint8_t *)digest, sizeof(digest), (void  * _Nonnull)NULL, 0);
    is_status(status, errSSLCrypto, "NULL signature failure");

    const SecAsn1AlgId algId = { .algorithm = CSSMOID_SHA1WithRSA };
    signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privateKey, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1, (CFDataRef)digestData, (void *)&error));
    ok(signature, "Sign message, err %@", error);

    status = SecKeyDigestAndVerify((__bridge SecKeyRef)publicKey, &algId, (const uint8_t *)digest, sizeof(digest), signature.bytes, signature.length);
    ok_status(status, "Raw verify correct signature");

    status = SecKeyDigestAndVerify((__bridge SecKeyRef)publicKey, &algId, (const uint8_t *)digest, sizeof(digest), (void * _Nonnull)NULL, 0);
    is_status(status, errSSLCrypto, "NULL signature failure");

    signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privateKey, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1, (CFDataRef)digestData, (void *)&error));
    ok(signature, "Sign message, err %@", error);

    status = SecKeyVerifyDigest((__bridge SecKeyRef)publicKey, &algId, (const uint8_t *)digest, sizeof(digest), signature.bytes, signature.length);
    ok_status(status, "Raw verify correct signature");

    status = SecKeyVerifyDigest((__bridge SecKeyRef)publicKey, &algId, (const uint8_t *)digest, sizeof(digest), (void * _Nonnull)NULL, 0);
    is_status(status, errSSLCrypto, "NULL signature failure");
}
static const int TestCountBadSignature = 12;

static const int TestCount =
TestCountEncryption +
TestCountBadInputSize +
TestCountBadSignature;

int si_44_seckey_rsa(int argc, char *const *argv) {
    plan_tests(TestCount);

    @autoreleasepool {
        test_encryption();
        test_bad_input_size();
        test_bad_signature();
    }
    
    return 0;
}
