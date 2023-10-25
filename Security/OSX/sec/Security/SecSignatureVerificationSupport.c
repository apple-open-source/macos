//
//  SecSignatureVerificationSupport.c
//  sec
//

#include <TargetConditionals.h>
#include <AssertMacros.h>
#include <Security/SecSignatureVerificationSupport.h>

#include <CoreFoundation/CFString.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>

#include <Security/SecBasePriv.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecECKeyPriv.h>

#include <corecrypto/ccn.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccder.h>
#include <libDER/DER_Keys.h>
#include <libDER/oids.h>

static const uint8_t *sec_decode_forced_uint(cc_size n,
    cc_unit *r, const uint8_t *der, const uint8_t *der_end)
{
    size_t len;
    der = ccder_decode_tl(CCDER_INTEGER, &len, der, der_end);
    if (der && ccn_read_uint(n, r, len, der) >= 0) {
        return der + len;
    }
    return NULL;
}

static CFErrorRef
SecCreateSignatureVerificationError(OSStatus errorCode, CFStringRef descriptionString)
{
    const CFStringRef defaultDescription = CFSTR("Error verifying signature.");
    const void* keys[1] = { kCFErrorDescriptionKey };
    const void* values[2] = { (descriptionString) ? descriptionString : defaultDescription };
    return CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault,
        kCFErrorDomainOSStatus, errorCode, keys, values, 1);
}

static CF_RETURNS_RETAINED CFDataRef
SecRecreateSignatureWithDERAlgorithmId(SecKeyRef publicKey, const DERAlgorithmId *algId,
    const uint8_t *oldSignature, size_t oldSignatureSize)
{
    if (!publicKey || !algId ||
        kSecECDSAAlgorithmID != SecKeyGetAlgorithmId(publicKey)) {
        // ECDSA SHA-256 is the only type of signature currently supported by this function
        return NULL;
    }

    cc_size n = ccec_cp_n(ccec_cp_256());
    cc_unit r[n], s[n];

    const uint8_t *oldSignatureEnd = oldSignature + oldSignatureSize;

    oldSignature = ccder_decode_sequence_tl(&oldSignatureEnd, oldSignature, oldSignatureEnd);
    oldSignature = sec_decode_forced_uint(n, r, oldSignature, oldSignatureEnd);
    oldSignature = sec_decode_forced_uint(n, s, oldSignature, oldSignatureEnd);
    if (!oldSignature || !(oldSignatureEnd == oldSignature)) {
        // failed to decode the old signature successfully
        return NULL;
    }


    size_t newSignatureSize = ccder_sizeof_integer(n, r) + ccder_sizeof_integer(n, s);
    newSignatureSize = ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, newSignatureSize);
    if (newSignatureSize < oldSignatureSize || newSignatureSize > oldSignatureSize + 5) {
        // shortest case is no-change, worst-case is that both integers get zero-padded,
        // plus size of each integer and sequence size increases by 1
        return NULL;
    }
    CFMutableDataRef encodedSig = CFDataCreateMutable(NULL, (CFIndex)newSignatureSize);
    CFDataSetLength(encodedSig, (CFIndex)newSignatureSize);
    uint8_t *outputPointer = CFDataGetMutableBytePtr(encodedSig);
    uint8_t *outputEndPointer = outputPointer + newSignatureSize;

    outputPointer = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE,
        outputEndPointer, outputPointer,
        ccder_encode_integer(n, r, outputPointer, ccder_encode_integer(n, s, outputPointer, outputEndPointer)));
    if (!outputPointer) {
        CFReleaseNull(encodedSig);
    }
    return encodedSig;
}

