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
 * CMS recipientInfo methods.
 */

#include "cmslocal.h"

#include "cert.h"
#include "secitem.h"
#include "secoid.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecCertificatePriv.h>

Boolean
nss_cmsrecipientinfo_usessubjectkeyid(SecCmsRecipientInfoRef ri)
{
    if (ri->recipientInfoType == SecCmsRecipientInfoIDKeyTrans) {
	SecCmsRecipientIdentifier *rid;
	rid = &ri->ri.keyTransRecipientInfo.recipientIdentifier;
	if (rid->identifierType == SecCmsRecipientIDSubjectKeyID) {
	    return PR_TRUE;
	}
    }
    return PR_FALSE;
}


SecCmsRecipientInfoRef
nss_cmsrecipientinfo_create(SecCmsMessageRef cmsg, SecCmsRecipientIDSelector type,
                            SecCertificateRef cert, SecPublicKeyRef pubKey, 
                            CSSM_DATA_PTR subjKeyID)
{
    SecCmsRecipientInfoRef ri;
    void *mark;
    SECOidTag certalgtag;
    OSStatus rv = SECSuccess;
    SecCmsRecipientEncryptedKey *rek;
    SecCmsOriginatorIdentifierOrKey *oiok;
    unsigned long version;
    CSSM_DATA_PTR dummy;
    PLArenaPool *poolp;
    const SECAlgorithmID *algid;
    SecCmsRecipientIdentifier *rid;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    ri = (SecCmsRecipientInfoRef)PORT_ArenaZAlloc(poolp, sizeof(SecCmsRecipientInfo));
    if (ri == NULL)
	goto loser;

    ri->cmsg = cmsg;
    if (type == SecCmsRecipientIDIssuerSN)
    {
	ri->cert = CERT_DupCertificate(cert);
	if (ri->cert == NULL)
		goto loser;
	rv = SecCertificateGetAlgorithmID(cert,&algid);
    } else {
	PORT_Assert(pubKey);
	rv = SecKeyGetAlgorithmID(pubKey,&algid);
    }

    certalgtag = SECOID_GetAlgorithmTag(algid);

    rid = &ri->ri.keyTransRecipientInfo.recipientIdentifier;
    switch (certalgtag) {
    case SEC_OID_PKCS1_RSA_ENCRYPTION:
	ri->recipientInfoType = SecCmsRecipientInfoIDKeyTrans;
	rid->identifierType = type;
	if (type == SecCmsRecipientIDIssuerSN) {
	    rid->id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert);
	    if (rid->id.issuerAndSN == NULL) {
	      break;
	    }
	} else if (type == SecCmsRecipientIDSubjectKeyID){
	    SecCmsKeyTransRecipientInfoEx *riExtra;

	    rid->id.subjectKeyID = PORT_ArenaNew(poolp, CSSM_DATA);
	    if (rid->id.subjectKeyID == NULL) {
		rv = SECFailure;
		PORT_SetError(SEC_ERROR_NO_MEMORY);
		break;
	    } 
	    SECITEM_CopyItem(poolp, rid->id.subjectKeyID, subjKeyID);
	    if (rid->id.subjectKeyID->Data == NULL) {
		rv = SECFailure;
		PORT_SetError(SEC_ERROR_NO_MEMORY);
		break;
	    }
	    riExtra = &ri->ri.keyTransRecipientInfoEx;
	    riExtra->version = 0;
	    riExtra->pubKey = SECKEY_CopyPublicKey(pubKey);
	    if (riExtra->pubKey == NULL) {
		rv = SECFailure;
		PORT_SetError(SEC_ERROR_NO_MEMORY);
		break;
	    }
	} else {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    rv = SECFailure;
	}
	break;
    case SEC_OID_MISSI_KEA_DSS_OLD:
    case SEC_OID_MISSI_KEA_DSS:
    case SEC_OID_MISSI_KEA:
        PORT_Assert(type != SecCmsRecipientIDSubjectKeyID);
	if (type == SecCmsRecipientIDSubjectKeyID) {
	    rv = SECFailure;
	    break;
	}
	/* backward compatibility - this is not really a keytrans operation */
	ri->recipientInfoType = SecCmsRecipientInfoIDKeyTrans;
	/* hardcoded issuerSN choice for now */
	ri->ri.keyTransRecipientInfo.recipientIdentifier.identifierType = SecCmsRecipientIDIssuerSN;
	ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert);
	if (ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN == NULL) {
	    rv = SECFailure;
	    break;
	}
	break;
    case SEC_OID_X942_DIFFIE_HELMAN_KEY: /* dh-public-number */
        PORT_Assert(type != SecCmsRecipientIDSubjectKeyID);
	if (type == SecCmsRecipientIDSubjectKeyID) {
	    rv = SECFailure;
	    break;
	}
	/* a key agreement op */
	ri->recipientInfoType = SecCmsRecipientInfoIDKeyAgree;

	if (ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN == NULL) {
	    rv = SECFailure;
	    break;
	}
	/* we do not support the case where multiple recipients 
	 * share the same KeyAgreeRecipientInfo and have multiple RecipientEncryptedKeys
	 * in this case, we would need to walk all the recipientInfos, take the
	 * ones that do KeyAgreement algorithms and join them, algorithm by algorithm
	 * Then, we'd generate ONE ukm and OriginatorIdentifierOrKey */

	/* only epheremal-static Diffie-Hellman is supported for now
	 * this is the only form of key agreement that provides potential anonymity
	 * of the sender, plus we do not have to include certs in the message */

	/* force single recipientEncryptedKey for now */
	if ((rek = SecCmsRecipientEncryptedKeyCreate(poolp)) == NULL) {
	    rv = SECFailure;
	    break;
	}

	/* hardcoded IssuerSN choice for now */
	rek->recipientIdentifier.identifierType = SecCmsKeyAgreeRecipientIDIssuerSN;
	if ((rek->recipientIdentifier.id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert)) == NULL) {
	    rv = SECFailure;
	    break;
	}

	oiok = &(ri->ri.keyAgreeRecipientInfo.originatorIdentifierOrKey);

	/* see RFC2630 12.3.1.1 */
	oiok->identifierType = SecCmsOriginatorIDOrKeyOriginatorPublicKey;

	rv = SecCmsArrayAdd(poolp, (void ***)&ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys,
				    (void *)rek);

	break;
	
    case SEC_OID_EC_PUBLIC_KEY:	    
	/* ephemeral-static ECDH - issuerAndSN, OriginatorPublicKey only */
        PORT_Assert(type != SecCmsRecipientIDSubjectKeyID);
	if (type == SecCmsRecipientIDSubjectKeyID) {
	    rv = SECFailure;
	    break;
	}
	/* a key agreement op */
	ri->recipientInfoType = SecCmsRecipientInfoIDKeyAgree;
	ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert);
	if (ri->ri.keyTransRecipientInfo.recipientIdentifier.id.issuerAndSN == NULL) {
	    rv = SECFailure;
	    break;
	}
	/* we do not support the case where multiple recipients 
	 * share the same KeyAgreeRecipientInfo and have multiple RecipientEncryptedKeys
	 * in this case, we would need to walk all the recipientInfos, take the
	 * ones that do KeyAgreement algorithms and join them, algorithm by algorithm
	 * Then, we'd generate ONE ukm and OriginatorIdentifierOrKey */

	/* force single recipientEncryptedKey for now */
	if ((rek = SecCmsRecipientEncryptedKeyCreate(poolp)) == NULL) {
	    rv = SECFailure;
	    break;
	}

	/* hardcoded IssuerSN choice for now */
	rek->recipientIdentifier.identifierType = SecCmsKeyAgreeRecipientIDIssuerSN;
	if ((rek->recipientIdentifier.id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert)) == NULL) {
	    rv = SECFailure;
	    break;
	}

	oiok = &(ri->ri.keyAgreeRecipientInfo.originatorIdentifierOrKey);

	/* see RFC 3278 3.1.1 */
	oiok->identifierType = SecCmsOriginatorIDOrKeyOriginatorPublicKey;

	rv = SecCmsArrayAdd(poolp, (void ***)&ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys,
				    (void *)rek);

	break;

    default:
	/* other algorithms not supported yet */
	/* NOTE that we do not support any KEK algorithm */
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	rv = SECFailure;
	break;
    }

    if (rv == SECFailure)
	goto loser;

    /* set version */
    switch (ri->recipientInfoType) {
    case SecCmsRecipientInfoIDKeyTrans:
	if (ri->ri.keyTransRecipientInfo.recipientIdentifier.identifierType == SecCmsRecipientIDIssuerSN)
	    version = SEC_CMS_KEYTRANS_RECIPIENT_INFO_VERSION_ISSUERSN;
	else
	    version = SEC_CMS_KEYTRANS_RECIPIENT_INFO_VERSION_SUBJKEY;
	dummy = SEC_ASN1EncodeInteger(poolp, &(ri->ri.keyTransRecipientInfo.version), version);
	if (dummy == NULL)
	    goto loser;
	break;
    case SecCmsRecipientInfoIDKeyAgree:
	dummy = SEC_ASN1EncodeInteger(poolp, &(ri->ri.keyAgreeRecipientInfo.version),
						SEC_CMS_KEYAGREE_RECIPIENT_INFO_VERSION);
	if (dummy == NULL)
	    goto loser;
	break;
    case SecCmsRecipientInfoIDKEK:
	/* NOTE: this cannot happen as long as we do not support any KEK algorithm */
	dummy = SEC_ASN1EncodeInteger(poolp, &(ri->ri.kekRecipientInfo.version),
						SEC_CMS_KEK_RECIPIENT_INFO_VERSION);
	if (dummy == NULL)
	    goto loser;
	break;
    
    }

    PORT_ArenaUnmark (poolp, mark);
