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
 * crlRefresh.cpp - CRL cache refresh and update logic
 */

/*
 * Examine the CRLs in the system CRL cache, looking for expired CRLs
 * for which we don't have current valid entries. Perform net fetch for
 * all such entries to get up-to-date entries. Purge entries older
 * than specified date (i.e., "stale" CRLs). 
 *
 * Terminology used here:
 *
 * 'nowTime' is the absolute current time.
 * 'updateTime' is the time at which we evaluate a CRL's NextUpdate
 *     attribute to determine whether a CRL has expired. This is 
 *     generally subsequent to nowTime. 
 * 'expired' means that a CRL's NextUpdate time has passed, relative
 *    to updateTime, and that we need to fetch a new CRL to replace
 *    the expired CRL.
 * 'expireOverlap' is (updateTime - nowTime) in seconds. It's the 
 *    distance into the future at which we evaluate a CRL's expiration
 *    status.
 * 'stale' means that a CRL is so old that it should be deleted from
 *    the cache. 
 * 'staleTime' is maximum age (relative to nowTime) that a CRL can
 *    achieve in cache before being deemed stale. StaleTime is always
 *    greater than expireOverlap (i.e., if a CRL is stale, it MUST be
 *    expired, but a CRL can be expired without being stale). 
 *
 * CRLs are only deleted from cache if they are stale; multiple 
 * CRLs from one CA may exist in cache at a given time but (generally)
 * only one of them is not expired.
 *
 * expireOverlap and staleTime have defaults which can be overridden
 * via RPC arguments. 
 */

#include "crlRefresh.h"
#include "attachCommon.h"
#include "ocspdNetwork.h"

#include <security_ocspd/ocspdDebug.h>
#include <assert.h>
#include <Security/cssmapi.h>
#include <Security/SecTrust.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utils/cuDbUtils.h>
#include <security_cdsa_utils/cuTimeStr.h>
#include <security_utilities/logging.h>
#include <Security/TrustSettingsSchema.h>

#define DEFAULT_STALE_DAYS				10
#define DEFAULT_EXPIRE_OVERLAP_SECONDS	3600

#define SECONDS_PER_DAY					(60 * 60 * 24)

#define CRL_CACHE_DB	"/var/db/crls/crlcache.db"

#pragma mark ----- DB attribute stuff -----

