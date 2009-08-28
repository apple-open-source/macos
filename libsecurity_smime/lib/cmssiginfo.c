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
#include "secitem.h"
#include "secoid.h"
#include "cryptohi.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <Security/SecKeychain.h>
#include <Security/SecIdentity.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecKeyPriv.h>
#include <CoreFoundation/CFTimeZone.h>


#define HIDIGIT(v) (((v) / 10) + '0')    
#define LODIGIT(v) (((v) % 10) + '0')     

#define ISDIGIT(dig) (((dig) >= '0') && ((dig) <= '9'))
#define CAPTURE(var,p,label)                              \
{                                                         \
    if (!ISDIGIT((p)[0]) || !ISDIGIT((p)[1])) goto label; \
    (var) = ((p)[0] - '0') * 10 + ((p)[1] - '0');         \
}

#define SIGINFO_DEBUG	0
#if	SIGINFO_DEBUG
#define dprintf(args...)      printf(args)
#else
#define dprintf(args...)
#endif

static OSStatus
DER_UTCTimeToCFDate(const CSSM_DATA_PTR utcTime, CFAbsoluteTime *date)
{
    CFGregorianDate gdate;
    char *string = (char *)utcTime->Data;
    long year, month, mday, hour, minute, second, hourOff, minOff;
    CFTimeZoneRef timeZone;

    /* Verify time is formatted properly and capture information */
    second = 0;
    hourOff = 0;
    minOff = 0;
    CAPTURE(year,string+0,loser);
    if (year < 50) {
        /* ASSUME that year # is in the 2000's, not the 1900's */
        year += 100;
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

    gdate.year = year + 1900;
    gdate.month = month;
    gdate.day = mday;
    gdate.hour = hour;
    gdate.minute = minute;
    gdate.second = second;

    if (hourOff == 0 && minOff == 0)
	timeZone = NULL; /* GMT */
    else
    {
	timeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, (hourOff * 60 + minOff) * 60);
    }

    *date = CFGregorianDateGetAbsoluteTime(gdate, timeZone);
    if (timeZone)
	CFRelease(timeZone);

    return SECSuccess;

loser:
    return SECFailure;
}

static OSStatus
DER_CFDateToUTCTime(CFAbsoluteTime date, CSSM_DATA_PTR utcTime)
{
    CFGregorianDate gdate =  CFAbsoluteTimeGetGregorianDate(date, NULL /* GMT */);
    unsigned char *d;
    SInt8 second;

    utcTime->Length = 13;
    utcTime->Data = d = PORT_Alloc(13);
    if (!utcTime->Data)
	return SECFailure;

    /* UTC time does not handle the years before 1950 */
    if (gdate.year < 1950)
            return SECFailure;

    /* remove the century since it's added to the year by the
       CFAbsoluteTimeGetGregorianDate routine, but is not needed for UTC time */
    gdate.year %= 100;
    second = gdate.second + 0.5;

    d[0] = HIDIGIT(gdate.year);
    d[1] = LODIGIT(gdate.year);
    d[2] = HIDIGIT(gdate.month);   
    d[3] = LODIGIT(gdate.month);
    d[4] = HIDIGIT(gdate.day);
    d[5] = LODIGIT(gdate.day);
    d[6] = HIDIGIT(gdate.hour);
    d[7] = LODIGIT(gdate.hour);  
    d[8] = HIDIGIT(gdate.minute);
    d[9] = LODIGIT(gdate.minute);
    d[10] = HIDIGIT(second);
    d[11] = LODIGIT(second);
    d[12] = 'Z';
    return SECSuccess;
}

/* =============================================================================
 * SIGNERINFO
 */
SecCmsSignerInfoRef
nss_cmssignerinfo_create(SecCmsMessageRef cmsg, SecCmsSignerIDSelector type, SecCertificateRef cert, CSSM_DATA_PTR subjKeyID, SecPublicKeyRef pubKey, SecPrivateKeyRef signingKey, SECOidTag digestalgtag);

SecCmsSignerInfoRef
SecCmsSignerInfoCreateWithSubjKeyID(SecCmsMessageRef cmsg, CSSM_DATA_PTR subjKeyID, SecPublicKeyRef pubKey, SecPrivateKeyRef signingKey, SECOidTag digestalgtag)
{
    return nss_cmssignerinfo_create(cmsg, SecCmsSignerIDSubjectKeyID, NULL, subjKeyID, pubKey, signingKey, digestalgtag); 
}

