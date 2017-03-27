/*
 * crypto.h - public data structures and prototypes for the crypto library
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "cryptohi.h"

#include "secoid.h"
#include "cmspriv.h"
#include <security_asn1/secerr.h>
#include <Security/cssmapi.h>
#include <Security/cssmapi.h>
#include <Security/SecKeyPriv.h>
#include <Security/cssmapple.h>
#include <Security/SecItem.h>

#ifdef	NDEBUG
#define CSSM_PERROR(f, r)
#define dprintf(args...)
#else
#define CSSM_PERROR(f, r)   cssmPerror(f, r)
#define dprintf(args...)    fprintf(stderr, args)
#endif

static CSSM_CSP_HANDLE gCsp = 0;
static char gCssmInitialized = 0;

/* @@@ Ugly hack casting, but the extra argument at the end will be ignored. */
static CSSM_API_MEMORY_FUNCS memFuncs =
{
    (CSSM_MALLOC)malloc,
    (CSSM_FREE)free,
    (CSSM_REALLOC)realloc,
    (CSSM_CALLOC)calloc,
    NULL
};

/*
 *
 * SecCspHandleForAlgorithm
 * @@@ This function should get more parameters like keysize and operation required and use mds.
 *
 */
CSSM_CSP_HANDLE
SecCspHandleForAlgorithm(CSSM_ALGORITHMS algorithm)
{

    if (!gCsp)
    {
	CSSM_VERSION version = { 2, 0 };
	CSSM_RETURN rv;

	if (!gCssmInitialized)
	{
	    CSSM_GUID myGuid = { 0xFADE, 0, 0, { 1, 2, 3, 4, 5, 6, 7, 0 } };
	    CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;
    
	    rv = CSSM_Init (&version, CSSM_PRIVILEGE_SCOPE_NONE, &myGuid, CSSM_KEY_HIERARCHY_NONE, &pvcPolicy, NULL);
	    if (rv)
		goto loser;
	    gCssmInitialized = 1;
	}

	rv = CSSM_ModuleLoad(&gGuidAppleCSP, CSSM_KEY_HIERARCHY_NONE, NULL, NULL);
	if (rv)
	    goto loser;
	rv = CSSM_ModuleAttach(&gGuidAppleCSP, &version, &memFuncs, 0, CSSM_SERVICE_CSP, 0, CSSM_KEY_HIERARCHY_NONE, NULL, 0, NULL, &gCsp);
    }

loser:
    return gCsp;
}

OSStatus cmsNullWrapKey(SecKeyRef refKey,
                               CSSM_KEY_PTR rawKey)
{
    CSSM_DATA descData = {0, 0};
    CSSM_RETURN crtn;
    CSSM_CC_HANDLE ccHand;
    CSSM_ACCESS_CREDENTIALS creds;
    CSSM_CSP_HANDLE refCspHand = CSSM_INVALID_HANDLE;
    const CSSM_KEY *cssmKey = NULL;
    uint32 keyAttr;

    memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
    memset(rawKey, 0, sizeof(CSSM_KEY));

    crtn = SecKeyGetCSSMKey(refKey, &cssmKey);
    if(crtn) {
        CSSM_PERROR("SecKeyGetCSSMKey", crtn);
        goto loser;
    }
    crtn = SecKeyGetCSPHandle(refKey, &refCspHand);
    if(crtn) {
        CSSM_PERROR("SecKeyGetCSPHandle", crtn);
        goto loser;
    }

    crtn = CSSM_CSP_CreateSymmetricContext(refCspHand,
                                           CSSM_ALGID_NONE,
                                           CSSM_ALGMODE_NONE,
                                           &creds,
                                           NULL,			// unwrappingKey
                                           NULL,			// initVector
                                           CSSM_PADDING_NONE,
                                           0,				// Params
                                           &ccHand);
    if(crtn) {
        CSSM_PERROR("CSSM_CSP_CreateSymmetricContext", crtn);
        return crtn;
    }

    keyAttr = rawKey->KeyHeader.KeyAttr;
    keyAttr &= ~(CSSM_KEYATTR_ALWAYS_SENSITIVE | CSSM_KEYATTR_NEVER_EXTRACTABLE |
                 CSSM_KEYATTR_MODIFIABLE);
    keyAttr |= CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
    crtn = CSSM_WrapKey(ccHand,
                        &creds,
                        cssmKey,
                        &descData,
                        rawKey);
    if(crtn != CSSM_OK) {
        CSSM_PERROR("CSSM_WrapKey", crtn);
    }
    CSSM_DeleteContext(ccHand);

loser:
    return crtn;
}

