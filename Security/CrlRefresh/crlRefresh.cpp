/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
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
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
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
 * 'expireOverlap' is (nowTime - updateTime) in seconds. It's the 
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
 * via command line arguments. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <CdsaUtils/cuCdsaUtils.h>
#include <CdsaUtils/cuTimeStr.h>
#include <CdsaUtils/cuDbUtils.h>
#include <CdsaUtils/cuFileIo.h>
#include <strings.h>
#include "ldapFetch.h"
#include <Security/keychainacl.h>
#include <Security/cssmacl.h>
#include <Security/aclclient.h>
#include <Security/cssmdata.h>
#include <Security/SecTrust.h>

#define DEFAULT_STALE_DAYS				10
#define DEFAULT_EXPIRE_OVERLAP_SECONDS	3600

#define SECONDS_PER_DAY					(60 * 60 * 24)

#define CRL_CACHE_DB	"/var/db/crls/crlcache.db"
#define X509_CERT_DB	"/System/Library/Keychains/X509Certificates"

#ifdef	NDEBUG
#define DEBUG_PRINT		0
#else
#define DEBUG_PRINT		1
#endif

#if		DEBUG_PRINT
#define dprintf(args...)	fprintf(stderr, args)
#else
#define dprintf(args...)
#endif

static void usage(char **argv)
{
	printf("Usage\n");
	printf("Refresh    : %s r [options]\n", argv[0]);
	printf("Fetch CRL  : %s f URI [options]\n", argv[0]);
	printf("Fetch cert : %s F URI [options]\n", argv[0]);
	printf("Refresh options:\n");
	printf("   s=stale_period in DAYS; default=%d\n", DEFAULT_STALE_DAYS);
	printf("   o=expire_overlap in SECONDS; default=%d\n",
					DEFAULT_EXPIRE_OVERLAP_SECONDS);
	printf("   p (Purge all entries, ensuring refresh with fresh CRLs)\n");
	printf("   f (Full crypto CRL verification)\n");
	printf("   k=keychainName (default=%s\n", CRL_CACHE_DB);
	printf("   v(erbose)\n");
	printf("Fetch options:\n");
	printf("   F=outFileName (default is stdout)\n");
	printf("   n (no write to cache after fetch)\n");
	exit(1);
}

/*
 * Print string. Null terminator is not assumed. 
 */
static void printString(
	const CSSM_DATA *str)
{
	unsigned i;
	char *cp = (char *)str->Data;
	for(i=0; i<str->Length; i++) {
		printf("%c", *cp++);
	}
}

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
				printf("***freeAttrs screwup: NULL data\n");
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
int compareTimes(
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
	
	/* print the printable name + '\n' to stdout */
	void					printName();
	
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
		dprintf("***Badly formed uint32 attr at dex %u\n", dex);
		mIsBadlyFormed = true;
		return 1;
	}
	rtn = cuDER_ToInt(val);
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

