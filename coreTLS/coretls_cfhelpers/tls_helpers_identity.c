//
//  tls_helpers_identity.c
//  coretls
//

#include <Security/Security.h>
#include <Security/SecKeyPriv.h>
#include <AssertMacros.h>
#include <tls_handshake.h>

#if TARGET_OS_IPHONE
#include <Security/oidsalg.h>
#include <Security/SecECKey.h>
#endif

#include <Security/SecCertificatePriv.h>

#include "sslMemory.h"

#include <tls_helpers.h>

#define sslErrorLog(...)

#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); CF = NULL; }
#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf);}


/* Private Key operations */
static
int mySSLPrivKeyRSA_sign(void *key, tls_hash_algorithm hash, const uint8_t *plaintext, size_t plaintextLen, uint8_t *sig, size_t *sigLen)
{
    SecKeyRef keyRef = key;
    SecKeyAlgorithm algo;
    switch (hash) {
        case tls_hash_algorithm_None:
            return SecKeyRawSign(keyRef, kSecPaddingPKCS1, plaintext, plaintextLen, sig, sigLen);
        case tls_hash_algorithm_SHA1:
            algo = kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1;
            break;
        case tls_hash_algorithm_SHA256:
            algo = kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256;
            break;
        case tls_hash_algorithm_SHA384:
            algo = kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384;
            break;
        case tls_hash_algorithm_SHA512:
            algo = kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512;
            break;
        default:
            /* Unsupported hash - Internal error */
            return errSSLInternal;
    }

    int err = errSSLInternal;
    CFDataRef signature = NULL;
    CFDataRef data = NULL;

    data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, plaintext, plaintextLen, kCFAllocatorNull);
    require(data, errOut);
    signature = SecKeyCreateSignature(keyRef, algo, data , NULL);
    require(signature, errOut);

    CFIndex len = CFDataGetLength(signature);
    const uint8_t *p = CFDataGetBytePtr(signature);

    require(p, errOut);
    require(len>=*sigLen, errOut);

    memcpy(sig, p, len);
    *sigLen = len;
    err = noErr;

errOut:
    CFReleaseSafe(data);
    CFReleaseSafe(signature);
    return err;
}

static
int mySSLPrivKeyRSA_decrypt(void *key, const uint8_t *ciphertext, size_t ciphertextLen, uint8_t *plaintext, size_t *plaintextLen)
{
    SecKeyRef keyRef = key;

    return SecKeyDecrypt(keyRef, kSecPaddingPKCS1, ciphertext, ciphertextLen, plaintext, plaintextLen);
}

static
int mySSLPrivKeyECDSA_sign(void *key, const uint8_t *plaintext, size_t plaintextLen, uint8_t *sig, size_t *sigLen)
{
    SecKeyRef keyRef = key;

    return SecKeyRawSign(keyRef, kSecPaddingPKCS1, plaintext, plaintextLen, sig, sigLen);
}


