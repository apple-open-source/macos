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
 * TPCertInfo.h - TP's private certificate info classes
 *
 * Written 10/23/2000 by Doug Mitchell.
 */

#include "TPCertInfo.h"
#include "tpdebugging.h"
#include "tpTime.h"
#include "certGroupUtils.h"
#include "TPDatabase.h"
#include "TPNetwork.h"
#include <Security/cssmapi.h>
#include <Security/x509defs.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>
#include <string.h>						/* for memcmp */
#include <Security/threading.h>			/* for Mutex */
#include <Security/globalizer.h>
#include <Security/debugging.h>
#include <Security/cssmapple.h>

#define tpTimeDbg(args...)		secdebug("tpTime", ## args) 
#define tpCertInfoDbg(args...)	secdebug("tpCert", ## args)

static const TPClItemCalls tpCertClCalls =
{
	CSSM_CL_CertGetFirstCachedFieldValue,
	CSSM_CL_CertAbortQuery,
	CSSM_CL_CertCache,
	CSSM_CL_CertAbortCache,
	CSSM_CL_CertVerify,
	&CSSMOID_X509V1ValidityNotBefore,
	&CSSMOID_X509V1ValidityNotAfter,
	CSSMERR_TP_INVALID_CERT_POINTER,
	CSSMERR_TP_CERT_EXPIRED,
	CSSMERR_TP_CERT_NOT_VALID_YET
};

TPClItemInfo::TPClItemInfo(
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	const TPClItemCalls	&clCalls,
	const CSSM_DATA		*itemData,
	TPItemCopy			copyItemData,
	const char			*verifyTime)	// may be NULL
		: 	
			mClHand(clHand),
			mCspHand(cspHand),
			mClCalls(clCalls),
			mWeOwnTheData(false),
			mCacheHand(0),
			mIssuerName(NULL),
			mItemData(NULL),
			mSigAlg(CSSM_ALGID_NONE),
			mIsExpired(false),
			mIsNotValidYet(false),
			mIndex(0)
{
	try {
		cacheItem(itemData, copyItemData);
		/* 
		 * Fetch standard fields...
		 * Issue name assumes same OID for Certs and CRLs!
		 */
		CSSM_RETURN crtn = fetchField(&CSSMOID_X509V1IssuerName, &mIssuerName);
		if(crtn) {
			CssmError::throwMe(crtn);
		}
		
		/* 
		 * Signing algorithm, infer from TBS algId 
		 * Note this assumesÊthat the OID for fetching this field is the
		 * same for CRLs and Certs.
		 */
		CSSM_DATA_PTR algField;
		crtn = fetchField(&CSSMOID_X509V1SignatureAlgorithmTBS, &algField);
		if(crtn) {
			releaseResources();
			CssmError::throwMe(crtn);
		}
		if(algField->Length != sizeof(CSSM_X509_ALGORITHM_IDENTIFIER)) {
			tpErrorLog("TPClItemInfo: bad CSSM_X509_ALGORITHM_IDENTIFIER\n");
			CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
		}
		CSSM_X509_ALGORITHM_IDENTIFIER *algId = 
			(CSSM_X509_ALGORITHM_IDENTIFIER *)algField->Data;
		bool algFound = cssmOidToAlg(&algId->algorithm, &mSigAlg);
		if(!algFound) {
			tpErrorLog("TPClItemInfo: unknown signature algorithm\n");
			CssmError::throwMe(CSSMERR_TP_UNKNOWN_FORMAT);
		}
		freeField(&CSSMOID_X509V1SignatureAlgorithmTBS, algField);
	
		fetchNotBeforeAfter();
		calculateCurrent(verifyTime);
	}
	catch(...) {
		releaseResources();
		throw;
	}
}

TPClItemInfo::~TPClItemInfo()
{
	tpCertInfoDbg("TPClItemInfo destruct this %p", this);
	releaseResources();
}

void TPClItemInfo::releaseResources()
{
	if(mWeOwnTheData && (mItemData != NULL)) {
		tpFreeCssmData(CssmAllocator::standard(), mItemData, CSSM_TRUE);
		mWeOwnTheData = false;
		mItemData = NULL;
	}
	if(mIssuerName) {
		freeField(&CSSMOID_X509V1IssuerName, mIssuerName);
		mIssuerName = NULL;
	}
	if(mCacheHand != 0) {
		mClCalls.abortCache(mClHand, mCacheHand);
		mCacheHand = 0;
	}
}

/* fetch arbitrary field from cached cert */
CSSM_RETURN TPClItemInfo::fetchField(
	const CSSM_OID	*fieldOid,
	CSSM_DATA_PTR	*fieldData)		// mallocd by CL and RETURNED
{
	CSSM_RETURN crtn;
	
	uint32 NumberOfFields = 0;
	CSSM_HANDLE resultHand = 0;
	*fieldData = NULL;

	assert(mClCalls.getField != NULL);
	assert(mCacheHand != 0);
	crtn = mClCalls.getField(
		mClHand,
		mCacheHand,
	    fieldOid,
	    &resultHand,
	    &NumberOfFields,
		fieldData);
	if(crtn) {
		return crtn;
	}
	if(NumberOfFields != 1) {
		tpErrorLog("TPCertInfo::fetchField: numFields %d, expected 1\n", 
			(int)NumberOfFields);
	}
  	mClCalls.abortQuery(mClHand, resultHand);
	return CSSM_OK;
}

/* free arbitrary field obtained from fetchField() */
CSSM_RETURN TPClItemInfo::freeField( 
	const CSSM_OID	*fieldOid,
	CSSM_DATA_PTR	fieldData)	
{
	return CSSM_CL_FreeFieldValue(mClHand, fieldOid, fieldData);

}

/* 
 * Verify with an issuer cert - works on certs and CRLs.
 * Issuer/subject name match already performed by caller.
 * Optional paramCert is used to provide parameters when issuer
 * has a partial public key.
 */
