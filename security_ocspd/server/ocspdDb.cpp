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
 * ocspdDb.cpp - OCSP daemon database
 */
 
#include "ocspdDb.h"
#include "attachCommon.h"
#include <security_ocspd/ocspdDbSchema.h>
#include <security_ocspd/ocspdDebug.h>
#include <security_ocspd/ocspResponse.h>
#include <security_ocspd/ocspdUtils.h>
#include <Security/Security.h>
#include <security_utilities/utilities.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/threading.h>
#include <security_utilities/logging.h>
#include <vproc.h>

class TransactionLock
{
private:
	vproc_transaction_t mHandle;

public:
	TransactionLock() {mHandle = vproc_transaction_begin(NULL);}
	~TransactionLock() {vproc_transaction_end(NULL, mHandle);}
};



#define OCSP_DB_FILE		"/private/var/db/crls/ocspcache.db"

/* max default TTL currently 24 hours */
#define OCSPD_CACHE_TTL		(60.0 * 60.0 * 24.0)

#pragma mark ---- OCSPD database singleton ----

class OcspdDatabase
{
	NOCOPY(OcspdDatabase)
public:
	OcspdDatabase();
	~OcspdDatabase();
	
	/* methods associated with public API of this module */
	bool lookup(
		SecAsn1CoderRef		coder,
		const CSSM_DATA		&certID,
		const CSSM_DATA		*localResponder,		// optional
		CSSM_DATA			&derResp);				// RETURNED
	
	void addResponse(
		const CSSM_DATA		&ocspResp,				// DER encoded SecAsn1OCSPResponse
		const CSSM_DATA		&URI);					// where it came from 

	void flushCertID(
		const CSSM_DATA 	&certID);
	
	void flushStale();
	
private:
	CSSM_RETURN dlAttach();
	CSSM_RETURN dbCreate();
	CSSM_RETURN dbOpen(bool doCreate);
	
	/* see implementations for comments */
	bool validateRecord(
		const CSSM_DATA				&certID,
		const CSSM_DATA				&recordData,	// raw OCSP response
		const CSSM_DATA				&expireTime,	// the attribute data
		CSSM_DB_UNIQUE_RECORD_PTR	recordPtr);
		
	CSSM_RETURN lookupPriv(
		/* search predicates, both optional */
		const CSSM_DATA				*certID,		
		const CSSM_DATA				*localResponder,
		
		/* always returned on success */
		CSSM_HANDLE_PTR				resultHand,
		CSSM_DB_UNIQUE_RECORD_PTR	*recordPtr,
		
		/* optionaly returned */
		CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attrData,
		CSSM_DATA_PTR				data);		// i.e., an encoded OCSP response

	
	/* everything this module does is protected by this global lock */
	Mutex					mLock;
	CSSM_DL_DB_HANDLE		mDlDbHandle;
};

OcspdDatabase::OcspdDatabase()
{
	mDlDbHandle.DLHandle = 0;
	mDlDbHandle.DBHandle = 0;
}

/* I believe that this code actually never executes */
OcspdDatabase::~OcspdDatabase()
{
	if(mDlDbHandle.DBHandle != 0) {
		CSSM_DL_DbClose(mDlDbHandle);
		mDlDbHandle.DBHandle = 0;
	}
	if(mDlDbHandle.DLHandle != 0) {
		CSSM_ModuleDetach(mDlDbHandle.DLHandle);
		CSSM_ModuleUnload(&gGuidAppleFileDL, NULL, NULL);
		mDlDbHandle.DLHandle = 0;
	}
}

/* 
 * Ensure we're attached to AppleFileDL. Caller must hold mLock. 
 */
CSSM_RETURN OcspdDatabase::dlAttach()
{
	if(mDlDbHandle.DLHandle != 0) {
		return CSSM_OK;
	}
	ocspdDbDebug("ocspd: attaching to DL");
	mDlDbHandle.DLHandle = attachCommon(&gGuidAppleFileDL, CSSM_SERVICE_DL);
	if(mDlDbHandle.DLHandle == 0) {
		Syslog::alert("Error loading AppleFileDL");
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
	}
	return CSSM_OK;
}

/* 
 * Create the database, which caller has determined does not exist. 
 * Caller must hold mLock.
 */
