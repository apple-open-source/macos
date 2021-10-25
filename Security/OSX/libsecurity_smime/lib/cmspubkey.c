/*
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

/*
 * CMS public key crypto
 */

#include "cmslocal.h"

#include "cryptohi.h"
#include "secitem.h"
#include "secoid.h"

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonRandom.h>
#include <CommonCrypto/CommonRandomSPI.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCmsBase.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/Security.h>
#include <Security/keyTemplates.h>
#include <Security/secasn1t.h>
#include <security_asn1/plarenas.h>
#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <utilities/SecCFWrappers.h>

/* ====== RSA ======================================================================= */

/*
 * SecCmsUtilEncryptSymKeyRSA - wrap a symmetric key with RSA
 *
 * this function takes a symmetric key and encrypts it using an RSA public key
 * according to PKCS#1 and RFC2633 (S/MIME)
 */
OSStatus SecCmsUtilEncryptSymKeyRSA(PLArenaPool* poolp,
                                    SecCertificateRef cert,
                                    SecSymmetricKeyRef bulkkey,
                                    CSSM_DATA_PTR encKey)
{
    OSStatus rv;
    SecPublicKeyRef publickey = SecCertificateCopyKey(cert);
    if (publickey == NULL) {
        return SECFailure;
    }

    rv = SecCmsUtilEncryptSymKeyRSAPubKey(poolp, publickey, bulkkey, encKey);
    CFReleaseNull(publickey);
    return rv;
}

OSStatus SecCmsUtilEncryptSymKeyRSAPubKey(PLArenaPool* poolp,
                                          SecPublicKeyRef publickey,
                                          SecSymmetricKeyRef bulkkey,
                                          CSSM_DATA_PTR encKey)
{
    OSStatus rv;
    unsigned int data_len;
    //KeyType keyType;
    void* mark = NULL;
    CFDictionaryRef theirKeyAttrs = NULL;

    mark = PORT_ArenaMark(poolp);
    if (!mark) {
        goto loser;
    }
    /* allocate memory for the encrypted key */
    theirKeyAttrs = SecKeyCopyAttributes(publickey);
    if (!theirKeyAttrs) {
        goto loser;
    }

    CFNumberRef keySizeNum = CFDictionaryGetValue(theirKeyAttrs, kSecAttrKeySizeInBits);
    if (!CFNumberGetValue(keySizeNum, kCFNumberIntType, &data_len)) {
        goto loser;
    }
    // Convert length to bytes;
    data_len /= 8;

    encKey->Data = (unsigned char*)PORT_ArenaAlloc(poolp, data_len);
    encKey->Length = data_len;
    if (encKey->Data == NULL) {
        goto loser;
    }

    /* encrypt the key now */
    rv = WRAP_PubWrapSymKey(publickey, bulkkey, encKey);
    if (rv != SECSuccess)
        goto loser;

    PORT_ArenaUnmark(poolp, mark);
    CFReleaseNull(theirKeyAttrs);
    return SECSuccess;

loser:
    CFReleaseNull(theirKeyAttrs);
    if (mark) {
        PORT_ArenaRelease(poolp, mark);
    }
    return SECFailure;
}

/*
 * SecCmsUtilDecryptSymKeyRSA - unwrap a RSA-wrapped symmetric key
 *
 * this function takes an RSA-wrapped symmetric key and unwraps it, returning a symmetric
 * key handle. Please note that the actual unwrapped key data may not be allowed to leave
 * a hardware token...
 */
SecSymmetricKeyRef SecCmsUtilDecryptSymKeyRSA(SecPrivateKeyRef privkey, CSSM_DATA_PTR encKey, SECOidTag bulkalgtag)
{
    /* that's easy */
    return WRAP_PubUnwrapSymKey(privkey, encKey, bulkalgtag);
}

#define CFRELEASE(cf)  \
    if (cf != NULL) {  \
        CFRelease(cf); \
    }

/* ====== ECDH (Ephemeral-Static Diffie-Hellman) ==================================== */

#pragma mark---- ECDH support functions ----

#ifdef NDEBUG
#define CSSM_PERROR(f, r)
#define dprintf(args...)
#else
#define CSSM_PERROR(f, r) cssmPerror(f, r)
#define dprintf(args...) fprintf(stderr, args)
#endif

/* Length of KeyAgreeRecipientInfo.ukm we create */
#define UKM_LENGTH 8