static OSStatus
parseIncomingCerts(CFArrayRef			certs,
                   SSLCertificate       **destCertChain, /* &ctx->{localCertChain,encryptCertChain} */
                   tls_private_key_t    *sslPrivKey)	 /* &ctx->signingPrivKeyRef, etc. */
{
    OSStatus			ortn;
    CFIndex				ix, numCerts;
    SecIdentityRef 		identity;
    SSLCertificate      *certChain = NULL;	/* Retained */
    SecCertificateRef	leafCert = NULL;	/* Retained */
    SecKeyRef           privKey = NULL;	/* Retained */

    assert(destCertChain != NULL);		/* though its referent may be NULL */
    assert(sslPrivKey != NULL);

    if (certs == NULL) {
        sslErrorLog("parseIncomingCerts: NULL incoming cert array\n");
        ortn = errSSLBadCert;
        goto errOut;
    }
    numCerts = CFArrayGetCount(certs);
    if (numCerts == 0) {
        sslErrorLog("parseIncomingCerts: empty incoming cert array\n");
        ortn = errSSLBadCert;
        goto errOut;
    }

    certChain=sslMalloc(numCerts*sizeof(SSLCertificate));
    if (!certChain) {
        ortn = errSecAllocate;
        goto errOut;
    }

    /*
     * Certs[0] is an SecIdentityRef from which we extract subject cert,
     * privKey, pubKey.
     *
     * 1. ensure the first element is a SecIdentityRef.
     */
    identity = (SecIdentityRef)CFArrayGetValueAtIndex(certs, 0);
    if (identity == NULL) {
        sslErrorLog("parseIncomingCerts: bad cert array (1)\n");
        ortn = errSecParam;
        goto errOut;
    }
    if (CFGetTypeID(identity) != SecIdentityGetTypeID()) {
        sslErrorLog("parseIncomingCerts: bad cert array (2)\n");
        ortn = errSecParam;
        goto errOut;
    }

    /*
     * 2. Extract cert, keys and convert to local format.
     */
    ortn = SecIdentityCopyCertificate(identity, &leafCert);
    if (ortn) {
        sslErrorLog("parseIncomingCerts: bad cert array (3)\n");
        goto errOut;
    }

    /* Fetch private key from identity */
    ortn = SecIdentityCopyPrivateKey(identity, &privKey);
    if (ortn) {
        sslErrorLog("parseIncomingCerts: SecIdentityCopyPrivateKey err %d\n",
                    (int)ortn);
        goto errOut;
    }

    /* Convert the input array of SecIdentityRef at the start to an array of
     all certificates. */
    SSLCopyBufferFromData(SecCertificateGetBytePtr(leafCert), SecCertificateGetLength(leafCert), &certChain[0].derCert);
    certChain[0].next = NULL;

    for (ix = 1; ix < numCerts; ++ix) {
        SecCertificateRef intermediate =
        (SecCertificateRef)CFArrayGetValueAtIndex(certs, ix);
        if (intermediate == NULL) {
            sslErrorLog("parseIncomingCerts: bad cert array (5)\n");
            ortn = errSecParam;
            goto errOut;
        }
        if (CFGetTypeID(intermediate) != SecCertificateGetTypeID()) {
            sslErrorLog("parseIncomingCerts: bad cert array (6)\n");
            ortn = errSecParam;
            goto errOut;
        }

        SSLCopyBufferFromData(SecCertificateGetBytePtr(intermediate), SecCertificateGetLength(intermediate), &certChain[ix].derCert);
        certChain[ix].next = NULL;
        certChain[ix-1].next = &certChain[ix];

    }

    size_t size = SecKeyGetBlockSize(privKey);
    tls_private_key_desc_t desc;

    if(SecKeyGetAlgorithmId(privKey) == kSecRSAAlgorithmID) {
        desc.type = tls_private_key_type_rsa;
        desc.rsa.sign = mySSLPrivKeyRSA_sign;
        desc.rsa.decrypt = mySSLPrivKeyRSA_decrypt;
        desc.rsa.size = SecKeyGetBlockSize(privKey);
    } else if (SecKeyGetAlgorithmId(privKey) == kSecECDSAAlgorithmID) {
        desc.type = tls_private_key_type_ecdsa;
        desc.ecdsa.sign = mySSLPrivKeyECDSA_sign;
        desc.ecdsa.curve = SecECKeyGetNamedCurve(privKey);
#if TARGET_OS_IPHONE
        /* Compute signature size from key size */
        desc.ecdsa.size  = 8+2*size;
#else
        desc.ecdsa.size  = size;
#endif
    } else {
        ortn = errSecParam;
        goto errOut;
    }
    *sslPrivKey = tls_private_key_create(&desc, privKey, (tls_private_key_ctx_release)&CFRelease);
    if(*sslPrivKey)
        ortn = errSecSuccess;
    else
        ortn = errSecAllocate;
    
    /* SUCCESS */
errOut:
    CFReleaseSafe(leafCert);

    if (ortn) {
        free(certChain);
        CFReleaseSafe(privKey);
        *destCertChain = NULL;
    } else {
        *destCertChain = certChain;
    }
    
    return ortn;
}

/* Free SSLCertificate array created by parseIncomingCerts */
static int freeCertificates(SSLCertificate *certs)
{
    SSLCertificate *head = certs;
    while(certs) {
        SSLFreeBuffer(&certs->derCert);
        certs = certs->next;
    }
    sslFree(head);
    return 0;
}

OSStatus
tls_helper_set_identity_from_array(tls_handshake_t hdsk, CFArrayRef certchain)
{
    SSLCertificate *certs;
    tls_private_key_t pkey;
    OSStatus ortn;

    ortn = parseIncomingCerts(certchain, &certs, &pkey);

    if(ortn == errSecSuccess) {
        CFRetain(tls_private_key_get_context(pkey)); // Required because tls_handshake_set_identity does a copy but do not properly retain key_ctx yet
        ortn = tls_handshake_set_identity(hdsk, certs, pkey);
        tls_private_key_destroy(pkey); // Should be changed to a release.
        freeCertificates(certs);
    }

    return ortn;
}
