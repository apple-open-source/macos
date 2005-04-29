/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * pkinit_apple_cms.c - CMS encode/decode routines, Mac OS X version
 *
 * Created 19 May 2004 by Doug Mitchell at Apple.
 */

/* 
 * Until we redo the SPI for the SMIME lib, it's not usable for verifying messages
 * with possible cert-related errors like unknown root.
 */
#define IGNORE_VERIFY_ERRORS    1

/*
 * As of May 19 2004, the SMIME library is incapable of handling a CMS message with
 * ContentType SignedData wherein the inner EncapsulatedData has a ContentType
 * of Data. This precludes following the PKINIT spec, which specifies an eContentType
 * of pkauthdata for the SignedAuthPack type.
 *
 * Eventually the SMIME library will need something like this:
 *
 * extern OSStatus SecCmsContentInfoSetContentDataAndOid(
 *      SecCmsMessageRef cmsg, 
 *      SecCmsContentInfoRef cinfo, 
 *      CSSM_DATA_PTR data, 
 *      SECOidTag oid,		// ==> eContentType
 *      Boolean detached);
 *
 * The same is true for the ContentType of an EncryptedContentInfo - it's hard-coded
 * to be id-data. 
 *
 * I tried implementing this but ran into a host of problems internal to SMIME which 
 * result from the lack of handling anything other than the OIDs int he CMS
 * spec for this field. Radar 3665640 is tracking this enhancement request. 
 */
#define PKINIT_VARIABLE_CONTENT_TYPE      0

#include "pkinit_cms.h"
#include "pkinit_asn1.h"
#include "pkinit_apple_utils.h"
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsEnvelopedData.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsRecipientInfo.h>
#include <Security/SecSMIME.h>
#include <Security/Security.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecTrustPriv.h>
#include <assert.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>

#pragma mark ----- CMS utilities ----

/*
 * Convert platform-specific cert/signature status to pki_cert_sig_status.
 */
static pki_cert_sig_status pkiCertSigStatus(
    OSStatus certStatus)
{
    switch(certStatus) {
	case CSSM_OK:
	    return pki_cs_good;
	case CSSMERR_CSP_VERIFY_FAILED:
	    return pki_cs_sig_verify_fail;
	case CSSMERR_TP_NOT_TRUSTED:
	    return pki_cs_no_root;
	case CSSMERR_TP_INVALID_ANCHOR_CERT:
	    return pki_cs_unknown_root;
	case CSSMERR_TP_CERT_EXPIRED:
	    return pki_cs_expired;
	case CSSMERR_TP_CERT_NOT_VALID_YET:
	    return pki_cs_not_valid_yet;
	case CSSMERR_TP_CERT_REVOKED:
	    return pki_cs_revoked;
	case KRB5_KDB_UNAUTH:
	    return pki_cs_untrusted;
	case CSSMERR_TP_INVALID_CERTIFICATE:
	    return pki_cs_bad_leaf;
	default:
	    return pki_cs_other_err;
    }
}

/*
 * Cook up a SecCertificateRef from a krb5_data.
 */
static OSStatus pkiKrb5DataToSecCert(
    const krb5_data *rawCert,
    SecCertificateRef *secCert)     // RETURNED
{
    CSSM_DATA certData;
    OSStatus ortn;
    
    assert((rawCert != NULL) && (secCert != NULL));
    
    certData.Data = rawCert->data;
    certData.Length = rawCert->length;
    ortn = SecCertificateCreateFromData(&certData, CSSM_CERT_X_509v3, 
	CSSM_CERT_ENCODING_DER, secCert);
    if(ortn) {
	pkiCssmErr("SecCertificateCreateFromData", ortn);
    }
    return ortn;
}