CSSM_ALGORITHMS
SECOID_FindyCssmAlgorithmByTag(SECOidTag algTag)
{
    const SECOidData *oidData = SECOID_FindOIDByTag(algTag);
    return oidData ? oidData->cssmAlgorithm : CSSM_ALGID_NONE;
}


static void SEC_PrintCFError(CFErrorRef CF_RELEASES_ARGUMENT error) {
    if (error) {
    CFStringRef errorDesc = CFErrorCopyDescription(error);
    dprintf("SecKey API returned: %ld, %s", CFErrorGetCode(error),
            errorDesc ? CFStringGetCStringPtr(errorDesc, kCFStringEncodingUTF8) : "");
    CFRelease(error);
    if (errorDesc) { CFRelease(errorDesc); }
    }

}

/* The new SecKey API has made this very painful */
static SecKeyAlgorithm SECOID_FindSecKeyAlgorithmByTags(SECOidTag sigAlgTag, SECOidTag digAlgTag, bool isDigest) {
    switch(sigAlgTag) {
        case(SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION):
            if (digAlgTag == SEC_OID_MD5) {
                return ((isDigest) ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5 :
                        kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5);
            }
            break;
        case(SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION):
            if (digAlgTag == SEC_OID_SHA1) {
                return ((isDigest) ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1
                        : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1);
            }
            break;
        case(SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION):
            if (digAlgTag == SEC_OID_SHA256) {
                return ((isDigest) ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256
                        : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256);
            }
            break;
        case(SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION):
            if (digAlgTag == SEC_OID_SHA384) {
                return ((isDigest) ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384
                        : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384);
            }
            break;
        case(SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION):
            if (digAlgTag == SEC_OID_SHA512) {
                return ((isDigest) ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512
                        : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512);
            }
            break;
        case(SEC_OID_PKCS1_RSA_ENCRYPTION):
            switch (digAlgTag) {
                case (SEC_OID_MD5):
                    return ((isDigest) ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5 :
                            kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5);
                case(SEC_OID_SHA1):
                    return ((isDigest) ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1
                            : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1);
                case(SEC_OID_SHA256):
                    return ((isDigest) ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256
                            : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256);
                case(SEC_OID_SHA384):
                    return ((isDigest) ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384
                            : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384);
                case(SEC_OID_SHA512):
                    return ((isDigest) ? kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512
                            : kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512);
                default:
                    return NULL;
            }
        case(SEC_OID_ECDSA_WithSHA1):
            if (digAlgTag == SEC_OID_SHA1) {
                return ((isDigest) ? kSecKeyAlgorithmECDSASignatureDigestX962
                        : kSecKeyAlgorithmECDSASignatureMessageX962SHA1);
            }
            break;
        case(SEC_OID_ECDSA_WITH_SHA256):
            if (digAlgTag == SEC_OID_SHA256) {
                return ((isDigest) ? kSecKeyAlgorithmECDSASignatureDigestX962
                        : kSecKeyAlgorithmECDSASignatureMessageX962SHA256);
            }
            break;
        case(SEC_OID_ECDSA_WITH_SHA384):
            if (digAlgTag == SEC_OID_SHA384) {
                return ((isDigest) ? kSecKeyAlgorithmECDSASignatureDigestX962
                        : kSecKeyAlgorithmECDSASignatureMessageX962SHA384);
            }
            break;
        case(SEC_OID_ECDSA_WITH_SHA512):
            if (digAlgTag == SEC_OID_SHA512) {
                return ((isDigest) ? kSecKeyAlgorithmECDSASignatureDigestX962
                        : kSecKeyAlgorithmECDSASignatureMessageX962SHA512);
            }
            break;
        case(SEC_OID_EC_PUBLIC_KEY):
        case(SEC_OID_SECP_256_R1):
        case(SEC_OID_SECP_384_R1):
        case(SEC_OID_SECP_521_R1):
            switch (digAlgTag) {
                case(SEC_OID_SHA1):
                    return ((isDigest) ? kSecKeyAlgorithmECDSASignatureDigestX962
                            : kSecKeyAlgorithmECDSASignatureMessageX962SHA1);
                case(SEC_OID_SHA256):
                    return ((isDigest) ? kSecKeyAlgorithmECDSASignatureDigestX962
                            : kSecKeyAlgorithmECDSASignatureMessageX962SHA256);
                case(SEC_OID_SHA384):
                    return ((isDigest) ? kSecKeyAlgorithmECDSASignatureDigestX962
                            : kSecKeyAlgorithmECDSASignatureMessageX962SHA384);
                case(SEC_OID_SHA512):
                    return ((isDigest) ? kSecKeyAlgorithmECDSASignatureDigestX962
                            : kSecKeyAlgorithmECDSASignatureMessageX962SHA512);
                default:
                    return NULL;
            }
        default:
            return NULL;
    }
    return NULL;
}