/* declare a CSSM_DB_ATTRIBUTE_INFO with NAME_AS_STRING */
#define DB_ATTRIBUTE(name, type) \
	{  CSSM_DB_ATTRIBUTE_NAME_AS_STRING, \
	   {#name}, \
	   CSSM_DB_ATTRIBUTE_FORMAT_ ## type \
	}

/* The CRL DB attributes we care about*/
/* Keep these positions in sync with ATTR_DEX_xxx, below */
static const CSSM_DB_ATTRIBUTE_INFO x509CrlRecordAttrs[] = {
	DB_ATTRIBUTE(CrlType, UINT32),			// 0
	DB_ATTRIBUTE(CrlEncoding, UINT32),		// 1
	DB_ATTRIBUTE(PrintName, BLOB),			// 2
	DB_ATTRIBUTE(Issuer, BLOB),				// 3
	DB_ATTRIBUTE(NextUpdate, BLOB),			// 4
	DB_ATTRIBUTE(URI, BLOB),				// 5
	
	/* we don't use these */
	// DB_ATTRIBUTE(ThisUpdate, BLOB),			// 4
	// DB_ATTRIBUTE(DeltaCrlNumber, UINT32)
	// DB_ATTRIBUTE(Alias, BLOB),
	// DB_ATTRIBUTE(CrlNumber, UINT32),
};

#define NUM_CRL_ATTRS	\
	(sizeof(x509CrlRecordAttrs) / sizeof(x509CrlRecordAttrs[0]))

#define ATTR_DEX_CRL_TYPE		0
#define ATTR_DEX_CRL_ENC		1
#define ATTR_DEX_PRINT_NAME		2
#define ATTR_DEX_ISSUER			3
#define ATTR_DEX_NEXT_UPDATE	4
#define ATTR_DEX_URI			5

/* free attribute(s) allocated by DL */
static void freeAttrs(
	CSSM_DB_ATTRIBUTE_DATA 	*attrs,
	unsigned				numAttrs)
{
	unsigned i;
	
	for(i=0; i<numAttrs; i++) {
		CSSM_DB_ATTRIBUTE_DATA_PTR attrData = &attrs[i];
		unsigned j;
		for(j=0; j<attrData->NumberOfValues; j++) {
			CSSM_DATA_PTR data = &attrData->Value[j];
			if(data == NULL) {
				/* fault of DL, who said there was a value here */
				ocspdErrorLog("***freeAttrs screwup: NULL data\n");
				return;
			}
			APP_FREE(data->Data);
			data->Data = NULL;
			data->Length = 0;
		}
		APP_FREE(attrData->Value);
		attrData->Value = NULL;
	}
}

/*
 * Compare two CSSM_TIMESTRINGs. Returns:
 * -1 if t1 <  t2
 *  0 if t1 == t2
 *  1 if t1 >  t2
 */
static int compareTimes(
	const char *t1,
	const char *t2)
{
	for(unsigned dex=0; dex<CSSM_TIME_STRLEN; dex++, t1++, t2++) {
		if(*t1 > *t2) {
			return 1;
		}
		if(*t1 < *t2) {
			return -1;
		}
		/* else same, on to next byte */
	}
	/* equal */
	return 0;
}

#ifndef	NDEBUG

static void printString(const char *prefixStr, const CSSM_DATA *val)
{
	/* make a C string */
	unsigned len = val->Length;
	char *s = (char *)malloc(len + 1);
	memmove(s, val->Data, len);
	s[len] = '\0';
	ocspdCrlDebug("%s: %s", prefixStr, s);
	free(s);
}

#else
#define printString(p, s)
#endif

#pragma mark ----- CrlInfo class -----

/*
 * everything we know or care about a CRL.
 */
class CrlInfo
{
public:
	CrlInfo(
		CSSM_DL_DB_HANDLE 			dlDbHand,
		CSSM_DB_ATTRIBUTE_DATA 		*attrData,		// [NUM_CRL_ATTRS]
		CSSM_DB_UNIQUE_RECORD_PTR	record,
		CSSM_DATA_PTR				crlBlob);		// optional
	~CrlInfo();
	
	CSSM_DATA_PTR 			fetchValidAttr(
								unsigned attrDex);
	int 					fetchIntAttr(
								unsigned dex, 
								uint32 &rtn);
	
	bool					isSameIssuer(
								CrlInfo *other);
	void					printName(const char *prefixStr);
	
	void 					validateTimes(
								const char *updateTime,
								const char *staleTime,
								unsigned dex);
								
	/* state inferred from attributes, and maintained by 
	 * owner (not by us) */
	bool						mIsBadlyFormed;	// general parse error
	bool						mIsExpired;		// compare to 'now'
	bool						mIsStale;		// compared to "staleTime'
	bool						mRefreshed;		// already refreshed

	/*
	 * Actual CRL, optionally fetched from DB if doing a full crypto verify
	 */
	CSSM_DATA					mCrlBlob;
	
	
	/* accessors for read-only member vars */
	CSSM_DL_DB_HANDLE 			dlDbHand()	{ return mDlDbHand; }
	CSSM_DB_ATTRIBUTE_DATA_PTR 	attrData()	{ return &mAttrData[0]; }
	CSSM_DB_UNIQUE_RECORD_PTR	record()	{ return mRecord; };

private:
	/* member variables which are read-only subsequent to construction */
	CSSM_DL_DB_HANDLE 		mDlDbHand;
	
	/*
	 * array of attr data 
	 * contents APP_MALLOCd by DL
	 * contents APP_FREEd by our destructor
	 */
	CSSM_DB_ATTRIBUTE_DATA 		mAttrData[NUM_CRL_ATTRS];
	
	/*
	 * For possible use in CSSM_DL_DataDelete
	 * Our destructor does CSSM_DL_FreeUniqueRecord
	 */
	CSSM_DB_UNIQUE_RECORD_PTR	mRecord;
};

CrlInfo::CrlInfo(
	CSSM_DL_DB_HANDLE 			dlDbHand,
	CSSM_DB_ATTRIBUTE_DATA 		*attrData,		// [NUM_CRL_ATTRS]
	CSSM_DB_UNIQUE_RECORD_PTR	record,
	CSSM_DATA_PTR				crlBlob)		// optional
	: mIsBadlyFormed(false),
	  mIsExpired(false),
	  mIsStale(false),
	  mRefreshed(false),
	  mDlDbHand(dlDbHand),
	  mRecord(record)
{
	if(crlBlob) {
		mCrlBlob = *crlBlob;
	}
	else {
		mCrlBlob.Data = NULL;
		mCrlBlob.Length = 0;
	}
	memmove(mAttrData, attrData, 
		sizeof(CSSM_DB_ATTRIBUTE_DATA) * NUM_CRL_ATTRS);
}

CrlInfo::~CrlInfo()
{
	freeAttrs(&mAttrData[0], NUM_CRL_ATTRS);
	CSSM_DL_FreeUniqueRecord(mDlDbHand, mRecord);
	if(mCrlBlob.Data) {
		APP_FREE(mCrlBlob.Data);
	}
}

/* 
 * Is attribute at specified index present with one value? Returns the 
 * value if so, else returns NULL. 
 */
CSSM_DATA_PTR CrlInfo::fetchValidAttr(
	unsigned attrDex)
{
	if(mAttrData[attrDex].NumberOfValues != 1) {
		return NULL;
	}
	return mAttrData[attrDex].Value;
}

/* 
 * Fetch uint32 attr if it's there at specified attr index.
 * Returns non zero if it's not there and flags the CRL as bad.
 */
int CrlInfo::fetchIntAttr(
	unsigned	dex,
	uint32		&rtn)
{
	CSSM_DATA *val = fetchValidAttr(dex);
	if((val == NULL) || (val->Length != sizeof(uint32))) {
		ocspdErrorLog("CrlInfo::fetchIntAttr: Badly formed uint32 attr at dex %u\n", 
			dex);
		mIsBadlyFormed = true;
		return 1;
	}
	rtn = *(uint32 *)val->Data;
	return 0;
}


/*
 * See if two CRLs have same issuer. Requires (and verifies) that both 
 * issuer attrs are well formed.
 */
bool CrlInfo::isSameIssuer(
	CrlInfo *other)
{
	CSSM_DATA_PTR thisIssuer = fetchValidAttr(ATTR_DEX_ISSUER);
	if(thisIssuer == NULL) {
		return false;
	}
	CSSM_DATA_PTR otherIssuer = other->fetchValidAttr(ATTR_DEX_ISSUER);
	if(otherIssuer == NULL) {
		return false;
	}
	return cuCompareCssmData(thisIssuer, otherIssuer) ? true : false;
}


void CrlInfo::printName(const char *prefixStr)
{
	#ifndef	NDEBUG
	CSSM_DATA_PTR val = fetchValidAttr(ATTR_DEX_PRINT_NAME);
	if(val == NULL) {
		ocspdCrlDebug("%s: X509 CRL <no name>", prefixStr);
	}
	else {
		printString(prefixStr, val);
	}
	#endif
}


/*
 * Given time strings representing 'update time' and 'stale time', 
 * calculate mIsExpired and mIsStale. 
 */
void CrlInfo::validateTimes(
	const char *updateTime,		// now - expireOverlap
	const char *staleTime,		// now - staleTime
	unsigned dex)				// for debug info
{
	CSSM_DATA *nextUpdateData = fetchValidAttr(ATTR_DEX_NEXT_UPDATE);
	if((nextUpdateData == NULL) || 
		(nextUpdateData->Length != CSSM_TIME_STRLEN)) {
		ocspdErrorLog("CrlInfo::validateTimes: Badly formed NextUpdate attr on "
			"CRL %u", dex);
		mIsBadlyFormed = true;
		return;
	}
	printString("NextUpdate ", nextUpdateData); 
	char *nextUpdate = (char *)nextUpdateData->Data;
	if(compareTimes(nextUpdate, updateTime) < 0) {
		ocspdCrlDebug("...CRL %u is expired", dex);
		mIsExpired = true;
		if(compareTimes(nextUpdate, staleTime) < 0) {
			ocspdCrlDebug("...CRL %u is stale", dex);
			mIsStale = true;
		}
		/* note it can't be stale and not expired */
	}
}

#pragma mark ----- private routines -----

/*
 * Fetch attrs for all CRLs from DB. CRL blobs themselves are not fetched
 * unless the fetchBlobs argument is asserted.
 */
static CSSM_RETURN fetchAllCrls(
	CSSM_DL_DB_HANDLE 	dlDbHand, 
	bool				fetchBlobs,		// fetch actual CRL data
	CrlInfo				**&rtnCrlInfo,	// RETURNED
	unsigned			&numCrls)		// RETURNED
{
	CSSM_QUERY						query;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand;
	unsigned 						attrDex;
	CSSM_DB_ATTRIBUTE_DATA			attrData[NUM_CRL_ATTRS];
	CSSM_DATA_PTR					crlDataPtr = NULL;
	CSSM_DATA						crlData;
	
	numCrls = 0;
	rtnCrlInfo = NULL;
	
	/* build an ATTRIBUTE_DATA array from list attrs */
	memset(attrData, 0, sizeof(CSSM_DB_ATTRIBUTE_DATA) * NUM_CRL_ATTRS);
	for(attrDex=0; attrDex<NUM_CRL_ATTRS; attrDex++) {
		attrData[attrDex].Info = x509CrlRecordAttrs[attrDex];
	}

	recordAttrs.DataRecordType     = CSSM_DL_DB_RECORD_X509_CRL;
	recordAttrs.NumberOfAttributes = NUM_CRL_ATTRS;
	recordAttrs.AttributeData      = &attrData[0];
	
	/* just search by recordType, no predicates */
	query.RecordType = CSSM_DL_DB_RECORD_X509_CRL;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;
	query.QueryLimits.TimeLimit = 0;			// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;			// FIXME - meaningful?
	query.QueryFlags = 0;		// CSSM_QUERY_RETURN_DATA...FIXME - used?

	if(fetchBlobs) {
		crlDataPtr = &crlData;
	}
	
	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		&recordAttrs,
		crlDataPtr,		
		&record);
	switch(crtn) {
		case CSSM_OK:
			break;		// proceed
		case CSSMERR_DL_ENDOFDATA:
			/* we're done here */
			return CSSM_OK;
		case CSSMERR_DL_INVALID_RECORDTYPE:
			/* this means that this keychain hasn't been initialized
			 * for CRL schema; treat it as empty. */
			return CSSM_OK;
		default:
			ocspdErrorLog("fetchAllCrls: DataGetFirst returned %u", (unsigned)crtn);
			return crtn;
	}

	/* Cook up a CrlInfo, add it to outgoing array */
	CrlInfo *crlInfo = new CrlInfo(dlDbHand, &attrData[0], record, crlDataPtr);
	rtnCrlInfo = (CrlInfo **)malloc(sizeof(CrlInfo*));
	rtnCrlInfo[0] = crlInfo;
	numCrls++;

	/* now the rest of them */
	for(;;) {
		crtn = CSSM_DL_DataGetNext(dlDbHand,
			resultHand, 
			&recordAttrs,
			crlDataPtr,
			&record);
		switch(crtn) {
			case CSSM_OK:
				rtnCrlInfo = (CrlInfo **)realloc(rtnCrlInfo, 
					sizeof(CrlInfo *) * (numCrls + 1));
				rtnCrlInfo[numCrls] = new CrlInfo(dlDbHand, &attrData[0], record,
					crlDataPtr);
				numCrls++;
				break;		// and go again 
			case CSSMERR_DL_ENDOFDATA:
				/* normal termination */
				return CSSM_OK;
			default:
				ocspdErrorLog("fetchAllCrls: DataGetNext returned %u", (unsigned)crtn);
				return crtn;
		}
	}
	/* not reached */
}