static OSStatus pkiEncodeCms(
    SecCmsMessageRef	cmsMsg,
    const unsigned char *inData,	// add in this
    unsigned		inDataLen,
    unsigned char	**outData,	// mallocd and RETURNED
    unsigned		*outDataLen)	// RETURNED
{
    SecArenaPoolRef arena = NULL;
    SecArenaPoolCreate(1024, &arena);
    SecCmsEncoderRef cmsEnc = NULL;
    CSSM_DATA output = { 0, NULL };
    OSStatus ortn;
    
    ortn = SecCmsEncoderCreate(cmsMsg, 
	    NULL, NULL,			// no callback 
	    &output, arena,		// data goes here
	    NULL, NULL,			// no password callback (right?) 
	    NULL, NULL,			// decrypt key callback
	    NULL, NULL,			// detached digests
	    &cmsEnc);
    if(ortn) {
	pkiCssmErr("SecCmsEncoderCreate", ortn);
	goto errOut;
    }
    ortn = SecCmsEncoderUpdate(cmsEnc, (char *)inData, inDataLen);
    if(ortn) {
	pkiCssmErr("SecCmsEncoderUpdate", ortn);
	goto errOut;
    }
    ortn = SecCmsEncoderFinish(cmsEnc);
    if(ortn) {
	pkiCssmErr("SecCMsEncoderFinish", ortn);
	goto errOut;
    }
    
    /* Did we get any data? */
    if(output.Length) {
	*outData = (unsigned char *)malloc(output.Length);
	memmove(*outData, output.Data, output.Length);
	*outDataLen = output.Length;
    }
    else {
	*outData = NULL;
	*outDataLen = 0;
    }
errOut:
    if(arena) {
	SecArenaPoolFree(arena, false);
    }
    return ortn;
}

#pragma mark ----- Create SignedData ----

/*
 * Create a ContentInfo, Type SignedData. 
 */
