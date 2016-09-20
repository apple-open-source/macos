/*
 * Copyright (c) 2002,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * tpCredRequest.cpp - credential request functions SubmitCredRequest, 
 *                     RetrieveCredResult 
 *
 */
 
#include "AppleTPSession.h"
#include "certGroupUtils.h"
#include "tpdebugging.h"
#include "tpTime.h"
#include <Security/oidsalg.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/cssmapple.h>
#include <security_utilities/debugging.h>
#include <Security/cssmapple.h>
#include <assert.h>

#define tpCredDebug(args...)	secinfo("tpCred", ## args)

/*
 * Build up a CSSM_X509_NAME from an arbitrary list of name/OID pairs. 
 * We do one a/v pair per RDN. 
 */
CSSM_X509_NAME * AppleTPSession::buildX509Name(
	const CSSM_APPLE_TP_NAME_OID *nameArray,
	unsigned numNames)
{
	CSSM_X509_NAME *top = (CSSM_X509_NAME *)malloc(sizeof(CSSM_X509_NAME));
	top->numberOfRDNs = numNames;
	if(numNames == 0) {
		/* legal! */
		top->RelativeDistinguishedName = NULL;
		return top;
	}
	top->RelativeDistinguishedName = 
		(CSSM_X509_RDN_PTR)malloc(sizeof(CSSM_X509_RDN) * numNames);
	CSSM_X509_RDN_PTR rdn;
	const CSSM_APPLE_TP_NAME_OID *nameOid;
	unsigned nameDex;
	for(nameDex=0; nameDex<numNames; nameDex++) {
		rdn = &top->RelativeDistinguishedName[nameDex];
		nameOid = &nameArray[nameDex];
		rdn->numberOfPairs = 1;
		rdn->AttributeTypeAndValue = (CSSM_X509_TYPE_VALUE_PAIR_PTR)
			malloc(sizeof(CSSM_X509_TYPE_VALUE_PAIR));
		CSSM_X509_TYPE_VALUE_PAIR_PTR atvp = rdn->AttributeTypeAndValue;
		tpCopyCssmData(*this, nameOid->oid, &atvp->type);
		atvp->value.Length = strlen(nameOid->string);
		if(tpCompareOids(&CSSMOID_CountryName, nameOid->oid)) {
			/* 
			 * Country handled differently per RFC 3280 - must be printable,
			 * max of two characters in length
			 */
			if(atvp->value.Length > 2) {
				CssmError::throwMe(CSSMERR_TP_INVALID_DATA);
			}
			for(unsigned dex=0; dex<atvp->value.Length; dex++) {
				int c = nameOid->string[dex];
				if(!isprint(c) || (c == EOF)) {
					CssmError::throwMe(CSSMERR_TP_INVALID_DATA);
				}
			}
			atvp->valueType = BER_TAG_PRINTABLE_STRING;
		}
		/* other special cases per RFC 3280 */
		else if(tpCompareOids(&CSSMOID_DNQualifier, nameOid->oid)) {
			atvp->valueType = BER_TAG_PRINTABLE_STRING;
		}
		else if(tpCompareOids(&CSSMOID_SerialNumber, nameOid->oid)) {
			atvp->valueType = BER_TAG_PRINTABLE_STRING;
		}
		else if(tpCompareOids(&CSSMOID_EmailAddress, nameOid->oid)) {
			atvp->valueType = BER_TAG_IA5_STRING;
		}
		else {
			/* Default type */
			atvp->valueType = BER_TAG_PKIX_UTF8_STRING;
		}
		atvp->value.Data = (uint8 *)malloc(atvp->value.Length);
		memmove(atvp->value.Data, nameOid->string, atvp->value.Length);
	}
	return top;
}

