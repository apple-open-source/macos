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
#include <Security/cssmapi.h>
#include <Security/x509defs.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>
#include <string.h>						/* for memcmp */
#include <Security/threading.h>	/* for Mutex */
#include <Security/globalizer.h>
#include <Security/debugging.h>
#include <Security/cssmapple.h>

#define tpTimeDbg(args...)	debug("tpTime", ## args) 

/* 
 * No default constructor - this is the only way.
 * This caches the cert and fetches subjectName and issuerName
 * to ensure the incoming certData is well-constructed.
 */
TPCertInfo::TPCertInfo(
	const CSSM_DATA		*certData,
	CSSM_CL_HANDLE		clHand,
	const char 			*cssmTimeStr,		// = NULL
	bool				copyCertData) :		// true: we copy, we free
											// false - caller owns
		mClHand(clHand),
		mCacheHand(CSSM_INVALID_HANDLE),
		mSubjectName(NULL),
		mIssuerName(NULL),
		mIndex(0),
		mIsAnchor(false),
		mIsFromDb(false),
		mNumStatusCodes(0),
		mStatusCodes(NULL),
		mUniqueRecord(NULL)
{
	CSSM_RETURN	crtn;

	mDlDbHandle.DBHandle = 0;
	mDlDbHandle.DLHandle = 0;
	
	if(copyCertData) {
		mCertData = tpMallocCopyCssmData(CssmAllocator::standard(), certData);
	}
	else {
		mCertData = const_cast<CSSM_DATA *>(certData);
	}
	mWeOwnTheData = copyCertData;
	
	/* cache the cert */
	mClHand = clHand;
	crtn = CSSM_CL_CertCache(clHand, mCertData, &mCacheHand);
	if(crtn) {
		/* bad cert */
		CssmError::throwMe(crtn);
	}
	
	/* fetch subject name */
	crtn = fetchField(&CSSMOID_X509V1SubjectName, &mSubjectName);
	if(crtn) {
		/* bad cert */
		releaseResources();
		CssmError::throwMe(crtn);
	}
	
	/* fetch issuer name */
	crtn = fetchField(&CSSMOID_X509V1IssuerName, &mIssuerName);
	if(crtn) {
		/* bad cert */
		releaseResources();
		CssmError::throwMe(crtn);
	}
	
	/* calculate other commonly used fields */
	mIsRoot = tpCompareCssmData(mSubjectName, mIssuerName) ? true : false;
	calculateCurrent(cssmTimeStr);
}
	
/* frees mSubjectName, mIssuerName, mCacheHand via mClHand */
TPCertInfo::~TPCertInfo()
{
	releaseResources();
}

void TPCertInfo::releaseResources()
{
	if(mWeOwnTheData && (mCertData != NULL)) {
		tpFreeCssmData(CssmAllocator::standard(), mCertData, CSSM_TRUE);
	}
	if(mSubjectName) {
		freeField(&CSSMOID_X509V1SubjectName, mSubjectName);
	}
	if(mIssuerName) {
		freeField(&CSSMOID_X509V1IssuerName, mIssuerName);
	}
	if(mCacheHand != CSSM_INVALID_HANDLE) {
		CSSM_CL_CertAbortCache(mClHand, mCacheHand);
	}
	if(mStatusCodes) {
		free(mStatusCodes);
	}
}

/* fetch arbitrary field from cached cert */
CSSM_RETURN TPCertInfo::fetchField(
	const CSSM_OID	*fieldOid,
	CSSM_DATA_PTR	*fieldData)		// mallocd by CL and RETURNED
{
	CSSM_RETURN crtn;
	
	uint32 NumberOfFields = 0;
	CSSM_HANDLE resultHand = 0;
	*fieldData = NULL;

	crtn = CSSM_CL_CertGetFirstCachedFieldValue(
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
		errorLog1("TPCertInfo::fetchField: numFields %d, expected 1\n", 
			(int)NumberOfFields);
	}
  	CSSM_CL_CertAbortQuery(mClHand, resultHand);
	return CSSM_OK;
}

