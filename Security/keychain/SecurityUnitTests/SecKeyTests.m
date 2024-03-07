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

#import <XCTest/XCTest.h>
#import <Security/SecKeyPriv.h>
#import <Security/SecItemPriv.h>

@interface SecKeyTests : XCTestCase
@end

@implementation SecKeyTests

- (void)testSecKeyAttributesCanBeReadWithMatchingStringsAsKeys {
    CFMutableDictionaryRef keyParameters = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(keyParameters, kSecAttrKeyType, kSecAttrKeyTypeECSECPrimeRandom);
    CFDictionarySetValue(keyParameters, kSecAttrKeySizeInBits, (__bridge CFNumberRef)@(384));
    CFDictionarySetValue(keyParameters, CFSTR("nleg"), kCFBooleanTrue);
    SecKeyRef secKey = SecKeyCreateRandomKey(keyParameters, nil);
    NSDictionary* attributes = (__bridge_transfer NSDictionary*)SecKeyCopyAttributes(secKey);
    XCTAssertEqualObjects(attributes[(__bridge NSString*)kSecAttrKeySizeInBits], attributes[@"bsiz"], @"the SecKey attributes dictionary value of 'kSecAttrKeySizeInBits' and 'bsiz' are not the same");
    XCTAssertNotNil(attributes[@"bsiz"], @"the SecKey attributes dictionary value for 'bsiz' is nil");
    CFRelease(secKey);
}

- (void)testECIESDecryptBadInputData {
    NSData *message = [@"message" dataUsingEncoding:NSUTF8StringEncoding];
    NSError *error;
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256}, (void *)&error));
    XCTAssertNotNil(privKey, @"key generation failed: %@", error);
    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    XCTAssertNotNil(pubKey);
    NSData *ciphertext = CFBridgingRelease(SecKeyCreateEncryptedData((SecKeyRef)pubKey, kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM, (CFDataRef)message, (void *)&error));
    XCTAssertNotNil(ciphertext, @"Encryption failed: %@", error);
    NSData *plaintext = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privKey, kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM, (CFDataRef)ciphertext, (void *)&error));
    XCTAssertEqualObjects(message, plaintext, @"Decryption did not provide original message");

    // Strip tag from ciphertext
    NSData *strippedCiphertext = [ciphertext subdataWithRange:NSMakeRange(0, ciphertext.length - 16)];
    NSData *failedDecrypted = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privKey, kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM, (CFDataRef)strippedCiphertext, (void *)&error));
    XCTAssertNil(failedDecrypted, @"Decryption of malformed data did not fail");
    XCTAssertEqual(error.code, errSecParam, @"Unexpected error code provided");
}

static CFIndex SecTestKeyGetAlgorithmID(SecKeyRef key) {
    return kSecECDSAAlgorithmID;
}

static SecKeyAlgorithm secTestKeySupportedAlgorithm = NULL;
static CFTypeRef SecTestKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm, CFArrayRef allAlgorithms, SecKeyOperationMode mode, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (!CFEqual(algorithm, secTestKeySupportedAlgorithm)) {
        return kCFNull;
    }

    NSArray *algs = (__bridge NSArray *)allAlgorithms;
    XCTAssertEqualObjects(algs.lastObject, (__bridge id)secTestKeySupportedAlgorithm);
    if (mode == kSecKeyOperationModeCheckIfSupported) {
        return kCFBooleanTrue;
    }

    id tempKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{
        (id)kSecAttrIsPermanent: @NO,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits: @256,
    }, error));
    if (tempKey == nil) {
        return nil;
    }
    return SecKeyCreateSignature((SecKeyRef)tempKey, algorithm, in1, error);
}

static size_t SecTestKeyGetBlockSize(SecKeyRef key) {
    return 256 / 8;
}

static SecKeyDescriptor SecTestKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "SecTestKey",
    .getAlgorithmID = SecTestKeyGetAlgorithmID,
    .blockSize = SecTestKeyGetBlockSize,
    .copyOperationResult = SecTestKeyCopyOperationResult,
};

