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
 * ocspResponse.cpp - OCSP Response class
 */
#include "ocspResponse.h"
#include "ocspdUtils.h"
#include <Security/cssmapple.h>
#include <Security/oidscrl.h>
#include <Security/oidsalg.h>
#include "ocspdDebug.h"
#include <CommonCrypto/CommonDigest.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/SecAsn1Templates.h>

/* malloc & copy CSSM_DATA using std malloc */
static void allocCopyData(
	const CSSM_DATA &src,
	CSSM_DATA &dst)
{
	if(src.Length == 0) {
		dst.Data = NULL;
		dst.Length = 0;
		return;
	}
	dst.Data = (uint8 *)malloc(src.Length);
	memmove(dst.Data, src.Data, src.Length);
	dst.Length = src.Length;
}

/* std free() of a CSSM_DATA */
static void freeData(
	CSSM_DATA &d)
{
	if(d.Data) {
		free(d.Data);
		d.Data = NULL;
	}
	d.Length = 0;
}

#pragma mark ---- OCSPClientCertID ----

/*
 * Basic constructor given issuer's public key and name, and subject's
 * serial number.
 */
OCSPClientCertID::OCSPClientCertID(
	const CSSM_DATA			&issuerName,
	const CSSM_DATA			&issuerPubKey,
	const CSSM_DATA			&subjectSerial)
{
	mEncoded.Data = NULL;
	mEncoded.Length = 0;
	allocCopyData(issuerName, mIssuerName);
	allocCopyData(issuerPubKey, mIssuerPubKey);
	allocCopyData(subjectSerial, mSubjectSerial);
}
		
OCSPClientCertID::~OCSPClientCertID()
{
	freeData(mIssuerName);
	freeData(mIssuerPubKey);
	freeData(mSubjectSerial);
	freeData(mEncoded);
}
	
/* preencoded DER NULL */
static uint8 nullParam[2] = {5, 0};

/*
 * DER encode in specified coder's memory.
 */
const CSSM_DATA *OCSPClientCertID::encode()
{
	if(mEncoded.Data != NULL) {
		return &mEncoded;
	}
	
	SecAsn1OCSPCertID	certID;
	uint8				issuerNameHash[CC_SHA1_DIGEST_LENGTH];
	uint8				pubKeyHash[CC_SHA1_DIGEST_LENGTH];
	
	/* algId refers to the hash we'll perform in issuer name and key */
	certID.algId.algorithm = CSSMOID_SHA1;
	certID.algId.parameters.Data = nullParam;
	certID.algId.parameters.Length = sizeof(nullParam);

	/* SHA1(issuerName) */
	ocspdSha1(mIssuerName.Data, (CC_LONG)mIssuerName.Length, issuerNameHash);
	/* SHA1(issuer public key) */
	ocspdSha1(mIssuerPubKey.Data, (CC_LONG)mIssuerPubKey.Length, pubKeyHash);
	
	/* build the CertID from those components */
	certID.issuerNameHash.Data = issuerNameHash;
	certID.issuerNameHash.Length = CC_SHA1_DIGEST_LENGTH;
	certID.issuerPubKeyHash.Data = pubKeyHash;
	certID.issuerPubKeyHash.Length = CC_SHA1_DIGEST_LENGTH;	
	certID.serialNumber = mSubjectSerial;
	
	/* encode */
	SecAsn1CoderRef coder;
	SecAsn1CoderCreate(&coder);
	
	CSSM_DATA tmp = {0, NULL};
	SecAsn1EncodeItem(coder, &certID, kSecAsn1OCSPCertIDTemplate, &tmp);
	allocCopyData(tmp, mEncoded);
	SecAsn1CoderRelease(coder);
	return &mEncoded;
}
		
/*
 * Does this object refer to the same cert as specified SecAsn1OCSPCertID?
 * This is the main purpose of this class's existence; this function works 
 * even if specified SecAsn1OCSPCertID uses a different hash algorithm
 * than we do, since we keep copies of our basic components. 
 *
 * Returns true if compare successful.
 */
