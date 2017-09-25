//
//  Copyright 2016 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <Security/SecKeyPriv.h>
#if !TARGET_OS_OSX
#import "MobileGestalt.h"
#endif

#import "shared_regressions.h"

static id generateKey(id keyType) {
    id accessControl = (__bridge_transfer id)SecAccessControlCreateWithFlags(NULL, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly, kSecAccessControlPrivateKeyUsage, NULL);
    NSDictionary *keyAttributes = @{ (id)kSecAttrTokenID : (id)kSecAttrTokenIDAppleKeyStore,
                                     (id)kSecAttrKeyType : keyType,
                                     (id)kSecAttrAccessControl : accessControl,
                                     (id)kSecAttrIsPermanent : @NO };
    NSError *error;
    id key = (__bridge_transfer id)SecKeyCreateRandomKey((CFDictionaryRef)keyAttributes, (void *)&error);
    ok(key, "failed to create random key %@", error);
    return key;
}

static void secKeySepTest(BOOL testPKA) {
    NSArray *keyTypes;
    if (testPKA) {
        keyTypes = @[(id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeyTypeECSECPrimeRandomPKA, (id)kSecAttrKeyTypeSecureEnclaveAttestation];
    } else {
        keyTypes = @[(id)kSecAttrKeyTypeECSECPrimeRandom, (id)kSecAttrKeyTypeSecureEnclaveAttestation];
    }
    for (id keyType in keyTypes) {
        id privateKey = generateKey((id)keyType);
        ok(privateKey, "failed to create key '%@'", keyType);
        id publicKey = (__bridge_transfer id)SecKeyCopyPublicKey((SecKeyRef)privateKey);

        NSArray *attestaionKeyTypes = @[@(kSecKeyAttestationKeyTypeSIK), @(kSecKeyAttestationKeyTypeGID)];
        for (NSNumber *attestationKeyType in attestaionKeyTypes) {
            id attestationKey = (__bridge_transfer id)SecKeyCopyAttestationKey([attestationKeyType unsignedIntValue], NULL);
            ok(attestationKey, "failed to create attestaion key '%@'", attestationKeyType);
            NSError *error;
            if (![keyType isEqual:(id)kSecAttrKeyTypeSecureEnclaveAttestation]) {
                const char rawData[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
                NSData *dataToSign = [NSData dataWithBytes:rawData length:sizeof(rawData)];
                NSData *signedData = (__bridge_transfer NSData*)SecKeyCreateSignature((SecKeyRef)privateKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)dataToSign, (void *)&error);
                ok(signedData, "failed to sign data, error %@", error);
                error = nil;
                ok(SecKeyVerifySignature((SecKeyRef)publicKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)dataToSign, (__bridge CFDataRef)signedData, (void *)&error),
                   "failed to verify data '%@'", error);

                // Try signing large data.
                dataToSign = [NSMutableData dataWithLength:10 * 1024 * 1024];
                error = nil;
                signedData = (__bridge_transfer NSData*)SecKeyCreateSignature((SecKeyRef)privateKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)dataToSign, (void *)&error);
                ok(signedData, "failed to sign data, error %@", error);
                error = nil;
                ok(SecKeyVerifySignature((SecKeyRef)publicKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)dataToSign, (__bridge CFDataRef)signedData, (void *)&error),
                   "failed to verify data '%@'", error);
            }
            NSData *attestationData = (__bridge_transfer NSData *)SecKeyCreateAttestation((__bridge SecKeyRef)attestationKey, (__bridge SecKeyRef)privateKey, (void *)&error);
            ok(attestationData, "failed to attest key '%@'", error);
        }

        NSDictionary *keyAttrs = (__bridge_transfer NSDictionary *)SecKeyCopyAttributes((SecKeyRef)privateKey);
        NSData *keyBlob = keyAttrs[(id)kSecAttrTokenOID];

        NSDictionary *params = @{ (id)kSecAttrTokenID : (id)kSecAttrTokenIDAppleKeyStore,
                                  (id)kSecAttrTokenOID : keyBlob,
                                  (id)kSecReturnRef : @YES };

        privateKey = nil;
        NSError *error;
        privateKey = (__bridge_transfer id)SecKeyCreateWithData((__bridge CFDataRef)keyBlob, (__bridge CFDictionaryRef)params, (void *)&error);
        ok(privateKey, "failed to create key with data '%@'", error);
//        <rdar://problem/30651629> SecItemAdd fails to create SecKey from aks key blob
//
//        ok_status(SecItemAdd((__bridge CFDictionaryRef)params, (void *)&privateKey), "failed to create key from aks blob");
//        ok_status(SecItemDelete((__bridge CFDictionaryRef) @{(id)kSecValueRef : privateKey}), "failed to delete key from aks blob");
    }
}

static void attestationTest(void) {
    NSError *error;
    id privKey = generateKey((id)kSecAttrKeyTypeECSECPrimeRandom);
    id uik = generateKey((id)kSecAttrKeyTypeSecureEnclaveAttestation);
    id sik = CFBridgingRelease(SecKeyCopyAttestationKey(kSecKeyAttestationKeyTypeSIK, (void *)&error));
    ok(sik != nil, "get SIk key: %@", error);

    error = nil;
    NSData *attSIKPlain = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)sik, (__bridge SecKeyRef)uik, (void *)&error));
    ok(attSIKPlain != nil, "SIK attesting UIK, no nonce: %@", error);

    error = nil;
    NSData *attUIKPlain = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)uik, (__bridge SecKeyRef)privKey, (void *)&error));
    ok(attUIKPlain != nil, "UIK attesting privKey, no nonce: %@", error);

    error = nil;
    NSData *nonce = [@"TESTnonce" dataUsingEncoding:NSUTF8StringEncoding];
    ok(SecKeySetParameter((__bridge SecKeyRef)sik, kSecKeyParameterSETokenAttestationNonce, (__bridge CFPropertyListRef)nonce, (void *)&error), "Set nonce to SIK: %@", error);
    NSData *attSIKNonce = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)sik, (__bridge SecKeyRef)uik, (void *)&error));
    ok(attSIKNonce != nil, "SIK attesting UIK, with nonce: %@", error);
//    NSRange found = [attSIKNonce rangeOfData:nonce options:0 range:NSMakeRange(0, attSIKNonce.length)];
//    ok(found.location != NSNotFound, "nonce found in SIK-attested data");

    error = nil;
    ok(SecKeySetParameter((__bridge SecKeyRef)uik, kSecKeyParameterSETokenAttestationNonce, (__bridge CFPropertyListRef)nonce, (void *)&error), "Set nonce to UIK: %@", error);
    NSData *attUIKNonce = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)uik, (__bridge SecKeyRef)privKey, (void *)&error));
    ok(attUIKNonce != nil, "SIK attesting UIK, with nonce: %@", error);
//    found = [attUIKNonce rangeOfData:nonce options:0 range:NSMakeRange(0, attUIKNonce.length)];
//    ok(found.location != NSNotFound, "nonce found in UIK-attested data");
}

int si_44_seckey_aks(int argc, char *const *argv) {
    @autoreleasepool {
        BOOL testPKA = YES;
#if !TARGET_OS_OSX
        id hasPKA = (__bridge_transfer id)MGCopyAnswer(kMGQHasPKA, NULL);
        if(![hasPKA isKindOfClass:NSNumber.class] || ![(NSNumber *)hasPKA boolValue]) {
            testPKA = NO;
        }
#else
        testPKA = NO;
#endif
        plan_tests(testPKA ? 46 : 31);
        secKeySepTest(testPKA);
        attestationTest();
        return 0;
    }
}