krb5_error_code pkinit_create_signed_data(
    const krb5_data	    *to_be_signed,	// Content
    pkinit_signing_cert_t   signing_cert,	// to be signed by this cert
    krb5_boolean	    include_cert,	// TRUE --> include signing_cert in 
						//     SignerInfo
    PKI_ContentType	    content_type,       // OID for EncapsulatedData
    krb5_data		    *content_info)      // contents mallocd and RETURNED
{
    OSStatus ortn;
    SecIdentityRef idRef = (SecIdentityRef)signing_cert;
    SecCmsMessageRef cmsMsg = NULL;
    SecCmsContentInfoRef contentInfo = NULL;
    SecCmsSignedDataRef signedData = NULL;
    SecCertificateRef ourCert = NULL;
    SecCmsSignerInfoRef signerInfo;
    SecKeychainRef kcRef = NULL;
    unsigned char *outData;
    unsigned outDataLen;
    SecKeyRef keyRef = NULL;
    #if     PKINIT_VARIABLE_CONTENT_TYPE
    SECOidTag whichOid;
    #endif
    SecCmsCertChainMode certChainMode;
    
    assert((to_be_signed != NULL) && 
	   (signing_cert != NULL) &&
	   (content_info != NULL));
	       
    #if     PKINIT_VARIABLE_CONTENT_TYPE
    /*
     * FIXME: our CMS encoder can't deal with nonstandard eContentTypes.
     * We're going to use SEC_OID_PKCS7_DATA in all cases, which is 
     * not conforming to the PKINIT spec.
     */
    switch(content_type) {
	case ECT_Data:
	    whichOid = SEC_OID_PKCS7_DATA;
	    break;
	case ECT_PkAuthData:
	    pkiDebug("**WARNING: content_type ECT_PkAuthData not supported\n");
	    whichOid = SEC_OID_KERBEROS_PK_AUTH_DATA;
	    break;
	default:
	    pkiDebug("pkinit_create_signed_data: bad content_type spec\n");
	    ortn = paramErr;
	    goto errOut;
    }
    #endif  /* PKINIT_VARIABLE_CONTENT_TYPE */
    
    /* Save the actual keychain-resident cert for later use */
    ortn = SecIdentityCopyCertificate(idRef, &ourCert);
    if(ortn) {
	pkiCssmErr("SecIdentityCopyCertificate", ortn);
	goto errOut;
    }
    
    /*
     * Get keychain on which the identity resides. 
     * Due to Radar 3661602 we need to jump thru some hoops here.
     */
    ortn = SecIdentityCopyPrivateKey(idRef, &keyRef);
    if(ortn) {
	pkiCssmErr("SecIdentityCopyPrivateKey", ortn);
	goto errOut;
    }
    ortn = SecKeychainItemCopyKeychain((SecKeychainItemRef)keyRef, &kcRef);
    CFRelease(keyRef);
    if(ortn) {
	pkiCssmErr("SecKeychainItemCopyKeychain", ortn);
	goto errOut;
    }

    /* build chain of objects: message->signedData->data */
    cmsMsg = SecCmsMessageCreate(NULL);
    if(cmsMsg == NULL) {
	pkiDebug("***Error creating SecCmsMessageRef\n");
	ortn = -1;
	goto errOut;
    }
    signedData = SecCmsSignedDataCreate(cmsMsg);
    if(signedData == NULL) {
	pkiDebug("***Error creating SecCmsSignedDataRef\n");
	ortn = -1;
	goto errOut;
    }
    contentInfo = SecCmsMessageGetContentInfo(cmsMsg);
    ortn = SecCmsContentInfoSetContentSignedData(cmsMsg, contentInfo, signedData);
    if(ortn) {
	pkiCssmErr("SecCmsContentInfoSetContentSignedData", ortn);
	goto errOut;
    }
    contentInfo = SecCmsSignedDataGetContentInfo(signedData);
    #if     PKINIT_VARIABLE_CONTENT_TYPE
    ortn = SecCmsContentInfoSetContentDataAndOid(cmsMsg, contentInfo, NULL /* data */, 
	whichOid, false);
    #else
    ortn = SecCmsContentInfoSetContentData(cmsMsg, contentInfo, NULL, false);
    #endif  /* PKINIT_VARIABLE_CONTENT_TYPE */
    if(ortn) {
	pkiCssmErr("SecCmsContentInfoSetContentData", ortn);
	goto errOut;
    }

    /* 
     * create & attach signer information
     */
    signerInfo = SecCmsSignerInfoCreate(cmsMsg, idRef, SEC_OID_SHA1);
    if (signerInfo == NULL) {
	pkiDebug("***Error on SecCmsSignerInfoCreate\n");
	ortn = -1;
	goto errOut;
    }
    /* we want the cert chain included for this one */
    /* FIXME - what's the significance of the usage? */
    if(include_cert) {
	certChainMode = SecCmsCMCertChainWithRoot;
    }
    else {
	certChainMode = SecCmsCMNone;
    }
    ortn = SecCmsSignerInfoIncludeCerts(signerInfo, certChainMode, certUsageEmailSigner);
    if(ortn) {
	pkiCssmErr("SecCmsSignerInfoIncludeCerts", ortn);
	goto errOut;
    }

    ortn = SecCmsSignedDataAddSignerInfo(signedData, signerInfo);
    if(ortn) {
	pkiCssmErr("SecCmsSignedDataAddSignerInfo", ortn);
	goto errOut;
    }

    /* go */
    ortn = pkiEncodeCms(cmsMsg, to_be_signed->data, to_be_signed->length, 
	&outData, &outDataLen);
    if(ortn) {
	goto errOut;
    }
    
    /* transfer ownership of mallocd data to caller */
    content_info->data = (char *)outData;
    content_info->length = outDataLen;

errOut:
    if(cmsMsg) {
	SecCmsMessageDestroy(cmsMsg);
    }
    if(ourCert) {
	CFRelease(ourCert);
    }
    if(kcRef) {
	CFRelease(kcRef);
    }
    return ortn;
}

#pragma mark ----- Create EnvelopedData ----

/*
 * Create a ContentInfo, Type EnvelopedData. 
 */