#if 0
    if (freeSpki)
      SECKEY_DestroySubjectPublicKeyInfo(freeSpki);
#endif
    return ri;

loser:
#if 0
    if (freeSpki)
      SECKEY_DestroySubjectPublicKeyInfo(freeSpki);
#endif
    PORT_ArenaRelease (poolp, mark);
    return NULL;
}

/*
 * SecCmsRecipientInfoCreate - create a recipientinfo
 *
 * we currently do not create KeyAgreement recipientinfos with multiple 
 * recipientEncryptedKeys the certificate is supposed to have been 
 * verified by the caller
 */
SecCmsRecipientInfoRef
SecCmsRecipientInfoCreate(SecCmsMessageRef cmsg, SecCertificateRef cert)
{
    return nss_cmsrecipientinfo_create(cmsg, SecCmsRecipientIDIssuerSN, cert, 
                                       NULL, NULL);
}

SecCmsRecipientInfoRef
SecCmsRecipientInfoCreateWithSubjKeyID(SecCmsMessageRef cmsg, 
                                     CSSM_DATA_PTR subjKeyID,
                                     SecPublicKeyRef pubKey)
{
    return nss_cmsrecipientinfo_create(cmsg, SecCmsRecipientIDSubjectKeyID, 
                                       NULL, pubKey, subjKeyID);
}