/* KEK algorithm info we generate */
#define ECDH_KEK_ALG_TAG SEC_OID_DES_EDE3_CBC
#define ECDH_KEK_KEY_CSSM_ALGID CSSM_ALGID_3DES_3KEY
#define ECDH_KEK_ENCR_CSSM_ALGID CSSM_ALGID_3DES_3KEY_EDE
#define ECDH_KEK_KEY_LEN_BYTES 24
#define ECDH_KEK_IV_LEN_BYTES 8

#define CMS_DUMP_BUFS 0
#if CMS_DUMP_BUFS

static void dumpBuf(const char* label, const CSSM_DATA* cd)
{
    unsigned dex;

    printf("%s:\n   ", label);
    for (dex = 0; dex < cd->Length; dex++) {
        printf("%02X ", cd->Data[dex]);
        if (((dex % 16) == 15) && (dex != (cd->Length - 1))) {
            printf("\n   ");
        }
    }
    putchar('\n');
}

#else
#define dumpBuf(l, d)
#endif /* CMS_DUMP_BUFS */

/* 
 * The ECC-CMS-SharedInfo struct, as defined in RFC 3278 8.2, and the 
 * template for DER encoding and decoding it. 
 */
typedef struct {
    SECAlgorithmID algId;  /* KEK alg, NULL params */
    CSSM_DATA entityUInfo; /* optional, ukm */
    CSSM_DATA suppPubInfo; /* length of KEK in bits as 4-byte integer */
} ECC_CMS_SharedInfo;

static const SecAsn1Template ECC_CMS_SharedInfoTemplate[] = {
    {SEC_ASN1_SEQUENCE, 0, NULL, sizeof(ECC_CMS_SharedInfo)},
    {SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC | 0,
     offsetof(ECC_CMS_SharedInfo, entityUInfo),
     kSecAsn1OctetStringTemplate},
    {SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC | 2,
     offsetof(ECC_CMS_SharedInfo, suppPubInfo),
     kSecAsn1OctetStringTemplate},
    {0}};

/*
 * Given a context specified via a CSSM_CC_HANDLE, add a new
 * CSSM_CONTEXT_ATTRIBUTE to the context as specified by AttributeType,
 * AttributeLength, and an untyped pointer.
 */
/* specify either 32-bit integer or a pointer as an added attribute value */
typedef enum { CAT_Uint32, CAT_Ptr } ContextAttrType;

/* convert uint32 to big-endian 4 bytes */
static void int32ToBytes(uint32_t i, unsigned char* b)
{
    int dex;
    for (dex = 3; dex >= 0; dex--) {
        b[dex] = i;
        i >>= 8;
    }
}

/* 
 * Given an OID tag, return key size and mode. 
 * NOTE: ciphers with variable key sizes, like RC2, RC4, and RC5 cannot
 * be used here because the message does not contain a key size
 * indication. 
 */
static OSStatus encrAlgInfo(SECOidTag oidTag,
                            uint32* keySizeBits,    /* RETURNED */
                            CCAlgorithm* algorithm, /* RETURNED */
                            CCOptions* options)     /* RETURNED */
{
    *keySizeBits = 64;                /* default */
    *options = kCCOptionPKCS7Padding; /* default */

    switch (oidTag) {
        case SEC_OID_RC2_CBC:
        case SEC_OID_RC4:
        case SEC_OID_RC5_CBC_PAD:
            dprintf("encrAlgInfo: key size unknowable\n");
            return errSecDataNotAvailable;
        case SEC_OID_DES_EDE:
            /* Not sure about this; SecCmsCipherContextStart() treats this
             * like SEC_OID_DES_EDE3_CBC... */
            *options = kCCOptionECBMode;
            // fall through
        case SEC_OID_DES_EDE3_CBC:
            *keySizeBits = 192;
            *algorithm = kCCAlgorithm3DES;
            break;
        case SEC_OID_DES_ECB:
            *options = kCCOptionECBMode;
            // fall through
        case SEC_OID_DES_CBC:
            *algorithm = kCCAlgorithmDES;
            break;
        case SEC_OID_AES_128_CBC:
            *keySizeBits = 128;
            *algorithm = kCCAlgorithmAES;
            break;
        case SEC_OID_AES_192_CBC:
            *keySizeBits = 192;
            *algorithm = kCCAlgorithmAES;
            break;
        case SEC_OID_AES_256_CBC:
            *keySizeBits = 256;
            *algorithm = kCCAlgorithmAES;
            break;
        case SEC_OID_AES_128_ECB:
            *keySizeBits = 128;
            *algorithm = kCCAlgorithmAES;
            *options = kCCOptionECBMode;
            break;
        case SEC_OID_AES_192_ECB:
            *keySizeBits = 192;
            *algorithm = kCCAlgorithmAES;
            *options = kCCOptionECBMode;
            break;
        case SEC_OID_AES_256_ECB:
            *keySizeBits = 256;
            *algorithm = kCCAlgorithmAES;
            *options = kCCOptionECBMode;
            break;
        default:
            dprintf("encrAlgInfo: unknown alg tag (%d)\n", (int)oidTag);
            return errSecDataNotAvailable;
    }
    return noErr;
}