/*
 * Validate each CRL's integrity (Not including expiration or stale time).
 */
static void validateCrls(
	CrlInfo		**crlInfo, 
	unsigned	numCrls)
{
	CrlInfo *crl;
	
	for(unsigned dex=0; dex<numCrls; dex++) {
		crl = crlInfo[dex];
		
		/* get CrlType, make sure it's acceptable */
		uint32 i;
		
		if(crl->fetchIntAttr(ATTR_DEX_CRL_TYPE, i)) {
			continue;
		}
		switch(i) {
			case CSSM_CRL_TYPE_X_509v1:
			case CSSM_CRL_TYPE_X_509v2:
				/* OK */
				break;
			default:
				ocspdErrorLog("validateCrls: bad CRL type (%u) on CRL %u\n", 
					(unsigned)i, dex);
				crl->mIsBadlyFormed = true;
				continue;
		}
		
		/* ditto for encoding */
		if(crl->fetchIntAttr(ATTR_DEX_CRL_ENC, i)) {
			continue;
		}
		switch(i) {
			case CSSM_CRL_ENCODING_BER:
			case CSSM_CRL_ENCODING_DER:
				/* OK */
				break;
			default:
				ocspdErrorLog("validateCrls: bad CRL encoding (%u) on CRL %u\n", 
					(unsigned)i, dex);
				crl->mIsBadlyFormed = true;
				continue;
		}
		/* any other grounds for deletion? */
	}
}