CSSM_RETURN OcspdDatabase::dbCreate()
{
	assert(mDlDbHandle.DLHandle != 0);
	assert(mDlDbHandle.DBHandle == 0);
	
	ocspdDbDebug("ocspd: creating DB");
	CSSM_DBINFO dbInfo;
	memset(&dbInfo, 0, sizeof(dbInfo));
	dbInfo.NumberOfRecordTypes = 1;
	dbInfo.IsLocal = CSSM_TRUE;		// TBD - what does this mean?
	dbInfo.AccessPath = NULL;		// TBD
	
	/* 
	 * Alloc kNumOcspDbRelations elements for parsingModule, recordAttr, 
	 * and recordIndex info arrays 
	 */
	unsigned size = sizeof(CSSM_DB_PARSING_MODULE_INFO) * kNumOcspDbRelations;
	dbInfo.DefaultParsingModules = (CSSM_DB_PARSING_MODULE_INFO_PTR)malloc(size);
	memset(dbInfo.DefaultParsingModules, 0, size);
	size = sizeof(CSSM_DB_RECORD_ATTRIBUTE_INFO) * kNumOcspDbRelations;
	dbInfo.RecordAttributeNames = (CSSM_DB_RECORD_ATTRIBUTE_INFO_PTR)malloc(size);
	memset(dbInfo.RecordAttributeNames, 0, size);
	size = sizeof(CSSM_DB_RECORD_INDEX_INFO) * kNumOcspDbRelations;
	dbInfo.RecordIndexes = (CSSM_DB_RECORD_INDEX_INFO_PTR)malloc(size);
	memset(dbInfo.RecordIndexes, 0, size);
	
	/* cook up attribute and index info for each relation */
	unsigned relation;
	for(relation=0; relation<kNumOcspDbRelations; relation++) {
		const OcspdDbRelationInfo *relp = &kOcspDbRelations[relation];	// source
		CSSM_DB_RECORD_ATTRIBUTE_INFO_PTR attrInfo = 
			&dbInfo.RecordAttributeNames[relation];						// dest 1
		CSSM_DB_RECORD_INDEX_INFO_PTR indexInfo = 
			&dbInfo.RecordIndexes[relation];							// dest 2
			
		attrInfo->DataRecordType = relp->recordType;
		attrInfo->NumberOfAttributes = relp->numberOfAttributes;
		attrInfo->AttributeInfo = 
			const_cast<CSSM_DB_ATTRIBUTE_INFO_PTR>(relp->attrInfo);
		
		indexInfo->DataRecordType = relp->recordType;
		indexInfo->NumberOfIndexes = relp->numIndexes;
		indexInfo->IndexInfo = const_cast<CSSM_DB_INDEX_INFO_PTR>(relp->indexInfo);
	}

	/* autocommit and mode */
	CSSM_APPLEDL_OPEN_PARAMETERS openParams;
	memset(&openParams, 0, sizeof(openParams));
	openParams.length = sizeof(openParams);
	openParams.version = CSSM_APPLEDL_OPEN_PARAMETERS_VERSION;
	openParams.autoCommit = CSSM_TRUE;
	/* ensure mode 644 */
	openParams.mask = kCSSM_APPLEDL_MASK_MODE;
	openParams.mode = 0644;
	
	CSSM_RETURN crtn;
	crtn = CSSM_DL_DbCreate(mDlDbHandle.DLHandle,
		OCSP_DB_FILE,		// DbName is the same as file path 
		NULL,				// DbLocation
		&dbInfo,
		CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
		NULL,				// CredAndAclEntry
		&openParams, 
		&mDlDbHandle.DBHandle);
	if(crtn) {
		Syslog::alert("Error creating DB");
		#ifndef	NDEBUG
		cssmPerror("CSSM_DL_DbCreate", crtn);
		#endif
	}
	free(dbInfo.DefaultParsingModules);
	free(dbInfo.RecordAttributeNames);
	free(dbInfo.RecordIndexes);
	return crtn;
}

/* 
 * Open, optionally creating. If !doCreate and DB doesn't exist, 
 * CSSMERR_DL_DATASTORE_DOESNOT_EXIST is returned. Any other error is 
 * a gross failure. 
 *
 * Caller must hold mLock.
 */
