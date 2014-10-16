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
 * CMS signerInfo methods.
 */

#include <Security/SecCmsSignerInfo.h>
#include "SecSMIMEPriv.h"

#include "cmslocal.h"

#include "cert.h"
#include "SecAsn1Item.h"
#include "secoid.h"
#include "cryptohi.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <security_asn1/secport.h>

#if USE_CDSA_CRYPTO
#include <Security/SecKeychain.h>
#endif

#include <Security/SecIdentity.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecInternal.h>
#include <Security/SecKeyPriv.h>
#include <utilities/SecCFWrappers.h>
#include <CoreFoundation/CFTimeZone.h>


#define HIDIGIT(v) (((v) / 10) + '0')    
#define LODIGIT(v) (((v) % 10) + '0')     

#define ISDIGIT(dig) (((dig) >= '0') && ((dig) <= '9'))
#define CAPTURE(var,p,label)                              \
{                                                         \
    if (!ISDIGIT((p)[0]) || !ISDIGIT((p)[1])) goto label; \
    (var) = ((p)[0] - '0') * 10 + ((p)[1] - '0');         \
}


static OSStatus
DER_UTCTimeToCFDate(const SecAsn1Item * utcTime, CFAbsoluteTime *date)
{
    char *string = (char *)utcTime->Data;
    int year, month, mday, hour, minute, second, hourOff, minOff;

    /* Verify time is formatted properly and capture information */
    second = 0;
    hourOff = 0;
    minOff = 0;
    CAPTURE(year,string+0,loser);
    if (year < 50) {
        /* ASSUME that year # is in the 2000's, not the 1900's */
        year += 2000;
    } else {
        year += 1900;
    }
    CAPTURE(month,string+2,loser);
    if ((month == 0) || (month > 12)) goto loser;
    CAPTURE(mday,string+4,loser);
    if ((mday == 0) || (mday > 31)) goto loser;
    CAPTURE(hour,string+6,loser);
    if (hour > 23) goto loser;
    CAPTURE(minute,string+8,loser);
    if (minute > 59) goto loser;
    if (ISDIGIT(string[10])) {
        CAPTURE(second,string+10,loser);
        if (second > 59) goto loser;
        string += 2;
    }
    if (string[10] == '+') {
        CAPTURE(hourOff,string+11,loser);
        if (hourOff > 23) goto loser;
        CAPTURE(minOff,string+13,loser);
        if (minOff > 59) goto loser;
    } else if (string[10] == '-') {
        CAPTURE(hourOff,string+11,loser);
        if (hourOff > 23) goto loser;
        hourOff = -hourOff;
        CAPTURE(minOff,string+13,loser);
        if (minOff > 59) goto loser;
        minOff = -minOff;
    } else if (string[10] != 'Z') {
        goto loser;
    }

    if (hourOff == 0 && minOff == 0) {
        *date = CFAbsoluteTimeForGregorianZuluMoment(year, month, mday, hour, minute, second);
    } else {
        CFTimeZoneRef tz = CFTimeZoneCreateWithTimeIntervalFromGMT(kCFAllocatorDefault, (hourOff * 60 + minOff) * 60);
        *date = CFAbsoluteTimeForGregorianMoment(tz, year, month, mday, hour, minute, second);
        CFReleaseSafe(tz);
    }

    return SECSuccess;

loser:
    return SECFailure;
}

static OSStatus
DER_CFDateToUTCTime(CFAbsoluteTime date, SecAsn1Item * utcTime)
{
    unsigned char *d;

    utcTime->Length = 13;
    utcTime->Data = d = PORT_Alloc(13);
    if (!utcTime->Data)
	return SECFailure;

    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    
    if (!CFCalendarDecomposeAbsoluteTime(SecCFCalendarGetZulu(), date, "yMdHms", &year, &month, &day, &hour, &minute, &second))
        return SECFailure;


    /* UTC time does not handle the years before 1950 */
    if (year < 1950)
        return SECFailure;

    /* remove the century since it's added to the year by the
       CFAbsoluteTimeGetGregorianDate routine, but is not needed for UTC time */
    year %= 100;

    d[0] = HIDIGIT(year);
    d[1] = LODIGIT(year);
    d[2] = HIDIGIT(month);
    d[3] = LODIGIT(month);
    d[4] = HIDIGIT(day);
    d[5] = LODIGIT(day);
    d[6] = HIDIGIT(hour);
    d[7] = LODIGIT(hour);
    d[8] = HIDIGIT(minute);
    d[9] = LODIGIT(minute);
    d[10] = HIDIGIT(second);
    d[11] = LODIGIT(second);
    d[12] = 'Z';
    return SECSuccess;
}

/* =============================================================================
 * SIGNERINFO
 */
SecCmsSignerInfoRef
nss_cmssignerinfo_create(SecCmsSignedDataRef sigd, SecCmsSignerIDSelector type, SecCertificateRef cert, const SecAsn1Item *subjKeyID, SecPublicKeyRef pubKey, SecPrivateKeyRef signingKey, SECOidTag digestalgtag);

SecCmsSignerInfoRef
SecCmsSignerInfoCreateWithSubjKeyID(SecCmsSignedDataRef sigd, const SecAsn1Item *subjKeyID, SecPublicKeyRef pubKey, SecPrivateKeyRef signingKey, SECOidTag digestalgtag)
{
    return nss_cmssignerinfo_create(sigd, SecCmsSignerIDSubjectKeyID, NULL, subjKeyID, pubKey, signingKey, digestalgtag); 
}

