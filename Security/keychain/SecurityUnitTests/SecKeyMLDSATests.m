//
//  SecKeyMLDSATests.m
//  Example: ML-DSA tests using SecKey & CoreCrypto
//

#import <XCTest/XCTest.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <Security/SecKeyPriv.h>
#import <CoreFoundation/CoreFoundation.h>
#import <corecrypto/ccmldsa.h>
#import <os/log.h>
#include <utilities/SecCFWrappers.h>

@interface SecKeyMLDSATests : XCTestCase
@end

@implementation SecKeyMLDSATests

- (void)testMLDSAKeyGenerationSize65 {
    [self verifyKeyGenerationWithSize:(id)kSecAttrKeySizeMLDSA65 ccMLDSAParams:ccmldsa65()];
}

- (void)testMLDSAKeyGenerationSize87 {
    [self verifyKeyGenerationWithSize:(id)kSecAttrKeySizeMLDSA87 ccMLDSAParams:ccmldsa87()];
}

- (void)testMLDSASignAndVerifySize65 {
    [self signAndVerifyWithKeySize:(id)kSecAttrKeySizeMLDSA65 algorithm:kSecKeyAlgorithmMLDSASignatureMessage];
}

- (void)testMLDSASignAndVerifySize87 {
    [self signAndVerifyWithKeySize:(id)kSecAttrKeySizeMLDSA87 algorithm:kSecKeyAlgorithmMLDSASignatureMessage];
}

- (void)testMLDSASignatureVerificationFailsWithCorruptedSignatureSize65 {
    [self verifySignatureFailureWithKeySize:(id)kSecAttrKeySizeMLDSA65];
}

- (void)testMLDSASignatureVerificationFailsWithCorruptedSignatureSize87 {
    [self verifySignatureFailureWithKeySize:(id)kSecAttrKeySizeMLDSA87];
}

- (void)testMLDSAImportExportKeysSize65 {
    [self verifyImportExportPublicPrivateKeyWithSize:(id)kSecAttrKeySizeMLDSA65];
}

- (void)testMLDSAImportExportKeysSize87 {
    [self verifyImportExportPublicPrivateKeyWithSize:(id)kSecAttrKeySizeMLDSA87];
}

/// Test an SPI which supports ML-DSA with context
- (void)testSecKeyCreateMLDSASignature {
    // Test signature with context
    NSData *messageData = [@"Hello ML-DSA" dataUsingEncoding:NSUTF8StringEncoding];
    NSError *error = nil;

    NSData *context = [@"context" dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary *parameters = @{ (id)kSecKeySignatureParameterContext: context };

    id privKey = [self generatePrivateKeyWithSize:(id)kSecAttrKeySizeMLDSA87];
    XCTAssertNotNil(privKey, @"Failed to generate private key for signing and verification.");

    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));

    ccmldsa_pub_ctx_t ctx = NULL;
    int err = importMLDSAPublicKey((__bridge SecKeyRef)(pubKey), &ctx);

    NSData *signatureWithContext = CFBridgingRelease(SecKeyCreateMLDSASignature((__bridge SecKeyRef)privKey,
                                                                                  kSecKeyAlgorithmMLDSASignatureMessage,
                                                                                  (__bridge CFDataRef)messageData,
                                                                                  (__bridge CFDictionaryRef)parameters,
                                                                                  (void *)&error));

    // We only support ML-DSA signature generation with context,
    // so the verification is done via corecrypto
    err = ccmldsa_verify_with_context(ctx,
                                      signatureWithContext.length, signatureWithContext.bytes,
                                      messageData.length, messageData.bytes,
                                      context.length, context.bytes);
    XCTAssertEqual(err, 0, @"corecrypto verification of signature with context failed with error %d", err);


    NSData *signatureWithoutContext = CFBridgingRelease(SecKeyCreateMLDSASignature((__bridge SecKeyRef)privKey,
                                                                                  kSecKeyAlgorithmMLDSASignatureMessage,
                                                                                  (__bridge CFDataRef)messageData,
                                                                                  NULL,
                                                                                  (void *)&error));

    err = ccmldsa_verify(ctx,
                         signatureWithoutContext.length, signatureWithoutContext.bytes,
                         messageData.length, messageData.bytes);
    XCTAssertEqual(err, 0, @"corecrypto verification of signature with context failed with error %d", err);

    // Test that only ML-DSA keys can be used
    NSDictionary *params = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeEd25519,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecUseDataProtectionKeychain : @YES
    };
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(privateKey, @"SEP key generation failed: %@", error);

    XCTAssertThrowsSpecificNamed(CFBridgingRelease(SecKeyCreateMLDSASignature((__bridge SecKeyRef)privateKey,
                                                                              kSecKeyAlgorithmMLDSASignatureMessage,
                                                                              (__bridge CFDataRef)messageData,
                                                                              (__bridge CFDictionaryRef)parameters,
                                                                              (void *)&error)),
                                 NSException,
                                 NSInvalidArgumentException,
                                 @"SecKeyCreateMLDSASignature should throw NSInvalidArgumentException when used with non-MLDSA keys");
}