/* free the CSSM_X509_NAME obtained from buildX509Name */
void AppleTPSession::freeX509Name(
	CSSM_X509_NAME *top)
{
	if(top == NULL) {
		return;
	}
	unsigned nameDex;
	CSSM_X509_RDN_PTR rdn;
	for(nameDex=0; nameDex<top->numberOfRDNs; nameDex++) {
		rdn = &top->RelativeDistinguishedName[nameDex];
		if(rdn->AttributeTypeAndValue) {
			for(unsigned aDex=0; aDex<rdn->numberOfPairs; aDex++) {
				CSSM_X509_TYPE_VALUE_PAIR_PTR atvp = 
					&rdn->AttributeTypeAndValue[aDex];
				free(atvp->type.Data);
				free(atvp->value.Data);
			}
			free(rdn->AttributeTypeAndValue);
		}
	}
	free(top->RelativeDistinguishedName);
	free(top);
}

/* Obtain a CSSM_X509_TIME representing "now" plus specified seconds */

/* 
 * Although RFC 2459, *the* spec for X509 certs, allows for not before/after
 * times to be expressed in ther generalized (4-digit year) or UTC (2-digit year
 * with implied century rollover), IE 5 on Mac will not accept the generalized
 * format.
 */
#define TP_FOUR_DIGIT_YEAR		0
#if		TP_FOUR_DIGIT_YEAR
#define TP_TIME_FORMAT 	TIME_GEN
#define TP_TIME_TAG		BER_TAG_GENERALIZED_TIME
#else
#define TP_TIME_FORMAT 	TIME_UTC
#define TP_TIME_TAG		BER_TAG_UTC_TIME
#endif	/* TP_FOUR_DIGIT_YEAR */

CSSM_X509_TIME * AppleTPSession::buildX509Time(
	unsigned secondsFromNow)
{
	CSSM_X509_TIME *xtime = (CSSM_X509_TIME *)malloc(sizeof(CSSM_X509_TIME));
	xtime->timeType = TP_TIME_TAG;
	char *ts = (char *)malloc(GENERALIZED_TIME_STRLEN + 1);
	{
		StLock<Mutex> _(tpTimeLock());
		timeAtNowPlus(secondsFromNow, TP_TIME_FORMAT, ts);
	}
	xtime->time.Data = (uint8 *)ts;
	xtime->time.Length = strlen(ts);
	return xtime;
}

/* Free CSSM_X509_TIME obtained in buildX509Time */
void AppleTPSession::freeX509Time(
	CSSM_X509_TIME	*xtime)
{
	if(xtime == NULL) {
		return;
	}
	free((char *)xtime->time.Data);
	free(xtime);
}

/*
 * Cook up a CSSM_DATA with specified integer, DER style (minimum number of
 * bytes, big-endian).
 */
static void intToDER(
	CSSM_INTPTR theInt,
	CSSM_DATA &DER_Data,
	Allocator &alloc)
{
	/*
 	 * Calculate length in bytes of encoded integer, minimum length of 1. 
	 */
	DER_Data.Length = 1;
	uintptr_t unsignedInt = (uintptr_t)theInt;
	while(unsignedInt > 0xff) {
		DER_Data.Length++;
		unsignedInt >>= 8;
	}

	/*
	 * DER encoding requires top bit to be zero, else it's a negative number. 
	 * Even though we're passing around integers as CSSM_INTPTR, they really are
 	 * always unsigned. 
	 * unsignedInt contains the m.s. byte of theInt in its l.s. byte. 
	 */
	if(unsignedInt & 0x80) {
		DER_Data.Length++;
	}
	
	DER_Data.Data = (uint8 *)alloc.malloc(DER_Data.Length);
	uint8 *dst = DER_Data.Data + DER_Data.Length - 1;
	unsignedInt = (uintptr_t)theInt;
	for(unsigned dex=0; dex<DER_Data.Length; dex++) {
		*dst-- = unsignedInt & 0xff;
		/* this shifts off to zero if we're adding a zero at the top */
		unsignedInt >>= 8;
	}
}

/* The reverse of the above. */
static CSSM_INTPTR DERToInt(
	const CSSM_DATA &DER_Data)
{
	CSSM_INTPTR rtn = 0;
	uint8 *bp = DER_Data.Data;
	for(unsigned dex=0; dex<DER_Data.Length; dex++) {
		rtn <<= 8;
		rtn |= *bp++;
	}
	return rtn;
}

