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
// AppleX509CLSession.h - general CL session functions.
//
#ifndef _H_APPLEX509CLSESSION
#define _H_APPLEX509CLSESSION

#include <security_cdsa_plugin/CLsession.h>
#include "CLCachedEntry.h"
#include "DecodedCert.h"
#include "LockedMap.h"
#include <security_utilities/threading.h>
#include <Security/cssmapple.h>

class AppleX509CLSession : public CLPluginSession {

public:

	AppleX509CLSession(
		CSSM_MODULE_HANDLE theHandle,
		CssmPlugin &plug,
		const CSSM_VERSION &version,
		uint32 subserviceId,
		CSSM_SERVICE_TYPE subserviceType,
		CSSM_ATTACH_FLAGS attachFlags,
		const CSSM_UPCALLS &upcalls);

	~AppleX509CLSession();

// ====================================================================
// Cert Interpretation
// ====================================================================				
	
	void CertDescribeFormat(
		uint32 &NumberOfFields,
		CSSM_OID_PTR &OidList);

// Non-cached
	
	void CertGetAllFields(
		const CssmData &Cert,
		uint32 &NumberOfFields,
		CSSM_FIELD_PTR &CertFields);

	CSSM_HANDLE CertGetFirstFieldValue(
		const CssmData &Cert,
		const CssmData &CertField,
		uint32 &NumberOfMatchedFields,
		CSSM_DATA_PTR &Value);

	bool CertGetNextFieldValue(
		CSSM_HANDLE ResultsHandle,
		CSSM_DATA_PTR &Value);


// Cached
	
	void CertCache(
		const CssmData &Cert,
		CSSM_HANDLE &CertHandle);

	CSSM_HANDLE CertGetFirstCachedFieldValue(
		CSSM_HANDLE CertHandle,
		const CssmData &CertField,
		uint32 &NumberOfMatchedFields,
		CSSM_DATA_PTR &Value);

	bool CertGetNextCachedFieldValue(
		CSSM_HANDLE ResultsHandle,
		CSSM_DATA_PTR &Value);

	void CertAbortCache(
		CSSM_HANDLE CertHandle);

	void CertAbortQuery(
		CSSM_HANDLE ResultsHandle);



// Templates
							
	void CertCreateTemplate(
		uint32 NumberOfFields,
		const CSSM_FIELD CertFields[],
		CssmData &CertTemplate);

	void CertGetAllTemplateFields(
		const CssmData &CertTemplate,
		uint32 &NumberOfFields,
		CSSM_FIELD_PTR &CertFields);
						

// Memory						

	void FreeFields(
		uint32 NumberOfFields,
		CSSM_FIELD_PTR &FieldArray);
	void FreeFieldValue(
		const CssmData &CertOrCrlOid,
		CssmData &Value);

// Key
	
	void CertGetKeyInfo(
		const CssmData &Cert,
		CSSM_KEY_PTR &Key);
						
// ====================================================================
// CRL Interpretation
// ====================================================================

// Non-cached
	
	void CrlDescribeFormat(
		uint32 &NumberOfFields,
		CSSM_OID_PTR &OidList);

	void CrlGetAllFields(
		const CssmData &Crl,
		uint32 &NumberOfCrlFields,
		CSSM_FIELD_PTR &CrlFields);

	CSSM_HANDLE CrlGetFirstFieldValue(
		const CssmData &Crl,
		const CssmData &CrlField,
		uint32 &NumberOfMatchedFields,
		CSSM_DATA_PTR &Value);
	
	bool CrlGetNextFieldValue(
		CSSM_HANDLE ResultsHandle,
		CSSM_DATA_PTR &Value);

	void IsCertInCrl(
		const CssmData &Cert,
		const CssmData &Crl,
		CSSM_BOOL &CertFound);
	
	
// Cached

	void CrlCache(
		const CssmData &Crl,
		CSSM_HANDLE &CrlHandle);

	void CrlGetAllCachedRecordFields(CSSM_HANDLE CrlHandle,
		const CssmData &CrlRecordIndex,
		uint32 &NumberOfFields,
		CSSM_FIELD_PTR &CrlFields);
						
	CSSM_HANDLE CrlGetFirstCachedFieldValue(
		CSSM_HANDLE CrlHandle,
		const CssmData *CrlRecordIndex,
		const CssmData &CrlField,
		uint32 &NumberOfMatchedFields,
		CSSM_DATA_PTR &Value);

	bool CrlGetNextCachedFieldValue(
		CSSM_HANDLE ResultsHandle,
		CSSM_DATA_PTR &Value);

