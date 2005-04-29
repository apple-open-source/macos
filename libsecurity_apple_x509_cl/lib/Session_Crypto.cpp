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


/* 
 * Session_Crypto.cpp: CL session functions: sign, verify, CSSM_KEY extraction.
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 */

#include "AppleX509CLSession.h"
#include "DecodedCert.h"
#include "cldebugging.h"
#include "CSPAttacher.h"
#include "clNssUtils.h"
#include <Security/keyTemplates.h>
#include <security_asn1/nssUtils.h>
#include <Security/oidscert.h>
#include <Security/cssmapple.h>

/*
 * Given a DER-encoded cert, obtain a fully usable CSSM_KEY representing
 * the cert's public key. 
 */
void
AppleX509CLSession::CertGetKeyInfo(
	const CssmData &Cert,
	CSSM_KEY_PTR &Key)
{
	DecodedCert decodedCert(*this, Cert);
	Key = decodedCert.extractCSSMKey(*this);
}

/*
 * Given a DER-encoded cert and a fully specified crypto context, verify 
 * cert's TBS and signature. 
 */
void
AppleX509CLSession::CertVerifyWithKey(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &CertToBeVerified)
{
	CssmAutoData tbs(*this);
	CssmAutoData algId(*this);
	CssmAutoData sig(*this);
	CL_certCrlDecodeComponents(CertToBeVerified, tbs, algId, sig);
	verifyData(CCHandle, tbs, sig);
}

/*
 * Verify a DER-encoded cert, obtaining crypto context from either
 * caller-specified context or by inference from SignerCert.
 */