SecCmsRecipientInfoRef
SecCmsRecipientInfoCreateWithSubjKeyIDFromCert(SecCmsMessageRef cmsg,
                                             SecCertificateRef cert)
{
    SecPublicKeyRef pubKey = NULL;
    CSSM_DATA subjKeyID = {0, NULL};
    SecCmsRecipientInfoRef retVal = NULL;

    if (!cmsg || !cert) {
	return NULL;
    }
    pubKey = CERT_ExtractPublicKey(cert);
    if (!pubKey) {
	goto done;
    }
    if (CERT_FindSubjectKeyIDExtension(cert, &subjKeyID) != SECSuccess ||
        subjKeyID.Data == NULL) {
	goto done;
    }
    retVal = SecCmsRecipientInfoCreateWithSubjKeyID(cmsg, &subjKeyID, pubKey);
done:
    if (pubKey)
	SECKEY_DestroyPublicKey(pubKey);

    if (subjKeyID.Data)
	SECITEM_FreeItem(&subjKeyID, PR_FALSE);

    return retVal;
}

void
SecCmsRecipientInfoDestroy(SecCmsRecipientInfoRef ri)
{
    /* version was allocated on the pool, so no need to destroy it */
    /* issuerAndSN was allocated on the pool, so no need to destroy it */
    if (ri->cert != NULL)
	CERT_DestroyCertificate(ri->cert);

    if (nss_cmsrecipientinfo_usessubjectkeyid(ri)) {
	SecCmsKeyTransRecipientInfoEx *extra;
	extra = &ri->ri.keyTransRecipientInfoEx;
	if (extra->pubKey)
	    SECKEY_DestroyPublicKey(extra->pubKey);
    }

    /* recipientInfo structure itself was allocated on the pool, so no need to destroy it */
    /* we're done. */
}