/* Convert a reference key to a raw key. */
void AppleTPSession::refKeyToRaw(
	CSSM_CSP_HANDLE	cspHand,
	const CSSM_KEY	*refKey,	
	CSSM_KEY_PTR	rawKey)			// RETURNED
{
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	memset(rawKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			CSSM_ALGID_NONE,
			CSSM_ALGMODE_NONE,
			&creds,				// passPhrase
			NULL,				// wrapping key
			NULL,				// init vector
			CSSM_PADDING_NONE,	// Padding
			0,					// Params
			&ccHand);
	if(crtn) {
		tpCredDebug("AppleTPSession::refKeyToRaw: context err");
		CssmError::throwMe(crtn);
	}
	
	crtn = CSSM_WrapKey(ccHand,
		&creds,
		refKey,
		NULL,			// DescriptiveData
		rawKey);
	if(crtn != CSSM_OK) {
		tpCredDebug("AppleTPSession::refKeyToRaw: wrapKey err");
		CssmError::throwMe(crtn);
	}
	CSSM_DeleteContext(ccHand);
}


/*
 * Cook up an unsigned cert.
 * This is just a wrapper for CSSM_CL_CertCreateTemplate().
 */
void AppleTPSession::makeCertTemplate(
	/* required */
	CSSM_CL_HANDLE			clHand,
	CSSM_CSP_HANDLE			cspHand,		// for converting ref to raw key
	uint32					serialNumber,
	const CSSM_X509_NAME	*issuerName,	
	const CSSM_X509_NAME	*subjectName,
	const CSSM_X509_TIME	*notBefore,	
	const CSSM_X509_TIME	*notAfter,	
	const CSSM_KEY			*subjectPubKey,
	const CSSM_OID			&sigOid,		// e.g., CSSMOID_SHA1WithRSA
	/* optional */
	const CSSM_DATA			*subjectUniqueId,
	const CSSM_DATA			*issuerUniqueId,
	CSSM_X509_EXTENSION		*extensions,
	unsigned				numExtensions,
	CSSM_DATA_PTR			&rawCert)
{
	CSSM_FIELD		*certTemp;		
	unsigned		fieldDex = 0;			// index into certTemp
	CSSM_DATA		serialDER = {0, NULL};	// serial number, DER format
	CSSM_DATA		versionDER = {0, NULL};
	unsigned		extNum;
	CSSM_X509_ALGORITHM_IDENTIFIER algId;
	const CSSM_KEY	*actPubKey;
	CSSM_KEY		rawPubKey;
	CSSM_BOOL		freeRawKey = CSSM_FALSE;
	
	rawCert = NULL;
    
    /* 
     * Set Signature Algorithm OID and parameters
     */
	algId.algorithm = sigOid;
    
    /* NULL params - skip for ECDSA */
    CSSM_ALGORITHMS algorithmType = 0;
    cssmOidToAlg(&sigOid, &algorithmType);
    switch(algorithmType) {
        case CSSM_ALGID_SHA1WithECDSA:
        case CSSM_ALGID_SHA224WithECDSA:
        case CSSM_ALGID_SHA256WithECDSA:
        case CSSM_ALGID_SHA384WithECDSA:
        case CSSM_ALGID_SHA512WithECDSA:
        case CSSM_ALGID_ECDSA_SPECIFIED:
            algId.parameters.Data = NULL;
            algId.parameters.Length = 0;
            break;
        default:
            static const uint8 encNull[2] = { SEC_ASN1_NULL, 0 };
            CSSM_DATA encNullData;
            encNullData.Data = (uint8 *)encNull;
            encNullData.Length = 2;
            
            algId.parameters = encNullData;
            break;
    }

	
	/*
	 * Convert possible ref public key to raw format as required by CL.
	 */
	switch(subjectPubKey->KeyHeader.BlobType) {
		case CSSM_KEYBLOB_RAW:
			actPubKey = subjectPubKey;
			break;
		case CSSM_KEYBLOB_REFERENCE:
			refKeyToRaw(cspHand, subjectPubKey, &rawPubKey);
			actPubKey = &rawPubKey;
			freeRawKey = CSSM_TRUE;
			break;
		default:
			tpCredDebug("CSSM_CL_CertCreateTemplate: bad key blob type (%u)",
				(unsigned)subjectPubKey->KeyHeader.BlobType);
			CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
			

	/*
	 * version, always 2 (X509v3)
	 * serialNumber thru subjectPubKey
	 */
	unsigned numFields = 8 + numExtensions;
	if(subjectUniqueId) {
		numFields++;
	}
	if(issuerUniqueId) {
		numFields++;
	}

	certTemp = (CSSM_FIELD *)malloc(sizeof(CSSM_FIELD) * numFields);

	 
	/* version */
	intToDER(2, versionDER, *this);
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1Version;
	certTemp[fieldDex++].FieldValue = versionDER;
	
	/* serial number */
	intToDER(serialNumber, serialDER, *this);
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1SerialNumber;
	certTemp[fieldDex++].FieldValue = serialDER;

	/* subject and issuer name  */
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1IssuerNameCStruct;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)issuerName;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_NAME);
	
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1SubjectNameCStruct;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)subjectName;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_NAME);

	/* not before/after */
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1ValidityNotBefore;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)notBefore;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_TIME);

	certTemp[fieldDex].FieldOid = CSSMOID_X509V1ValidityNotAfter;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)notAfter;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_TIME);

	/* the subject key */
	certTemp[fieldDex].FieldOid = CSSMOID_CSSMKeyStruct;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)actPubKey;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_KEY);

	/* signature algorithm */
	certTemp[fieldDex].FieldOid = CSSMOID_X509V1SignatureAlgorithmTBS;
	certTemp[fieldDex].FieldValue.Data = (uint8 *)&algId;
	certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_ALGORITHM_IDENTIFIER);
	
	/* subject/issuer unique IDs */
	if(subjectUniqueId != 0) {
		certTemp[fieldDex].FieldOid = CSSMOID_X509V1CertificateSubjectUniqueId;
		certTemp[fieldDex++].FieldValue = *subjectUniqueId;
	}
	if(issuerUniqueId != 0) {
		certTemp[fieldDex].FieldOid = CSSMOID_X509V1CertificateIssuerUniqueId;
		certTemp[fieldDex++].FieldValue = *issuerUniqueId;
	}

	for(extNum=0; extNum<numExtensions; extNum++) {
		CSSM_X509_EXTENSION_PTR ext = &extensions[extNum];
		if(ext->format == CSSM_X509_DATAFORMAT_PARSED) {
			certTemp[fieldDex].FieldOid = ext->extnId;
		}
		else {
			certTemp[fieldDex].FieldOid = CSSMOID_X509V3CertificateExtensionCStruct;
		}
		certTemp[fieldDex].FieldValue.Data = (uint8 *)ext;
		certTemp[fieldDex++].FieldValue.Length = sizeof(CSSM_X509_EXTENSION);
	}
	assert(fieldDex == numFields);
	
	/*
	 * OK, here we go
	 */
	rawCert = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA));
	rawCert->Data = NULL;
	rawCert->Length = 0;
	CSSM_RETURN crtn = CSSM_CL_CertCreateTemplate(clHand,
		fieldDex,
		certTemp,
		rawCert);
	if(crtn) {
		tpCredDebug("CSSM_CL_CertCreateTemplate returned %ld", (long)crtn);
		free(rawCert->Data);
		free(rawCert);
		rawCert = NULL;
	}

	/* free the stuff we mallocd to get here */
	free(serialDER.Data);
	free(versionDER.Data);
	free(certTemp);
	if(freeRawKey) {
		tpFreeCssmData(*this, &rawPubKey.KeyData, CSSM_FALSE);
	}
	if(crtn) {
		CssmError::throwMe(crtn);
	}
}