CFStringRef SECOID_CopyKeyTypeByTag(SECOidTag tag) {
    CFStringRef keyType = NULL;

    switch(tag) {
        case(SEC_OID_RC2_CBC):
        case(SEC_OID_CMS_RC2_KEY_WRAP):
            keyType = kSecAttrKeyTypeRC2;
            break;
        case(SEC_OID_RC4):
            keyType = kSecAttrKeyTypeRC4;
            break;
        case(SEC_OID_DES_ECB):
        case(SEC_OID_DES_CBC):
        case(SEC_OID_DES_OFB):
        case(SEC_OID_DES_CFB):
            keyType = kSecAttrKeyTypeDES;
            break;
        case(SEC_OID_DES_EDE):
        case(SEC_OID_DES_EDE3_CBC):
        case(SEC_OID_CMS_3DES_KEY_WRAP):
            keyType = kSecAttrKeyType3DES;
            break;
        case(SEC_OID_AES_128_ECB):
        case(SEC_OID_AES_128_CBC):
        case(SEC_OID_AES_192_ECB):
        case(SEC_OID_AES_192_CBC):
        case(SEC_OID_AES_256_ECB):
        case(SEC_OID_AES_256_CBC):
        case(SEC_OID_AES_128_KEY_WRAP):
        case(SEC_OID_AES_192_KEY_WRAP):
        case(SEC_OID_AES_256_KEY_WRAP):
            keyType = kSecAttrKeyTypeAES;
            break;
        default:
            keyType = NULL;
    }

    return keyType;
}

static SECStatus SGN_SignAll(uint8_t *buf, size_t len,
                             SecPrivateKeyRef pk, SECItem *resultSignature,
                             SECOidTag digAlgTag, SECOidTag sigAlgTag,
                             bool isDigest) {
    OSStatus rv  = SECFailure;
    CFDataRef signature = NULL, dataToSign = NULL;
    CFErrorRef error = NULL;
    SecKeyAlgorithm keyAlg = NULL;

    keyAlg = SECOID_FindSecKeyAlgorithmByTags(sigAlgTag, digAlgTag, isDigest);

    /* we no longer support signing with MD5 */
    if (keyAlg == kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5 ||
        keyAlg == kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5) {
        dprintf("CMS signature failed: MD5 algorithm is disallowed for generating signatures.");
        rv = SEC_ERROR_INVALID_ALGORITHM;
        goto out;
    }

    if (keyAlg == NULL) {
        rv = SEC_ERROR_INVALID_ALGORITHM;
        goto out;
    }

    dataToSign = CFDataCreate(NULL, buf, len);
    if (!dataToSign) {
        goto out;
    }

    signature = SecKeyCreateSignature(pk, keyAlg, dataToSign, &error);
    if (!signature) {
        goto out;
    }

    CFIndex signatureLength = CFDataGetLength(signature);
    if (signatureLength < 0 || signatureLength > 1024) {
        goto out;
    }
    resultSignature->Data = (uint8_t *)malloc(signatureLength);
    if (!resultSignature->Data) {
        goto out;
    }

    memcpy(resultSignature->Data, CFDataGetBytePtr(signature), signatureLength);
    resultSignature->Length = signatureLength;
    rv = SECSuccess;

out:
    if (signature) { CFRelease(signature); }
    if (dataToSign) {CFRelease(dataToSign); }
    SEC_PrintCFError(error);
    if (rv) {
        PORT_SetError(rv);
    }
    return rv;
}

SECStatus
SEC_SignData(SECItem *result, unsigned char *buf, int len,
	    SecPrivateKeyRef pk, SECOidTag digAlgTag, SECOidTag sigAlgTag)
{
    return SGN_SignAll(buf, len, pk, result, digAlgTag, sigAlgTag, false);
}