	void IsCertInCachedCrl(
		const CssmData &Cert,
		CSSM_HANDLE CrlHandle,
		CSSM_BOOL &CertFound,
		CssmData &CrlRecordIndex);

	void CrlAbortCache(
		CSSM_HANDLE CrlHandle);

	void CrlAbortQuery(
		CSSM_HANDLE ResultsHandle);


// Template

	void CrlCreateTemplate(
		uint32 NumberOfFields,
		const CSSM_FIELD *CrlTemplate,
		CssmData &NewCrl);

	void CrlSetFields(
		uint32 NumberOfFields,
		const CSSM_FIELD *CrlTemplate,
		const CssmData &OldCrl,
		CssmData &ModifiedCrl);

	void CrlAddCert(
		CSSM_CC_HANDLE CCHandle,
		const CssmData &Cert,
		uint32 NumberOfFields,
		const CSSM_FIELD CrlEntryFields[],
		const CssmData &OldCrl,
		CssmData &NewCrl);

	void CrlRemoveCert(
		const CssmData &Cert,
		const CssmData &OldCrl,
		CssmData &NewCrl);

// ====================================================================
// Verify/Sign
// ====================================================================
	
// Certs
	
	void CertVerifyWithKey(
		CSSM_CC_HANDLE CCHandle,
		const CssmData &CertToBeVerified);
	
	void CertVerify(
		CSSM_CC_HANDLE CCHandle,
		const CssmData &CertToBeVerified,
		const CssmData *SignerCert,
		const CSSM_FIELD *VerifyScope,
		uint32 ScopeSize);
						
	void CertSign(
		CSSM_CC_HANDLE CCHandle,
		const CssmData &CertTemplate,
		const CSSM_FIELD *SignScope,
		uint32 ScopeSize,
		CssmData &SignedCert);

// Cert Groups

	void CertGroupFromVerifiedBundle(
		CSSM_CC_HANDLE CCHandle,
		const CSSM_CERT_BUNDLE &CertBundle,
		const CssmData *SignerCert,
		CSSM_CERTGROUP_PTR &CertGroup);
						
	void CertGroupToSignedBundle(
		CSSM_CC_HANDLE CCHandle,
		const CSSM_CERTGROUP &CertGroupToBundle,
		const CSSM_CERT_BUNDLE_HEADER *BundleInfo,
		CssmData &SignedBundle);
						
// CRLs

	void CrlVerifyWithKey(
		CSSM_CC_HANDLE CCHandle,
		const CssmData &CrlToBeVerified);

	void CrlVerify(
		CSSM_CC_HANDLE CCHandle,
		const CssmData &CrlToBeVerified,
		const CssmData *SignerCert,
		const CSSM_FIELD *VerifyScope,
		uint32 ScopeSize);
						
	void CrlSign(
		CSSM_CC_HANDLE CCHandle,
		const CssmData &UnsignedCrl,
		const CSSM_FIELD *SignScope,
		uint32 ScopeSize,
		CssmData &SignedCrl);

// ====================================================================
// Module Specific Pass-Through
// ====================================================================
	
	void PassThrough(
		CSSM_CC_HANDLE CCHandle,
		uint32 PassThroughId,
		const void *InputParams,
		void **OutputParams);

private:
	/* routines in Session_Cert.cpp */
	void getAllParsedCertFields(
		const DecodedCert	&cert,
		uint32 				&NumberOfFields,		// RETURNED
		CSSM_FIELD_PTR 		&CertFields);			// RETURNED

	/* routines in Session_Crypto.cpp */
	void signData(
		CSSM_CC_HANDLE		ccHand,
		const CssmData		&tbs,
		CssmOwnedData		&sig);			// mallocd and returned
	void verifyData(
		CSSM_CC_HANDLE		ccHand,
		const CssmData		&tbs,
		const CssmData		&sig);	
		
	/* routines in Session_CSR.cpp */
	void generateCsr(
		CSSM_CC_HANDLE 		CCHandle,
		const CSSM_APPLE_CL_CSR_REQUEST *csrReq,
		CSSM_DATA_PTR		&csrPtr);
	void verifyCsr(
		const CSSM_DATA		*csrPtr);

	/*
	 * Maps of cached certs, CRLs, and active queries
	 * This one holds cached certs and CRLs.
	 */
	LockedMap<CSSM_HANDLE, CLCachedEntry>	cacheMap;
	LockedMap<CSSM_HANDLE, CLQuery>			queryMap;

	CLCachedCert *lookupCachedCert(CSSM_HANDLE handle);
	CLCachedCRL	 *lookupCachedCRL(CSSM_HANDLE handle);
};

#endif //_H_APPLEX509CLSESSION
