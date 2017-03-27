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
#include "SecAsn1Item.h"
#include "secoid.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <security_asn1/secport.h>

#include <Security/SecKeyPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateInternal.h>

#include "SecCmsRecipientInfo.h"

static Boolean
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


static SecCmsRecipientInfoRef
nss_cmsrecipientinfo_create(SecCmsEnvelopedDataRef envd, SecCmsRecipientIDSelector type,
                            SecCertificateRef cert, SecPublicKeyRef pubKey, 
                            const SecAsn1Item *subjKeyID)
{
    SecCmsRecipientInfoRef ri;
    void *mark;
    SECOidTag certalgtag;
    OSStatus rv = SECSuccess;
    SecCmsRecipientEncryptedKey *rek;
    SecCmsOriginatorIdentifierOrKey *oiok;
    unsigned long version;
    SecAsn1Item * dummy;
    PLArenaPool *poolp;
    const SECAlgorithmID *algid;
    SECAlgorithmID freeAlgID;
    
    SecCmsRecipientIdentifier *rid;

    poolp = envd->contentInfo.cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    ri = (SecCmsRecipientInfoRef)PORT_ArenaZAlloc(poolp, sizeof(SecCmsRecipientInfo));
    if (ri == NULL)
	goto loser;

    ri->envelopedData = envd;

    ri->cert = CERT_DupCertificate(cert);
    if (ri->cert == NULL)
        goto loser;

    const SecAsn1AlgId *length_data_swapped = (const SecAsn1AlgId *)SecCertificateGetPublicKeyAlgorithm(cert);
    freeAlgID.algorithm.Length = (size_t)length_data_swapped->algorithm.Data;
    freeAlgID.algorithm.Data = (uint8_t *)length_data_swapped->algorithm.Length;
    freeAlgID.parameters.Length = (size_t)length_data_swapped->parameters.Data;
    freeAlgID.parameters.Data = (uint8_t *)length_data_swapped->parameters.Length;
    algid = &freeAlgID;

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

	    rid->id.subjectKeyID = PORT_ArenaNew(poolp, SecAsn1Item);
	    if (rid->id.subjectKeyID == NULL) {
		rv = SECFailure;
		PORT_SetError(SEC_ERROR_NO_MEMORY);
		break;
	    } 
            if (SECITEM_CopyItem(poolp, rid->id.subjectKeyID, subjKeyID)) {
                rv = SECFailure;
                PORT_SetError(SEC_ERROR_UNKNOWN_CERT);
                break;
            }
	    if (rid->id.subjectKeyID->Data == NULL) {
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

    if (SecCmsEnvelopedDataAddRecipient(envd, ri))
	goto loser;

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
SecCmsRecipientInfoCreate(SecCmsEnvelopedDataRef envd, SecCertificateRef cert)
{
    /* TODO: We might want to prefer subjkeyid */
#if 0
    SecCmsRecipientInfoRef info = SecCmsRecipientInfoCreateWithSubjKeyIDFromCert(envd, cert);

    if (info)
        return info;
    else
#endif
        return nss_cmsrecipientinfo_create(envd, SecCmsRecipientIDIssuerSN, cert,
                                       NULL, NULL);
}

SecCmsRecipientInfoRef
SecCmsRecipientInfoCreateWithSubjKeyID(SecCmsEnvelopedDataRef envd, 
                                     const SecAsn1Item * subjKeyID,
                                     SecPublicKeyRef pubKey)
{
    return nss_cmsrecipientinfo_create(envd, SecCmsRecipientIDSubjectKeyID, 
                                       NULL, pubKey, subjKeyID);
}

SecCmsRecipientInfoRef
SecCmsRecipientInfoCreateWithSubjKeyIDFromCert(SecCmsEnvelopedDataRef envd,
                                               SecCertificateRef cert)
{
    SecPublicKeyRef pubKey = NULL;
    SecAsn1Item subjKeyID = {0, NULL};
    SecCmsRecipientInfoRef retVal = NULL;
    CFDataRef subjectKeyIDData = NULL;

    if (!envd || !cert) {
	return NULL;
    }
    subjectKeyIDData = SecCertificateGetSubjectKeyID(cert);
    if (!subjectKeyIDData)
        goto done;
    subjKeyID.Length =
    CFDataGetLength(subjectKeyIDData);
    subjKeyID.Data = (uint8_t *)CFDataGetBytePtr(subjectKeyIDData);
    retVal = nss_cmsrecipientinfo_create(envd, SecCmsRecipientIDSubjectKeyID,
                                         cert, pubKey, &subjKeyID);

done:

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
    SecAsn1Item * versionitem = NULL;

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

    /* always take apart the SecAsn1Item */
    if (SEC_ASN1DecodeInteger(versionitem, &version) != SECSuccess)
	return 0;
    else
	return (int)version;
}

SecAsn1Item *
SecCmsRecipientInfoGetEncryptedKey(SecCmsRecipientInfoRef ri, int subIndex)
{
    SecAsn1Item * enckey = NULL;

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
    SecCmsRecipientEncryptedKey *rek;
    SecCmsOriginatorIdentifierOrKey *oiok;
    const SECAlgorithmID *algid;
    SECAlgorithmID freeAlgID;
    PLArenaPool *poolp;
    Boolean usesSubjKeyID;
    uint8_t nullData[2] = {SEC_ASN1_NULL, 0};
    SECItem nullItem;
    SecCmsKeyAgreeRecipientInfo *kari;

    poolp = ri->envelopedData->contentInfo.cmsg->poolp;
    cert = ri->cert;
    usesSubjKeyID = nss_cmsrecipientinfo_usessubjectkeyid(ri);
    if (cert) {
        const SecAsn1AlgId *length_data_swapped = (const SecAsn1AlgId *)SecCertificateGetPublicKeyAlgorithm(cert);
        freeAlgID.algorithm.Length = (size_t)length_data_swapped->algorithm.Data;
        freeAlgID.algorithm.Data = (uint8_t *)length_data_swapped->algorithm.Length;
        freeAlgID.parameters.Length = (size_t)length_data_swapped->parameters.Data;
        freeAlgID.parameters.Data = (uint8_t *)length_data_swapped->parameters.Length;
        algid = &freeAlgID;
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
	}

	rv = SECOID_SetAlgorithmID(poolp, &(ri->ri.keyTransRecipientInfo.keyEncAlg), certalgtag, NULL);
	break;
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
        break;
    default:
	/* other algorithms not supported yet */
	/* NOTE that we do not support any KEK algorithm */
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	rv = SECFailure;
	break;
    }

    return rv;
}

#ifdef	NDEBUG
#define dprintf(args...)
#else
#define dprintf(args...)    fprintf(stderr, args)
#endif

SecSymmetricKeyRef
SecCmsRecipientInfoUnwrapBulkKey(SecCmsRecipientInfoRef ri, int subIndex, 
	SecCertificateRef cert, SecPrivateKeyRef privkey, SECOidTag bulkalgtag)
{
    SecSymmetricKeyRef bulkkey = NULL;
    SECAlgorithmID *encalg;
    SECOidTag encalgtag;
    SecAsn1Item * enckey;
    int error;

    ri->cert = CERT_DupCertificate(cert);
        	/* mark the recipientInfo so we can find it later */

    switch (ri->recipientInfoType) {
    case SecCmsRecipientInfoIDKeyTrans:
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
            encalg = &(ri->ri.keyTransRecipientInfo.keyEncAlg);
	    bulkkey = SecCmsUtilDecryptSymKeyMISSI(privkey, enckey, encalg, bulkalgtag, ri->cmsg->pwfn_arg);
	    break;
#endif /* 0 */
	default:
	    error = SEC_ERROR_UNSUPPORTED_KEYALG;
	    goto loser;
	}
	break;
    case SecCmsRecipientInfoIDKeyAgree:
	encalgtag = SECOID_GetAlgorithmTag(&(ri->ri.keyAgreeRecipientInfo.keyEncAlg));
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
            enckey = &(ri->ri.keyAgreeRecipientInfo.recipientEncryptedKeys[subIndex]->encKey);
            encalg = &(ri->ri.keyAgreeRecipientInfo.keyEncAlg);
            SecCmsKeyAgreeRecipientInfo *kari = &ri->ri.keyAgreeRecipientInfo;
            SecCmsOriginatorIdentifierOrKey *oiok = &kari->originatorIdentifierOrKey;
            if(oiok->identifierType != SecCmsOriginatorIDOrKeyOriginatorPublicKey) {
                dprintf("SEC_OID_EC_PUBLIC_KEY unwrap key: bad oiok.id\n");
                error = SEC_ERROR_LIBRARY_FAILURE;
                goto loser;
            }
            SecCmsOriginatorPublicKey *opk = &oiok->id.originatorPublicKey;
            /* FIXME - verify opk->algorithmIdentifier here? */
            SecAsn1Item senderPubKey = opk->publicKey;
            SecAsn1Item *ukm = &kari->ukm;
            bulkkey = SecCmsUtilDecryptSymKeyECDH(privkey, enckey, ukm, encalg, bulkalgtag, &senderPubKey);
            break;
        }
	default:
	    error = SEC_ERROR_UNSUPPORTED_KEYALG;
	    goto loser;
	}
	break;
    case SecCmsRecipientInfoIDKEK:
	/* not supported yet */
	error = SEC_ERROR_UNSUPPORTED_KEYALG;
	goto loser;
	break;
    }
    /* XXXX continue here */
    return bulkkey;

loser:
    PORT_SetError(error);
    return NULL;
}