/* free arbitrary field obtained from fetchField() */
CSSM_RETURN TPCertInfo::freeField( 
	const CSSM_OID	*fieldOid,
	CSSM_DATA_PTR	fieldData)	
{
	return CSSM_CL_FreeFieldValue(mClHand, fieldOid, fieldData);

}

/* accessors */
CSSM_CL_HANDLE TPCertInfo::clHand()
{
	return mClHand;
}

CSSM_HANDLE TPCertInfo::cacheHand()
{
	return mCacheHand;
}

const CSSM_DATA *TPCertInfo::certData()
{
	CASSERT(mCertData != NULL);
	return mCertData;
}

const CSSM_DATA *TPCertInfo::subjectName()
{
	CASSERT(mSubjectName != NULL);
	return mSubjectName;
}

const CSSM_DATA *TPCertInfo::issuerName()						
{
	CASSERT(mIssuerName != NULL);
	return mIssuerName;
}

/* 
 * Verify validity (not before/after). Only throws on gross error
 * (CSSMERR_TP_INVALID_CERT_POINTER, etc.).
 *
 * We use some stdlib time calls over in tpTime.c; the stdlib function
 * gmtime() is not thread-safe, so we do the protection here. Note that
 * this makes *our* calls to gmtime() thread-safe, but if the app has
 * other threads which are also calling gmtime, we're out of luck.
 */
ModuleNexus<Mutex> tpTimeLock;

void TPCertInfo::calculateCurrent(
	const char *cssmTimeStr /* = NULL */)
{
	CSSM_DATA_PTR	notBeforeField = NULL;
	CSSM_DATA_PTR	notAfterField = NULL;
	CSSM_RETURN		crtn = CSSM_OK;
	CSSM_X509_TIME 	*xNotAfter;
	
	CASSERT(mCacheHand != CSSM_INVALID_HANDLE);
	crtn = fetchField(&CSSMOID_X509V1ValidityNotBefore, &notBeforeField);
	if(crtn) {
		errorLog0("TPCertInfo::calculateCurrent: GetField error");
		CssmError::throwMe(crtn);
	}
	
	/* subsequent errors to errOut */
	struct tm now;
	if(cssmTimeStr != NULL) {
		/* caller specifies verification time base */
		if(timeStringToTm(cssmTimeStr, strlen(cssmTimeStr), &now)) {
			errorLog0("TPCertInfo::calculateCurrent: timeStringToTm error");
			CssmError::throwMe(CSSMERR_TP_INVALID_TIMESTRING);
		}
	}
	else {
		/* time base = right now */
		StLock<Mutex> _(tpTimeLock());
		nowTime(&now);
	}
	struct tm notBefore;
	CSSM_X509_TIME *xNotBefore = (CSSM_X509_TIME *)notBeforeField->Data;

	if(timeStringToTm((char *)xNotBefore->time.Data, xNotBefore->time.Length, 
			&notBefore)) {
		errorLog0("TPCertInfo::calculateCurrent: malformed notBefore time\n");
		crtn = CSSMERR_TP_INVALID_CERT_POINTER;
		goto errOut;
	}
	if(compareTimes(&now, &notBefore) < 0) {
		mNotValidYet = true;
		tpTimeDbg("\nTP_CERT_NOT_VALID_YET:\n   now y:%d m:%d d:%d h:%d m:%d",
			now.tm_year, now.tm_mon, now.tm_mday, now.tm_hour, 
			now.tm_min);
		tpTimeDbg(" notBefore y:%d m:%d d:%d h:%d m:%d",
			notBefore.tm_year, notBefore.tm_mon, notBefore.tm_mday, 
			notBefore.tm_hour, notBefore.tm_min);
	}
	else {
		mNotValidYet = false;
	}
	
	struct tm notAfter;
	crtn = fetchField(&CSSMOID_X509V1ValidityNotAfter, &notAfterField);
	if(crtn) {
		errorLog0("TPCertInfo::calculateCurrent: GetField error");
		goto errOut;
	}

	xNotAfter = (CSSM_X509_TIME *)notAfterField->Data;
	if(timeStringToTm((char *)xNotAfter->time.Data, xNotAfter->time.Length, 
			&notAfter)) {
		errorLog0("TPCertInfo::calculateCurrent: malformed notAfter time\n");
		crtn = CSSMERR_TP_INVALID_CERT_POINTER;
		goto errOut;
	}
	else if(compareTimes(&now, &notAfter) > 0) {
		crtn = CSSMERR_TP_CERT_EXPIRED;
		tpTimeDbg("\nTP_CERT_EXPIRED: \n   now y:%d m:%d d:%d "
				"h:%d m:%d",
			now.tm_year, now.tm_mon, now.tm_mday, 
			now.tm_hour, now.tm_min);
		tpTimeDbg(" notAfter y:%d m:%d d:%d h:%d m:%d",
			notAfter.tm_year, notAfter.tm_mon, notAfter.tm_mday, 
			notAfter.tm_hour, notAfter.tm_min);
		mExpired = true;
	}
	else {
		mExpired = false;
	}
	crtn = CSSM_OK;
errOut:
	if(notAfterField) {
		freeField(&CSSMOID_X509V1ValidityNotAfter, notAfterField);
	}
	if(notBeforeField) {
		freeField(&CSSMOID_X509V1ValidityNotBefore, notBeforeField);
	}
	if(crtn != CSSM_OK) {
		CssmError::throwMe(crtn);
	}
}

