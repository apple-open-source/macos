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
// Apple X.509 CRL-related session functions.
//

#include "AppleX509CLSession.h"
#include "clNssUtils.h"
#include "clNameUtils.h"

void
AppleX509CLSession::CrlDescribeFormat(
	uint32 &NumberOfFields,
	CSSM_OID_PTR &OidList)
{
	DecodedCrl::describeFormat(*this, NumberOfFields, OidList);
}


void
AppleX509CLSession::CrlGetAllFields(
	const CssmData &Crl,
	uint32 &NumberOfCrlFields,
	CSSM_FIELD_PTR &CrlFields)
{
	class DecodedCrl decodedCrl(*this, Crl);
	decodedCrl.getAllParsedCrlFields(NumberOfCrlFields, CrlFields);
}


CSSM_HANDLE
AppleX509CLSession::CrlGetFirstFieldValue(
	const CssmData &Crl,
	const CssmData &CrlField,
	uint32 &NumberOfMatchedFields,
	CSSM_DATA_PTR &Value)
{
	NumberOfMatchedFields = 0;
	Value = NULL;
	CssmAutoData aData(*this);
	
	DecodedCrl *decodedCrl = new DecodedCrl(*this, Crl);
	uint32 numMatches;
	
	/* this returns false if field not there, throws on bad OID */
	bool brtn;
	try {
		brtn = decodedCrl->getCrlFieldData(CrlField, 
			0, 				// index
			numMatches, 
			aData);
	}
	catch (...) {
		delete decodedCrl;
		throw;
	}
	if(!brtn) {
		delete decodedCrl;
		return CSSM_INVALID_HANDLE;
	}

	/* cook up a CLCachedCRL, stash it in cache */
	CLCachedCRL *cachedCrl = new CLCachedCRL(*decodedCrl);
	cacheMap.addEntry(*cachedCrl, cachedCrl->handle());
	
	/* cook up a CLQuery, stash it */
	CLQuery *query = new CLQuery(
		CLQ_CRL, 
		CrlField, 
		numMatches,
		false,				// isFromCache
		cachedCrl->handle());
	queryMap.addEntry(*query, query->handle());
	
	/* success - copy field data to outgoing Value */
	Value = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA));
	*Value = aData.release();
	NumberOfMatchedFields = numMatches;
	return query->handle();
}

	
bool
AppleX509CLSession::CrlGetNextFieldValue(
	CSSM_HANDLE ResultsHandle,
	CSSM_DATA_PTR &Value)
{
	/* fetch & validate the query */
	CLQuery *query = queryMap.lookupEntry(ResultsHandle);
	if(query == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_RESULTS_HANDLE);
	}
	if(query->queryType() != CLQ_CRL) {
		clErrorLog("CrlGetNextFieldValue: bad queryType (%d)", 
			(int)query->queryType());
		CssmError::throwMe(CSSMERR_CL_INVALID_RESULTS_HANDLE);
	}
	if(query->nextIndex() >= query->numFields()) {
		return false;
	}

	/* fetch the associated cached CRL */
	CLCachedCRL *cachedCrl = lookupCachedCRL(query->cachedObject());
	uint32 dummy;
	CssmAutoData aData(*this);
	if(!cachedCrl->crl().getCrlFieldData(query->fieldId(), 
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
AppleX509CLSession::IsCertInCrl(
	const CssmData &Cert,
	const CssmData &Crl,
	CSSM_BOOL &CertFound)
{
	/* 
	 * Decode the two entities. Note that doing it this way incurs
	 * the unnecessary (for our purposes) overhead of decoding
	 * extensions, but doing it this way is so spiffy that I can't 
	 * resist.
	 */
	DecodedCert decodedCert(*this, Cert);
	DecodedCrl  decodedCrl(*this, Crl);

	NSS_TBSCertificate &tbsCert = decodedCert.mCert.tbs;
	NSS_TBSCrl &tbsCrl = decodedCrl.mCrl.tbs;
	
	/* trivial case - empty CRL */
	unsigned numCrlEntries = 
		clNssArraySize((const void **)tbsCrl.revokedCerts);
	if(numCrlEntries == 0) {
		clFieldLog("IsCertInCrl: empty CRL");
		CertFound = CSSM_FALSE;
		return;
	}
	
	/* 
	 * Get normalized and encoded versions of issuer names. 
	 * Since the decoded entities are local, we can normalize in place.
	 */
	CssmAutoData encCertIssuer(*this);
	CssmAutoData encCrlIssuer(*this);
	try {
		/* snag a handy temp allocator */
		SecNssCoder &coder = decodedCert.coder();
		CL_normalizeX509NameNSS(tbsCert.issuer, coder);
		PRErrorCode prtn = SecNssEncodeItemOdata(&tbsCert.issuer, 
			kSecAsn1NameTemplate, encCertIssuer);
		if(prtn) {
			CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
		}
			
		CL_normalizeX509NameNSS(tbsCrl.issuer, coder);
		prtn = SecNssEncodeItemOdata(&tbsCrl.issuer, 
			kSecAsn1NameTemplate, encCrlIssuer);
		if(prtn) {
			CssmError::throwMe(CSSMERR_CL_MEMORY_ERROR);
		}
	}
	catch(...) {
		clFieldLog("IsCertInCrl: normalize failure");
		throw;
	}
	
	/* issuer names match? */
	CertFound = CSSM_FALSE;
	if(encCertIssuer.get() != encCrlIssuer.get()) {
		clFieldLog("IsCertInCrl: issuer name mismatch");
		return;
	}
	
	/* is this cert's serial number in the CRL? */
	CSSM_DATA &certSerial = tbsCert.serialNumber;
	for(unsigned dex=0; dex<numCrlEntries; dex++) {
		NSS_RevokedCert *revokedCert = tbsCrl.revokedCerts[dex];
		assert(revokedCert != NULL);
		CSSM_DATA &revokedSerial = revokedCert->userCertificate;
		if(clCompareCssmData(&certSerial, &revokedSerial)) {
			/* success */ 
			CertFound = CSSM_TRUE;
			break;
		}
	}
}

#pragma mark --- Cached ---
	
void
AppleX509CLSession::CrlCache(
	const CssmData &Crl,
	CSSM_HANDLE &CrlHandle)
{
	DecodedCrl *decodedCrl = new DecodedCrl(*this, Crl);
	
	/* cook up a CLCachedCRL, stash it in cache */
	CLCachedCRL *cachedCrl = new CLCachedCRL(*decodedCrl);
	cacheMap.addEntry(*cachedCrl, cachedCrl->handle());
	CrlHandle = cachedCrl->handle();
}

/* 
 * FIXME - CrlRecordIndex not supported, it'll require mods to 
 * the DecodedCrl::getCrlFieldData mechanism
 */
CSSM_HANDLE
AppleX509CLSession::CrlGetFirstCachedFieldValue(
	CSSM_HANDLE CrlHandle,
	const CssmData *CrlRecordIndex,
	const CssmData &CrlField,
	uint32 &NumberOfMatchedFields,
	CSSM_DATA_PTR &Value)
{
	if(CrlRecordIndex != NULL) {
		/* not yet */
		CssmError::throwMe(CSSMERR_CL_INVALID_CRL_INDEX);
	}
	
	/* fetch the associated cached CRL */
	CLCachedCRL *cachedCrl = lookupCachedCRL(CrlHandle);
	if(cachedCrl == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_CACHE_HANDLE);
	}
	
	CssmAutoData aData(*this);
	uint32 numMatches;

	/* this returns false if field not there, throws on bad OID */
	if(!cachedCrl->crl().getCrlFieldData(CrlField, 
			0, 				// index
			numMatches, 
			aData)) {
		return CSSM_INVALID_HANDLE;
	}

	/* cook up a CLQuery, stash it */
	CLQuery *query = new CLQuery(
		CLQ_CRL, 
		CrlField, 
		numMatches,
		true,				// isFromCache
		cachedCrl->handle());
	queryMap.addEntry(*query, query->handle());
	
	/* success - copy field data to outgoing Value */
	Value = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA));
	*Value = aData.release();
	NumberOfMatchedFields = numMatches;
	return query->handle();
}