int
SecCmsRecipientInfoGetVersion(SecCmsRecipientInfoRef ri)
{
    unsigned long version;
    CSSM_DATA_PTR versionitem = NULL;

    switch (ri->recipientInfoType) {
    case SecCmsRecipientInfoIDKeyTrans:
	/* ignore subIndex */
	versionitem = &(ri->ri.keyTransRecipientInfo.version);
	break;
    case SecCmsRecipientInfoIDKEK:
	/* ignore subIndex */
	versionitem = &(ri->ri.kekRecipientInfo.version);
	break;
    case SecCmsRecipientInfoIDKeyAgree:
	versionitem = &(ri->ri.keyAgreeRecipientInfo.version);
	break;
    }

    PORT_Assert(versionitem);
    if (versionitem == NULL) 
	return 0;

    /* always take apart the CSSM_DATA */
    if (SEC_ASN1DecodeInteger(versionitem, &version) != SECSuccess)
	return 0;
    else
	return (int)version;
}

CSSM_DATA_PTR
SecCmsRecipientInfoGetEncryptedKey(SecCmsRecipientInfoRef ri, int subIndex)
{
    CSSM_DATA_PTR enckey = NULL;

    switch (ri->recipientInfoType) {
    case SecCmsRecipientInfoIDKeyTrans:
	/* ignore subIndex */
	enckey = &(ri->ri.keyTransRecipientInfo.encKey);
	break;
    case SecCmsRecipientInfoIDKEK:
	/* ignore subIndex */
	enckey = &(ri->ri.kekRecipientInfo.encKey);
	break;
    case SecCmsRecipientInfoIDKeyAgree:
	enckey = &(ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[subIndex]->encKey);
	break;
    }
    return enckey;
}


SECOidTag
SecCmsRecipientInfoGetKeyEncryptionAlgorithmTag(SecCmsRecipientInfoRef ri)
{
    SECOidTag encalgtag = SEC_OID_UNKNOWN; /* an invalid encryption alg */

    switch (ri->recipientInfoType) {
    case SecCmsRecipientInfoIDKeyTrans:
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.keyTransRecipientInfo.keyEncAlg));
	break;
    case SecCmsRecipientInfoIDKeyAgree:
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.keyAgreeRecipientInfo.keyEncAlg));
	break;
    case SecCmsRecipientInfoIDKEK:
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.kekRecipientInfo.keyEncAlg));
	break;
    }
    return encalgtag;
}

