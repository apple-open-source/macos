/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
 * TPCrlInfo.h - TP's private CRL and CRL group
 *
 * Written 9/30/2002 by Doug Mitchell.
 */

#include "TPCrlInfo.h"
#include "tpdebugging.h"
#include "certGroupUtils.h"
#include "tpCrlVerify.h"
#include "tpPolicies.h"
#include "tpTime.h"
#include <Security/cssmapi.h>
#include <Security/x509defs.h>
#include <Security/oidscert.h>
#include <Security/oidscrl.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <string.h>						/* for memcmp */
#include <Security/cssmapple.h>

/*
 * Replacement for CSSM_CL_CrlGetFirstCachedFieldValue for use with 
 * TPCrlItemInfo's generic getFirstCachedField mechanism. 
 */
static CSSM_RETURN tpGetFirstCachedFieldValue (CSSM_CL_HANDLE CLHandle,
                                     CSSM_HANDLE CrlHandle,
                                     const CSSM_OID *CrlField,
                                     CSSM_HANDLE_PTR ResultsHandle,
                                     uint32 *NumberOfMatchedFields,
                                     CSSM_DATA_PTR *Value)
{
	return CSSM_CL_CrlGetFirstCachedFieldValue(CLHandle,
		CrlHandle,
        NULL,		// const CSSM_DATA *CrlRecordIndex,
		CrlField,
		ResultsHandle,
		NumberOfMatchedFields,
		Value);
}

static const TPClItemCalls tpCrlClCalls =
{
	tpGetFirstCachedFieldValue,
	CSSM_CL_CrlAbortQuery,
	CSSM_CL_CrlCache,
	CSSM_CL_CrlAbortCache,
	CSSM_CL_CrlVerify,
	&CSSMOID_X509V1CRLThisUpdate,
	&CSSMOID_X509V1CRLNextUpdate,
	CSSMERR_TP_INVALID_CRL_POINTER,
	CSSMERR_APPLETP_CRL_EXPIRED,
	CSSMERR_APPLETP_CRL_NOT_VALID_YET
};


/* 
 * No default constructor - this is the only way.
 * This caches the cert and fetches subjectName and issuerName
 * to ensure the incoming certData is well-constructed.
 */
TPCrlInfo::TPCrlInfo(
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	const CSSM_DATA		*crlData,
	TPItemCopy			copyCrlData,		// true: we copy, we free
											// false - caller owns
	const char 			*verifyTime)		// = NULL
	
	: TPClItemInfo(clHand, cspHand, tpCrlClCalls, crlData, 
			copyCrlData, verifyTime),
		mRefCount(0),
		mFromWhere(CFW_Nowhere),
		mX509Crl(NULL),
		mCrlFieldToFree(NULL),
		mVerifyState(CVS_Unknown),
		mVerifyError(CSSMERR_TP_INTERNAL_ERROR)
{
	CSSM_RETURN	crtn;

	mUri.Data = NULL;
	mUri.Length = 0;
	
	/* fetch parsed CRL */
	crtn = fetchField(&CSSMOID_X509V2CRLSignedCrlCStruct, &mCrlFieldToFree);
	if(crtn) {
		/* bad CRL */
		releaseResources();
		CssmError::throwMe(crtn);
	}
	if(mCrlFieldToFree->Length != sizeof(CSSM_X509_SIGNED_CRL)) {
		tpErrorLog("fetchField(SignedCrlCStruct) length error\n");
		releaseResources();
		CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
	}
	mX509Crl = (CSSM_X509_SIGNED_CRL *)mCrlFieldToFree->Data;
	/* any other other commonly used fields? */
}
	
TPCrlInfo::~TPCrlInfo()
{
	releaseResources();
}

void TPCrlInfo::releaseResources()
{
	if(mCrlFieldToFree) {
		freeField(&CSSMOID_X509V2CRLSignedCrlCStruct, mCrlFieldToFree);
		mCrlFieldToFree = NULL;
	}
	if(mUri.Data) {
		Allocator::standard().free(mUri.Data);
		mUri.Data = NULL;
		mUri.Length = 0;
	}
	TPClItemInfo::releaseResources();
}