bool
AppleX509CLSession::CrlGetNextCachedFieldValue(
	CSSM_HANDLE ResultsHandle,
	CSSM_DATA_PTR &Value)
{
	/* Identical to, so just call... */
	return CrlGetNextFieldValue(ResultsHandle, Value);
}


void
AppleX509CLSession::IsCertInCachedCrl(
	const CssmData &Cert,
	CSSM_HANDLE CrlHandle,
	CSSM_BOOL &CertFound,
	CssmData &CrlRecordIndex)
{
	unimplemented();
}


void
AppleX509CLSession::CrlAbortCache(
	CSSM_HANDLE CrlHandle)
{
	/* fetch the associated cached CRL, remove from map, delete it */
	CLCachedCRL *cachedCrl = lookupCachedCRL(CrlHandle);
	if(cachedCrl == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_CACHE_HANDLE);
	}
	cacheMap.removeEntry(cachedCrl->handle());
	delete cachedCrl;
}


void
AppleX509CLSession::CrlAbortQuery(
	CSSM_HANDLE ResultsHandle)
{
	/* fetch & validate the query */
	CLQuery *query = queryMap.lookupEntry(ResultsHandle);
	if(query == NULL) {
		CssmError::throwMe(CSSMERR_CL_INVALID_RESULTS_HANDLE);
	}
	if(query->queryType() != CLQ_CRL) {
		clErrorLog("CrlAbortQuery: bad queryType (%d)", (int)query->queryType());
		CssmError::throwMe(CSSMERR_CL_INVALID_RESULTS_HANDLE);
	}

	if(!query->fromCache()) {
		/* the associated cached CRL was created just for this query; dispose */
		CLCachedCRL *cachedCrl = lookupCachedCRL(query->cachedObject());
		if(cachedCrl == NULL) {
			/* should never happen */
			CssmError::throwMe(CSSMERR_CL_INTERNAL_ERROR);
		}
		cacheMap.removeEntry(cachedCrl->handle());
		delete cachedCrl;
	}
	queryMap.removeEntry(query->handle());
	delete query;
}