CSSM_RETURN TPClItemInfo::verifyWithIssuer(
	TPCertInfo		*issuerCert,
	TPCertInfo		*paramCert /* = NULL */) const
{
	CSSM_RETURN	crtn;

	assert(mClHand != 0);
	assert(issuerCert->isIssuerOf(*this));
	assert(mCspHand != 0);
	
	/*
	 * Special case: detect partial public key right now; don't even 
	 * bother trying the cert verify in that case.
	 */
	if(issuerCert->hasPartialKey() && (paramCert == NULL)) {
		/* caller deals with this later */
		tpVfyDebug("verifyWithIssuer PUBLIC_KEY_INCOMPLETE");
		return CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE;
	}
	
	CSSM_CC_HANDLE ccHand;
	crtn = CSSM_CSP_CreateSignatureContext(mCspHand,
		mSigAlg,
		NULL,			// Access Creds
		issuerCert->pubKey(),
		&ccHand);
	if(crtn != CSSM_OK) {
		tpErrorLog("verifyWithIssuer: CreateSignatureContext error\n");
		CssmError::throwMe(crtn);
	}
	if(paramCert != NULL) {
		assert(issuerCert->hasPartialKey());
		
		/* add in parameter-bearing key */
		CSSM_CONTEXT_ATTRIBUTE		newAttr;	
		
		newAttr.AttributeType   = CSSM_ATTRIBUTE_PARAM_KEY;
		newAttr.AttributeLength = sizeof(CSSM_KEY);
		newAttr.Attribute.Key   = paramCert->pubKey();
		crtn = CSSM_UpdateContextAttributes(ccHand, 1, &newAttr);
		if(crtn) {
			tpErrorLog("verifyWithIssuer: CSSM_UpdateContextAttributes error\n");
			CssmError::throwMe(crtn);
		}
	}
	crtn = mClCalls.itemVerify(mClHand, 
    	ccHand, 
    	mItemData,
    	NULL,				// issuer cert
    	NULL,				// VerifyScope
    	0);					// ScopeSize

	switch(crtn) {
		case CSSM_OK:		// success
		case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:	// caller handles
			tpVfyDebug("verifyWithIssuer GOOD");
			break;
		default:
			/* all others appear here as general cert verify error */
			crtn = CSSMERR_TP_VERIFICATION_FAILURE;
			tpVfyDebug("verifyWithIssuer BAD");
			break;
	}
	CSSM_DeleteContext(ccHand);
	return crtn;
}

CSSM_RETURN TPClItemInfo::cacheItem(
	const CSSM_DATA		*itemData,
	TPItemCopy			copyItemData)												
{
	switch(copyItemData) {
		case TIC_NoCopy:
			mItemData = const_cast<CSSM_DATA *>(itemData);
			break;
		case TIC_CopyData:
			mItemData = tpMallocCopyCssmData(CssmAllocator::standard(), itemData);
			mWeOwnTheData = true;
			break;
		default:
			assert(0);
			CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
	}
	
	/* cache the cert/CRL in the CL */
	return mClCalls.cacheItem(mClHand, mItemData, &mCacheHand);
}

/* 
 * Calculate not before/after times as struct tm. Only throws on 
 * gross error (CSSMERR_TP_INVALID_CERT_POINTER, etc.).
 *
 * Only differences between Cert and CRL flavors of this are the 
 * OIDs used to fetch the appropriate before/after times, both of
 * which are expressed as CSSM_X509_TIME structs for both Certs 
 * and CRLS.
 */
void TPClItemInfo::fetchNotBeforeAfter()
{
	CSSM_DATA_PTR	notBeforeField = NULL;
	CSSM_DATA_PTR	notAfterField = NULL;
	CSSM_RETURN		crtn = CSSM_OK;
	CSSM_X509_TIME 	*xTime;
	
	assert(cacheHand() != CSSM_INVALID_HANDLE);
	crtn = fetchField(mClCalls.notBeforeOid, &notBeforeField);
	if(crtn) {
		tpErrorLog("fetchNotBeforeAfter: GetField error\n");
		CssmError::throwMe(mClCalls.invalidItemRtn);
	}
	
	/* subsequent errors to errOut */
	xTime = (CSSM_X509_TIME *)notBeforeField->Data;
	if(timeStringToTm((char *)xTime->time.Data, xTime->time.Length, &mNotBefore)) {
		tpErrorLog("fetchNotBeforeAfter: malformed notBefore time\n");
		crtn = mClCalls.invalidItemRtn;
		goto errOut;
	}

	crtn = fetchField(mClCalls.notAfterOid, &notAfterField);
	if(crtn) {
		/*
		 * Tolerate a missing NextUpdate in CRL only 
		 */
		if(mClCalls.notAfterOid == &CSSMOID_X509V1ValidityNotAfter) {
			tpErrorLog("fetchNotBeforeAfter: GetField error\n");
			crtn = mClCalls.invalidItemRtn;
			goto errOut;
		}
		else {
			/*
			 * Fake NextUpdate to be "at the end of time"
			 */
			timeStringToTm(CSSM_APPLE_CRL_END_OF_TIME, 
				strlen(CSSM_APPLE_CRL_END_OF_TIME), 
				&mNotAfter);
		}
	}
	else {
		xTime = (CSSM_X509_TIME *)notAfterField->Data;
		if(timeStringToTm((char *)xTime->time.Data, xTime->time.Length, &mNotAfter)) {
			tpErrorLog("fetchNotBeforeAfter: malformed notAfter time\n");
			crtn = mClCalls.invalidItemRtn;
			goto errOut;
		}
	}
	crtn = CSSM_OK;
errOut:
	if(notAfterField) {
		freeField(mClCalls.notAfterOid, notAfterField);
	}
	if(notBeforeField) {
		freeField(mClCalls.notBeforeOid, notBeforeField);
	}
	if(crtn != CSSM_OK) {
		CssmError::throwMe(crtn);
	}
}

/* 
 * Verify validity (not before/after) by comparing the reference
 * time (verifyString if present, or "now" if NULL) to the 
 * not before/after fields fetched from the item at construction.
 *
 * Called implicitly at construction; can be called again any time
 * to re-establish validity (e.g. after fetching an item from a cache).
 *
 * We use some stdlib time calls over in tpTime.c; the stdlib function
 * gmtime() is not thread-safe, so we do the protection here. Note that
 * this makes *our* calls to gmtime() thread-safe, but if the app has
 * other threads which are also calling gmtime, we're out of luck.
 */
