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

#include "secitem.h"
#include "secoid.h"
#include "cryptohi.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/Security.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/SecCmsBase.h>
#include <Security/secasn1t.h>
#include <security_asn1/plarenas.h>
#include <Security/keyTemplates.h>

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
                              CSSM_DATA_PTR encKey)
{
    OSStatus rv;
    SecPublicKeyRef publickey;

    rv = SecCertificateCopyPublicKey(cert,&publickey);
    if (publickey == NULL)
	return SECFailure;

    rv = SecCmsUtilEncryptSymKeyRSAPubKey(poolp, publickey, bulkkey, encKey);
    CFRelease(publickey);
    return rv;
}

OSStatus
SecCmsUtilEncryptSymKeyRSAPubKey(PLArenaPool *poolp, 
				 SecPublicKeyRef publickey, 
				 SecSymmetricKeyRef bulkkey, CSSM_DATA_PTR encKey)
{
    OSStatus rv;
    unsigned int data_len;
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
    rv = SecKeyGetStrengthInBits(publickey, NULL, &data_len);
    if (rv)
	goto loser;

    // Convert length to bytes;
    data_len >>= 2;
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
SecCmsUtilDecryptSymKeyRSA(SecPrivateKeyRef privkey, CSSM_DATA_PTR encKey, SECOidTag bulkalgtag)
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
			SECOidTag symalgtag, CSSM_DATA_PTR encKey, CSSM_DATA_PTR *pparams, void *pwfn_arg)
{
    SECOidTag certalgtag;	/* the certificate's encryption algorithm */
    SECOidTag encalgtag;	/* the algorithm used for key exchange/agreement */
    OSStatus rv = SECFailure;
    CSSM_DATA_PTR params = NULL;
    OSStatus err;
    SecSymmetricKeyRef tek;
    SecCertificateRef ourCert;
    SecPublicKeyRef ourPubKey, *publickey = NULL;
    SecPrivateKeyRef ourPrivKey = NULL;
    SecCmsKEATemplateSelector whichKEA = SecCmsKEAInvalid;
    SecCmsSMIMEKEAParameters keaParams;
    PLArenaPool *arena = NULL;
    extern const SecAsn1Template *nss_cms_get_kea_template(SecCmsKEATemplateSelector whichTemplate);
    const SECAlgorithmID *algid;

    /* Clear keaParams, since cleanup code checks the lengths */
    (void) memset(&keaParams, 0, sizeof(keaParams));

    SecCertificateGetAlgorithmID(cert,&algid);
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
SecCmsUtilDecryptSymKeyMISSI(SecPrivateKeyRef privkey, CSSM_DATA_PTR encKey, SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag, void *pwfn_arg)
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
			CSSM_DATA_PTR encKey, CSSM_DATA_PTR ukm, SECAlgorithmID *keyEncAlg,
			CSSM_DATA_PTR pubKey)
{
#if 0 /* not yet done */
    SECOidTag certalgtag;	/* the certificate's encryption algorithm */
    SECOidTag encalgtag;	/* the algorithm used for key exchange/agreement */
    OSStatus rv;
    CSSM_DATA_PTR params = NULL;
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
SecCmsUtilDecryptSymKeyESDH(SecPrivateKeyRef privkey, CSSM_DATA_PTR encKey, SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag, void *pwfn_arg)
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

#endif	/* Fortezza, DIffie-Hellman */

#define CFRELEASE(cf)	if(cf != NULL) { CFRelease(cf); }

/* ====== ECDH (Ephemeral-Static Diffie-Hellman) ==================================== */

#pragma mark ---- ECDH support functions ----

#ifdef	NDEBUG
#define CSSM_PERROR(f, r)
#define dprintf(args...)
#else
#define CSSM_PERROR(f, r)   cssmPerror(f, r)
#define dprintf(args...)    printf(args)
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
    CSSM_DATA	    entityUInfo;    /* optional, ukm */
    CSSM_DATA	    suppPubInfo;    /* length of KEK in bits as 4-byte integer */
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

static CSSM_RETURN cmsAddContextAttribute(
    CSSM_CC_HANDLE CCHandle,
    uint32 AttributeType,
    uint32 AttributeLength,
    ContextAttrType attrType,
    /* specify exactly one of these */
    const void *AttributePtr,
    uint32 attributeInt)
{
    CSSM_CONTEXT_ATTRIBUTE		newAttr;	
    CSSM_RETURN					crtn;
    
    newAttr.AttributeType     = AttributeType;
    newAttr.AttributeLength   = AttributeLength;
    if(attrType == CAT_Uint32) {
	    newAttr.Attribute.Uint32  = attributeInt;
    }
    else {
	    /* this is a union of a bunch of different pointers...*/
	    newAttr.Attribute.Data    = (CSSM_DATA_PTR)AttributePtr;
    }
    crtn = CSSM_UpdateContextAttributes(CCHandle, 1, &newAttr);
    if(crtn) {
	CSSM_PERROR("CSSM_UpdateContextAttributes", crtn);
    }
    return crtn;
}

static CSSM_RETURN cmsGenRand(
    CSSM_CSP_HANDLE cspHand,
    CSSM_SIZE len,
    uint8 *randOut)
{
    CSSM_CC_HANDLE ccHand = 0;
    CSSM_DATA randData = {len, randOut};
    
    CSSM_RETURN crtn = CSSM_CSP_CreateRandomGenContext(cspHand,
	    CSSM_ALGID_APPLE_YARROW,
	    NULL, /* seed*/
	    len,
	    &ccHand);
    if(crtn) {
	CSSM_PERROR("CSSM_CSP_CreateRandomGenContext", crtn);
	return crtn;
    }
    crtn = CSSM_GenerateRandom(ccHand, &randData);
    CSSM_DeleteContext(ccHand);
    if(crtn) {
	CSSM_PERROR("CSSM_GenerateRandom", crtn);
    }
    return crtn;
}

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
 * NULL wrap a ref key to raw key in default format. 
 */ 
static OSStatus cmsNullWrapKey(
    CSSM_CSP_HANDLE cspHand,
    const CSSM_KEY *refKey,
    CSSM_KEY_PTR rawKey)
{
    CSSM_DATA descData = {0, 0};
    CSSM_RETURN crtn;
    CSSM_CC_HANDLE ccHand;
    CSSM_ACCESS_CREDENTIALS creds;
    uint32 keyAttr;
    
    memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));	
    memset(rawKey, 0, sizeof(CSSM_KEY));
    
    crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
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
	    refKey,
	    &descData,
	    rawKey);
    if(crtn != CSSM_OK) {
	CSSM_PERROR("CSSM_WrapKey", crtn);
    }
    CSSM_DeleteContext(ccHand);
    return crtn;
}