void TPCrlInfo::uri(const CSSM_DATA &uri)
{
	tpCopyCssmData(Allocator::standard(), &uri, &mUri);
}

/*
 * List of extensions we understand and can accept as critical.
 */
static const CSSM_OID *const TPGoodCrlExtens[] = 
{
	&CSSMOID_CrlNumber,
	/* Note NOT CSSMOID_DeltaCrlIndicator! That's fatal */
	&CSSMOID_CrlReason,
	&CSSMOID_CertIssuer,
	&CSSMOID_IssuingDistributionPoint,
	&CSSMOID_HoldInstructionCode,
	&CSSMOID_InvalidityDate,
	&CSSMOID_AuthorityKeyIdentifier,
	&CSSMOID_SubjectAltName,
	&CSSMOID_IssuerAltName
};

#define NUM_KNOWN_EXTENS (sizeof(TPGoodCrlExtens) / sizeof(CSSM_OID_PTR))

/*
 * Do our best to understand all the entries in a CSSM_X509_EXTENSIONS,
 * which may be per-CRL or per-entry.
 *
 * For now, we just ensure that for every critical extension,
 * we actually understand it and can deal it.
 */
CSSM_RETURN TPCrlInfo::parseExtensions(
	TPVerifyContext				&vfyCtx,
	bool						isPerEntry,
	uint32						entryIndex,		// if isPerEntry
	const CSSM_X509_EXTENSIONS	&extens,
	TPCertInfo					*forCert,		// optional
	bool						&isIndirectCrl)	// RETURNED
{
	isIndirectCrl = false;
	for(uint32 dex=0; dex<extens.numberOfExtensions; dex++) {
		CSSM_X509_EXTENSION_PTR exten = &extens.extensions[dex];
		if(exten->critical) {
			/* critical: is it in our list of understood extensions? */
			unsigned i;
			for(i=0; i<NUM_KNOWN_EXTENS; i++) {
				if(tpCompareOids(&exten->extnId, TPGoodCrlExtens[i])) {
					/* we're cool with this one */
					break;
				}
			}
			if(i == NUM_KNOWN_EXTENS) {
				tpCrlDebug("parseExtensions: Unknown Critical Extension\n");
				return CSSMERR_APPLETP_UNKNOWN_CRL_EXTEN;
			}
		}
		
		/* Specific extension handling. */
		if(tpCompareOids(&exten->extnId, 
				&CSSMOID_IssuingDistributionPoint)) {
			/*
			 * If this assertion fails, we're out of sync with the CL
			 */
			assert(exten->format == CSSM_X509_DATAFORMAT_PARSED);
			CE_IssuingDistributionPoint *idp = 
				(CE_IssuingDistributionPoint *)
					exten->value.parsedValue;
					
			/*
			 * Snag indirectCrl flag for caller in any case
			 */
			if(idp->indirectCrlPresent && idp->indirectCrl) {
				isIndirectCrl = true;
			}
			if(forCert != NULL) {
				/* If no target cert, i.e., we're just verifying a CRL,
				 * skip the remaining IDP checks. */
				
				/* verify onlyCACerts/onlyUserCerts */
				bool isUserCert;
				if(forCert->isLeaf() &&
					!(vfyCtx.actionFlags & CSSM_TP_ACTION_LEAF_IS_CA)) {
						isUserCert = true;
				}
				else {
					isUserCert = false;
				}
				if((idp->onlyUserCertsPresent) && (idp->onlyUserCerts)) {
					if(!isUserCert) {
						tpCrlDebug("parseExtensions: onlyUserCerts, "
							"!leaf\n");
						return CSSMERR_APPLETP_IDP_FAIL;
					}
				}
				if((idp->onlyCACertsPresent) && (idp->onlyCACerts)) {
					if(isUserCert) {
						tpCrlDebug("parseExtensions: onlyCACerts, leaf\n");
						return CSSMERR_APPLETP_IDP_FAIL;
					}
				}
			}	/* IDP */
		} 		/* have target cert */
	}
	
	return CSSM_OK;
}