- (void)testLegacyAPIBridging {
    NSData *message = [NSMutableData dataWithLength:256 / 8];
    NSError *error;
    id privKey = CFBridgingRelease(SecKeyCreate(kCFAllocatorDefault, &SecTestKeyDescriptor, 0, 0, 0));
    CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)@{(id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256}, (void *)&error));
    XCTAssertNotNil(privKey, @"key generation failed: %@", error);
    NSMutableData *signature = [NSMutableData dataWithLength:256];
    size_t sigLength = signature.length;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    secTestKeySupportedAlgorithm = kSecKeyAlgorithmECDSASignatureDigestX962SHA256;
    OSStatus status = SecKeyRawSign((__bridge SecKeyRef)privKey, kSecPaddingPKCS1, message.bytes, message.length, signature.mutableBytes, &sigLength);
#pragma clang diagnostic pop
    XCTAssertEqual(status, errSecSuccess, @"Encryption failed");
}

#if TARGET_OS_OSX
- (BOOL)isCDSAKey:(id)key {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    XCTAssertNotNil(key);
    return [[key description] hasPrefix:@"<SecCDSAKeyRef"];
#pragma clang diagnostic pop
}

- (void)testSecKeyGenerateWithKeychainSelection {
    NSDictionary *params;
    NSError *error;
    id ac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, 0, NULL));

    @autoreleasepool {
        params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256 };
        id key = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(key, @"failed to create CDSA key: %@", error);
        XCTAssert([self isCDSAKey:key], @"expected to create CDSA key, but got %@", key);
    }
    
    @autoreleasepool {
        params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256,
                    (id)kSecUseDataProtectionKeychain: @YES,
        };
        id key = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(key, @"failed to create modern key: %@", error);
        XCTAssertFalse([self isCDSAKey:key], @"expected to create modern key, but got %@", key);
    }
    
    @autoreleasepool {
        params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256,
                    (id)kSecUseSystemKeychainAlways: @YES,
        };
        id key = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(key, @"failed to create modern key in syskeychain: %@", error);
        XCTAssertFalse([self isCDSAKey:key], @"expected to create modern key, but got %@", key);
    }
    
    @autoreleasepool {
        params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256,
                    (id)kSecAttrAccessControl: ac,
        };
        id key = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(key, @"failed to create modern key: %@", error);
        XCTAssertFalse([self isCDSAKey:key], @"expected to create modern key, but got %@", key);
    }
    
    @autoreleasepool {
        params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256,
                    (id)kSecUseDataProtectionKeychain: @NO,
        };
        id key = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(key, @"failed to create CDSA key: %@", error);
        XCTAssert([self isCDSAKey:key], @"expected to create CDSA key, but got %@", key);
    }
    
    @autoreleasepool {
        params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256,
                    (id)kSecPrivateKeyAttrs: @{
                        (id)kSecUseDataProtectionKeychain: @YES,
                    },
        };
        id key = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(key, @"failed to create modern key: %@", error);
        XCTAssertFalse([self isCDSAKey:key], @"expected to create modern key, but got %@", key);
    }
    
    @autoreleasepool {
        params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256,
                    (id)kSecPublicKeyAttrs: @{
                        (id)kSecUseDataProtectionKeychain: @YES,
                    },
        };
        id key = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(key, @"failed to create modern key: %@", error);
        XCTAssertFalse([self isCDSAKey:key], @"expected to create modern key, but got %@", key);
    }

    @autoreleasepool {
        params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256,
                    (id)kSecPrivateKeyAttrs: @{
                        (id)kSecUseDataProtectionKeychain: @YES,
                    },
                    (id)kSecPublicKeyAttrs: @{
                        (id)kSecUseDataProtectionKeychain: @NO,
                    },
        };
        id key = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNil(key, @"did not fail when trying to create mixed modern/CDSA keypair, got privkey %@", key);
    }

    @autoreleasepool {
        params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256,
                    (id)kSecPrivateKeyAttrs: @{
                        (id)kSecUseSystemKeychainAlways: @YES,
                    },
        };
        id key = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(key, @"failed to create modern key: %@", error);
        XCTAssertFalse([self isCDSAKey:key], @"expected to create modern key, but got %@", key);
    }

    @autoreleasepool {
        params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256,
                    (id)kSecPrivateKeyAttrs: @{
                        (id)kSecAttrAccessControl: ac,
                    },
        };
        id key = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(key, @"failed to create modern key: %@", error);
        XCTAssertFalse([self isCDSAKey:key], @"expected to create modern key, but got %@", key);
    }
}