#pragma mark --- Template ---

void
AppleX509CLSession::CrlCreateTemplate(
	uint32 NumberOfFields,
	const CSSM_FIELD *CrlTemplate,
	CssmData &NewCrl)
{
	unimplemented();
}


void
AppleX509CLSession::CrlSetFields(
	uint32 NumberOfFields,
	const CSSM_FIELD *CrlTemplate,
	const CssmData &OldCrl,
	CssmData &ModifiedCrl)
{
	unimplemented();
}


void
AppleX509CLSession::CrlAddCert(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &Cert,
	uint32 NumberOfFields,
	const CSSM_FIELD CrlEntryFields[],
	const CssmData &OldCrl,
	CssmData &NewCrl)
{
	unimplemented();
}


void
AppleX509CLSession::CrlRemoveCert(
	const CssmData &Cert,
	const CssmData &OldCrl,
	CssmData &NewCrl)
{
	unimplemented();
}


void
AppleX509CLSession::CrlGetAllCachedRecordFields(
	CSSM_HANDLE CrlHandle,
	const CssmData &CrlRecordIndex,
	uint32 &NumberOfFields,
	CSSM_FIELD_PTR &CrlFields)
{
	unimplemented();
}

/* 
 * These are functionally identical to the corresponding
 * Cert functions.
 */
void
AppleX509CLSession::CrlVerifyWithKey(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &CrlToBeVerified)
{
	CertVerifyWithKey(CCHandle, CrlToBeVerified);
}


void
AppleX509CLSession::CrlVerify(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &CrlToBeVerified,
	const CssmData *SignerCert,
	const CSSM_FIELD *VerifyScope,
	uint32 ScopeSize)
{
	CertVerify(CCHandle, CrlToBeVerified, SignerCert, VerifyScope, 
		ScopeSize);
}

void
AppleX509CLSession::CrlSign(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &UnsignedCrl,
	const CSSM_FIELD *SignScope,
	uint32 ScopeSize,
	CssmData &SignedCrl)
{
	unimplemented();
}




