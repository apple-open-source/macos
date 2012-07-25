/*
 * Copyright (c) 2004-2011 Apple Inc. All Rights Reserved.
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
 * crlDb.cpp - CRL cache
 */
#if OCSP_DEBUG
#define OCSP_USE_SYSLOG	1
#endif
#include "crlDb.h"
#include "attachCommon.h"
#include "crlRefresh.h"
#include <security_utilities/utilities.h>
#include <security_utilities/logging.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/threading.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utils/cuDbUtils.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <security_ocspd/ocspdDebug.h>
#include <security_cdsa_client/keychainacl.h>
#include <security_cdsa_client/aclclient.h>
#include <sys/types.h>
#include <sys/stat.h>

#define CRL_CACHE_DB	"/private/var/db/crls/crlcache.db"

#pragma mark ---- OCSPD database singleton ----

class CrlDatabase
{
	NOCOPY(CrlDatabase)
public:
	CrlDatabase();
	~CrlDatabase();

	/* methods associated with public API of this module */
	bool lookup(
		Allocator			&alloc,
		const CSSM_DATA		*url,
		const CSSM_DATA		*issuer,		// optional
		const CSSM_DATA		&verifyTime,
		CSSM_DATA			&crlData);		// allocd in alloc space and RETURNED

	CSSM_RETURN add(
		const CSSM_DATA		&crlData,		// as it came from the server
		const CSSM_DATA		&url);			// where it came from

	void flush(
		const CSSM_DATA 	&url);

	void refresh(
		unsigned			staleDays,
		unsigned			expireOverlapSeconds,
		bool				purgeAll,
		bool				fullCryptoVerify,
		bool				doRefresh);

private:
	CSSM_RETURN openDatabase(
		const char			*dbFileName,
		CSSM_DB_HANDLE		&dbHand,		// RETURNED
		bool				&didCreate);	// RETURNED

	void closeDatabase(
		CSSM_DB_HANDLE		dbHand);

	/* see implementation for comments */
	CSSM_RETURN lookupPriv(
		CSSM_DB_HANDLE				dbHand,
		const CSSM_DATA				*url,
		const CSSM_DATA				*issuer,
		const CSSM_DATA				*verifyTime,
		CSSM_HANDLE_PTR				resultHandPtr,
		CSSM_DB_UNIQUE_RECORD_PTR	*recordPtr,
		CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attrData,
		CSSM_DATA					*crlData);

	/* everything this module does is protected by this global lock */
	Mutex					mLock;

	/*
	 * We maintain open handles to these two modules, but we do NOT maintain
	 * a handle to an open DB - we open and close that as needed for robustness.
	 */
	CSSM_DL_HANDLE			mDlHand;
	CSSM_CL_HANDLE			mClHand;
};

CrlDatabase::CrlDatabase()
	: mDlHand(0), mClHand(0)
{
	/* Attach to DL side of, CL, and CSP */
	mDlHand = attachCommon(&gGuidAppleCSPDL, CSSM_SERVICE_DL);
	if(mDlHand == 0) {
		Syslog::alert("Error loading AppleFileDL");
		CssmError::throwMe(CSSMERR_CSSM_ADDIN_LOAD_FAILED);
	}
	mClHand = attachCommon(&gGuidAppleX509CL, CSSM_SERVICE_CL);
	if(mClHand == 0) {
		Syslog::alert("Error loading AppleX509CL");
		CssmError::throwMe(CSSMERR_CSSM_ADDIN_LOAD_FAILED);
	}
}

/*
 * I believe that this code actually never executes due to ModuleNexus limitations,
 * but it may run someday.
 */
CrlDatabase::~CrlDatabase()
{
	if(mDlHand != 0) {
		detachCommon(&gGuidAppleCSPDL, mDlHand);
	}
	if(mClHand != 0) {
		detachCommon(&gGuidAppleX509CL, mClHand);
	}
}

/*
 * Common code to open or create our DB. If it doesn't exist we'll create it.
 * We never have to unlock it; its password is the same as the filename.
 */