CSSM_RETURN OcspdDatabase::dbOpen(bool doCreate)
{
	/* first ensure we're attached to the DL */
	CSSM_RETURN crtn = dlAttach();
	if(crtn) {
		return crtn;
	}
	if(mDlDbHandle.DBHandle != 0) {
		return CSSM_OK;
	}
	crtn = CSSM_DL_DbOpen(mDlDbHandle.DLHandle,
		OCSP_DB_FILE, 
		NULL,			// DbLocation
		CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
		NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
		NULL,			// void *OpenParameters
		&mDlDbHandle.DBHandle);
	switch(crtn) {
		case CSSM_OK:
			return CSSM_OK;
		case CSSMERR_DL_DATASTORE_DOESNOT_EXIST:
			if(!doCreate) {
				/* just trying to read, doesn't exist */
				ocspdDbDebug("ocspd: read access, DB does not yet exist");
				return crtn;
			}
			else {
				return dbCreate();
			}
		default:
			Syslog::alert("Error opening DB");
			#ifndef	NDEBUG
			cssmPerror("CSSM_DL_DbOpen", crtn);
			#endif
			return crtn;
	}
}
	
/* 
 * Free CSSM_DB_ATTRIBUTE_DATA
 */
static void freeAttrData(
	CSSM_DB_ATTRIBUTE_DATA &attrData)
{
	if(attrData.Value == NULL) {
		return;
	}
	for(unsigned dex=0; dex<attrData.NumberOfValues; dex++) {
		CSSM_DATA_PTR d = &attrData.Value[dex];
		if(d->Data) {
			APP_FREE(d->Data);
		}
	}
	APP_FREE(attrData.Value);
}

/*
 * Validate a record found in the DB. Returns true if record is good to go.
 * We delete the record if it's stale. 
 */
bool OcspdDatabase::validateRecord(
	const CSSM_DATA				&certID,
	const CSSM_DATA				&recordData,	// raw OCSP response
	const CSSM_DATA				&expireTime,	// the attribute data
	CSSM_DB_UNIQUE_RECORD_PTR	recordPtr)
{
	/* 
	 * First off, if the entire record is stale, we're done.
	 */
	CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
	CFAbsoluteTime cfExpireTime = genTimeToCFAbsTime(&expireTime);
	if(now >= cfExpireTime) {
		ocspdDbDebug("OcspdDatabase::validateRecord: record EXPIRED");
		CSSM_DL_DataDelete(mDlDbHandle, recordPtr);
		return false;
	}
	
	/* 
	 * Parse: this should never fail since we did this when we first got the 
	 * response, and we just made sure it isn't stale. 
	 */
	OCSPResponse *ocspResp = NULL;
	bool ourRtn = false;
	try {
		ocspResp = new OCSPResponse(recordData, OCSPD_CACHE_TTL);
	}
	catch(...) {
		Syslog::alert("Error parsing stored record");
		return false;
	}
	
	/* 
	 * Find the singleResponse for this certID.
	 */
	OCSPSingleResponse *singleResp = ocspResp->singleResponseFor(certID);
	if(singleResp != NULL) {
		CFAbsoluteTime nextUpdate = singleResp->nextUpdate();
		/* SingleResponses with no nextUpdate goes stale with the response as a whole */
		if((nextUpdate != NULL_TIME) && (now > nextUpdate)) {
			ocspdDbDebug("OcspdDatabase::validateRecord: SingleResponse EXPIRED");
		}
		else {
			ourRtn = true;
		}
		delete singleResp;
	}
	else {
		/* Not sure how this could happen */
		ocspdDbDebug("OcspdDatabase::validateRecord: SingleResponse NOT FOUND");
	}
	delete ocspResp;
	ocspdDbDebug("OcspdDatabase::validateRecord returning %s", 
		ourRtn ? "TRUE" : "FALSE");
	return ourRtn;
}

/* 
 * Basic private lookup. Key on any or all attrs. On success, always returns a 
 * CSSM_DB_UNIQUE_RECORD_PTR (which caller must either use to delete the 
 * record or free with CSSM_DL_FreeUniqueRecord()) *AND* a CSSM_HANDLE_PTR
 * ResultHandle(which caller must free with CSSM_DL_DataAbortQuery, possibly
 * after doing some CSSM_DL_DataGetNext() ops with it). On success, also
 * optionally returns CSSM_DB_RECORD_ATTRIBUTE_DATA which caller must free
 * via APP_FREE().
 *
 * Caller holds mLock and has ensured the the DB is open.
 */
