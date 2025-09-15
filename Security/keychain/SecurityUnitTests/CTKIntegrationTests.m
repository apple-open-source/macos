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


#import <XCTest/XCTest.h>
#import <Security/SecKeyPriv.h>
#import <Security/SecItemPriv.h>
#import <Security/SecKeyPriv.h>
#import <LocalAuthentication/LocalAuthentication.h>

@interface CTKIntegrationTests : XCTestCase

@end

@implementation CTKIntegrationTests

- (void)testItemAddQueryDelete {
    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
            (id)kSecReturnRef: @YES
        };
        id result;
        XCTAssertEqual(SecItemAdd((CFDictionaryRef)query, (void *)&result), errSecSuccess, @"Failed to generate key");
        XCTAssertEqual(CFGetTypeID((__bridge CFTypeRef)result), SecKeyGetTypeID(), @"Expected SecKey, got %@", result);
    }

    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecReturnRef: @YES
        };
        id result;
        OSStatus status = SecItemCopyMatching((CFDictionaryRef)query, (void *)&result);
        XCTAssertEqual(status, errSecSuccess, @"ItemCopyMatching failed");
    }

    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore
        };
        OSStatus status = SecItemDelete((CFDictionaryRef)query);
        XCTAssertEqual(status, errSecSuccess, @"Deletion failed");
    }

    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecReturnRef: @YES
        };
        id result;
        OSStatus status = SecItemCopyMatching((CFDictionaryRef)query, (void *)&result);
        XCTAssertEqual(status, errSecItemNotFound, @"ItemCopyMatching should not find deleted item");
    }
}

#if TARGET_OS_OSX // not yet for embedded
- (void)testSystemKeychainItemAddQueryDelete {
    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
            (id)kSecUseSystemKeychainAlways: @YES,
            (id)kSecReturnRef: @YES
        };
        id result;
        XCTAssertEqual(SecItemAdd((CFDictionaryRef)query, (void *)&result), errSecSuccess, @"Failed to generate key");
        XCTAssertEqual(CFGetTypeID((__bridge CFTypeRef)result), SecKeyGetTypeID(), @"Expected SecKey, got %@", result);
        NSDictionary *attributes = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)result));
        XCTAssertNotNil(attributes);
        XCTAssertEqualObjects(attributes[(id)kSecUseSystemKeychainAlways], @YES);
    }

    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecUseSystemKeychainAlways: @YES,
            (id)kSecReturnRef: @YES,
        };
        id result;
        OSStatus status = SecItemCopyMatching((CFDictionaryRef)query, (void *)&result);
        XCTAssertEqual(status, errSecSuccess, @"ItemCopyMatching failed");
        NSDictionary *attributes = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)result));
        XCTAssertNotNil(attributes);
        XCTAssertEqualObjects(attributes[(id)kSecUseSystemKeychainAlways], @YES);
    }

    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecReturnRef: @YES,
        };
        id result;
        OSStatus status = SecItemCopyMatching((CFDictionaryRef)query, (void *)&result);
        XCTAssertEqual(status, errSecItemNotFound, @"ItemCopyMatching should not find item in non-system keychain");
    }

    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecUseSystemKeychainAlways: @YES,
        };
        OSStatus status = SecItemDelete((CFDictionaryRef)query);
        XCTAssertEqual(status, errSecSuccess, @"Deletion failed");
    }

    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecUseSystemKeychainAlways: @YES,
            (id)kSecReturnRef: @YES,
        };
        id result;
        OSStatus status = SecItemCopyMatching((CFDictionaryRef)query, (void *)&result);
        XCTAssertEqual(status, errSecItemNotFound, @"ItemCopyMatching should not find deleted item");
    }

    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecReturnRef: @YES,
        };
        id result;
        OSStatus status = SecItemCopyMatching((CFDictionaryRef)query, (void *)&result);
        XCTAssertEqual(status, errSecItemNotFound, @"ItemCopyMatching should not find item in non-system keychain");
    }
}
#endif