krb5_error_code pkinit_create_envel_data(
    const krb5_data	*raw_content,	    // Content
    const krb5_data	*recip_cert,	    // to be encrypted with this cert
    PKI_ContentType     content_type,       // OID for EncryptedContentInfo
    krb5_data		*content_info)      // contents mallocd and RETURNED
{
    SecCmsMessageRef cmsMsg = NULL;
    SecCmsContentInfoRef contentInfo = NULL;
    SecCmsEnvelopedDataRef envelopedData = NULL;
    SecCmsRecipientInfoRef recipientInfo = NULL;
    OSStatus ortn;
    SecCertificateRef allCerts[2];
    SECOidTag algorithmTag;
    int keySize;
    unsigned char *outData;
    unsigned outDataLen;
    #if     PKINIT_VARIABLE_CONTENT_TYPE
    SECOidTag whichOid;
    #endif
	
    assert((raw_content != NULL) && (recip_cert != NULL) && (content_info != NULL));

    #if     PKINIT_VARIABLE_CONTENT_TYPE
    /*
     * FIXME: our CMS encoder can't deal with nonstandard EncryptedContentInfo.ContentTypes.
     * We're going to use SEC_OID_PKCS7_DATA in all cases, which is 
     * not conforming to the PKINIT spec.
     */
    switch(content_type) {
	case ECT_Data:
	    whichOid = SEC_OID_PKCS7_DATA;
	    break;
	case ECT_PkReplyKeyKata:
	    pkiDebug("**WARNING: content_type ECT_PkReplyKeyKata not supported\n");
	    goto errOut;
    }
    #endif  /* PKINIT_VARIABLE_CONTENT_TYPE */
    
    /*
     * Set up a NULL_terminated array of recipient certs in SecCertificateRef format.
     */
    ortn = pkiKrb5DataToSecCert(recip_cert, &allCerts[0]);
    if(ortn) {
	return ortn;
    }
    allCerts[1] = NULL;
    
    /* Infer some reasonable encryption parameters */
    ortn = SecSMIMEFindBulkAlgForRecipients(allCerts, &algorithmTag, &keySize);
    if(ortn) {
	pkiCssmErr("SecSMIMEFindBulkAlgForRecipients", ortn);
	goto errOut;
    }
	
    /* build chain of objects: message->envelopedData->data */
    cmsMsg = SecCmsMessageCreate(NULL);
    if(cmsMsg == NULL) {
	pkiDebug("***Error creating SecCmsMessageRef\n");
	ortn = -1;
	goto errOut;
    }
    envelopedData = SecCmsEnvelopedDataCreate(cmsMsg, algorithmTag, keySize);
    if(envelopedData == NULL) {
	pkiDebug("***Error creating SecCmsEnvelopedDataRef\n");
	ortn = -1;
	goto errOut;
    }
    contentInfo = SecCmsMessageGetContentInfo(cmsMsg);
    ortn = SecCmsContentInfoSetContentEnvelopedData(cmsMsg, contentInfo, envelopedData);
    if(ortn) {
	pkiCssmErr("SecCmsContentInfoSetContentEnvelopedData", ortn);
	goto errOut;
    }
    contentInfo = SecCmsEnvelopedDataGetContentInfo(envelopedData);
    #if     PKINIT_VARIABLE_CONTENT_TYPE
    /* something like this: */
    ortn = SecCmsContentInfoSetContentDataAndOid(cmsMsg, contentInfo, NULL /* data */, 
	whichOid, false);
    #else
    ortn = SecCmsContentInfoSetContentData(cmsMsg, contentInfo, NULL /* data */, false);
    #endif  /* PKINIT_VARIABLE_CONTENT_TYPE */
    if(ortn) {
	pkiCssmErr("SecCmsContentInfoSetContentData", ortn);
	goto errOut;
    }
	
    /* create & attach recipient information */
    recipientInfo = SecCmsRecipientInfoCreate(cmsMsg, allCerts[0]);
    ortn = SecCmsEnvelopedDataAddRecipient(envelopedData, recipientInfo);
    if(ortn) {
	pkiCssmErr("SecCmsEnvelopedDataAddRecipient", ortn);
	goto errOut;
    }

    /* go */
    ortn = pkiEncodeCms(cmsMsg, raw_content->data, raw_content->length, 
	&outData, &outDataLen);
    if(ortn) {
	goto errOut;
    }
    
    /* transfer ownership of mallocd data to caller */
    content_info->data = (char *)outData;
    content_info->length = outDataLen;

errOut:
    /* free resources */
    if(cmsMsg) {
	SecCmsMessageDestroy(cmsMsg);
    }
    if(allCerts[0]) {
	CFRelease(allCerts[0]);
    }
    return ortn;
}