void
AppleX509CLSession::CertVerify(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &CertToBeVerified,
	const CssmData *SignerCert,
	const CSSM_FIELD *VerifyScope,
	uint32 ScopeSize)
{
	if((VerifyScope != NULL) || (ScopeSize != 0)) {
		CssmError::throwMe(CSSMERR_CL_SCOPE_NOT_SUPPORTED);
	}
	if((CCHandle == CSSM_INVALID_HANDLE) && (SignerCert == NULL)) {
		/* need one or the other */
		CssmError::throwMe(CSSMERR_CL_INVALID_CONTEXT_HANDLE);
	}
	
	/* get top-level components  */
	CssmAutoData tbs(*this);		// in DER format
	CssmAutoData algId(*this);		// in DER format
	CssmAutoData sig(*this);		// in DER format
	CL_certCrlDecodeComponents(CertToBeVerified, tbs, algId, sig);

	/* these must be explicitly freed upon exit */
	CSSM_KEY_PTR signerPubKey = NULL;
	CSSM_CONTEXT_PTR context = NULL;
	CSSM_CSP_HANDLE cspHand = CSSM_INVALID_HANDLE;
	CSSM_CC_HANDLE ourCcHand = CSSM_INVALID_HANDLE;
	
	/* SignerCert optional; if present, obtain its subject key */
	if(SignerCert != NULL) {
		CertGetKeyInfo(*SignerCert, signerPubKey);
	}
	
	/* signerPubKey must be explicitly freed in any case */
	try {
		if(CCHandle != CSSM_INVALID_HANDLE) {
			/*
			 * We'll use this CCHandle for the sig verify, but 
			 * make sure it matches possible incoming SignerCert parameters
			 */
			if(SignerCert != NULL) {
				CSSM_RETURN crtn;
				
				/* extract signer's public key as a CSSM_KEY from context */
				crtn = CSSM_GetContext(CCHandle, &context);
				if(crtn) {
					CssmError::throwMe(CSSMERR_CL_INVALID_CONTEXT_HANDLE);
				}
				CSSM_CONTEXT_ATTRIBUTE_PTR attr;
				crtn = CSSM_GetContextAttribute(context,
					CSSM_ATTRIBUTE_KEY,
					&attr);
				if(crtn) {
					clErrorLog("CertVerify: valid CCHandle but no key!\n");
					CssmError::throwMe(CSSMERR_CL_INVALID_CONTEXT_HANDLE);
				}
				/* require match */
				assert(signerPubKey != NULL);
				CSSM_KEY_PTR contextPubKey = attr->Attribute.Key;
				if(contextPubKey->KeyHeader.AlgorithmId != 
				   signerPubKey->KeyHeader.AlgorithmId) {
					clErrorLog("CertVerify: AlgorithmId mismatch!\n");
					CssmError::throwMe(CSSMERR_CL_INVALID_CONTEXT_HANDLE);
				}
				
				/* TBD - check key size, when we have a CSP which can report it */
				/* TBD - anything else? */
			}	/* verifying multiple contexts */
			/* OK to use CCHandle as is for verify context */
		}	/* valid CCHandle */
		else {
			/* 
			 * All we have is signer cert. We already have its public key;
			 * get signature alg from CertToBeVerified's Cert.algID, which 
			 * we currently have in DER form. Decode it into temp memory.
			 */
			assert(SignerCert != NULL);
			assert(signerPubKey != NULL);
			
			CSSM_X509_ALGORITHM_IDENTIFIER cssmAlgId;
			SecNssCoder coder;
			PRErrorCode prtn;
			
			CssmData &algIdData = algId.get();
			memset(&cssmAlgId, 0, sizeof(cssmAlgId));
			prtn = coder.decode(algIdData.data(), algIdData.length(),
				kSecAsn1AlgorithmIDTemplate, &cssmAlgId);
			if(prtn) {
				CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);
			}

			CSSM_ALGORITHMS vfyAlg = CL_oidToAlg(cssmAlgId.algorithm);
			
			/* attach to CSP, cook up a context */
			cspHand = getGlobalCspHand(true);
			CSSM_RETURN crtn;
			crtn = CSSM_CSP_CreateSignatureContext(cspHand,
				vfyAlg,
				NULL,			// Access Creds
				signerPubKey,
				&ourCcHand);
			CCHandle = ourCcHand;
		}	/* inferring sig verify context from SignerCert */
		verifyData(CCHandle, tbs, sig);
	}
	catch(...) {
		/* FIXME - isn't there a better way to do this? Save the 
		 * exception as a CSSM_RETURN and throw it if nonzero later?
		 */
		if(context != NULL) {
			CSSM_FreeContext(context);
		}
		CL_freeCSSMKey(signerPubKey, *this);
		if(ourCcHand != CSSM_INVALID_HANDLE) {
			CSSM_DeleteContext(ourCcHand);
		}
		throw;
	}
	if(context != NULL) {
		CSSM_FreeContext(context);
	}
	CL_freeCSSMKey(signerPubKey, *this);
	if(ourCcHand != CSSM_INVALID_HANDLE) {
		CSSM_DeleteContext(ourCcHand);
	}
}

/*
 * Given a DER-encoded TBSCert and a fully specified crypto context,
 * sign the TBSCert and return the resulting DER-encoded Cert.
 */
