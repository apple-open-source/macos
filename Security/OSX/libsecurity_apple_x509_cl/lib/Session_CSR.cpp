/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
#include "clNameUtils.h"
#include "clNssUtils.h"
#include "cldebugging.h"
#include "CSPAttacher.h"
#include "clNssUtils.h"
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/cssmapple.h>
#include <Security/csrTemplates.h>
#include <Security/SecAsn1Templates.h>

/* 
 * Generate a DER-encoded CSR.
 */
void AppleX509CLSession::generateCsr(
	CSSM_CC_HANDLE 		CCHandle,
	const CSSM_APPLE_CL_CSR_REQUEST *csrReq,
	CSSM_DATA_PTR		&csrPtr)
{
	/*
	 * We use the full NSSCertRequest here; we encode the 
	 * NSSCertRequestInfo component separately to calculate
	 * its signature, then we encode the whole NSSCertRequest
	 * after dropping in the signature and SignatureAlgorithmIdentifier.
	 */ 
	NSSCertRequest certReq;
	NSSCertRequestInfo &reqInfo = certReq.reqInfo;
	PRErrorCode prtn;

	memset(&certReq, 0, sizeof(certReq));
	
	/* 
	 * Step 1: convert CSSM_APPLE_CL_CSR_REQUEST to CertificationRequestInfo.
	 * All allocs via local arena pool.
	 */
	SecNssCoder coder;
	ArenaAllocator alloc(coder);
	clIntToData(0, reqInfo.version, alloc);
	
	/* subject Name, required  */
	if(csrReq->subjectNameX509 == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_POINTER);
	}
	CL_cssmNameToNss(*csrReq->subjectNameX509, reqInfo.subject, coder);
	
	/* key --> CSSM_X509_SUBJECT_PUBLIC_KEY_INFO */
	CL_CSSMKeyToSubjPubKeyInfoNSS(*csrReq->subjectPublicKey, 
		reqInfo.subjectPublicKeyInfo, coder);

	/* attributes - see sm_x501if - we support one, CSSMOID_ChallengePassword,
 	 * as a printable string */
	if(csrReq->challengeString) {
		/* alloc a NULL_terminated array of NSS_Attribute pointers */
		reqInfo.attributes = (NSS_Attribute **)coder.malloc(2 * sizeof(NSS_Attribute *));
		reqInfo.attributes[1] = NULL;
		
		/* alloc one NSS_Attribute */
		reqInfo.attributes[0] = (NSS_Attribute *)coder.malloc(sizeof(NSS_Attribute));
		NSS_Attribute *attr = reqInfo.attributes[0];
		memset(attr, 0, sizeof(NSS_Attribute));
		
		 /* NULL_terminated array of attrValues */
		attr->attrValue = (CSSM_DATA **)coder.malloc(2 * sizeof(CSSM_DATA *));
		attr->attrValue[1] = NULL;
		
		/* one value - we're almost there */
		attr->attrValue[0] = (CSSM_DATA *)coder.malloc(sizeof(CSSM_DATA));
		
		/* attrType is an OID, temp, use static OID */
		attr->attrType = CSSMOID_ChallengePassword;

		/* one value, spec'd as AsnAny, we have to encode first. */		
		CSSM_DATA strData;
		strData.Data = (uint8 *)csrReq->challengeString;
		strData.Length = strlen(csrReq->challengeString);
		prtn = coder.encodeItem(&strData, kSecAsn1PrintableStringTemplate,
			*attr->attrValue[0]);
		if(prtn) {
			clErrorLog("generateCsr: error encoding challengeString\n");
			CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
		}
	}
	
	/*
	 * Step 2: DER-encode the NSSCertRequestInfo prior to signing.
	 */
	CSSM_DATA encReqInfo;
	prtn = coder.encodeItem(&reqInfo, kSecAsn1CertRequestInfoTemplate, encReqInfo);
	if(prtn) {
		clErrorLog("generateCsr: error encoding CertRequestInfo\n");
		CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
	}
	
	/*
	 * Step 3: sign the encoded NSSCertRequestInfo.
	 */
	CssmAutoData sig(*this);
	CssmData &infoData = CssmData::overlay(encReqInfo);
	signData(CCHandle, infoData, sig);
	 
	/*
	 * Step 4: finish up NSSCertRequest - signatureAlgorithm, signature
	 */
	certReq.signatureAlgorithm.algorithm = csrReq->signatureOid;
	/* FIXME - for now assume NULL alg params */
	CL_nullAlgParams(certReq.signatureAlgorithm);
	certReq.signature.Data = (uint8 *)sig.data();
	certReq.signature.Length = sig.length() * 8;
	
	/* 
	 * Step 5: DER-encode the finished NSSCertRequest into app space.
	 */
	CssmAutoData encCsr(*this);
	prtn = SecNssEncodeItemOdata(&certReq, kSecAsn1CertRequestTemplate, encCsr);
	if(prtn) {
		clErrorLog("generateCsr: error encoding CertRequestInfo\n");
		CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
	}
	
	/* TBD - enc64 the result, when we have this much working */
	csrPtr = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA));
	csrPtr->Data = (uint8 *)encCsr.data();
	csrPtr->Length = encCsr.length();
	encCsr.release();
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
	NSSCertRequest certReq;
	SecNssCoder coder;
	PRErrorCode prtn;
	
	memset(&certReq, 0, sizeof(certReq));
	prtn = coder.decodeItem(*csrPtr, kSecAsn1CertRequestTemplate, &certReq);
	if(prtn) {
		CssmError::throwMe(CSSMERR_CL_INVALID_DATA);
	}
	
	NSSCertRequestInfo &reqInfo = certReq.reqInfo;
	CSSM_KEY_PTR cssmKey = CL_extractCSSMKeyNSS(reqInfo.subjectPublicKeyInfo, 
		*this,		// alloc
		NULL);		// no DecodedCert

	/*
	 * 2. Obtain signature algorithm and parameters. 
	 */
	CSSM_X509_ALGORITHM_IDENTIFIER sigAlgId = certReq.signatureAlgorithm;
	CSSM_ALGORITHMS vfyAlg = CL_oidToAlg(sigAlgId.algorithm);
			
	/* 
	 * Handle CSSMOID_ECDSA_WithSpecified, which requires additional
	 * decode to get the digest algorithm.
	 */
	if(vfyAlg == CSSM_ALGID_ECDSA_SPECIFIED) {
		vfyAlg = CL_nssDecodeECDSASigAlgParams(sigAlgId.parameters, coder);
	}
	
	/*
	 * 3. Extract the raw bits to be verified and the signature. We 
	 *    decode the CSR as a CertificationRequestSigned for this, which 
	 *    avoids the decode of the CertificationRequestInfo.
	 */
	NSS_SignedCertRequest certReqSigned;
	memset(&certReqSigned, 0, sizeof(certReqSigned));
	prtn = coder.decodeItem(*csrPtr, kSecAsn1SignedCertRequestTemplate, &certReqSigned);
	if(prtn) {
		CssmError::throwMe(CSSMERR_CL_INVALID_DATA);
	}

	CSSM_DATA sigBytes = certReqSigned.signature;
	sigBytes.Length = (sigBytes.Length + 7 ) / 8;
	CssmData &sigCdata = CssmData::overlay(sigBytes);
	CssmData &toVerify = CssmData::overlay(certReqSigned.certRequestBlob);
	
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
	verifyData(ccHand, toVerify, sigCdata);
	CL_freeCSSMKey(cssmKey, *this);
}