/*
 * Perform full crypto CRL validation. 
 * We use the system-wide intermediate cert keychain here, but do
 * NOT use the CRL cache we're working on (or any other), since
 * we don't really want to trust anything at this point. 
 */
static void cryptoValidateCrls(
	CrlInfo			**crlInfo, 
	unsigned		numCrls,
	CSSM_TP_HANDLE	tpHand,
	CSSM_CSP_HANDLE	cspHand,
	CSSM_CL_HANDLE	clHand,
	CSSM_DL_HANDLE	dlHand)
{
	CrlInfo 		*crl;
	
	/* the system-wide intermediate certs */
	CSSM_DL_DB_HANDLE certDb;
	CSSM_DL_DB_HANDLE_PTR certDbPtr = NULL;
	CSSM_RETURN crtn = CSSM_DL_DbOpen(dlHand,
		SYSTEM_CERT_STORE_PATH, 
		NULL,			// DbLocation
		CSSM_DB_ACCESS_READ,
		NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
		NULL,			// void *OpenParameters
		&certDb.DBHandle);
	if(crtn) {
		ocspdErrorLog("cryptoValidateCrls: CSSM_DL_DbOpen returned %u", (unsigned)crtn);
		/* Oh well, keep trying */
	}
	else {
		certDb.DLHandle = dlHand;
		certDbPtr = &certDb;
	}
	
	for(unsigned dex=0; dex<numCrls; dex++) {
		crl = crlInfo[dex];
		crtn = cuCrlVerify(tpHand, clHand, cspHand,
			&crl->mCrlBlob,
			certDbPtr,
			NULL,		// anchors - use Trust Settings
			0);			// anchorCount
		switch(crtn) {
			case CSSMERR_APPLETP_CRL_EXPIRED:
				/* special case, we'll handle this via its attrs */
			case CSSM_OK:
				break;
			default:
				ocspdCrlDebug("...CRL %u FAILED crypto verify", dex);
				crl->printName("FAILED crypto verify");
				crl->mIsBadlyFormed = true;
			break;
		}
	}
	CSSM_DL_DbClose(certDb);
}