#pragma mark - Helper methods

/// Helper method to verify key generation for a given size
- (void)verifyKeyGenerationWithSize:(id)keySize ccMLDSAParams:(const struct ccmldsa_params *)ccParams {
    NSDictionary *params = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeMLDSA,
        (id)kSecAttrKeySizeInBits: keySize,
    };

    NSError *error = nil;
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    XCTAssertNotNil(privKey, @"ML-DSA private key generation failed for size %@: %@", keySize, error.localizedDescription);

    NSDictionary *privAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)privKey));
    XCTAssertNotNil(privAttrs);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeyType], (id)kSecAttrKeyTypeMLDSA);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPrivate);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrCanSign], @YES);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrCanVerify], @NO);
    XCTAssertEqualObjects(privAttrs[(id)kSecAttrKeySizeInBits], keySize);
    XCTAssertNotNil(privAttrs[(id)kSecValueData]);
    XCTAssertNotEqualObjects(privAttrs[(id)kSecValueData], NSMutableData.data);
    XCTAssertEqual(SecKeyGetBlockSize((SecKeyRef)privKey), ccmldsa_privkey_nbytes_params(ccParams));
    XCTAssertEqual(SecKeyIsAlgorithmSupported((SecKeyRef)privKey, kSecKeyOperationTypeSign, kSecKeyAlgorithmMLDSASignatureMessage), YES);
    XCTAssertEqual(SecKeyIsAlgorithmSupported((SecKeyRef)privKey, kSecKeyOperationTypeVerify, kSecKeyAlgorithmMLDSASignatureMessage), NO);

    // Extract and verify public key attributes
    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    XCTAssertNotNil(pubKey);

    NSDictionary *pubAttrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)pubKey));
    XCTAssertNotNil(pubAttrs);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeyType], (id)kSecAttrKeyTypeMLDSA);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeyClass], (id)kSecAttrKeyClassPublic);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrCanSign], @NO);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrCanVerify], @YES);
    XCTAssertEqualObjects(pubAttrs[(id)kSecAttrKeySizeInBits], keySize);
    XCTAssertEqual(SecKeyGetBlockSize((SecKeyRef)pubKey), ccmldsa_pubkey_nbytes_params(ccParams));
    XCTAssertEqual(SecKeyIsAlgorithmSupported((SecKeyRef)pubKey, kSecKeyOperationTypeVerify, kSecKeyAlgorithmMLDSASignatureMessage), YES);
    XCTAssertEqual(SecKeyIsAlgorithmSupported((SecKeyRef)pubKey, kSecKeyOperationTypeSign, kSecKeyAlgorithmMLDSASignatureMessage), NO);

    NSData *data = pubAttrs[(id)kSecValueData];
    XCTAssertTrue(data.length > 0);

    XCTAssertEqualObjects(privAttrs[(id)kSecAttrApplicationLabel],
                          pubAttrs[(id)kSecAttrApplicationLabel]);
}

