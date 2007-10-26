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
// Session_Cert.cpp - cert-related session functions.
//

#include "AppleX509CLSession.h"
#include "DecodedCert.h"
#include "DecodedCrl.h"
#include "CLCachedEntry.h"
#include "cldebugging.h"
#include <Security/oidscert.h>

void
AppleX509CLSession::CertDescribeFormat(
	uint32 &NumberOfFields,
	CSSM_OID_PTR &OidList)
{
	DecodedCert::describeFormat(*this, NumberOfFields, OidList);
}

void
AppleX509CLSession::CertGetAllFields(
	const CssmData &Cert,
	uint32 &NumberOfFields,
	CSSM_FIELD_PTR &CertFields)
{
	DecodedCert decodedCert(*this, Cert);
	decodedCert.getAllParsedCertFields(NumberOfFields, CertFields);
}


CSSM_HANDLE
AppleX509CLSession::CertGetFirstFieldValue(
	const CssmData &EncodedCert,
	const CssmData &CertField,
	uint32 &NumberOfMatchedFields,
	CSSM_DATA_PTR &Value)
{
	NumberOfMatchedFields = 0;
	Value = NULL;
	CssmAutoData aData(*this);
	
	DecodedCert *decodedCert = new DecodedCert(*this, EncodedCert);
	uint32 numMatches;
	
	/* this returns false if field not there, throws on bad OID */
	bool brtn;
	try {
		brtn = decodedCert->getCertFieldData(CertField, 
			0, 				// index
			numMatches, 
			aData);
	}
	catch (...) {
		delete decodedCert;
		throw;
	}
	if(!brtn) {
		delete decodedCert;
		return CSSM_INVALID_HANDLE;
	}

	/* cook up a CLCachedCert, stash it in cache */
	CLCachedCert *cachedCert = new CLCachedCert(*decodedCert);
	cacheMap.addEntry(*cachedCert, cachedCert->handle());
	
	/* cook up a CLQuery, stash it */
	CLQuery *query = new CLQuery(
		CLQ_Cert, 
		CertField, 
		numMatches,
		false,				// isFromCache
		cachedCert->handle());
	queryMap.addEntry(*query, query->handle());
	
	/* success - copy field data to outgoing Value */
	Value = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA));
	*Value = aData.release();
	NumberOfMatchedFields = numMatches;
	return query->handle();
}


bool
AppleX509CLSession::CertGetNextFieldValue(
	CSSM_HANDLE ResultsHandle,
	CSSM_DATA_PTR &Value)
{
	/* fetch & validate the query */
	CLQuery *query = queryMap.lookupEntry(ResultsHandle);
	if(query == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_RESULTS_HANDLE);
	}
	if(query->queryType() != CLQ_Cert) {
		clErrorLog("CertGetNextFieldValue: bad queryType (%d)", (int)query->queryType());
		CssmError::throwMe(CSSMERR_CL_INVALID_RESULTS_HANDLE);
	}
	if(query->nextIndex() >= query->numFields()) {
		return false;
	}

	/* fetch the associated cached cert */
	CLCachedCert *cachedCert = lookupCachedCert(query->cachedObject());
	uint32 dummy;
	CssmAutoData aData(*this);
	if(!cachedCert->cert().getCertFieldData(query->fieldId(), 
		query->nextIndex(), 
		dummy,
		aData))  {
		return false;
	}
		
	/* success - copy field data to outgoing Value */
	Value = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA));
	*Value = aData.release();
	query->incrementIndex();
	return true;
}

void
AppleX509CLSession::CertCache(
	const CssmData &EncodedCert,
	CSSM_HANDLE &CertHandle)
{
	DecodedCert *decodedCert = new DecodedCert(*this, EncodedCert);
	
	/* cook up a CLCachedCert, stash it in cache */
	CLCachedCert *cachedCert = new CLCachedCert(*decodedCert);
	cacheMap.addEntry(*cachedCert, cachedCert->handle());
	CertHandle = cachedCert->handle();
}