ModuleNexus<Mutex> tpTimeLock;

CSSM_RETURN TPClItemInfo::calculateCurrent(
	const char 			*verifyString)
{
	struct tm 		refTime;
	
	if(verifyString != NULL) {
		/* caller specifies verification time base */
		if(timeStringToTm(verifyString, strlen(verifyString), &refTime)) {
			tpErrorLog("calculateCurrent: timeStringToTm error\n");
			return CSSMERR_TP_INVALID_TIMESTRING;
		}
	}
	else {
		/* time base = right now */
		StLock<Mutex> _(tpTimeLock());
		nowTime(&refTime);
	}
	if(compareTimes(&refTime, &mNotBefore) < 0) {
		mIsNotValidYet = true;
		tpTimeDbg("\nTP_CERT_NOT_VALID_YET:\n   now y:%d m:%d d:%d h:%d m:%d",
			refTime.tm_year, refTime.tm_mon, refTime.tm_mday, 
			refTime.tm_hour, refTime.tm_min);
		tpTimeDbg(" notBefore y:%d m:%d d:%d h:%d m:%d",
			mNotBefore.tm_year, mNotBefore.tm_mon, mNotBefore.tm_mday, 
			mNotBefore.tm_hour, mNotBefore.tm_min);
		return mClCalls.notValidYetRtn;
	}
	else {
		mIsNotValidYet = false;
	}

	if(compareTimes(&refTime, &mNotAfter) > 0) {
		mIsExpired = true;
		tpTimeDbg("\nTP_CERT_EXPIRED: \n   now y:%d m:%d d:%d "
				"h:%d m:%d",
			refTime.tm_year, refTime.tm_mon, refTime.tm_mday, 
			refTime.tm_hour, refTime.tm_min);
		tpTimeDbg(" notAfter y:%d m:%d d:%d h:%d m:%d",
			mNotAfter.tm_year, mNotAfter.tm_mon, mNotAfter.tm_mday, 
			mNotAfter.tm_hour, mNotAfter.tm_min);
		return mClCalls.expiredRtn;
	}
	else {
		mIsExpired = false;
		return CSSM_OK;
	}
}


/* 
 * No default constructor - this is the only way.
 * This caches the cert and fetches subjectName, issuerName, and
 * mPublicKey to ensure the incoming certData is well-constructed.
 */
TPCertInfo::TPCertInfo(
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		*certData,
	TPItemCopy			copyCertData,		// true: we copy, we free
											// false - caller owns
	const char			*verifyTime)		// may be NULL
	:
		TPClItemInfo(clHand, cspHand, tpCertClCalls, certData, 
			copyCertData, verifyTime),
		mSubjectName(NULL),
		mPublicKey(NULL),
		mIsAnchor(false),
		mIsFromDb(false),
		mIsFromNet(false),
		mNumStatusCodes(0),
		mStatusCodes(NULL),
		mUniqueRecord(NULL),
		mUsed(false),
		mIsLeaf(false),
		mIsRoot(TRS_Unknown)
{
	CSSM_RETURN	crtn;

	tpCertInfoDbg("TPCertInfo construct this %p", this);
	mDlDbHandle.DLHandle = 0;
	mDlDbHandle.DBHandle = 0;
	
	/* fetch subject name */
	crtn = fetchField(&CSSMOID_X509V1SubjectName, &mSubjectName);
	if(crtn) {
		/* bad cert */
		releaseResources();
		CssmError::throwMe(crtn);
	}
	
	/* this cert's public key */
	crtn = CSSM_CL_CertGetKeyInfo(clHand, certData, &mPublicKey);
	if(crtn) {
		/* bad cert */
		releaseResources();
		CssmError::throwMe(crtn);
	}
	
	/* calculate other commonly used fields */
	if(tpCompareCssmData(mSubjectName, issuerName())) {
		/*
		 * Per Radar 3374978, perform complete signature verification
		 * lazily - just check subject/issuer match here.
		 */
		tpAnchorDebug("TPCertInfo potential anchor");
		mIsRoot = TRS_NamesMatch;
	}
	else {
		mIsRoot = TRS_NotRoot;
	}
}
	
/* frees mSubjectName, mIssuerName, mCacheHand via mClHand */
TPCertInfo::~TPCertInfo()
{
	tpCertInfoDbg("TPCertInfo destruct this %p", this);
	releaseResources();
}

void TPCertInfo::releaseResources()
{
	if(mSubjectName) {
		freeField(&CSSMOID_X509V1SubjectName, mSubjectName);
		mSubjectName = NULL;
	}
	if(mStatusCodes) {
		free(mStatusCodes);
		mStatusCodes = NULL;
	}
	if(mPublicKey) {
		/* allocated by CL */
		tpFreePluginMemory(clHand(), mPublicKey->KeyData.Data);
		tpFreePluginMemory(clHand(), mPublicKey);
		mPublicKey = NULL;
	}
	TPClItemInfo::releaseResources();
}

const CSSM_DATA *TPCertInfo::subjectName()
{
	assert(mSubjectName != NULL);
	return mSubjectName;
}

/* 
 * Perform semi-lazy evaluation of "rootness". Subject and issuer names
 * compared at constructor.
 */
bool TPCertInfo::isSelfSigned()
{
	switch(mIsRoot) {
		case TRS_NotRoot:			// known not to be root
			return false;
		case TRS_IsRoot:
			return true;
		case TRS_Unknown:			// actually shouldn't happen, but to be safe...
		case TRS_NamesMatch:
		default:
			/* do the signature verify */
			if(verifyWithIssuer(this) == CSSM_OK) {
				tpAnchorDebug("isSelfSigned anchor verified");
				mIsRoot = TRS_IsRoot;
				return true;
			}
			else {
				tpAnchorDebug("isSelfSigned anchor vfy FAIL");
				mIsRoot = TRS_NotRoot;
				return false;
			}
	}
}

/*
 * Am I the issuer of the specified subject item? Returns true if so.
 * Works for subject certs as well as CRLs. 
 */