- (void)testProtectedItemsAddQueryDelete {
    NSData *password = [@"password" dataUsingEncoding:NSUTF8StringEncoding];
    @autoreleasepool {
        NSError *error;
        id sac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, kSecAccessControlApplicationPassword | kSecAccessControlPrivateKeyUsage, (void *)&error));
        XCTAssertNotNil(sac);
        LAContext *authContext = [[LAContext alloc] init];
        [authContext setCredential:password type:LACredentialTypeApplicationPassword];

        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
            (id)kSecAttrAccessControl: sac,
            (id)kSecUseAuthenticationContext: authContext,
            (id)kSecReturnRef: @YES
        };
        id result;
        XCTAssertEqual(SecItemAdd((CFDictionaryRef)query, (void *)&result), errSecSuccess, @"Failed to generate key");
        XCTAssertEqual(CFGetTypeID((__bridge CFTypeRef)result), SecKeyGetTypeID(), @"Expected SecKey, got %@", result);
    }

    @autoreleasepool {
        LAContext *authContext = [[LAContext alloc] init];
        [authContext setCredential:password type:LACredentialTypeApplicationPassword];
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecUseAuthenticationContext: authContext,
            (id)kSecReturnRef: @YES
        };
        id result;
        OSStatus status = SecItemCopyMatching((CFDictionaryRef)query, (void *)&result);
        XCTAssertEqual(status, errSecSuccess, @"ItemCopyMatching failed");
    }

    @autoreleasepool {
        LAContext *authContext = [[LAContext alloc] init];
        [authContext setCredential:password type:LACredentialTypeApplicationPassword];
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecUseAuthenticationContext: authContext
        };
        OSStatus status = SecItemDelete((CFDictionaryRef)query);
        XCTAssertEqual(status, errSecSuccess, @"Deletion failed");
    }

    @autoreleasepool {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
            (id)kSecReturnRef: @YES
        };
        id result;
        OSStatus status = SecItemCopyMatching((CFDictionaryRef)query, (void *)&result);
        XCTAssertEqual(status, errSecItemNotFound, @"ItemCopyMatching should not find deleted item");
    }
}

- (void)testSecKeyOperations {
    NSError *error;
    NSDictionary *attributes = @{
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (id)kSecAttrIsPermanent: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom
    };
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)attributes, (void *)&error));
    XCTAssertNotNil(privKey, @"Failed to generate key, error %@", error);

    // Get key attributes
    attributes = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privKey));
    XCTAssertNotNil(attributes, @"Failed to get key attributes");
    XCTAssertEqualObjects(attributes[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPrivate);
    XCTAssertEqualObjects(attributes[(id)kSecAttrTokenID], (id)kSecAttrTokenIDAppleKeyStore);
    XCTAssertEqualObjects(attributes[(id)kSecAttrKeyType], (id)kSecAttrKeyTypeECSECPrimeRandom);
    XCTAssertEqualObjects(attributes[(id)kSecAttrKeySizeInBits], @256);

    // Get key attributes through keychain
    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (id)kSecReturnAttributes: @YES
    };
    attributes = nil;
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)query, (void *)&attributes), errSecSuccess);
    XCTAssertNotNil(attributes, @"Failed to get key attributes through keychain");
    XCTAssertEqual([attributes[(id)kSecAttrKeyClass] integerValue], [(id)kSecAttrKeyClassPrivate integerValue]);
    XCTAssertEqualObjects(attributes[(id)kSecAttrTokenID], (id)kSecAttrTokenIDAppleKeyStore);
    XCTAssertEqual([attributes[(id)kSecAttrKeyType] integerValue], [(id)kSecAttrKeyTypeECSECPrimeRandom integerValue]);
    XCTAssertEqualObjects(attributes[(id)kSecAttrKeySizeInBits], @256);

    // Create signature with the key.
    NSData *message = [@"message" dataUsingEncoding:NSUTF8StringEncoding];

    SecKeyAlgorithm algorithm = kSecKeyAlgorithmECDSASignatureMessageX962SHA256;
    NSData *signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privKey, algorithm, (CFDataRef)message, (void *)&error));
    XCTAssertNotNil(signature, @"Failed to sign with token key, error: %@", error);

    // Get public key and verify the signature.
    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    XCTAssertNotNil(pubKey, @"Failed to get pubKey from token privKey");
    XCTAssert(SecKeyVerifySignature((SecKeyRef)pubKey, algorithm, (CFDataRef)message, (CFDataRef)signature, (void *)&error));

    // Perform ECIES encryptoon and decryption.
    algorithm = kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM;
    NSData *ciphertext = CFBridgingRelease(SecKeyCreateEncryptedData((SecKeyRef)pubKey, algorithm, (CFDataRef)message, (void *)&error));
    XCTAssertNotNil(ciphertext, @"Failed to ECIES encrypt data, error:%@", error);

    NSData *plaintext = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privKey, algorithm, (CFDataRef)ciphertext, (void *)&error));
    XCTAssertNotNil(plaintext, @"Failed to decrypt ECIES encrypted data, error:%@", error);
    XCTAssertEqualObjects(plaintext, message);

    // Delete key from keychain.
    query = @{ (id)kSecClass: (id)kSecClassKey, (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore };
    OSStatus status = SecItemDelete((CFDictionaryRef)query);
    XCTAssertEqual(status, errSecSuccess, @"Deletion failed");
}