#pragma mark ----- Parse SignedData ----

/*
 * Glean as much info from a SecTrust as possible, down to the TP verify code.
 * Return codes of note:
 * 
 * KRB5_KDB_UNAUTH - user-specified trust violation
 * CSSMERR_TP_INVALID_ANCHOR_CERT- Untrusted root
 * CSSMERR_TP_NOT_TRUSTED - No root cert found
 * CSSMERR_TP_CERT_EXPIRED
 * CSSMERR_TP_CERT_NOT_VALID_YET
 */
static OSStatus pkiEvalSecTrust(
    SecTrustRef secTrust)
{
    OSStatus ortn;
    SecTrustResultType			secTrustResult;
    
    ortn = SecTrustEvaluate(secTrust, &secTrustResult);
    if(ortn) {
	/* should never happen */
	pkiCssmErr("SecTrustEvaluate", ortn);
	return ortn;
    }
    switch(secTrustResult) {
	case kSecTrustResultUnspecified:
	    /* cert chain valid, no special UserTrust assignments */
	case kSecTrustResultProceed:
	    /* cert chain valid AND user explicitly trusts this */
	    return noErr;
	case kSecTrustResultDeny:
	case kSecTrustResultConfirm:
	    /*
	     * Cert chain may well have verified OK, but user has flagged
	     * one of these certs as untrustable.
	     */
	    return KRB5_KDB_UNAUTH;
	default:
	{
	    /* get low-level TP error */
	    OSStatus tpStatus;
	    ortn = SecTrustGetCssmResultCode(secTrust, &tpStatus);
	    if(ortn) {
		pkiCssmErr("SecTrustGetCssmResultCode", ortn);
		return ortn;
	    }
	    return tpStatus;
	}
    } 	/* SecTrustEvaluate error */
}

/*
 * Parse a SignedData, assumed to be signed by ONE signer.
 * All return fields are optional. In particular passing in a NULL for
 * certVerifyStatus causes us to skip the signature and cert verify operations
 * (which are atomic at the CMS API, both are done in 
 * SecCmsSignedDataVerifySignerInfo().)
 *
 * Nonzero return means we flat out could not get to the low-level info.
 */