bool TPCertInfo::isIssuerOf(
	const TPClItemInfo	&subject)
{
	assert(mSubjectName != NULL);
	assert(subject.issuerName() != NULL);
	if(tpCompareCssmData(mSubjectName, subject.issuerName())) {
		return true;
	}
	else {
		return false;
	}
}

void TPCertInfo::addStatusCode(CSSM_RETURN code)
{
	mNumStatusCodes++;
	mStatusCodes = (CSSM_RETURN *)realloc(mStatusCodes, 
		mNumStatusCodes * sizeof(CSSM_RETURN));
	mStatusCodes[mNumStatusCodes - 1] = code;
}

/* 
 * Indicate whether this cert's public key is a CSSM_KEYATTR_PARTIAL
 * key.
 */
bool TPCertInfo::hasPartialKey()
{
	if(mPublicKey->KeyHeader.KeyAttr & CSSM_KEYATTR_PARTIAL) {
		return true;
	}
	else {
		return false;
	}
}

/***
 *** TPCertGroup class
 ***/
 
/* build empty group */
TPCertGroup::TPCertGroup(
	CssmAllocator		&alloc,
	TPGroupOwner		whoOwns) :
		mAlloc(alloc),
		mCertInfo(NULL),
		mNumCerts(0),
		mSizeofCertInfo(0),
		mWhoOwns(whoOwns)
{
	tpCertInfoDbg("TPCertGroup simple construct this %p", this);
	/* nothing for now */
}
	
/*
 * Construct from unordered, untrusted CSSM_CERTGROUP. Resulting
 * TPCertInfos are more or less in the same order as the incoming
 * certs, though incoming certs are discarded if they don't parse.
 * No verification of any sort is performed. 
 */
TPCertGroup::TPCertGroup(
	const CSSM_CERTGROUP 	&CertGroupFrag,
	CSSM_CL_HANDLE 			clHand,
	CSSM_CSP_HANDLE 		cspHand,
	CssmAllocator			&alloc,
	const char				*verifyTime,			// may be NULL
	bool					firstCertMustBeValid,
	TPGroupOwner			whoOwns) :
		mAlloc(alloc),
		mCertInfo(NULL),
		mNumCerts(0),
		mSizeofCertInfo(0),
		mWhoOwns(whoOwns)
{
	tpCertInfoDbg("TPCertGroup hard construct this %p", this);

	/* verify input args */
	if(cspHand == CSSM_INVALID_HANDLE) {
		CssmError::throwMe(CSSMERR_TP_INVALID_CSP_HANDLE);
	}
	if(clHand == CSSM_INVALID_HANDLE)	{
		CssmError::throwMe(CSSMERR_TP_INVALID_CL_HANDLE);
	}
	if(firstCertMustBeValid) {
		if( (CertGroupFrag.NumCerts == 0) || 			
	        (CertGroupFrag.GroupList.CertList[0].Data == NULL) ||
	        (CertGroupFrag.GroupList.CertList[0].Length == 0)) {
				CssmError::throwMe(CSSMERR_TP_INVALID_CERTIFICATE);
		}
	}
	if(CertGroupFrag.CertGroupType != CSSM_CERTGROUP_DATA) {
		CssmError::throwMe(CSSMERR_TP_INVALID_CERTGROUP);
	}
	switch(CertGroupFrag.CertType) {
		case CSSM_CERT_X_509v1:
		case CSSM_CERT_X_509v2:
		case CSSM_CERT_X_509v3:
			break;
		default:
			CssmError::throwMe(CSSMERR_TP_UNKNOWN_FORMAT);
	}
	switch(CertGroupFrag.CertEncoding) {
		case CSSM_CERT_ENCODING_BER:
		case CSSM_CERT_ENCODING_DER:
			break;
		default:
			CssmError::throwMe(CSSMERR_TP_UNKNOWN_FORMAT);
	}
	
	/* 
	 * Add remaining input certs to mCertInfo. 
	 */
	TPCertInfo *certInfo = NULL;
	for(unsigned certDex=0; certDex<CertGroupFrag.NumCerts; certDex++) {
		try {
			certInfo = new TPCertInfo(clHand,
				cspHand,
				&CertGroupFrag.GroupList.CertList[certDex],
				TIC_NoCopy,			// caller owns
				verifyTime);
		}
		catch (...) {
			if((certDex == 0) && firstCertMustBeValid) {
				CssmError::throwMe(CSSMERR_TP_INVALID_CERTIFICATE);
			}
			/* else just ignore this cert */
			continue;
		}
		certInfo->index(certDex);
		appendCert(certInfo);
	}
}

/*
 * Deletes contents of mCertInfo[] if appropriate.
 */
TPCertGroup::~TPCertGroup()
{
	if(mWhoOwns == TGO_Group) {
		unsigned i;
		for(i=0; i<mNumCerts; i++) {
			delete mCertInfo[i];
		}
	}
	mAlloc.free(mCertInfo);
}

/* add/remove/access TPTCertInfo's. */
/*
 * NOTE: I am aware that most folks would just use an array<> here, but
 * gdb is so lame that it doesn't even let one examine the contents
 * of an array<> (or just about anything else in the STL). I prefer
 * debuggability over saving a few lines of trivial code.
 */
void TPCertGroup::appendCert(
	TPCertInfo			*certInfo)			// appends to end of mCertInfo
{
	if(mNumCerts == mSizeofCertInfo) {
		if(mSizeofCertInfo == 0) {
			/* appending to empty array */
			mSizeofCertInfo = 1;
		}
		else {
			mSizeofCertInfo *= 2;
		}
		mCertInfo = (TPCertInfo **)mAlloc.realloc(mCertInfo, 
			mSizeofCertInfo * sizeof(TPCertInfo *));
	}
	mCertInfo[mNumCerts++] = certInfo;
}

TPCertInfo *TPCertGroup::certAtIndex(
	unsigned			index)
{
	if(index > (mNumCerts - 1)) {
		CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
	}
	return mCertInfo[index];
}