CSSM_RETURN OcspdDatabase::lookupPriv(
	/* search predicates, both optional */
	const CSSM_DATA		*certID,		
	const CSSM_DATA		*URI,
	
	/* these two always returned on success */
	CSSM_HANDLE_PTR				resultHandPtr,
	CSSM_DB_UNIQUE_RECORD_PTR	*recordPtr,
	
	/* 
	 * Optionally returned, in/out (in to specify which attrs, out as the 
	 * attrs fetched) 
	 */
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attrData,
	
	/* optionally returned - an encoded OCSP response */
	CSSM_DATA_PTR				data)	
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate[2];
	CSSM_SELECTION_PREDICATE		*predPtr = predicate;
	CSSM_DB_ATTRIBUTE_INFO			certIDAttr = OCSPD_DBATTR_CERT_ID;
	CSSM_DB_ATTRIBUTE_INFO			uriAttr = OCSPD_DBATTR_URI;

	assert(resultHandPtr != NULL);
	assert(recordPtr != NULL);
	assert(mDlDbHandle.DBHandle != 0);
	
	memset(&query, 0, sizeof(query));
	query.RecordType = OCSPD_DB_RECORDTYPE;
	query.Conjunctive = CSSM_DB_NONE;
	
	if(certID) {
		predPtr->DbOperator = CSSM_DB_EQUAL;
		predPtr->Attribute.Info = certIDAttr;
		predPtr->Attribute.NumberOfValues = 1;
		predPtr->Attribute.Value = const_cast<CSSM_DATA_PTR>(certID);
		query.NumSelectionPredicates++;
		query.SelectionPredicate = predicate;
		predPtr++;
	}
	if(URI) {
		predPtr->DbOperator = CSSM_DB_EQUAL;
		predPtr->Attribute.Info = uriAttr;
		predPtr->Attribute.NumberOfValues = 1;
		predPtr->Attribute.Value = const_cast<CSSM_DATA_PTR>(URI);
		query.NumSelectionPredicates++;
		query.SelectionPredicate = predicate;
	}
	if(data) {
		query.QueryFlags = CSSM_QUERY_RETURN_DATA;	// FIXME - used?
	}
	if(query.NumSelectionPredicates > 1) {
		query.Conjunctive = CSSM_DB_AND;
	}
	return CSSM_DL_DataGetFirst(mDlDbHandle, &query, resultHandPtr,
		attrData, data, recordPtr);
}

