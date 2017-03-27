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

#include "SecAsn1Item.h"
#include "secoid.h"
#include "cryptohi.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <security_asn1/secport.h>

#include <Security/Security.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecKeyPriv.h>

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonRandomSPI.h>
#include <CommonCrypto/CommonRandom.h>

/* ====== RSA ======================================================================= */

/*
 * SecCmsUtilEncryptSymKeyRSA - wrap a symmetric key with RSA
 *
 * this function takes a symmetric key and encrypts it using an RSA public key
 * according to PKCS#1 and RFC2633 (S/MIME)
 */
OSStatus
SecCmsUtilEncryptSymKeyRSA(PLArenaPool *poolp, SecCertificateRef cert, 
                              SecSymmetricKeyRef bulkkey,
                              SecAsn1Item * encKey)
{
    OSStatus rv;
    SecPublicKeyRef publickey;
#if TARGET_OS_MAC && !TARGET_OS_IPHONE
    rv = SecCertificateCopyPublicKey(cert,&publickey);
#else
    publickey = SecCertificateCopyPublicKey(cert);
#endif
    if (publickey == NULL)
	return SECFailure;

    rv = SecCmsUtilEncryptSymKeyRSAPubKey(poolp, publickey, bulkkey, encKey);
    CFRelease(publickey);
    return rv;
}