SecCmsSignerInfoRef
SecCmsSignerInfoCreate(SecCmsSignedDataRef sigd, SecIdentityRef identity, SECOidTag digestalgtag)
{
    SecCmsSignerInfoRef signerInfo = NULL;
    SecCertificateRef cert = NULL;
    SecPrivateKeyRef signingKey = NULL;

    if (SecIdentityCopyCertificate(identity, &cert))
	goto loser;
    if (SecIdentityCopyPrivateKey(identity, &signingKey))
	goto loser;

    signerInfo = nss_cmssignerinfo_create(sigd, SecCmsSignerIDIssuerSN, cert, NULL, NULL, signingKey, digestalgtag);

loser:
    if (cert)
	CFRelease(cert);
    if (signingKey)
	CFRelease(signingKey);

    return signerInfo;
}

SecCmsSignerInfoRef
nss_cmssignerinfo_create(SecCmsSignedDataRef sigd, SecCmsSignerIDSelector type, SecCertificateRef cert, const SecAsn1Item *subjKeyID, SecPublicKeyRef pubKey, SecPrivateKeyRef signingKey, SECOidTag digestalgtag)
{
    void *mark;
    SecCmsSignerInfoRef signerinfo;
    int version;
    PLArenaPool *poolp;

    poolp = sigd->contentInfo.cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    signerinfo = (SecCmsSignerInfoRef)PORT_ArenaZAlloc(poolp, sizeof(SecCmsSignerInfo));
    if (signerinfo == NULL) {
	PORT_ArenaRelease(poolp, mark);
	return NULL;
    }


    signerinfo->signedData = sigd;

    switch(type) {
    case SecCmsSignerIDIssuerSN:
        signerinfo->signerIdentifier.identifierType = SecCmsSignerIDIssuerSN;
        if ((signerinfo->cert = CERT_DupCertificate(cert)) == NULL)
	    goto loser;
        if ((signerinfo->signerIdentifier.id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert)) == NULL)
	    goto loser;
        break;
    case SecCmsSignerIDSubjectKeyID:
        signerinfo->signerIdentifier.identifierType = SecCmsSignerIDSubjectKeyID;
        PORT_Assert(subjKeyID);
        if (!subjKeyID)
            goto loser;
        signerinfo->signerIdentifier.id.subjectKeyID = PORT_ArenaNew(poolp, SecAsn1Item);
        SECITEM_CopyItem(poolp, signerinfo->signerIdentifier.id.subjectKeyID,
                         subjKeyID);
        signerinfo->pubKey = SECKEY_CopyPublicKey(pubKey);
        if (!signerinfo->pubKey)
            goto loser;
        break;
    default:
        goto loser;
    }

    if (!signingKey)
	goto loser;

    signerinfo->signingKey = SECKEY_CopyPrivateKey(signingKey);
    if (!signerinfo->signingKey)
	goto loser;

    /* set version right now */
    version = SEC_CMS_SIGNER_INFO_VERSION_ISSUERSN;
    /* RFC2630 5.3 "version is the syntax version number. If the .... " */
    if (signerinfo->signerIdentifier.identifierType == SecCmsSignerIDSubjectKeyID)
	version = SEC_CMS_SIGNER_INFO_VERSION_SUBJKEY;
    (void)SEC_ASN1EncodeInteger(poolp, &(signerinfo->version), (long)version);

    if (SECOID_SetAlgorithmID(poolp, &signerinfo->digestAlg, digestalgtag, NULL) != SECSuccess)
	goto loser;

    if (SecCmsSignedDataAddSignerInfo(sigd, signerinfo))
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return signerinfo;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/*
 * SecCmsSignerInfoDestroy - destroy a SignerInfo data structure
 */
void
SecCmsSignerInfoDestroy(SecCmsSignerInfoRef si)
{
    if (si->cert != NULL)
	CERT_DestroyCertificate(si->cert);

    if (si->certList != NULL) 
	CFRelease(si->certList);

    /* XXX storage ??? */
}

static SecAsn1AlgId SecCertificateGetPublicKeyAlgorithmID(SecCertificateRef cert)
{
    const DERAlgorithmId *length_data_swapped = SecCertificateGetPublicKeyAlgorithm(cert);
    SecAsn1AlgId temp = {
        { length_data_swapped->oid.length, length_data_swapped->oid.data },
        { length_data_swapped->params.length, length_data_swapped->params.data } };

    return temp;
}

/*
 * SecCmsSignerInfoSign - sign something
 *
 */