OSStatus
SecCmsRecipientInfoWrapBulkKey(SecCmsRecipientInfoRef ri, SecSymmetricKeyRef bulkkey, 
                                 SECOidTag bulkalgtag)
{
    SecCertificateRef cert;
    SECOidTag certalgtag;
    OSStatus rv = SECSuccess;
#if 0
    CSSM_DATA_PTR params = NULL;
#endif /* 0 */
    SecCmsRecipientEncryptedKey *rek;
    SecCmsOriginatorIdentifierOrKey *oiok;
    const SECAlgorithmID *algid;
    PLArenaPool *poolp;
    SecCmsKeyTransRecipientInfoEx *extra = NULL;
    Boolean usesSubjKeyID;
    uint8 nullData[2] = {SEC_ASN1_NULL, 0};
    SECItem nullItem;
    SecCmsKeyAgreeRecipientInfo *kari;
    
    poolp = ri->cmsg->poolp;
    cert = ri->cert;
    usesSubjKeyID = nss_cmsrecipientinfo_usessubjectkeyid(ri);
    if (cert) {
	rv = SecCertificateGetAlgorithmID(cert,&algid);
	if (rv)
	    return SECFailure;
	certalgtag = SECOID_GetAlgorithmTag(algid);
    } else if (usesSubjKeyID) {
	extra = &ri->ri.keyTransRecipientInfoEx;
	/* sanity check */
	PORT_Assert(extra->pubKey);
	if (!extra->pubKey) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    return SECFailure;
	}
	rv = SecKeyGetAlgorithmID(extra->pubKey,&algid);
	if (rv)
	    return SECFailure;
	certalgtag = SECOID_GetAlgorithmTag(algid);
    } else {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* XXX set ri->recipientInfoType to the proper value here */
    /* or should we look if it's been set already ? */

    certalgtag = SECOID_GetAlgorithmTag(algid);
    switch (certalgtag) {
    case SEC_OID_PKCS1_RSA_ENCRYPTION:
	/* wrap the symkey */
	if (cert) {
	    rv = SecCmsUtilEncryptSymKeyRSA(poolp, cert, bulkkey, 
	                         &ri->ri.keyTransRecipientInfo.encKey);
 	    if (rv != SECSuccess)
		break;
	} else if (usesSubjKeyID) {
	    PORT_Assert(extra != NULL);
	    rv = SecCmsUtilEncryptSymKeyRSAPubKey(poolp, extra->pubKey,
	                         bulkkey, &ri->ri.keyTransRecipientInfo.encKey);
 	    if (rv != SECSuccess)
		break;
	}

	rv = SECOID_SetAlgorithmID(poolp, &(ri->ri.keyTransRecipientInfo.keyEncAlg), certalgtag, NULL);
	break;
#if 0
    case SEC_OID_MISSI_KEA_DSS_OLD:
    case SEC_OID_MISSI_KEA_DSS:
    case SEC_OID_MISSI_KEA:
	rv = SecCmsUtilEncryptSymKeyMISSI(poolp, cert, bulkkey,
					bulkalgtag,
					&ri->ri.keyTransRecipientInfo.encKey,
					&params, ri->cmsg->pwfn_arg);
	if (rv != SECSuccess)
	    break;

	/* here, we DO need to pass the params to the wrap function because, with
	 * RSA, there is no funny stuff going on with generation of IV vectors or so */
	rv = SECOID_SetAlgorithmID(poolp, &(ri->ri.keyTransRecipientInfo.keyEncAlg), certalgtag, params);
	break;
    case SEC_OID_X942_DIFFIE_HELMAN_KEY: /* dh-public-number */
	rek = ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[0];
	if (rek == NULL) {
	    rv = SECFailure;
	    break;
	}

	oiok = &(ri->ri.keyAgreeRecipientInfo.originatorIdentifierOrKey);
	PORT_Assert(oiok->identifierType == SecCmsOriginatorIDOrKeyOriginatorPublicKey);

	/* see RFC2630 12.3.1.1 */
	if (SECOID_SetAlgorithmID(poolp, &oiok->id.originatorPublicKey.algorithmIdentifier,
				    SEC_OID_X942_DIFFIE_HELMAN_KEY, NULL) != SECSuccess) {
	    rv = SECFailure;
	    break;
	}

	/* this will generate a key pair, compute the shared secret, */
	/* derive a key and ukm for the keyEncAlg out of it, encrypt the bulk key with */
	/* the keyEncAlg, set encKey, keyEncAlg, publicKey etc. */
	rv = SecCmsUtilEncryptSymKeyESDH(poolp, cert, bulkkey,
					&rek->encKey,
					&ri->ri.keyAgreeRecipientInfo.ukm,
					&ri->ri.keyAgreeRecipientInfo.keyEncAlg,
					&oiok->id.originatorPublicKey.publicKey);

	break;
#endif /* 0 */

    case SEC_OID_EC_PUBLIC_KEY:
	/* These were set up in nss_cmsrecipientinfo_create() */
	kari = &ri->ri.keyAgreeRecipientInfo;
	rek = kari->recipientEncryptedKeys[0];
	if (rek == NULL) {
	    rv = SECFailure;
	    break;
	}

	oiok = &(kari->originatorIdentifierOrKey);
	PORT_Assert(oiok->identifierType == SecCmsOriginatorIDOrKeyOriginatorPublicKey);

	/* 
	 * RFC 3278 3.1.1 says this AlgId must contain NULL params which is contrary to 
	 * any other use of the SEC_OID_EC_PUBLIC_KEY OID. So we provide one
	 * explicitly instead of mucking up the login in SECOID_SetAlgorithmID().
	 */
	nullItem.Data = nullData;
	nullItem.Length = 2;
	if (SECOID_SetAlgorithmID(poolp, &oiok->id.originatorPublicKey.algorithmIdentifier,
				    SEC_OID_EC_PUBLIC_KEY, &nullItem) != SECSuccess) {
	    rv = SECFailure;
	    break;
	}

	/* this will generate a key pair, compute the shared secret, */
	/* derive a key and ukm for the keyEncAlg out of it, encrypt the bulk key with */
	/* the keyEncAlg, set encKey, keyEncAlg, publicKey etc. */
	rv = SecCmsUtilEncryptSymKeyECDH(poolp, cert, bulkkey,
					&rek->encKey,
					&kari->ukm,
					&kari->keyEncAlg,
					&oiok->id.originatorPublicKey.publicKey);
	/* this is a BIT STRING */
	oiok->id.originatorPublicKey.publicKey.Length <<= 3;
	break;

    default:
	/* other algorithms not supported yet */
	/* NOTE that we do not support any KEK algorithm */
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	rv = SECFailure;
	break;
    }
#if 0
    if (freeSpki)
	SECKEY_DestroySubjectPublicKeyInfo(freeSpki);
#endif

    return rv;
}