OSStatus
SecCmsUtilEncryptSymKeyRSAPubKey(PLArenaPool *poolp, 
				 SecPublicKeyRef publickey, 
				 SecSymmetricKeyRef bulkkey, SecAsn1Item * encKey)
{
    OSStatus rv;
    size_t data_len;
    //KeyType keyType;
    void *mark = NULL;

    mark = PORT_ArenaMark(poolp);
    if (!mark)
	goto loser;

#if 0
    /* sanity check */
    keyType = SECKEY_GetPublicKeyType(publickey);
    PORT_Assert(keyType == rsaKey);
    if (keyType != rsaKey) {
	goto loser;
    }
#endif
    /* allocate memory for the encrypted key */
#if TARGET_OS_MAC && !TARGET_OS_IPHONE
    rv = SecKeyGetStrengthInBits(publickey, NULL, &data_len);
    if (rv)
	goto loser;
    // Convert length to bytes;
    data_len = data_len / 8;
#else
    data_len = SecKeyGetSize(publickey, kSecKeyEncryptedDataSize);
#endif

    encKey->Data = (unsigned char*)PORT_ArenaAlloc(poolp, data_len);
    encKey->Length = data_len;
    if (encKey->Data == NULL)
	goto loser;

    /* encrypt the key now */
    rv = WRAP_PubWrapSymKey(publickey, bulkkey, encKey);
    if (rv != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
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
SecSymmetricKeyRef
SecCmsUtilDecryptSymKeyRSA(SecPrivateKeyRef privkey, SecAsn1Item * encKey, SECOidTag bulkalgtag)
{
    /* that's easy */
    return WRAP_PubUnwrapSymKey(privkey, encKey, bulkalgtag);
}

#if 0
// @@@ Implement Fortezza and Diffie hellman support

/* ====== MISSI (Fortezza) ========================================================== */

extern const SecAsn1Template NSS_SMIMEKEAParamTemplateAllParams[];

OSStatus
SecCmsUtilEncryptSymKeyMISSI(PLArenaPool *poolp, SecCertificateRef cert, SecSymmetricKeyRef bulkkey,
			SECOidTag symalgtag, SecAsn1Item * encKey, SecAsn1Item * *pparams, void *pwfn_arg)
{
    SECOidTag certalgtag;	/* the certificate's encryption algorithm */
    SECOidTag encalgtag;	/* the algorithm used for key exchange/agreement */
    OSStatus rv = SECFailure;
    SecAsn1Item * params = NULL;
    OSStatus err;
    SecSymmetricKeyRef tek;
    SecCertificateRef ourCert;
    SecPublicKeyRef ourPubKey, *publickey = NULL;
    SecPrivateKeyRef ourPrivKey = NULL;
    SecCmsKEATemplateSelector whichKEA = SecCmsKEAInvalid;
    SecCmsSMIMEKEAParameters keaParams;
    PLArenaPool *arena = NULL;
    const SECAlgorithmID *algid;

    /* Clear keaParams, since cleanup code checks the lengths */
    (void) memset(&keaParams, 0, sizeof(keaParams));

    certalgtag = SECOID_GetAlgorithmTag(algid);
    PORT_Assert(certalgtag == SEC_OID_MISSI_KEA_DSS_OLD ||
		certalgtag == SEC_OID_MISSI_KEA_DSS ||
		certalgtag == SEC_OID_MISSI_KEA);

#define SMIME_FORTEZZA_RA_LENGTH 128
#define SMIME_FORTEZZA_IV_LENGTH 24
#define SMIME_FORTEZZA_MAX_KEY_SIZE 256

    /* We really want to show our KEA tag as the key exchange algorithm tag. */
    encalgtag = SEC_OID_NETSCAPE_SMIME_KEA;

    /* Get the public key of the recipient. */
    publickey = CERT_ExtractPublicKey(cert);
    if (publickey == NULL) goto loser;

    /* Find our own cert, and extract its keys. */
    ourCert = PK11_FindBestKEAMatch(cert, pwfn_arg);
    if (ourCert == NULL) goto loser;

    arena = PORT_NewArena(1024);
    if (arena == NULL)
	goto loser;

    ourPubKey = CERT_ExtractPublicKey(ourCert);
    if (ourPubKey == NULL) {
	CERT_DestroyCertificate(ourCert);
	goto loser;
    }

    /* While we're here, copy the public key into the outgoing
     * KEA parameters. */
    SECITEM_CopyItem(arena, &(keaParams.originatorKEAKey), &(ourPubKey->u.fortezza.KEAKey));
    SECKEY_DestroyPublicKey(ourPubKey);
    ourPubKey = NULL;

    /* Extract our private key in order to derive the KEA key. */
    ourPrivKey = PK11_FindKeyByAnyCert(ourCert, pwfn_arg);
    CERT_DestroyCertificate(ourCert); /* we're done with this */
    if (!ourPrivKey)
	goto loser;

    /* Prepare raItem with 128 bytes (filled with zeros). */
    keaParams.originatorRA.Data = (unsigned char *)PORT_ArenaAlloc(arena,SMIME_FORTEZZA_RA_LENGTH);
    keaParams.originatorRA.Length = SMIME_FORTEZZA_RA_LENGTH;

    /* Generate the TEK (token exchange key) which we use
     * to wrap the bulk encryption key. (keaparams.originatorRA) will be
     * filled with a random seed which we need to send to
     * the recipient. (user keying material in RFC2630/DSA speak) */
    tek = PK11_PubDerive(ourPrivKey, publickey, PR_TRUE,
			 &keaParams.originatorRA, NULL,
			 CKM_KEA_KEY_DERIVE, CKM_SKIPJACK_WRAP,
			 CKA_WRAP, 0,  pwfn_arg);

    SECKEY_DestroyPublicKey(publickey);
    SECKEY_DestroyPrivateKey(ourPrivKey);
    publickey = NULL;
    ourPrivKey = NULL;
    
    if (!tek)
	goto loser;

    /* allocate space for the wrapped key data */
    encKey->Data = (unsigned char *)PORT_ArenaAlloc(poolp, SMIME_FORTEZZA_MAX_KEY_SIZE);
    encKey->Length = SMIME_FORTEZZA_MAX_KEY_SIZE;

    if (encKey->Data == NULL) {
	CFRelease(tek);
	goto loser;
    }

    /* Wrap the bulk key. What we do with the resulting data
       depends on whether we're using Skipjack to wrap the key. */
    switch (PK11_AlgtagToMechanism(symalgtag)) {
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
	/* SKIPJACK, we use the wrap mechanism because we can do it on the hardware */
	err = PK11_WrapSymKey(CKM_SKIPJACK_WRAP, NULL, tek, bulkkey, encKey);
	whichKEA = SecCmsKEAUsesSkipjack;
	break;
    default:
	/* Not SKIPJACK, we encrypt the raw key data */
	keaParams.nonSkipjackIV.Data = 
	  (unsigned char *)PORT_ArenaAlloc(arena, SMIME_FORTEZZA_IV_LENGTH);
	keaParams.nonSkipjackIV.Length = SMIME_FORTEZZA_IV_LENGTH;
	err = PK11_WrapSymKey(CKM_SKIPJACK_CBC64, &keaParams.nonSkipjackIV, tek, bulkkey, encKey);
	if (err != SECSuccess)
	    goto loser;

	if (encKey->Length != PK11_GetKeyLength(bulkkey)) {
	    /* The size of the encrypted key is not the same as
	       that of the original bulk key, presumably due to
	       padding. Encode and store the real size of the
	       bulk key. */
	    if (SEC_ASN1EncodeInteger(arena, &keaParams.bulkKeySize, PK11_GetKeyLength(bulkkey)) == NULL)
		err = (OSStatus)PORT_GetError();
	    else
		/* use full template for encoding */
		whichKEA = SecCmsKEAUsesNonSkipjackWithPaddedEncKey;
	}
	else
	    /* enc key length == bulk key length */
	    whichKEA = SecCmsKEAUsesNonSkipjack; 
	break;
    }

    CFRelease(tek);

    if (err != SECSuccess)
	goto loser;

    PORT_Assert(whichKEA != SecCmsKEAInvalid);

    /* Encode the KEA parameters into the recipient info. */
    params = SEC_ASN1EncodeItem(poolp, NULL, &keaParams, nss_cms_get_kea_template(whichKEA));
    if (params == NULL)
	goto loser;

    /* pass back the algorithm params */
    *pparams = params;

    rv = SECSuccess;

loser:
    if (arena)
	PORT_FreeArena(arena, PR_FALSE);
    if (publickey)
        SECKEY_DestroyPublicKey(publickey);
    if (ourPrivKey)
        SECKEY_DestroyPrivateKey(ourPrivKey);
    return rv;
}

SecSymmetricKeyRef
SecCmsUtilDecryptSymKeyMISSI(SecPrivateKeyRef privkey, SecAsn1Item * encKey, SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag, void *pwfn_arg)
{
    /* fortezza: do a key exchange */
    OSStatus err;
    CK_MECHANISM_TYPE bulkType;
    SecSymmetricKeyRef tek;
    SecPublicKeyRef originatorPubKey;
    SecCmsSMIMEKEAParameters keaParams;
    SecSymmetricKeyRef bulkkey;
    int bulkLength;

    (void) memset(&keaParams, 0, sizeof(keaParams));

    /* NOTE: this uses the SMIME v2 recipientinfo for compatibility.
       All additional KEA parameters are DER-encoded in the encryption algorithm parameters */

    /* Decode the KEA algorithm parameters. */
    err = SEC_ASN1DecodeItem(NULL, &keaParams, NSS_SMIMEKEAParamTemplateAllParams,
			     &(keyEncAlg->parameters));
    if (err != SECSuccess)
	goto loser;

    /* get originator's public key */
   originatorPubKey = PK11_MakeKEAPubKey(keaParams.originatorKEAKey.Data,
			   keaParams.originatorKEAKey.Length);
   if (originatorPubKey == NULL)
	  goto loser;
    
   /* Generate the TEK (token exchange key) which we use to unwrap the bulk encryption key.
      The Derive function generates a shared secret and combines it with the originatorRA
      data to come up with an unique session key */
   tek = PK11_PubDerive(privkey, originatorPubKey, PR_FALSE,
			 &keaParams.originatorRA, NULL,
			 CKM_KEA_KEY_DERIVE, CKM_SKIPJACK_WRAP,
			 CKA_WRAP, 0, pwfn_arg);
   SECKEY_DestroyPublicKey(originatorPubKey);	/* not needed anymore */
   if (tek == NULL)
	goto loser;
    
    /* Now that we have the TEK, unwrap the bulk key
       with which to decrypt the message. We have to
       do one of two different things depending on 
       whether Skipjack was used for *bulk* encryption 
       of the message. */
    bulkType = PK11_AlgtagToMechanism(bulkalgtag);
    switch (bulkType) {
    case CKM_SKIPJACK_CBC64:
    case CKM_SKIPJACK_ECB64:
    case CKM_SKIPJACK_OFB64:
    case CKM_SKIPJACK_CFB64:
    case CKM_SKIPJACK_CFB32:
    case CKM_SKIPJACK_CFB16:
    case CKM_SKIPJACK_CFB8:
	/* Skipjack is being used as the bulk encryption algorithm.*/
	/* Unwrap the bulk key. */
	bulkkey = PK11_UnwrapSymKey(tek, CKM_SKIPJACK_WRAP, NULL,
				    encKey, CKM_SKIPJACK_CBC64, CKA_DECRYPT, 0);
	break;
    default:
	/* Skipjack was not used for bulk encryption of this
	   message. Use Skipjack CBC64, with the nonSkipjackIV
	   part of the KEA key parameters, to decrypt 
	   the bulk key. If the optional parameter bulkKeySize is present,
	   bulk key size is different than the encrypted key size */
	if (keaParams.bulkKeySize.Length > 0) {
	    err = SEC_ASN1DecodeItem(NULL, &bulkLength,
				     SEC_ASN1_GET(SEC_IntegerTemplate),
				     &keaParams.bulkKeySize);
	    if (err != SECSuccess)
		goto loser;
	}
	
	bulkkey = PK11_UnwrapSymKey(tek, CKM_SKIPJACK_CBC64, &keaParams.nonSkipjackIV, 
				    encKey, bulkType, CKA_DECRYPT, bulkLength);
	break;
    }
    return bulkkey;
loser:
    return NULL;
}

/* ====== ESDH (Ephemeral-Static Diffie-Hellman) ==================================== */

OSStatus
SecCmsUtilEncryptSymKeyESDH(PLArenaPool *poolp, SecCertificateRef cert, SecSymmetricKeyRef key,
			SecAsn1Item * encKey, SecAsn1Item * *ukm, SECAlgorithmID *keyEncAlg,
			SecAsn1Item * pubKey)
{
#if 0 /* not yet done */
    SECOidTag certalgtag;	/* the certificate's encryption algorithm */
    SECOidTag encalgtag;	/* the algorithm used for key exchange/agreement */
    OSStatus rv;
    SecAsn1Item * params = NULL;
    int data_len;
    OSStatus err;
    SecSymmetricKeyRef tek;
    SecCertificateRef ourCert;
    SecPublicKeyRef ourPubKey;
    SecCmsKEATemplateSelector whichKEA = SecCmsKEAInvalid;

    certalgtag = SECOID_GetAlgorithmTag(&(cert->subjectPublicKeyInfo.algorithm));
    PORT_Assert(certalgtag == SEC_OID_X942_DIFFIE_HELMAN_KEY);

    /* We really want to show our KEA tag as the key exchange algorithm tag. */
    encalgtag = SEC_OID_CMS_EPHEMERAL_STATIC_DIFFIE_HELLMAN;

    /* Get the public key of the recipient. */
    publickey = CERT_ExtractPublicKey(cert);
    if (publickey == NULL) goto loser;

    /* XXXX generate a DH key pair on a PKCS11 module (XXX which parameters?) */
    /* XXXX */ourCert = PK11_FindBestKEAMatch(cert, wincx);
    if (ourCert == NULL) goto loser;

    arena = PORT_NewArena(1024);
    if (arena == NULL) goto loser;

    /* While we're here, extract the key pair's public key data and copy it into */
    /* the outgoing parameters. */
    /* XXXX */ourPubKey = CERT_ExtractPublicKey(ourCert);
    if (ourPubKey == NULL)
    {
	goto loser;
    }
    SECITEM_CopyItem(arena, pubKey, /* XXX */&(ourPubKey->u.fortezza.KEAKey));
    SECKEY_DestroyPublicKey(ourPubKey); /* we only need the private key from now on */
    ourPubKey = NULL;

    /* Extract our private key in order to derive the KEA key. */
    ourPrivKey = PK11_FindKeyByAnyCert(ourCert,wincx);
    CERT_DestroyCertificate(ourCert); /* we're done with this */
    if (!ourPrivKey) goto loser;

    /* If ukm desired, prepare it - allocate enough space (filled with zeros). */
    if (ukm) {
	ukm->Data = (unsigned char*)PORT_ArenaZAlloc(arena,/* XXXX */);
	ukm->Length = /* XXXX */;
    }

    /* Generate the KEK (key exchange key) according to RFC2631 which we use
     * to wrap the bulk encryption key. */
    kek = PK11_PubDerive(ourPrivKey, publickey, PR_TRUE,
			 ukm, NULL,
			 /* XXXX */CKM_KEA_KEY_DERIVE, /* XXXX */CKM_SKIPJACK_WRAP,
			 CKA_WRAP, 0, wincx);

    SECKEY_DestroyPublicKey(publickey);
    SECKEY_DestroyPrivateKey(ourPrivKey);
    publickey = NULL;
    ourPrivKey = NULL;
    
    if (!kek)
	goto loser;

    /* allocate space for the encrypted CEK (bulk key) */
    encKey->Data = (unsigned char*)PORT_ArenaAlloc(poolp, SMIME_FORTEZZA_MAX_KEY_SIZE);
    encKey->Length = SMIME_FORTEZZA_MAX_KEY_SIZE;

    if (encKey->Data == NULL)
    {
	CFRelease(kek);
	goto loser;
    }


    /* Wrap the bulk key using CMSRC2WRAP or CMS3DESWRAP, depending on the */
    /* bulk encryption algorithm */
    switch (/* XXXX */PK11_AlgtagToMechanism(enccinfo->encalg))
    {
    case /* XXXX */CKM_SKIPJACK_CFB8:
	err = PK11_WrapSymKey(/* XXXX */CKM_CMS3DES_WRAP, NULL, kek, bulkkey, encKey);
	whichKEA = SecCmsKEAUsesSkipjack;
	break;
    case /* XXXX */CKM_SKIPJACK_CFB8:
	err = PK11_WrapSymKey(/* XXXX */CKM_CMSRC2_WRAP, NULL, kek, bulkkey, encKey);
	whichKEA = SecCmsKEAUsesSkipjack;
	break;
    default:
	/* XXXX what do we do here? Neither RC2 nor 3DES... */
        err = SECFailure;
        /* set error */
	break;
    }

    CFRelease(kek);	/* we do not need the KEK anymore */
    if (err != SECSuccess)
	goto loser;

    PORT_Assert(whichKEA != SecCmsKEAInvalid);

    /* see RFC2630 12.3.1.1 "keyEncryptionAlgorithm must be ..." */
    /* params is the DER encoded key wrap algorithm (with parameters!) (XXX) */
    params = SEC_ASN1EncodeItem(arena, NULL, &keaParams, sec_pkcs7_get_kea_template(whichKEA));
    if (params == NULL)
	goto loser;

    /* now set keyEncAlg */
    rv = SECOID_SetAlgorithmID(poolp, keyEncAlg, SEC_OID_CMS_EPHEMERAL_STATIC_DIFFIE_HELLMAN, params);
    if (rv != SECSuccess)
	goto loser;

    /* XXXXXXX this is not right yet */
loser:
    if (arena) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    if (publickey) {
        SECKEY_DestroyPublicKey(publickey);
    }
    if (ourPrivKey) {
        SECKEY_DestroyPrivateKey(ourPrivKey);
    }
#endif
    return SECFailure;
}

SecSymmetricKeyRef
SecCmsUtilDecryptSymKeyESDH(SecPrivateKeyRef privkey, SecAsn1Item * encKey, SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag, void *pwfn_arg)
{
#if 0 /* not yet done */
    OSStatus err;
    CK_MECHANISM_TYPE bulkType;
    SecSymmetricKeyRef tek;
    SecPublicKeyRef originatorPubKey;
    SecCmsSMIMEKEAParameters keaParams;

   /* XXXX get originator's public key */
   originatorPubKey = PK11_MakeKEAPubKey(keaParams.originatorKEAKey.Data,
			   keaParams.originatorKEAKey.Length);
   if (originatorPubKey == NULL)
      goto loser;
    
   /* Generate the TEK (token exchange key) which we use to unwrap the bulk encryption key.
      The Derive function generates a shared secret and combines it with the originatorRA
      data to come up with an unique session key */
   tek = PK11_PubDerive(privkey, originatorPubKey, PR_FALSE,
			 &keaParams.originatorRA, NULL,
			 CKM_KEA_KEY_DERIVE, CKM_SKIPJACK_WRAP,
			 CKA_WRAP, 0, pwfn_arg);
   SECKEY_DestroyPublicKey(originatorPubKey);	/* not needed anymore */
   if (tek == NULL)
	goto loser;
    
    /* Now that we have the TEK, unwrap the bulk key
       with which to decrypt the message. */
    /* Skipjack is being used as the bulk encryption algorithm.*/
    /* Unwrap the bulk key. */
    bulkkey = PK11_UnwrapSymKey(tek, CKM_SKIPJACK_WRAP, NULL,
				encKey, CKM_SKIPJACK_CBC64, CKA_DECRYPT, 0);

    return bulkkey;

loser:
#endif
    return NULL;
}

#endif

/* ====== ECDH (Ephemeral-Static Diffie-Hellman) ==================================== */

#pragma mark ---- ECDH support functions ----

#ifdef	NDEBUG
#define CSSM_PERROR(f, r)
#define dprintf(args...)
#else
#define CSSM_PERROR(f, r)   cssmPerror(f, r)
#define dprintf(args...)    fprintf(stderr, args)
#endif

/* Length of KeyAgreeRecipientInfo.ukm we create */
#define UKM_LENGTH	8

/* KEK algorithm info we generate */
#define ECDH_KEK_ALG_TAG	    SEC_OID_DES_EDE3_CBC
#define ECDH_KEK_KEY_CSSM_ALGID	    CSSM_ALGID_3DES_3KEY
#define ECDH_KEK_ENCR_CSSM_ALGID    CSSM_ALGID_3DES_3KEY_EDE
#define ECDH_KEK_KEY_LEN_BYTES	    24
#define ECDH_KEK_IV_LEN_BYTES	    8

#define CMS_DUMP_BUFS	    0
#if	CMS_DUMP_BUFS

static void dumpBuf(
                    const char *label,
                    const CSSM_DATA *cd)
{
    unsigned dex;

    printf("%s:\n   ", label);
    for(dex=0; dex<cd->Length; dex++) {
        printf("%02X ", cd->Data[dex]);
        if(((dex % 16) == 15) && (dex != (cd->Length - 1))) {
            printf("\n   ");
        }
    }
    putchar('\n');
}

#else
#define dumpBuf(l, d)
#endif	/* CMS_DUMP_BUFS */

/*
 * The ECC-CMS-SharedInfo struct, as defined in RFC 3278 8.2, and the
 * template for DER encoding and decoding it.
 */
typedef struct {
    SECAlgorithmID  algId;	    /* KEK alg, NULL params */
    SecAsn1Item	    entityUInfo;    /* optional, ukm */
    SecAsn1Item	    suppPubInfo;    /* length of KEK in bits as 4-byte integer */
} ECC_CMS_SharedInfo;

static const SecAsn1Template ECC_CMS_SharedInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(ECC_CMS_SharedInfo) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC | 0,
        offsetof(ECC_CMS_SharedInfo,entityUInfo),
        kSecAsn1OctetStringTemplate },
    { SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | SEC_ASN1_CONTEXT_SPECIFIC | 2,
        offsetof(ECC_CMS_SharedInfo,suppPubInfo),
        kSecAsn1OctetStringTemplate },
    { 0 }
};