TPCertInfo *TPCertGroup::removeCertAtIndex(
	unsigned			index)				// doesn't delete the cert, just 
											// removes it from out list
{
	if(index > (mNumCerts - 1)) {
		CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
	}
	TPCertInfo *rtn = mCertInfo[index];
	
	/* removed requested element and compact remaining array */
	unsigned i;
	for(i=index; i<(mNumCerts - 1); i++) {
		mCertInfo[i] = mCertInfo[i+1];
	}
	mNumCerts--;
	return rtn;
}

TPCertInfo *TPCertGroup::firstCert()
{
	if(mNumCerts == 0) {
		/* the caller really should not do this... */
		CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
	}
	else {
		return mCertInfo[0];
	}
}

TPCertInfo *TPCertGroup::lastCert()
{
	if(mNumCerts == 0) {
		return NULL;
	}
	else {
		return mCertInfo[mNumCerts - 1];
	}
}

/* build a CSSM_CERTGROUP corresponding with our mCertInfo */
CSSM_CERTGROUP_PTR TPCertGroup::buildCssmCertGroup()
{
	CSSM_CERTGROUP_PTR cgrp = 
		(CSSM_CERTGROUP_PTR)mAlloc.malloc(sizeof(CSSM_CERTGROUP));
	cgrp->NumCerts = mNumCerts;
	cgrp->CertGroupType = CSSM_CERTGROUP_DATA;
	cgrp->CertType = CSSM_CERT_X_509v3;
	cgrp->CertEncoding = CSSM_CERT_ENCODING_DER; 
	if(mNumCerts == 0) {
		/* legal */
		cgrp->GroupList.CertList = NULL;
		return cgrp;
	}
	cgrp->GroupList.CertList = (CSSM_DATA_PTR)mAlloc.calloc(mNumCerts, 
		sizeof(CSSM_DATA));
	for(unsigned i=0; i<mNumCerts; i++) {
		tpCopyCssmData(mAlloc, mCertInfo[i]->itemData(), 
			&cgrp->GroupList.CertList[i]);
	}
	return cgrp;
}

/* build a CSSM_TP_APPLE_EVIDENCE_INFO array */
CSSM_TP_APPLE_EVIDENCE_INFO *TPCertGroup::buildCssmEvidenceInfo()
{
	CSSM_TP_APPLE_EVIDENCE_INFO *infoArray;
	
	infoArray = (CSSM_TP_APPLE_EVIDENCE_INFO *)mAlloc.calloc(mNumCerts,
		sizeof(CSSM_TP_APPLE_EVIDENCE_INFO));
	for(unsigned i=0; i<mNumCerts; i++) {
		TPCertInfo *certInfo = mCertInfo[i];
		CSSM_TP_APPLE_EVIDENCE_INFO *evInfo = &infoArray[i];
		
		/* first the booleans */
		if(certInfo->isExpired()) {
			evInfo->StatusBits |= CSSM_CERT_STATUS_EXPIRED;
		}
		if(certInfo->isNotValidYet()) {
			evInfo->StatusBits |= CSSM_CERT_STATUS_NOT_VALID_YET;
		}
		if(certInfo->dlDbHandle().DLHandle == 0) {
			if(certInfo->isAnchor()) {
				evInfo->StatusBits |= CSSM_CERT_STATUS_IS_IN_ANCHORS;
			}
			else if(certInfo->isFromNet()) {
				evInfo->StatusBits |= CSSM_CERT_STATUS_IS_FROM_NET;
			}
			else {
				evInfo->StatusBits |= CSSM_CERT_STATUS_IS_IN_INPUT_CERTS;
			}
		}
		if(certInfo->isSelfSigned()) {
			evInfo->StatusBits |= CSSM_CERT_STATUS_IS_ROOT;
		}
		
		unsigned numCodes = certInfo->numStatusCodes();
		if(numCodes) {
			evInfo->NumStatusCodes = numCodes;
			evInfo->StatusCodes = (CSSM_RETURN *)mAlloc.calloc(numCodes,
				sizeof(CSSM_RETURN));
			for(unsigned j=0; j<numCodes; j++) {
				evInfo->StatusCodes[j] = (certInfo->statusCodes())[j];
			}
		}
		
		evInfo->Index = certInfo->index();
		evInfo->DlDbHandle = certInfo->dlDbHandle();
		evInfo->UniqueRecord = certInfo->uniqueRecord();
	}
	return infoArray;
}
		
/* Given a status for basic construction of a cert group and a status
 * of (optional) policy verification, plus the implicit notBefore/notAfter
 * status in the certs, calculate a global return code. This just 
 * encapsulates a policy for CertGroupConstruct and CertGroupVerify.
 */
CSSM_RETURN TPCertGroup::getReturnCode(
	CSSM_RETURN constructStatus,
	CSSM_BOOL	allowExpired,
	CSSM_BOOL	allowExpiredRoot,
	CSSM_RETURN policyStatus /* = CSSM_OK */)
{
	if(constructStatus) {
		/* CSSMERR_TP_NOT_TRUSTED, CSSMERR_TP_INVALID_ANCHOR_CERT, gross errors */
		return constructStatus;
	}
	
	/* check for expired, not valid yet */
	bool expired = false;
	bool notValid = false;
	for(unsigned i=0; i<mNumCerts; i++) {
		if(mCertInfo[i]->isExpired() &&
		   !(allowExpiredRoot && mCertInfo[i]->isSelfSigned())) {
			expired = true;
		}
		if(mCertInfo[i]->isNotValidYet()) {
			notValid = true;
		}
	}
	if(expired && !allowExpired) {
		return CSSMERR_TP_CERT_EXPIRED;
	}
	if(notValid) {
		return CSSMERR_TP_CERT_NOT_VALID_YET;
	}
	return policyStatus;
}

/* set all TPCertINfo.mUsed flags false */
void TPCertGroup::setAllUnused()
{
	for(unsigned dex=0; dex<mNumCerts; dex++) {
		mCertInfo[dex]->used(false);
	}
}

/* 
 * Search unused incoming certs to find an issuer of specified cert or CRL.
 * WARNING this assumes a valid "used" state for all certs in this group.
 * If partialIssuerKey is true on return, caller must re-verify signature
 * of subject later when sufficient info is available. 
 */