static OSStatus pkiParseSignedData(
    SecCmsSignedDataRef signedData,
    pkinit_cert_db_t cert_db,		// required for verifying SignedData
    OSStatus *certVerifyStatus,		// optional, RETURNED
    SecCertificateRef *signerCert,      // optional, RETURNED
    CSSM_DATA ***allCerts)		// optional, RETURNED
{
    SecTrustRef secTrust = NULL;
    OSStatus ortn = noErr;
    SecPolicyRef policy = NULL;
    SecPolicySearchRef policySearch = NULL;
    Boolean b;
    SecCmsSignerInfoRef signerInfo = NULL;
    
    if(signerCert) {
	*signerCert = NULL;
    }
    if(certVerifyStatus) {
	*certVerifyStatus = -1;
    }
    if(allCerts) {
	*allCerts = NULL;
    }
    
    int numSigners = SecCmsSignedDataSignerInfoCount(signedData);
    if(numSigners != 1) {
	pkiDebug("***pkiParseSignedData: numSigners %d, expected 1\n", numSigners);
	return internalComponentErr;
    }

    /* 
     * We have to retrieve the cert list in two cases - caller wants to do a sig/cert
     * verify, or caller wants the cert list or even the signing cert. 
     */
    if((certVerifyStatus != NULL) ||
       (signerCert != NULL) ||
       (allCerts != NULL)) {
	CSSM_DATA_PTR *certList = SecCmsSignedDataGetCertificateList(signedData);
	if(certList == NULL) {
	    pkiDebug("***pkiParseSignedData: no certList available\n");
	    return ASN1_BAD_FORMAT;
	}
	if(allCerts != NULL) {
	    *allCerts = certList;
	}
	if((certVerifyStatus != NULL) ||
	   (signerCert != NULL)) {
	    /*
	     * For this we have to import the certs from the CMS message into
	     * the caller-specified keychain. Ugh. But this is how our CMS library
	     * is "designed".
	     */
	    unsigned numCerts = pkiNssArraySize((const void **)certList);
	    unsigned dex;
	    if(numCerts == 0) {
		pkiDebug("***pkiParseSignedData: empty certList\n");
		return ASN1_BAD_FORMAT;
	    }
	    if(cert_db == NULL) {
		pkiDebug("***pkiParseSignedData requires a cert_db to proceed\n");
		return internalComponentErr;
	    }
	    for(dex=0; dex<numCerts; dex++) {
		SecCertificateRef certRef;
		OSStatus ortn;
		ortn = SecCertificateCreateFromData(certList[dex], CSSM_CERT_X_509v3,
		    CSSM_CERT_ENCODING_DER, &certRef);
		if(ortn) {
		    pkiCssmErr("pkiParseSignedData:SecCertificateCreateFromData", ortn);
		    return ortn;
		}
		ortn = SecCertificateAddToKeychain(certRef, (SecKeychainRef)cert_db);
		switch(ortn) {
		    case noErr:
			break;
		    case errSecDuplicateItem:   // this is perfectly OK
			ortn = noErr;
			break;
		    default:
			pkiCssmErr("pkiParseSignedData:SecCertificateAddToKeychain", 
			    ortn);
			break;
		}
		CFRelease(certRef);
		if(ortn) {
		    return ortn;
		}
	    }
	}
    }
    if(certVerifyStatus != NULL) {
	ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
	    &CSSMOID_APPLE_X509_BASIC, NULL, &policySearch);
	if(ortn) {
	    pkiCssmErr("SecPolicySearchCreate", ortn);
	    return ortn;
	}
	ortn = SecPolicySearchCopyNext(policySearch, &policy);
	if(ortn) {
	    pkiCssmErr("SecPolicySearchCopyNext", ortn);
	    CFRelease(policySearch);
	    return ortn;
	}
	
	b = SecCmsSignedDataHasDigests(signedData);
	if(b) {
	    ortn = SecCmsSignedDataVerifySignerInfo(signedData, 0, cert_db, 
		    policy, &secTrust);
	    if(ortn) {
		/* FIXME: in this case if we have a SecTrust, we may or may not 
		 * have actually verified the signature. We have to do that ourself
		 * to get an accurate "unknown root but good signature" status.
		 * This is a big TBD, the SMIME libray API just isn't written to
		 * allow this. 
		 */
		pkiCssmErr(" SecCmsSignedDataVerifySignerInfo", ortn);
		#if IGNORE_VERIFY_ERRORS
		ortn = noErr;
		#endif
	    }
	    if(secTrust == NULL) {
		pkiDebug("***NO SecTrust available!\n");
		#if IGNORE_VERIFY_ERRORS
		ortn = noErr;
		*certVerifyStatus = noErr;
		#else
		ortn = internalComponentErr;
		#endif
	    }
	    else {
		*certVerifyStatus = pkiEvalSecTrust(secTrust);
	    }
	}
	else {
	    pkiDebug("pkiParseSignedData: No digest attached to cms message\n");
	}
    }
    if(signerCert != NULL) {
	signerInfo = SecCmsSignedDataGetSignerInfo(signedData, 0);
	*signerCert = SecCmsSignerInfoGetSigningCertificate(signerInfo, cert_db);
    }
    if(allCerts != NULL) {
	*allCerts = SecCmsSignedDataGetCertificateList(signedData);
    }
    if(policySearch) {
	CFRelease(policySearch);
    }
    if(policy) {
	CFRelease(policy);
    }
    return ortn;
}

