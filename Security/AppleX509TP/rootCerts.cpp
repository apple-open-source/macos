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
#include "certGroupUtils.h"
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
			debug("tpAnchor", "Bad certGroup Type (%d)\n",
				(int)roots.type());
			return NULL;
		}
		numTpRoots = roots.count();
		if(numTpRoots == 0) {
			debug("tpAnchor", "empty certGroup\n");
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
				debug("tpAnchor", "GetFirstFieldValue error on cert %u",
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
				debug("tpAnchor", "CSSM_CL_CertGetKeyInfo error on cert %u",
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
				debug("tpAnchor", "CSSM_GetAPIMemoryFunctions error");
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