/*
 * Calculate expired/stale state for all CRLs.
 */
int calcCurrent(
	CrlInfo		**crlInfo, 
	unsigned	numCrls, 
	int 		expireOverlapSeconds,
	int			staleTimeSeconds)
{
	if(expireOverlapSeconds > staleTimeSeconds) {
		ocspdErrorLog("calcCurrent: ExpireOverlap greater than StaleTime; aborting.\n");
		return 1;
	}
	char *updateTime = cuTimeAtNowPlus(expireOverlapSeconds, TIME_CSSM);
	char *staleTime  = cuTimeAtNowPlus(-staleTimeSeconds, TIME_CSSM);

	ocspdCrlDebug("updateTime : %s", updateTime);
	ocspdCrlDebug("staleTime  : %s", staleTime);

	for(unsigned dex=0; dex<numCrls; dex++) {
		crlInfo[dex]->validateTimes(updateTime, staleTime, dex);
	}
	APP_FREE(updateTime);
	APP_FREE(staleTime);
	return 0;
}

/* 
 * Mark all CRLs as stale (i.e., force them to be deleted later).
 */
static void purgeAllCrls(
	CrlInfo		**crlInfo, 
	unsigned	numCrls)
{
	for(unsigned dex=0; dex<numCrls; dex++) {
		CrlInfo *crl = crlInfo[dex];
		crl->mIsExpired = true;
		crl->mIsStale = true;
	}
}