TPCertInfo *TPCertGroup::findIssuerForCertOrCrl(
	const TPClItemInfo &subject,
	bool &partialIssuerKey)
{
	partialIssuerKey = false;
	for(unsigned certDex=0; certDex<mNumCerts; certDex++) {
		TPCertInfo *certInfo = certAtIndex(certDex);
		
		/* has this one already been used in this search? */
		if(certInfo->used()) {
			continue;
		}
		
		/* subject/issuer names match? */
		if(certInfo->isIssuerOf(subject)) {
			/* yep, do a sig verify */
			tpVfyDebug("findIssuerForCertOrCrl issuer/subj match checking sig");
			CSSM_RETURN crtn = subject.verifyWithIssuer(certInfo);
			switch(crtn) {
				case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:
					/* issuer OK, check sig later */
					partialIssuerKey = true;
					/* and fall thru */
				case CSSM_OK:
					/* YES */
					certInfo->used(true);
					return certInfo;
				default:
					/* just skip this one and keep looking */
					tpVfyDebug("findIssuerForCertOrCrl issuer/subj match BAD SIG");
					break;
			}
		} 	/* names match */
	}
	/* not found */
	return NULL;
} 	

/*
 * Construct ordered, verified cert chain from a variety of inputs. 
 * Time validity is ignored and needs to be checked by caller (it's
 * stored in each TPCertInfo we add to ourself during construction).
 * The only error returned is CSSMERR_APPLETP_INVALID_ROOT, meaning 
 * we verified back to a supposed root cert which did not in fact
 * self-verify. Other interesting status is returned via the
 * verifiedToRoot and verifiedToAnchor flags. 
 *
 * NOTE: is it the caller's responsibility to call setAllUnused() for both 
 * incoming cert groups (inCertGroup and gatheredCerts). We don't do that
 * here because we may call ourself recursively. 
 */