- (void)testCDSAKeyExportImportFormats {
    @autoreleasepool {
        NSError *error;
        id keyType = (id)kSecAttrKeyTypeECSECPrimeRandom;
        NSDictionary *params = @{ (id)kSecAttrKeyType: keyType, (id)kSecAttrKeySizeInBits: @256,
                                  (id)kSecUseDataProtectionKeychain: @NO,
                                  (id)kSecAttrIsPermanent: @NO,
        };
        id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(privKey, @"gen EC key: %@", error);
        XCTAssert([self isCDSAKey:privKey]);
        id pubKeyTemp = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
        XCTAssertNotNil(pubKeyTemp);
        NSData *pubKeyTempData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)pubKeyTemp, (void *)&error));
        XCTAssertNotNil(pubKeyTempData, @"Export pubkey: %@", error);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        id pubKey = CFBridgingRelease(SecKeyCreateFromData((CFDictionaryRef)@{ (id)kSecAttrKeyType: keyType, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic, (id)kSecAttrKeySizeInBits: @256}, (CFDataRef)pubKeyTempData, (void *)&error));
#pragma clang diagnostic pop
        XCTAssertNotNil(pubKey, @"Import pubKey as CDSA: %@", error);
        XCTAssert([self isCDSAKey:pubKey]);

        NSData *privKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privKey, (void *)&error));
        XCTAssertNotNil(privKeyData, @"EC privKey export: %@", error);
        id privKey2 = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privKeyData, (CFDictionaryRef)@{ (id)kSecAttrKeyType: keyType, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate }, (void *)&error));
        XCTAssertNotNil(privKey2, @"EC privKey import: %@", error);

        NSData *pubKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)pubKey, (void *)&error));
        XCTAssertNotNil(pubKeyData, @"EC pubKey export: %@", error);
        id pubKeyImport = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)pubKeyData, (CFDictionaryRef)@{ (id)kSecAttrKeyType: keyType, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic }, (void *)&error));
        XCTAssert(![self isCDSAKey:pubKeyImport]);
        XCTAssertNotNil(pubKeyImport, @"EC pubKey import: %@", error);
        id pubKey2 = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
        XCTAssert([self isCDSAKey:pubKey2]);
        NSDictionary *pubKeyImportAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)pubKeyImport));
        NSDictionary *pubKey2Attrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)pubKey2));
        XCTAssertEqualObjects(pubKey2Attrs[(id)kSecAttrApplicationLabel], pubKeyImportAttrs[(id)kSecAttrApplicationLabel], @"Imported and copyPublicKey are not the same");

        SecKeyAlgorithm algorithm = kSecKeyAlgorithmECDSASignatureMessageX962SHA256;
        NSData *message = [@"message" dataUsingEncoding:NSUTF8StringEncoding];
        
        NSData *sig1 = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privKey, algorithm, (CFDataRef)message, (void *)&error));
        XCTAssertNotNil(sig1, "EC privKey sign: %@", error);
        XCTAssert(SecKeyVerifySignature((SecKeyRef)pubKey2, algorithm, (CFDataRef)message, (CFDataRef)sig1, (void *)&error), @"EC pubKey2 verify: %@", error);

        NSData *sig2 = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privKey2, algorithm, (CFDataRef)message, (void *)&error));
        XCTAssertNotNil(sig1, "EC privKey2 sign: %@", error);
        XCTAssert(SecKeyVerifySignature((SecKeyRef)pubKey, algorithm, (CFDataRef)message, (CFDataRef)sig2, (void *)&error), @"EC pubKey verify: %@", error);
    }

    @autoreleasepool {
        NSError *error;
        id keyType = (id)kSecAttrKeyTypeRSA;
        NSDictionary *params = @{ (id)kSecAttrKeyType: keyType, (id)kSecAttrKeySizeInBits: @2048,
                                  (id)kSecUseDataProtectionKeychain: @NO,
                                  (id)kSecAttrIsPermanent: @NO,
        };
        id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(privKey, @"gen RSA key: %@", error);
        XCTAssert([self isCDSAKey:privKey]);
        id pubKeyTemp = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
        XCTAssertNotNil(pubKeyTemp);
        NSData *pubKeyTempData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)pubKeyTemp, (void *)&error));
        XCTAssertNotNil(pubKeyTempData, @"Export pubkey: %@", error);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        id pubKey = CFBridgingRelease(SecKeyCreateFromData((CFDictionaryRef)@{ (id)kSecAttrKeyType: keyType, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic, (id)kSecAttrKeySizeInBits: @2048}, (CFDataRef)pubKeyTempData, (void *)&error));