/* 
 * The heavyweight "perform full verification of this CRL" op.
 * Must verify to an anchor cert in tpVerifyContext or via
 * Trust Settings if so enabled. 
 * Intermediate certs can come from signerCerts or dBList. 
 */
CSSM_RETURN TPCrlInfo::verifyWithContext(
	TPVerifyContext			&tpVerifyContext,
	TPCertInfo				*forCert,		// optional 
	bool					doCrlVerify)	
{
	/*
	 * Step 1: this CRL must be current. Caller might have re-evaluated
	 * expired/notValidYet since our construction via calculateCurrent().
	 */
	if(isExpired()) {
		return CSSMERR_APPLETP_CRL_EXPIRED;
	}
	if(isNotValidYet()) {
		return CSSMERR_APPLETP_CRL_NOT_VALID_YET;
	}

	/* subsequent verify state is cached */
	switch(mVerifyState) {
		case CVS_Good:
			return CSSM_OK;
		case CVS_Bad:
			return mVerifyError;
		case CVS_Unknown:
			break;
		default:
			tpErrorLog("verifyWithContext: bad verifyState\n");
			return CSSMERR_TP_INTERNAL_ERROR;
	}
	
	/*
	 * Step 2: parse & understand all critical CRL extensions. 
	 */
	CSSM_RETURN crtn;
	bool isIndirectCrl;
	crtn = parseExtensions(tpVerifyContext,
		false,
		0,
		mX509Crl->tbsCertList.extensions,
		forCert,
		isIndirectCrl);
	if(crtn) {
		mVerifyState = CVS_Bad;
		if(!forCert || forCert->addStatusCode(crtn)) {
			return crtn;
		}
		/* else continue */
	}
	CSSM_X509_REVOKED_CERT_LIST_PTR revoked = 
			mX509Crl->tbsCertList.revokedCertificates;
	if(revoked != NULL) {
		for(uint32 dex=0; dex<revoked->numberOfRevokedCertEntries; dex++) {
			bool dummyIsIndirect;	// can't be set here 
			crtn = parseExtensions(tpVerifyContext,
				true,
				dex,
				revoked->revokedCertEntry[dex].extensions,
				forCert,
				dummyIsIndirect);
			if(crtn) {
				if(!forCert || forCert->addStatusCode(crtn)) {
					mVerifyState = CVS_Bad;
					return crtn;
				}
			}
		}
	}
	
	/*
	 * Step 3: obtain a fully verified cert chain which verifies this CRL.
	 */
	CSSM_BOOL	verifiedToRoot;
	CSSM_BOOL	verifiedToAnchor;
	CSSM_BOOL	verifiedViaTrustSetting;
	
	TPCertGroup outCertGroup(tpVerifyContext.alloc, 
		TGO_Caller);			// CRLs owned by inCertGroup

	/* set up for disposal of TPCertInfos created by 
	 * CertGroupConstructPriv */
	TPCertGroup	certsToBeFreed(tpVerifyContext.alloc, TGO_Group);
	
	if(tpVerifyContext.signerCerts) {
		/* start from scratch with this group */
		tpVerifyContext.signerCerts->setAllUnused();
	}
	crtn = outCertGroup.buildCertGroup(
			*this,							// subject item
			tpVerifyContext.signerCerts,	// inCertGroup, optional
			tpVerifyContext.dbList,			// optional
			tpVerifyContext.clHand,
			tpVerifyContext.cspHand,
			tpVerifyContext.verifyTime,
			tpVerifyContext.numAnchorCerts,
			tpVerifyContext.anchorCerts,
			certsToBeFreed,
			&tpVerifyContext.gatheredCerts,
			CSSM_FALSE,						// subjectIsInGroup
			tpVerifyContext.actionFlags,
			tpVerifyContext.policyOid,
			tpVerifyContext.policyStr,
			tpVerifyContext.policyStrLen,
			kSecTrustSettingsKeyUseSignRevocation,
			verifiedToRoot,	
			verifiedToAnchor,
			verifiedViaTrustSetting);
	/* subsequent errors to errOut: */

	if(crtn) {
		tpCrlDebug("TPCrlInfo::verifyWithContext buildCertGroup failure "
			"index %u",	index());
		if(!forCert || forCert->addStatusCode(crtn)) {
			goto errOut;
		}
	}
	if (verifiedToRoot && (tpVerifyContext.actionFlags & CSSM_TP_ACTION_IMPLICIT_ANCHORS))
		verifiedToAnchor = CSSM_TRUE;
	if(!verifiedToAnchor && !verifiedViaTrustSetting) {
		/* required */
		if(verifiedToRoot) {
			/* verified to root which is not an anchor */
			tpCrlDebug("TPCrlInfo::verifyWithContext root, no anchor, "
				"index %u",	index());
			crtn = CSSMERR_APPLETP_CRL_INVALID_ANCHOR_CERT;
		}
		else {
			/* partial chain, no root, not verifiable by anchor */
			tpCrlDebug("TPCrlInfo::verifyWithContext no root, no anchor, "
				"index %u",	index());
			crtn = CSSMERR_APPLETP_CRL_NOT_TRUSTED;
		}
		if(!forCert || forCert->addStatusCode(crtn)) {
			mVerifyState = CVS_Bad;
			goto errOut;
		}
	}
	
	/* 
	 * Step 4: policy verification on the returned cert group 
	 * We need to (temporarily) assert the "leaf cert is a CA" flag
	 * here. 
	 */
	outCertGroup.certAtIndex(0)->isLeaf(true);
	crtn = tp_policyVerify(kCrlPolicy,
		tpVerifyContext.alloc,
		tpVerifyContext.clHand,
		tpVerifyContext.cspHand,
		&outCertGroup,
		verifiedToRoot,
		verifiedViaTrustSetting,
		tpVerifyContext.actionFlags | CSSM_TP_ACTION_LEAF_IS_CA,
		NULL,							// sslOpts
		NULL);							// policyOpts, not currently used 
	if(crtn) {
		tpCrlDebug("   ...verifyWithContext policy FAILURE CRL %u",
			index());
		if(!forCert || forCert->addStatusCode(CSSMERR_APPLETP_CRL_POLICY_FAIL)) {
			mVerifyState = CVS_Bad;
			goto errOut;
		}
	}
	
	/*
	 * Step 5: recursively perform CRL verification on the certs 
	 * gathered to verify this CRL. 
	 * Only performed if this CRL is an indirect CRL or the caller
	 * explicitly told us to do this (i.e., caller is verifying a
	 * CRL, not a cert chain).
	 */
	if(isIndirectCrl || doCrlVerify) {
		tpCrlDebug("verifyWithContext recursing to "
			"tpVerifyCertGroupWithCrls");
		crtn = tpVerifyCertGroupWithCrls(tpVerifyContext,
			outCertGroup);
		if(crtn) {
			tpCrlDebug("   ...verifyWithContext CRL reverify FAILURE CRL %u",
				index());
			if(!forCert || forCert->addStatusCode(crtn)) {
				mVerifyState = CVS_Bad;
				goto errOut;
			}
		}
	}

	tpCrlDebug("   ...verifyWithContext CRL %u SUCCESS", index());
	mVerifyState = CVS_Good;
errOut:
	/* we own these, we free the DB records */
	certsToBeFreed.freeDbRecords();
	return crtn;
}

