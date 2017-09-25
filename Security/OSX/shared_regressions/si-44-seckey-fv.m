//
//  si-44-seckey-fv.m
//

#import <Foundation/Foundation.h>

#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR
#import "SecureKeyVaultPublic.h"
#import <Security/SecKey.h>

#import "shared_regressions.h"

static void testFileVaultKeyRawSign() {
    id key = CFBridgingRelease(SecKeyCreateWithSecureKeyVaultID(kCFAllocatorDefault, kSecureKeyVaultIAPAuthPrivateKey));
    id certificate = CFBridgingRelease(SecCertificateCreateWithSecureKeyVaultID(kCFAllocatorDefault, kSecureKeyVaultIAPAuthPrivateKey));
    id pubKey = CFBridgingRelease(SecCertificateCopyPublicKey((SecCertificateRef)certificate));

    uint8_t hash[20] = { 0 };
    uint8_t signature[256] = { 0 };
    size_t siglen = sizeof(signature);
    ok_status(SecKeyRawSign((SecKeyRef)key, kSecPaddingPKCS1SHA1, hash, sizeof(hash), signature, &siglen), "rawSign for fileVault failed");
    ok_status(SecKeyRawVerify((SecKeyRef)pubKey, kSecPaddingPKCS1SHA1, hash, sizeof(hash), signature, siglen), "rawverify for fileVault failed");
}

static void testFileVaultKeySign() {
    NSData *data = [@"dataToSign" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *signature;
    SecKeyAlgorithm algorithm = NULL;
    NSError *error;
    id key = CFBridgingRelease(SecKeyCreateWithSecureKeyVaultID(kCFAllocatorDefault, kSecureKeyVaultIAPAuthPrivateKey));
    id certificate = CFBridgingRelease(SecCertificateCreateWithSecureKeyVaultID(kCFAllocatorDefault, kSecureKeyVaultIAPAuthPrivateKey));
    id pubKey = CFBridgingRelease(SecCertificateCopyPublicKey((SecCertificateRef)certificate));

    algorithm = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1;
    error = nil;
    signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)key, algorithm, (CFDataRef)data, (void *)&error));
    ok(signature != NULL, "signing with alg %@ failed, err %@", algorithm, error);
    ok(SecKeyVerifySignature((SecKeyRef)pubKey, algorithm, (CFDataRef)data, (CFDataRef)signature, (void *)&error));

    algorithm = kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256;
    error = nil;
    signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)key, algorithm, (CFDataRef)data, (void *)&error));
    ok(signature != NULL, "signing with alg %@ failed, err %@", algorithm, error);
    ok(SecKeyVerifySignature((SecKeyRef)pubKey, algorithm, (CFDataRef)data, (CFDataRef)signature, (void *)&error));

    algorithm = kSecKeyAlgorithmRSASignatureMessagePSSSHA1;
    error = nil;
    signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)key, algorithm, (CFDataRef)data, (void *)&error));
    ok(signature != NULL, "signing with alg %@ failed, err %@", algorithm, error);
    ok(SecKeyVerifySignature((SecKeyRef)pubKey, algorithm, (CFDataRef)data, (CFDataRef)signature, (void *)&error));

    algorithm = kSecKeyAlgorithmRSASignatureMessagePSSSHA256;
    error = nil;
    signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)key, algorithm, (CFDataRef)data, (void *)&error));
    ok(signature != NULL, "signing with alg %@ failed, err %@", algorithm, error);
    ok(SecKeyVerifySignature((SecKeyRef)pubKey, algorithm, (CFDataRef)data, (CFDataRef)signature, (void *)&error));
}

int si_44_seckey_fv(int argc, char *const *argv) {
    @autoreleasepool {
        plan_tests(10);
        testFileVaultKeyRawSign();
        testFileVaultKeySign();
        return 0;
    }
}

#endif