#pragma clang diagnostic pop
        XCTAssertNotNil(pubKey, @"Import pubKey as CDSA: %@", error);
        XCTAssert([self isCDSAKey:pubKey]);

        NSData *privKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privKey, (void *)&error));
        XCTAssertNotNil(privKeyData, @"RSA privKey export: %@", error);
        id privKey2 = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privKeyData, (CFDictionaryRef)@{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate }, (void *)&error));
        XCTAssertNotNil(privKey2, @"RSA privKey import: %@", error);

        NSData *pubKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)pubKey, (void *)&error));
        XCTAssertNotNil(pubKeyData, @"RSA pubKey export: %@", error);
        id pubKeyImport = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)pubKeyData, (CFDictionaryRef)@{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA, (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic }, (void *)&error));
        XCTAssert(![self isCDSAKey:pubKeyImport]);
        XCTAssertNotNil(pubKeyImport, @"EC pubKey import: %@", error);
        id pubKey2 = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
        XCTAssert([self isCDSAKey:pubKey2]);
        NSDictionary *pubKeyImportAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)pubKeyImport));
        NSDictionary *pubKey2Attrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)pubKey2));
        XCTAssertEqualObjects(pubKey2Attrs[(id)kSecAttrApplicationLabel], pubKeyImportAttrs[(id)kSecAttrApplicationLabel], @"Imported and copyPublicKey are not the same");

        SecKeyAlgorithm algorithm = kSecKeyAlgorithmRSASignatureMessagePSSSHA256;
        NSData *message = [@"message" dataUsingEncoding:NSUTF8StringEncoding];

        NSData *sig1 = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privKey, algorithm, (CFDataRef)message, (void *)&error));
        XCTAssertNotNil(sig1, "RSA privKey sign: %@", error);
        XCTAssert(SecKeyVerifySignature((SecKeyRef)pubKey2, algorithm, (CFDataRef)message, (CFDataRef)sig1, (void *)&error), @"RSA pubKey2 verify: %@", error);

        NSData *sig2 = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privKey2, algorithm, (CFDataRef)message, (void *)&error));
        XCTAssertNotNil(sig1, "RSA privKey2 sign: %@", error);
        XCTAssert(SecKeyVerifySignature((SecKeyRef)pubKey, algorithm, (CFDataRef)message, (CFDataRef)sig2, (void *)&error), @"RSA pubKey verify: %@", error);
    }
}