CSSM_RETURN TPCertGroup::buildCertGroup(
	const TPClItemInfo		&subjectItem,	// Cert or CRL
	TPCertGroup				*inCertGroup,	// optional
	const CSSM_DL_DB_LIST 	*dbList,		// optional
	CSSM_CL_HANDLE 			clHand,
	CSSM_CSP_HANDLE 		cspHand,
	const char				*verifyTime,	// optional, for establishing
											//   validity of new TPCertInfos
	/* trusted anchors, optional */
	/* FIXME - maybe this should be a TPCertGroup */
	uint32 					numAnchorCerts,
	const CSSM_DATA			*anchorCerts,
	
	/* 
	 * Certs to be freed by caller (i.e., TPCertInfo which we allocate
	 * as a result of using a cert from anchorCerts or dbList) are added
	 * to this group.
	 */
	TPCertGroup				&certsToBeFreed,
	
	/*
	 * Other certificates gathered during the course of this operation,
	 * currently consisting of certs fetched from DBs and from the net.
	 * This is not used when called by AppleTPSession::CertGroupConstructPriv;
	 * it's an optimization for the case when we're building a cert group
	 * for TPCrlInfo::verifyWithContext - we avoid re-fetching certs from
	 * the net which are needed to verify both the subject cert and a CRL.
	 */
	TPCertGroup				*gatheredCerts,
	
	/*
	 * Indicates that subjectItem is the last element in this cert group.
	 * If true, that cert will be tested for "root-ness", including 
	 *   -- subject/issuer compare
	 *   -- signature self-verify
	 *   -- anchor compare
	 */
	CSSM_BOOL				subjectIsInGroup,
	
	/* currently, only CSSM_TP_ACTION_FETCH_CERT_FROM_NET is interesting */
	CSSM_APPLE_TP_ACTION_FLAGS	actionFlags,
	
	/* returned */
	CSSM_BOOL				&verifiedToRoot,	// end of chain self-verifies
	CSSM_BOOL				&verifiedToAnchor)	// end of chain in anchors
{
	const TPClItemInfo *thisSubject = &subjectItem;
	CSSM_RETURN crtn = CSSM_OK;
	TPCertInfo *issuerCert = NULL;
	unsigned certDex;
	TPCertInfo *anchorInfo = NULL;
	bool foundPartialIssuer = false;
	
	tpVfyDebug("buildCertGroup top");
	
	/* possible expired root which we'll only use if we can't find 
	 * a better one */
	TPCertInfo *expiredRoot = NULL;
	
	verifiedToRoot = CSSM_FALSE;
	verifiedToAnchor = CSSM_FALSE;
	
	/*** main loop to seach inCertGroup and dbList ***
	 *
	 * Exit loop on: 
	 *   -- find a root cert in the chain
	 *   -- memory error
	 *   -- or no more certs to add to chain. 
	 */
	for(;;) {
		/* 
		 * Top of loop: thisSubject is the item we're trying to verify. 
		 */
		 
		/* is thisSubject a root cert?  */
		if(subjectIsInGroup) {
			TPCertInfo *subjCert = lastCert();
			assert(subjCert != NULL);
			if(subjCert->isSelfSigned()) {
				/* We're at the end of the chain. */
				verifiedToRoot = CSSM_TRUE;
				
				/*
				 * Special case if this root is expired (and it's not the 
				 * leaf): remove it from the outgoing cert group, save it,
				 * and try to proceed with anchor cert processing.
				 */
				if(subjCert->isExpired() && (mNumCerts > 1)) {
					tpDebug("buildCertGroup: EXPIRED ROOT, looking for good one");
					mNumCerts--;
					expiredRoot = subjCert;
					thisSubject = lastCert();
				}
				break;
			}
		}
		
		/* 
		 * Search unused incoming certs to find an issuer.
		 * Both cert groups are optional.
		 * We'll add issuer to outCertGroup below.
		*/
		if(inCertGroup != NULL) {
			bool partial = false;
			issuerCert = inCertGroup->findIssuerForCertOrCrl(*thisSubject, 
				partial);
			if(issuerCert) {
				if(partial) {
					/* deal with this later */
					foundPartialIssuer = true;
					tpDebug("buildCertGroup: PARTIAL Cert FOUND in inCertGroup");		
				}
				else {
					tpDebug("buildCertGroup: Cert FOUND in inCertGroup");		
				}
			}
		}
		if((issuerCert == NULL) && (gatheredCerts != NULL)) {
			bool partial = false;
			issuerCert = gatheredCerts->findIssuerForCertOrCrl(*thisSubject,
				partial);
			if(issuerCert) {
				if(partial) {
					/* deal with this later */
					foundPartialIssuer = true;
					tpDebug("buildCertGroup: PARTIAL Cert FOUND in gatheredCerts");		
				}
				else {
					tpDebug("buildCertGroup: Cert FOUND in gatheredCerts");
				}
			}
		}
		
		if((issuerCert == NULL) && (dbList != NULL)) {
			/* Issuer not in incoming cert group. Search DBList. */
			bool partial = false;
			issuerCert = tpDbFindIssuerCert(mAlloc,
				clHand,
				cspHand,
				thisSubject,
				dbList,
				verifyTime,
				partial);
			if(issuerCert) {
				/* caller must free */
				certsToBeFreed.appendCert(issuerCert);
				if(partial) {
					/* deal with this later */
					foundPartialIssuer = true;
					tpDebug("buildCertGroup: PARTIAL Cert FOUND in dbList");		
				}
				else {
					tpDebug("buildCertGroup: Cert FOUND in dbList");
				}
			}
		}	/*  Issuer not in incoming cert group */
		
		if(issuerCert == NULL) {
			/* end of search, broken chain */
			break;
		}
		
		/*
		 * One way or the other, we've found a cert which verifies subjectCert.
		 * Add the issuer to outCertGroup and make it the new thisSubject for
		 * the next pass.
		 */
		appendCert(issuerCert);
		thisSubject = issuerCert;
		subjectIsInGroup = CSSM_TRUE;
		issuerCert = NULL;
	}	/* main loop */
	
	/* 
	 * This can be NULL if we're evaluating a CRL (and we haven't 
	 * gotten very far).
	 */
	TPCertInfo *endCert = lastCert();
	
	if(numAnchorCerts == 0) {
		/* we're probably done */
		goto post_anchor;
	}
	assert(anchorCerts != NULL);
	
	/*** anchor cert handling ***/
	/*
	 * Case 1: last cert in output is a root cert. See if 
	 * the root cert is in AnchorCerts. This also applies to 
	 * the expiredRoot case; we report a different error for
	 * "we trust the root but it's expired" versus "we don't
	 * trust the root".
	 * Note that the above loop did the actual root self-verify test.
	 * FIXME - shouldn't we be searching for a match in AnchorCerts
	 * whether or not endCert is a root!!?
	 */
	if((endCert && endCert->isSelfSigned()) || expiredRoot) {
		
		TPCertInfo *theRoot;
		if(expiredRoot) {
			/* this is NOT in our outgoing cert group (yet) */
			theRoot = expiredRoot;
		}
		else {
			theRoot = endCert;
		}
		/* see if that root cert is identical to one of the anchor certs */
		for(certDex=0; certDex<numAnchorCerts; certDex++) {
			if(tp_CompareCerts(theRoot->itemData(), &anchorCerts[certDex])) {
				/* one fully successful return */
				verifiedToAnchor = CSSM_TRUE;
				theRoot->isAnchor(true);
				theRoot->index(certDex);
				if(expiredRoot) {
					/* verified to anchor but caller will see 
					 * CSSMERR_TP_CERT_EXPIRED */
					appendCert(expiredRoot);
				}
				/* one more thing: partial public key processing needed? */
				if(foundPartialIssuer) {
					return verifyWithPartialKeys(subjectItem);
				}
				else {
					return CSSM_OK;
				}
			}
		}
		
		if(!expiredRoot) {
			/* verified to a root cert which is not an anchor */
			/* Generally maps to CSSMERR_TP_INVALID_ANCHOR_CERT by caller */
			/* one more thing: partial public key processing needed? */
			if(foundPartialIssuer) {
				return verifyWithPartialKeys(subjectItem);
			}
			else {
				return CSSM_OK;
			}
		}
		/* else try finding a good anchor */
	}

	/* 
	 * Case 2: try to validate thisSubject with anchor certs 
	 */
	for(certDex=0; certDex<numAnchorCerts; certDex++) {
		
		try {
			anchorInfo = new TPCertInfo(clHand,
				cspHand,
				&anchorCerts[certDex], 
				TIC_NoCopy, 
				verifyTime);
		}
		catch(...) {
			/* bad anchor cert - ignore it */
			anchorInfo = NULL;
			continue;
		}
		
		/* 
		 * We must subsequently delete goodAnchor one way or the other.
		 * If we add it to tpCertGroup, we also add it to certsToBeFreed.
		 * Otherwise we delete it.
		 */
		if(!anchorInfo->isIssuerOf(*thisSubject)) {
			/* not this anchor */
			tpAnchorDebug("buildCertGroup anchor not issuer");
			delete anchorInfo;
			anchorInfo = NULL;
			continue;
		}

		crtn = thisSubject->verifyWithIssuer(anchorInfo);
		switch(crtn) {
			case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:
				/*
				 * A bit of a corner case. Found an issuer in AnchorCerts, but
				 * we can't do a signature verify since the issuer has a partial
				 * public key. Proceed but return 
				 * CSSMERR_TP_CERTIFICATE_CANT_OPERATE.
				 */
				 crtn = CSSMERR_TP_CERTIFICATE_CANT_OPERATE;
				 anchorInfo->addStatusCode(CSSMERR_TP_CERTIFICATE_CANT_OPERATE);
				 foundPartialIssuer = true;
				 /* drop thru */
			case CSSM_OK:
				/*  The other normal fully successful return. */
				verifiedToAnchor = CSSM_TRUE;
				if(anchorInfo->isSelfSigned()) {
					verifiedToRoot = CSSM_TRUE;	
				}
				
				/*
				 * Add this anchor cert to the output group 
				 * and to certsToBeFreed.
				 */
				appendCert(anchorInfo);
				anchorInfo->isAnchor(true);
				anchorInfo->index(certDex);
				certsToBeFreed.appendCert(anchorInfo);
				tpDebug("buildCertGroup: Cert FOUND by signer in AnchorList");	
				/* one more thing: partial public key processing needed? */
				if(foundPartialIssuer) {
					return verifyWithPartialKeys(subjectItem);
				}
				else {
					return crtn;
				}
				
			default:
				/* continue to next anchor */
				tpVfyDebug("buildCertGroup found issuer in anchor, BAD SIG");
				delete anchorInfo;
				anchorInfo = NULL;
				break;
		}
	}	/* for each anchor */
	/* regardless of anchor search status... */
	crtn = CSSM_OK;
post_anchor:
	if(expiredRoot) {
		/*
		 * One remaining special case: expiredRoot found in input certs, but 
		 * no luck resolving the problem with the anchors. Go ahead and append
		 * the expired root and return.
		 */
		tpDebug("buildCertGroup: accepting EXPIRED root");
		appendCert(expiredRoot);
		if(foundPartialIssuer) {
			return verifyWithPartialKeys(subjectItem);
		}
		else {
			return CSSM_OK;
		}
	}
	
	/* 
	 * If we haven't verified to a root, and net fetch of certs is enabled,
	 * try to get the issuer of the last cert in the chain from the net.
	 * If that succeeds, then call ourself recursively to perform the 
	 * whole search again (including comparing to or verifying against
	 * anchor certs).
	 */
	if(!verifiedToRoot && !verifiedToAnchor &&
	   (endCert != NULL) &&
	   (actionFlags & CSSM_TP_ACTION_FETCH_CERT_FROM_NET)) {
		TPCertInfo *issuer = NULL;
		CSSM_RETURN cr = tpFetchIssuerFromNet(*endCert,
			clHand,
			cspHand,
			verifyTime,
			issuer);
		switch(cr) {
			case CSSMERR_TP_CERTGROUP_INCOMPLETE:
				/* no issuerAltName, no reason to log this */
				break;
			default:
				/* gross error */
				endCert->addStatusCode(cr);
				break;
			case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:
				/* use this one but re-verify later */
				foundPartialIssuer = true;
				/* and drop thru */
			case CSSM_OK:
				tpDebug("buildCertGroup: Cert FOUND from Net; recursing");
				
				/* add this fetched cert to constructed group */
				appendCert(issuer);
				issuer->isFromNet(true);
				certsToBeFreed.appendCert(issuer);
				
				/* and go again */
				cr = buildCertGroup(*issuer,
					inCertGroup,
					dbList,
					clHand,
					cspHand,
					verifyTime,
					numAnchorCerts,
					anchorCerts,
					certsToBeFreed,
					gatheredCerts,
					CSSM_TRUE,			// subjectIsInGroup	
					actionFlags,
					verifiedToRoot,
					verifiedToAnchor);
				if(cr) {
					return cr;
				}
				
				/* one more thing: partial public key processing needed? */
				if(foundPartialIssuer) {
					return verifyWithPartialKeys(subjectItem);
				}
				else {
					return CSSM_OK;
				}
		}
	}
	/* regardless of outcome, check for partial keys to log per-cert status */
	CSSM_RETURN partRtn = CSSM_OK;
	if(foundPartialIssuer) {
		partRtn = verifyWithPartialKeys(subjectItem);
	}
	if(crtn) {
		return crtn;
	}
	else {
		return partRtn;
	}
}