void
AppleX509CLSession::CertSign(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &CertTemplate,
	const CSSM_FIELD *SignScope,
	uint32 ScopeSize,
	CssmData &SignedCert)
{
	if((SignScope != NULL) || (ScopeSize != 0)) {
		CssmError::throwMe(CSSMERR_CL_SCOPE_NOT_SUPPORTED);
	}
	if(CCHandle == CSSM_INVALID_HANDLE) {
		CssmError::throwMe(CSSMERR_CL_INVALID_CONTEXT_HANDLE);
	}
	
	/* cook up algId from context->(signing key, sig algorithm) */
	CSSM_CONTEXT_PTR context = NULL;		// must be freed
	CSSM_RETURN crtn;
	crtn = CSSM_GetContext(CCHandle, &context);
	if(crtn) {
		CssmError::throwMe(CSSMERR_CL_INVALID_CONTEXT_HANDLE);
	}
	CSSM_CONTEXT_ATTRIBUTE_PTR attr;		// not freed
	crtn = CSSM_GetContextAttribute(context,
		CSSM_ATTRIBUTE_KEY,
		&attr);
	if(crtn) {
		clErrorLog("CertSign: valid CCHandle but no signing key!\n");
		CssmError::throwMe(CSSMERR_CL_INVALID_CONTEXT_HANDLE);
	}
	CSSM_KEY_PTR signingKey = attr->Attribute.Key;
	if(signingKey == NULL) {
		clErrorLog("CertSign: valid CCHandle, NULL signing key!\n");
		CssmError::throwMe(CSSMERR_CL_INVALID_CONTEXT_HANDLE);
	}

	CssmAutoData encAlgId(*this);
	CssmAutoData rawSig(*this);
	CssmAutoData fullCert(*this);
	try {
		/*
		 * FIXME: we really should break up the template and ensure that its
		 * signature algId matches the one we're signing with, or just use
		 * that algId here....for now, this is up to the app to make sure.
		 */
		
		/* temp allocs/encode into here */
		SecNssCoder coder;
		
		/* CSSM alg --> CSSM_X509_ALGORITHM_IDENTIFIER */
		CSSM_X509_ALGORITHM_IDENTIFIER algId;
		memset(&algId, 0, sizeof(algId));
		const CSSM_OID *oid = cssmAlgToOid(context->AlgorithmType);

		if(oid == NULL) {
			clErrorLog("CertSIgn: unknown alg (%u)\n", 
				(unsigned)context->AlgorithmType);
			CssmError::throwMe(CSSMERR_CL_UNKNOWN_FORMAT);		
		}
		algId.algorithm = *oid;

		/* NULL params - FIXME - is this OK? */
		CL_nullAlgParams(algId);
		/* DER-encode the algID */
		PRErrorCode prtn;
		prtn = SecNssEncodeItemOdata(&algId, kSecAsn1AlgorithmIDTemplate, 
			encAlgId);
		if(prtn) {
			CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
		}

		/* sign TBS --> rawSig */
		signData(CCHandle, CertTemplate, rawSig);
		/* put it all together */
		CL_certEncodeComponents(CertTemplate, encAlgId, rawSig, fullCert);
	}
	catch (...) {
		CSSM_FreeContext(context);
		throw;
	}
	CSSM_FreeContext(context);
	SignedCert = fullCert.release();
}

/*** Private functions ***/

/*
 * Sign a CssmData with the specified signing context. Used for
 * signing both certs and CRLs; this routine doesn't know anything 
 * about either one. 
 */
void 
AppleX509CLSession::signData(
	CSSM_CC_HANDLE	ccHand,
	const CssmData	&tbs,
	CssmOwnedData	&sig)			// mallocd and returned
{
	CSSM_RETURN crtn;
	CssmData cSig;
	
	crtn = CSSM_SignData(
		ccHand,
		&tbs,
		1,					// DataBufCount
		CSSM_ALGID_NONE,	// DigestAlgorithm,
		&cSig);
	if(crtn) {
		clErrorLog("AppleX509CLSession::CSSM_SignData: %ld\n", crtn);
		CssmError::throwMe(crtn);
	}
	sig.set(cSig);
}

/*
 * Verify a block of data given a crypto context and a signature. 
 * Used for verifying certs and CRLs. Returns a CSSM_RETURN (callers
 * always need to clean up after calling us).
 */ 
void AppleX509CLSession::verifyData(
	CSSM_CC_HANDLE	ccHand,
	const CssmData	&tbs,
	const CssmData	&sig)
{
	CSSM_RETURN crtn;
	
	crtn = CSSM_VerifyData(ccHand,
		&tbs,
		1,
		CSSM_ALGID_NONE,		// Digest alg
		&sig);
	if(crtn) {
		if(crtn == CSSMERR_CSP_VERIFY_FAILED) {
			/* CSP and CL report this differently */
			CssmError::throwMe(CSSMERR_CL_VERIFICATION_FAILURE);
		}
		else {
			CssmError::throwMe(crtn);
		}
	}
}

