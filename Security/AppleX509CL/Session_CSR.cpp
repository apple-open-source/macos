/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


//
// Session_CSP.cpp - CSR-related session functions.
//

#include "AppleX509CLSession.h"
#include "DecodedCert.h"
#include "SnaccUtils.h"
#include "cldebugging.h"
#include "CSPAttacher.h"
#include "CertBuilder.h"
#include <Security/oidscert.h>
#include <Security/cssmapple.h>
#include <Security/cssmerrno.h>
#include <Security/cdsaUtils.h>
#include <Security/pkcs10.h>

/* 
 * Generate a DER-encoded CSR.
 */
void AppleX509CLSession::generateCsr(
	CSSM_CC_HANDLE 		CCHandle,
	const CSSM_APPLE_CL_CSR_REQUEST *csrReq,
	CSSM_DATA_PTR		&csrPtr)
{
	/*
	 * We use the full CertificationRequest here; we encode the 
	 * CertificationRequestInfo component separately to calculate
	 * its signature, then we encode the whole CertificationRequest
	 * after dropping in the signature and SignatureAlgorithmIdentifier.
	 *
	 * CertificationRequestInfo, CertificationRequest from pkcs10 
	 */ 
	CertificationRequest certReq;
	CertificationRequestInfo *reqInfo = new CertificationRequestInfo;
	certReq.certificationRequestInfo = reqInfo;
	
	/* 
	 * Step 1: convert CSSM_APPLE_CL_CSR_REQUEST to CertificationRequestInfo.
	 */
	reqInfo->version.Set(0);
	
	/* subject Name */
	NameBuilder *subject = new NameBuilder;
	reqInfo->subject = subject;
	subject->addX509Name(csrReq->subjectNameX509);
	
	/* SubjectPublicKeyInfo, AlgorithmIdentifier from sm_x509af */
	SubjectPublicKeyInfo *snaccKeyInfo = new SubjectPublicKeyInfo;
	reqInfo->subjectPublicKeyInfo = snaccKeyInfo;
	AlgorithmIdentifier *snaccAlgId = new AlgorithmIdentifier;
	snaccKeyInfo->algorithm = snaccAlgId;
	CL_cssmAlgToSnaccOid(csrReq->subjectPublicKey->KeyHeader.AlgorithmId,
		snaccAlgId->algorithm);
	/* FIXME - for now assume NULL alg params */
	CL_nullAlgParams(*snaccAlgId);
	
	/* actual public key blob - AsnBits */
	snaccKeyInfo->subjectPublicKey.Set(reinterpret_cast<char *>
		(csrReq->subjectPublicKey->KeyData.Data), 
		 csrReq->subjectPublicKey->KeyData.Length * 8);

	/* attributes - see sm_x501if - we support one, CSSMOID_ChallengePassword,
 	 * as a printable string */
	if(csrReq->challengeString) {
		Attribute *attr = reqInfo->attributes.Append();
		/* attr->type is an OID */
		attr->type.Set(challengePassword_arc);
		/* one value, spec'd as AsnAny, we have to encode first. */
		PrintableString snaccStr(csrReq->challengeString);
		CssmAutoData encChallenge(*this);
		SC_encodeAsnObj(snaccStr, encChallenge, 
				strlen(csrReq->challengeString) + 32);
		/* AttributeValue is an AsnAny as far as SNACC is concerned */
		AttributeValue *av = attr->values.Append();
		CSM_Buffer *cbuf = new CSM_Buffer((char *)encChallenge.data(), 
			encChallenge.length());
		av->value = cbuf;
	}
	
	/*
	 * Step 2: DER-encode the CertificationRequestInfo.
	 */
	CssmAutoData encReqInfo(*this);
	SC_encodeAsnObj(*reqInfo, encReqInfo, 8 * 1024);	// totally wild guess
	
	/*
	 * Step 3: sign the encoded CertificationRequestInfo.
	 */
	CssmAutoData sig(*this);
	signData(CCHandle, encReqInfo, sig);
	 
	/*
	 * Step 4: finish up CertificationRequest - signatureAlgorithm, signature
	 */
	certReq.signatureAlgorithm = new SignatureAlgorithmIdentifier;
	certReq.signatureAlgorithm->algorithm.Set(reinterpret_cast<char *>(
		csrReq->signatureOid.Data), csrReq->signatureOid.Length);
	/* FIXME - for now assume NULL alg params */
	CL_nullAlgParams(*certReq.signatureAlgorithm);
	certReq.signature.Set((char *)sig.data(), sig.length() * 8);
	
	/* 
	 * Step 5: DER-encode the finished CertificationRequestSigned.
	 */
	CssmAutoData encCsr(*this);
	SC_encodeAsnObj(certReq, encCsr, 
		encReqInfo.length() + 			// size of the thing we signed
		sig.length() +					// size of signature
		100);							// sigAlgId plus encoding overhead
		
	/* TBD - enc64 the result, when we have this much working */
	csrPtr = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA));
	csrPtr->Data = (uint8 *)malloc(encCsr.length());
	csrPtr->Length = encCsr.length();
	memmove(csrPtr->Data, encCsr.data(), encCsr.length());
}

