/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please 
 * see the License for the specific language governing rights and 
 * limitations under the License.
 */


/*
	File:		rootCerts.cp

	Contains:	Bridge between SecTrustGetCSSMAnchorCertificates() and 
	            TP's internally cached tpRootCert array.

	Written by:	Doug Mitchell. 

	Copyright:	Copyright 2002 by Apple Computer, Inc., all rights reserved.

*/

#include "rootCerts.h"

#if		TP_ROOT_CERT_ENABLE

#include "certGroupUtils.h"
#include "tpdebugging.h"
#include <Security/Trust.h>
#include <Security/TrustStore.h>
#include <Security/debugging.h>
#include <Security/oidscert.h>

/* static in TPRootStore */
ModuleNexus<TPRootStore> TPRootStore::tpGlobalRoots;

TPRootStore::~TPRootStore()
{
	/* 
	 * Technically this never gets called because the only instance
	 * of a TPRootStore is via tpGlobalRoots. Freeing mRootCerts
	 * here really doesn't accomplish anything.
	 */
}

const tpRootCert *TPRootStore::rootCerts(
	CSSM_CL_HANDLE clHand,
	unsigned &numRootCerts)
{
	StLock<Mutex> _(mLock);
	if(mRootCerts) {
		numRootCerts = mNumRootCerts;
		return mRootCerts;
	}
	
	CssmAllocator &alloc(CssmAllocator::standard());
	CertGroup roots;
	tpRootCert *tpRoots = NULL;		// copy to mRootCerts on success
	unsigned numTpRoots = 0;
	
	try {
		/* Obtain system-wide root certs in blob format */
		Security::KeychainCore::TrustStore &trustStore = 
			Security::KeychainCore::Trust::gStore();
		trustStore.getCssmRootCertificates(roots);
		if(roots.type() != CSSM_CERTGROUP_DATA) {
			secdebug("tpAnchor", "Bad certGroup Type (%d)\n",
				(int)roots.type());
			return NULL;
		}
		numTpRoots = roots.count();
		if(numTpRoots == 0) {
			secdebug("tpAnchor", "empty certGroup\n");
			return NULL;
		}
		
		/* set up tpRoots array, one for each cert in the group */
		tpRoots = 
			(tpRootCert *)alloc.malloc(numTpRoots * sizeof(tpRootCert));
		memset(tpRoots, 0, numTpRoots * sizeof(tpRootCert));
		for(uint32 certNum=0; certNum<numTpRoots; certNum++) {
			tpRootCert *tpRoot = &tpRoots[certNum];
			const CSSM_DATA *certData = &((roots.blobCerts())[certNum]);
			
			/* extract normalized subject name */
			CSSM_DATA *field;
			CSSM_HANDLE ResultsHandle;
			uint32 numFields;
			CSSM_RETURN crtn;
			crtn = CSSM_CL_CertGetFirstFieldValue(
				clHand,
				certData,
				&CSSMOID_X509V1SubjectName,
				&ResultsHandle,
				&numFields,
				&field);
			if(crtn) {
				secdebug("tpAnchor", "GetFirstFieldValue error on cert %u",
					(unsigned)certNum);
				continue;
			}
			CSSM_CL_CertAbortQuery(clHand, ResultsHandle);
			tpCopyCssmData(alloc, field, &tpRoot->subjectName);
			CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SubjectName, 
				field);
				
			/* extract public key info - the blob and key size in bits */
			CSSM_KEY_PTR key;
			crtn = CSSM_CL_CertGetKeyInfo(clHand, certData, &key);
			if(crtn) {
				secdebug("tpAnchor", "CSSM_CL_CertGetKeyInfo error on cert %u",
					(unsigned)certNum);
				/* clear out this tpRoot? */
				continue;
			}
			tpCopyCssmData(alloc, &key->KeyData, &tpRoot->publicKey);
			tpRoot->keySize = key->KeyHeader.LogicalKeySizeInBits;
			
			/* A hole in the CDSA API: there is no free function at the
			 * CL API for this key. It got mallocd with clHand's
			 * allocator....
			 */
			CSSM_API_MEMORY_FUNCS memFuncs;
			crtn = CSSM_GetAPIMemoryFunctions(clHand, &memFuncs);
			if(crtn) {
				secdebug("tpAnchor", "CSSM_GetAPIMemoryFunctions error");
				/* Oh well.. */
				continue;
			}
			memFuncs.free_func(key->KeyData.Data, memFuncs.AllocRef);
			memFuncs.free_func(key, memFuncs.AllocRef);
		}	/* main loop */
	}
	catch(...) {
		/* TBD */
		return NULL;
	}
	mNumRootCerts = numTpRoots;
	numRootCerts = mNumRootCerts;
	mRootCerts = tpRoots;
	return mRootCerts;
}

/*
 * Compare a root cert to a list of known embedded roots.
 */
