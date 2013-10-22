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
 * ocspRequest.cpp - OCSP Request class
 */
 
#include "ocspRequest.h"
#include "certGroupUtils.h"
#include "tpdebugging.h"
#include <security_ocspd/ocspResponse.h>
#include <security_ocspd/ocspExtensions.h>
#include <security_ocspd/ocspdUtils.h>
#include <assert.h>
#include <string.h>
#include <Security/oidsalg.h>
#include <Security/oidscert.h>
#include <Security/ocspTemplates.h>
#include <security_utilities/devrandom.h>
#include <CommonCrypto/CommonDigest.h>
#include <security_cdsa_utilities/cssmerrors.h>

/* preencoded DER NULL */
static uint8 nullParam[2] = {5, 0};

/* size of nonce we generate, in bytes */
#define OCSP_NONCE_SIZE		8

/*
 * The only constructor. Subject and issuer must remain valid for the 
 * lifetime of this object (they are not refcounted). 
 */
OCSPRequest::OCSPRequest(
	TPCertInfo		&subject,
	TPCertInfo		&issuer,
	bool			genNonce)
		: mCoder(NULL),
		  mSubject(subject), 
		  mIssuer(issuer),
		  mGenNonce(genNonce),
		  mCertID(NULL)
{
	SecAsn1CoderCreate(&mCoder);
	mNonce.Data = NULL;
	mNonce.Length = 0;
	mEncoded.Data = NULL;
	mEncoded.Length = 0;
}

OCSPRequest::~OCSPRequest()
{
	delete mCertID;
	if(mCoder) {
		SecAsn1CoderRelease(mCoder);
	}
}

const CSSM_DATA *OCSPRequest::encode()
{
	/* fields obtained from issuer */
	CSSM_DATA_PTR	issuerName;
	CSSM_DATA_PTR	issuerKey;
	CSSM_KEY_PTR	issuerPubKey;
	/* from subject */
	CSSM_DATA_PTR	subjectSerial=NULL;

	CSSM_RETURN					crtn;
	uint8						issuerNameHash[CC_SHA1_DIGEST_LENGTH];
	uint8						pubKeyHash[CC_SHA1_DIGEST_LENGTH];
	SecAsn1OCSPRequest			singleReq;
	SecAsn1OCSPCertID			&certId = singleReq.reqCert;
	SecAsn1OCSPSignedRequest	signedReq;
	SecAsn1OCSPRequest			*reqArray[2] = { &singleReq, NULL };
	SecAsn1OCSPTbsRequest		&tbs = signedReq.tbsRequest;
	uint8						version = 0;
	CSSM_DATA					vers = {1, &version};
	uint8						nonceBytes[OCSP_NONCE_SIZE];
	CSSM_DATA					nonceData = {OCSP_NONCE_SIZE, nonceBytes};
	OCSPNonce					*nonce = NULL;
	NSS_CertExtension			*extenArray[2] = {NULL, NULL};
	
	if(mEncoded.Data) {
		/* already done */
		return &mEncoded;
	}

	/* 
	 * One single request, no extensions
	 */
	memset(&singleReq, 0, sizeof(singleReq));
	
	/* algId refers to the hash we'll perform in issuer name and key */
	certId.algId.algorithm = CSSMOID_SHA1;
	certId.algId.parameters.Data = nullParam;
	certId.algId.parameters.Length = sizeof(nullParam);

	/* gather fields from two certs */
	crtn = mSubject.fetchField(&CSSMOID_X509V1IssuerNameStd, &issuerName);
	if(crtn) {
		CssmError::throwMe(crtn);
	}
	crtn = mIssuer.fetchField(&CSSMOID_CSSMKeyStruct, &issuerKey);
	if(crtn) {
		goto errOut;
	}
	crtn = mSubject.fetchField(&CSSMOID_X509V1SerialNumber, &subjectSerial);
	if(crtn) {
		goto errOut;
	}

	/* SHA1(issuerName) */
	ocspdSha1(issuerName->Data, (CC_LONG)issuerName->Length, issuerNameHash);

	/* SHA1(issuer public key) */
	if(issuerKey->Length != sizeof(CSSM_KEY)) {
		tpErrorLog("OCSPRequest::encode: malformed issuer key\n");
		crtn = CSSMERR_TP_INTERNAL_ERROR;
		goto errOut;
	}
	issuerPubKey = (CSSM_KEY_PTR)issuerKey->Data;
	ocspdSha1(issuerPubKey->KeyData.Data, (CC_LONG)issuerPubKey->KeyData.Length, pubKeyHash);
	
	/* build the CertID from those components */
	certId.issuerNameHash.Data = issuerNameHash;
	certId.issuerNameHash.Length = CC_SHA1_DIGEST_LENGTH;
	certId.issuerPubKeyHash.Data = pubKeyHash;
	certId.issuerPubKeyHash.Length = CC_SHA1_DIGEST_LENGTH;	
	certId.serialNumber = *subjectSerial;

	/* 
	 * Build top level request with one entry in requestList, no signature,
	 * one optional extension (a nonce)
	 */
	memset(&signedReq, 0, sizeof(signedReq));
	tbs.version = &vers;
	tbs.requestList = reqArray;

	/* one extension - the nonce */
	if(mGenNonce) {
		DevRandomGenerator drg;
		drg.random(nonceBytes, OCSP_NONCE_SIZE);
		nonce = new OCSPNonce(mCoder, false, nonceData);
		extenArray[0] = nonce->nssExt();
		tbs.requestExtensions = extenArray;
		SecAsn1AllocCopyItem(mCoder, &nonceData, &mNonce);
	}
	
	/* Encode */
	if(SecAsn1EncodeItem(mCoder, &signedReq, kSecAsn1OCSPSignedRequestTemplate, 
			&mEncoded)) {
		tpErrorLog("OCSPRequest::encode: error encoding OCSP req\n");
		crtn = CSSMERR_TP_INTERNAL_ERROR;
		goto errOut;
	}
	/* save a copy of the CertID */
	mCertID = new OCSPClientCertID(*issuerName, issuerPubKey->KeyData, *subjectSerial);
	
errOut:
	if(issuerName) {
		mIssuer.freeField(&CSSMOID_X509V1IssuerNameStd, issuerName);
	}
	if(issuerKey) {
		mIssuer.freeField(&CSSMOID_CSSMKeyStruct, issuerKey);
	}
	if(subjectSerial) {
		mSubject.freeField(&CSSMOID_X509V1SerialNumber, subjectSerial);
	}
	if(nonce) {
		delete nonce;
	}
	if(crtn) {
		CssmError::throwMe(crtn);
	}
	return &mEncoded;
}

const CSSM_DATA *OCSPRequest::nonce()
{
	/* not legal before encode() called */
	assert(mEncoded.Data != NULL);
	if(mNonce.Data) {
		return &mNonce;
	}
	else {
		return NULL;
	}
}

OCSPClientCertID *OCSPRequest::certID()
{
	encode();
	return mCertID;
}