/* methods associated with public API of this module */
bool OcspdDatabase::lookup(
	SecAsn1CoderRef		coder,
	const CSSM_DATA		&certID,
	const CSSM_DATA		*URI,			// optional
	CSSM_DATA			&derResp)		// RETURNED
{
	TransactionLock tLock;
	StLock<Mutex> _(mLock);
	if(dbOpen(false)) {
		return false;
	}
	
	CSSM_RETURN crtn;
	CSSM_HANDLE resultHand;
	CSSM_DB_UNIQUE_RECORD_PTR recordPtr;
	CSSM_DATA resultData;			// if found, free via APP_FREE()
	bool foundIt = false;
	
	/* set up to retrieve just the expiration time */
	CSSM_DB_RECORD_ATTRIBUTE_DATA recordAttrData;
	memset(&recordAttrData, 0, sizeof(recordAttrData));
	CSSM_DB_ATTRIBUTE_DATA attrData;
	memset(&attrData, 0, sizeof(attrData));
	CSSM_DB_ATTRIBUTE_INFO expireInfo = OCSPD_DBATTR_EXPIRATION;
	attrData.Info = expireInfo;
	recordAttrData.DataRecordType = OCSPD_DB_RECORDTYPE;
	recordAttrData.NumberOfAttributes = 1;
	recordAttrData.AttributeData = &attrData;
	
	crtn = lookupPriv(&certID, URI, &resultHand, &recordPtr,
		&recordAttrData, &resultData);
	if(crtn) {
		ocspdDbDebug("OcspdDatabase::lookup: MISS");
		return false;
	}
	foundIt = validateRecord(certID, resultData, *attrData.Value, recordPtr);
	/* done with attrs and the record itself regardless.... */
	freeAttrData(attrData);
	CSSM_DL_FreeUniqueRecord(mDlDbHandle, recordPtr);
	
	if(!foundIt) {
		/* no good. free what we just got and try again */
		ocspdDbDebug("OcspdDatabase::lookup: invalid record (1)");
		if(resultData.Data) {
			APP_FREE(resultData.Data);
			resultData.Data = NULL;
			resultData.Length = 0;
		}
		do {
			crtn = CSSM_DL_DataGetNext(mDlDbHandle, resultHand,
				&recordAttrData, &resultData, &recordPtr);
			if(crtn) {
				/* done, not found */
				break;
			}
			foundIt = validateRecord(certID, resultData, *attrData.Value, recordPtr);
			ocspdDbDebug("OcspdDatabase::lookup: %s",
				foundIt ? "HIT (2)" : "invalid record (2)");
			freeAttrData(attrData);
			CSSM_DL_FreeUniqueRecord(mDlDbHandle, recordPtr);
			if(!foundIt && (resultData.Data != NULL)) {
				APP_FREE(resultData.Data);
				resultData.Data = NULL;
				resultData.Length = 0;
			}
		/* break on full success, or end of DB search */
		} while(!foundIt && (crtn == CSSM_OK));
	}
	else {
		ocspdDbDebug("OcspdDatabase::lookup: HIT (1)");
	}
	CSSM_DL_DataAbortQuery(mDlDbHandle, resultHand);
	if(foundIt) {
		assert(resultData.Data != NULL);
		SecAsn1AllocCopyItem(coder, &resultData, &derResp);
		APP_FREE(resultData.Data);
	}
	return foundIt;
}