OSStatus
SecCmsSignerInfoSign(SecCmsSignerInfoRef signerinfo, SecAsn1Item * digest, SecAsn1Item * contentType)
{
    SecCertificateRef cert;
    SecPrivateKeyRef privkey = NULL;
    SECOidTag digestalgtag;
    SECOidTag pubkAlgTag;
    SecAsn1Item signature = { 0 };
    OSStatus rv;
    PLArenaPool *poolp, *tmppoolp;
    const SECAlgorithmID *algID = NULL;
    //CERTSubjectPublicKeyInfo *spki;

    PORT_Assert (digest != NULL);

    poolp = signerinfo->signedData->contentInfo.cmsg->poolp;

    switch (signerinfo->signerIdentifier.identifierType) {
    case SecCmsSignerIDIssuerSN:
        privkey = signerinfo->signingKey;
        signerinfo->signingKey = NULL;
        cert = signerinfo->cert;
#if USE_CDSA_CRYPTO
	if (SecCertificateGetAlgorithmID(cert,&algID)) {
	    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	    goto loser;
        }
#else
        SecAsn1AlgId _algID = SecCertificateGetPublicKeyAlgorithmID(cert);
        algID = &_algID;
#endif
        break;
    case SecCmsSignerIDSubjectKeyID:
        privkey = signerinfo->signingKey;
        signerinfo->signingKey = NULL;
#if 0
        spki = SECKEY_CreateSubjectPublicKeyInfo(signerinfo->pubKey);
        SECKEY_DestroyPublicKey(signerinfo->pubKey);
        signerinfo->pubKey = NULL;
        SECOID_CopyAlgorithmID(NULL, &freeAlgID, &spki->algorithm);
        SECKEY_DestroySubjectPublicKeyInfo(spki);
        algID = &freeAlgID;
#else
#if USE_CDSA_CRYPTO
	if (SecKeyGetAlgorithmID(signerinfo->pubKey,&algID)) {
	    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	    goto loser;
        }
#endif
#endif
	CFRelease(signerinfo->pubKey);
        signerinfo->pubKey = NULL;
        break;
    default:
        PORT_SetError(SEC_ERROR_UNSUPPORTED_MESSAGE_TYPE);
        goto loser;
    }
    digestalgtag = SecCmsSignerInfoGetDigestAlgTag(signerinfo);
    pubkAlgTag = SECOID_GetAlgorithmTag(algID);
#if USE_CDSA_CRYPTO
    if (signerinfo->signerIdentifier.identifierType == SecCmsSignerIDSubjectKeyID) {
      SECOID_DestroyAlgorithmID(&freeAlgID, PR_FALSE);
    }
#endif
#if 0
    // @@@ Not yet
    /* Fortezza MISSI have weird signature formats.  
     * Map them to standard DSA formats 
     */
    pubkAlgTag = PK11_FortezzaMapSig(pubkAlgTag);
#endif

    if (signerinfo->authAttr != NULL) {
	SecAsn1Item encoded_attrs;

	/* find and fill in the message digest attribute. */
	rv = SecCmsAttributeArraySetAttr(poolp, &(signerinfo->authAttr), 
	                       SEC_OID_PKCS9_MESSAGE_DIGEST, digest, PR_FALSE);
	if (rv != SECSuccess)
	    goto loser;

	if (contentType != NULL) {
	    /* if the caller wants us to, find and fill in the content type attribute. */
	    rv = SecCmsAttributeArraySetAttr(poolp, &(signerinfo->authAttr), 
	                    SEC_OID_PKCS9_CONTENT_TYPE, contentType, PR_FALSE);
	    if (rv != SECSuccess)
		goto loser;
	}

	if ((tmppoolp = PORT_NewArena (1024)) == NULL) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    goto loser;
	}

	/*
	 * Before encoding, reorder the attributes so that when they
	 * are encoded, they will be conforming DER, which is required
	 * to have a specific order and that is what must be used for
	 * the hash/signature.  We do this here, rather than building
	 * it into EncodeAttributes, because we do not want to do
	 * such reordering on incoming messages (which also uses
	 * EncodeAttributes) or our old signatures (and other "broken"
	 * implementations) will not verify.  So, we want to guarantee
	 * that we send out good DER encodings of attributes, but not
	 * to expect to receive them.
	 */
	if (SecCmsAttributeArrayReorder(signerinfo->authAttr) != SECSuccess)
	    goto loser;

	encoded_attrs.Data = NULL;
	encoded_attrs.Length = 0;
	if (SecCmsAttributeArrayEncode(tmppoolp, &(signerinfo->authAttr), 
	                &encoded_attrs) == NULL)
	    goto loser;

#if USE_CDSA_CRYPTO
	rv = SEC_SignData(&signature, encoded_attrs.Data, encoded_attrs.Length, 
	                  privkey, digestalgtag, pubkAlgTag);
#else
        signature.Length = SecKeyGetSize(privkey, kSecKeySignatureSize);
        signature.Data = PORT_ZAlloc(signature.Length);
        if (!signature.Data) {
            signature.Length = 0;
            goto loser;
        }
        rv = SecKeyDigestAndSign(privkey, &signerinfo->digestAlg, encoded_attrs.Data, encoded_attrs.Length, signature.Data, &signature.Length);
        if (rv) {
            PORT_ZFree(signature.Data, signature.Length);
            signature.Length = 0;
        }
#endif

	PORT_FreeArena(tmppoolp, PR_FALSE); /* awkward memory management :-( */
    } else {
        signature.Length = SecKeyGetSize(privkey, kSecKeySignatureSize);
        signature.Data = PORT_ZAlloc(signature.Length);
        if (!signature.Data) {
            signature.Length = 0;
            goto loser;
        }
        rv = SecKeySignDigest(privkey, &signerinfo->digestAlg, digest->Data, digest->Length,
                              signature.Data, &signature.Length);
        if (rv) {
            PORT_ZFree(signature.Data, signature.Length);
            signature.Length = 0;
        }
    }
    SECKEY_DestroyPrivateKey(privkey);
    privkey = NULL;

    if (rv != SECSuccess)
	goto loser;

    if (SECITEM_CopyItem(poolp, &(signerinfo->encDigest), &signature) 
          != SECSuccess)
	goto loser;

    SECITEM_FreeItem(&signature, PR_FALSE);

    if (SECOID_SetAlgorithmID(poolp, &(signerinfo->digestEncAlg), pubkAlgTag, 
                              NULL) != SECSuccess)
	goto loser;

    return SECSuccess;