/* 
 * Called from buildCertGroup as final processing of a constructed
 * group when CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE has been
 * detected. Perform partial public key processing.
 *
 * We don't have to verify every element, just the ones whose
 * issuers have partial public keys.
 *
 * Returns:
 *	 CSSMERR_TP_CERTIFICATE_CANT_OPERATE in the case of an issuer cert 
 *		with a partial public key which can't be completed.
 *	 CSSMERR_TP_INVALID_CERT_AUTHORITY if sig verify failed with 
 *		a (supposedly) completed partial key
 */
CSSM_RETURN TPCertGroup::verifyWithPartialKeys(
	const TPClItemInfo	&subjectItem)		// Cert or CRL
{
	TPCertInfo *lastFullKeyCert = NULL;
	tpDebug("verifyWithPartialKeys top");
	
	/* start from the end - it's easier */
	for(int dex=mNumCerts-1; dex >= 0; dex--) {
		TPCertInfo *thisCert = mCertInfo[dex];
		
		/*
		 * If this is the start of the cert chain, and it's not being
		 * used to verify subjectItem, then we're done.
		 */
		if(dex == 0) {
			if((void *)thisCert == (void *)&subjectItem) {
				tpDebug("verifyWithPartialKeys: success at leaf cert");
				return CSSM_OK;
			}
		}
		if(!thisCert->hasPartialKey()) {
			/* 
			 * Good to know. Record this and move on.
			 */
			lastFullKeyCert = thisCert;
			tpDebug("full key cert found at index %d", dex);
			continue;
		}
		if(lastFullKeyCert == NULL) {
			/*
			 * No full keys between here and the end!
			 */
			tpDebug("UNCOMPLETABLE cert at index %d", dex);
			thisCert->addStatusCode(CSSMERR_TP_CERTIFICATE_CANT_OPERATE);
			return CSSMERR_TP_CERTIFICATE_CANT_OPERATE;
		}
		
		/* do the verify - of next cert in chain or of subjectItem */
		const TPClItemInfo *subject;
		if(dex == 0) {
			subject = &subjectItem;
			tpDebug("...verifying subject item with partial cert 0");
		}
		else {
			subject = mCertInfo[dex - 1];
			tpDebug("...verifying with partial cert %d", dex);
		}
		CSSM_RETURN crtn = subject->verifyWithIssuer(thisCert, 
			lastFullKeyCert);
		if(crtn) {
			tpDebug("CERT VERIFY ERROR with partial cert at index %d", dex);
			thisCert->addStatusCode(CSSMERR_TP_CERTIFICATE_CANT_OPERATE);
			return CSSMERR_TP_INVALID_CERT_AUTHORITY;
		}
	}
	
	/* we just verified subjectItem - right?  */
	assert((void *)mCertInfo[0] != (void *)&subjectItem);
	tpDebug("verifyWithPartialKeys: success at subjectItem");
	return CSSM_OK;
}