void OcspdDatabase::addResponse(
	const CSSM_DATA		&ocspResp,				// DER encoded SecAsn1OCSPResponse
	const CSSM_DATA		&URI)	
{
	TransactionLock tLock;
	StLock<Mutex> _(mLock);
	ocspdDbDebug("addResponse: top");
	if(dbOpen(true)) {
		return;
	}
	
	/* open it up... */
	OCSPResponse *resp = NULL;
	try {
		resp = new OCSPResponse(ocspResp, OCSPD_CACHE_TTL);
	}
	catch(...) {
		ocspdDbDebug("addResponse: error parsing response");
		return;
	}
	if(resp->responseStatus() != RS_Success) {
		/* e.g., RS_Unauthorized */
		ocspdDbDebug("addResponse: responseStatus %d, aborting", (int)resp->responseStatus());
		delete resp;
		return;
	}
	
	/*
	 * Get expiration date in the form of the latest of all of the enclosed
	 * SingleResponse nextUpdate fields.
	 */
	CFAbsoluteTime expireTime = resp->expireTime();
	char expireStr[GENERAL_TIME_STRLEN+1];
	cfAbsTimeToGgenTime(expireTime, expireStr);
	CSSM_DATA expireData = {GENERAL_TIME_STRLEN, (uint8 *)expireStr};
	CSSM_RETURN crtn;
	
	CSSM_DB_RECORD_ATTRIBUTE_DATA recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA attrData[OCSPD_NUM_DB_ATTRS];
	CSSM_DB_UNIQUE_RECORD_PTR recordPtr = NULL;
	memset(&recordAttrs, 0, sizeof(recordAttrs));

	recordAttrs.DataRecordType = OCSPD_DB_RECORDTYPE;
	recordAttrs.SemanticInformation = 0;			// what's this for?
	recordAttrs.NumberOfAttributes = OCSPD_NUM_DB_ATTRS;
	recordAttrs.AttributeData = attrData;
	
	/*
	 * Now fill in the attributes. CertID is unusual in that it can contain multiple
	 * values, one for each SingleResponse.
	 */
	SecAsn1CoderRef coder;			// for tons of mallocs
	SecAsn1CoderCreate(&coder);
	const SecAsn1OCSPResponseData &respData = resp->responseData();
	unsigned numSingleResps = ocspdArraySize((const void **)respData.responses);
	CSSM_DB_ATTRIBUTE_INFO oneAttr = OCSPD_DBATTR_CERT_ID;
	CSSM_DB_ATTRIBUTE_INFO twoAttr = OCSPD_DBATTR_URI;
	CSSM_DB_ATTRIBUTE_INFO threeAttr = OCSPD_DBATTR_EXPIRATION;
	attrData[0].Info = oneAttr;
	attrData[0].NumberOfValues = numSingleResps;
	#ifndef	NDEBUG
	if(numSingleResps > 1) {
		ocspdDbDebug("addResponse: MULTIPLE SINGLE RESPONSES (%u)", numSingleResps);
	}
	#endif
	attrData[0].Value = (CSSM_DATA_PTR)SecAsn1Malloc(coder, 
		numSingleResps * sizeof(CSSM_DATA));
	memset(attrData[0].Value, 0, numSingleResps * sizeof(CSSM_DATA));
	for(unsigned dex=0; dex<numSingleResps; dex++) {
		/* 
		 * Get this single response. SKIP IT if the hash algorithm is not 
		 * SHA1 since we do lookups in the DB by encoded value assuming SHA1
		 * hash. Incoming responses with other hash values would never be found.
		 */
		SecAsn1OCSPSingleResponse *resp = respData.responses[dex];
		SecAsn1OCSPCertID &certID = resp->certID;
		if(!ocspdCompareCssmData(&certID.algId.algorithm, &CSSMOID_SHA1)) {
			ocspdDbDebug("addResponse: SKIPPING resp due to nonstandard hash alg");
			attrData[0].NumberOfValues--;
			continue;
		}
		/* encode this certID as attr[0]value[dex] */
		if(SecAsn1EncodeItem(coder, &certID, kSecAsn1OCSPCertIDTemplate, 
				&attrData[0].Value[dex])) {
			ocspdErrorLog("OcspdDatabase::addResponse: encode error\n");
			crtn = CSSMERR_TP_INTERNAL_ERROR;
			goto errOut;
		}
	}

	attrData[1].Info = twoAttr;
	attrData[1].NumberOfValues = 1;
	attrData[1].Value = const_cast<CSSM_DATA_PTR>(&URI);	
	
	attrData[2].Info = threeAttr;
	attrData[2].NumberOfValues = 1;
	attrData[2].Value = &expireData;	

	crtn = CSSM_DL_DataInsert(mDlDbHandle,
		OCSPD_DB_RECORDTYPE,
		&recordAttrs,
		&ocspResp,
		&recordPtr);
	if(crtn) {
		Syslog::alert("Error writing to DB");
		#ifndef	NDEBUG
		cssmPerror("CSSM_DL_DbOpen", crtn);
		#endif
	}
	else {
		CSSM_DL_FreeUniqueRecord(mDlDbHandle, recordPtr);
	}
errOut:
	delete resp;
	SecAsn1CoderRelease(coder);
}

void OcspdDatabase::flushCertID(
	const CSSM_DATA 	&certID)
{
	TransactionLock tLock;
	StLock<Mutex> _(mLock);
	if(dbOpen(false)) {
		return;
	}
	
	CSSM_RETURN crtn;
	CSSM_HANDLE resultHand;
	CSSM_DB_UNIQUE_RECORD_PTR recordPtr;
	
	/* just retrieve the record, no attrs, no data */
	crtn = lookupPriv(&certID, NULL, &resultHand, &recordPtr, NULL, NULL);
	if(crtn) {
		ocspdDbDebug("OcspdDatabase::flushCertID: no such record");
		return;
	}
	try {
		ocspdDbDebug("OcspdDatabase::flushCertID: deleting (1)");
		CSSM_DL_DataDelete(mDlDbHandle, recordPtr);
		CSSM_DL_FreeUniqueRecord(mDlDbHandle, recordPtr);
		
		/* any more? */
		do {
			crtn = CSSM_DL_DataGetNext(mDlDbHandle, resultHand,	NULL, NULL, &recordPtr);
			if(crtn) {
				/* done, not found */
				break;
			}
			ocspdDbDebug("OcspdDatabase::flushCertID: deleting (2)");
			CSSM_DL_DataDelete(mDlDbHandle, recordPtr);
			CSSM_DL_FreeUniqueRecord(mDlDbHandle, recordPtr);
		} while(crtn == CSSM_OK);
		CSSM_DL_DataAbortQuery(mDlDbHandle, resultHand);
	}
	catch (...) {}; // <rdar://8833413>
	return;
}