loser:
    if (signature.Length != 0)
	SECITEM_FreeItem (&signature, PR_FALSE);
    if (privkey)
	SECKEY_DestroyPrivateKey(privkey);
    return SECFailure;
}

#if !USE_CDSA_CRYPTO
static CFArrayRef
SecCmsSignerInfoCopySigningCertificates(SecCmsSignerInfoRef signerinfo)
{
    CFMutableArrayRef certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    SecAsn1Item **cert_datas = signerinfo->signedData->rawCerts;
    SecAsn1Item *cert_data;
    if (cert_datas) while ((cert_data = *cert_datas) != NULL) {
        SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, cert_data->Data, cert_data->Length);
        if (cert) {
            switch (signerinfo->signerIdentifier.identifierType) {
            case SecCmsSignerIDIssuerSN: 
                if (CERT_CheckIssuerAndSerial(cert,
                    &(signerinfo->signerIdentifier.id.issuerAndSN->derIssuer),
                    &(signerinfo->signerIdentifier.id.issuerAndSN->serialNumber)))
                        CFArrayInsertValueAtIndex(certs, 0, cert);
                else
                    CFArrayAppendValue(certs, cert);
                break;
            case SecCmsSignerIDSubjectKeyID: 
                {
                    CFDataRef cert_keyid = SecCertificateGetSubjectKeyID(cert);
                    SecAsn1Item *tbf_keyid = signerinfo->signerIdentifier.id.subjectKeyID;
                    if (tbf_keyid->Length == (size_t)CFDataGetLength(cert_keyid) &&
                        !memcmp(tbf_keyid->Data, CFDataGetBytePtr(cert_keyid), tbf_keyid->Length))
                            CFArrayInsertValueAtIndex(certs, 0, cert);
                    else
                        CFArrayAppendValue(certs, cert);
                    break;
                }
            }
            CFReleaseNull(cert); 
        }
        cert_datas++;
    }
    
    if ((CFArrayGetCount(certs) == 0) && 
        (signerinfo->signerIdentifier.identifierType == SecCmsSignerIDIssuerSN)) 
    {
        SecCertificateRef cert = CERT_FindCertificateByIssuerAndSN(signerinfo->signedData->certs, signerinfo->signerIdentifier.id.issuerAndSN);
        if (cert) {
            CFArrayAppendValue(certs, cert);
            CFRelease(cert);
        }
    }
    return certs;
}
#endif