/*
 * Convert a NULL-terminated array of CSSM_DATAs to a mallocd array of krb5_datas.
 */
static OSStatus pkiCertArrayToKrb5Data(
    CSSM_DATA   **cdAllCerts,
    unsigned    *num_all_certs,
    krb5_data   **all_certs)	
{
    krb5_data *allCerts = NULL;
    OSStatus ortn = noErr;
    unsigned numCerts;
    unsigned dex;
    
    assert(num_all_certs != NULL);
    assert(all_certs != NULL);
    *num_all_certs = 0;
    *all_certs = NULL;

    if(cdAllCerts == NULL) {
	return 0;
    }
    numCerts = pkiNssArraySize((const void **)cdAllCerts);
    if(numCerts == 0) {
	return 0;
    }
    allCerts = (krb5_data *)malloc(sizeof(krb5_data) * numCerts);
    if(allCerts == NULL) {
	return ENOMEM;
    }
    for(dex=0; dex<numCerts; dex++) {
	if(pkiCssmDataToKrb5Data(cdAllCerts[dex], &allCerts[dex])) {
	    ortn = ENOMEM;
	    goto errOut;
	}
    }
errOut:
    if(ortn) {
	if(allCerts) {
	    free(allCerts);
	}
    }
    else {
	*all_certs = allCerts;
	*num_all_certs = (unsigned)numCerts;
    }
    return ortn;
}

#pragma mark ----- Generalized parse ContentInfo ----

/*
 * Parse a ContentInfo as best we can. All return fields are optional.
 * If signer_cert_status is NULL on entry, NO signature or cert evaluation 
 * will be performed. 
 */
