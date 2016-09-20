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

static void test_export_import_run(int size) {
    NSError *error;
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @(size)}, (void *)&error));
    ok(privKey, "generate private key (size %d, error %@)", size, error);

    NSData *message = [NSData dataWithBytes:"hello" length:5];
    error = nil;
    NSData *signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA1, (CFDataRef)message, (void *)&error));
    ok(signature, "create signature, %@", error);

    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    error = nil;
    NSData *pubKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)pubKey, (void *)&error));
    ok(pubKeyData, "export public key, %@", error);
    size = (size + 7) / 8;
    is(pubKeyData.length, (unsigned)size * 2 + 1, "pubkey data has expected length");

    id importedPubKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)pubKeyData, (CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic}, (void *)&error));
    ok(importedPubKey, "import public key, %@", error);
    ok(SecKeyVerifySignature((SecKeyRef)importedPubKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA1, (CFDataRef)message, (CFDataRef)signature, (void *)&error), "verify signature, %@", error);

    error = nil;
    NSData *privKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privKey, (void *)&error));
    ok(privKeyData, "export privKey, %@", error);
    is(privKeyData.length, (unsigned)size * 3 + 1, "privkey data has expected length");

    error = nil;
    id importedPrivKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privKeyData, (CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate}, (void *)&error));
    ok(importedPrivKey, "import privKey, %@", error);

    id importedPrivKeyPubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)importedPrivKey));
    error = nil;
    ok(SecKeyVerifySignature((SecKeyRef)importedPrivKeyPubKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA1, (CFDataRef)message, (CFDataRef)signature, (void *)&error), "verify signature, %@", error);
}
static const int TestExportImportRun = 10;

static void test_export_import() {
    test_export_import_run(192);
    test_export_import_run(256);
    test_export_import_run(521);
}
static const int TestExportImport = TestExportImportRun * 3;

static const int TestCount = TestExportImport;
int si_44_seckey_ec(int argc, char *const *argv) {
    plan_tests(TestCount);

    @autoreleasepool {
        test_export_import();
    }
    
    return 0;
}