CSSM_HANDLE
AppleX509CLSession::CertGetFirstCachedFieldValue(
	CSSM_HANDLE CertHandle,
	const CssmData &CertField,
	uint32 &NumberOfMatchedFields,
	CSSM_DATA_PTR &Value)
{
	/* fetch the associated cached cert */
	CLCachedCert *cachedCert = lookupCachedCert(CertHandle);
	if(cachedCert == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_CACHE_HANDLE);
	}
	
	CssmAutoData aData(*this);
	uint32 numMatches;

	/* this returns false if field not there, throws on bad OID */
	if(!cachedCert->cert().getCertFieldData(CertField, 
			0, 				// index
			numMatches, 
			aData)) {
		return CSSM_INVALID_HANDLE;
	}

	/* cook up a CLQuery, stash it */
	CLQuery *query = new CLQuery(
		CLQ_Cert, 
		CertField, 
		numMatches,
		true,				// isFromCache
		cachedCert->handle());
	queryMap.addEntry(*query, query->handle());
	
	/* success - copy field data to outgoing Value */
	Value = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA));
	*Value = aData.release();
	NumberOfMatchedFields = numMatches;
	return query->handle();
}


bool
AppleX509CLSession::CertGetNextCachedFieldValue(
	CSSM_HANDLE ResultsHandle,
	CSSM_DATA_PTR &Value)
{
	/* Identical to, so just call... */
	return CertGetNextFieldValue(ResultsHandle, Value);
}

void
AppleX509CLSession::CertAbortCache(
	CSSM_HANDLE CertHandle)
{
	/* fetch the associated cached cert, remove from map, delete it */
	CLCachedCert *cachedCert = lookupCachedCert(CertHandle);
	if(cachedCert == NULL) {
		clErrorLog("CertAbortCache: cachedCert not found");
		CssmError::throwMe(CSSMERR_CL_INVALID_CACHE_HANDLE);
	}
	cacheMap.removeEntry(cachedCert->handle());
	delete cachedCert;
}

/*
 * Abort either type of cert field query (cache based or non-cache based)
 */
void
AppleX509CLSession::CertAbortQuery(
	CSSM_HANDLE ResultsHandle)
{
	/* fetch & validate the query */
	CLQuery *query = queryMap.lookupEntry(ResultsHandle);
	if(query == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_RESULTS_HANDLE);
	}
	if(query->queryType() != CLQ_Cert) {
		clErrorLog("CertAbortQuery: bad queryType (%d)", (int)query->queryType());
		CssmError::throwMe(CSSMERR_CL_INVALID_RESULTS_HANDLE);
	}
	
	if(!query->fromCache()) {
		/* the associated cached cert was created just for this query; dispose */
		CLCachedCert *cachedCert = lookupCachedCert(query->cachedObject());
		if(cachedCert == NULL) {
			/* should never happen */
			clErrorLog("CertAbortQuery: cachedCert not found");
			CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
		}
		cacheMap.removeEntry(cachedCert->handle());
		delete cachedCert;
	}
	queryMap.removeEntry(query->handle());
	delete query;
}

void
AppleX509CLSession::CertCreateTemplate(
	uint32 NumberOfFields,
	const CSSM_FIELD CertFields[],
	CssmData &CertTemplate)
{
	/* cook up an empty Cert */
	DecodedCert cert(*this);

	/* grind thru specified fields; exceptions are fatal */
	for(uint32 dex=0; dex<NumberOfFields; dex++) {
		cert.setCertField(
			CssmOid::overlay(CertFields[dex].FieldOid), 
			CssmData::overlay(CertFields[dex].FieldValue));
	}
	
	/* TBD - ensure all required fields are set? We do this 
	 * when we sign the cert; maybe we should do it here. */
	
	/* 
	 * We have the CertificateToSign in NSS format. Encode.
	 */
	CertTemplate.Data = NULL;
	CertTemplate.Length = 0;
	CssmRemoteData rData(*this, CertTemplate);
	cert.encodeTbs(rData);
	rData.release();
}


void
AppleX509CLSession::CertGetAllTemplateFields(
	const CssmData &CertTemplate,
	uint32 &NumberOfFields,
	CSSM_FIELD_PTR &CertFields)
{
	DecodedCert	cert(*this);		// empty
	cert.decodeTbs(CertTemplate);
	cert.getAllParsedCertFields(NumberOfFields, CertFields);
}