/*
 * Wrapper for verifyWithContext for use when evaluating a CRL
 * "now" instead of at the time in TPVerifyContext.verifyTime.
 * In this case, on entry, TPVerifyContext.verifyTime is the 
 * time at which a cert is being evaluated.
 */
CSSM_RETURN TPCrlInfo::verifyWithContextNow(
	TPVerifyContext		&tpVerifyContext,
	TPCertInfo			*forCert,			// optional
	bool				doCrlVerify)
{
	CSSM_TIMESTRING ctxTime = tpVerifyContext.verifyTime;
	CSSM_RETURN crtn = verifyWithContext(tpVerifyContext, forCert, doCrlVerify);
	tpVerifyContext.verifyTime = ctxTime;
	return crtn;
}

/*
 * Do I have the same issuer as the specified subject cert? Returns 
 * true if so.
 */
bool TPCrlInfo::hasSameIssuer(
	const TPCertInfo	&subject)
{
	assert(subject.issuerName() != NULL);
	if(tpCompareCssmData(issuerName(), subject.issuerName())) {
		return true;
	}
	else {
		return false;
	}
}

/*
 * Determine if specified cert has been revoked as of the
 * provided time; a NULL timestring indicates "now". 
 *
 * Assumes current CRL is verified good and that issuer names of 
 * the cert and CRL match.
 *
 * This duplicates similar logic in the CL, but to avoid re-parsing
 * the subject cert (which we have parsed and cached), we just do it 
 * here.
 *
 * Possible errors are 
 *	CSSMERR_TP_CERT_REVOKED
 * 	CSSMERR_TP_CERT_SUSPENDED
 *  TBD
 *
 * Error status is added to subjectCert. 
 */