/*
 * Verify CSR with its own public key. 
 */
void AppleX509CLSession::verifyCsr(
	const CSSM_DATA		*csrPtr)
{
	/*
	 * 1. Extract the public key from the CSR. We do this by decoding
	 *    the whole thing and getting a CSSM_KEY from the 
	 *    SubjectPublicKeyInfo.
	 */
	CertificationRequest certReq;
	const CssmData &csrEnc = CssmData::overlay(*csrPtr);
	SC_decodeAsnObj(csrEnc, certReq);
	CertificationRequestInfo *certReqInfo = certReq.certificationRequestInfo;
	if(certReqInfo == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_DATA);
	}
	CSSM_KEY_PTR cssmKey = 	CL_extractCSSMKey(*certReqInfo->subjectPublicKeyInfo, 
		*this,		// alloc
		NULL);		// no DecodedCert

	/*
	 * 2. Obtain signature algorithm and parameters. 
	 */
	SignatureAlgorithmIdentifier *snaccAlgId = certReq.signatureAlgorithm;
	if(snaccAlgId == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_DATA);
	}
	CSSM_ALGORITHMS vfyAlg = CL_snaccOidToCssmAlg(snaccAlgId->algorithm);
			
	/*
	 * 3. Extract the raw bits to be verified and the signature. We 
	 *    decode the CSR as a CertificationRequestSigned for this, which 
	 *    avoids the decode of the CertificationRequestInfo.
	 */
	CertificationRequestSigned certReqSigned;
	SC_decodeAsnObj(csrEnc, certReqSigned);

	CSM_Buffer	*cbuf = certReqSigned.certificationRequestInfo.value;
	char 		*cbufData = const_cast<char *>(cbuf->Access());
	CssmData	toVerify(cbufData, cbuf->Length());
	AsnBits		sigBits = certReqSigned.signature;
	size_t		sigBytes = (sigBits.BitLen() + 7) / 8;
	CssmData	sig(const_cast<char *>(sigBits.BitOcts()), sigBytes);
	
	/*
	 * 4. Attach to CSP, cook up signature context, verify signature.
	 */
	CSSM_CSP_HANDLE cspHand = getGlobalCspHand(true);
	CSSM_RETURN crtn;
	CSSM_CC_HANDLE ccHand;
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		vfyAlg,
		NULL,			// Access Creds
		cssmKey,
		&ccHand);
	if(crtn) {
		CssmError::throwMe(crtn);
	}
	verifyData(ccHand, toVerify, sig);
	CL_freeCSSMKey(cssmKey, *this);
}