CSSM_BOOL tp_isKnownRootCert(
	TPCertInfo			*rootCert,		// raw cert to compare
	CSSM_CL_HANDLE		clHand)
{
	const CSSM_DATA		*subjectName = NULL;
	CSSM_DATA_PTR		publicKey = NULL;
	unsigned			dex;
	CSSM_BOOL			brtn = CSSM_FALSE;
	CSSM_DATA_PTR		valToFree = NULL;
	const 				tpRootCert *roots;
	unsigned 			numRoots;
	
	roots = TPRootStore::tpGlobalRoots().rootCerts(clHand, numRoots);
	
	subjectName = rootCert->subjectName();
	publicKey = tp_CertGetPublicKey(rootCert, &valToFree);	
	if(publicKey == NULL) {
		tpPolicyError("tp_isKnownRootCert: error retrieving public "
			"key info!");
		goto errOut;
	}
	
	/*
	 * Grind thru the list of known certs, demanding perfect match of 
	 * both fields 
	 */
	for(dex=0; dex<numRoots; dex++) {
		if(!tpCompareCssmData(subjectName, 
	                          &roots[dex].subjectName)) {
	    	continue;
	    }
		if(!tpCompareCssmData(publicKey,
	                          &roots[dex].publicKey)) {
	    	continue;
	    }
	    brtn = CSSM_TRUE;
	    break;
	}
errOut:
	tp_CertFreePublicKey(rootCert->clHand(), valToFree);
	return brtn;
}

/*
 * Attempt to verify specified cert (from the end of a chain) with one of
 * our known roots. 
 */
CSSM_BOOL tp_verifyWithKnownRoots(
	CSSM_CL_HANDLE	clHand, 
	CSSM_CSP_HANDLE	cspHand, 
	TPCertInfo		*certToVfy)			// last in chain, not root
{
	CSSM_KEY 		rootKey;			// pub key manufactured from tpRootCert info
	CSSM_CC_HANDLE	ccHand;				// signature context
	CSSM_RETURN		crtn;
	unsigned 		dex;
	const tpRootCert *rootInfo;
	CSSM_BOOL		brtn = CSSM_FALSE;
	CSSM_KEYHEADER	*hdr = &rootKey.KeyHeader;
	CSSM_X509_ALGORITHM_IDENTIFIER_PTR algId;
	CSSM_DATA_PTR	valToFree = NULL;
	CSSM_ALGORITHMS	sigAlg;
	const tpRootCert *rootCerts = NULL;
	unsigned 		numRootCerts = 0;
		
	memset(&rootKey, 0, sizeof(CSSM_KEY));
	
	/*
	 * Get signature algorithm from subject key
	 */
	algId = tp_CertGetAlgId(certToVfy, &valToFree);
	if(algId == NULL) {
		/* bad cert */
		return CSSM_FALSE;
	}
	/* subsequest errors to errOut: */
	
	/* map to key and signature algorithm */
	sigAlg = tpOidToAldId(&algId->algorithm, &hdr->AlgorithmId);
	if(sigAlg == CSSM_ALGID_NONE) {
		tpPolicyError("tp_verifyWithKnownRoots: unknown sig alg");
		goto errOut;
	}
	
	/* Set up other constant key fields */
	hdr->BlobType = CSSM_KEYBLOB_RAW;
	switch(hdr->AlgorithmId) {
		case CSSM_ALGID_RSA:
			hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
			break;
		case CSSM_ALGID_DSA:
			hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
			break;
		case CSSM_ALGID_FEE:
			hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
			break;
		default:
			/* punt */
			hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	}
	hdr->KeyClass = CSSM_KEYCLASS_PUBLIC_KEY;
	hdr->KeyAttr = CSSM_KEYATTR_MODIFIABLE | CSSM_KEYATTR_EXTRACTABLE;
	hdr->KeyUsage = CSSM_KEYUSE_VERIFY;
	
	rootCerts = TPRootStore::tpGlobalRoots().rootCerts(clHand, numRootCerts);
	for(dex=0; dex<numRootCerts; dex++) {
		rootInfo = &rootCerts[dex];
		if(!tpCompareCssmData(&rootInfo->subjectName, certToVfy->issuerName())) {
			/* not this root */
			continue;
		}

		/* only variation in key in the loop - raw key bits and size */
		rootKey.KeyData = rootInfo->publicKey;
		hdr->LogicalKeySizeInBits = rootInfo->keySize;
		crtn = CSSM_CSP_CreateSignatureContext(cspHand,
			sigAlg,
			NULL,		// AcccedCred
			&rootKey,
			&ccHand);
		if(crtn) {
			tpPolicyError("tp_verifyWithKnownRoots: "
				"CSSM_CSP_CreateSignatureContext err");
			CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
		}
		crtn = CSSM_CL_CertVerify(clHand,
			ccHand,
			certToVfy->itemData(),
			NULL,			// no signer cert
			NULL,			// VerifyScope
			0);				// ScopeSize
		CSSM_DeleteContext(ccHand);
		if(crtn == CSSM_OK) {
			/* success! */
			brtn = CSSM_TRUE;
			break;
		}
	}
errOut:
	if(valToFree != NULL) {
		tp_CertFreeAlgId(clHand, valToFree);
	}
	return brtn;
}

#endif	/* TP_ROOT_CERT_ENABLE */