/// Helper method to perform signing and verification for a given key size
- (void)signAndVerifyWithKeySize:(NSNumber *)keySize algorithm:(SecKeyAlgorithm)algorithm {
    id privKey = [self generatePrivateKeyWithSize:keySize];
    XCTAssertNotNil(privKey, @"Failed to generate private key for signing and verification.");

    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));

    NSData *messageData = [@"Hello ML-DSA" dataUsingEncoding:NSUTF8StringEncoding];
    NSError *error = nil;

    NSData *signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privKey,
                                                        algorithm,
                                                        (__bridge CFDataRef)messageData,
                                                        (void *)&error));
    XCTAssertNotNil(signature, @"Signing failed: %@", error.localizedDescription);

    // Verify with SecKey
    BOOL isValid = SecKeyVerifySignature((SecKeyRef)pubKey,
                                         algorithm,
                                         (__bridge CFDataRef)messageData,
                                         (__bridge CFDataRef)signature,
                                         (void *)&error);
    XCTAssertTrue(isValid, @"Signature verification failed: %@", error.localizedDescription);

    // Verify with corecrypto
    ccmldsa_pub_ctx_t ctx = NULL;
    int err = importMLDSAPublicKey((__bridge SecKeyRef)(pubKey), &ctx);
    XCTAssertEqual(err, 0, @"Failed to import ML-DSA public key into ccmldsa_pub_ctx");

    err = ccmldsa_verify(ctx,
                         signature.length, signature.bytes,
                         messageData.length, messageData.bytes);
    XCTAssertEqual(err, 0, @"CoreCrypto verification failed with error %d", err);

    ccmldsa_pub_ctx_clear(ctx->params, ctx);
    free(ctx);
}

- (void)verifyImportExportPublicPrivateKeyWithSize:(id)keySize {
    NSError *error = nil;
    id privKey = [self generatePrivateKeyWithSize:keySize];
    XCTAssertNotNil(privKey, @"ML-DSA private key generation failed: %@", error.localizedDescription);

    NSData *privKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)privKey, (void *)&error));
    XCTAssertNotNil(privKeyData, @"Exporting ML-DSA private key failed: %@", error.localizedDescription);

    // Re-import private key
    NSDictionary *importParams = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeMLDSA,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrKeySizeInBits: keySize,
    };

    id privKeyImported = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)privKeyData,
                                                         (CFDictionaryRef)importParams,
                                                         (void *)&error));
    XCTAssertNotNil(privKeyImported, @"Re-importing ML-DSA private key failed: %@", error.localizedDescription);
    XCTAssertEqualObjects(privKey, privKeyImported, @"Private keys differ after import/export round-trip");

    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    XCTAssertNotNil(pubKey);

    NSData *pubKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)pubKey, (void *)&error));
    XCTAssertNotNil(pubKeyData, @"Exporting public key failed for import/export test: %@", error.localizedDescription);

    // Re-import public key
    NSDictionary *pubImportParams = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeMLDSA,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
        (id)kSecAttrKeySizeInBits: keySize,
    };

    id pubKeyImported = CFBridgingRelease(SecKeyCreateWithData((__bridge CFDataRef)pubKeyData,
                                                               (__bridge CFDictionaryRef)pubImportParams,
                                                               (void *)&error));

    XCTAssertNotNil(pubKeyImported, @"Re-importing ML-DSA public key failed: %@", error.localizedDescription);
    XCTAssertEqualObjects(pubKey, pubKeyImported, @"Public keys differ after import/export round-trip");
}