CSSM_RETURN TPCrlInfo::isCertRevoked(
	TPCertInfo &subjectCert,
	CSSM_TIMESTRING verifyTime)
{
	assert(mVerifyState == CVS_Good);
	CSSM_X509_TBS_CERTLIST_PTR tbs = &mX509Crl->tbsCertList;
	
	/* trivial case - empty CRL */
	if((tbs->revokedCertificates == NULL) ||
	   (tbs->revokedCertificates->numberOfRevokedCertEntries == 0)) {
	   tpCrlDebug("   isCertRevoked: empty CRL at index %u", index());
	   return CSSM_OK;
	}
	
	/* is subject cert's serial number in this CRL? */
	CSSM_DATA_PTR subjSerial = NULL;
	CSSM_RETURN crtn;
	crtn = subjectCert.fetchField(&CSSMOID_X509V1SerialNumber, &subjSerial);
	if(crtn) {
		/* should never happen */
		tpErrorLog("TPCrlInfo:isCertRevoked: error fetching serial number\n");
		if(subjectCert.addStatusCode(crtn)) {
			return crtn;
		}
		else {
			/* allowed error - can't proceed; punt with success */
			return CSSM_OK;
		}
	}
	/* subsequent errors to errOut: */
	
	uint32 numEntries = tbs->revokedCertificates->numberOfRevokedCertEntries;
	CSSM_X509_REVOKED_CERT_ENTRY_PTR entries = 
		tbs->revokedCertificates->revokedCertEntry;
	crtn = CSSM_OK;
	CFDateRef cfRevokedTime = NULL;
	CFDateRef cfVerifyTime = NULL;

	for(uint32 dex=0; dex<numEntries; dex++) {
		CSSM_X509_REVOKED_CERT_ENTRY_PTR entry = &entries[dex];
		if(tpCompareCssmData(subjSerial, &entry->certificateSerialNumber)) {
			/* 
			 * It's in there. Compare revocation time in the CRL to 
			 * our caller-specified verifyTime.
			 */
			CSSM_X509_TIME_PTR xTime = &entry->revocationDate;
			int rtn;
			rtn = timeStringToCfDate((char *)xTime->time.Data, xTime->time.Length, 
				&cfRevokedTime);
			if(rtn) {
				tpErrorLog("fetchNotBeforeAfter: malformed revocationDate\n");
			}
			else {
				if(verifyTime != NULL) {
					rtn = timeStringToCfDate((char *)verifyTime, strlen(verifyTime), 
											 &cfVerifyTime);
				}
				else {
					/* verify right now */
					cfVerifyTime = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
				}
				if((rtn == 0) && cfVerifyTime != NULL) {
					CFComparisonResult res = CFDateCompare(cfVerifyTime, cfRevokedTime, NULL);
					if(res == kCFCompareLessThan) {
						/* cfVerifyTime < cfRevokedTime; I guess this one's OK */
						tpCrlDebug("   isCertRevoked: cert %u NOT YET REVOKED by CRL %u", 
								   subjectCert.index(), index());
						break;
					}
				}
			}

			/*
			 * REQUIRED TBD: parse the entry's extensions, specifically to 
			 * get a reason. This will entail a bunch of new TP/cert specific
			 * CSSM_RETURNS.
			 * For now, just flag it revoked. 
			 */
			crtn = CSSMERR_TP_CERT_REVOKED;
			tpCrlDebug("   isCertRevoked: cert %u REVOKED by CRL %u", 
				subjectCert.index(), index());
			break;
		}
	}
	
	subjectCert.freeField(&CSSMOID_X509V1SerialNumber, subjSerial);
	if(crtn && !subjectCert.addStatusCode(crtn)) {
		return CSSM_OK;
	}
	if(cfRevokedTime) {
		CFRelease(cfRevokedTime);
	}
	if(cfVerifyTime) {
		CFRelease(cfVerifyTime);
	}
	return crtn;
}