/*
 * Delete all stale and badly formed CRLs from cache.
 */
static void deleteBadCrls(
	CrlInfo		**crlInfo, 
	unsigned	numCrls)
{
	CrlInfo *crl;

	for(unsigned dex=0; dex<numCrls; dex++) {
		crl = crlInfo[dex];
	
		/* 
		 * expired is not grounds for deletion; mIsStale is.
		 */
		if(crl->mIsBadlyFormed || crl->mIsStale) {
			crl->printName("deleting");
			CSSM_RETURN crtn = CSSM_DL_DataDelete(crl->dlDbHand(), 
				crl->record());
			if(crtn) {
				ocspdErrorLog("deleteBadCrls: CSSM_DL_DataDelete returned %u", (unsigned)crtn);
			}
		}
	}
}

/*
 * For each expired CRL, fetch a new one if we don't have a current
 * CRL from the same place. 
 */
static void refreshExpiredCrls(
	CrlInfo			**crlInfo, 
	unsigned		numCrls,
	CSSM_CL_HANDLE	clHand)
{
	CrlInfo *crl;
	bool haveCurrent;
	CSSM_DATA	newCrl;
	
	for(unsigned dex=0; dex<numCrls; dex++) {
		crl = crlInfo[dex];
		
		if(!crl->mIsExpired || crl->mRefreshed) {
			continue;
		}

		/* do we have one for the same issuer that's current? */
		haveCurrent = false;
		for(unsigned i=0; i<numCrls; i++) {
			if(i == dex) {
				/* skip identity */
				continue;
			}
			CrlInfo *checkCrl = crlInfo[i];
			if(checkCrl->mIsBadlyFormed) {
				/* forget this one */
				continue;
			}
			if(checkCrl->mIsExpired && !checkCrl->mRefreshed) {
				continue;
			}
			if(crl->isSameIssuer(checkCrl)) {
				/* have a match; this one's OK */
				ocspdCrlDebug("up-to-date CRL at dex %u matching expired CRL %u", 
					i, dex);
				haveCurrent = true;
				break;
			}
		}
		if(haveCurrent) {
			continue;
		}
		
		/* 
		 * Not all CRLs have a URI attribute, which is required for 
		 * refresh 
		 */
		CSSM_DATA_PTR uri = crl->fetchValidAttr(ATTR_DEX_URI);
		if(uri == NULL) {
			ocspdCrlDebug("Expired CRL with no URI at dex %u", dex);
			continue;
		}
		
		/* fetch a new one */
		crl->printName("fetching new");
		Allocator &alloc = Allocator::standard();
		CSSM_RETURN crtn = ocspdNetFetch(alloc, *uri, LT_Crl, newCrl);
		if(crtn) {
			ocspdErrorLog("ocspdNetFetch returned %u", (unsigned)crtn);
			continue;
		}
		
		/* store it in the DB */
		crtn = cuAddCrlToDb(crl->dlDbHand(), clHand, &newCrl, uri);
		alloc.free(newCrl.Data);
		
		/*
		 * One special error case - UNIQUE_INDEX_DATA indicates that 
		 * the CRL we just fetched is already in the cache. This
		 * can occur when expireOverlap is sufficiently large that
		 * we decide to fetch before a CRL is actually expired. In
		 * this case process as usual, avoiding any further updates
		 * from this CA/URI.
		 */
		switch(crtn) {
			case CSSM_OK:
				ocspdCrlDebug("...refreshed CRL added to DB to account "
					"for expired CRL %u",	dex);
				break;
			case CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA:
				ocspdCrlDebug("...refreshed CRL is a dup of CRL %u; skipping",
					dex);
				break;
			default:
				continue;
		}
		
			
		/*
		 * In case there are other CRLs still to be discovered
		 * in our list which are a) expired, and b) from this same issuer,
		 * we flag the current (expired) CRL as refreshed to ensure that
		 * we don't do this fetch again. A lot easier than cooking up 
		 * a new CrlInfo object for the CRL we just fetched.
		 */
		crl->mRefreshed = true;
	}
}