CSSM_RETURN CrlDatabase::openDatabase(
	const char		*dbFileName,
	CSSM_DB_HANDLE	&dbHand,		// RETURNED
	bool			&didCreate)		// RETURNED
{
	didCreate = false;

	/* try to open existing DB */
	CSSM_RETURN crtn = CSSM_DL_DbOpen(mDlHand,
		dbFileName,
		NULL,			// DbLocation
		CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
		NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
		NULL,			// void *OpenParameters
		&dbHand);
	switch(crtn) {
		case CSSM_OK:
			return CSSM_OK;
		case CSSMERR_DL_DATASTORE_DOESNOT_EXIST:
			/* proceed to create it */
			break;
		default:
			ocspdErrorLog("CrlDatabase::openDatabase: CSSM_DL_DbOpen returned %u",
				(unsigned)crtn);
			return crtn;
	}

	/* create new one */
	CSSM_DBINFO	dbInfo;
	memset(&dbInfo, 0, sizeof(CSSM_DBINFO));

	/* password = fileName */
	Security::Allocator &alloc = Security::Allocator::standard();
	CssmClient::AclFactory::PasswordChangeCredentials pCreds((StringData(dbFileName)),
			alloc);
	const AccessCredentials* aa = pCreds;

	/* weird code copied from old crlRefresh. I do not pretend to understand this. */
	TypedList subject(alloc, CSSM_ACL_SUBJECT_TYPE_ANY);
	AclEntryPrototype protoType(subject);
	AuthorizationGroup &authGroup = protoType.authorization();
	CSSM_ACL_AUTHORIZATION_TAG tag = CSSM_ACL_AUTHORIZATION_ANY;
	authGroup.NumberOfAuthTags = 1;
	authGroup.AuthTags = &tag;

	const ResourceControlContext rcc(protoType, const_cast<AccessCredentials *>(aa));

	crtn = CSSM_DL_DbCreate(mDlHand,
		dbFileName,
		NULL,						// DbLocation
		&dbInfo,
		CSSM_DB_ACCESS_PRIVILEGED,
		&rcc,						// CredAndAclEntry
		NULL,						// OpenParameters
		&dbHand);
	if(crtn) {
		ocspdErrorLog("CrlDatabase::openDatabase: CSSM_DL_DbCreate returned %u", (unsigned)crtn);
		return crtn;
	}
	else {
		/* one more thing: make it world readable, only writable by us */
		if(chmod(dbFileName, 0644)) {
			ocspdErrorLog("CrlDatabase::openDatabase: chmod error");
			crtn = CSSMERR_DL_DB_LOCKED;
		}
		else {
			didCreate = true;
		}
	}
	return crtn;
}

void CrlDatabase::closeDatabase(
	CSSM_DB_HANDLE dbHand)
{
	assert(mDlHand != 0);
	if(dbHand != 0) {
		CSSM_DL_DB_HANDLE dlDbHand = {mDlHand, dbHand};
		CSSM_DL_DbClose(dlDbHand);
	}
}

/*
 * Common code to initiate a DataGetFirst op. Query still active on successful
 * return; caller must both CSSM_DL_FreeUniqueRecord() and eventually
 * CSSM_DL_DataAbortQuery.
 */
CSSM_RETURN CrlDatabase::lookupPriv(
	CSSM_DB_HANDLE				dbHand,

	/* all predicate attributes optional */
	const CSSM_DATA				*url,
	const CSSM_DATA				*issuer,
	const CSSM_DATA				*verifyTime,

	/* these two always returned on success */
	CSSM_HANDLE_PTR				resultHandPtr,
	CSSM_DB_UNIQUE_RECORD_PTR	*recordPtr,

	/*
	 * Optionally returned, in/out (in to specify which attrs, out as the
	 * attrs fetched)
	 */
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attrData,

	/* optionally returned - the CRL itself */
	CSSM_DATA					*crlData)
{
	CSSM_QUERY					query;
	CSSM_SELECTION_PREDICATE	pred[4];
	CSSM_SELECTION_PREDICATE	*predPtr = pred;
	CSSM_DL_DB_HANDLE			dlDbHand = {mDlHand, dbHand};

	assert(mDlHand != 0);
	assert(dbHand != 0);
	assert(resultHandPtr != NULL);
	assert(recordPtr != NULL);

	/* zero, one, two, or four predicates...first, the URI */
	if(url) {
		predPtr->DbOperator = CSSM_DB_EQUAL;
		predPtr->Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		predPtr->Attribute.Info.Label.AttributeName = (char*) "URI";
		predPtr->Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
		predPtr->Attribute.Value = const_cast<CSSM_DATA_PTR>(url);
		predPtr->Attribute.NumberOfValues = 1;
		predPtr++;
	}
	if(issuer) {
		predPtr->DbOperator = CSSM_DB_EQUAL;
		predPtr->Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		predPtr->Attribute.Info.Label.AttributeName = (char*) "Issuer";
		predPtr->Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
		predPtr->Attribute.Value = const_cast<CSSM_DATA_PTR>(issuer);
		predPtr->Attribute.NumberOfValues = 1;
		predPtr++;
	}

	if(verifyTime) {
		/*
		 * Before/after: ask for a CRL which is/was valid at verifyTime.
		 * Caller MUST give us a properly formatted CSSM_TIMESTRING or this will fail!
		 */
		predPtr->DbOperator = CSSM_DB_LESS_THAN;
		predPtr->Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		predPtr->Attribute.Info.Label.AttributeName = (char*) "NextUpdate";
		predPtr->Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
		predPtr->Attribute.Value = const_cast<CSSM_DATA_PTR>(verifyTime);
		predPtr->Attribute.NumberOfValues = 1;
		predPtr++;

		predPtr->DbOperator = CSSM_DB_GREATER_THAN;
		predPtr->Attribute.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
		predPtr->Attribute.Info.Label.AttributeName = (char*) "ThisUpdate";
		predPtr->Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
		predPtr->Attribute.Value = const_cast<CSSM_DATA_PTR>(verifyTime);
		predPtr->Attribute.NumberOfValues = 1;
		predPtr++;
	}

	query.RecordType = CSSM_DL_DB_RECORD_X509_CRL;
	query.Conjunctive = CSSM_DB_AND;
	query.NumSelectionPredicates = predPtr - pred;
	query.SelectionPredicate = pred;
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = crlData ? CSSM_QUERY_RETURN_DATA : 0;

	return CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		resultHandPtr,
		attrData,
		crlData,
		recordPtr);
}