SECStatus
SGN_Digest(SecPrivateKeyRef pk, SECOidTag digAlgTag, SECOidTag sigAlgTag, SECItem *result, SECItem *digest)
{
    return SGN_SignAll(digest->Data, digest->Length, pk, result, digAlgTag, sigAlgTag, true);
}

static SECStatus VFY_VerifyAll(uint8_t *buf, size_t len,
                     SecPublicKeyRef pk, SECItem *sig,
                     SECOidTag digAlgTag, SECOidTag sigAlgTag,
                     bool isDigest) {
    OSStatus rv = SECFailure;
    CFDataRef signature = NULL, data = NULL;
    CFErrorRef error = NULL;
    SecKeyAlgorithm keyAlg = NULL;

    signature = CFDataCreate(NULL, sig->Data, sig->Length);
    data = CFDataCreate(NULL, buf, len);
    if (!signature || !data) {
        goto out;
    }

    keyAlg = SECOID_FindSecKeyAlgorithmByTags(sigAlgTag, digAlgTag, isDigest);
    if (keyAlg == NULL) {
        rv = SEC_ERROR_INVALID_ALGORITHM;
        goto out;
    }

    if(SecKeyVerifySignature(pk, keyAlg, data, signature, &error)) {
        rv = SECSuccess;
    }

out:
    if (signature) { CFRelease(signature); }
    if (data) { CFRelease(data); }
    SEC_PrintCFError(error);
    if (rv) {
        PORT_SetError(rv);
    }
    return rv;
}

SECStatus
VFY_VerifyData(unsigned char *buf, int len,
		SecPublicKeyRef pk, SECItem *sig,
		SECOidTag digAlgTag, SECOidTag sigAlgTag, void *wincx)
{
    return VFY_VerifyAll(buf, len, pk, sig,
                         digAlgTag, sigAlgTag, false);
}

SECStatus
VFY_VerifyDigest(SECItem *digest, SecPublicKeyRef pk,
		SECItem *sig, SECOidTag digAlgTag, SECOidTag sigAlgTag, void *wincx)
{
    return VFY_VerifyAll(digest->Data, digest->Length, pk, sig,
                         digAlgTag, sigAlgTag, true);
}

SECStatus
WRAP_PubWrapSymKey(SecPublicKeyRef publickey,
		   SecSymmetricKeyRef bulkkey,
		   CSSM_DATA_PTR encKey)
{
    OSStatus rv;
    CSSM_KEY bk;

    rv = cmsNullWrapKey(bulkkey, &bk);
    if (rv) {
        return rv;
    }

    return SecKeyEncrypt(publickey, kSecPaddingPKCS1,
                         bk.KeyData.Data, bk.KeyData.Length,
                         encKey->Data, &encKey->Length);
}


SecSymmetricKeyRef
WRAP_PubUnwrapSymKey(SecPrivateKeyRef privkey, CSSM_DATA_PTR encKey, SECOidTag bulkalgtag)
{
    CFDataRef encryptedKey = NULL, bulkkey = NULL;
    CFMutableDictionaryRef keyparams = NULL;
    CFStringRef keyType = NULL;
    CFErrorRef error = NULL;
    SecSymmetricKeyRef bk = NULL;

    /* decrypt the key */
    encryptedKey = CFDataCreate(NULL, encKey->Data, encKey->Length);
    if (!encryptedKey) {
        goto out;
    }

    bulkkey = SecKeyCreateDecryptedData(privkey, kSecKeyAlgorithmRSAEncryptionPKCS1, encryptedKey, &error);
    if (!bulkkey) {
        goto out;
    }

    /* create the SecSymmetricKeyRef */
    keyType = SECOID_CopyKeyTypeByTag(bulkalgtag);
    if (!keyType) {
        goto out;
    }

    keyparams = CFDictionaryCreateMutable(NULL, 1,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
    if (!keyparams) {
        goto out;
    }

    CFDictionaryAddValue(keyparams, kSecAttrKeyType, keyType);
    bk = SecKeyCreateFromData(keyparams, bulkkey, NULL);

out:
    if (encryptedKey) { CFRelease(encryptedKey); }
    if (bulkkey) { CFRelease(bulkkey); }
    if (keyparams) { CFRelease(keyparams); }
    if (keyType) { CFRelease(keyType); }
    SEC_PrintCFError(error);
    return bk;
}