OSStatus
SecCmsSignerInfoVerifyCertificate(SecCmsSignerInfoRef signerinfo, SecKeychainRef keychainOrArray,
				  CFTypeRef policies, SecTrustRef *trustRef)
{
    CFAbsoluteTime stime;
    OSStatus rv;

#if USE_CDSA_CRYPTO
    SecCertificateRef cert;
    
    if ((cert = SecCmsSignerInfoGetSigningCertificate(signerinfo, keychainOrArray)) == NULL) {
#else
    CFArrayRef certs;

    if ((certs = SecCmsSignerInfoCopySigningCertificates(signerinfo)) == NULL) {
#endif
	signerinfo->verificationStatus = SecCmsVSSigningCertNotFound;
	return SECFailure;
    }
    /*
     * Get and convert the signing time; if available, it will be used
     * both on the cert verification and for importing the sender
     * email profile.
     */
    if (SecCmsSignerInfoGetSigningTime(signerinfo, &stime) != SECSuccess)
	stime = CFAbsoluteTimeGetCurrent();

#if USE_CDSA_CRYPTO
    rv = CERT_VerifyCert(keychainOrArray, cert, policies, stime, trustRef);
#else
    rv = CERT_VerifyCert(keychainOrArray, certs, policies, stime, trustRef);
    CFRelease(certs);
#endif
    if (rv || !trustRef)
    {
	if (PORT_GetError() == SEC_ERROR_UNTRUSTED_CERT)
	{
	    /* Signature or digest level verificationStatus errors should supercede certificate level errors, so only change the verificationStatus if the status was GoodSignature. */
	    if (signerinfo->verificationStatus == SecCmsVSGoodSignature)
		signerinfo->verificationStatus = SecCmsVSSigningCertNotTrusted;
	}
    }

    return rv;
}

/*
 * SecCmsSignerInfoVerify - verify the signature of a single SignerInfo
 *
 * Just verifies the signature. The assumption is that verification of the certificate
 * is done already.
 */
OSStatus
SecCmsSignerInfoVerify(SecCmsSignerInfoRef signerinfo, SecAsn1Item * digest, SecAsn1Item * contentType)
{
    SecPublicKeyRef publickey = NULL;
    SecCmsAttribute *attr;
    SecAsn1Item encoded_attrs;
    SecCertificateRef cert;
    SecCmsVerificationStatus vs = SecCmsVSUnverified;
    PLArenaPool *poolp;
    SECOidTag digestAlgTag, digestEncAlgTag;

    if (signerinfo == NULL)
	return SECFailure;

    /* SecCmsSignerInfoGetSigningCertificate will fail if 2nd parm is NULL and */
    /* cert has not been verified */
    if ((cert = SecCmsSignerInfoGetSigningCertificate(signerinfo, NULL)) == NULL) {
	vs = SecCmsVSSigningCertNotFound;
	goto loser;
    }

#if USE_CDSA_CRYPTO
    if (SecCertificateCopyPublicKey(cert, &publickey)) {
	vs = SecCmsVSProcessingError;
	goto loser;
    }
#else
    publickey = SecCertificateCopyPublicKey(cert);
    if (publickey == NULL)
        goto loser;
#endif

    digestAlgTag = SECOID_GetAlgorithmTag(&(signerinfo->digestAlg));
    digestEncAlgTag = SECOID_GetAlgorithmTag(&(signerinfo->digestEncAlg));
    if (!SecCmsArrayIsEmpty((void **)signerinfo->authAttr)) {
	if (contentType) {
	    /*
	     * Check content type
	     *
	     * RFC2630 sez that if there are any authenticated attributes,
	     * then there must be one for content type which matches the
	     * content type of the content being signed, and there must
	     * be one for message digest which matches our message digest.
	     * So check these things first.
	     */
	    if ((attr = SecCmsAttributeArrayFindAttrByOidTag(signerinfo->authAttr,
					SEC_OID_PKCS9_CONTENT_TYPE, PR_TRUE)) == NULL)
	    {
		vs = SecCmsVSMalformedSignature;
		goto loser;
	    }
		
	    if (SecCmsAttributeCompareValue(attr, contentType) == PR_FALSE) {
		vs = SecCmsVSMalformedSignature;
		goto loser;
	    }
	}

	/*
	 * Check digest
	 */
	if ((attr = SecCmsAttributeArrayFindAttrByOidTag(signerinfo->authAttr, SEC_OID_PKCS9_MESSAGE_DIGEST, PR_TRUE)) == NULL)
	{
	    vs = SecCmsVSMalformedSignature;
	    goto loser;
	}
	if (SecCmsAttributeCompareValue(attr, digest) == PR_FALSE) {
	    vs = SecCmsVSDigestMismatch;
	    goto loser;
	}

	if ((poolp = PORT_NewArena (1024)) == NULL) {
	    vs = SecCmsVSProcessingError;
	    goto loser;
	}

	/*
	 * Check signature
	 *
	 * The signature is based on a digest of the DER-encoded authenticated
	 * attributes.  So, first we encode and then we digest/verify.
	 * we trust the decoder to have the attributes in the right (sorted) order
	 */
	encoded_attrs.Data = NULL;
	encoded_attrs.Length = 0;

	if (SecCmsAttributeArrayEncode(poolp, &(signerinfo->authAttr), &encoded_attrs) == NULL ||
		encoded_attrs.Data == NULL || encoded_attrs.Length == 0)
	{
	    vs = SecCmsVSProcessingError;
	    goto loser;
	}
        if (errSecSuccess == SecKeyDigestAndVerify(publickey, &signerinfo->digestAlg, encoded_attrs.Data, encoded_attrs.Length, signerinfo->encDigest.Data, signerinfo->encDigest.Length))
            vs = SecCmsVSGoodSignature;
        else
            vs = SecCmsVSBadSignature;

	PORT_FreeArena(poolp, PR_FALSE);	/* awkward memory management :-( */

    } else {
	SecAsn1Item * sig;

	/* No authenticated attributes. The signature is based on the plain message digest. */
	sig = &(signerinfo->encDigest);
	if (sig->Length == 0)
	    goto loser;

        if (SecKeyVerifyDigest(publickey, &signerinfo->digestAlg, digest->Data, digest->Length, sig->Data, sig->Length))
            vs = SecCmsVSBadSignature;
        else
            vs = SecCmsVSGoodSignature;
    }

    if (vs == SecCmsVSBadSignature) {
	/*
	 * XXX Change the generic error into our specific one, because
	 * in that case we get a better explanation out of the Security
	 * Advisor.  This is really a bug in our error strings (the
	 * "generic" error has a lousy/wrong message associated with it
	 * which assumes the signature verification was done for the
	 * purposes of checking the issuer signature on a certificate)
	 * but this is at least an easy workaround and/or in the
	 * Security Advisor, which specifically checks for the error
	 * SEC_ERROR_PKCS7_BAD_SIGNATURE and gives more explanation
	 * in that case but does not similarly check for
	 * SEC_ERROR_BAD_SIGNATURE.  It probably should, but then would
	 * probably say the wrong thing in the case that it *was* the
	 * certificate signature check that failed during the cert
	 * verification done above.  Our error handling is really a mess.
	 */
	if (PORT_GetError() == SEC_ERROR_BAD_SIGNATURE)
	    PORT_SetError(SEC_ERROR_PKCS7_BAD_SIGNATURE);
    }

    if (publickey != NULL)
	CFRelease(publickey);

    signerinfo->verificationStatus = vs;

    return (vs == SecCmsVSGoodSignature) ? SECSuccess : SECFailure;

loser:
    if (publickey != NULL)
	SECKEY_DestroyPublicKey (publickey);

    signerinfo->verificationStatus = vs;

    PORT_SetError (SEC_ERROR_PKCS7_BAD_SIGNATURE);
    return SECFailure;
}

SecCmsVerificationStatus
SecCmsSignerInfoGetVerificationStatus(SecCmsSignerInfoRef signerinfo)
{
    return signerinfo->verificationStatus;
}

SECOidData *
SecCmsSignerInfoGetDigestAlg(SecCmsSignerInfoRef signerinfo)
{
    return SECOID_FindOID (&(signerinfo->digestAlg.algorithm));
}

SECOidTag
SecCmsSignerInfoGetDigestAlgTag(SecCmsSignerInfoRef signerinfo)
{
    SECOidData *algdata;

    algdata = SECOID_FindOID (&(signerinfo->digestAlg.algorithm));
    if (algdata != NULL)
	return algdata->offset;
    else
	return SEC_OID_UNKNOWN;
}

CFArrayRef
SecCmsSignerInfoGetCertList(SecCmsSignerInfoRef signerinfo)
{
    return signerinfo->certList;
}

int
SecCmsSignerInfoGetVersion(SecCmsSignerInfoRef signerinfo)
{
    unsigned long version;

    /* always take apart the SecAsn1Item */
    if (SEC_ASN1DecodeInteger(&(signerinfo->version), &version) != SECSuccess)
	return 0;
    else
	return (int)version;
}

/*
 * SecCmsSignerInfoGetSigningTime - return the signing time,
 *				      in UTCTime format, of a CMS signerInfo.
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a pointer to XXXX (what?)
 * A return value of NULL is an error.
 */
OSStatus
SecCmsSignerInfoGetSigningTime(SecCmsSignerInfoRef sinfo, CFAbsoluteTime *stime)
{
    SecCmsAttribute *attr;
    SecAsn1Item * value;

    if (sinfo == NULL)
	return SECFailure;

    if (sinfo->signingTime != 0) {
	*stime = sinfo->signingTime;	/* cached copy */
	return SECSuccess;
    }

    attr = SecCmsAttributeArrayFindAttrByOidTag(sinfo->authAttr, SEC_OID_PKCS9_SIGNING_TIME, PR_TRUE);
    /* XXXX multi-valued attributes NIH */
    if (attr == NULL || (value = SecCmsAttributeGetValue(attr)) == NULL)
	return SECFailure;
    if (DER_UTCTimeToCFDate(value, stime) != SECSuccess)
	return SECFailure;
    sinfo->signingTime = *stime;	/* make cached copy */
    return SECSuccess;
}

/*
 * Return the signing cert of a CMS signerInfo.
 *
 * the certs in the enclosing SignedData must have been imported already
 */
SecCertificateRef
SecCmsSignerInfoGetSigningCertificate(SecCmsSignerInfoRef signerinfo, SecKeychainRef keychainOrArray)
{
    SecCertificateRef cert = NULL;

    if (signerinfo->cert != NULL)
	return signerinfo->cert;

    /* @@@ Make sure we search though all the certs in the cms message itself as well, it's silly
       to require them to be added to a keychain first. */

#if USE_CDSA_CRYPTO
    SecCmsSignerIdentifier *sid;

    /*
     * This cert will also need to be freed, but since we save it
     * in signerinfo for later, we do not want to destroy it when
     * we leave this function -- we let the clean-up of the entire
     * cinfo structure later do the destroy of this cert.
     */
    sid = &signerinfo->signerIdentifier;
    switch (sid->identifierType) {
    case SecCmsSignerIDIssuerSN:
	cert = CERT_FindCertByIssuerAndSN(keychainOrArray, sid->id.issuerAndSN);
	break;
    case SecCmsSignerIDSubjectKeyID:
	cert = CERT_FindCertBySubjectKeyID(keychainOrArray, sid->id.subjectKeyID);
	break;
    default:
	cert = NULL;
	break;
    }

    /* cert can be NULL at that point */
    signerinfo->cert = cert;	/* earmark it */
#else
    SecAsn1Item **cert_datas = signerinfo->signedData->rawCerts;
    SecAsn1Item *cert_data;
    if (cert_datas) while ((cert_data = *cert_datas) != NULL) {
        cert = SecCertificateCreateWithBytes(NULL, cert_data->Data, cert_data->Length);
        if (cert) {
            switch (signerinfo->signerIdentifier.identifierType) {
            case SecCmsSignerIDIssuerSN: 
                if (CERT_CheckIssuerAndSerial(cert,
                    &(signerinfo->signerIdentifier.id.issuerAndSN->derIssuer),
                    &(signerinfo->signerIdentifier.id.issuerAndSN->serialNumber)))
                        signerinfo->cert = cert;
                    break;
            case SecCmsSignerIDSubjectKeyID: {
                CFDataRef cert_keyid = SecCertificateGetSubjectKeyID(cert);
                SecAsn1Item *tbf_keyid = signerinfo->signerIdentifier.id.subjectKeyID;
                if (tbf_keyid->Length == (size_t)CFDataGetLength(cert_keyid) &&
                    !memcmp(tbf_keyid->Data, CFDataGetBytePtr(cert_keyid), tbf_keyid->Length))
                        signerinfo->cert = cert;
                    }
            }
            if (signerinfo->cert)
                break;
            CFReleaseNull(cert); 
        }
        cert_datas++;
    }

    if (!signerinfo->cert && (signerinfo->signerIdentifier.identifierType == SecCmsSignerIDIssuerSN)) {
        cert = CERT_FindCertificateByIssuerAndSN(signerinfo->signedData->certs, signerinfo->signerIdentifier.id.issuerAndSN);
        signerinfo->cert = cert;
    }
#endif

    return cert;
}


/*
 * SecCmsSignerInfoGetSignerCommonName - return the common name of the signer
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a CFStringRef containing the common name of the signer.
 * A return value of NULL is an error.
 */
CFStringRef
SecCmsSignerInfoGetSignerCommonName(SecCmsSignerInfoRef sinfo)
{
    SecCertificateRef signercert;
    CFStringRef commonName = NULL;
    
    /* will fail if cert is not verified */
    if ((signercert = SecCmsSignerInfoGetSigningCertificate(sinfo, NULL)) == NULL)
	return NULL;

#if USE_CDSA_CRYPTO
    SecCertificateGetCommonName(signercert, &commonName);
#else
    CFArrayRef commonNames = SecCertificateCopyCommonNames(signercert);
    if (commonNames) {
        /* SecCertificateCopyCommonNames doesn't return empty arrays */
        commonName = (CFStringRef)CFArrayGetValueAtIndex(commonNames, CFArrayGetCount(commonNames) - 1);
        CFRetain(commonName);
        CFRelease(commonNames);
    }
#endif

    return commonName;
}

/*
 * SecCmsSignerInfoGetSignerEmailAddress - return the email address of the signer
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a CFStringRef containing the name of the signer.
 * A return value of NULL is an error.
 */
CFStringRef
SecCmsSignerInfoGetSignerEmailAddress(SecCmsSignerInfoRef sinfo)
{
    SecCertificateRef signercert;
    CFStringRef emailAddress = NULL;

    if ((signercert = SecCmsSignerInfoGetSigningCertificate(sinfo, NULL)) == NULL)
	return NULL;

#if USE_CDSA_CRYPTO
    SecCertificateGetEmailAddress(signercert, &emailAddress);
#else
    CFArrayRef names = SecCertificateCopyRFC822Names(signercert);
    if (names) {
        if (CFArrayGetCount(names) > 0)
            emailAddress = (CFStringRef)CFArrayGetValueAtIndex(names, 0);
        if (emailAddress)
            CFRetain(emailAddress);
        CFRelease(names);
    }
#endif
    return emailAddress;
}


/*
 * SecCmsSignerInfoAddAuthAttr - add an attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 */
OSStatus
SecCmsSignerInfoAddAuthAttr(SecCmsSignerInfoRef signerinfo, SecCmsAttribute *attr)
{
    return SecCmsAttributeArrayAddAttr(signerinfo->signedData->contentInfo.cmsg->poolp, &(signerinfo->authAttr), attr);
}

/*
 * SecCmsSignerInfoAddUnauthAttr - add an attribute to the
 * unauthenticated attributes of "signerinfo". 
 */
OSStatus
SecCmsSignerInfoAddUnauthAttr(SecCmsSignerInfoRef signerinfo, SecCmsAttribute *attr)
{
    return SecCmsAttributeArrayAddAttr(signerinfo->signedData->contentInfo.cmsg->poolp, &(signerinfo->unAuthAttr), attr);
}

/* 
 * SecCmsSignerInfoAddSigningTime - add the signing time to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 *
 * This is expected to be included in outgoing signed
 * messages for email (S/MIME) but is likely useful in other situations.
 *
 * This should only be added once; a second call will do nothing.
 *
 * XXX This will probably just shove the current time into "signerinfo"
 * but it will not actually get signed until the entire item is
 * processed for encoding.  Is this (expected to be small) delay okay?
 */
OSStatus
SecCmsSignerInfoAddSigningTime(SecCmsSignerInfoRef signerinfo, CFAbsoluteTime t)
{
    SecCmsAttribute *attr;
    SecAsn1Item stime;
    void *mark;
    PLArenaPool *poolp;

    poolp = signerinfo->signedData->contentInfo.cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    /* create new signing time attribute */
    if (DER_CFDateToUTCTime(t, &stime) != SECSuccess)
	goto loser;

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_PKCS9_SIGNING_TIME, &stime, PR_FALSE)) == NULL) {
	SECITEM_FreeItem (&stime, PR_FALSE);
	goto loser;
    }

    SECITEM_FreeItem (&stime, PR_FALSE);

    if (SecCmsSignerInfoAddAuthAttr(signerinfo, attr) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark (poolp, mark);

    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

/* 
 * SecCmsSignerInfoAddSMIMECaps - add a SMIMECapabilities attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 *
 * This is expected to be included in outgoing signed
 * messages for email (S/MIME).
 */
OSStatus
SecCmsSignerInfoAddSMIMECaps(SecCmsSignerInfoRef signerinfo)
{
    SecCmsAttribute *attr;
    SecAsn1Item * smimecaps = NULL;
    void *mark;
    PLArenaPool *poolp;

    poolp = signerinfo->signedData->contentInfo.cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    smimecaps = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimecaps == NULL)
	goto loser;

    /* create new signing time attribute */
#if 1
    // @@@ We don't do Fortezza yet.
    if (SecSMIMECreateSMIMECapabilities(poolp, smimecaps, PR_FALSE) != SECSuccess)
#else
    if (SecSMIMECreateSMIMECapabilities(poolp, smimecaps,
			    PK11_FortezzaHasKEA(signerinfo->cert)) != SECSuccess)
#endif
	goto loser;

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_PKCS9_SMIME_CAPABILITIES, smimecaps, PR_TRUE)) == NULL)
	goto loser;

    if (SecCmsSignerInfoAddAuthAttr(signerinfo, attr) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark (poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

/* 
 * SecCmsSignerInfoAddSMIMEEncKeyPrefs - add a SMIMEEncryptionKeyPreferences attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 *
 * This is expected to be included in outgoing signed messages for email (S/MIME).
 */
OSStatus
SecCmsSignerInfoAddSMIMEEncKeyPrefs(SecCmsSignerInfoRef signerinfo, SecCertificateRef cert, SecKeychainRef keychainOrArray)
{
    SecCmsAttribute *attr;
    SecAsn1Item * smimeekp = NULL;
    void *mark;
    PLArenaPool *poolp;

#if 0
    CFTypeRef policy;

    /* verify this cert for encryption */
    policy = CERT_PolicyForCertUsage(certUsageEmailRecipient);
    if (CERT_VerifyCert(keychainOrArray, cert, policy, CFAbsoluteTimeGetCurrent(), NULL) != SECSuccess) {
	CFRelease(policy);
	return SECFailure;
    }
    CFRelease(policy);
#endif

    poolp = signerinfo->signedData->contentInfo.cmsg->poolp;
    mark = PORT_ArenaMark(poolp);

    smimeekp = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimeekp == NULL)
	goto loser;

    /* create new signing time attribute */
    if (SecSMIMECreateSMIMEEncKeyPrefs(poolp, smimeekp, cert) != SECSuccess)
	goto loser;

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_SMIME_ENCRYPTION_KEY_PREFERENCE, smimeekp, PR_TRUE)) == NULL)
	goto loser;

    if (SecCmsSignerInfoAddAuthAttr(signerinfo, attr) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark (poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

/* 
 * SecCmsSignerInfoAddMSSMIMEEncKeyPrefs - add a SMIMEEncryptionKeyPreferences attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo", using the OID prefered by Microsoft.
 *
 * This is expected to be included in outgoing signed messages for email (S/MIME),
 * if compatibility with Microsoft mail clients is wanted.
 */
OSStatus
SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(SecCmsSignerInfoRef signerinfo, SecCertificateRef cert, SecKeychainRef keychainOrArray)
{
    SecCmsAttribute *attr;
    SecAsn1Item * smimeekp = NULL;
    void *mark;
    PLArenaPool *poolp;

#if 0
    CFTypeRef policy;

    /* verify this cert for encryption */
    policy = CERT_PolicyForCertUsage(certUsageEmailRecipient);
    if (CERT_VerifyCert(keychainOrArray, cert, policy, CFAbsoluteTimeGetCurrent(), NULL) != SECSuccess) {
	CFRelease(policy);
	return SECFailure;
    }
    CFRelease(policy);
#endif

    poolp = signerinfo->signedData->contentInfo.cmsg->poolp;
    mark = PORT_ArenaMark(poolp);

    smimeekp = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimeekp == NULL)
	goto loser;

    /* create new signing time attribute */
    if (SecSMIMECreateMSSMIMEEncKeyPrefs(poolp, smimeekp, cert) != SECSuccess)
	goto loser;

    if ((attr = SecCmsAttributeCreate(poolp, SEC_OID_MS_SMIME_ENCRYPTION_KEY_PREFERENCE, smimeekp, PR_TRUE)) == NULL)
	goto loser;

    if (SecCmsSignerInfoAddAuthAttr(signerinfo, attr) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark (poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

/* 
 * SecCmsSignerInfoAddCounterSignature - countersign a signerinfo
 *
 * 1. digest the DER-encoded signature value of the original signerinfo
 * 2. create new signerinfo with correct version, sid, digestAlg
 * 3. add message-digest authAttr, but NO content-type
 * 4. sign the authAttrs
 * 5. DER-encode the new signerInfo
 * 6. add the whole thing to original signerInfo's unAuthAttrs
 *    as a SEC_OID_PKCS9_COUNTER_SIGNATURE attribute
 *
 * XXXX give back the new signerinfo?
 */
OSStatus
SecCmsSignerInfoAddCounterSignature(SecCmsSignerInfoRef signerinfo,
				    SECOidTag digestalg, SecIdentityRef identity)
{
    /* XXXX TBD XXXX */
    return SECFailure;
}

/*
 * XXXX the following needs to be done in the S/MIME layer code
 * after signature of a signerinfo is verified
 */
OSStatus
SecCmsSignerInfoSaveSMIMEProfile(SecCmsSignerInfoRef signerinfo)
{
    return -4 /*unImp*/;
}

/*
 * SecCmsSignerInfoIncludeCerts - set cert chain inclusion mode for this signer
 */
OSStatus
SecCmsSignerInfoIncludeCerts(SecCmsSignerInfoRef signerinfo, SecCmsCertChainMode cm, SECCertUsage usage)
{
    if (signerinfo->cert == NULL)
	return SECFailure;

    /* don't leak if we get called twice */
    if (signerinfo->certList != NULL) {
	CFRelease(signerinfo->certList);
	signerinfo->certList = NULL;
    }

    switch (cm) {
    case SecCmsCMNone:
	signerinfo->certList = NULL;
	break;
    case SecCmsCMCertOnly:
	signerinfo->certList = CERT_CertListFromCert(signerinfo->cert);
	break;
    case SecCmsCMCertChain:
	signerinfo->certList = CERT_CertChainFromCert(signerinfo->cert, usage, PR_FALSE);
	break;
    case SecCmsCMCertChainWithRoot:
	signerinfo->certList = CERT_CertChainFromCert(signerinfo->cert, usage, PR_TRUE);
	break;
    }

    if (cm != SecCmsCMNone && signerinfo->certList == NULL)
	return SECFailure;
    
    return SECSuccess;
}