/* methods associated with public API of this module */
bool CrlDatabase::lookup(
	Allocator			&alloc,
	const CSSM_DATA		*url,
	const CSSM_DATA		*issuer,
	const CSSM_DATA		&verifyTime,
	CSSM_DATA			&crlData)		// allocd in alloc space and RETURNED
{
	StLock<Mutex> _(mLock);

	CSSM_DB_HANDLE	dbHand;
	bool			didCreate;

	crlData.Data = NULL;
	crlData.Length = 0;

	if(openDatabase(CRL_CACHE_DB, dbHand, didCreate)) {
		/* error: no DB, we're done */
		ocspdErrorLog("CrlDatabase::lookup: no cache DB\n");
		return false;
	}
	if(didCreate) {
		/* we just created empty DB, we're done */
		ocspdCrlDebug("CrlDatabase::lookup: empty cache DB");
		closeDatabase(dbHand);
		return false;
	}

	CSSM_DB_UNIQUE_RECORD_PTR	record = NULL;
	CSSM_DATA					dbCrl = {0, NULL};
	bool						ourRtn = false;
	CSSM_HANDLE					resultHand = 0;
	CSSM_RETURN					crtn;
	CSSM_DL_DB_HANDLE			dlDbHand = {mDlHand, dbHand};

	crtn = lookupPriv(dbHand, url, issuer, &verifyTime, &resultHand, &record, NULL, &dbCrl);
	if(resultHand) {
		CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
	}

	if(record != NULL) {
		if(dbCrl.Data != NULL) {
			crlData.Data = (uint8 *)alloc.malloc(dbCrl.Length);
			crlData.Length = dbCrl.Length;
			memmove(crlData.Data, dbCrl.Data, dbCrl.Length);
			ourRtn = true;
			APP_FREE(dbCrl.Data);
			ocspdCrlDebug("CrlDatabase::lookup: cache HIT");
		}
		else {
			ocspdCrlDebug("CrlDatabase::lookup: DB read succeeded, but no data");
		}
		CSSM_DL_FreeUniqueRecord(dlDbHand, record);

	}
	closeDatabase(dbHand);
	return ourRtn;
}

CSSM_RETURN CrlDatabase::add(
	const CSSM_DATA		&crlData,		// as it came from the server
	const CSSM_DATA		&url)			// where it came from
{
	StLock<Mutex> _(mLock);

	bool didCreate;
	CSSM_DB_HANDLE dbHand;

	if(openDatabase(CRL_CACHE_DB, dbHand, didCreate)) {
		/* no DB, we're done */
		ocspdErrorLog("CrlDatabase::add: no cache DB\n");
		return false;
	}
	CSSM_DL_DB_HANDLE dlDbHand = {mDlHand, dbHand};

	/* this routine takes care of all the attributes, gleaning them from
	 * the contents of the CRL itself */
	CSSM_RETURN crtn = cuAddCrlToDb(dlDbHand, mClHand, &crlData, &url);
	if(crtn) {
		ocspdErrorLog("CrlDatabase::add: error %ld adding CRL to DB\n", (long)crtn);
	}
	else {
		ocspdCrlDebug("CrlDatabase::add: CRL added to DB");
	}
	closeDatabase(dbHand);
	return crtn;
}

/*
 * Flush CRLs associated with specified URL. Called by TP when it detects a
 * bad CRL.
 */