typedef void (*hashFcn)(const void *data, CC_LONG len, unsigned char *md);

bool OCSPClientCertID::compareToExist(
	const SecAsn1OCSPCertID	&exist)
{
	/* easy part */
	if(!ocspdCompareCssmData(&mSubjectSerial, &exist.serialNumber)) {
		return false;
	}

	hashFcn hf = NULL;
	const CSSM_OID *alg = &exist.algId.algorithm;
	uint8 digest[OCSPD_MAX_DIGEST_LEN];
	CSSM_DATA digestData = {0, digest};
	
	if(ocspdCompareCssmData(alg, &CSSMOID_SHA1)) {
		hf = ocspdSha1;
		digestData.Length = CC_SHA1_DIGEST_LENGTH;
	}
	else if(ocspdCompareCssmData(alg, &CSSMOID_MD5)) {
		hf = ocspdMD5;
		digestData.Length = CC_MD5_DIGEST_LENGTH;
	}
	else if(ocspdCompareCssmData(alg, &CSSMOID_MD4)) {
		hf = ocspdMD4;
		digestData.Length = CC_MD4_DIGEST_LENGTH;
	}
	/* an OID for SHA256? */
	else {
		return false;
	}
	
	/* generate digests using exist's hash algorithm */
	hf(mIssuerName.Data, (CC_LONG)mIssuerName.Length, digest);
	if(!ocspdCompareCssmData(&digestData, &exist.issuerNameHash)) {
		return false;
	}
	hf(mIssuerPubKey.Data, (CC_LONG)mIssuerPubKey.Length, digest);
	if(!ocspdCompareCssmData(&digestData, &exist.issuerPubKeyHash)) {
		return false;
	}
	
	return true;
}

bool OCSPClientCertID::compareToExist(
	const CSSM_DATA	&exist)
{
	SecAsn1CoderRef coder;
	SecAsn1OCSPCertID certID;
	bool brtn = false;
	
	SecAsn1CoderCreate(&coder);
	memset(&certID, 0, sizeof(certID));
	if(SecAsn1DecodeData(coder, &exist, kSecAsn1OCSPCertIDTemplate, &certID)) {
		goto errOut;
	}
	brtn = compareToExist(certID);
errOut:
	SecAsn1CoderRelease(coder);
	return brtn;
}

#pragma mark ---- OCSPSingleResponse ----

/*
 * Constructor, called by OCSPResponse.
 */
