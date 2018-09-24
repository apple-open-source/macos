//
//  Copyright 2016 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <Security/SecKeyPriv.h>
#if !TARGET_OS_OSX
#import "MobileGestalt.h"
#else
#import <RemoteServiceDiscovery/RemoteServiceDiscovery.h>
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

    id pubSIK = CFBridgingRelease(SecKeyCopyPublicKey((__bridge SecKeyRef)sik));
    ok(pubSIK != nil, "get SIK pubkey");

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

    error = nil;
    ok(SecKeySetParameter((__bridge SecKeyRef)uik, kSecKeyParameterSETokenAttestationNonce, (__bridge CFPropertyListRef)nonce, (void *)&error), "Set nonce to UIK: %@", error);
    NSData *attUIKNonce = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)uik, (__bridge SecKeyRef)privKey, (void *)&error));
    ok(attUIKNonce != nil, "SIK attesting UIK, with nonce: %@", error);

    error = nil;
    id sysUikC = CFBridgingRelease(SecKeyCopyAttestationKey(kSecKeyAttestationKeyTypeUIKCommitted, (void *)&error));
    if (sysUikC == nil) {
        // Platform does not support system UIK, so just fake test rounds to avoid testplan counting failures.
        for (int i = 0; i < 19; i++) {
            ok(true);
        }
    } else {
        ok(sysUikC != nil, "get UIK-committed key, error: %@", error);
        error = nil;
        id sysUikP = CFBridgingRelease(SecKeyCopyAttestationKey(kSecKeyAttestationKeyTypeUIKProposed, (void *)&error));
        ok(sysUikP != nil, "get UIK-proposed key: %@", error);

        error = nil;
        NSData *attUIKC = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)sysUikC, (__bridge SecKeyRef)privKey, (void *)&error));
        ok(attUIKC != nil, "Sys-UIK-committed attesting privKey: %@", error);

        error = nil;
        NSData *attUIKP = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)sysUikP, (__bridge SecKeyRef)privKey, (void *)&error));
        ok(attUIKP != nil, "Sys-UIK-proposed attesting privKey: %@", error);

        id pubUIKP = CFBridgingRelease(SecKeyCopyPublicKey((__bridge SecKeyRef)sysUikP));
        ok(pubUIKP != nil, "Sys-UIK-proposed copy public key");
        id pubUIKC = CFBridgingRelease(SecKeyCopyPublicKey((__bridge SecKeyRef)sysUikC));
        ok(pubUIKC != nil, "Sys-UIK-proposed copy public key");
        ok([pubUIKP isEqual:pubUIKC], "Sys-UIK proposed and committed are same before bump");

        BOOL res = SecKeyControlLifetime((__bridge SecKeyRef)sysUikC, kSecKeyControlLifetimeTypeBump, (void *)&error);
        ok(res, "bumping sys-uik: %@", error);

        error = nil;
        NSData *attUIKCN = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)sysUikC, (__bridge SecKeyRef)privKey, (void *)&error));
        ok(attUIKCN != nil, "Sys-UIK-committed attesting privKey: %@", error);

        error = nil;
        NSData *attUIKPN = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)sysUikP, (__bridge SecKeyRef)privKey, (void *)&error));
        ok(attUIKPN != nil, "Sys-UIK-proposed attesting privKey: %@", error);

        id pubUIKPN = CFBridgingRelease(SecKeyCopyPublicKey((__bridge SecKeyRef)sysUikP));
        ok(pubUIKPN != nil, "Sys-UIK-proposed copy public key");
        ok(![pubUIKPN isEqual:pubUIKC], "Sys-UIK proposed and committed differ after bump");

        res = SecKeyControlLifetime((__bridge SecKeyRef)sysUikP, kSecKeyControlLifetimeTypeCommit, (void *)&error);
        ok(res, "committing sys-uik: %@", error);

        error = nil;
        NSData *attUIKCNN = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)sysUikC, (__bridge SecKeyRef)privKey, (void *)&error));
        ok(attUIKCNN != nil, "Sys-UIK-committed attesting privKey: %@", error);

        error = nil;
        NSData *attUIKPNN = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)sysUikP, (__bridge SecKeyRef)privKey, (void *)&error));
        ok(attUIKPNN != nil, "Sys-UIK-proposed attesting privKey: %@", error);

        id pubUIKCN = CFBridgingRelease(SecKeyCopyPublicKey((__bridge SecKeyRef)sysUikC));
        ok(pubUIKCN != nil, "Sys-UIK-committed copy public key");
        ok([pubUIKPN isEqual:pubUIKC], "Sys-UIK proposed and committed same after commit");

        // Attest system-UIK with SIK
        NSData *attSIKUIKP = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)sik, (__bridge SecKeyRef)sysUikP, (void *)&error));
        ok(attSIKUIKP != nil, "SIK attesting Sys-UIK-proposed, error: %@", error);

        NSData *attSIKUIKC = CFBridgingRelease(SecKeyCreateAttestation((__bridge SecKeyRef)sik, (__bridge SecKeyRef)sysUikC, (void *)&error));
        ok(attSIKUIKC != nil, "SIK attesting Sys-UIK-committed, error: %@", error);
    }
}

int si_44_seckey_aks(int argc, char *const *argv) {
    @autoreleasepool {
        BOOL testPKA = YES;
#if !TARGET_OS_OSX
        NSNumber *hasPKA = (__bridge_transfer id)MGCopyAnswer(kMGQHasPKA, NULL);
        if(![hasPKA isKindOfClass:NSNumber.class] || ![hasPKA boolValue]) {
            testPKA = NO;
        }
#else
        if (remote_device_copy_unique_of_type(REMOTE_DEVICE_TYPE_EOS) == nil && remote_device_copy_unique_of_type(REMOTE_DEVICE_TYPE_BRIDGE_COPROC) == nil) {
            // macOS without SEP cannot run attestations at all.
            plan_tests(1);
            ok(true);
            return 0;
        }

        testPKA = NO;
#endif
        plan_tests(testPKA ? 66 : 51);
        secKeySepTest(testPKA);
        attestationTest();
        return 0;
    }
}