void CrlDatabase::flush(
	const CSSM_DATA 	&url)
{
	StLock<Mutex> _(mLock);

	bool didCreate;
	CSSM_DB_HANDLE dbHand;

	if(openDatabase(CRL_CACHE_DB, dbHand, didCreate)) {
		/* error: no DB, we're done */
		ocspdErrorLog("CrlDatabase::flush: no cache DB\n");
		return;
	}
	if(didCreate) {
		/* we just created empty DB, we're done */
		ocspdCrlDebug("CrlDatabase::flush: empty cache DB");
		closeDatabase(dbHand);
		return;
	}

	CSSM_DB_UNIQUE_RECORD_PTR	record = NULL;
	CSSM_HANDLE					resultHand = 0;
	CSSM_RETURN					crtn;
	CSSM_DL_DB_HANDLE			dlDbHand = {mDlHand, dbHand};

	crtn = lookupPriv(dbHand,
		&url,
		NULL,		// issuer
		NULL,		// verify time
		&resultHand, &record,
		NULL,		// attrs
		NULL);		// data
	if(crtn) {
		ocspdCrlDebug("CrlDatabase::flush: no records found");
		goto done;
	}
	try {
		ocspdCrlDebug("CrlDatabase::flush: deleting a CRL");
		CSSM_DL_DataDelete(dlDbHand, record);
		CSSM_DL_FreeUniqueRecord(dlDbHand, record);

		/* any more? */
		do {
			crtn = CSSM_DL_DataGetNext(dlDbHand, resultHand, NULL, NULL, &record);
			if(crtn) {
				/* done, not found */
				break;
			}
			ocspdCrlDebug("CrlDatabase::flush: deleting a CRL");
			CSSM_DL_DataDelete(dlDbHand, record);
			CSSM_DL_FreeUniqueRecord(dlDbHand, record);
		} while(crtn == CSSM_OK);
		CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
	}
	catch (...) {}; // <rdar://8833413>
done:
	closeDatabase(dbHand);
	return;

}

void CrlDatabase::refresh(
	unsigned			staleDays,
	unsigned			expireOverlapSeconds,
	bool				purgeAll,
	bool				fullCryptoVerify,
	bool				doRefresh)
{
	StLock<Mutex> _(mLock);

	bool didCreate;
	CSSM_DB_HANDLE dbHand;

	if(openDatabase(CRL_CACHE_DB, dbHand, didCreate)) {
		/* error: no DB, we're done */
		ocspdErrorLog("CrlDatabase::refresh: no cache DB\n");
		return;
	}
	if(didCreate) {
		/* we just created empty DB, we're done */
		ocspdCrlDebug("CrlDatabase::refresh: empty cache DB");
		closeDatabase(dbHand);
		return;
	}

	CSSM_DL_DB_HANDLE dlDbHand = {mDlHand, dbHand};
	ocspdCrlRefresh(dlDbHand, mClHand, staleDays, expireOverlapSeconds,
		purgeAll, fullCryptoVerify, doRefresh);
	closeDatabase(dbHand);
	return;
}

static ModuleNexus<CrlDatabase> gCrlDatabase;

#pragma mark ----- Public API -----

/*
 * Lookup cached CRL by URL or issuer, and verifyTime.
 * Just a boolean returned; we found it, or not.
 * Exactly one of {url, issuer} should be non-NULL.
 */
bool crlCacheLookup(
	Allocator			&alloc,
	const CSSM_DATA		*url,
	const CSSM_DATA		*issuer,
	const CSSM_DATA		&verifyTime,
	CSSM_DATA			&crlData)			// allocd in alloc space and RETURNED
{
	return gCrlDatabase().lookup(alloc, url, issuer, verifyTime, crlData);
}

/*
 * Add a CRL response to cache. Incoming response is completely unverified;
 * we just verify that we can parse it.
 */
CSSM_RETURN crlCacheAdd(
	const CSSM_DATA		&crlData,			// as it came from the server
	const CSSM_DATA		&url)				// where it came from
{
	return gCrlDatabase().add(crlData, url);
}

/*
 * Delete any CRL associated with specified URL from cache.
 */
void crlCacheFlush(
	const CSSM_DATA		&url)
{
	return gCrlDatabase().flush(url);
}

/*
 * Purge/refresh the CRL cache.
 */
void crlCacheRefresh(
	unsigned			staleDays,
	unsigned			expireOverlapSeconds,
	bool				purgeAll,
	bool				fullCryptoVerify,
	bool				doRefresh)
{
	gCrlDatabase().refresh(staleDays, expireOverlapSeconds, purgeAll, fullCryptoVerify, doRefresh);
}