/***
 *** TPCrlGroup class
 ***/
 
/* build empty group */
TPCrlGroup::TPCrlGroup(
	Allocator			&alloc,
	TPGroupOwner		whoOwns) :
		mAlloc(alloc),
		mCrlInfo(NULL),
		mNumCrls(0),
		mSizeofCrlInfo(0),
		mWhoOwns(whoOwns)
{
	/* nothing for now */
}
	
/*
 * Construct from unordered, untrusted CSSM_CRLGROUP. Resulting
 * TPCrlInfos are more or less in the same order as the incoming
 * CRLs, though incoming CRLs are discarded if they don't parse.
 * No verification of any sort is performed. 
 */
TPCrlGroup::TPCrlGroup(
	const CSSM_CRLGROUP 	*cssmCrlGroup,			// optional
	CSSM_CL_HANDLE 			clHand,
	CSSM_CSP_HANDLE 		cspHand,
	Allocator				&alloc,
	const char				*verifyTime,			// may be NULL
	TPGroupOwner			whoOwns) :
		mAlloc(alloc),
		mCrlInfo(NULL),
		mNumCrls(0),
		mSizeofCrlInfo(0),
		mWhoOwns(whoOwns)
{
	/* verify input args */
	if((cssmCrlGroup == NULL) || (cssmCrlGroup->NumberOfCrls == 0)) {
		return;
	}
	if(cspHand == CSSM_INVALID_HANDLE) {
		CssmError::throwMe(CSSMERR_TP_INVALID_CSP_HANDLE);
	}
	if(clHand == CSSM_INVALID_HANDLE)	{
		CssmError::throwMe(CSSMERR_TP_INVALID_CL_HANDLE);
	}
	if(cssmCrlGroup->CrlGroupType != CSSM_CRLGROUP_DATA) {
		CssmError::throwMe(CSSMERR_TP_INVALID_CERTGROUP);
	}
	switch(cssmCrlGroup->CrlType) {
		case CSSM_CRL_TYPE_X_509v1:
		case CSSM_CRL_TYPE_X_509v2:
			break;
		default:
			CssmError::throwMe(CSSMERR_TP_UNKNOWN_FORMAT);
	}
	switch(cssmCrlGroup->CrlEncoding) {
		case CSSM_CRL_ENCODING_BER:
		case CSSM_CRL_ENCODING_DER:
			break;
		default:
			CssmError::throwMe(CSSMERR_TP_UNKNOWN_FORMAT);
	}
	
	/* 
	 * Add remaining input certs to mCrlInfo.
	 */
	TPCrlInfo *crlInfo = NULL;
	for(unsigned crlDex=0; crlDex<cssmCrlGroup->NumberOfCrls; crlDex++) {
		try {
			crlInfo = new TPCrlInfo(clHand,
				cspHand,
				&cssmCrlGroup->GroupCrlList.CrlList[crlDex],
				TIC_NoCopy,			// don't copy data 
				verifyTime);
		}
		catch (...) {
			/* just ignore this CRL */
			continue;
		}
		crlInfo->index(crlDex);
		appendCrl(*crlInfo);
	}
}