- (void)testExportCDSAKey {
    @autoreleasepool {
        NSDictionary *params = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeySizeInBits: @256,
                                  (id)kSecUseDataProtectionKeychain: @NO,
                                  (id)kSecAttrIsPermanent: @NO,
        };
        NSError *error;
        id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
        XCTAssertNotNil(privKey, @"failed to create CDSA key: %@", error);
        id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
        XCTAssertNotNil(pubKey, @"failed to get pubkey from gen key");

        NSData *data;
        OSStatus status = SecItemExport((SecKeyRef)privKey, kSecFormatPEMSequence, kSecItemPemArmour, NULL, (void *)&data);
        XCTAssertEqual(status, errSecSuccess, @"Failed to export gen privkey");

        // Import the key back
        NSArray *importedItems;
        SecExternalFormat externalFormat = kSecFormatUnknown;
        SecExternalItemType externalType = kSecItemTypeUnknown;
        status = SecItemImport((CFDataRef)data, NULL, &externalFormat, &externalType, 0, NULL, NULL, (void *)&importedItems);
        XCTAssertEqual(status, errSecSuccess, @"Failed to import gen privkey");
        XCTAssertEqual(importedItems.count, 1);
        id impPrivKey = importedItems.firstObject;
        NSDictionary *privKeyAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privKey));
        NSDictionary *impPrivKeyAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)impPrivKey));
        XCTAssertEqualObjects(impPrivKeyAttrs[(id)kSecAttrApplicationLabel], privKeyAttrs[(id)kSecAttrApplicationLabel], @"Imported key is not equal to exported one");

        id impPubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)impPrivKey));
        XCTAssertNotNil(impPubKey, @"getting pubkey from re-imported priv key");
        NSDictionary *pubKeyAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)pubKey));
        NSDictionary *impPubKeyAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)impPubKey));
        XCTAssertEqualObjects(impPubKeyAttrs[(id)kSecAttrApplicationLabel], pubKeyAttrs[(id)kSecAttrApplicationLabel], @"Imported pubkey is not equal to pubkey of exported one");
    }
}

#endif

- (void)testSecKeyECDSAAlgorithmAdaptors {
    id privKey = CFBridgingRelease(SecKeyCreate(kCFAllocatorDefault, &SecTestKeyDescriptor, 0, 0, 0));
    XCTAssertNotNil(privKey);
    SecKeyRef key = (__bridge SecKeyRef)privKey;
    
    secTestKeySupportedAlgorithm = kSecKeyAlgorithmECDSASignatureDigestX962SHA256;
    
    XCTAssert(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureDigestX962SHA256));
    XCTAssert(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureMessageX962SHA256));
    XCTAssertFalse(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureDigestX962));
    XCTAssertFalse(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureMessageX962SHA224));
    XCTAssertFalse(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureMessageX962SHA384));

    XCTAssert(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureDigestRFC4754SHA256));
    XCTAssert(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureMessageRFC4754SHA256));
    XCTAssertFalse(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureDigestRFC4754));
    XCTAssertFalse(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureMessageRFC4754SHA224));
    XCTAssertFalse(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureMessageRFC4754SHA384));
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    XCTAssertFalse(SecKeyIsAlgorithmSupported(key, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureRFC4754));
#pragma clang diagnostic pop

    NSData *digestSHA224 = [NSMutableData dataWithLength:224 / 8];
    NSData *digestSHA256 = [NSMutableData dataWithLength:256 / 8];
    NSData *message = [@"message" dataUsingEncoding:NSUTF8StringEncoding];
    NSError *error;
    XCTAssertNotNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureDigestX962SHA256, (CFDataRef)digestSHA256, (void *)&error)), @"error: %@", error);
    XCTAssertNotNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (CFDataRef)message, (void *)&error)), @"error: %@", error);
    XCTAssertNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureDigestX962, (CFDataRef)digestSHA256, (void *)&error)));
    XCTAssertNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureDigestX962SHA224, (CFDataRef)digestSHA224, (void *)&error)));
    XCTAssertNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureMessageX962SHA384, (CFDataRef)message, (void *)&error)));

    XCTAssertNotNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureDigestRFC4754SHA256, (CFDataRef)digestSHA256, (void *)&error)), @"error: %@", error);
    XCTAssertNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureDigestRFC4754SHA256, (CFDataRef)digestSHA224, (void *)&error)));
    XCTAssertNotNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureMessageRFC4754SHA256, (CFDataRef)message, (void *)&error)), @"error: %@", error);
    XCTAssertNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureDigestRFC4754, (CFDataRef)digestSHA256, (void *)&error)));
    XCTAssertNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureDigestRFC4754SHA224, (CFDataRef)digestSHA224, (void *)&error)));
    XCTAssertNil(CFBridgingRelease(SecKeyCreateSignature(key, kSecKeyAlgorithmECDSASignatureMessageRFC4754SHA384, (CFDataRef)message, (void *)&error)));
}

@end