CSSM_RETURN TPCertInfo::isCurrent(
	CSSM_BOOL		allowExpired)
{
	if(mNotValidYet) {
		return CSSMERR_TP_CERT_NOT_VALID_YET;
	}
	if(allowExpired || !mExpired) {
		return CSSM_OK;
	}
	else {
		return CSSMERR_TP_CERT_EXPIRED;
	}
}

void TPCertInfo::addStatusCode(CSSM_RETURN code)
{
	mNumStatusCodes++;
	mStatusCodes = (CSSM_RETURN *)realloc(mStatusCodes, 
		mNumStatusCodes * sizeof(CSSM_RETURN));
	mStatusCodes[mNumStatusCodes - 1] = code;
}

/***
 *** TPCertGroup class
 ***/
TPCertGroup::TPCertGroup(
	CssmAllocator		&alloc,
	unsigned			numCerts) :
		mAlloc(alloc),
		mNumCerts(0)
{
	mCertInfo = (TPCertInfo **)alloc.malloc(numCerts * sizeof(TPCertInfo *));
	mSizeofCertInfo = numCerts;
}
	
/*
 * Deletes all TPCertInfo's.
 */
TPCertGroup::~TPCertGroup()
{
	unsigned i;
	for(i=0; i<mNumCerts; i++) {
		delete mCertInfo[i];
	}
	mAlloc.free(mCertInfo);
}

/* add/remove/access TPTCertInfo's. */
void TPCertGroup::appendCert(
	TPCertInfo			*certInfo)			// appends to end of mCertInfo
{
	if(mNumCerts == mSizeofCertInfo) {
		/* FIXME - do we need the realloc workaround we used to have in TPSession? */
		mSizeofCertInfo *= 2;
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

unsigned TPCertGroup::numCerts()
{
	return mNumCerts;
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
		/* the caller really should not do this... */
		CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
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
		tpCopyCssmData(mAlloc, mCertInfo[i]->certData(), 
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
 * encapsulates a policy for CertGroupeConstruct and CertGroupVerify.
 */
CSSM_RETURN TPCertGroup::getReturnCode(
	CSSM_RETURN constructStatus,
	CSSM_BOOL	allowExpired,
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
		if(mCertInfo[i]->isExpired()) {
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