/*
 * Deletes all TPCrlInfo's if appropriate.
 */
TPCrlGroup::~TPCrlGroup()
{
	if(mWhoOwns == TGO_Group) {
		unsigned i;
		for(i=0; i<mNumCrls; i++) {
			delete mCrlInfo[i];
		}
	}
	mAlloc.free(mCrlInfo);
}

/* add/remove/access TPTCrlInfo's. */
/*
 * NOTE: I am aware that most folks would just use an array<> here, but
 * gdb is so lame that it doesn't even let one examine the contents
 * of an array<> (or just about anything else in the STL). I prefer
 * debuggability over saving a few lines of trivial code.
 */
void TPCrlGroup::appendCrl(
	TPCrlInfo			&crlInfo)
{
	if(mNumCrls == mSizeofCrlInfo) {
		if(mSizeofCrlInfo == 0) {
			/* appending to empty array */
			mSizeofCrlInfo = 1;
		}
		else {
			mSizeofCrlInfo *= 2;
		}
		mCrlInfo = (TPCrlInfo **)mAlloc.realloc(mCrlInfo, 
			mSizeofCrlInfo * sizeof(TPCrlInfo *));
	}
	mCrlInfo[mNumCrls++] = &crlInfo;
}

TPCrlInfo *TPCrlGroup::crlAtIndex(
	unsigned			index)
{
	if(index > (mNumCrls - 1)) {
		CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
	}
	return mCrlInfo[index];
}

TPCrlInfo &TPCrlGroup::removeCrlAtIndex(
	unsigned			index)				// doesn't delete the cert, just 
											// removes it from our list
{
	if(index > (mNumCrls - 1)) {
		CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
	}
	TPCrlInfo &rtn = *mCrlInfo[index];
	
	/* removed requested element and compact remaining array */
	unsigned i;
	for(i=index; i<(mNumCrls - 1); i++) {
		mCrlInfo[i] = mCrlInfo[i+1];
	}
	mNumCrls--;
	return rtn;
}

void TPCrlGroup::removeCrl(
	TPCrlInfo			&crlInfo)
{
	for(unsigned dex=0; dex<mNumCrls; dex++) {
		if(mCrlInfo[dex] == &crlInfo) {
			removeCrlAtIndex(dex);
			return;
		}
	}
	tpErrorLog("TPCrlGroup::removeCrl: CRL NOT FOUND\n");
	assert(0);
}

TPCrlInfo *TPCrlGroup::firstCrl()
{
	if(mNumCrls == 0) {
		/* the caller really should not do this... */
		CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
	}
	else {
		return mCrlInfo[0];
	}
}

TPCrlInfo *TPCrlGroup::lastCrl()
{
	if(mNumCrls == 0) {
		/* the caller really should not do this... */
		CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
	}
	else {
		return mCrlInfo[mNumCrls - 1];
	}
}

	/* 
	 * Find a CRL whose issuer matches specified subject cert.
	 * Returned CRL has not necessarily been verified.
	 */
TPCrlInfo *TPCrlGroup::findCrlForCert(
		TPCertInfo			&subject)
{
	for(unsigned dex=0; dex<mNumCrls; dex++) {
		TPCrlInfo *crl = mCrlInfo[dex];
		if(crl->hasSameIssuer(subject)) {
			return crl;
		}
	}
	return NULL;
}