/* given a cert and a ReferenceIdentifier, fill in ReferenceIdentifier and 
 * add it and the cert to tpCredMap. */
void AppleTPSession::addCertToMap(
	const CSSM_DATA		*cert,
	CSSM_DATA_PTR		refId)
{
	StLock<Mutex> _(tpCredMapLock);

	TpCredHandle hand = reinterpret_cast<TpCredHandle>(cert);
	intToDER(hand, *refId, *this);
	tpCredMap[hand] = cert;
}
	
/* given a ReferenceIdentifier, obtain associated cert and remove from the map */
CSSM_DATA_PTR AppleTPSession::getCertFromMap(
	const CSSM_DATA *refId)
{
	StLock<Mutex> _(tpCredMapLock);
	CSSM_DATA_PTR rtn = NULL;
	
	if((refId == NULL) || (refId->Data == NULL)) {
		return NULL;
	}
	TpCredHandle hand = DERToInt(*refId);
	credMap::iterator it = tpCredMap.find(hand);
	if(it == tpCredMap.end()) {
		return NULL;
	}
	rtn = const_cast<CSSM_DATA *>(it->second);
	tpCredMap.erase(hand);
	return rtn;
}

/*
 * SubmitCredRequest, CSR form.
 */
void AppleTPSession::SubmitCsrRequest(
	const CSSM_TP_REQUEST_SET &RequestInput,
	const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthContext,
	sint32 &EstimatedTime,						// RETURNED
	CssmData &ReferenceIdentifier)				// RETURNED
{
	CSSM_DATA_PTR	csrPtr = NULL;
	CSSM_CC_HANDLE 	sigHand = 0;
	CSSM_APPLE_CL_CSR_REQUEST csrReq;
	
	memset(&csrReq, 0, sizeof(csrReq));

	/* for now we're using the same struct for input as the the normal
	 * X509 cert request. */
	CSSM_APPLE_TP_CERT_REQUEST *certReq =
		(CSSM_APPLE_TP_CERT_REQUEST *)RequestInput.Requests;
	if((certReq->cspHand == 0) || 
	   (certReq->clHand == 0) ||
	   (certReq->certPublicKey == NULL) ||
	   (certReq->issuerPrivateKey == NULL) ||
	   (certReq->signatureOid.Data == NULL)) {
		CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
	
	/* convert ref public key to raw per CL requirements */
	const CSSM_KEY *subjectPubKey = certReq->certPublicKey;
	const CSSM_KEY *actPubKey = NULL;
	CSSM_BOOL freeRawKey = CSSM_FALSE;
	CSSM_KEY rawPubKey;
	
	switch(subjectPubKey->KeyHeader.BlobType) {
		case CSSM_KEYBLOB_RAW:
			actPubKey = subjectPubKey;
			break;
		case CSSM_KEYBLOB_REFERENCE:
			refKeyToRaw(certReq->cspHand, subjectPubKey, &rawPubKey);
			actPubKey = &rawPubKey;
			freeRawKey = CSSM_TRUE;
			break;
		default:
			tpCredDebug("SubmitCsrRequest: bad key blob type (%u)",
				(unsigned)subjectPubKey->KeyHeader.BlobType);
			CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}

	/* cook up a CL-passthrough-specific request */
	csrReq.subjectNameX509 	 = buildX509Name(certReq->subjectNames, 
											certReq->numSubjectNames);
	csrReq.signatureAlg 	 = certReq->signatureAlg;
	csrReq.signatureOid 	 = certReq->signatureOid;
	csrReq.cspHand 			 = certReq->cspHand;
	csrReq.subjectPublicKey  = actPubKey;
	csrReq.subjectPrivateKey = certReq->issuerPrivateKey;
	csrReq.challengeString   = certReq->challengeString;
	
	/* A crypto handle to pass to the CL */
	CSSM_RETURN crtn;
	crtn = CSSM_CSP_CreateSignatureContext(certReq->cspHand,
			certReq->signatureAlg,
			(CallerAuthContext ? CallerAuthContext->CallerCredentials : NULL),
			certReq->issuerPrivateKey,
			&sigHand);
	if(crtn) {
		tpCredDebug("CSSM_CSP_CreateSignatureContext returned %ld", (long)crtn);
		goto abort;
	}
	
	/* down to the CL to do the actual work */
	crtn = CSSM_CL_PassThrough(certReq->clHand,
		sigHand,
		CSSM_APPLEX509CL_OBTAIN_CSR,
		&csrReq,
		(void **)&csrPtr);
	if(crtn) {
		tpCredDebug("CSSM_CL_PassThrough returned %ld", (long)crtn);
		goto abort;
	}

	/* save it for retrieval by RetrieveCredResult */
	addCertToMap(csrPtr, &ReferenceIdentifier);
	EstimatedTime = 0;

abort:
	/* free local resources */
	if(csrReq.subjectNameX509) {
		freeX509Name(csrReq.subjectNameX509);
	}
	if(sigHand) {
		CSSM_DeleteContext(sigHand);
	}
	if(freeRawKey) {
		tpFreeCssmData(*this, &rawPubKey.KeyData, CSSM_FALSE);
	}
	if(crtn) {
		CssmError::throwMe(crtn);
	}
}

/*
 * Submit cred (cert) request. Currently the only form of request we
 * handle is the basis "sign this cert with key right now", with policy OI
 * CSSMOID_APPLE_TP_LOCAL_CERT_GEN.
 */
void AppleTPSession::SubmitCredRequest(
	const CSSM_TP_AUTHORITY_ID *PreferredAuthority,
	CSSM_TP_AUTHORITY_REQUEST_TYPE RequestType,
	const CSSM_TP_REQUEST_SET &RequestInput,
	const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthContext,
	sint32 &EstimatedTime,
	CssmData &ReferenceIdentifier)
{
	/* free all of these on return if non-NULL */
	CSSM_DATA_PTR certTemplate = NULL;
	CSSM_X509_TIME_PTR notBeforeX509 = NULL;
	CSSM_X509_TIME_PTR notAfterX509 = NULL;
	CSSM_X509_NAME_PTR subjectX509 = NULL;
	CSSM_X509_NAME_PTR issuerX509 = NULL;
	CSSM_X509_EXTENSION_PTR extens509 = NULL;
	CSSM_CC_HANDLE sigContext = 0;
	
	/* this gets saved on success */
	CSSM_DATA_PTR signedCert = NULL;
	
	/* validate rather limited set of input args */
	if(PreferredAuthority != NULL) {
		CssmError::throwMe(CSSMERR_TP_INVALID_AUTHORITY);
	}
	if(RequestType != CSSM_TP_AUTHORITY_REQUEST_CERTISSUE) {
		CssmError::throwMe(CSSMERR_TP_UNSUPPORTED_SERVICE);
	}
	if(CallerAuthContext == NULL) {
		CssmError::throwMe(CSSMERR_TP_INVALID_CALLERAUTH_CONTEXT_POINTER);
	}
	if((RequestInput.NumberOfRequests != 1) ||
	   (RequestInput.Requests == NULL)) {
		CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
	
	/* Apple-specific args */
	const CSSM_TP_POLICYINFO *tpPolicy = &CallerAuthContext->Policy;
	if((tpPolicy->NumberOfPolicyIds != 1) ||
	   (tpPolicy->PolicyIds == NULL)) {
		CssmError::throwMe(CSSMERR_TP_INVALID_CALLERAUTH_CONTEXT_POINTER);
	}
	if(tpCompareCssmData(&tpPolicy->PolicyIds->FieldOid,
		&CSSMOID_APPLE_TP_CSR_GEN)) {
		/* break out to CSR-specific code */
		SubmitCsrRequest(RequestInput, CallerAuthContext, EstimatedTime, ReferenceIdentifier);
		return;
	}
	else if(!tpCompareCssmData(&tpPolicy->PolicyIds->FieldOid,
		&CSSMOID_APPLE_TP_LOCAL_CERT_GEN)) {
		CssmError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
	}

	CSSM_APPLE_TP_CERT_REQUEST *certReq =
		(CSSM_APPLE_TP_CERT_REQUEST *)RequestInput.Requests;
	if((certReq->cspHand == 0) || 
	   (certReq->clHand == 0) ||
	   (certReq->certPublicKey == NULL) ||
	   (certReq->issuerPrivateKey == NULL)) {
		CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
	if((certReq->numExtensions != 0) & (certReq->extensions == NULL)) {
		CssmError::throwMe(CSSMERR_TP_INVALID_POINTER);
	}
	
	CSSM_RETURN ourRtn = CSSM_OK;
	
	try {
		/* convert caller's friendly names and times to CDSA style */
		subjectX509 = buildX509Name(certReq->subjectNames, certReq->numSubjectNames);
		if(certReq->issuerNames != NULL) {
			issuerX509 = buildX509Name(certReq->issuerNames, certReq->numIssuerNames);
		}
		else if(certReq->issuerNameX509) {
			/* caller obtained this from an existing signer's cert */
			issuerX509 = certReq->issuerNameX509;
		}
		else {
			/* self-signed */
			issuerX509 = subjectX509;
		}
		notBeforeX509 = buildX509Time(certReq->notBefore);
		notAfterX509 = buildX509Time(certReq->notAfter);
		
		if(certReq->numExtensions != 0) { 
			/* convert extensions array from CE_DataAndType to CSSM_X509_EXTENSION */
			extens509 = (CSSM_X509_EXTENSION *)malloc(sizeof(CSSM_X509_EXTENSION) * 
					certReq->numExtensions);
			memset(extens509, 0, sizeof(CSSM_X509_EXTENSION) * 
					certReq->numExtensions);
			for(unsigned dex=0; dex<certReq->numExtensions; dex++) {
				CSSM_X509_EXTENSION *extn = &extens509[dex];
				CE_DataAndType *cdt = &certReq->extensions[dex];
				void *parsedValue;
				CSSM_OID extnId;
				
				switch(cdt->type) {
					case DT_AuthorityKeyID:	
						parsedValue = &cdt->extension.authorityKeyID;
						extnId = CSSMOID_AuthorityKeyIdentifier;
						break;
					case DT_SubjectKeyID:		
						parsedValue = &cdt->extension.subjectKeyID;
						extnId = CSSMOID_SubjectKeyIdentifier;
						break;
					case DT_KeyUsage:				 
						parsedValue = &cdt->extension.keyUsage;
						extnId = CSSMOID_KeyUsage;
						break;
					case DT_SubjectAltName:			
						parsedValue = &cdt->extension.subjectAltName;
						extnId = CSSMOID_SubjectAltName;
						break;
					case DT_IssuerAltName:			
						parsedValue = &cdt->extension.issuerAltName;
						extnId = CSSMOID_IssuerAltName;
						break;
					case DT_ExtendedKeyUsage:		
						parsedValue = &cdt->extension.extendedKeyUsage;
						extnId = CSSMOID_ExtendedKeyUsage;
						break;
					case DT_BasicConstraints:		
						parsedValue = &cdt->extension.basicConstraints;
						extnId = CSSMOID_BasicConstraints;
						break;
					case DT_CertPolicies:			
						parsedValue = &cdt->extension.certPolicies;
						extnId = CSSMOID_CertificatePolicies;
						break;
					case DT_NetscapeCertType:		
						parsedValue = &cdt->extension.netscapeCertType;
						extnId = CSSMOID_NetscapeCertType;
						break;
					case DT_CrlDistributionPoints:		
						parsedValue = &cdt->extension.crlDistPoints;
						extnId = CSSMOID_CrlDistributionPoints;
						break;
					case DT_AuthorityInfoAccess:		
						parsedValue = &cdt->extension.authorityInfoAccess;
						extnId = CSSMOID_AuthorityInfoAccess;
						break;
					case DT_Other:		
					default:
						tpCredDebug("SubmitCredRequest: DT_Other not supported");
						CssmError::throwMe(CSSMERR_TP_UNKNOWN_TAG);
						// NOT REACHED
				}
				extn->extnId   			= extnId;
				extn->critical 			= cdt->critical;
				extn->format   			= CSSM_X509_DATAFORMAT_PARSED;
				extn->value.parsedValue 	= parsedValue;
				extn->BERvalue.Data = NULL;
				extn->BERvalue.Length = 0;
			}	/* for each extension */
		} 		/* converting extensions */
			
		/* cook up the unsigned template */
		makeCertTemplate(certReq->clHand,
			certReq->cspHand,
			certReq->serialNumber,
			issuerX509,
			subjectX509,
			notBeforeX509,
			notAfterX509,
			certReq->certPublicKey,
			certReq->signatureOid,
			NULL,				// subjectUniqueID, not used here (yet)
			NULL,				// issuerUniqueId
			extens509,
			certReq->numExtensions,
			certTemplate);
			
		/* create signature context */		
		ourRtn = CSSM_CSP_CreateSignatureContext(certReq->cspHand,
				certReq->signatureAlg,
				(CallerAuthContext ? CallerAuthContext->CallerCredentials : NULL),
				certReq->issuerPrivateKey,
				&sigContext);
		if(ourRtn) {
			tpCredDebug("CSSM_CSP_CreateSignatureContext returned %ld", (long)ourRtn);
			CssmError::throwMe(ourRtn);
		}
		
		signedCert = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA));
		signedCert->Data = NULL;
		signedCert->Length = 0;
		ourRtn = CSSM_CL_CertSign(certReq->clHand,
			sigContext,
			certTemplate,		// CertToBeSigned
			NULL,				// SignScope
			0,					// ScopeSize,
			signedCert);
		if(ourRtn) {
			tpCredDebug("CSSM_CL_CertSign returned %ld", (long)ourRtn);
			CssmError::throwMe(ourRtn);
		}
		
		/* save it for retrieval by RetrieveCredResult */
		addCertToMap(signedCert, &ReferenceIdentifier);
		EstimatedTime = 0;
	}
	catch (const CssmError &cerr) {
		tpCredDebug("SubmitCredRequest: CSSM error %ld", (long)cerr.error);
		ourRtn = cerr.error;
	}
	catch(...) {
		tpCredDebug("SubmitCredRequest: unknown exception");
		ourRtn = CSSMERR_TP_INTERNAL_ERROR;	// ??
	}
	
	/* free reources */
	tpFreeCssmData(*this, certTemplate, CSSM_TRUE);
	freeX509Name(subjectX509);
	if(certReq->issuerNames) {
		freeX509Name(issuerX509);
	}
	/* else same as subject */
	freeX509Time(notBeforeX509);
	freeX509Time(notAfterX509);
	if(extens509) {
		free(extens509);
	}
	if(sigContext != 0) {
		CSSM_DeleteContext(sigContext);
	}
	if(ourRtn) {
		CssmError::throwMe(ourRtn);
	}
}

void AppleTPSession::RetrieveCredResult(
	const CssmData &ReferenceIdentifier,
	const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthCredentials,
	sint32 &EstimatedTime,
	CSSM_BOOL &ConfirmationRequired,
	CSSM_TP_RESULT_SET_PTR &RetrieveOutput)
{
	CSSM_DATA *cert = getCertFromMap(&ReferenceIdentifier);
	
	if(cert == NULL) {
		tpCredDebug("RetrieveCredResult: refId not found");
		CssmError::throwMe(CSSMERR_TP_INVALID_IDENTIFIER);
	}
	
	/* CSSM_TP_RESULT_SET.Results points to a CSSM_ENCODED_CERT */
	CSSM_ENCODED_CERT *encCert = (CSSM_ENCODED_CERT *)malloc(sizeof(CSSM_ENCODED_CERT));
	encCert->CertType = CSSM_CERT_X_509v3;
	encCert->CertEncoding = CSSM_CERT_ENCODING_DER;
	
	/* 
	 * caller must free all three:
	 *   CSSM_TP_RESULT_SET_PTR RetrieveOutput
	 *   RetrieveOutput->Results (CSSM_ENCODED_CERT *encCert)
	 *   encCert->CertBlob.Data (the actual cert)
	 * We free:
	 * 	 cert 					-- mallocd in SubmitCredRequest
	 */
	encCert->CertBlob = *cert;
	RetrieveOutput = (CSSM_TP_RESULT_SET_PTR)malloc(
		sizeof(CSSM_TP_RESULT_SET));
	RetrieveOutput->Results = encCert;
	RetrieveOutput->NumberOfResults = 1;
	ConfirmationRequired = CSSM_FALSE;
	free(cert);	
	EstimatedTime = 0;
}