SecCmsSignerInfoRef
SecCmsSignerInfoCreate(SecCmsMessageRef cmsg, SecIdentityRef identity, SECOidTag digestalgtag)
{
    SecCmsSignerInfoRef signerInfo = NULL;
    SecCertificateRef cert = NULL;
    SecPrivateKeyRef signingKey = NULL;

    if (SecIdentityCopyCertificate(identity, &cert))
	goto loser;
    if (SecIdentityCopyPrivateKey(identity, &signingKey))
	goto loser;

    signerInfo = nss_cmssignerinfo_create(cmsg, SecCmsSignerIDIssuerSN, cert, NULL, NULL, signingKey, digestalgtag);

loser:
    if (cert)
	CFRelease(cert);
    if (signingKey)
	CFRelease(signingKey);

    return signerInfo;
}

SecCmsSignerInfoRef
nss_cmssignerinfo_create(SecCmsMessageRef cmsg, SecCmsSignerIDSelector type, SecCertificateRef cert, CSSM_DATA_PTR subjKeyID, SecPublicKeyRef pubKey, SecPrivateKeyRef signingKey, SECOidTag digestalgtag)
{
    void *mark;
    SecCmsSignerInfoRef signerinfo;
    int version;
    PLArenaPool *poolp;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    signerinfo = (SecCmsSignerInfoRef)PORT_ArenaZAlloc(poolp, sizeof(SecCmsSignerInfo));
    if (signerinfo == NULL) {
	PORT_ArenaRelease(poolp, mark);
	return NULL;
    }


    signerinfo->cmsg = cmsg;

    switch(type) {
    case SecCmsSignerIDIssuerSN:
        signerinfo->signerIdentifier.identifierType = SecCmsSignerIDIssuerSN;
        if ((signerinfo->cert = CERT_DupCertificate(cert)) == NULL)
	    goto loser;
        if ((signerinfo->signerIdentifier.id.issuerAndSN = CERT_GetCertIssuerAndSN(poolp, cert)) == NULL)
	    goto loser;
	dprintf("nss_cmssignerinfo_create: SecCmsSignerIDIssuerSN: cert.rc %d\n",
	    (int)CFGetRetainCount(signerinfo->cert));
        break;
    case SecCmsSignerIDSubjectKeyID:
        signerinfo->signerIdentifier.identifierType = SecCmsSignerIDSubjectKeyID;
        PORT_Assert(subjKeyID);
        if (!subjKeyID)
            goto loser;
        signerinfo->signerIdentifier.id.subjectKeyID = PORT_ArenaNew(poolp, CSSM_DATA);
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
    if (si->cert != NULL) {
	dprintf("SecCmsSignerInfoDestroy top: certp %p cert.rc %d\n",
	    si->cert, (int)CFGetRetainCount(si->cert));
	CERT_DestroyCertificate(si->cert);
    }
    if (si->certList != NULL) {
	dprintf("SecCmsSignerInfoDestroy top: certList.rc %d\n",
	    (int)CFGetRetainCount(si->certList));
	CFRelease(si->certList);
    }
    /* XXX storage ??? */
}

/*
 * SecCmsSignerInfoSign - sign something
 *
 */
OSStatus
SecCmsSignerInfoSign(SecCmsSignerInfoRef signerinfo, CSSM_DATA_PTR digest, CSSM_DATA_PTR contentType)
{
    SecCertificateRef cert;
    SecPrivateKeyRef privkey = NULL;
    SECOidTag digestalgtag;
    SECOidTag pubkAlgTag;
    CSSM_DATA signature = { 0 };
    OSStatus rv;
    PLArenaPool *poolp, *tmppoolp;
    const SECAlgorithmID *algID;
    SECAlgorithmID freeAlgID;
    //CERTSubjectPublicKeyInfo *spki;

    PORT_Assert (digest != NULL);

    poolp = signerinfo->cmsg->poolp;

    switch (signerinfo->signerIdentifier.identifierType) {
    case SecCmsSignerIDIssuerSN:
        privkey = signerinfo->signingKey;
        signerinfo->signingKey = NULL;
        cert = signerinfo->cert;
	if (SecCertificateGetAlgorithmID(cert,&algID)) {
	    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	    goto loser;
        }
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
	if (SecKeyGetAlgorithmID(signerinfo->pubKey,&algID)) {
	    PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	    goto loser;
        }
	CFRelease(signerinfo->pubKey);
        signerinfo->pubKey = NULL;
#endif
        break;
    default:
        PORT_SetError(SEC_ERROR_UNSUPPORTED_MESSAGE_TYPE);
        goto loser;
    }
    digestalgtag = SecCmsSignerInfoGetDigestAlgTag(signerinfo);
    /*
     * XXX I think there should be a cert-level interface for this,
     * so that I do not have to know about subjectPublicKeyInfo...
     */
    pubkAlgTag = SECOID_GetAlgorithmTag(algID);
    if (signerinfo->signerIdentifier.identifierType == SecCmsSignerIDSubjectKeyID) {
      SECOID_DestroyAlgorithmID(&freeAlgID, PR_FALSE);
    }

#if 0
    // @@@ Not yet
    /* Fortezza MISSI have weird signature formats.  
     * Map them to standard DSA formats 
     */
    pubkAlgTag = PK11_FortezzaMapSig(pubkAlgTag);
#endif

    if (signerinfo->authAttr != NULL) {
	CSSM_DATA encoded_attrs;

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

	rv = SEC_SignData(&signature, encoded_attrs.Data, encoded_attrs.Length, 
	                  privkey, digestalgtag, pubkAlgTag);
	PORT_FreeArena(tmppoolp, PR_FALSE); /* awkward memory management :-( */
    } else {
	rv = SGN_Digest(privkey, digestalgtag, pubkAlgTag, &signature, digest);
    }
    SECKEY_DestroyPrivateKey(privkey);
    privkey = NULL;

    if (rv != SECSuccess)
	goto loser;

    if (SECITEM_CopyItem(poolp, &(signerinfo->encDigest), &signature) 
          != SECSuccess)
	goto loser;

    SECITEM_FreeItem(&signature, PR_FALSE);

    if(pubkAlgTag == SEC_OID_EC_PUBLIC_KEY) {
	/*
	 * RFC 3278 section section 2.1.1 states that the signatureAlgorithm 
	 * field contains the full ecdsa-with-SHA1 OID, not plain old ecPublicKey 
	 * as would appear in other forms of signed datas. However Microsoft doesn't 
	 * do this, it puts ecPublicKey there, and if we put ecdsa-with-SHA1 there, 
	 * MS can't verify - presumably because it takes the digest of the digest 
	 * before feeding it to ECDSA.
	 * We handle this with a preference; default if it's not there is 
	 * "Microsoft compatibility mode". 
	 */
	if(!SecCmsMsEcdsaCompatMode()) {
	    pubkAlgTag = SEC_OID_ECDSA_WithSHA1;
	}
	/* else violating the spec for compatibility */
    }

    if (SECOID_SetAlgorithmID(poolp, &(signerinfo->digestEncAlg), pubkAlgTag, 
                              NULL) != SECSuccess)
	goto loser;

    return SECSuccess;

loser:
    if (signature.Length != 0)
	SECITEM_FreeItem (&signature, PR_FALSE);
    if (privkey)
	SECKEY_DestroyPrivateKey(privkey);
    if((algID != NULL) & (algID != &freeAlgID)) {
	/* this is dicey - this was actually mallocd by either SecCertificate or 
	 * by SecKey...it all boils down to a free() in the end though. */
	SECOID_DestroyAlgorithmID((SECAlgorithmID *)algID, PR_FALSE);
    }
    return SECFailure;
}

OSStatus
SecCmsSignerInfoVerifyCertificate(SecCmsSignerInfoRef signerinfo, SecKeychainRef keychainOrArray,
				  CFTypeRef policies, SecTrustRef *trustRef)
{
    SecCertificateRef cert;
    CFAbsoluteTime stime;
    OSStatus rv;
    CSSM_DATA_PTR *otherCerts;
    
    if ((cert = SecCmsSignerInfoGetSigningCertificate(signerinfo, keychainOrArray)) == NULL) {
	dprintf("SecCmsSignerInfoVerifyCertificate: no signing cert\n");
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
    rv = SecCmsSignedDataRawCerts(signerinfo->sigd, &otherCerts);
    if(rv) {
	return rv;
    }
    rv = CERT_VerifyCert(keychainOrArray, cert, otherCerts, policies, stime, trustRef);
    dprintf("SecCmsSignerInfoVerifyCertificate after vfy: certp %p cert.rc %d\n",
	    cert, (int)CFGetRetainCount(cert));
    if (rv || !trustRef)
    {
	if (PORT_GetError() == SEC_ERROR_UNTRUSTED_CERT)
	{
	    /* Signature or digest level verificationStatus errors should supercede certificate level errors, so only change the verificationStatus if the status was GoodSignature. */
	    if (signerinfo->verificationStatus == SecCmsVSGoodSignature)
		signerinfo->verificationStatus = SecCmsVSSigningCertNotTrusted;
	}
    }
    /* FIXME isn't this leaking the cert? */
    dprintf("SecCmsSignerInfoVerifyCertificate: CertVerify rtn %d\n", (int)rv);
    return rv;
}

/*
 * SecCmsSignerInfoVerify - verify the signature of a single SignerInfo
 *
 * Just verifies the signature. The assumption is that verification of the certificate
 * is done already.
 */
OSStatus
SecCmsSignerInfoVerify(SecCmsSignerInfoRef signerinfo, CSSM_DATA_PTR digest, CSSM_DATA_PTR contentType)
{
    SecPublicKeyRef publickey = NULL;
    SecCmsAttribute *attr;
    CSSM_DATA encoded_attrs;
    SecCertificateRef cert;
    SecCmsVerificationStatus vs = SecCmsVSUnverified;
    PLArenaPool *poolp;
    SECOidTag digestAlgTag, digestEncAlgTag;
    
    if (signerinfo == NULL)
	return SECFailure;
    
    /* SecCmsSignerInfoGetSigningCertificate will fail if 2nd parm is NULL and */
    /* cert has not been verified */
    if ((cert = SecCmsSignerInfoGetSigningCertificate(signerinfo, NULL)) == NULL) {
	dprintf("SecCmsSignerInfoVerify: no signing cert\n");
	vs = SecCmsVSSigningCertNotFound;
	goto loser;
    }
    dprintf("SecCmsSignerInfoVerify top: cert %p cert.rc %d\n",
	    cert, (int)CFGetRetainCount(cert));

    if (SecCertificateCopyPublicKey(cert, &publickey)) {
	vs = SecCmsVSProcessingError;
	goto loser;
    }

    digestAlgTag = SECOID_GetAlgorithmTag(&(signerinfo->digestAlg));
    digestEncAlgTag = SECOID_GetAlgorithmTag(&(signerinfo->digestEncAlg));
    
    /*
     * Gross hack necessitated by RFC 3278 section 2.1.1, which states 
     * that the signature algorithm (here, digestEncAlg) contains ecdsa_with-SHA1, 
     * *not* (as in all other algorithms) the raw signature algorithm, e.g. 
     * pkcs1RSAEncryption.
     */
    if(digestEncAlgTag == SEC_OID_ECDSA_WithSHA1) {
	digestEncAlgTag = SEC_OID_EC_PUBLIC_KEY;
    }
    
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

	vs = (VFY_VerifyData (encoded_attrs.Data, encoded_attrs.Length,
			publickey, &(signerinfo->encDigest),
			digestAlgTag, digestEncAlgTag,
			signerinfo->cmsg->pwfn_arg) != SECSuccess) ? SecCmsVSBadSignature : SecCmsVSGoodSignature;

	PORT_FreeArena(poolp, PR_FALSE);	/* awkward memory management :-( */

    } else {
	CSSM_DATA_PTR sig;

	/* No authenticated attributes. The signature is based on the plain message digest. */
	sig = &(signerinfo->encDigest);
	if (sig->Length == 0)
	    goto loser;

	vs = (VFY_VerifyDigest(digest, publickey, sig,
			digestAlgTag, digestEncAlgTag,
			signerinfo->cmsg->pwfn_arg) != SECSuccess) ? SecCmsVSBadSignature : SecCmsVSGoodSignature;
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
    dprintf("SecCmsSignerInfoVerify end: cerp %p cert.rc %d\n",
	    cert, (int)CFGetRetainCount(cert));

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
    dprintf("SecCmsSignerInfoGetCertList: certList.rc %d\n",
	    (int)CFGetRetainCount(signerinfo->certList));
    return signerinfo->certList;
}

int
SecCmsSignerInfoGetVersion(SecCmsSignerInfoRef signerinfo)
{
    unsigned long version;

    /* always take apart the CSSM_DATA */
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
    CSSM_DATA_PTR value;

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
    SecCertificateRef cert;
    SecCmsSignerIdentifier *sid;
    OSStatus ortn;
    CSSM_DATA_PTR *rawCerts;
    
    if (signerinfo->cert != NULL) {
	dprintf("SecCmsSignerInfoGetSigningCertificate top: cert %p cert.rc %d\n",
	    signerinfo->cert, (int)CFGetRetainCount(signerinfo->cert));
	return signerinfo->cert;
    }
    ortn = SecCmsSignedDataRawCerts(signerinfo->sigd, &rawCerts);
    if(ortn) {
	return NULL;
    }
    dprintf("SecCmsSignerInfoGetSigningCertificate: numRawCerts %d\n", 
	SecCmsArrayCount((void **)rawCerts));
    
    /*
     * This cert will also need to be freed, but since we save it
     * in signerinfo for later, we do not want to destroy it when
     * we leave this function -- we let the clean-up of the entire
     * cinfo structure later do the destroy of this cert.
     */
    sid = &signerinfo->signerIdentifier;
    switch (sid->identifierType) {
    case SecCmsSignerIDIssuerSN:
	cert = CERT_FindCertByIssuerAndSN(keychainOrArray, rawCerts, signerinfo->cmsg->poolp,
	    sid->id.issuerAndSN);
	break;
    case SecCmsSignerIDSubjectKeyID:
	cert = CERT_FindCertBySubjectKeyID(keychainOrArray, rawCerts, sid->id.subjectKeyID);
	break;
    default:
	cert = NULL;
	break;
    }

    /* cert can be NULL at that point */
    signerinfo->cert = cert;	/* earmark it */
    dprintf("SecCmsSignerInfoGetSigningCertificate end: certp %p cert.rc %d\n",
	    signerinfo->cert, (int)CFGetRetainCount(signerinfo->cert));

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

    SecCertificateGetCommonName(signercert, &commonName);

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

    SecCertificateGetEmailAddress(signercert, &emailAddress);

    return emailAddress;
}


/*
 * SecCmsSignerInfoAddAuthAttr - add an attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 */
OSStatus
SecCmsSignerInfoAddAuthAttr(SecCmsSignerInfoRef signerinfo, SecCmsAttribute *attr)
{
    return SecCmsAttributeArrayAddAttr(signerinfo->cmsg->poolp, &(signerinfo->authAttr), attr);
}

/*
 * SecCmsSignerInfoAddUnauthAttr - add an attribute to the
 * unauthenticated attributes of "signerinfo". 
 */
OSStatus
SecCmsSignerInfoAddUnauthAttr(SecCmsSignerInfoRef signerinfo, SecCmsAttribute *attr)
{
    return SecCmsAttributeArrayAddAttr(signerinfo->cmsg->poolp, &(signerinfo->unAuthAttr), attr);
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
    CSSM_DATA stime;
    void *mark;
    PLArenaPool *poolp;

    poolp = signerinfo->cmsg->poolp;

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
    CSSM_DATA_PTR smimecaps = NULL;
    void *mark;
    PLArenaPool *poolp;

    poolp = signerinfo->cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    smimecaps = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimecaps == NULL)
	goto loser;

    /* create new signing time attribute */
#if 1
    // @@@ We don't do Fortezza yet.
    if (SecSMIMECreateSMIMECapabilities((SecArenaPoolRef)poolp, smimecaps, PR_FALSE) != SECSuccess)
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
    CSSM_DATA_PTR smimeekp = NULL;
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

    poolp = signerinfo->cmsg->poolp;
    mark = PORT_ArenaMark(poolp);

    smimeekp = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimeekp == NULL)
	goto loser;

    /* create new signing time attribute */
    if (SecSMIMECreateSMIMEEncKeyPrefs((SecArenaPoolRef)poolp, smimeekp, cert) != SECSuccess)
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
    CSSM_DATA_PTR smimeekp = NULL;
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

    poolp = signerinfo->cmsg->poolp;
    mark = PORT_ArenaMark(poolp);

    smimeekp = SECITEM_AllocItem(poolp, NULL, 0);
    if (smimeekp == NULL)
	goto loser;

    /* create new signing time attribute */
    if (SecSMIMECreateMSSMIMEEncKeyPrefs((SecArenaPoolRef)poolp, smimeekp, cert) != SECSuccess)
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
    SecCertificateRef cert = NULL;
    CSSM_DATA_PTR profile = NULL;
    SecCmsAttribute *attr;
    CSSM_DATA_PTR utc_stime = NULL;
    CSSM_DATA_PTR ekp;
    int save_error;
    OSStatus rv;
    Boolean must_free_cert = PR_FALSE;
    OSStatus status;
    SecKeychainRef keychainOrArray;
    
    status = SecKeychainCopyDefault(&keychainOrArray);

    /* sanity check - see if verification status is ok (unverified does not count...) */
    if (signerinfo->verificationStatus != SecCmsVSGoodSignature)
        return SECFailure;

    /* find preferred encryption cert */
    if (!SecCmsArrayIsEmpty((void **)signerinfo->authAttr) &&
	(attr = SecCmsAttributeArrayFindAttrByOidTag(signerinfo->authAttr,
			       SEC_OID_SMIME_ENCRYPTION_KEY_PREFERENCE, PR_TRUE)) != NULL)
    { /* we have a SMIME_ENCRYPTION_KEY_PREFERENCE attribute! */
	ekp = SecCmsAttributeGetValue(attr);
	if (ekp == NULL)
	    return SECFailure;

	/* we assume that all certs coming with the message have been imported to the */
	/* temporary database */
	cert = SecSMIMEGetCertFromEncryptionKeyPreference(keychainOrArray, ekp);
	if (cert == NULL)
	    return SECFailure;
	must_free_cert = PR_TRUE;
    }

    if (cert == NULL) {
	/* no preferred cert found?
	 * find the cert the signerinfo is signed with instead */
	CFStringRef emailAddress=NULL;

	cert = SecCmsSignerInfoGetSigningCertificate(signerinfo, keychainOrArray);
	if (cert == NULL)
	    return SECFailure;
	if (SecCertificateGetEmailAddress(cert,&emailAddress))
	    return SECFailure;
    }

    /* verify this cert for encryption (has been verified for signing so far) */    /* don't verify this cert for encryption. It may just be a signing cert.
     * that's OK, we can still save the S/MIME profile. The encryption cert
     * should have already been saved */
#ifdef notdef
    if (CERT_VerifyCert(keychainOrArray, cert, certUsageEmailRecipient, CFAbsoluteTimeGetCurrent(), NULL) != SECSuccess) {
	if (must_free_cert)
	    CERT_DestroyCertificate(cert);
	return SECFailure;
    }
#endif

    /* XXX store encryption cert permanently? */

    /*
     * Remember the current error set because we do not care about
     * anything set by the functions we are about to call.
     */
    save_error = PORT_GetError();

    if (!SecCmsArrayIsEmpty((void **)signerinfo->authAttr)) {
	attr = SecCmsAttributeArrayFindAttrByOidTag(signerinfo->authAttr,
				       SEC_OID_PKCS9_SMIME_CAPABILITIES,
				       PR_TRUE);
	profile = SecCmsAttributeGetValue(attr);
	attr = SecCmsAttributeArrayFindAttrByOidTag(signerinfo->authAttr,
				       SEC_OID_PKCS9_SIGNING_TIME,
				       PR_TRUE);
	utc_stime = SecCmsAttributeGetValue(attr);
    }

    rv = CERT_SaveSMimeProfile (cert, profile, utc_stime);
    if (must_free_cert)
	CERT_DestroyCertificate(cert);

    /*
     * Restore the saved error in case the calls above set a new
     * one that we do not actually care about.
     */
    PORT_SetError (save_error);

    return rv;
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
