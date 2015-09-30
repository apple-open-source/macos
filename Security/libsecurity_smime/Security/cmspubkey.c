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

#include <Security/SecCertificateInternal.h>
#include <Security/SecKeyPriv.h>

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
#if USE_CDSA_CRYPTO
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
#if USE_CDSA_CRYPTO
    rv = SecKeyGetStrengthInBits(publickey, NULL, &data_len);
    if (rv)
	goto loser;
    // Convert length to bytes;
    data_len >>= 2;
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

#if USE_CDSA_CRYPTO
    SecCertificateGetAlgorithmID(cert,&algid);
#endif

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