void OcspdDatabase::flushStale()
{
	TransactionLock tLock;
	StLock<Mutex> _(mLock);
	if(dbOpen(false)) {
		return;
	}
	
	CSSM_RETURN crtn;
	CSSM_HANDLE resultHand;
	CSSM_DB_UNIQUE_RECORD_PTR recordPtr;
	
	CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();

	/* retrieve all records, one attr (expiration time), no data */
	CSSM_DB_RECORD_ATTRIBUTE_DATA recordAttrData;
	memset(&recordAttrData, 0, sizeof(recordAttrData));
	CSSM_DB_ATTRIBUTE_DATA attrData;
	memset(&attrData, 0, sizeof(attrData));
	CSSM_DB_ATTRIBUTE_INFO expireInfo = OCSPD_DBATTR_EXPIRATION;
	attrData.Info = expireInfo;
	recordAttrData.DataRecordType = OCSPD_DB_RECORDTYPE;
	recordAttrData.NumberOfAttributes = 1;
	recordAttrData.AttributeData = &attrData;

	crtn = lookupPriv(NULL, NULL, &resultHand, &recordPtr, &recordAttrData, NULL);
	if(crtn) {
		ocspdDbDebug("OcspdDatabase::flushStale: no records found");
		return;
	}
	try {
		CFAbsoluteTime cfExpireTime = genTimeToCFAbsTime(attrData.Value);
		if(now >= cfExpireTime) {
			ocspdDbDebug("OcspdDatabase::flushStale: record EXPIRED");
			CSSM_DL_DataDelete(mDlDbHandle, recordPtr);
		}
		CSSM_DL_FreeUniqueRecord(mDlDbHandle, recordPtr);
		
		/* any more? */
		do {
			crtn = CSSM_DL_DataGetNext(mDlDbHandle, resultHand,	NULL, NULL, &recordPtr);
			if(crtn) {
				/* done, not found */
				break;
			}
			cfExpireTime = genTimeToCFAbsTime(attrData.Value);
			if(now >= cfExpireTime) {
				ocspdDbDebug("OcspdDatabase::flushStale: record EXPIRED");
				CSSM_DL_DataDelete(mDlDbHandle, recordPtr);
			}
			CSSM_DL_FreeUniqueRecord(mDlDbHandle, recordPtr);
		} while(crtn == CSSM_OK);
		CSSM_DL_DataAbortQuery(mDlDbHandle, resultHand);
	}
	catch (...) {}; // <rdar://8833413>
	return;
}

static ModuleNexus<OcspdDatabase> gOcspdDatabase;


#pragma mark ---- Public API ----

/*
 * Lookup cached response. Result is a DER-encoded OCSP response,t he same bits
 * originally obtained from the net. Result is allocated in specified 
 * SecAsn1CoderRef's memory. Never returns a stale entry; we always check the 
 * enclosed SingleResponse for temporal validity. 
 *
 * Just a boolean returned; we found it, or not.
 */
bool ocspdDbCacheLookup(
	SecAsn1CoderRef		coder,
	const CSSM_DATA		&certID,
	const CSSM_DATA		*localResponder,		// optional
	CSSM_DATA			&derResp)				// RETURNED
{
	return gOcspdDatabase().lookup(coder, certID, localResponder, derResp);
}

/* 
 * Add a OCSP response to cache. Incoming response is completely unverified;
 * we just verify that we can parse it and is has at least one SingleResponse
 * which is temporally valid. 
 */
void ocspdDbCacheAdd(
	const CSSM_DATA		&ocspResp,			// DER encoded SecAsn1OCSPResponse
	const CSSM_DATA		&URI)				// where it came from 
{
	gOcspdDatabase().addResponse(ocspResp, URI);
}

/*
 * Delete any entry associated with specified certID from cache.
 */
void ocspdDbCacheFlush(
	const CSSM_DATA 	&certID)
{
	gOcspdDatabase().flushCertID(certID);
}

/*
 * Flush stale entries from cache. 
 */
void ocspdDbCacheFlushStale()
{
	gOcspdDatabase().flushStale();
}