static SecKeyAlgorithm SecKeyAlgorithmFromDERAlgorithmId(SecKeyRef publicKey, const DERAlgorithmId *sigAlgId) {
    switch(SecKeyGetAlgorithmId(publicKey)) {
        case kSecRSAAlgorithmID: {
            if (DEROidCompare(&sigAlgId->oid, &oidMd5Rsa) ||
                DEROidCompare(&sigAlgId->oid, &oidMd5)) {
                return kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5;
            } else if (DEROidCompare(&sigAlgId->oid, &oidSha1Rsa) ||
                       DEROidCompare(&sigAlgId->oid, &oidSha1)) {
                return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1;
            } else if (DEROidCompare(&sigAlgId->oid, &oidSha224Rsa) ||
                       DEROidCompare(&sigAlgId->oid, &oidSha224)) {
                return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224;
            } else if (DEROidCompare(&sigAlgId->oid, &oidSha256Rsa) ||
                       DEROidCompare(&sigAlgId->oid, &oidSha256)) {
                return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256;
            } else if (DEROidCompare(&sigAlgId->oid, &oidSha384Rsa) ||
                       DEROidCompare(&sigAlgId->oid, &oidSha384)) {
                return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384;
            } else if (DEROidCompare(&sigAlgId->oid, &oidSha512Rsa) ||
                       DEROidCompare(&sigAlgId->oid, &oidSha512)) {
                return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512;
            }
            return NULL;
        }
        case kSecECDSAAlgorithmID:
            if (DEROidCompare(&sigAlgId->oid, &oidSha1Ecdsa) ||
                       DEROidCompare(&sigAlgId->oid, &oidSha1)) {
                return kSecKeyAlgorithmECDSASignatureMessageX962SHA1;
            } else if (DEROidCompare(&sigAlgId->oid, &oidSha224Ecdsa) ||
                       DEROidCompare(&sigAlgId->oid, &oidSha224)) {
                return kSecKeyAlgorithmECDSASignatureMessageX962SHA224;
            } else if (DEROidCompare(&sigAlgId->oid, &oidSha256Ecdsa) ||
                       DEROidCompare(&sigAlgId->oid, &oidSha256)) {
                return kSecKeyAlgorithmECDSASignatureMessageX962SHA256;
            } else if (DEROidCompare(&sigAlgId->oid, &oidSha384Ecdsa) ||
                       DEROidCompare(&sigAlgId->oid, &oidSha384)) {
                return kSecKeyAlgorithmECDSASignatureMessageX962SHA384;
            } else if (DEROidCompare(&sigAlgId->oid, &oidSha512Ecdsa) ||
                       DEROidCompare(&sigAlgId->oid, &oidSha512)) {
                return kSecKeyAlgorithmECDSASignatureMessageX962SHA512;
            }
            return NULL;
        case kSecEd25519AlgorithmID:
            return kSecKeyAlgorithmEdDSASignatureMessageCurve25519SHA512;
        case kSecEd448AlgorithmID:
            return kSecKeyAlgorithmEdDSASignatureMessageCurve448SHAKE256;
        default:
            return NULL;
    }
}

bool SecVerifySignatureWithPublicKey(SecKeyRef publicKey, const DERAlgorithmId *sigAlgId,
                                     const uint8_t *dataToHash, size_t amountToHash,
                                     const uint8_t *signatureStart, size_t signatureSize,
                                     CFErrorRef *error)
{
    bool result = false;
    OSStatus errorCode = errSecParam;
    CFDataRef data = NULL, signature = NULL;
    require(amountToHash < LONG_MAX && signatureSize < LONG_MAX, fail);

    SecKeyAlgorithm alg = SecKeyAlgorithmFromDERAlgorithmId(publicKey, sigAlgId);
    data = CFDataCreate(NULL, dataToHash, (CFIndex)amountToHash);
    signature = CFDataCreate(NULL, signatureStart, (CFIndex)signatureSize);
    require_quiet(alg && data && signature, fail);

    result = SecKeyVerifySignature(publicKey, alg, data, signature, error);

    if (!result) {
        // fallback to potentially fix signatures with missing zero-byte padding.
        CFReleaseNull(signature);
        signature = SecRecreateSignatureWithDERAlgorithmId(publicKey, sigAlgId, signatureStart, signatureSize);
        require_quiet(signature, fail);

        if (error) {
            CFReleaseNull(*error);
        }
        result = SecKeyVerifySignature(publicKey, alg, data, signature, error);
    }

fail:
    CFReleaseNull(data);
    CFReleaseNull(signature);
    if (!result && error && !*error) {
        // Create a default error
        *error = SecCreateSignatureVerificationError(errorCode, CFSTR("Unable to verify signature"));
    }
    return result;
}