#pragma mark---- ECDH CEK key wrap ----

/* 
 * Encrypt bulk encryption key (a.k.a. content encryption key, CEK) using ECDH
 */
OSStatus SecCmsUtilEncryptSymKeyECDH(PLArenaPool* poolp,
                                     SecCertificateRef cert, /* recipient's cert */
                                     SecSymmetricKeyRef key, /* bulk key */
                                     /* remaining fields RETURNED */
                                     CSSM_DATA_PTR encKey, /* encrypted key --> recipientEncryptedKeys[0].EncryptedKey */
                                     CSSM_DATA_PTR ukm, /* random UKM --> KeyAgreeRecipientInfo.ukm */
                                     SECAlgorithmID* keyEncAlg, /* alg := dhSinglePass-stdDH-sha1kdf-scheme
				 * params := another encoded AlgId, with the KEK alg and IV */
                                     CSSM_DATA_PTR pubKey) /* our pub key as ECPoint --> 
				 * KeyAgreeRecipientInfo.originator.OriginatorPublicKey */
{
    OSStatus rv = noErr;
    SecKeyRef theirPubKey = NULL, ourPubKey = NULL, ourPrivKey = NULL;
    CFDictionaryRef theirKeyAttrs = NULL, ourKeyParams = NULL, kekParams = NULL;
    uint8_t iv[ECDH_KEK_IV_LEN_BYTES];
    CSSM_DATA ivData = {ECDH_KEK_IV_LEN_BYTES, iv};
    SECAlgorithmID kekAlgId;
    SECOidData* kekOid;
    ECC_CMS_SharedInfo sharedInfo;
    CSSM_DATA sharedInfoEnc = {0, NULL};
    uint8 nullData[2] = {SEC_ASN1_NULL, 0};
    uint8 keyLenAsBytes[4];
    CFDataRef sharedInfoData = NULL, kekData = NULL, ourPubData = NULL;
    CFNumberRef kekLen = NULL;
    CFErrorRef error = NULL;
    CCCryptorRef ciphercc = NULL;

    encKey->Data = NULL;
    encKey->Length = 0;

    /* Copy the recipient's static public ECDH key */
    theirPubKey = SecCertificateCopyKey(cert);
    if (rv || !theirPubKey) {
        dprintf("SecCmsUtilEncryptSymKeyECDH: failed to get public key from cert, %d\n", (int)rv);
        goto out;
    }

    theirKeyAttrs = SecKeyCopyAttributes(theirPubKey);
    if (!theirKeyAttrs) {
        dprintf("SecCmsUtilEncryptSymKeyECDH: failed to get key attributes\n");
        goto out;
    }

    CFStringRef keyType = NULL;
    CFNumberRef keySizeNum = NULL;
    keyType = CFDictionaryGetValue(theirKeyAttrs, kSecAttrKeyType);
    keySizeNum = CFDictionaryGetValue(theirKeyAttrs, kSecAttrKeySizeInBits);

    if (!CFEqual(kSecAttrKeyTypeECSECPrimeRandom, keyType)) {
        dprintf("SecCmsUtilEncryptSymKeyECDH: unsupported key type\n");
        rv = CSSMERR_CSP_INVALID_KEY;
        goto out;
    }

    /* Generate ephemeral ECDH key */
    const void* keys[] = {kSecAttrKeyType, kSecAttrKeySizeInBits, kSecUseDataProtectionKeychain};
    const void* values[] = {keyType, keySizeNum, kCFBooleanTrue};
    ourKeyParams = CFDictionaryCreate(
        NULL, keys, values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    rv = SecKeyGeneratePair(ourKeyParams, &ourPubKey, &ourPrivKey);
    if (rv || !ourPubKey || !ourPrivKey) {
        dprintf("SecKeyGeneratePair: unable to generate ECDH key pair, %d\n", (int)rv);
        goto out;
    }

    /* Generate UKM */
    ukm->Data = PORT_Alloc(UKM_LENGTH);
    ukm->Length = UKM_LENGTH;
    rv = CCRandomCopyBytes(kCCRandomDefault, ukm->Data, UKM_LENGTH);
    if (rv || !ukm->Data) {
        dprintf("CCRandomGenerateBytes failed, %d", (int)rv);
        goto out;
    }

    /*
     * OK, we have to set up a weird SECAlgorithmID.
     * algorithm = dhSinglePass-stdDH-sha1kdf-scheme
     * params = an encoded SECAlgorithmID representing the KEK algorithm, with
     *   algorithm = whatever we pick
     *   parameters = IV as octet string (though I haven't seen that specified
     *		      anywhere; it's how the CEK IV is encoded)
     */

    /* Generate 8-byte IV */
    rv = CCRandomCopyBytes(kCCRandomDefault, iv, ECDH_KEK_IV_LEN_BYTES);
    if (rv) {
        dprintf("CCRandomGenerateBytes failed, %d", (int)rv);
        goto out;
    }
    dumpBuf("sender IV", &ivData);

    memset(&kekAlgId, 0, sizeof(kekAlgId));
    if (!SEC_ASN1EncodeItem(poolp, &kekAlgId.parameters, &ivData, kSecAsn1OctetStringTemplate)) {
        rv = internalComponentErr;
        goto out;
    }

    /* Drop in the KEK OID and encode the whole thing */
    kekOid = SECOID_FindOIDByTag(ECDH_KEK_ALG_TAG);
    if (kekOid == NULL) {
        dprintf("SecCmsUtilEncryptSymKeyECDH: OID screwup\n");
        rv = internalComponentErr;
        goto out;
    }
    kekAlgId.algorithm = kekOid->oid;
    memset(keyEncAlg, 0, sizeof(*keyEncAlg));
    if (!SEC_ASN1EncodeItem(poolp, &keyEncAlg->parameters, &kekAlgId, SECOID_AlgorithmIDTemplate)) {
        rv = internalComponentErr;
        goto out;
    }
    kekOid = SECOID_FindOIDByTag(SEC_OID_DH_SINGLE_STD_SHA1KDF);
    if (kekOid == NULL) {
        dprintf("SecCmsUtilEncryptSymKeyECDH: OID screwup\n");
        rv = internalComponentErr;
        goto out;
    }
    keyEncAlg->algorithm = kekOid->oid;

    /*
     * Now in order to derive the KEK proper, we have to create a
     * ECC-CMS-SharedInfo, which does not appear in the message, and DER
     * encode that struct, the result of which is used as the
     * SharedInfo value in the KEK key derive.
     */
    memset(&sharedInfo, 0, sizeof(sharedInfo));
    kekOid = SECOID_FindOIDByTag(ECDH_KEK_ALG_TAG);
    sharedInfo.algId.algorithm = kekOid->oid;
    sharedInfo.algId.parameters.Data = nullData;
    sharedInfo.algId.parameters.Length = 2;
    sharedInfo.entityUInfo = *ukm;
    int32ToBytes(ECDH_KEK_KEY_LEN_BYTES << 3, keyLenAsBytes);
    sharedInfo.suppPubInfo.Length = 4;
    sharedInfo.suppPubInfo.Data = keyLenAsBytes;
    if (!SEC_ASN1EncodeItem(poolp, &sharedInfoEnc, &sharedInfo, ECC_CMS_SharedInfoTemplate)) {
        rv = internalComponentErr;
        goto out;
    }
    if (sharedInfoEnc.Length > LONG_MAX) {
        rv = errSecAllocate;
        goto out;
    }
    dumpBuf("sender encoded SharedInfo", &sharedInfoEnc);

    /* Derive KEK */
    sharedInfoData = CFDataCreate(NULL, sharedInfoEnc.Data, (CFIndex)sharedInfoEnc.Length);
    int32_t ecdh_key_key_len = ECDH_KEK_KEY_LEN_BYTES;
    kekLen = CFNumberCreate(NULL, kCFNumberSInt32Type, &ecdh_key_key_len);
    const void* kekKeys[] = {kSecKeyKeyExchangeParameterRequestedSize,
                             kSecKeyKeyExchangeParameterSharedInfo};
    const void* kekValues[] = {kekLen, sharedInfoData};
    kekParams = CFDictionaryCreate(
        NULL, kekKeys, kekValues, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    kekData = SecKeyCopyKeyExchangeResult(
        ourPrivKey, kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA1, theirPubKey, kekParams, &error);
    if (error || CFDataGetLength(kekData) < 0) {
        dprintf("SecKeyCopyKeyExchangeResult: failed\n");
        goto out;
    }

    /*
     * Encrypt the raw CEK bits with the KEK we just derived
     */
    rv = CCCryptorCreate(kCCEncrypt,
                         kCCAlgorithm3DES,
                         kCCOptionPKCS7Padding,
                         CFDataGetBytePtr(kekData),
                         (size_t)CFDataGetLength(kekData),
                         iv,
                         &ciphercc);
    if (rv) {
        dprintf("CCCryptorCreate failed: %d\n", (int)rv);
        goto out;
    }
    CSSM_KEY cek;
    rv = cmsNullWrapKey(key, &cek);
    if (rv) {
        dprintf("SecKeyGetCSSMKey failed: %d\n", (int)rv);
        goto out;
    }
    size_t expectedEncKeyLength = CCCryptorGetOutputLength(ciphercc, cek.KeyData.Length, true);
    encKey->Data = PORT_ArenaAlloc(poolp, expectedEncKeyLength);
    size_t bytes_output = 0;
    rv = CCCryptorUpdate(
        ciphercc, cek.KeyData.Data, cek.KeyData.Length, encKey->Data, expectedEncKeyLength, &bytes_output);
    if (rv) {
        dprintf("CCCryptorUpdate failed: %d\n", (int)rv);
        goto out;
    }
    size_t final_bytes_output = 0;
    rv = CCCryptorFinal(
        ciphercc, encKey->Data + bytes_output, expectedEncKeyLength - bytes_output, &final_bytes_output);
    if (rv) {
        dprintf("CCCryptorFinal failed: %d\n", (int)rv);
        goto out;
    }
    encKey->Length = bytes_output + final_bytes_output;

    /* Provide our ephemeral public key to the caller */
    ourPubData = SecKeyCopyExternalRepresentation(ourPubKey, &error);
    if (error || CFDataGetLength(ourPubData) < 0) {
        dprintf("SecKeyCopyExternalRepresentation failed\n");
        goto out;
    }
    pubKey->Length = (size_t)CFDataGetLength(ourPubData);
    pubKey->Data = malloc(pubKey->Length);
    if (pubKey->Data) {
        memcpy(pubKey->Data, CFDataGetBytePtr(ourPubData), pubKey->Length);
    } else {
        rv = errSecAllocate;
    }
    /* pubKey is bit string, convert here */
    pubKey->Length <<= 3;

out:
    CFReleaseNull(theirPubKey);
    CFReleaseNull(theirKeyAttrs);
    CFReleaseNull(ourKeyParams);
    CFReleaseNull(ourPubKey);
    CFReleaseNull(ourPrivKey);
    CFReleaseNull(sharedInfoData);
    CFReleaseNull(kekLen);
    CFReleaseNull(kekParams);
    CFReleaseNull(kekData);
    CFReleaseNull(error);
    CFReleaseNull(ourPubData);
    if (ciphercc) {
        CCCryptorRelease(ciphercc);
    }
    if (rv && encKey->Data) {
        PORT_Free(encKey->Data);
        encKey->Data = NULL;
        encKey->Length = 0;
    }
    if (rv && ukm->Data) {
        PORT_Free(ukm->Data);
        ukm->Data = NULL;
        ukm->Length = 0;
    }
    return rv;
}


#pragma mark---- ECDH CEK key unwrap ----

SecSymmetricKeyRef SecCmsUtilDecryptSymKeyECDH(SecPrivateKeyRef privkey, /* our private key */
                                               CSSM_DATA_PTR encKey, /* encrypted CEK */
                                               CSSM_DATA_PTR ukm, /* random UKM from KeyAgreeRecipientInfo.ukm */
                                               SECAlgorithmID* keyEncAlg, /* alg := dhSinglePass-stdDH-sha1kdf-scheme
				 * params := another encoded AlgId, with the KEK alg and IV */
                                               SECOidTag bulkalgtag, /* algorithm of returned key */
                                               CSSM_DATA_PTR pubKey) /* sender's pub key as ECPoint from
				 * KeyAgreeRecipientInfo.originator.OriginatorPublicKey */
{
    SecSymmetricKeyRef outKey = NULL;
    OSStatus rv = noErr;
    PLArenaPool* pool = NULL;
    SECAlgorithmID keyAlgParam;
    SECOidData* kekOid = NULL;
    CSSM_DATA iv = {0, NULL};
    ECC_CMS_SharedInfo sharedInfo;
    CSSM_DATA sharedInfoEnc = {0, NULL};
    uint8 nullData[2] = {SEC_ASN1_NULL, 0};
    uint8 keyLenAsBytes[4];
    uint32 kekSizeBits;
    SecKeyRef theirPubKey = NULL;
    CFStringRef keyType = NULL;
    CFDictionaryRef theirKeyAttrs = NULL, kekParams = NULL;
    CFMutableDictionaryRef cekParams = NULL;
    CFDataRef sharedInfoData = NULL, theirPubData = NULL, kekData = NULL, cekData = NULL;
    CFNumberRef kekLen = NULL, theirKeyLen = NULL;
    CFErrorRef error = NULL;
    CCAlgorithm alg;
    CCOptions options = 0;
    CCCryptorRef ciphercc = NULL;
    size_t theirKeySizeInBits = 0;

    /*
     * Decode keyEncAlg.params to get KEK algorithm and IV
     */
    pool = PORT_NewArena(1024);
    if (pool == NULL) {
        goto out;
    }
    memset(&keyAlgParam, 0, sizeof(keyAlgParam));
    if (SEC_ASN1DecodeItem(pool, &keyAlgParam, SECOID_AlgorithmIDTemplate, &keyEncAlg->parameters)) {
        dprintf("SecCmsUtilDecryptSymKeyECDH: error decoding keyAlgParams\n");
        goto out;
    }
    kekOid = SECOID_FindOID(&keyAlgParam.algorithm);
    if (kekOid == NULL) {
        dprintf("SecCmsUtilDecryptSymKeyECDH: unknown KEK enc OID\n");
        goto out;
    }
    rv = encrAlgInfo(kekOid->offset, &kekSizeBits, &alg, &options);
    if (rv) {
        goto out;
    }
    /* IV is OCTET STRING in the alg params */
    if (SEC_ASN1DecodeItem(pool, &iv, kSecAsn1OctetStringTemplate, &keyAlgParam.parameters)) {
        /*
         * Not sure here - is it legal to have no IV? I haven't seen this
         * addressed in any spec. Maybe we should condition the behavior
         * here on the KEK algorithm.
         */
        dprintf("SecCmsUtilDecryptSymKeyECDH: no KEK IV\n");
        goto out;
    }

    /*
     * Now in order to derive the KEK proper, we have to create a
     * ECC-CMS-SharedInfo, which does not appear in the message, and DER
     * encode that struct, the result of which is used as the
     * SharedInfo value in the KEK key derive.
     */
    memset(&sharedInfo, 0, sizeof(sharedInfo));
    sharedInfo.algId.algorithm = kekOid->oid;
    sharedInfo.algId.parameters.Data = nullData;
    sharedInfo.algId.parameters.Length = 2;
    sharedInfo.entityUInfo = *ukm;
    int32ToBytes(kekSizeBits, keyLenAsBytes);
    sharedInfo.suppPubInfo.Length = 4;
    sharedInfo.suppPubInfo.Data = keyLenAsBytes;
    if (!SEC_ASN1EncodeItem(pool, &sharedInfoEnc, &sharedInfo, ECC_CMS_SharedInfoTemplate)) {
        rv = internalComponentErr;
        goto out;
    }
    dumpBuf("receiver encoded SharedInfo", &sharedInfoEnc);
    dumpBuf("receiver IV", &iv);
    dumpBuf("receiver UKM", ukm);
    dumpBuf("sender's public key", pubKey);

    /* pubKey is bit string, convert here */
    theirKeySizeInBits = pubKey->Length;
    pubKey->Length = (theirKeySizeInBits + 7) >> 3;
    if (pubKey->Length > LONG_MAX) {
        goto out;
    }
    theirPubData = CFDataCreate(NULL, pubKey->Data, (CFIndex)pubKey->Length);
    theirKeyLen = CFNumberCreate(NULL, kCFNumberSInt64Type, &theirKeySizeInBits);
    const void* keys[] = {kSecAttrKeyType, kSecAttrKeyClass, kSecAttrKeySizeInBits};
    const void* values[] = {kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeyClassPublic, theirKeyLen};
    theirKeyAttrs = CFDictionaryCreate(
        NULL, keys, values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    theirPubKey = SecKeyCreateWithData(theirPubData, theirKeyAttrs, &error);
    if (error) {
        dprintf("SecKeyCreateWithData: failed\n");
        goto out;
    }
    if (sharedInfoEnc.Length > LONG_MAX) {
        goto out;
    }

    /* Derive KEK */
    sharedInfoData = CFDataCreate(NULL, sharedInfoEnc.Data, (CFIndex)sharedInfoEnc.Length);
    int32_t ecdh_key_key_len = (kekSizeBits + 7) >> 3;
    kekLen = CFNumberCreate(NULL, kCFNumberSInt32Type, &ecdh_key_key_len);
    const void* kekKeys[] = {kSecKeyKeyExchangeParameterRequestedSize,
                             kSecKeyKeyExchangeParameterSharedInfo};
    const void* kekValues[] = {kekLen, sharedInfoData};
    kekParams = CFDictionaryCreate(
        NULL, kekKeys, kekValues, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    kekData = SecKeyCopyKeyExchangeResult(
        privkey, kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA1, theirPubKey, kekParams, &error);
    if (error || CFDataGetLength(kekData) < 0) {
        dprintf("SecKeyCopyKeyExchangeResult: failed\n");
        goto out;
    }

    /*
     * Decrypt the raw CEK bits with the KEK we just derived
     */
    CSSM_DATA cek = {0, NULL};
    rv = CCCryptorCreate(
        kCCDecrypt, alg, options, CFDataGetBytePtr(kekData), (size_t)CFDataGetLength(kekData), iv.Data, &ciphercc);
    if (rv) {
        dprintf("CCCryptorCreate failed: %d\n", (int)rv);
        goto out;
    }
    size_t expectedKeyLength = CCCryptorGetOutputLength(ciphercc, encKey->Length, true);
    cek.Data = PORT_ArenaAlloc(pool, expectedKeyLength);
    size_t bytes_output = 0;
    rv = CCCryptorUpdate(ciphercc, encKey->Data, encKey->Length, cek.Data, expectedKeyLength, &bytes_output);
    if (rv) {
        dprintf("CCCryptorUpdate failed: %d\n", (int)rv);
        goto out;
    }
    size_t final_bytes_output = 0;
    rv = CCCryptorFinal(ciphercc, cek.Data + bytes_output, expectedKeyLength - bytes_output, &final_bytes_output);
    if (rv) {
        dprintf("CCCryptorFinal failed: %d\n", (int)rv);
        goto out;
    }
    cek.Length = bytes_output + final_bytes_output;
    if (cek.Length > LONG_MAX) {
        goto out;
    }

    /* create the SecSymmetricKeyRef */
    cekData = CFDataCreate(NULL, cek.Data, (CFIndex)cek.Length);
    keyType = SECOID_CopyKeyTypeByTag(bulkalgtag);
    if (!keyType) {
        goto out;
    }
    cekParams = CFDictionaryCreateMutable(
        NULL, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!cekParams) {
        goto out;
    }
    CFDictionaryAddValue(cekParams, kSecAttrKeyType, keyType);
    outKey = SecKeyCreateFromData(cekParams, cekData, NULL);

out:
    if (pool != NULL) {
        PORT_FreeArena(pool, PR_FALSE);
    }
    CFReleaseNull(theirPubData);
    CFReleaseNull(theirKeyLen);
    CFReleaseNull(theirPubKey);
    CFReleaseNull(theirKeyAttrs);
    CFReleaseNull(sharedInfoData);
    CFReleaseNull(kekLen);
    CFReleaseNull(kekParams);
    CFReleaseNull(kekData);
    CFReleaseNull(error);
    CFReleaseNull(cekData);
    CFReleaseNull(keyType);
    CFReleaseNull(cekParams);
    if (ciphercc) {
        CCCryptorRelease(ciphercc);
    }
    if (outKey == NULL) {
        PORT_SetError(SEC_ERROR_NO_KEY);
    }
    return outKey;
}