#pragma mark ----- public interface -----

CSSM_RETURN ocspdCrlRefresh(
	CSSM_DL_DB_HANDLE	dlDbHand,
	CSSM_CL_HANDLE		clHand,
	unsigned			staleDays,
	unsigned			expireOverlapSeconds,
	bool				purgeAll,
	bool				fullCryptoVerify,
	bool				doRefresh)		// false: just purge stale entries
{
	CSSM_TP_HANDLE	tpHand = 0;
	CSSM_CSP_HANDLE cspHand = 0;
	CSSM_RETURN		crtn = CSSM_OK;
	
	assert((dlDbHand.DLHandle != 0) &&
		   (dlDbHand.DBHandle != 0) &&
		   (clHand != 0));
		   
	if(staleDays == 0) {
		staleDays = DEFAULT_STALE_DAYS;
	}
	if(expireOverlapSeconds == 0) {
		expireOverlapSeconds = DEFAULT_EXPIRE_OVERLAP_SECONDS;
	}
	
	if(fullCryptoVerify) {
		/* also need TP, CSP */
		cspHand = attachCommon(&gGuidAppleCSP, CSSM_SERVICE_CSP);
		if(cspHand == 0) {
			Syslog::alert("Error loading AppleCSP");
			return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
		}
		tpHand = attachCommon(&gGuidAppleX509TP, CSSM_SERVICE_TP);
		if(tpHand == 0) {
			Syslog::alert("Error loading AppleX509TP");
			crtn = CSSMERR_CSSM_ADDIN_LOAD_FAILED;
			goto errOut;
		}
	}
	/* subsequent errors to errOut: */
	
	/* fetch all CRLs from the keychain */
	CrlInfo		**crlInfo;
	unsigned	numCrls;
	
	crtn = fetchAllCrls(dlDbHand, fullCryptoVerify, crlInfo, numCrls);
	if(crtn) {
		ocspdErrorLog("ocspdCrlRefresh: Error reading CRLs.");
		return crtn;
	}
	ocspdCrlDebug("ocspdCrlRefresh: %u CRLs found", numCrls);
	
	/* basic validation */
	validateCrls(crlInfo, numCrls);
	
	/* Optional full crypto validation */
	if(fullCryptoVerify) {
		cryptoValidateCrls(crlInfo, numCrls, tpHand, cspHand, clHand, 
			dlDbHand.DLHandle);
	}
	
	/* update the validity time flags on the CRLs */
	if(calcCurrent(crlInfo, numCrls, expireOverlapSeconds,
			staleDays * SECONDS_PER_DAY)) {
		ocspdErrorLog("ocspdCrlRefresh: Error calculating CRL times.");
		crtn = CSSMERR_TP_INTERNAL_ERROR;
		goto errOut;
	}
	
	if(purgeAll) {
		/* mark all of them stale */
		purgeAllCrls(crlInfo, numCrls);
	}
	
	/* 
	 * Delete all bad CRLs from DB. We do this before the refresh in 
	 * case of the purgeAll option, in which case we really want to 
	 * insert newly fetched CRLs in the DB even if they appear to 
	 * be trhe same as the ones they're replacing.
	 */
	deleteBadCrls(crlInfo, numCrls);
	
	/* refresh the out-of-date CRLs */
	if(doRefresh) {
		refreshExpiredCrls(crlInfo, numCrls, clHand);
	}
	
	/* clean up */
	for(unsigned dex=0; dex<numCrls; dex++) {
		delete crlInfo[dex];
	}
	free(crlInfo);

errOut:
	if(cspHand) {
		detachCommon(&gGuidAppleCSP, cspHand);
	}
	if(tpHand) {
		detachCommon(&gGuidAppleX509TP, tpHand);
	}
	return crtn;
}

	