OCSPSingleResponse::OCSPSingleResponse(
	SecAsn1OCSPSingleResponse	*resp)
	  : mCertStatus(CS_NotParsed),
		mThisUpdate(NULL_TIME),
		mNextUpdate(NULL_TIME),
		mRevokedTime(NULL_TIME),
		mCrlReason(CrlReason_NONE),
		mExtensions(NULL)
{
	assert(resp != NULL);
	
	SecAsn1CoderCreate(&mCoder);
	if((resp->certStatus.Data == NULL) || (resp->certStatus.Length == 0)) {
		ocspdErrorLog("OCSPSingleResponse: bad certStatus\n");
		CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
	mCertStatus = (SecAsn1OCSPCertStatusTag)(resp->certStatus.Data[0] & SEC_ASN1_TAGNUM_MASK);
	if(mCertStatus == CS_Revoked) {
		/* decode further to get SecAsn1OCSPRevokedInfo */
		SecAsn1OCSPCertStatus certStatus;
		memset(&certStatus, 0, sizeof(certStatus));
		if(SecAsn1DecodeData(mCoder, &resp->certStatus, 
				kSecAsn1OCSPCertStatusRevokedTemplate, &certStatus)) {
			ocspdErrorLog("OCSPSingleResponse: err decoding certStatus\n");
			CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
		}
		SecAsn1OCSPRevokedInfo *revokedInfo = certStatus.revokedInfo;
		if(revokedInfo != NULL) {
			/* Treat this as optional even for CS_Revoked */
			mRevokedTime = genTimeToCFAbsTime(&revokedInfo->revocationTime);
			const CSSM_DATA *revReason = revokedInfo->revocationReason;
			if((revReason != NULL) &&
			   (revReason->Data != NULL) &&
			   (revReason->Length != 0)) {
			   mCrlReason = revReason->Data[0];
			}
		}
	}
	mThisUpdate = genTimeToCFAbsTime(&resp->thisUpdate);
	if(resp->nextUpdate != NULL) {
		mNextUpdate = genTimeToCFAbsTime(resp->nextUpdate);
	}
	mExtensions = new OCSPExtensions(resp->singleExtensions);
	ocspdDebug("OCSPSingleResponse: status %d reason %d", (int)mCertStatus, 
		(int)mCrlReason);
}

OCSPSingleResponse::~OCSPSingleResponse()
{
	delete mExtensions;
	SecAsn1CoderRelease(mCoder);
}

/*** Extensions-specific accessors ***/
#if 0 /* unused ? */
const CSSM_DATA *OCSPSingleResponse::*crlUrl()
{
	/* TBD */
	return NULL;
}
#endif

const CSSM_DATA *OCSPSingleResponse::crlNum()
{
	/* TBD */
	return NULL;

}

CFAbsoluteTime OCSPSingleResponse::crlTime()			/* may be NULL_TIME */
{
	/* TBD */
	return NULL_TIME;
}

/* archive cutoff */
CFAbsoluteTime OCSPSingleResponse::archiveCutoff()
{
	/* TBD */
	return NULL_TIME;
}

#pragma mark ---- OCSPResponse ----

OCSPResponse::OCSPResponse(
	const CSSM_DATA &resp,
	CFTimeInterval defaultTTL)		// default time-to-live in seconds
		: mLatestNextUpdate(NULL_TIME), 
		  mExpireTime(NULL_TIME),
		  mExtensions(NULL)
{
	SecAsn1CoderCreate(&mCoder);
	memset(&mTopResp, 0, sizeof(mTopResp));
	memset(&mBasicResponse, 0, sizeof(mBasicResponse));
	memset(&mResponseData, 0, sizeof(mResponseData));
	memset(&mResponderId, 0, sizeof(mResponderId));
	mResponderIdTag = (SecAsn1OCSPResponderIDTag)0;		// invalid
	mEncResponderName.Data = NULL;
	mEncResponderName.Length = 0;
	
	if(SecAsn1DecodeData(mCoder, &resp, kSecAsn1OCSPResponseTemplate, &mTopResp)) {
		ocspdErrorLog("OCSPResponse: decode failure at top level\n");
		CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
	
	/* remainder is valid only on RS_Success */
	if((mTopResp.responseStatus.Data == NULL) ||
	   (mTopResp.responseStatus.Length == 0)) {
		ocspdErrorLog("OCSPResponse: no responseStatus\n");
		CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
	if(mTopResp.responseStatus.Data[0] != RS_Success) {
		/* not a failure of our constructor; this object is now useful, but 
		 * only for this one byte of status info */
		return;
	}
	if(mTopResp.responseBytes == NULL) {
		/* I don't see how this can be legal on RS_Success */
		ocspdErrorLog("OCSPResponse: empty responseBytes\n");
		CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
	if(!ocspdCompareCssmData(&mTopResp.responseBytes->responseType,
			&CSSMOID_PKIX_OCSP_BASIC)) {
		ocspdErrorLog("OCSPResponse: unknown responseType\n");
		CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
	
	/* decode the SecAsn1OCSPBasicResponse */
	if(SecAsn1DecodeData(mCoder, &mTopResp.responseBytes->response,
			kSecAsn1OCSPBasicResponseTemplate, &mBasicResponse)) {
		ocspdErrorLog("OCSPResponse: decode failure at SecAsn1OCSPBasicResponse\n");
		CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
	
	/* signature and cert evaluation done externally */
	
	/* decode the SecAsn1OCSPResponseData */
	if(SecAsn1DecodeData(mCoder, &mBasicResponse.tbsResponseData,
			kSecAsn1OCSPResponseDataTemplate, &mResponseData)) {
		ocspdErrorLog("OCSPResponse: decode failure at SecAsn1OCSPResponseData\n");
		CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
	if(mResponseData.responderID.Data == NULL) {
		ocspdErrorLog("OCSPResponse: bad responderID\n");
		CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
		
	/* choice processing for ResponderID */
	mResponderIdTag = (SecAsn1OCSPResponderIDTag)
		(mResponseData.responderID.Data[0] & SEC_ASN1_TAGNUM_MASK);
	const SecAsn1Template *templ;
	switch(mResponderIdTag) {
		case RIT_Name: 
			templ = kSecAsn1OCSPResponderIDAsNameTemplate; 
			break;
		case RIT_Key: 
			templ = kSecAsn1OCSPResponderIDAsKeyTemplate; 
			break;
		default:
			ocspdErrorLog("OCSPResponse: bad responderID tag\n");
			CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
	if(SecAsn1DecodeData(mCoder, &mResponseData.responderID, templ, &mResponderId)) {
		ocspdErrorLog("OCSPResponse: decode failure at responderID\n");
		CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
	
	/* check temporal validity */
	if(!calculateValidity(defaultTTL)) {
		/* Whoops, abort */
		CssmError::throwMe(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
	}
	
	/* 
	 * Individual responses looked into when we're asked for a specific one
	 * via singleResponse()
	 */
	mExtensions = new OCSPExtensions(mResponseData.responseExtensions);
}

OCSPResponse::~OCSPResponse()
{
	delete mExtensions;
	SecAsn1CoderRelease(mCoder);
}

SecAsn1OCSPResponseStatus OCSPResponse::responseStatus()
{
	assert(mTopResp.responseStatus.Data != NULL);	/* else constructor should have failed */
	return (SecAsn1OCSPResponseStatus)(mTopResp.responseStatus.Data[0]);
}

const CSSM_DATA	*OCSPResponse::nonce()			/* NULL means not present */
{
	OCSPExtension *ext = mExtensions->findExtension(CSSMOID_PKIX_OCSP_NONCE);
	if(ext == NULL) {
		return NULL;
	}
	OCSPNonce *nonceExt = dynamic_cast<OCSPNonce *>(ext);
	return &(nonceExt->nonce());
}

CFAbsoluteTime OCSPResponse::producedAt()
{
	return genTimeToCFAbsTime(&mResponseData.producedAt);
}

uint32 OCSPResponse::numSignerCerts()
{
	return ocspdArraySize((const void **)mBasicResponse.certs);
}

const CSSM_DATA *OCSPResponse::signerCert(uint32 dex)
{
	uint32 numCerts = numSignerCerts();
	if(dex >= numCerts) {
		ocspdErrorLog("OCSPResponse::signerCert: numCerts overflow\n");
		CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
	}
	return mBasicResponse.certs[dex];
}

/* 
 * Obtain a OCSPSingleResponse for a given "smart" CertID. 
 */
OCSPSingleResponse *OCSPResponse::singleResponseFor(OCSPClientCertID &matchCertID)
{
	unsigned numResponses = ocspdArraySize((const void **)mResponseData.responses);
	for(unsigned dex=0; dex<numResponses; dex++) {
		SecAsn1OCSPSingleResponse *resp = mResponseData.responses[dex];
		SecAsn1OCSPCertID &certID = resp->certID;
		if(matchCertID.compareToExist(certID)) {
			try {
				OCSPSingleResponse *singleResp = new OCSPSingleResponse(resp);
				return singleResp;
			}
			catch(...) {
				/* try to find another... */
				continue;
			}
		}
	}
	ocspdDebug("OCSPResponse::singleResponse: certID not found");
	return NULL;
}

/*
 * If responderID is of form RIT_Name, return the encoded version of the 
 * NSS_Name (for comparison with issuer's subjectName). Evaluated lazily,
 * once, in mCoder space.
 */
const CSSM_DATA *OCSPResponse::encResponderName()
{
	if(mResponderIdTag != RIT_Name) {
		assert(0);
		return NULL;
	}
	if(mEncResponderName.Data != NULL) {
		return &mEncResponderName;
	}
	if(SecAsn1EncodeItem(mCoder, &mResponderId.byName, kSecAsn1AnyTemplate,
			&mEncResponderName)) {
		ocspdDebug("OCSPResponse::encResponderName: error encoding ResponderId!");
		return NULL;
	}
	return &mEncResponderName;
}

/* 
 * Obtain a OCSPSingleResponse for a given raw encoded CertID. 
 */
OCSPSingleResponse *OCSPResponse::singleResponseFor(const CSSM_DATA &matchCertID)
{
	unsigned numResponses = ocspdArraySize((const void **)mResponseData.responses);
	for(unsigned dex=0; dex<numResponses; dex++) {
		SecAsn1OCSPSingleResponse *resp = mResponseData.responses[dex];
		CSSM_DATA certID = {0, NULL};
		if(SecAsn1EncodeItem(mCoder, &resp->certID, kSecAsn1OCSPCertIDTemplate,
				&certID)) {
			ocspdDebug("OCSPResponse::singleResponse: error encoding certID!");
			return NULL;
		}
		if(!ocspdCompareCssmData(&matchCertID, &certID)) {
			/* try to find another */
			continue;
		}
		try {
			OCSPSingleResponse *singleResp = new OCSPSingleResponse(resp);
			return singleResp;
		}
		catch(...) {
			/* try to find another... */
			continue;
		}
	}
	ocspdDebug("OCSPResponse::singleResponse: certID not found");
	return NULL;

}

/* 
 * Calculate temporal validity; set mLatestNextUpdate and mExpireTime. Only
 * called from constructor. Returns true if valid, else returns false. 
 */
bool OCSPResponse::calculateValidity(CFTimeInterval defaultTTL)
{
	mLatestNextUpdate = NULL_TIME;
	CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
	
	unsigned numResponses = ocspdArraySize((const void **)mResponseData.responses);
	for(unsigned dex=0; dex<numResponses; dex++) {
		SecAsn1OCSPSingleResponse *resp = mResponseData.responses[dex];
		
		/* 
		 * First off, a thisUpdate later than 'now' invalidates the whole response. 
		 */
		CFAbsoluteTime thisUpdate = genTimeToCFAbsTime(&resp->thisUpdate);
		if(thisUpdate > now) {
			ocspdErrorLog("OCSPResponse::calculateValidity: thisUpdate not passed\n");
			return false;
		}
		
		/* 
		 * Accumulate latest nextUpdate
		 */
		if(resp->nextUpdate != NULL) {
			CFAbsoluteTime nextUpdate = genTimeToCFAbsTime(resp->nextUpdate);
			if(nextUpdate > mLatestNextUpdate) {
				mLatestNextUpdate = nextUpdate;
			}
		}
	}
	
	CFAbsoluteTime defaultExpire = now + defaultTTL;
	if(mLatestNextUpdate == NULL_TIME) {
		/* absolute expire time = current time plus default TTL */
		mExpireTime = defaultExpire;
	}
	else if(defaultExpire < mLatestNextUpdate) {
		/* caller more stringent than response */
		mExpireTime = defaultExpire;
	}
	else {
		/* response more stringent than caller */
		if(mLatestNextUpdate < now) {
			ocspdErrorLog("OCSPResponse::calculateValidity: now > mLatestNextUpdate\n");
			return false;
		}
		mExpireTime = mLatestNextUpdate;
	}
	return true;
}