/* Print a CRL's PrintName attr */
void CrlInfo::printName()
{
	CSSM_DATA_PTR val = fetchValidAttr(ATTR_DEX_PRINT_NAME);
	if(val == NULL) {
		printf("X509 CRL\n");
	}
	else {
		printString(val);
		printf("\n");
	}
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
		printf("***Badly formed NextUpdate attr on CRL %u\n", dex);
		mIsBadlyFormed = true;
		return;
	}
	#if 	DEBUG_PRINT
	printf("Crl %u NextUpdate : ", dex); printString(nextUpdateData);
	printf("\n");
	#endif
	char *nextUpdate = (char *)nextUpdateData->Data;
	if(compareTimes(nextUpdate, updateTime) < 0) {
		dprintf("...CRL %u is expired\n", dex);
		mIsExpired = true;
		if(compareTimes(nextUpdate, staleTime) < 0) {
			dprintf("...CRL %u is stale\n", dex);
			mIsStale = true;
		}
		/* note it can't be stale and not expired */
	}
}

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
			cuPrintError("DataGetFirst", crtn);
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
				cuPrintError("DataGetNext", crtn);
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
	unsigned	numCrls,
	bool 		verbose)
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
				printf("***bad CRL type (%u) on CRL %u\n", (unsigned)i, dex);
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
				printf("***bad CRL encoding (%u) on CRL %u\n", 
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
 * we dont' really want to trust anything at this point. 
 */
static void cryptoValidateCrls(
	CrlInfo			**crlInfo, 
	unsigned		numCrls,
	bool 			verbose,
	CSSM_TP_HANDLE	tpHand,
	CSSM_CSP_HANDLE	cspHand,
	CSSM_CL_HANDLE	clHand,
	CSSM_DL_HANDLE	dlHand)
{
	CrlInfo 		*crl;
	const CSSM_DATA *anchors;
	uint32 			anchorCount;
	OSStatus 		ortn;
	
	/* just snag these once */
	ortn = SecTrustGetCSSMAnchorCertificates(&anchors, &anchorCount);
	if(ortn) {
		printf("SecTrustGetCSSMAnchorCertificates returned %u\n", (int)ortn);
		return;
	}
	
	/* and the system-wide intermediate certs */
	CSSM_DL_DB_HANDLE certDb;
	CSSM_DL_DB_HANDLE_PTR certDbPtr = NULL;
	CSSM_RETURN crtn = CSSM_DL_DbOpen(dlHand,
		X509_CERT_DB, 
		NULL,			// DbLocation
		CSSM_DB_ACCESS_READ,
		NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
		NULL,			// void *OpenParameters
		&certDb.DBHandle);
	if(crtn) {
		cuPrintError("CSSM_DL_DbOpen", crtn);
		printf("***Error opening intermediate cert file %s.\n", X509_CERT_DB);
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
			anchors,
			anchorCount);
		switch(crtn) {
			case CSSMERR_APPLETP_CRL_EXPIRED:
				/* special case, we'll handle this via its attrs */
			case CSSM_OK:
				break;
			default:
				if(verbose) {
					printf("...CRL %u FAILED crypto verify\n", dex);
				}
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
		printf("***ExpireOverlap greater than StaleTime; aborting.\n");
		return 1;
	}
	char *updateTime = cuTimeAtNowPlus(expireOverlapSeconds, TIME_CSSM);
	char *staleTime  = cuTimeAtNowPlus(-staleTimeSeconds, TIME_CSSM);

	dprintf("updateTime : %s\n", updateTime);
	dprintf("staleTime  : %s\n", staleTime);

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
	unsigned	numCrls,
	bool 		verbose)
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
	unsigned	numCrls,
	bool 		verbose)
{
	CrlInfo *crl;

	for(unsigned dex=0; dex<numCrls; dex++) {
		crl = crlInfo[dex];
	
		/* 
		 * expired is not grounds for deletion; mIsStale is.
		 */
		if(crl->mIsBadlyFormed || crl->mIsStale) {
			if(verbose || DEBUG_PRINT) {
				printf("...deleting CRL %u from ", dex);
				crl->printName();
			}
			CSSM_RETURN crtn = CSSM_DL_DataDelete(crl->dlDbHand(), 
				crl->record());
			if(crtn) {
				cuPrintError("CSSM_DL_DataDelete", crtn);
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
	CSSM_CL_HANDLE	clHand,
	bool			verbose)
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
				dprintf("up-to-date CRL at dex %u matching expired CRL %u\n", 
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
			dprintf("Expired CRL with no URI at dex %u\n", dex);
			continue;
		}
		
		/* fetch a new one */
		if(verbose || DEBUG_PRINT) {
			printf("...fetching new CRL from net to update CRL %u from ", 
					dex);
			crl->printName();
		}
		CSSM_RETURN crtn = netFetch(*uri, LT_Crl, newCrl);
		if(crtn) {
			cuPrintError("netFetch", crtn);
			continue;
		}
		
		/* store it in the DB */
		crtn = cuAddCrlToDb(crl->dlDbHand(), clHand, &newCrl, uri);
		
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
				dprintf("...refreshed CRL added to DB to account "
					"for expired CRL %u\n",	dex);
				break;
			case CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA:
				dprintf("...refreshed CRL is a dup of CRL %u; skipping\n",
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

/*
 * Open an existing keychain or create a new one.
 * This is a known "insecure" keychain/DB, since you don't need
 * to unlock it to add or remove CRLs to/from it. Thus if
 * we create it we use the filename as password.
 */
CSSM_RETURN openDatabase(
	CSSM_DL_HANDLE	dlHand,
	const char		*dbFileName,
	bool			verbose,
	CSSM_DB_HANDLE	&dbHand,		// RETURNED
	bool			&didCreate)		// RETURNED
{
	didCreate = false;
	
	/* try to open existing DB */
	CSSM_RETURN crtn = CSSM_DL_DbOpen(dlHand,
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
			cuPrintError("CSSM_DL_DbOpen", crtn);
			return crtn;
	}
	
	/* create new one */
	if(verbose) {
		printf("...creating database %s\n", dbFileName);
	}
	CSSM_DBINFO	dbInfo;
	memset(&dbInfo, 0, sizeof(CSSM_DBINFO));

	CssmAllocator &alloc = CssmAllocator::standard();
	CssmClient::AclFactory::PasswordChangeCredentials pCreds((StringData(dbFileName)), alloc);
	const AccessCredentials* aa = pCreds;
        
	// @@@ Create a nice wrapper for building the default AclEntryPrototype. 
	TypedList subject(alloc, CSSM_ACL_SUBJECT_TYPE_ANY);
	AclEntryPrototype protoType(subject);
	AuthorizationGroup &authGroup = protoType.authorization();
	CSSM_ACL_AUTHORIZATION_TAG tag = CSSM_ACL_AUTHORIZATION_ANY;
	authGroup.NumberOfAuthTags = 1;
	authGroup.AuthTags = &tag;

	const ResourceControlContext rcc(protoType, const_cast<AccessCredentials *>(aa));
	
	crtn = CSSM_DL_DbCreate(dlHand, 
		dbFileName,
		NULL,						// DbLocation
		&dbInfo,
		CSSM_DB_ACCESS_PRIVILEGED,
		&rcc,						// CredAndAclEntry
		NULL,						// OpenParameters
		&dbHand);
	if(crtn) {
		cuPrintError("CSSM_DL_DbCreate", crtn);
		return crtn;
	}
	else {
		/* one more thing: make it world writable by convention */
		if(chmod(dbFileName, 0666)) {
			perror(dbFileName);
			crtn = CSSMERR_DL_DB_LOCKED;
		}
		didCreate = true;
	}
	return crtn;
}

/*
 * Add CRL fetched from net to local cache, used only by fetchItemFromNet. 
 * Note we're not dealing with fetched certs here; they are not
 * stored on the fly.
 */
static int writeFetchedItem(
	LF_Type lfType,
	const CSSM_DATA *itemData,
	const CSSM_DATA *uriData)
{
	if(lfType == LT_Cert) {
		return 0;
	}
	
	/*
	 * The awkward part of this operation is that we have to open a DLDB
	 * (whose filename can only be hard coded at this point) and attach 
	 * to the CL.
	 */
	CSSM_DL_DB_HANDLE dlDbHand = {0, 0};
	CSSM_CL_HANDLE clHand = 0;
	CSSM_RETURN crtn;
	bool didCreate;
	int ourRtn = 0;
	
	clHand = cuClStartup();
	if(clHand == 0) {
		return 1;
	}
	/* subsequent errors to done: */
	dlDbHand.DLHandle = cuDlStartup();
	if(dlDbHand.DLHandle == 0) {
		ourRtn = 1;
		goto done;
	}
	crtn = openDatabase(dlDbHand.DLHandle,
		CRL_CACHE_DB,
		false,				// verbose
		dlDbHand.DBHandle,
		didCreate);
	if(crtn) {
		dprintf("***Error opening keychain %s. Aborting.\n", CRL_CACHE_DB);
		ourRtn = 1;
		goto done;
	}
	
	/* store it in the DB */
	crtn = cuAddCrlToDb(dlDbHand, clHand, itemData, uriData);
	
	/*
	 * One special error case - UNIQUE_INDEX_DATA indicates that 
	 * the CRL we just fetched is already in the cache. This
	 * can occur as a result of a race condition between searching
	 * for a CRL in the cache (currently done by the TP, who execs us)
	 * and the fetch we just completed, if multiple tasks or threads are
	 * searching for the same CRL.
	 * Eventually this will be handled more robustly by all of the searching
	 * and fetching being done in a daemon. 
	 */
	switch(crtn) {
		case CSSM_OK:
			dprintf("...fetched CRL added to DB\n");
			break;
		case CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA:
			dprintf("...fetched CRL is a dup; skipping\n");
			break;
		default:
			/* specific error logged by cuAddCrlToDb() */
			dprintf("Error writing CRL to cache\n");
			ourRtn = 1;
			break;
	}
done:
	if(dlDbHand.DBHandle) {
		CSSM_DL_DbClose(dlDbHand);
	}
	if(dlDbHand.DLHandle) {
		CSSM_ModuleDetach(dlDbHand.DLHandle);
	}
	if(clHand) {
		CSSM_ModuleDetach(clHand);
	}
	return ourRtn;
}
/*
 * Fetch a CRL or Cert from net; write it to a file.
 */
int fetchItemFromNet(
	LF_Type lfType,
	const char *URI,
	char *outFileName,		// NULL indicates write to stdout
	bool writeToCache)
{
	const CSSM_DATA uriData = {strlen(URI) + 1, (uint8 *)URI};
	CSSM_DATA item;
	CSSM_RETURN crtn;
	int irtn;
	
	dprintf("fetchItemFromNet %s outFile %s\n", 
		URI, outFileName ? outFileName : "stdout");
	
	/* netFetch deals with NULL-terminated string */
	uriData.Data[uriData.Length - 1] = 0;
	crtn = netFetch(uriData, lfType, item);
	if(crtn) {
		cuPrintError("netFetch", crtn);
		return 1;
	}
	dprintf("fetchItemFromNet netFetch complete, %u bytes read\n",
		(unsigned)item.Length);
	if(outFileName == NULL) {
		irtn = write(STDOUT_FILENO, item.Data, item.Length);
		if(irtn != (int)item.Length) {
			irtn = errno;
			perror("write");
		}
		else {
			irtn = 0;
		}
	}
	else {
		irtn = writeFile(outFileName, item.Data, item.Length);
		if(irtn) {
			perror(outFileName);
		}
	}
	if((irtn == 0) && writeToCache) {
		irtn = writeFetchedItem(lfType, &item, &uriData);
	}
	free(item.Data);
	dprintf("fetchItemFromNet returning %d\n", irtn);
	return irtn;
}

int main(int argc, char **argv)
{
	CSSM_RETURN 		crtn;
	CSSM_DL_DB_HANDLE	dlDbHand;
	CSSM_CL_HANDLE		clHand;
	CSSM_CSP_HANDLE		cspHand = 0;
	CSSM_TP_HANDLE		tpHand = 0;
	int					arg;
	char				*argp;
	bool				didCreate = false;
	int 				optArg;
	
	/* user-specified variables */
	bool				verbose = false;
	bool				purgeAll = false;
	bool				fullCryptoValidation = false;
	int 				staleDays = DEFAULT_STALE_DAYS;
	int					expireOverlapSeconds = 
							DEFAULT_EXPIRE_OVERLAP_SECONDS;
	char				*dbFileName = CRL_CACHE_DB;
	/* fetch options */
	LF_Type 			lfType = LT_Crl;
	char				*outFileName = NULL;
	bool				writeToCache = true;
	char				*uri = NULL;
	
	if(argc < 2) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'F':
			lfType = LT_Cert;
			/* and drop thru */
		case 'f':
			if(argc < 3) {
				usage(argv);
			}
			uri = argv[2];
			optArg = 3;
			break;
		case 'r':
			optArg = 2;
			break;
		default:
			usage(argv);
	}
	/* refresh options */
	for(arg=optArg; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 's':
				if(argp[1] != '=') {
					usage(argv);
				}
				staleDays = atoi(&argp[2]);
				break;
			case 'o':
				if(argp[1] != '=') {
					usage(argv);
				}
				expireOverlapSeconds = atoi(&argp[2]);
				break;
			case 'p':
				purgeAll = true;
				break;
			case 'f':
				fullCryptoValidation = true;
				break;
			case 'k':
				if(argp[1] != '=') {
					usage(argv);
				}
				dbFileName = &argp[2];
				break;
			case 'n':
				writeToCache = false;
				break;
			case 'F':
				if(argp[1] != '=') {
					usage(argv);
				}
				outFileName = &argp[2];
				break;
			case 'v':
				verbose = true;
				break;
			default:
				usage(argv);
		}
	}
	if(argv[1][0] != 'r') {
		return fetchItemFromNet(lfType, uri, outFileName, writeToCache);
	}

	dprintf("...staleDays %d  expireOverlapSeconds %d\n",
		staleDays, expireOverlapSeconds);
	
	/* 
	 * Open the keychain.
	 * Note that since we're doing a lot of CDSA_level DB ops, we
	 * just acces the keychain as a DLDB direwctly - that way we
	 * have control over the app-level memory callbacks.
	 */
	dlDbHand.DLHandle = cuDlStartup();
	if(dlDbHand.DLHandle == 0) {
		exit(1);
	}
	crtn = openDatabase(dlDbHand.DLHandle,
		dbFileName,
		verbose,
		dlDbHand.DBHandle,
		didCreate);
	if(crtn) {
		printf("***Error opening keychain %s. Aborting.\n", dbFileName);
		exit(1);
	}
	
	if(didCreate) {
		/* New, empty keychain. I guarantee you we're done. */
		CSSM_DL_DbClose(dlDbHand);
		CSSM_ModuleDetach(dlDbHand.DLHandle);
		return 0;
	}

	clHand = cuClStartup();
	if(clHand == 0) {
		exit(1);
	}
	if(fullCryptoValidation) {
		/* also need TP, CSP */
		cspHand = cuCspStartup(CSSM_TRUE);
		if(cspHand == 0) {
			exit(1);
		}
		tpHand = cuTpStartup();
		if(tpHand == 0) {
			exit(1);
		}
	}

	/* fetch all CRLs from the keychain */
	CrlInfo		**crlInfo;
	unsigned	numCrls;
	crtn = fetchAllCrls(dlDbHand, fullCryptoValidation, crlInfo, numCrls);
	if(crtn) {
		printf("***Error reading CRLs from %s. Aborting.\n", dbFileName);
		exit(1);
	}
	dprintf("...%u CRLs found\n", numCrls);
	
	/* basic validation */
	validateCrls(crlInfo, numCrls, verbose);
	
	/* Optional full crypto validation */
	if(fullCryptoValidation) {
		cryptoValidateCrls(crlInfo, numCrls, verbose, 
			tpHand, cspHand, clHand, dlDbHand.DLHandle);
	}
	
	/* update the validity time flags on the CRLs */
	if(calcCurrent(crlInfo, numCrls, expireOverlapSeconds,
			staleDays * SECONDS_PER_DAY)) {
		printf("***Error calculating CRL times. Aborting\n");
		exit(1);
	}
	
	if(purgeAll) {
		/* mark all of them stale */
		purgeAllCrls(crlInfo, numCrls, verbose);
	}
	
	/* 
	 * Delete all bad CRLs from DB. We do this before the refresh in 
	 * case of the purgeAll option, in which case we really want to 
	 * insert newly fetched CRLs in the DB even if they appear to 
	 * be trhe same as the ones they're replacing.
	 */
	deleteBadCrls(crlInfo, numCrls, verbose);
	
	/* refresh the out-of-date CRLs */
	refreshExpiredCrls(crlInfo, numCrls, clHand, verbose);
	
	/* clean up */
	for(unsigned dex=0; dex<numCrls; dex++) {
		delete crlInfo[dex];
	}
	free(crlInfo);
	CSSM_DL_DbClose(dlDbHand);
	CSSM_ModuleDetach(dlDbHand.DLHandle);
	CSSM_ModuleDetach(clHand);
	if(tpHand) {
		CSSM_ModuleDetach(tpHand);
	}
	if(cspHand) {
		CSSM_ModuleDetach(cspHand);
	}
	return 0;
}