/*
 * Free memory via specified plugin's app-level allocator
 */
void cmsFreeCssmMemory(
    CSSM_HANDLE	hand,
    void	*p)
{
    CSSM_API_MEMORY_FUNCS memFuncs;
    CSSM_RETURN crtn = CSSM_GetAPIMemoryFunctions(hand, &memFuncs);
    if(crtn) {
	    return;
    }
    memFuncs.free_func(p, memFuncs.AllocRef);
}

/* 
 * Given an OID tag, return key size and mode. 
 * NOTE: ciphers with variable key sizes, like RC2, RC4, and RC5 cannot
 * be used here because the message does not contain a key size
 * indication. 
 */
static OSStatus encrAlgInfo(
    SECOidTag		oidTag,
    uint32		*keySizeBits,	/* RETURNED */
    CSSM_ENCRYPT_MODE	*mode)		/* RETURNED */
{
    *keySizeBits = 64;		    /* default */
    *mode = CSSM_ALGMODE_CBCPadIV8; /* default */
    
    switch(oidTag) {
	case SEC_OID_RC2_CBC:
	case SEC_OID_RC4:
	case SEC_OID_RC5_CBC_PAD:
	    dprintf("encrAlgInfo: key size unknowable\n");
	    return errSecDataNotAvailable;
	    
	case SEC_OID_DES_EDE3_CBC:
	    *keySizeBits = 192;
	    break;
	case SEC_OID_DES_EDE:
	    /* Not sure about this; SecCmsCipherContextStart() treats this
	     * like SEC_OID_DES_EDE3_CBC... */
	case SEC_OID_DES_ECB:
	    *mode = CSSM_ALGMODE_ECB;
	    break;
	case SEC_OID_DES_CBC:
	    *mode = CSSM_ALGMODE_CBC;
	    break;
	case SEC_OID_AES_128_CBC:
	    *keySizeBits = 128;
	    break;
	case SEC_OID_AES_192_CBC:
	    *keySizeBits = 192;
	    break;
	case SEC_OID_AES_256_CBC:
	    *keySizeBits = 256;
	    break;
	case SEC_OID_AES_128_ECB:
	    *keySizeBits = 128;
	    *mode = CSSM_ALGMODE_ECB;
	    break;
	case SEC_OID_AES_192_ECB:
	    *keySizeBits = 192;
	    *mode = CSSM_ALGMODE_ECB;
	    break;
	case SEC_OID_AES_256_ECB:
	    *keySizeBits = 256;
	    *mode = CSSM_ALGMODE_ECB;
	    break;
	case SEC_OID_DES_OFB:
	    *mode = CSSM_ALGMODE_OFB;
	    break;
	case SEC_OID_DES_CFB:
	    *mode = CSSM_ALGMODE_CFB;
	    break;
	default:
	    dprintf("encrAlgInfo: unknown alg tag (%d)\n", (int)oidTag);
	    return errSecDataNotAvailable;	    
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
    CSSM_DATA_PTR encKey,	/* encrypted key --> recipientEncryptedKeys[0].EncryptedKey */
    CSSM_DATA_PTR ukm,		/* random UKM --> KeyAgreeRecipientInfo.ukm */
    SECAlgorithmID *keyEncAlg,	/* alg := dhSinglePass-stdDH-sha1kdf-scheme
				 * params := another encoded AlgId, with the KEK alg and IV */
    CSSM_DATA_PTR pubKey)	/* our pub key as ECPoint --> 
				 * KeyAgreeRecipientInfo.originator.OriginatorPublicKey */
{
    OSStatus rv = noErr;
    CSSM_KEY ourPrivKeyCssm;
    CSSM_KEY ourPubKeyCssm;
    SecKeyRef theirPubKeyRef = NULL;
    CSSM_KEY_PTR theirPubKeyCssm = NULL;
    const CSSM_KEY *cekCssmRef = NULL;
    uint32 ecdhKeySizeBits;
    CSSM_CSP_HANDLE rawCspHand = SecCspHandleForAlgorithm(CSSM_ALGID_ECDH);
    CSSM_CC_HANDLE ccHand = 0;
    CSSM_RETURN crtn;
    CSSM_DATA keyLabel = {8, (uint8 *)"tempKey"};
    SECAlgorithmID kekAlgId;
    uint8 iv[ECDH_KEK_IV_LEN_BYTES];
    CSSM_DATA ivData = {ECDH_KEK_IV_LEN_BYTES, iv};
    SECOidData *kekOid;
    ECC_CMS_SharedInfo sharedInfo;
    CSSM_DATA sharedInfoEnc = {0, NULL};
    uint8 nullData[2] = {SEC_ASN1_NULL, 0};
    uint8 keyLenAsBytes[4];
    CSSM_KEY kekDerive;
    CSSM_DATA certData;
    CSSM_CL_HANDLE clHand;
    CSSM_ACCESS_CREDENTIALS creds;
    CSSM_DATA paramData = {0, NULL};
    CSSM_KEY cekCssm;
    CSSM_CSP_HANDLE refCspHand;
    CSSM_SIZE bytesEncrypted;
    CSSM_DATA remData = {0, NULL};
    CSSM_DATA ctext = {0, NULL};
    CSSM_X509_SUBJECT_PUBLIC_KEY_INFO subjPubKey;
    
    if(rawCspHand == 0) {
	return internalComponentErr;
    }
    
    memset(&ourPrivKeyCssm, 0, sizeof(CSSM_KEY));
    memset(&ourPubKeyCssm, 0, sizeof(CSSM_KEY));
    memset(&cekCssm, 0, sizeof(CSSM_KEY));
    memset(&kekDerive, 0, sizeof(kekDerive));
   
    encKey->Data = NULL;
    encKey->Length = 0;
    
    /* 
     * Create our ECDH key pair matching the recipient's key.
     * Get the public key in "read-only" OCTET_STRING format, which 
     * is the ECPoint we put in 
     * KeyAgreeRecipientInfo.originator.OriginatorPublicKey.
     */
    rv = SecCertificateGetData(cert, &certData);
    if(rv) {
	CSSM_PERROR("SecCertificateGetData", rv);
	return rv;
    }
    rv = SecCertificateGetCLHandle(cert, &clHand);
    if(rv) {
	CSSM_PERROR("SecCertificateGetCLHandle", rv);
	return rv;
    }
    rv = CSSM_CL_CertGetKeyInfo(clHand, &certData, &theirPubKeyCssm);
     if(rv) {
	CSSM_PERROR("CSSM_CL_CertGetKeyInfo", rv);
	return rv;
    }
   
    /* 
     * Verify the EC curve of the recipient's public key. It's in the 
     * public key's AlgId.parameters as an OID. The key we were
     * given is in CSSM_X509_SUBJECT_PUBLIC_KEY_INFO form.
     */
    memset(&subjPubKey, 0, sizeof(subjPubKey));
    if(SEC_ASN1DecodeItem(poolp, &subjPubKey, kSecAsn1SubjectPublicKeyInfoTemplate, 
	    &theirPubKeyCssm->KeyData)) {
	dprintf("SecCmsUtilEncryptSymKeyECDH: error decoding SubjPubKey\n");
	/* oh well, keep going */
    }
    else {
	if(subjPubKey.algorithm.parameters.Data != NULL) {
	    CSSM_DATA curveOid;
	    if(SEC_ASN1DecodeItem(poolp, &curveOid, kSecAsn1ObjectIDTemplate, 
		    &subjPubKey.algorithm.parameters)) {
		dprintf("SecCmsUtilEncryptSymKeyECDH: error decoding curveOid\n");
		/* oh well, keep going */
	    }
	    else {
		/* We have the curve OID. Any other errors are fatal. */
		SECOidTag oidTag = SECOID_FindOIDTag(&curveOid);
		switch(oidTag) {
		    case SEC_OID_SECP_256_R1:
		    case SEC_OID_SECP_384_R1:
		    case SEC_OID_SECP_521_R1:
			break;
		    default:
			dprintf("SecCmsUtilEncryptSymKeyECDH: unsupported curveOid\n");
			rv = CSSMERR_CSP_INVALID_KEY;
			goto loser;
		}
	    }
	}
    }
    
    ecdhKeySizeBits = theirPubKeyCssm->KeyHeader.LogicalKeySizeInBits;
    crtn = CSSM_CSP_CreateKeyGenContext(rawCspHand,
	    CSSM_ALGID_ECDSA,
	    ecdhKeySizeBits,
	    NULL,			// Seed
	    NULL,			// Salt
	    NULL,			// StartDate
	    NULL,			// EndDate
	    NULL,			// Params
	    &ccHand);
    if(crtn) {
	CSSM_PERROR("CSSM_CSP_CreateKeyGenContext", crtn);
	rv = crtn;
	goto loser;
    }
    crtn = cmsAddContextAttribute(ccHand,
	CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT,
	sizeof(uint32),	
	CAT_Uint32,
	NULL,
	CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING);
    if(crtn) {
	CSSM_PERROR("AddContextAttribute(CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT)", crtn);
	rv = crtn;
	goto loser;
    }

    crtn = CSSM_GenerateKeyPair(ccHand,
	    CSSM_KEYUSE_DERIVE,
	    CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE,
	    &keyLabel,
	    &ourPubKeyCssm,
	    CSSM_KEYUSE_DERIVE,
	    CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE,
	    &keyLabel,		
	    NULL,			// CredAndAclEntry
	    &ourPrivKeyCssm);
    CSSM_DeleteContext(ccHand);
    ccHand = 0;
    if(crtn) {
	CSSM_PERROR("CSSM_GenerateKeyPair", crtn);
	rv = crtn;
	goto loser;
    }
    pubKey->Length = ourPubKeyCssm.KeyData.Length;
    pubKey->Data = (uint8 *)PORT_ArenaAlloc(poolp, pubKey->Length);
    memmove(pubKey->Data, ourPubKeyCssm.KeyData.Data, pubKey->Length);
    dumpBuf("sender's public key", pubKey);
    
    /*
     * Cook up random UKM 
     */
    ukm->Data = (uint8 *)PORT_ArenaAlloc(poolp, UKM_LENGTH);
    ukm->Length = UKM_LENGTH;
    crtn = cmsGenRand(rawCspHand, UKM_LENGTH, ukm->Data);
    if(crtn) {
	goto loser;
    }
    dumpBuf("sender UKM", ukm);
    
    /*
     * OK, we have to set up a weird SECAlgorithmID.
     * algorithm = dhSinglePass-stdDH-sha1kdf-scheme
     * params = an encoded SECAlgorithmID representing the KEK algorithm, with 
     *   algorithm = whatever we pick
     *   parameters = IV as octet string (though I haven't seen that specified
     *		      anywhere; it's how the CEK IV is encoded)
     *
     * First, the 8-byte random IV, encoded as octet string
     */
    crtn = cmsGenRand(rawCspHand, ECDH_KEK_IV_LEN_BYTES, iv);
    if(crtn) {
	goto loser;
    }
    dumpBuf("sender IV", &ivData);
    
    memset(&kekAlgId, 0, sizeof(kekAlgId));
    if (!SEC_ASN1EncodeItem(poolp, &kekAlgId.parameters,
	    &ivData, kSecAsn1OctetStringTemplate)) {
	rv = internalComponentErr;
	goto loser;
    }

    /* Drop in the KEK OID and encode the whole thing */
    kekOid = SECOID_FindOIDByTag(ECDH_KEK_ALG_TAG);
    if(kekOid == NULL) {
	dprintf("SecCmsUtilEncryptSymKeyECDH: OID screwup\n");
	rv = internalComponentErr;
	goto loser;
    }
    kekAlgId.algorithm = kekOid->oid;
    memset(keyEncAlg, 0, sizeof(*keyEncAlg));
    if (!SEC_ASN1EncodeItem(poolp, &keyEncAlg->parameters,
	    &kekAlgId, SECOID_AlgorithmIDTemplate)) {
	rv = internalComponentErr;
	goto loser;
    }
    kekOid = SECOID_FindOIDByTag(SEC_OID_DH_SINGLE_STD_SHA1KDF);
    if(kekOid == NULL) {
	dprintf("SecCmsUtilEncryptSymKeyECDH: OID screwup\n");
	rv = internalComponentErr;
	goto loser;
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
	rv = internalComponentErr;
	goto loser;
    }
    dumpBuf("sender encoded SharedInfo", &sharedInfoEnc);
   
    /*
     * Since we're using the raw CSP here, we can provide the "other" public 
     * key as an actual CSSM_KEY. When unwrapping, we won't be able to do that
     * since we'll be using our private key obtained from a SecIdentityRef. 
     */
    memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
    crtn = CSSM_CSP_CreateDeriveKeyContext(rawCspHand,
		CSSM_ALGID_ECDH_X963_KDF,
		ECDH_KEK_KEY_CSSM_ALGID,	// algorithm of the KEK
		ECDH_KEK_KEY_LEN_BYTES * 8,
		&creds,
		&ourPrivKeyCssm,	// BaseKey
		0,			// IterationCount
		&sharedInfoEnc,		// Salt
		0,			// Seed
		&ccHand);
    if(crtn) {
	CSSM_PERROR("CSSM_CSP_CreateDeriveKeyContext", crtn);
	rv = crtn;
	goto loser;
    }
    
    /* add recipient's pub key as a context attr */
    crtn = cmsAddContextAttribute(ccHand,
	    CSSM_ATTRIBUTE_PUBLIC_KEY,
	    sizeof(CSSM_KEY),	
	    CAT_Ptr,
	    (void *)theirPubKeyCssm,
	    0);
    if(crtn) {
	rv = crtn;
	goto loser;
    }
 
    /* Derive the KEK */
    crtn = CSSM_DeriveKey(ccHand,
	    &paramData,
	    CSSM_KEYUSE_ANY,
	    CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE,
	    &keyLabel,
	    NULL,				// cread/acl
	    &kekDerive);
    if(crtn) {
	CSSM_PERROR("CSSM_DeriveKey", crtn);
	rv = crtn;
	goto loser;
    }
    CSSM_DeleteContext(ccHand);
    ccHand = 0;
   
    /* 
     * Obtain the raw CEK bits.
     */
    rv = SecKeyGetCSSMKey(key, &cekCssmRef);
    if(rv) {
	CSSM_PERROR("SecKeyGetCSSMKey", rv);
	goto loser;
    }
    rv = SecKeyGetCSPHandle(key, &refCspHand);
    if(rv) {
	CSSM_PERROR("SecKeyGetCSPHandle", rv);
	goto loser;
    }
    rv = cmsNullWrapKey(refCspHand, cekCssmRef, &cekCssm);
    if(rv) {
	goto loser;
    }
    
    /* 
     * Finally, encrypt the raw CEK bits with the KEK we just derived
     */
    crtn = CSSM_CSP_CreateSymmetricContext(rawCspHand,
	    ECDH_KEK_ENCR_CSSM_ALGID,
	    CSSM_ALGMODE_CBCPadIV8,
	    NULL,			// access cred
	    &kekDerive,
	    &ivData,			// InitVector
	    CSSM_PADDING_PKCS7,	
	    NULL,			// Params
	    &ccHand);
    if(rv) {
	CSSM_PERROR("CSSM_CSP_CreateSymmetricContext", rv);
	goto loser;
    }
    rv = CSSM_EncryptData(ccHand,
	    &cekCssm.KeyData,
	    1,
	    &ctext,
	    1,
	    &bytesEncrypted,
	    &remData);
    if(rv) {
	CSSM_PERROR("CSSM_EncryptData", rv);
	goto loser;
    }
    encKey->Data = PORT_ArenaAlloc(poolp, bytesEncrypted);
    encKey->Length = bytesEncrypted;
    memmove(encKey->Data, ctext.Data, ctext.Length);
    if(bytesEncrypted != ctext.Length) {
	memmove(encKey->Data + ctext.Length, remData.Data, remData.Length);
    }
    dumpBuf("sender encKey", encKey);
    
loser:
    if(ccHand) {
	CSSM_DeleteContext(ccHand);
    }
    CFRELEASE(theirPubKeyRef);
    if(ourPubKeyCssm.KeyData.Data) {
	CSSM_FreeKey(rawCspHand, NULL, &ourPubKeyCssm, CSSM_FALSE);
    }
    if(ourPrivKeyCssm.KeyData.Data) {
	CSSM_FreeKey(rawCspHand, NULL, &ourPrivKeyCssm, CSSM_FALSE);
    }
    if(ctext.Data) {
	cmsFreeCssmMemory(rawCspHand, ctext.Data);
    }
    if(remData.Data) {
	cmsFreeCssmMemory(rawCspHand, remData.Data);
    }
    if(cekCssm.KeyData.Data) {
	CSSM_FreeKey(refCspHand, NULL, &cekCssm, CSSM_FALSE);
    }
    if(kekDerive.KeyData.Data) {
	CSSM_FreeKey(rawCspHand, NULL, &kekDerive, CSSM_FALSE);
    }
    if(theirPubKeyCssm) {
	/* Allocated by CL */
	cmsFreeCssmMemory(clHand, theirPubKeyCssm->KeyData.Data);
	cmsFreeCssmMemory(clHand, theirPubKeyCssm);
    }
    return rv;
}

#pragma mark ---- ECDH CEK key unwrap ----

SecSymmetricKeyRef
SecCmsUtilDecryptSymKeyECDH(
    SecPrivateKeyRef privkey,	/* our private key */
    CSSM_DATA_PTR encKey,	/* encrypted CEK */
    CSSM_DATA_PTR ukm,		/* random UKM from KeyAgreeRecipientInfo.ukm */
    SECAlgorithmID *keyEncAlg,	/* alg := dhSinglePass-stdDH-sha1kdf-scheme
				 * params := another encoded AlgId, with the KEK alg and IV */
    SECOidTag bulkalgtag,	/* algorithm of returned key */
    CSSM_DATA_PTR pubKey)	/* sender's pub key as ECPoint from
				 * KeyAgreeRecipientInfo.originator.OriginatorPublicKey */
    
{
    SecSymmetricKeyRef outKey = NULL;
    OSStatus rv = noErr;
    const CSSM_KEY *ourPrivKeyCssm;
    PLArenaPool *pool = NULL;
    SECAlgorithmID keyAlgParam;
    SECOidData *kekOid = NULL;
    CSSM_DATA iv = {0, NULL};
    ECC_CMS_SharedInfo sharedInfo;
    CSSM_DATA sharedInfoEnc = {0, NULL};
    uint8 nullData[2] = {SEC_ASN1_NULL, 0};
    uint8 keyLenAsBytes[4];
    CSSM_ENCRYPT_MODE kekMode;
    uint32 kekSizeBits;
    CSSM_KEY kekDerive;
    CSSM_RETURN crtn;
    CSSM_ACCESS_CREDENTIALS creds;
    CSSM_CSP_HANDLE refCspHand;
    CSSM_CC_HANDLE ccHand = 0;
    CSSM_DATA keyLabel = {8, (uint8 *)"tempKey"};
    const CSSM_ACCESS_CREDENTIALS *accessCred;
    CSSM_KEY wrappedKey;
    CSSM_KEY unwrappedKey;
    CSSM_ALGORITHMS bulkAlg;
    CSSM_DATA descriptiveData = {0, NULL};
    
    dumpBuf("receiver encKey", encKey);
 
    memset(&kekDerive, 0, sizeof(kekDerive));

    /* our private key in CSSM form */
    rv = SecKeyGetCSSMKey(privkey, &ourPrivKeyCssm);
    if(rv) {
	CSSM_PERROR("SecKeyGetCSSMKey", rv);
	goto loser;
    }
    
    /* 
     * Decode keyEncAlg.params to get KEK algorithm and IV
     */ 
    pool = PORT_NewArena(1024);
    if(pool = NULL) {
	goto loser;
    }
    memset(&keyAlgParam, 0, sizeof(keyAlgParam));
    if(SEC_ASN1DecodeItem(pool, &keyAlgParam, SECOID_AlgorithmIDTemplate, 
	    &keyEncAlg->parameters)) {
	dprintf("SecCmsUtilDecryptSymKeyECDH: error decoding keyAlgParams\n");
	goto loser;
    }
    kekOid = SECOID_FindOID(&keyAlgParam.algorithm);
    if(kekOid == NULL) {
	dprintf("SecCmsUtilDecryptSymKeyECDH: unknown KEK enc OID\n");
	goto loser;
    }
    rv = encrAlgInfo(kekOid->offset, &kekSizeBits, &kekMode);
    if(rv) {
	goto loser;
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
	goto loser;
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
	rv = internalComponentErr;
	goto loser;
    }
    dumpBuf("receiver encoded SharedInfo", &sharedInfoEnc);
    dumpBuf("receiver IV", &iv);
    dumpBuf("receiver UKM", ukm);
    dumpBuf("sender's public key", pubKey);

    /* 
     * Using the Sec-layer CSPDL, "other's" public key specified as ECPOint param. Which
     * is fortunate because that's what we have...
     */
    memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
    rv = SecKeyGetCSPHandle(privkey, &refCspHand);
    if(rv) {
	CSSM_PERROR("SecKeyGetCSPHandle", rv);
	goto loser;
    }
    rv = SecKeyGetCredentials(privkey,
	    CSSM_ACL_AUTHORIZATION_DERIVE,
	    kSecCredentialTypeDefault,
	    &accessCred);
    if (rv) {
	CSSM_PERROR("SecKeyGetCredentials", rv);
	goto loser;
    }
    crtn = CSSM_CSP_CreateDeriveKeyContext(refCspHand,
		CSSM_ALGID_ECDH_X963_KDF,
		kekOid->cssmAlgorithm,	// algorithm of the KEK
		kekSizeBits,
		&creds,
		ourPrivKeyCssm,		// BaseKey
		0,			// IterationCount
		&sharedInfoEnc,		// Salt
		0,			// Seed
		&ccHand);
    if(crtn) {
	CSSM_PERROR("CSSM_CSP_CreateDeriveKeyContext", crtn);
	goto loser;
    }
    crtn = CSSM_DeriveKey(ccHand,
	    pubKey,			// param
	    CSSM_KEYUSE_ANY,
	    CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE,
	    &keyLabel,
	    NULL,			// cred/acl
	    &kekDerive);
    CSSM_DeleteContext(ccHand);
    ccHand = 0;
    if(crtn) {
	CSSM_PERROR("CSSM_DeriveKey", crtn);
	goto loser;
    }
 
    /* 
     * Decrypt the encrypted key bits with the KEK key.
     */
    crtn = CSSM_CSP_CreateSymmetricContext(refCspHand,
	    kekOid->cssmAlgorithm,
	    kekMode,
	    NULL,			// access cred
	    &kekDerive,
	    &iv,			// InitVector
	    /* FIXME is this variable too? */
	    CSSM_PADDING_PKCS7,	
	    NULL,			// Params
	    &ccHand);
    if(rv) {
	CSSM_PERROR("CSSM_CSP_CreateSymmetricContext", rv);
	goto loser;
    }
    
    memset(&wrappedKey, 0, sizeof(CSSM_KEY));
    memset(&unwrappedKey, 0, sizeof(CSSM_KEY));

    bulkAlg = SECOID_FindyCssmAlgorithmByTag(bulkalgtag);
    if(bulkAlg == CSSM_ALGID_NONE) {
	dprintf("SecCmsUtilDecryptSymKeyECDH: unknown bulk alg\n");
	goto loser;
    }
    
    wrappedKey.KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
    wrappedKey.KeyHeader.BlobType = CSSM_KEYBLOB_WRAPPED;
    wrappedKey.KeyHeader.Format = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7;
    wrappedKey.KeyHeader.AlgorithmId = bulkAlg;
    wrappedKey.KeyHeader.KeyClass = CSSM_KEYCLASS_SESSION_KEY;
    wrappedKey.KeyHeader.WrapAlgorithmId = kekOid->cssmAlgorithm;
    wrappedKey.KeyHeader.WrapMode = CSSM_ALGMODE_NONE; 
    wrappedKey.KeyData = *encKey;
    
    crtn = CSSM_UnwrapKey(ccHand,
	    NULL, /* publicKey */
	    &wrappedKey,
	    CSSM_KEYUSE_DECRYPT,
	    CSSM_KEYATTR_EXTRACTABLE,
	    &keyLabel,
	    NULL, /* rcc */
	    &unwrappedKey,
	    &descriptiveData);
    CSSM_DeleteContext(ccHand);
    ccHand = 0;
    if(crtn) {
	CSSM_PERROR("CSSM_UnwrapKey", crtn);
	goto loser;
    }
    rv = SecKeyCreateWithCSSMKey(&unwrappedKey, &outKey);
    if (rv) {
	CSSM_PERROR("SecKeyCreateWithCSSMKey", rv);
    }
  
loser:
    if(pool != NULL) {
	PORT_FreeArena(pool, PR_FALSE);
    }
    if(kekDerive.KeyData.Data) {
	CSSM_FreeKey(refCspHand, NULL, &kekDerive, CSSM_FALSE);
    }
    if(outKey == NULL) {
	PORT_SetError(SEC_ERROR_NO_KEY);    
    }
    return outKey;
}