/// Helper method to verify that signature verification fails with corrupted signatures
- (void)verifySignatureFailureWithKeySize:(id)keySize {
    id privKey = [self generatePrivateKeyWithSize:keySize];
    XCTAssertNotNil(privKey);

    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    XCTAssertNotNil(pubKey);

    NSData *message = [@"Hello ML-DSA" dataUsingEncoding:NSUTF8StringEncoding];
    NSError *error = nil;
    NSData *signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)privKey,
                                                                kSecKeyAlgorithmMLDSASignatureMessage,
                                                                (__bridge CFDataRef)message,
                                                                (void *)&error));
    XCTAssertNotNil(signature, @"Signing failed: %@", error.localizedDescription);

    // Corrupt signature
    NSMutableData *badSignature = [signature mutableCopy];
    if (badSignature.length > 0) {
        ((uint8_t *)badSignature.mutableBytes)[0] ^= 0xFF; // Flip first byte
    } else {
        XCTFail(@"Signature data is empty, cannot corrupt.");
        return;
    }

    // Verify corrupted signature with SecKey
    BOOL isValid = SecKeyVerifySignature((SecKeyRef)pubKey,
                                         kSecKeyAlgorithmMLDSASignatureMessage,
                                         (__bridge CFDataRef)message,
                                         (__bridge CFDataRef)badSignature,
                                         (void *)&error);
    XCTAssertFalse(isValid, @"Corrupted signature should not verify.");

    // Verify corrupted signature with corecrypto
    ccmldsa_pub_ctx_t ctx = NULL;
    int err = importMLDSAPublicKey((__bridge SecKeyRef)(pubKey), &ctx);
    XCTAssertEqual(err, 0, @"Failed to import ML-DSA public key into ccmldsa_pub_ctx for negative testing.");

    err = ccmldsa_verify(ctx,
                         badSignature.length, badSignature.bytes,
                         message.length, message.bytes);
    XCTAssertNotEqual(err, 0, @"CoreCrypto verification should fail with corrupted signature.");

    ccmldsa_pub_ctx_clear(ctx->params, ctx);
    free(ctx);
}

/// Imports a SecKeyRef ML-DSA public key into a ccmldsa_pub_ctx_t context.
static int importMLDSAPublicKey(SecKeyRef pubSecKey, ccmldsa_pub_ctx_t *outPubCtx) {
    if (!pubSecKey || !outPubCtx) {
        return -1; // Invalid argument
    }

    CFErrorRef cfError = NULL;
    CFDataRef pubData = SecKeyCopyExternalRepresentation(pubSecKey, &cfError);
    if (!pubData) {
        CFReleaseSafe(cfError);
        return -2; // Failed to export raw public key
    }

    size_t pubLen = CFDataGetLength(pubData);
    const uint8_t *pubBytes = CFDataGetBytePtr(pubData);

    // Determine the appropriate parameter set based on the public key length
    const struct ccmldsa_params *params = NULL;
    if (pubLen == ccmldsa_pubkey_nbytes_params(ccmldsa65())) {
        params = ccmldsa65();
    } else if (pubLen == ccmldsa_pubkey_nbytes_params(ccmldsa87())) {
        params = ccmldsa87();
    } else {
        CFReleaseSafe(pubData);
        CFReleaseSafe(cfError);
        return -3; // Unknown/invalid public key size
    }

    // Allocate memory for the ML-DSA public context
    *outPubCtx = calloc(1, ccmldsa_sizeof_pub_ctx(params));
    ccmldsa_pub_ctx_init(*outPubCtx, params);

    if (*outPubCtx == NULL) {
        CFReleaseSafe(pubData);
        CFReleaseSafe(cfError);
        return -4; // Memory allocation failure
    }

    // Import the raw bytes into the allocated context
    int err = ccmldsa_import_pubkey(params, pubLen, pubBytes, *outPubCtx);

    // Release the pubData and cfError as they are no longer needed
    CFReleaseSafe(pubData);
    CFReleaseSafe(cfError);

    return err; // 0 on success, non-zero on failure
}

/// Generates a private ML-DSA key with the specified size
- (id)generatePrivateKeyWithSize:(id)keySize {
    NSDictionary *params = @{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeMLDSA,
        (id)kSecAttrKeySizeInBits: keySize,
    };

    NSError *error = nil;
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    if (!privKey) {
        XCTFail(@"Key generation failed for size %@: %@", keySize, error.localizedDescription);
    }
    return privKey;
}

@end