void
AppleX509CLSession::FreeFields(
	uint32 NumberOfFields,
	CSSM_FIELD_PTR &FieldArray)
{
	unsigned 		i;
	CSSM_FIELD_PTR 	thisField;
	CSSM_OID_PTR	thisOid;
	
	for(i=0; i<NumberOfFields; i++) {
		thisField = &FieldArray[i];
		thisOid = &thisField->FieldOid;
		
		/* oid-specific handling of value */
		/* BUG - the CssmRemoteData constructor clears the referent,
		 * iff the referent is a CSSSM_DATA (as opposed to a CssmData).
		 */
		CssmData &cData = CssmData::overlay(thisField->FieldValue);
		CssmRemoteData rData(*this, cData);
		try {
			DecodedCert::freeCertFieldData(CssmOid::overlay(*thisOid), rData);
		}
		catch(...) {
			/* CRL field? */
			DecodedCrl::freeCrlFieldData(CssmOid::overlay(*thisOid), rData);
		}
		/* and the oid itself */
		free(thisOid->Data);
		thisOid->Data = NULL;
		thisOid->Length = 0;
	}
	free(FieldArray);
}

void
AppleX509CLSession::FreeFieldValue(
	const CssmData &CertOrCrlOid,
	CssmData &Value)
{
	CssmRemoteData cd(*this, Value);
	try {
		DecodedCert::freeCertFieldData(CertOrCrlOid, cd);
	}
	catch(...) {
		/* CRL field? */
		DecodedCrl::freeCrlFieldData(CertOrCrlOid, cd);
	}
	free(&Value);
}

void
AppleX509CLSession::CertGroupFromVerifiedBundle(
	CSSM_CC_HANDLE CCHandle,
	const CSSM_CERT_BUNDLE &CertBundle,
	const CssmData *SignerCert,
	CSSM_CERTGROUP_PTR &CertGroup)
{
	unimplemented();
}

void
AppleX509CLSession::CertGroupToSignedBundle(
	CSSM_CC_HANDLE CCHandle,
	const CSSM_CERTGROUP &CertGroupToBundle,
	const CSSM_CERT_BUNDLE_HEADER *BundleInfo,
	CssmData &SignedBundle)
{
	unimplemented();
}

void
AppleX509CLSession::PassThrough(
	CSSM_CC_HANDLE CCHandle,
	uint32 PassThroughId,
	const void *InputParams,
	void **OutputParams)
{
	switch(PassThroughId) {
		case CSSM_APPLEX509CL_OBTAIN_CSR:
		{
			/*
			 * Create a Cert Signing Request (CSR).
			 * Input is a CSSM_APPLE_CL_CSR_REQUEST.
			 * Output is a PEM-encoded CertSigningRequest (NSS type
			 * NSS_SignedCertRequest from pkcs10). 
			 */
			if(InputParams == NULL) {
				CssmError::throwMe(CSSMERR_CL_INVALID_INPUT_POINTER);
			}
			if(OutputParams == NULL) {
				CssmError::throwMe(CSSMERR_CL_INVALID_OUTPUT_POINTER);
			}
			CSSM_APPLE_CL_CSR_REQUEST *csrReq = 
				(CSSM_APPLE_CL_CSR_REQUEST *)InputParams;
			if((csrReq->subjectNameX509 == NULL) ||
			(csrReq->signatureOid.Data == NULL) ||
			(csrReq->subjectPublicKey == NULL) ||
			(csrReq->subjectPrivateKey == NULL)) {
				CssmError::throwMe(CSSMERR_CL_INVALID_INPUT_POINTER);
			}
			CSSM_DATA_PTR csrPtr = NULL;
			generateCsr(CCHandle, csrReq, csrPtr);
			*OutputParams = csrPtr;
			break;
		}	
		case CSSM_APPLEX509CL_VERIFY_CSR:
		{
			/*
			 * Perform signature verify of a CSR.
			 * Input:  CSSM_DATA referring to a DER-encoded CSR.
			 * Output: Nothing, throws CSSMERR_CL_VERIFICATION_FAILURE
			 *         on failure.
			 */
			if(InputParams == NULL) {
				CssmError::throwMe(CSSMERR_CL_INVALID_INPUT_POINTER);
			}
			const CSSM_DATA *csrPtr = (const CSSM_DATA *)InputParams;
			verifyCsr(csrPtr);
			break;
		}	
		default:
			CssmError::throwMe(CSSMERR_CL_INVALID_PASSTHROUGH_ID);
	}
}