#ifdef	NDEBUG
#define dprintf(args...)
#else
#define dprintf(args...)    printf(args)
#endif

SecSymmetricKeyRef
SecCmsRecipientInfoUnwrapBulkKey(SecCmsRecipientInfoRef ri, int subIndex, 
	SecCertificateRef cert, SecPrivateKeyRef privkey, SECOidTag bulkalgtag)
{
    SecSymmetricKeyRef bulkkey = NULL;
    SECAlgorithmID *encalg;
    SECOidTag encalgtag;
    CSSM_DATA_PTR enckey;
    int error;

    ri->cert = CERT_DupCertificate(cert);
        	/* mark the recipientInfo so we can find it later */

    switch (ri->recipientInfoType) {
    case SecCmsRecipientInfoIDKeyTrans:
	encalg = &(ri->ri.keyTransRecipientInfo.keyEncAlg);
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.keyTransRecipientInfo.keyEncAlg));
	enckey = &(ri->ri.keyTransRecipientInfo.encKey); /* ignore subIndex */
	switch (encalgtag) {
	case SEC_OID_PKCS1_RSA_ENCRYPTION:
	    /* RSA encryption algorithm: */
	    /* get the symmetric (bulk) key by unwrapping it using our private key */
	    bulkkey = SecCmsUtilDecryptSymKeyRSA(privkey, enckey, bulkalgtag);
	    break;
#if 0
	case SEC_OID_NETSCAPE_SMIME_KEA:
	    /* FORTEZZA key exchange algorithm */
	    /* the supplemental data is in the parameters of encalg */
	    bulkkey = SecCmsUtilDecryptSymKeyMISSI(privkey, enckey, encalg, bulkalgtag, ri->cmsg->pwfn_arg);
	    break;
#endif /* 0 */
	default:
	    error = SEC_ERROR_UNSUPPORTED_KEYALG;
	    goto loser;
	}
	break;
    case SecCmsRecipientInfoIDKeyAgree:
	encalg = &(ri->ri.keyAgreeRecipientInfo.keyEncAlg);
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.keyAgreeRecipientInfo.keyEncAlg));
	enckey = &(ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[subIndex]->encKey);
	switch (encalgtag) {
	case SEC_OID_X942_DIFFIE_HELMAN_KEY:
	    /* Diffie-Helman key exchange */
	    /* XXX not yet implemented */
	    /* XXX problem: SEC_OID_X942_DIFFIE_HELMAN_KEY points to a PKCS3 mechanism! */
	    /* we support ephemeral-static DH only, so if the recipientinfo */
	    /* has originator stuff in it, we punt (or do we? shouldn't be that hard...) */
	    /* first, we derive the KEK (a symkey!) using a Derive operation, then we get the */
	    /* content encryption key using a Unwrap op */
	    /* the derive operation has to generate the key using the algorithm in RFC2631 */
	    error = SEC_ERROR_UNSUPPORTED_KEYALG;
	    break;
	case SEC_OID_DH_SINGLE_STD_SHA1KDF:
	{  
	    /* ephemeral-static ECDH */
	    SecCmsKeyAgreeRecipientInfo *kari = &ri->ri.keyAgreeRecipientInfo;
	    SecCmsOriginatorIdentifierOrKey *oiok = &kari->originatorIdentifierOrKey;
	    if(oiok->identifierType != SecCmsOriginatorIDOrKeyOriginatorPublicKey) {
		dprintf("SEC_OID_EC_PUBLIC_KEY unwrap key: bad oiok.id\n");
		goto loser;
	    }
	    SecCmsOriginatorPublicKey *opk = &oiok->id.originatorPublicKey;
	    /* FIXME - verify opk->algorithmIdentifier here? */
	    CSSM_DATA senderPubKey = opk->publicKey;
	    /* Bit string, convert here */
	    senderPubKey.Length = (senderPubKey.Length + 7) >> 3;
	    CSSM_DATA_PTR ukm = &kari->ukm;
	    bulkkey = SecCmsUtilDecryptSymKeyECDH(privkey, enckey, ukm, encalg, bulkalgtag, &senderPubKey);
	    break;
	}
	default:
	    error = SEC_ERROR_UNSUPPORTED_KEYALG;
	    goto loser;
	}
	break;
    case SecCmsRecipientInfoIDKEK:
	encalg = &(ri->ri.kekRecipientInfo.keyEncAlg);
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.kekRecipientInfo.keyEncAlg));
	enckey = &(ri->ri.kekRecipientInfo.encKey);
	/* not supported yet */
	error = SEC_ERROR_UNSUPPORTED_KEYALG;
	goto loser;
	break;
    }
    /* XXXX continue here */
    return bulkkey;

loser:
    return NULL;
}