- (void)testProtectedSecKeyOperations {
    NSData *password = [@"password" dataUsingEncoding:NSUTF8StringEncoding];
    NSError *error;
    id sac = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, kSecAccessControlApplicationPassword | kSecAccessControlPrivateKeyUsage, (void *)&error));
    XCTAssertNotNil(sac);
    LAContext *authContext = [[LAContext alloc] init];
    [authContext setCredential:password type:LACredentialTypeApplicationPassword];

    NSDictionary *attributes = @{
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (id)kSecAttrIsPermanent: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecUseAuthenticationContext: authContext,
        (id)kSecPrivateKeyAttrs: @{
                (id)kSecAttrAccessControl: sac
        }
    };
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)attributes, (void *)&error));
    XCTAssertNotNil(privKey, @"Failed to generate key, error %@", error);

    // Create signature with the key.
    NSData *message = [@"message" dataUsingEncoding:NSUTF8StringEncoding];
    SecKeyAlgorithm algorithm = kSecKeyAlgorithmECDSASignatureMessageX962SHA256;
    NSData *signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privKey, algorithm, (CFDataRef)message, (void *)&error));
    XCTAssertNotNil(signature, @"Failed to sign with token key, error: %@", error);

    // Get public key and verify the signature.
    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    XCTAssertNotNil(pubKey, @"Failed to get pubKey from token privKey");
    XCTAssert(SecKeyVerifySignature((SecKeyRef)pubKey, algorithm, (CFDataRef)message, (CFDataRef)signature, (void *)&error));

    // Perform ECIES encryptoon and decryption.
    algorithm = kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM;
    NSData *ciphertext = CFBridgingRelease(SecKeyCreateEncryptedData((SecKeyRef)pubKey, algorithm, (CFDataRef)message, (void *)&error));
    XCTAssertNotNil(ciphertext, @"Failed to ECIES encrypt data, error:%@", error);

    NSData *plaintext = CFBridgingRelease(SecKeyCreateDecryptedData((SecKeyRef)privKey, algorithm, (CFDataRef)ciphertext, (void *)&error));
    XCTAssertNotNil(plaintext, @"Failed to decrypt ECIES encrypted data, error:%@", error);
    XCTAssertEqualObjects(plaintext, message);

    // Delete key from keychain.
    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (id)kSecUseAuthenticationContext: authContext,
    };
    OSStatus status = SecItemDelete((CFDictionaryRef)query);
    XCTAssertEqual(status, errSecSuccess, @"Deletion failed");
}

- (void)testMLDSAIntegration {
    NSData *messageData = [@"Hello ML-DSA" dataUsingEncoding:NSUTF8StringEncoding];
    [self verifyMLDSAIntegrationWithKeySize:(id)kSecAttrKeySizeMLDSA65 messageData:messageData];
    [self verifyMLDSAIntegrationWithKeySize:(id)kSecAttrKeySizeMLDSA87 messageData:messageData];
}

- (void)testMLKEMKEMIntegration {
    [self verifyKEMIntegrationWithKeyType:(id)kSecAttrKeyTypeMLKEM keySize:(id)kSecAttrKeySizeMLKEM768 algorithm:kSecKeyAlgorithmKEMMLKEM];
    [self verifyKEMIntegrationWithKeyType:(id)kSecAttrKeyTypeMLKEM keySize:(id)kSecAttrKeySizeMLKEM1024 algorithm:kSecKeyAlgorithmKEMMLKEM];
    [self verifyKEMIntegrationWithKeyType:(id)kSecAttrKeyTypeKyber keySize:(id)kSecAttrKeySizeKyber768 algorithm:kSecKeyAlgorithmKEMKyber];
    [self verifyKEMIntegrationWithKeyType:(id)kSecAttrKeyTypeKyber keySize:(id)kSecAttrKeySizeKyber1024 algorithm:kSecKeyAlgorithmKEMKyber];
}

#pragma mark - Helpers

