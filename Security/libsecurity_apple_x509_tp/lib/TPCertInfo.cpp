/*
 * Copyright (c) 2000-2011 Apple Inc. All Rights Reserved.
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
 * TPCertInfo.cpp - TP's private certificate info classes
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
#include <security_utilities/threading.h> /* for Mutex */
#include <security_utilities/globalizer.h>
#include <security_utilities/debugging.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssmapple.h>
#include <Security/SecCertificate.h>
#include <Security/SecImportExport.h>
#include <Security/SecTrustSettingsPriv.h>

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
			mNotBefore(NULL),
			mNotAfter(NULL),
			mIsExpired(false),
			mIsNotValidYet(false),
			mIndex(0)
{
	try {
		CSSM_RETURN crtn = cacheItem(itemData, copyItemData);
		if(crtn) {
			CssmError::throwMe(crtn);
		}

		/*
		 * Fetch standard fields...
		 * Issue name assumes same OID for Certs and CRLs!
		 */
		crtn = fetchField(&CSSMOID_X509V1IssuerName, &mIssuerName);
		if(crtn) {
			CssmError::throwMe(crtn);
		}

		/*
		 * Signing algorithm, infer from TBS algId
		 * Note this assumes that the OID for fetching this field is the
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
		if(mSigAlg == CSSM_ALGID_ECDSA_SPECIFIED) {
			/* Further processing needed to get digest algorithm */
			if(decodeECDSA_SigAlgParams(&algId->parameters, &mSigAlg)) {
				tpErrorLog("TPClItemInfo: incomplete/unknown ECDSA signature algorithm\n");
				CssmError::throwMe(CSSMERR_TP_UNKNOWN_FORMAT);
			}
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
		tpFreeCssmData(Allocator::standard(), mItemData, CSSM_TRUE);
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
	if(mNotBefore) {
		CFRelease(mNotBefore);
		mNotBefore = NULL;
	}
	if(mNotAfter) {
		CFRelease(mNotAfter);
		mNotAfter = NULL;
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
		tpCertInfoDbg("TPClItemInfo::fetchField: numFields %d, expected 1\n",
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
			mItemData = tpMallocCopyCssmData(Allocator::standard(), itemData);
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
	if(timeStringToCfDate((char *)xTime->time.Data, (unsigned)xTime->time.Length, &mNotBefore)) {
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
			timeStringToCfDate(CSSM_APPLE_CRL_END_OF_TIME,
				strlen(CSSM_APPLE_CRL_END_OF_TIME),
				&mNotAfter);
		}
	}
	else {
		xTime = (CSSM_X509_TIME *)notAfterField->Data;
		if(timeStringToCfDate((char *)xTime->time.Data, (unsigned)xTime->time.Length, &mNotAfter)) {
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
	CFDateRef refTime = NULL;

	if(verifyString != NULL) {
		/* caller specifies verification time base */
		if(timeStringToCfDate(verifyString, (unsigned)strlen(verifyString), &refTime)) {
			tpErrorLog("calculateCurrent: timeStringToCfDate error\n");
			return CSSMERR_TP_INVALID_TIMESTRING;
		}
	}
	else {
		/* time base = right now */
		refTime = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
	}
	if(compareTimes(refTime, mNotBefore) < 0) {
		mIsNotValidYet = true;
		tpTimeDbg("\nTP_CERT_NOT_VALID_YET: now %g notBefore %g",
			CFDateGetAbsoluteTime(refTime), CFDateGetAbsoluteTime(mNotBefore));
		CFRelease(refTime);
		return mClCalls.notValidYetRtn;
	}
	else {
		mIsNotValidYet = false;
	}

	if(compareTimes(refTime, mNotAfter) > 0) {
		mIsExpired = true;
		tpTimeDbg("\nTP_CERT_EXPIRED: now %g notBefore %g",
			CFDateGetAbsoluteTime(refTime), CFDateGetAbsoluteTime(mNotBefore));
		CFRelease(refTime);
		return mClCalls.expiredRtn;
	}
	else {
		mIsExpired = false;
		CFRelease(refTime);
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
		mPublicKeyData(NULL),
		mPublicKey(NULL),
		mIsAnchor(false),
		mIsFromInputCerts(false),
		mIsFromNet(false),
		mNumStatusCodes(0),
		mStatusCodes(NULL),
		mUniqueRecord(NULL),
		mUsed(false),
		mIsLeaf(false),
		mIsRoot(TRS_Unknown),
		mRevCheckGood(false),
		mRevCheckComplete(false),
		mTrustSettingsEvaluated(false),
		mTrustSettingsDomain(kSecTrustSettingsDomainSystem),
		mTrustSettingsResult(kSecTrustSettingsResultInvalid),
		mTrustSettingsFoundAnyEntry(false),
		mTrustSettingsFoundMatchingEntry(false),
		mAllowedErrs(NULL),
		mNumAllowedErrs(0),
		mIgnoredError(false),
		mTrustSettingsKeyUsage(0),
		mCertHashStr(NULL)
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
	crtn = fetchField(&CSSMOID_CSSMKeyStruct, &mPublicKeyData);
	if(crtn || (mPublicKeyData->Length != sizeof(CSSM_KEY))) {
		/* bad cert */
		releaseResources();
		CssmError::throwMe(crtn);
	}
	mPublicKey = (CSSM_KEY_PTR)mPublicKeyData->Data;

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
	if(mPublicKeyData) {
		freeField(&CSSMOID_CSSMKeyStruct, mPublicKeyData);
		mPublicKey = NULL;
		mPublicKeyData = NULL;
	}
	if(mStatusCodes) {
		free(mStatusCodes);
		mStatusCodes = NULL;
	}
	if(mAllowedErrs) {
		free(mAllowedErrs);
	}
	if(mCertHashStr) {
		CFRelease(mCertHashStr);
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
 * If avoidVerify is true, we won't do the signature verify: caller
 * just wants to know if the subject and issuer names match.
 */
bool TPCertInfo::isSelfSigned(bool avoidVerify)
{
	switch(mIsRoot) {
		case TRS_NotRoot:			// known not to be root
			return false;
		case TRS_IsRoot:
			return true;
		case TRS_NamesMatch:
			if(avoidVerify) {
				return true;
			}
			/* else drop through and verify */
		case TRS_Unknown:			// actually shouldn't happen, but to be safe...
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

bool TPCertInfo::addStatusCode(CSSM_RETURN code)
{
	mNumStatusCodes++;
	mStatusCodes = (CSSM_RETURN *)realloc(mStatusCodes,
		mNumStatusCodes * sizeof(CSSM_RETURN));
	mStatusCodes[mNumStatusCodes - 1] = code;
	return isStatusFatal(code);
}

bool TPCertInfo::hasStatusCode(CSSM_RETURN code)
{
	for(unsigned dex=0; dex<mNumStatusCodes; dex++) {
		if(mStatusCodes[dex] == code) {
			return true;
		}
	}
	return false;
}

bool TPCertInfo::isStatusFatal(CSSM_RETURN code)
{
	for(unsigned dex=0; dex<mNumAllowedErrs; dex++) {
		if(mAllowedErrs[dex] == code) {
			tpTrustSettingsDbg("isStatusFatal(%ld): ALLOWED", (unsigned long)code);
			mIgnoredError = true;
			return false;
		}
	}
	return true;
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

/*
 * <rdar://9145531>
 */
bool TPCertInfo::shouldReject()
{
	static unsigned char _UTN_UF_H_ISSUER_BYTES[154] = {
	  0x30, 0x81, 0x97, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06,
	  0x13, 0x02, 0x55, 0x53, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04,
	  0x08, 0x13, 0x02, 0x55, 0x54, 0x31, 0x17, 0x30, 0x15, 0x06, 0x03, 0x55,
	  0x04, 0x07, 0x13, 0x0e, 0x53, 0x41, 0x4c, 0x54, 0x20, 0x4c, 0x41, 0x4b,
	  0x45, 0x20, 0x43, 0x49, 0x54, 0x59, 0x31, 0x1e, 0x30, 0x1c, 0x06, 0x03,
	  0x55, 0x04, 0x0a, 0x13, 0x15, 0x54, 0x48, 0x45, 0x20, 0x55, 0x53, 0x45,
	  0x52, 0x54, 0x52, 0x55, 0x53, 0x54, 0x20, 0x4e, 0x45, 0x54, 0x57, 0x4f,
	  0x52, 0x4b, 0x31, 0x21, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13,
	  0x18, 0x48, 0x54, 0x54, 0x50, 0x3a, 0x2f, 0x2f, 0x57, 0x57, 0x57, 0x2e,
	  0x55, 0x53, 0x45, 0x52, 0x54, 0x52, 0x55, 0x53, 0x54, 0x2e, 0x43, 0x4f,
	  0x4d, 0x31, 0x1f, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x16,
	  0x55, 0x54, 0x4e, 0x2d, 0x55, 0x53, 0x45, 0x52, 0x46, 0x49, 0x52, 0x53,
	  0x54, 0x2d, 0x48, 0x41, 0x52, 0x44, 0x57, 0x41, 0x52, 0x45
	};
	CSSM_DATA _UTN_UF_H_ISSUER = { sizeof(_UTN_UF_H_ISSUER_BYTES), _UTN_UF_H_ISSUER_BYTES };

	static CSSM_DATA _UTN_UF_H_SERIALS[] = {
		{ 17, (uint8*)"\x00\x92\x39\xd5\x34\x8f\x40\xd1\x69\x5a\x74\x54\x70\xe1\xf2\x3f\x43" }, // amo
		{ 17, (uint8*)"\x00\xd8\xf3\x5f\x4e\xb7\x87\x2b\x2d\xab\x06\x92\xe3\x15\x38\x2f\xb0" }, // gt
		{ 17, (uint8*)"\x00\xb0\xb7\x13\x3e\xd0\x96\xf9\xb5\x6f\xae\x91\xc8\x74\xbd\x3a\xc0" }, // llc
		{ 17, (uint8*)"\x00\xe9\x02\x8b\x95\x78\xe4\x15\xdc\x1a\x71\x0a\x2b\x88\x15\x44\x47" }, // lsc
		{ 17, (uint8*)"\x00\xd7\x55\x8f\xda\xf5\xf1\x10\x5b\xb2\x13\x28\x2b\x70\x77\x29\xa3" }, // lyc
		{ 16, (uint8*)"\x39\x2a\x43\x4f\x0e\x07\xdf\x1f\x8a\xa3\x05\xde\x34\xe0\xc2\x29" },     // lyc1
		{ 16, (uint8*)"\x3e\x75\xce\xd4\x6b\x69\x30\x21\x21\x88\x30\xae\x86\xa8\x2a\x71" },     // lyc2
		{ 16, (uint8*)"\x04\x7e\xcb\xe9\xfc\xa5\x5f\x7b\xd0\x9e\xae\x36\xe1\x0c\xae\x1e" },     // mgc
		{ 17, (uint8*)"\x00\xf5\xc8\x6a\xf3\x61\x62\xf1\x3a\x64\xf5\x4f\x6d\xc9\x58\x7c\x06" }, // wgc
		{ 0, NULL }
	};

	const CSSM_DATA *issuer=issuerName();
	if(!issuer || !(tpCompareCssmData(issuer, &_UTN_UF_H_ISSUER)))
		return false;

	CSSM_DATA *serialNumber=NULL;
	CSSM_RETURN crtn = fetchField(&CSSMOID_X509V1SerialNumber, &serialNumber);
	if(crtn || !serialNumber)
		return false;

	CSSM_DATA *p=_UTN_UF_H_SERIALS;
	bool matched=false;
	while(p->Length) {
		if(tpCompareCssmData(serialNumber, p)) {
			matched=true;
			addStatusCode(CSSMERR_TP_CERT_REVOKED);
			break;
		}
		++p;
	}
	freeField(&CSSMOID_X509V1SerialNumber, serialNumber);
	return matched;
}

/*
 * Evaluate trust settings; returns true in *foundMatchingEntry if positive
 * match found - i.e., cert chain construction is done.
 */
OSStatus TPCertInfo::evaluateTrustSettings(
	const CSSM_OID &policyOid,
	const char *policyString,			// optional
	uint32 policyStringLen,
	SecTrustSettingsKeyUsage keyUse,	// required
	bool *foundMatchingEntry,			// RETURNED
	bool *foundAnyEntry)				// RETURNED
{
	/*
	 * We might have to force a re-evaluation if the requested key usage
	 * is not a subset of what we already checked for (and cached).
	 */
	if(mTrustSettingsEvaluated) {
		bool doFlush = false;
		if(mTrustSettingsKeyUsage != kSecTrustSettingsKeyUseAny) {
			if(keyUse == kSecTrustSettingsKeyUseAny) {
				/* now want "any", checked something else before */
				doFlush = true;
			}
			else if((keyUse & mTrustSettingsKeyUsage) != keyUse) {
				/* want bits that we didn't ask for before */
				doFlush = true;
			}
		}
		if(doFlush) {
			tpTrustSettingsDbg("evaluateTrustSettings: flushing cached trust for "
				"%p due to keyUse 0x%x", this, (int)keyUse);
			mTrustSettingsEvaluated = false;
			mTrustSettingsFoundAnyEntry = false;
			mTrustSettingsResult = kSecTrustSettingsResultInvalid;
			mTrustSettingsFoundMatchingEntry = false;
			if(mAllowedErrs != NULL) {
				free(mAllowedErrs);
			}
			mNumAllowedErrs = 0;
		}
		/* else we can safely use the cached values */
	}
	if(!mTrustSettingsEvaluated) {

		if(mCertHashStr == NULL) {
			const CSSM_DATA *certData = itemData();
			mCertHashStr = SecTrustSettingsCertHashStrFromData(certData->Data,
				certData->Length);
		}

		OSStatus ortn = SecTrustSettingsEvaluateCert(mCertHashStr,
			&policyOid,
			policyString,
			policyStringLen,
			keyUse,
			/*
			 * This is the purpose of the avoidVerify option, right here.
			 * If this is a root cert and it has trust settings, we avoid
			 * the signature verify. If it turns out there are no trust
			 * settings and this is a root, we'll verify the signature
			 * elsewhere (e.g. post_trust_setting: in buildCertGroup()).
			 */
			isSelfSigned(true),
			&mTrustSettingsDomain,
			&mAllowedErrs,
			&mNumAllowedErrs,
			&mTrustSettingsResult,
			&mTrustSettingsFoundMatchingEntry,
			&mTrustSettingsFoundAnyEntry);
		if(ortn) {
			tpTrustSettingsDbg("evaluateTrustSettings: SecTrustSettingsEvaluateCert error!");
			return ortn;
		}
		mTrustSettingsEvaluated = true;
		mTrustSettingsKeyUsage = keyUse;
		#ifndef	NDEBUG
		if(mTrustSettingsFoundMatchingEntry) {
			tpTrustSettingsDbg("evaluateTrustSettings: found for %p result %d",
				this, (int)mTrustSettingsResult);
		}
		#endif
		/* one more thing... */
		if(shouldReject()) {
			return CSSMERR_TP_INVALID_CERTIFICATE;
		}
	}
	*foundMatchingEntry = mTrustSettingsFoundMatchingEntry;
	*foundAnyEntry = mTrustSettingsFoundAnyEntry;

	return errSecSuccess;
}

/* true means "verification terminated due to user trust setting" */
bool TPCertInfo::trustSettingsFound()
{
	switch(mTrustSettingsResult) {
		case kSecTrustSettingsResultUnspecified:	/* entry but not definitive */
		case kSecTrustSettingsResultInvalid:		/* no entry */
			return false;
		default:
			return true;
	}
}

/*
 * Determine if this has an empty SubjectName field. Returns true if so.
 */
bool TPCertInfo::hasEmptySubjectName()
{
	/*
	 * A "pure" empty subject is two bytes (0x30 00) - constructed sequence,
	 * short form length, length 0. We'll be robust and tolerate a missing
	 * field, as well as a possible BER-encoded subject with some extra cruft.
	 */
	if((mSubjectName == NULL) || (mSubjectName->Length <= 4)) {
		return true;
	}
	else {
		return false;
	}
}

/*
 * Free mUniqueRecord if it exists.
 * This is *not* done in our destructor because this record sometimes
 * has to persist in the form of a CSSM evidence chain.
 */
void TPCertInfo::freeUniqueRecord()
{
	if(mUniqueRecord == NULL) {
		return;
	}
	tpDbDebug("freeUniqueRecord: freeing cert record %p", mUniqueRecord);
	CSSM_DL_FreeUniqueRecord(mDlDbHandle, mUniqueRecord);
}

/***
 *** TPCertGroup class
 ***/

/* build empty group */
TPCertGroup::TPCertGroup(
	Allocator			&alloc,
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
	Allocator				&alloc,
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
		if(certInfo->isAnchor()) {
			tpAnchorDebug("buildCssmEvidenceInfo: flagging IS_IN_ANCHORS");
			evInfo->StatusBits |= CSSM_CERT_STATUS_IS_IN_ANCHORS;
		}
		if(certInfo->dlDbHandle().DLHandle == 0) {
			if(certInfo->isFromNet()) {
				evInfo->StatusBits |= CSSM_CERT_STATUS_IS_FROM_NET;
			}
			else if(certInfo->isFromInputCerts()) {
				evInfo->StatusBits |= CSSM_CERT_STATUS_IS_IN_INPUT_CERTS;
			}
		}
		/* If trust settings apply to a root, skip verifying the signature */
		bool avoidVerify = false;
		switch(certInfo->trustSettingsResult()) {
			case kSecTrustSettingsResultTrustRoot:
			case kSecTrustSettingsResultTrustAsRoot:
				/* these two can be disambiguated by IS_ROOT */
				evInfo->StatusBits |= CSSM_CERT_STATUS_TRUST_SETTINGS_TRUST;
				avoidVerify = true;
				break;
			case kSecTrustSettingsResultDeny:
				evInfo->StatusBits |= CSSM_CERT_STATUS_TRUST_SETTINGS_DENY;
				avoidVerify = true;
				break;
			case kSecTrustSettingsResultUnspecified:
			case kSecTrustSettingsResultInvalid:
			default:
				break;
		}
		if(certInfo->isSelfSigned(avoidVerify)) {
			evInfo->StatusBits |= CSSM_CERT_STATUS_IS_ROOT;
		}
		if(certInfo->ignoredError()) {
			evInfo->StatusBits |= CSSM_CERT_STATUS_TRUST_SETTINGS_IGNORED_ERROR;
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
		if(evInfo->StatusBits & (CSSM_CERT_STATUS_TRUST_SETTINGS_TRUST |
								 CSSM_CERT_STATUS_TRUST_SETTINGS_DENY |
								 CSSM_CERT_STATUS_TRUST_SETTINGS_IGNORED_ERROR)) {
			/* Something noteworthy happened involving TrustSettings */
			uint32 whichDomain = 0;
			switch(certInfo->trustSettingsDomain()) {
				case kSecTrustSettingsDomainUser:
					whichDomain = CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_USER;
					break;
				case kSecTrustSettingsDomainAdmin:
					whichDomain = CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_ADMIN;
					break;
				case kSecTrustSettingsDomainSystem:
					whichDomain = CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_SYSTEM;
					break;
			}
			evInfo->StatusBits |= whichDomain;
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
	CSSM_RETURN					constructStatus,
	CSSM_RETURN					policyStatus,
	CSSM_APPLE_TP_ACTION_FLAGS	actionFlags)
{
	if(constructStatus) {
		/* CSSMERR_TP_NOT_TRUSTED, CSSMERR_TP_INVALID_ANCHOR_CERT, gross errors */
		return constructStatus;
	}

	bool expired = false;
	bool postdated = false;
	bool allowExpiredRoot = (actionFlags & CSSM_TP_ACTION_ALLOW_EXPIRED_ROOT) ?
		true : false;
	bool allowExpired = (actionFlags & CSSM_TP_ACTION_ALLOW_EXPIRED) ? true : false;
	bool allowPostdated = allowExpired; // flag overrides any temporal invalidity
	bool requireRevPerCert = (actionFlags & CSSM_TP_ACTION_REQUIRE_REV_PER_CERT) ?
		true : false;

	/* check for expired, not valid yet */
	for(unsigned i=0; i<mNumCerts; i++) {
		TPCertInfo *ci = mCertInfo[i];
		/*
		 * Note avoidVerify = true for isSelfSigned(); if it were appropriate to
		 * verify the signature, that would have happened in
		 * buildCssmEvidenceInfo() at the latest.
		 */
		if(ci->isExpired() &&
		   !(allowExpiredRoot && ci->isSelfSigned(true)) &&		// allowed globally
		    ci->isStatusFatal(CSSMERR_TP_CERT_EXPIRED)) {	// allowed for this cert
			expired = true;
		}
		if(ci->isNotValidYet() &&
		   ci->isStatusFatal(CSSMERR_TP_CERT_NOT_VALID_YET)) {
			postdated = true;
		}
	}
	if(expired && !allowExpired) {
		return CSSMERR_TP_CERT_EXPIRED;
	}
	if(postdated && !allowPostdated) {
		return CSSMERR_TP_CERT_NOT_VALID_YET;
	}

	/* Check for missing revocation check */
	if(requireRevPerCert) {
		for(unsigned i=0; i<mNumCerts; i++) {
			TPCertInfo *ci = mCertInfo[i];
			if(ci->isSelfSigned(true)) {
				/* revocation check meaningless for a root cert */
				tpDebug("getReturnCode: ignoring revocation for self-signed cert %d", i);
				continue;
			}
			if(!ci->revokeCheckGood() &&
			   ci->isStatusFatal(CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK)) {
				tpDebug("getReturnCode: FATAL: revocation check incomplete for cert %d", i);
				return CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK;
			}
			#ifndef	NDEBUG
			else {
				tpDebug("getReturnCode: revocation check %s for cert %d",
					(ci->revokeCheckGood()) ? "GOOD" : "OK", i);
			}
			#endif
		}
	}
	return policyStatus;
}

/* set all TPCertInfo.mUsed flags false */
void TPCertGroup::setAllUnused()
{
	for(unsigned dex=0; dex<mNumCerts; dex++) {
		mCertInfo[dex]->used(false);
	}
}

/*
 * See if the specified error status is allowed (return true) or
 * fatal (return false) per each cert's mAllowedErrs[]. Returns
 * true if any cert returns false for its isStatusFatal() call.
 * The list of errors which can apply to cert-chain-wide allowedErrors
 * is right here; if the incoming error is not in that list, we
 * return false. If the incoming error code is CSSM_OK we return
 * true as a convenience for our callers.
 */
bool TPCertGroup::isAllowedError(
	CSSM_RETURN	code)
{
	switch(code) {
		case CSSM_OK:
			return true;
		case CSSMERR_TP_NOT_TRUSTED:
		case CSSMERR_TP_INVALID_ANCHOR_CERT:
		case CSSMERR_TP_VERIFY_ACTION_FAILED:
		case CSSMERR_TP_INVALID_CERT_AUTHORITY:
		case CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH:
		case CSSMERR_APPLETP_RS_BAD_CERT_CHAIN_LENGTH:
			/* continue processing these candidates */
			break;
		default:
			/* not a candidate for cert-chain-wide allowedErrors */
			return false;
	}

	for(unsigned dex=0; dex<mNumCerts; dex++) {
		if(!mCertInfo[dex]->isStatusFatal(code)) {
			tpTrustSettingsDbg("TPCertGroup::isAllowedError: allowing for cert %u",
				dex);
			return true;
		}
	}

	/* every cert thought this was fatal; it is. */
	return false;
}

/*
 * Determine if we already have the specified cert in this group.
 */
bool TPCertGroup::isInGroup(TPCertInfo &certInfo)
{
	for(unsigned dex=0; dex<mNumCerts; dex++) {
		if(tpCompareCssmData(certInfo.itemData(), mCertInfo[dex]->itemData())) {
			return true;
		}
	}
	return false;
}

/*
 * Encode issuing certs in this group as a PEM-encoded data blob.
 * Caller must free.
 */
void TPCertGroup::encodeIssuers(CSSM_DATA &issuers)
{
	/* FIXME: probably want to rewrite this using pemEncode() from libsecurity_cdsa_utils,
	 * since use of Sec* APIs from this layer violates the API reentrancy contract.
	 */
	issuers.Data = NULL;
	issuers.Length = 0;
	CFMutableArrayRef certArray = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	if(!certArray) {
		return;
	}
	for(unsigned certDex=0; certDex<mNumCerts; certDex++) {
		TPCertInfo *certInfo = certAtIndex(certDex);
		if(!certDex && mNumCerts > 1) {
			continue; /* don't need the leaf */
		}
		CSSM_DATA *cssmData = (CSSM_DATA*)((certInfo) ? certInfo->itemData() : NULL);
		if(!cssmData || !cssmData->Data || !cssmData->Length) {
			continue;
		}
		CFDataRef dataRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
			(const UInt8 *)cssmData->Data, cssmData->Length,
			kCFAllocatorNull);
		if(!dataRef) {
			continue;
		}
		SecCertificateRef certRef = SecCertificateCreateWithData(kCFAllocatorDefault,
			dataRef);
		if(!certRef) {
			CFRelease(dataRef);
			continue;
		}
		CFArrayAppendValue(certArray, certRef);
		CFRelease(certRef);
		CFRelease(dataRef);
	}
	CFDataRef exportedPEMData = NULL;
	OSStatus status = SecItemExport(certArray,
		kSecFormatPEMSequence,
		kSecItemPemArmour,
		NULL,
		&exportedPEMData);
	CFRelease(certArray);

	if(!status)	{
		uint8 *dataPtr = (uint8*)CFDataGetBytePtr(exportedPEMData);
		size_t dataLen = CFDataGetLength(exportedPEMData);
		issuers.Data = (uint8*)malloc(dataLen);
		memmove(issuers.Data, dataPtr, dataLen);
		issuers.Length = dataLen;
		CFRelease(exportedPEMData);
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
	TPCertInfo *expiredIssuer = NULL;

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
					/*
					 * Temporal validity check: if we're not already holding an expired
					 * issuer, and this one's invalid, hold it and keep going.
					 */
					if((crtn == CSSM_OK) && (expiredIssuer == NULL)) {
						if(certInfo->isExpired() || certInfo->isNotValidYet()) {
							tpDebug("findIssuerForCertOrCrl: holding expired cert %p",
								certInfo);
							expiredIssuer = certInfo;
							break;
						}
					}
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
	if(expiredIssuer != NULL) {
		/* OK, we'll use this one */
		tpDbDebug("findIssuerForCertOrCrl: using expired cert %p", expiredIssuer);
		expiredIssuer->used(true);
		return expiredIssuer;
	}

	/* not found */
	return NULL;
}

/*
 * Construct ordered, verified cert chain from a variety of inputs.
 * Time validity does not affect the function return or any status,
 * we always try to find a valid cert to replace an expired or
 * not-yet-valid cert if we can. Final temporal validity of each
 * cert must be checked by caller (it's stored in each TPCertInfo
 * we add to ourself during construction).
 *
 * Only possible error returns are:
 *	 CSSMERR_TP_CERTIFICATE_CANT_OPERATE : issuer cert was found with a partial
 *			public key, rendering full verification impossible.
 *   CSSMERR_TP_INVALID_CERT_AUTHORITY : issuer cert was found with a partial
 *			public key and which failed to perform subsequent signature
 *			verification.
 *
 * Other interesting status is returned via the verifiedToRoot and
 * verifiedToAnchor flags.
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
	 * We don't modify this TPCertGroup, we only use certs from it.
	 */
	TPCertGroup				*gatheredCerts,

	/*
	 * Indicates that subjectItem is a cert in this cert group.
	 * If true, that cert will be tested for "root-ness", including
	 *   -- subject/issuer compare
	 *   -- signature self-verify
	 *   -- anchor compare
	 */
	CSSM_BOOL				subjectIsInGroup,

	/*
	 * CSSM_TP_ACTION_FETCH_CERT_FROM_NET,
	 * CSSM_TP_ACTION_TRUST_SETTING,
	 * CSSM_TP_ACTION_IMPLICIT_ANCHORS are interesting
	 */
	CSSM_APPLE_TP_ACTION_FLAGS	actionFlags,

	/* CSSM_TP_ACTION_TRUST_SETTING parameters */
	const CSSM_OID			*policyOid,
	const char				*policyStr,
	uint32					policyStrLen,
	SecTrustSettingsKeyUsage leafKeyUse,				// usage of *first* cert in chain

	/* returned */
	CSSM_BOOL				&verifiedToRoot,			// end of chain self-verifies
	CSSM_BOOL				&verifiedToAnchor,			// end of chain in anchors
	CSSM_BOOL				&verifiedViaTrustSettings)	// chain ends per User Trust setting
{
	const TPClItemInfo *thisSubject = &subjectItem;
	CSSM_RETURN crtn = CSSM_OK;
	TPCertInfo *issuerCert = NULL;
	unsigned certDex;
	TPCertInfo *anchorInfo = NULL;
	bool foundPartialIssuer = false;
	bool attemptNetworkFetch = false;
	CSSM_BOOL firstSubjectIsInGroup = subjectIsInGroup;
	TPCertInfo *endCert;

	tpVfyDebug("buildCertGroup top");

	/* possible expired root which we'll only use if we can't find
	 * a better one */
	TPCertInfo *expiredRoot = NULL;

	/* and the general case of an expired or not yet valid cert */
	TPCertInfo *expiredIssuer = NULL;

	verifiedToRoot = CSSM_FALSE;
	verifiedToAnchor = CSSM_FALSE;
	verifiedViaTrustSettings = CSSM_FALSE;

	/*** main loop to seach inCertGroup and dbList ***
	 *
	 * Exit loop on:
	 *   -- find a root cert in the chain (self-signed)
	 *   -- find a non-root cert which is also in the anchors list
	 *   -- find a cert which is trusted per Trust Settings (if enabled)
	 *   -- memory error
	 *   -- or no more certs to add to chain.
	 */
	for(;;) {
		/*
		 * Top of loop: thisSubject is the item we're trying to verify.
		 */

		/* is thisSubject a root cert or listed in user trust list?  */
		if(subjectIsInGroup) {
			TPCertInfo *subjCert = lastCert();
			assert(subjCert != NULL);

			if(actionFlags & CSSM_TP_ACTION_TRUST_SETTINGS)	{
				assert(policyOid != NULL);

				/*
				 * Figure out key usage. If this is a leaf cert, the caller - actually
				 * the per-policy code - inferred the usage. Else it could be for
				 * verifying a cert or a CRL.
				 *
				 * We want to avoid multiple calls to the effective portion of
				 * evaluateTrustSettings(), but a CA cert could be usable for only
				 * signing certs and not CRLs. Thus we're evaluating a CA cert,
				 * try to evaluate for signing certs *and* CRLs in case we come
				 * this way again later when performing CRL verification. If that
				 * fails, then retry with just cert signing.
				 */
				SecTrustSettingsKeyUsage localKeyUse;
				bool doRetry = false;
				if(subjCert == firstCert()) {
					/* leaf - use caller's spec */
					localKeyUse = leafKeyUse;
					/* FIXME - add in CRL if this is cert checking? */
				}
				else {
					localKeyUse = kSecTrustSettingsKeyUseSignCert | kSecTrustSettingsKeyUseSignRevocation;
					/* and if necessary */
					doRetry = true;
				}
				/* this lets us avoid searching for the same thing twice when there
				 * is in fact no entry for it */
				bool foundEntry = false;
				bool trustSettingsFound = false;
				OSStatus ortn = subjCert->evaluateTrustSettings(*policyOid,
					policyStr, policyStrLen, localKeyUse, &trustSettingsFound, &foundEntry);
				if(ortn) {
					/* this is only a dire error */
					crtn = ortn;
					goto final_out;
				}
				if(!trustSettingsFound && foundEntry && doRetry) {
					tpTrustSettingsDbg("buildCertGroup: retrying evaluateTrustSettings with Cert only");
					ortn = subjCert->evaluateTrustSettings(*policyOid,
						policyStr, policyStrLen, kSecTrustSettingsKeyUseSignCert,
						&trustSettingsFound, &foundEntry);
					if(ortn) {
						crtn = ortn;
						goto final_out;
					}
				}
				if(trustSettingsFound) {
					switch(subjCert->trustSettingsResult()) {
						case kSecTrustSettingsResultInvalid:
							/* should not happen... */
							assert(0);
							crtn = CSSMERR_TP_INTERNAL_ERROR;
							break;
						case kSecTrustSettingsResultTrustRoot:
						case kSecTrustSettingsResultTrustAsRoot:
							tpTrustSettingsDbg("Trust[As]Root found");
							crtn = CSSM_OK;
							break;
						case kSecTrustSettingsResultDeny:
							tpTrustSettingsDbg("TrustResultDeny found");
							crtn = CSSMERR_APPLETP_TRUST_SETTING_DENY;
							break;
						case kSecTrustSettingsResultUnspecified:
							/* special case here: this means "keep going, we don't trust or
 							 * distrust this cert". Typically used to express allowed errors
							 * only.
							 */
							tpTrustSettingsDbg("TrustResultUnspecified found");
							goto post_trust_setting;
						default:
							tpTrustSettingsDbg("Unknown TrustResult (%d)",
								(int)subjCert->trustSettingsResult());
							crtn = CSSMERR_TP_INTERNAL_ERROR;
							break;
					}
					/* cleanup partial key processing */
					verifiedViaTrustSettings = CSSM_TRUE;
					goto final_out;
				}
			}	/* CSSM_TP_ACTION_TRUST_SETTING */

post_trust_setting:
			if(subjCert->isSelfSigned()) {
				/* We're at the end of the chain. */
				verifiedToRoot = CSSM_TRUE;

				/*
				 * Special case if this root is temporally invalid (and it's not
				 * the leaf): remove it from the outgoing cert group, save it,
				 * and proceed, looking another (good) root in anchors.
				 * There's no way we'll find another good one in this loop.
				 */
				if((subjCert->isExpired() || subjCert->isNotValidYet()) &&
				   (!firstSubjectIsInGroup || (mNumCerts > 1))) {
					tpDebug("buildCertGroup: EXPIRED ROOT %p, looking for good one", subjCert);
					expiredRoot = subjCert;
					if(mNumCerts) {
						/* roll back to previous cert */
						mNumCerts--;
					}
					if(mNumCerts == 0) {
						/* roll back to caller's initial condition */
						thisSubject = &subjectItem;
					}
					else {
						thisSubject = lastCert();
					}
				}
				break;		/* out of main loop */
			}	/* root */

			/*
			 * If this non-root cert is in the provided anchors list,
			 * we can stop building the chain at this point.
			 *
			 * If this cert is a leaf, the chain ends in an anchor, but if it's
			 * also temporally invalid, we can't do anything further. However,
			 * if it's not a leaf, then we need to roll back the chain to a
			 * point just before this cert, so Case 1 will subsequently find
			 * the anchor (and handle the anchor correctly if it's expired.)
			 */
			if(numAnchorCerts && anchorCerts) {
				bool foundNonRootAnchor = false;
				for(certDex=0; certDex<numAnchorCerts; certDex++) {
					if(tp_CompareCerts(subjCert->itemData(), &anchorCerts[certDex])) {
						foundNonRootAnchor = true;
						/* if it's not the leaf, remove it from the outgoing cert group. */
						if(!firstSubjectIsInGroup || (mNumCerts > 1)) {
							if(mNumCerts) {
								/* roll back to previous cert */
								mNumCerts--;
							}
							if(mNumCerts == 0) {
								/* roll back to caller's initial condition */
								thisSubject = &subjectItem;
							}
							else {
								thisSubject = lastCert();
							}
							tpAnchorDebug("buildCertGroup: CA cert in input AND anchors");
						} /* not leaf */
						else {
							if(subjCert->isExpired() || subjCert->isNotValidYet()) {
								crtn = CSSM_CERT_STATUS_EXPIRED;
							} else {
								crtn = CSSM_OK;
							}
							subjCert->isAnchor(true);
							verifiedToAnchor = CSSM_TRUE;
							tpAnchorDebug("buildCertGroup: leaf cert in input AND anchors");
						} /* leaf */
						break;	/* out of anchor-checking loop */
					}
				}
				if(foundNonRootAnchor) {
					break; /* out of main loop */
				}
			} /* non-root */

		}	/* subjectIsInGroup */

		/*
		 * Search unused incoming certs to find an issuer.
		 * Both cert groups are optional.
		 * We'll add issuer to outCertGroup below.
		 * If we find  a cert that's expired or not yet valid, we hold on to it
		 * and look for a better one. If we don't find it here we drop back to the
		 * expired one at the end of the loop. If that expired cert is a root
		 * cert, we'll use the expiredRoot mechanism (see above) to roll back and
		 * see if we can find a good root in the incoming anchors.
	 	 */
		if(inCertGroup != NULL) {
			bool partial = false;
			issuerCert = inCertGroup->findIssuerForCertOrCrl(*thisSubject,
				partial);
			if(issuerCert) {
				issuerCert->isFromInputCerts(true);
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
		if(issuerCert != NULL) {
			if(issuerCert->isExpired() || issuerCert->isNotValidYet()) {
				if(expiredIssuer == NULL) {
					tpDebug("buildCertGroup: saving expired cert %p (1)", issuerCert);
					expiredIssuer = issuerCert;
					issuerCert = NULL;
				}
				/* else we already have an expired issuer candidate */
			}
			else {
				/* unconditionally done with possible expiredIssuer */
				#ifndef	NDEBUG
				if(expiredIssuer != NULL) {
					tpDebug("buildCertGroup: DISCARDING expired cert %p (1)", expiredIssuer);
				}
				#endif
				expiredIssuer = NULL;
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

		if(issuerCert != NULL) {
			if(issuerCert->isExpired() || issuerCert->isNotValidYet()) {
				if(expiredIssuer == NULL) {
					tpDebug("buildCertGroup: saving expired cert %p (2)", issuerCert);
					expiredIssuer = issuerCert;
					issuerCert = NULL;
				}
				/* else we already have an expired issuer candidate */
			}
			else {
				/* unconditionally done with possible expiredIssuer */
				#ifndef	NDEBUG
				if(expiredIssuer != NULL) {
					tpDebug("buildCertGroup: DISCARDING expired cert %p (2)", expiredIssuer);
				}
				#endif
				expiredIssuer = NULL;
			}
		}

		if((issuerCert == NULL) && (dbList != NULL)) {
			/* Issuer not in incoming cert group or gathered certs. Search DBList. */
			bool partial = false;
			try {
				issuerCert = tpDbFindIssuerCert(mAlloc,
					clHand,
					cspHand,
					thisSubject,
					dbList,
					verifyTime,
					partial);
			}
			catch (...) {}

			if(issuerCert) {
				/* unconditionally done with possible expiredIssuer */
				#ifndef	NDEBUG
				if(expiredIssuer != NULL) {
					tpDebug("buildCertGroup: DISCARDING expired cert %p (3)", expiredIssuer);
				}
				#endif
				expiredIssuer = NULL;

				/*
				 * Handle Radar 4566041, endless loop of cross-signed certs.
				 * This can only happen when fetching certs from a DLDB or
				 * from the net; we prevent that from happening when the certs
				 * are in inCertGroup or gatheredCerts by keeping track of those
				 * certs' mUsed state.
				 */
				if(isInGroup(*issuerCert)) {
					tpDebug("buildCertGroup: Multiple instances of cert");
					delete issuerCert;
					issuerCert = NULL;
				}
				else {
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
			}
		}	/*  searching DLDB list */

		/*
		 * Note: we don't handle an expired cert returned from tpDbFindIssuerCert()
		 * in any special way like we do with findIssuerForCertOrCrl().
		 * tpDbFindIssuerCert() does its best to give us a temporally valid cert; if
		 * it returns an expired cert (or, if findIssuerForCertOrCrl() gave us an
		 * expired cert and tpDbFindIssuerCert() could not do any better), that's all
		 * we have to work with at this point. We'll go back to the top of the loop
		 * and apply trust settings if enabled; if an expired cert is trusted per
		 * Trust Settings, we're done. (Note that anchors are fetched from a DLDB
		 * when Trust Settings are enabled, so even if two roots with the same key
		 * and subject name are in DLDBs, and one of them is expired, we'll have the
		 * good one at this time because of tpDbFindIssuerCert()'s ability to find
		 * the best cert.)
		 *
		 * If Trust Settings are not enabled, and we have an expired root at this
		 * point, the expiredRoot mechanism is used to roll back and search for
		 * an anchor that verifies the last good cert.
		 */

		if((issuerCert == NULL) &&			/* tpDbFindIssuerCert() hasn't found one and
											 * we don't have a good one */
		   (expiredIssuer != NULL)) {		/* but we have an expired candidate */
			/*
			 * OK, we'll take the expired issuer.
			 * Note we don't have to free expiredIssuer if we found a good one since
			 * expiredIssuer can only come from inCertGroup or gatheredCerts (not from
			 * dbList).
			 */
			tpDebug("buildCertGroup: USING expired cert %p", expiredIssuer);
			issuerCert = expiredIssuer;
			expiredIssuer = NULL;
		}
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
	endCert = lastCert();

	/*
	 * This, on the other hand, is always valid. It could be a CRL.
	 */
	assert(thisSubject != NULL);

	if( (actionFlags & CSSM_TP_ACTION_IMPLICIT_ANCHORS) &&
		( (endCert && endCert->isSelfSigned()) || expiredRoot) ) {
		/*
		 * Caller will be satisfied with this; skip further anchor processing.
		 */
		tpAnchorDebug("buildCertGroup: found IMPLICIT anchor");
		goto post_anchor;
	}
	if(numAnchorCerts == 0) {
		/* we're probably done */
		goto post_anchor;
	}
	assert(anchorCerts != NULL);

	/*** anchor cert handling ***/

	/*
	 * Case 1: If thisSubject is not a root cert, try to validate with incoming anchor certs.
	 */
	expiredIssuer = NULL;
	if(!(endCert && endCert->isSelfSigned())) {
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
			 * We must subsequently delete anchorInfo one way or the other.
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

			if(crtn == CSSM_OK) {
				if(anchorInfo->isExpired() || anchorInfo->isNotValidYet()) {
					if(expiredIssuer == NULL) {
						/*
						 * Hang on to this one; keep looking for a better one.
						 */
						tpDebug("buildCertGroup: saving expired anchor %p", anchorInfo);
						expiredIssuer = anchorInfo;
						/* flag this condition for the switch below */
						crtn = CSSM_CERT_STATUS_EXPIRED;
						expiredIssuer->isAnchor(true);
						assert(!anchorInfo->isFromInputCerts());
						expiredIssuer->index(certDex);
						certsToBeFreed.appendCert(expiredIssuer);
					}
					/* else we already have an expired candidate anchor */
				}
				else {
					/*
					 * Done with possible expiredIssuer. We don't delete it, since we already added
					 * it to certsToBeFreed, above.
					 */
					if(expiredIssuer != NULL) {
						tpDebug("buildCertGroup: DISCARDING expired anchor %p", expiredIssuer);
						expiredIssuer = NULL;
					}
				}
			}

			switch(crtn) {
				case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:
					/*
					 * A bit of a corner case. Found an issuer in AnchorCerts, but
					 * we can't do a signature verify since the issuer has a partial
					 * public key. Proceed but return
					 * CSSMERR_TP_CERTIFICATE_CANT_OPERATE.
					 */
					if(anchorInfo->addStatusCode(CSSMERR_TP_CERTIFICATE_CANT_OPERATE)) {
						foundPartialIssuer = true;
						crtn = CSSMERR_TP_CERTIFICATE_CANT_OPERATE;
					}
					else {
						/* ignore */
						crtn = CSSM_OK;
					}
					 /* drop thru */
				case CSSM_OK:
					/*  A fully successful return. */
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
					assert(!anchorInfo->isFromInputCerts());
					anchorInfo->index(certDex);
					certsToBeFreed.appendCert(anchorInfo);
					tpDebug("buildCertGroup: Cert FOUND by signer in AnchorList");
					tpAnchorDebug("buildCertGroup: Cert FOUND by signer in AnchorList");
					/* one more thing: partial public key processing needed? */
					if(foundPartialIssuer) {
						return verifyWithPartialKeys(subjectItem);
					}
					else {
						return crtn;
					}

				default:
					/* continue to next anchor */
					if(crtn != CSSM_CERT_STATUS_EXPIRED) {
						/* Expired means we're saving it in expiredIssuer */
						tpVfyDebug("buildCertGroup found issuer in anchor, BAD SIG");
						delete anchorInfo;
					}
					anchorInfo = NULL;
					break;
			}
		}	/* for each anchor */
	}	/* thisSubject not a root cert */

	/*
	 * Case 2: Check whether endCert is present in anchor certs.
	 *
	 * Also used to validate an expiredRoot that we pulled off the chain in
	 * hopes of finding something better (which, if we're here, we haven't done).
	 *
	 * Note that the main loop above did the actual root self-verify test.
	 */
	if(endCert || expiredRoot) {

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
				tpAnchorDebug("buildCertGroup: end cert in input AND anchors");
				verifiedToAnchor = CSSM_TRUE;
				theRoot->isAnchor(true);
				if(!theRoot->isFromInputCerts()) {
					/* Don't override index into input certs */
					theRoot->index(certDex);
				}
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
		tpAnchorDebug("buildCertGroup: end cert in input, NOT anchors");

		if(!expiredRoot && endCert->isSelfSigned()) {
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

	/* regardless of anchor search status... */
	crtn = CSSM_OK;
	if(!verifiedToAnchor && (expiredIssuer != NULL)) {
		/* expiredIssuer here is always an anchor */
		tpDebug("buildCertGroup: accepting expired anchor %p", expiredIssuer);
		appendCert(expiredIssuer);
		verifiedToAnchor = CSSM_TRUE;
		if(expiredIssuer->isSelfSigned()) {
			verifiedToRoot = CSSM_TRUE;
		}
		/* no matter what, we don't want this one */
		expiredRoot = NULL;
	}
post_anchor:
	if(expiredRoot) {
		/*
		 * One remaining special case: expiredRoot found in input certs, but
		 * no luck resolving the problem with the anchors. Go ahead and (re-)append
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

	/* If we get here, determine if fetching the issuer from the network
	 * should be attempted: <rdar://6113890&7419584&7422356>
	 */
	attemptNetworkFetch = (actionFlags & CSSM_TP_ACTION_FETCH_CERT_FROM_NET);
	if( (!dbList || (dbList->NumHandles == 0)) &&
		 (!anchorCerts || (numAnchorCerts == 0)) ) {
		/* DB list is empty *and* anchors are empty; there is no point in going
		 * out to the network, since we cannot build a chain to a trusted root.
		 * (This can occur when the caller wants to evaluate a single certificate
		 * without trying to build the chain, e.g. to check its key usage.)
		 */
		attemptNetworkFetch = false;
	}

	/*
	 * If we haven't verified to a root, and net fetch of certs is enabled,
	 * try to get the issuer of the last cert in the chain from the net.
	 * If that succeeds, then call ourself recursively to perform the
	 * whole search again (including comparing to or verifying against
	 * anchor certs).
	 */
	if(!verifiedToRoot && !verifiedToAnchor &&
		(endCert != NULL) && attemptNetworkFetch) {
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
				if (!issuer)
					break;
				tpDebug("buildCertGroup: Cert FOUND from Net; recursing");

				if(isInGroup(*issuer)) {
					tpDebug("buildCertGroup: Multiple instances of cert from net");
					delete issuer;
					issuer = NULL;
					crtn = CSSMERR_TP_CERTGROUP_INCOMPLETE;
					break;
				}

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
					CSSM_TRUE,		// subjectIsInGroup
					actionFlags,
					policyOid,
					policyStr,
					policyStrLen,
					leafKeyUse,		// actually don't care since the leaf will not
									// be evaluated
					verifiedToRoot,
					verifiedToAnchor,
					verifiedViaTrustSettings);
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
final_out:
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
			if(thisCert->addStatusCode(CSSMERR_TP_CERTIFICATE_CANT_OPERATE)) {
				return CSSMERR_TP_CERTIFICATE_CANT_OPERATE;
			}
			else {
				break;
			}
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
			if(thisCert->addStatusCode(CSSMERR_TP_CERTIFICATE_CANT_OPERATE)) {
				return CSSMERR_TP_INVALID_CERT_AUTHORITY;
			}
			else {
				break;
			}
		}
	}

	/* we just verified subjectItem - right?  */
	assert((void *)mCertInfo[0] != (void *)&subjectItem);
	tpDebug("verifyWithPartialKeys: success at subjectItem");
	return CSSM_OK;
}

/*
 * Free records obtained from DBs. Called when these records are not going to
 * be passed to caller of CertGroupConstruct or CertGroupVerify.
 */
void TPCertGroup::freeDbRecords()
{
	for(unsigned dex=0; dex<mNumCerts; dex++) {
		TPCertInfo *certInfo = mCertInfo[dex];
		certInfo->freeUniqueRecord();
	}
}