krb5_error_code pkinit_parse_content_info(
    const krb5_data     *content_info,
    pkinit_cert_db_t    cert_db,		// may be required for SignedData
    krb5_boolean	*is_signed,		// RETURNED
    krb5_boolean	*is_encrypted,		// RETURNED
    krb5_data		*raw_data,		// RETURNED
    PKI_ContentType     *inner_content_type,    // Returned, ContentType of
						//    EncapsulatedData or
						//    EncryptedContentInfo
    krb5_data		*signer_cert,		// RETURNED
    pki_cert_sig_status *signer_cert_status,    // RETURNED 
    unsigned		*num_all_certs,		// size of *all_certs RETURNED
    krb5_data		**all_certs)		// entire cert chain RETURNED
{
    SecArenaPoolRef arena = NULL;
    SecCmsMessageRef cmsMsg = NULL;
    SecCmsDecoderRef decoder;
    OSStatus ortn;
    Boolean b;
    int numContentInfos;
    CSSM_DATA_PTR odata;
    int dex;
    OSStatus osCertStatus;
    OSStatus *osCertStatusP = NULL;
    
    assert(content_info != NULL);

    if(signer_cert) {
	signer_cert->data = NULL;
	signer_cert->length = 0;
    }
    if(raw_data) {
	raw_data->data = NULL;
	raw_data->length = 0;
    }
    if(all_certs) {
	assert(num_all_certs != NULL);
	*all_certs = NULL;
	*num_all_certs = 0;
    }
    if(signer_cert_status) {
	*signer_cert_status = -1;
	osCertStatusP = &osCertStatus;
    }
    
    SecArenaPoolCreate(1024, &arena);
    ortn = SecCmsDecoderCreate(arena, NULL, NULL, NULL, NULL, NULL, NULL, &decoder);
    if(ortn) {
	pkiCssmErr("SecCmsDecoderCreate", ortn);
	return ortn;
    }
    /* subsequent errors to errOut: */
    
    ortn = SecCmsDecoderUpdate(decoder, content_info->data, content_info->length);
    if(ortn) {
	pkiCssmErr("SecCmsDecoderUpdate", ortn);
	goto errOut;
    }
    ortn = SecCmsDecoderFinish(decoder, &cmsMsg);
    if(ortn) {
	pkiCssmErr("SecCmsDecoderFinish", ortn);
	goto errOut;
    }
    
    if(is_signed) {
	b = SecCmsMessageIsSigned(cmsMsg);
	*is_signed = b ? TRUE : FALSE;
    }
    if(is_encrypted) {
	b = SecCmsMessageIsEncrypted(cmsMsg);
	*is_encrypted = b ? TRUE : FALSE;
    }
    
    numContentInfos = SecCmsMessageContentLevelCount(cmsMsg);
    if(numContentInfos == 0) {
	pkiDebug("pkinit_parse_content_info: no ContentInfos!\n");
	ortn = ASN1_BAD_FORMAT;
	goto errOut;
    }
    
    /*
     * Do we need to get signer info - either to evaluate the signature and
     * evaluate the cert chain, or to return cert-related fields?
     */
    if((signer_cert != NULL) || (signer_cert_status != NULL) || 
       (num_all_certs != NULL) || (all_certs != NULL)) {
	b = TRUE;
    }
    else {
	b = FALSE;
    }
    if(b) {
	bool gotOneSignedData = false;
	for(dex=0; dex<numContentInfos; dex++) {
	    SecCmsContentInfoRef ci = SecCmsMessageContentLevel(cmsMsg, dex);
	    SECOidTag tag = SecCmsContentInfoGetContentTypeTag(ci);
	    switch(tag) {
		case SEC_OID_PKCS7_SIGNED_DATA:
		{
		    /* get signer cert info and status */
		    SecCmsSignedDataRef sd = 
			    (SecCmsSignedDataRef) SecCmsContentInfoGetContent(ci);
		    SecCertificateRef certRef = NULL;
		    SecCertificateRef *certRefP = NULL;
		    CSSM_DATA certData;
		    CSSM_DATA **cdAllCerts = NULL;
		    CSSM_DATA ***cdAllCertsP = NULL;
		    
		    if(signer_cert) {
			/* optional */
			certRefP = &certRef;
		    }
		    if(all_certs) {
			/* optional */
			cdAllCertsP = &cdAllCerts;
		    }
		    if(gotOneSignedData) {
			pkiDebug("pkinit_parse_content_info: Multiple SignedDatas!\n");
			ortn = ASN1_BAD_FORMAT;
			goto errOut;
		    }
		    
		    ortn = pkiParseSignedData(sd, cert_db, osCertStatusP, certRefP,
			cdAllCertsP);
		    if(ortn) {
			goto errOut;
		    }   
		    if(certRef) {
			ortn = SecCertificateGetData(certRef, &certData);
			if(ortn) {
			    pkiCssmErr("SecCertificateGetData", ortn);
			    goto errOut;
			}
			pkiDataToKrb5Data(certData.Data, certData.Length, signer_cert);
		    }
		    if(cdAllCerts) {
			ortn = pkiCertArrayToKrb5Data(cdAllCerts, num_all_certs,
			    all_certs);
			if(ortn) {
			    goto errOut;
			}
		    }
		    if(signer_cert_status) {
			*signer_cert_status = pkiCertSigStatus(osCertStatus);
		    }
		    gotOneSignedData = true;
		    break;
		}
		/* nothing interesting for these */
		case SEC_OID_PKCS7_DATA:
		case SEC_OID_PKCS7_ENVELOPED_DATA:
		case SEC_OID_PKCS7_ENCRYPTED_DATA:
		/* all others - right? */
		default:
			break;
	    }
	}
    }
    if(raw_data != NULL) {
	odata = SecCmsMessageGetContent(cmsMsg);
	if(odata == NULL) {
	    pkiDebug("***pkinit_parse_content_info: No inner content available\n");
	}
	else {
	    pkiDataToKrb5Data(odata->Data, odata->Length, raw_data);
	}
    }
    
errOut:
    if(arena) {
	SecArenaPoolFree(arena, false);
    }
    if(cmsMsg) {
	SecCmsMessageDestroy(cmsMsg);
    }
    return ortn;
}