/*
 * Given a context specified via a CSSM_CC_HANDLE, add a new
 * CSSM_CONTEXT_ATTRIBUTE to the context as specified by AttributeType,
 * AttributeLength, and an untyped pointer.
 */
/* specify either 32-bit integer or a pointer as an added attribute value */
typedef enum {
    CAT_Uint32,
    CAT_Ptr
} ContextAttrType;

/* convert uint32 to big-endian 4 bytes */
static void int32ToBytes(
                         uint32_t i,
                         unsigned char *b)
{
    int dex;
    for(dex=3; dex>=0; dex--) {
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
static OSStatus encrAlgInfo(
                            SECOidTag		oidTag,
                            uint32_t		*keySizeBits,	/* RETURNED */
                            CCAlgorithm         *algorithm,	/* RETURNED */
                            CCOptions           *options)	/* RETURNED */
{
    *keySizeBits = 64;		    /* default */
    *options = kCCOptionPKCS7Padding; /* default */

    switch(oidTag) {
        case SEC_OID_RC2_CBC:
        case SEC_OID_RC4:
        case SEC_OID_RC5_CBC_PAD:
            dprintf("encrAlgInfo: key size unknowable\n");
            return errSecNotAvailable;
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
            return errSecNotAvailable;
    }
    return noErr;
}

#pragma mark ---- ECDH CEK key wrap ----

/*
 * Encrypt bulk encryption key (a.k.a. content encryption key, CEK) using ECDH
 */
OSStatus
SecCmsUtilEncryptSymKeyECDH(
                            PLArenaPool *poolp,
                            SecCertificateRef cert,	/* recipient's cert */
                            SecSymmetricKeyRef key,	/* bulk key */
                            /* remaining fields RETURNED */
                            SecAsn1Item  *encKey,	/* encrypted key --> recipientEncryptedKeys[0].EncryptedKey */
                            SecAsn1Item  *ukm,		/* random UKM --> KeyAgreeRecipientInfo.ukm */
                            SECAlgorithmID *keyEncAlg,	/* alg := dhSinglePass-stdDH-sha1kdf-scheme
                                                         * params := another encoded AlgId, with the KEK alg and IV */
                            SecAsn1Item  *pubKey)	/* our pub key as ECPoint -->
                                                         * KeyAgreeRecipientInfo.originator.OriginatorPublicKey */
{
    OSStatus rv = noErr;
    SecKeyRef theirPubKey = NULL, ourPubKey = NULL, ourPrivKey = NULL;
    CFDictionaryRef theirKeyAttrs = NULL, ourKeyParams = NULL, kekParams = NULL;
    uint8_t iv[ECDH_KEK_IV_LEN_BYTES];
    SecAsn1Item ivData = { ECDH_KEK_IV_LEN_BYTES, iv };
    SECAlgorithmID kekAlgId;
    SECOidData *kekOid;
    ECC_CMS_SharedInfo sharedInfo;
    SecAsn1Item sharedInfoEnc = {0, NULL};
    uint8_t nullData[2] = {SEC_ASN1_NULL, 0};
    uint8_t keyLenAsBytes[4];
    CFDataRef sharedInfoData = NULL, kekData = NULL, ourPubData = NULL;
    CFNumberRef kekLen = NULL;
    CFErrorRef error = NULL;
    CCCryptorRef ciphercc = NULL;

    encKey->Data = NULL;
    encKey->Length = 0;

    /* Copy the recipient's static public ECDH key */
#if TARGET_OS_IPHONE
    theirPubKey = SecCertificateCopyPublicKey(cert);
#else
    rv = SecCertificateCopyPublicKey(cert, &theirPubKey);
#endif
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
        rv = SEC_ERROR_INVALID_KEY;
        goto out;
    }

    /* Generate ephemeral ECDH key */
    const void *keys[] = { kSecAttrKeyType, kSecAttrKeySizeInBits};
    const void *values[] = { keyType, keySizeNum };
    ourKeyParams = CFDictionaryCreate(NULL, keys, values, 2,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
    rv = SecKeyGeneratePair(ourKeyParams, &ourPubKey, &ourPrivKey);
    if (rv || !ourPubKey || !ourPrivKey) {
        dprintf("SecKeyGeneratePair: unable to generate ECDH key pair, %d\n", (int)rv);
        goto out;
    }

    /* Generate UKM */
    ukm->Data = PORT_Alloc(UKM_LENGTH);
    ukm->Length = UKM_LENGTH;
    rv = CCRandomCopyBytes(kCCRandomDefault, ukm->Data,  UKM_LENGTH);
    if (rv || !ukm->Data) {
        dprintf("CCRandomGenerateBytes failed, %d", (int)rv);
        goto out;
    }
    ukm->Length = UKM_LENGTH;

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
    if (!SEC_ASN1EncodeItem(poolp, &kekAlgId.parameters,
                            &ivData, kSecAsn1OctetStringTemplate)) {
        rv = errSecInternalComponent;
        goto out;
    }

    /* Drop in the KEK OID and encode the whole thing */
    kekOid = SECOID_FindOIDByTag(ECDH_KEK_ALG_TAG);
    if(kekOid == NULL) {
        dprintf("SecCmsUtilEncryptSymKeyECDH: OID screwup\n");
        rv = errSecInternalComponent;
        goto out;
    }
    kekAlgId.algorithm = kekOid->oid;
    memset(keyEncAlg, 0, sizeof(*keyEncAlg));
    if (!SEC_ASN1EncodeItem(poolp, &keyEncAlg->parameters,
                            &kekAlgId, SECOID_AlgorithmIDTemplate)) {
        rv = errSecInternalComponent;
        goto out;
    }
    kekOid = SECOID_FindOIDByTag(SEC_OID_DH_SINGLE_STD_SHA1KDF);
    if(kekOid == NULL) {
        dprintf("SecCmsUtilEncryptSymKeyECDH: OID screwup\n");
        rv = errSecInternalComponent;
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
    if (!SEC_ASN1EncodeItem(poolp, &sharedInfoEnc,
                            &sharedInfo, ECC_CMS_SharedInfoTemplate)) {
        rv = errSecInternalComponent;
        goto out;
    }
    dumpBuf("sender encoded SharedInfo", &sharedInfoEnc);

    /* Derive KEK */
    sharedInfoData = CFDataCreate(NULL, sharedInfoEnc.Data, sharedInfoEnc.Length);
    int32_t ecdh_key_key_len = ECDH_KEK_KEY_LEN_BYTES;
    kekLen = CFNumberCreate(NULL, kCFNumberSInt32Type, &ecdh_key_key_len);
    const void *kekKeys[] = { kSecKeyKeyExchangeParameterRequestedSize, kSecKeyKeyExchangeParameterSharedInfo };
    const void *kekValues[] = { kekLen, sharedInfoData };
    kekParams = CFDictionaryCreate(NULL, kekKeys, kekValues, 2,
                                   &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks);
    kekData = SecKeyCopyKeyExchangeResult(ourPrivKey, kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA1,
                                          theirPubKey, kekParams, &error);
    if (error) {
        dprintf("SecKeyCopyKeyExchangeResult: failed\n");
        goto out;
    }

    /*
     * Encrypt the raw CEK bits with the KEK we just derived
     */
    rv = CCCryptorCreate(kCCEncrypt, kCCAlgorithm3DES, kCCOptionPKCS7Padding,
                         CFDataGetBytePtr(kekData), CFDataGetLength(kekData), iv, &ciphercc);
    if (rv) {
        dprintf("CCCryptorCreate failed: %d\n", (int)rv);
        goto out;
    }

    size_t expectedEncKeyLength = CCCryptorGetOutputLength(ciphercc, CFDataGetLength(key), true);
    encKey->Data = PORT_ArenaAlloc(poolp, expectedEncKeyLength);
    size_t bytes_output = 0;
    rv = CCCryptorUpdate(ciphercc, CFDataGetBytePtr(key), CFDataGetLength(key), encKey->Data, expectedEncKeyLength, &bytes_output);
    if (rv) {
        dprintf("CCCryptorUpdate failed: %d\n", (int)rv);
        goto out;
    }
    size_t final_bytes_output = 0;
    rv = CCCryptorFinal(ciphercc, encKey->Data+bytes_output, expectedEncKeyLength - bytes_output, &final_bytes_output);
    if (rv) {
        dprintf("CCCryptorFinal failed: %d\n", (int)rv);
        goto out;
    }
    encKey->Length = bytes_output + final_bytes_output;

    /* Provide our ephemeral public key to the caller */
    ourPubData = SecKeyCopyExternalRepresentation(ourPubKey, &error);
    if (error) {
        dprintf("SecKeyCopyExternalRepresentation failed\n");
        goto out;
    }
    pubKey->Length = CFDataGetLength(ourPubData);
    pubKey->Data = malloc(pubKey->Length);
    if (pubKey->Data) {
        memcpy(pubKey->Data, CFDataGetBytePtr(ourPubData), pubKey->Length);
    } else {
        rv = errSecAllocate;
    }
    /* pubKey is bit string, convert here */
    pubKey->Length <<= 3;

out:
    if (theirPubKey) { CFRelease(theirPubKey); }
    if (theirKeyAttrs) { CFRelease(theirKeyAttrs); }
    if (ourKeyParams) { CFRelease(ourKeyParams); }
    if (ourPubKey) { CFRelease(ourPubKey); }
    if (ourPrivKey) { CFRelease(ourPrivKey); }
    if (sharedInfoData) { CFRelease(sharedInfoData); }
    if (kekLen) { CFRelease(kekLen); }
    if (kekParams) { CFRelease(kekParams); }
    if (kekData) { CFRelease(kekData); }
    if (error) { CFRelease(error); }
    if (ciphercc) { CCCryptorRelease(ciphercc); }
    if (ourPubData) { CFRelease(ourPubData); }
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


#pragma mark ---- ECDH CEK key unwrap ----

SecSymmetricKeyRef
SecCmsUtilDecryptSymKeyECDH(
                            SecPrivateKeyRef privkey,	/* our private key */
                            SecAsn1Item *encKey,	/* encrypted CEK */
                            SecAsn1Item *ukm,		/* random UKM from KeyAgreeRecipientInfo.ukm */
                            SECAlgorithmID *keyEncAlg,	/* alg := dhSinglePass-stdDH-sha1kdf-scheme
                                                         * params := another encoded AlgId, with the KEK alg and IV */
                            SECOidTag bulkalgtag,	/* algorithm of returned key */
                            SecAsn1Item *pubKey)	/* sender's pub key as ECPoint from
                                                         * KeyAgreeRecipientInfo.originator.OriginatorPublicKey */
{
    SecSymmetricKeyRef outKey = NULL;
    OSStatus rv = noErr;
    PLArenaPool *pool = NULL;
    SECAlgorithmID keyAlgParam;
    SECOidData *kekOid = NULL;
    SecAsn1Item iv = {0, NULL};
    ECC_CMS_SharedInfo sharedInfo;
    SecAsn1Item sharedInfoEnc = {0, NULL};
    uint8_t nullData[2] = {SEC_ASN1_NULL, 0};
    uint8_t keyLenAsBytes[4];
    uint32_t kekSizeBits;
    SecKeyRef theirPubKey = NULL;
    CFDictionaryRef theirKeyAttrs = NULL, kekParams = NULL;
    CFDataRef sharedInfoData = NULL, theirPubData= NULL, kekData = NULL;
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
    if(pool == NULL) {
        goto out;
    }
    memset(&keyAlgParam, 0, sizeof(keyAlgParam));
    if(SEC_ASN1DecodeItem(pool, &keyAlgParam, SECOID_AlgorithmIDTemplate,
                          &keyEncAlg->parameters)) {
        dprintf("SecCmsUtilDecryptSymKeyECDH: error decoding keyAlgParams\n");
        goto out;
    }
    kekOid = SECOID_FindOID(&keyAlgParam.algorithm);
    if(kekOid == NULL) {
        dprintf("SecCmsUtilDecryptSymKeyECDH: unknown KEK enc OID\n");
        goto out;
    }
    rv = encrAlgInfo(kekOid->offset, &kekSizeBits, &alg, &options);
    if(rv) {
        goto out;
    }
    /* IV is OCTET STRING in the alg params */
    if(SEC_ASN1DecodeItem(pool, &iv, kSecAsn1OctetStringTemplate,
                          &keyAlgParam.parameters)) {
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
    if (!SEC_ASN1EncodeItem(pool, &sharedInfoEnc,
                            &sharedInfo, ECC_CMS_SharedInfoTemplate)) {
        rv = errSecInternalComponent;
        goto out;
    }
    dumpBuf("receiver encoded SharedInfo", &sharedInfoEnc);
    dumpBuf("receiver IV", &iv);
    dumpBuf("receiver UKM", ukm);
    dumpBuf("sender's public key", pubKey);

    /* pubKey is bit string, convert here */
    theirKeySizeInBits = pubKey->Length;
    pubKey->Length = (theirKeySizeInBits + 7) >> 3;
    theirPubData = CFDataCreate(NULL, pubKey->Data, pubKey->Length);
    theirKeyLen = CFNumberCreate(NULL, kCFNumberSInt32Type, &theirKeySizeInBits);
    const void *keys[] = { kSecAttrKeyType, kSecAttrKeyClass, kSecAttrKeySizeInBits };
    const void *values[] = { kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeyClassPublic, theirKeyLen};
    theirKeyAttrs = CFDictionaryCreate(NULL, keys, values, 3,
                                       &kCFTypeDictionaryKeyCallBacks,
                                       &kCFTypeDictionaryValueCallBacks);
    theirPubKey = SecKeyCreateWithData(theirPubData, theirKeyAttrs, &error);
    if (error) {
        dprintf("SecKeyCreateWithData: failed\n");
        goto out;
    }

    /* Derive KEK */
    sharedInfoData = CFDataCreate(NULL, sharedInfoEnc.Data, sharedInfoEnc.Length);
    int32_t ecdh_key_key_len = (kekSizeBits + 7) >> 3;
    kekLen = CFNumberCreate(NULL, kCFNumberSInt32Type, &ecdh_key_key_len);
    const void *kekKeys[] = { kSecKeyKeyExchangeParameterRequestedSize, kSecKeyKeyExchangeParameterSharedInfo };
    const void *kekValues[] = { kekLen, sharedInfoData };
    kekParams = CFDictionaryCreate(NULL, kekKeys, kekValues, 2,
                                   &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks);
    kekData = SecKeyCopyKeyExchangeResult(privkey, kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA1,
                                          theirPubKey, kekParams, &error);
    if (error) {
        dprintf("SecKeyCopyKeyExchangeResult: failed\n");
        goto out;
    }

    /*
     * Decrypt the raw CEK bits with the KEK we just derived
     */
    SecAsn1Item cek = { 0, NULL };
    rv = CCCryptorCreate(kCCDecrypt, alg, options,
                         CFDataGetBytePtr(kekData), CFDataGetLength(kekData), iv.Data, &ciphercc);
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
    rv = CCCryptorFinal(ciphercc, cek.Data+bytes_output, expectedKeyLength - bytes_output, &final_bytes_output);
    if (rv) {
        dprintf("CCCryptorFinal failed: %d\n", (int)rv);
        goto out;
    }
    cek.Length = bytes_output + final_bytes_output;

    /* create the SecSymmetricKeyRef */
    outKey = (SecSymmetricKeyRef)CFDataCreate(NULL, cek.Data, cek.Length);

out:
    if(pool != NULL) {
        PORT_FreeArena(pool, PR_FALSE);
    }
    if (theirPubData) { CFRelease(theirPubData); }
    if (theirKeyLen) { CFRelease(theirKeyLen); }
    if (theirPubKey) { CFRelease(theirPubKey); }
    if (theirKeyAttrs) { CFRelease(theirKeyAttrs); }
    if (sharedInfoData) { CFRelease(sharedInfoData); }
    if (kekLen) { CFRelease(kekLen); }
    if (kekParams) { CFRelease(kekParams); }
    if (kekData) { CFRelease(kekData); }
    if (error) { CFRelease(error); }
    if (ciphercc) { CCCryptorRelease(ciphercc); }
    if(outKey == NULL) {
        PORT_SetError(SEC_ERROR_NO_KEY);
    }
    return outKey;
}
