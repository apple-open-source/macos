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

static const int TestCount = TestCountEncryption;
int si_44_seckey_rsa(int argc, char *const *argv) {
    plan_tests(TestCount);

    @autoreleasepool {
        test_encryption();
    }
    
    return 0;
}