- (void)verifyMLDSAIntegrationWithKeySize:(id)keySize messageData:(NSData *)messageData {
    id keyType = (id)kSecAttrKeyTypeMLDSA;
    SecKeyAlgorithm algorithm = kSecKeyAlgorithmMLDSASignatureMessage;

    NSDictionary *privKeyAttributes = @{
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeMLDSA,
        (id)kSecAttrKeySizeInBits: keySize,
    };

    NSError *error = nil;
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)privKeyAttributes, (void *)&error));
    XCTAssertNotNil(privKey, @"Failed to generate key pair: %@", error.localizedDescription);

    // Verify Private Key Attributes
    NSDictionary *privAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privKey));
    XCTAssertNotNil(privAttrs, @"Failed to retrieve private key attributes");
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeyType], keyType);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPrivate);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeySizeInBits], keySize);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrTokenID], (id)kSecAttrTokenIDAppleKeyStore);

    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    XCTAssertNotNil(pubKey);

    NSDictionary *pubAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)pubKey));
    XCTAssertNotNil(pubAttrs, @"Failed to retrieve public key attributes");
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeyType], keyType);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPublic);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeySizeInBits], keySize);

    // Sign with CTK
    NSData *signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privKey,
                                                                algorithm,
                                                                (__bridge CFDataRef)messageData,
                                                                (void *)&error));
    XCTAssertNotNil(signature, @"Signing failed: %@", error.localizedDescription);

    // Verify with SecKey
    BOOL isValid = SecKeyVerifySignature((SecKeyRef)pubKey,
                                         algorithm,
                                         (CFDataRef)messageData,
                                         (CFDataRef)signature,
                                         (void *)&error);
    XCTAssertTrue(isValid, @"Signature verification failed: %@", error.localizedDescription);

#if !TARGET_OS_SIMULATOR
    NSDictionary *deleteQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyType: keyType,
        (id)kSecAttrKeySizeInBits: keySize,
    };
    OSStatus deleteStatus = SecItemDelete((CFDictionaryRef)deleteQuery);
    XCTAssertEqual(deleteStatus, errSecSuccess, @"Failed to delete key pair, OSStatus: %d", (int)deleteStatus);
#endif
}

- (void)verifyKEMIntegrationWithKeyType:(id)keyType keySize:(id)keySize algorithm:(SecKeyAlgorithm)algorithm {
    NSDictionary *privKeyAttributes = @{
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (id)kSecAttrKeyType: keyType,
        (id)kSecAttrKeySizeInBits: keySize,
    };

    NSError *error = nil;
    id privKeyRef = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)privKeyAttributes, (void *)&error));
    XCTAssertNotNil(privKeyRef, @"Failed to generate key pair: %@", error.localizedDescription);

    // Verify Private Key Attributes
    NSDictionary *privAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privKeyRef));
    XCTAssertNotNil(privAttrs, @"Failed to retrieve private key attributes");
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeyType], keyType);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPrivate);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeySizeInBits], keySize);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrTokenID], (id)kSecAttrTokenIDAppleKeyStore);

    id pubKeyRef = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKeyRef));
    XCTAssertNotNil(pubKeyRef, @"Failed to retrieve public key from private key");

    NSDictionary *pubAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)pubKeyRef));
    XCTAssertNotNil(pubAttrs, @"Failed to retrieve public key attributes");
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeyType], keyType);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPublic);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeySizeInBits], keySize);

    // Export Public Key
    CFErrorRef exportError = NULL;
    CFDataRef cfPubKeyData = SecKeyCopyExternalRepresentation((SecKeyRef)pubKeyRef, &exportError);
    NSData *pubKeyData = CFBridgingRelease(cfPubKeyData);
    XCTAssertNotNil(pubKeyData, @"Failed to export public key: %@", (__bridge NSError *)exportError);

    CFDataRef cfSharedKey = NULL;
    CFDataRef cfEncapsulatedKey = SecKeyCreateEncapsulatedKey((__bridge SecKeyRef)pubKeyRef,
                                                              algorithm,
                                                              &cfSharedKey,
                                                              (void *)&error);

    NSData *sharedKey = CFBridgingRelease(cfSharedKey);
    NSData *encapsulatedKey = CFBridgingRelease(cfEncapsulatedKey);
    XCTAssertNotNil(sharedKey, @"Failed to create shared key: %@", error.localizedDescription);
    XCTAssertNotNil(encapsulatedKey, @"Failed to create encapsulated key: %@", error.localizedDescription);

    NSData *decapsulatedSharedKey = CFBridgingRelease(SecKeyCreateDecapsulatedKey((__bridge SecKeyRef)(privKeyRef),
                                                                                  algorithm,
                                                                                  cfEncapsulatedKey,
                                                                                  (void *)&error));
    XCTAssertNotNil(decapsulatedSharedKey, @"Failed to decapsulate key: %@", error.localizedDescription);

#if !TARGET_OS_SIMULATOR
    NSDictionary *deleteQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyType: keyType,
        (id)kSecAttrKeySizeInBits: keySize,
    };
    OSStatus deleteStatus = SecItemDelete((CFDictionaryRef)deleteQuery);
    XCTAssertEqual(deleteStatus, errSecSuccess, @"Failed to delete key pair, OSStatus: %d", (int)deleteStatus);
#endif
}

@end
