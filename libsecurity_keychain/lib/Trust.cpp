/*
 * Copyright (c) 2002-2009 Apple Inc. All Rights Reserved.
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

//
// Trust.cpp
//
#include <security_keychain/Trust.h>
#include <security_keychain/TrustSettingsSchema.h>
#include <security_cdsa_utilities/cssmdates.h>
#include <security_utilities/cfutilities.h>
#include <CoreFoundation/CFData.h>
#include <Security/SecCertificate.h>
#include "SecBridge.h"
#include "TrustAdditions.h"

using namespace KeychainCore;


//
// For now, we use a global TrustStore
//
ModuleNexus<TrustStore> Trust::gStore;
        
/*
 * Singleton maintaining open references to standard system keychains,
 * to avoid having them be opened anew every time SecTrust is used. 
 */
class TrustKeychains 
{
public:
	TrustKeychains();
	~TrustKeychains()	{}
	CSSM_DL_DB_HANDLE	rootStoreHandle()	{ return mRootStore->database()->handle(); }
	CSSM_DL_DB_HANDLE	systemKcHandle()	{ return mSystem->database()->handle(); }
	Keychain			rootStore()			{ return mRootStore; }
	Keychain			systemKc()			{ return mSystem; }
private:
	Keychain	mRootStore;
	Keychain	mSystem;
};

TrustKeychains::TrustKeychains()
	: mRootStore(globals().storageManager.make(SYSTEM_ROOT_STORE_PATH, false)),
	  mSystem(globals().storageManager.make(ADMIN_CERT_STORE_PATH, false))
{
}

static ModuleNexus<TrustKeychains> trustKeychains;

//      
// Translate CFDataRef to CssmData. The output shares the input's buffer.//
//
static inline CssmData cfData(CFDataRef data)
{
    return CssmData(const_cast<UInt8 *>(CFDataGetBytePtr(data)),
        CFDataGetLength(data));
}

//
// Convert a SecPointer to a CF object.
//
static SecCertificateRef
convert(const SecPointer<Certificate> &certificate)
{
	return *certificate;
}

//
// Construct a Trust object with suitable defaults.
// Use setters for additional arguments before calling evaluate().
//
Trust::Trust(CFTypeRef certificates, CFTypeRef policies)
    : mTP(gGuidAppleX509TP), mAction(CSSM_TP_ACTION_DEFAULT),
      mCerts(cfArrayize(certificates)), mPolicies(cfArrayize(policies)),
      mResult(kSecTrustResultInvalid), mUsingTrustSettings(false),
	  mMutex(Mutex::recursive)
{
	// set default search list from user's default
	globals().storageManager.getSearchList(mSearchLibs);
}


//
// Clean up a Trust object
//
Trust::~Trust() 
{
	clearResults();
}


//
// Retrieve the last TP evaluation result, if any
//
CSSM_TP_VERIFY_CONTEXT_RESULT_PTR Trust::cssmResult()
{
	if (mResult == kSecTrustResultInvalid)
		MacOSError::throwMe(errSecTrustNotAvailable);
    return &mTpResult;
}


// SecCertificateRef -> CssmData
CssmData cfCertificateData(SecCertificateRef certificate)
{
    return Certificate::required(certificate)->data();
}

// SecPolicyRef -> CssmField (CFDataRef/NULL or oid/value of a SecPolicy)
CssmField cfField(SecPolicyRef item)
{
	SecPointer<Policy> policy = Policy::required(SecPolicyRef(item));
    return CssmField(policy->oid(), policy->value());
}

// SecKeychain -> CssmDlDbHandle
CSSM_DL_DB_HANDLE cfKeychain(SecKeychainRef ref)
{
	Keychain keychain = KeychainImpl::required(ref);
	return keychain->database()->handle();
}


//
// Here's the big "E" - evaluation.
// We build most of the CSSM-layer input structures dynamically right here;
// they will auto-destruct when we're done. The output structures are kept
// around (in our data members) for later analysis.
// Note that evaluate() can be called repeatedly, so we must be careful to
// dispose of prior results.
//
void Trust::evaluate()
{
	StLock<Mutex>_(mMutex);
	// if we have evaluated before, release prior result
	clearResults();
	
	// determine whether the leaf certificate is an EV candidate
	CFArrayRef allowedAnchors = allowedEVRootsForLeafCertificate(mCerts);
	CFArrayRef filteredCerts = NULL;
	bool isEVCandidate = (allowedAnchors) ? true : false;
	if (isEVCandidate) {
		secdebug("evTrust", "Trust::evaluate() certificate is EV candidate");
		filteredCerts = potentialEVChainWithCertificates(mCerts);
	} else {
		if (mCerts) {
			filteredCerts = CFArrayCreateMutableCopy(NULL, 0, mCerts);
		}
		if (mAnchors) {
			allowedAnchors = CFArrayCreateMutableCopy(NULL, 0, mAnchors);
		}
	}
	// retain these certs as long as we potentially could have results involving them
	// (note that assignment to a CFRef type performs an implicit retain)
	mAllowedAnchors = allowedAnchors;
	mFilteredCerts = filteredCerts;

	if (allowedAnchors)
		CFRelease(allowedAnchors);
	if (filteredCerts)
		CFRelease(filteredCerts);
	
    // build the target cert group
    CFToVector<CssmData, SecCertificateRef, cfCertificateData> subjects(mFilteredCerts);
    CertGroup subjectCertGroup(CSSM_CERT_X_509v3,
            CSSM_CERT_ENCODING_BER, CSSM_CERTGROUP_DATA);
    subjectCertGroup.count() = subjects;
    subjectCertGroup.blobCerts() = subjects;
    
    // build a TP_VERIFY_CONTEXT, a veritable nightmare of a data structure
    TPBuildVerifyContext context(mAction);

	/* 
	 * Guarantee *some* action data... 
	 * NOTE this only works with the local X509 TP. When this module can deal with other TPs
	 * this must be revisited.
	 */
	CSSM_APPLE_TP_ACTION_DATA localActionData;
	memset(&localActionData, 0, sizeof(localActionData));
	CssmData localActionCData((uint8 *)&localActionData, sizeof(localActionData));
	CSSM_APPLE_TP_ACTION_DATA *actionDataP = &localActionData;
    if (mActionData) {
        context.actionData() = cfData(mActionData);
		actionDataP = (CSSM_APPLE_TP_ACTION_DATA *)context.actionData().data();
	}
	else {
		context.actionData() = localActionCData;
	}
	
    /*
	 * Policies (one at least, please).
	 * For revocation policies, see if any have been explicitly specified...
	 */
	CFMutableArrayRef allPolicies = NULL;
	uint32 numSpecAdded = 0;
	uint32 numPrefAdded = 0;
	if (isEVCandidate) {
		// force OCSP revocation checking for this evaluation
		secdebug("evTrust", "Trust::evaluate() forcing OCSP revocation checking");
		allPolicies = forceOCSPRevocationPolicy(numPrefAdded, context.allocator);
	}
	else if(!(revocationPolicySpecified(mPolicies))) {
		/* 
		 * None specified in mPolicies; see if any specified via SPI.
		 */
		allPolicies = addSpecifiedRevocationPolicies(numSpecAdded, context.allocator);
		if(allPolicies == NULL) {
			/* 
			 * None there; try preferences. 
			 */
			allPolicies = Trust::addPreferenceRevocationPolicies(numPrefAdded,
				context.allocator);
		}

	}
	if(allPolicies == NULL) {
		allPolicies = CFMutableArrayRef(CFArrayRef(mPolicies));
	}	
    CFToVector<CssmField, SecPolicyRef, cfField> policies(allPolicies);
    if (policies.empty())
        MacOSError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
    context.setPolicies(policies, policies);

    // anchor certificates - only use if caller provides them, or if cert requires EV,
	// else use UserTrust
    CFCopyRef<CFArrayRef> anchors(mAllowedAnchors);
	CFToVector<CssmData, SecCertificateRef, cfCertificateData> roots(anchors);
	if (!anchors) {
		mUsingTrustSettings = true;
		secdebug("userTrust", "Trust::evaluate() using UserTrust");
    }
	else {
		mUsingTrustSettings = false;
		secdebug("userTrust", "Trust::evaluate() !UserTrust; using %s anchors",
				(isEVCandidate) ? "EV" : "caller");
		context.anchors(roots, roots);
	}
    
	// dlDbList (keychain list)
	vector<CSSM_DL_DB_HANDLE> dlDbList;
	for (StorageManager::KeychainList::const_iterator it = mSearchLibs.begin();
			it != mSearchLibs.end(); it++)
	{
		try
		{
			dlDbList.push_back((*it)->database()->handle());
		}
		catch (...)
		{
		}
	}
	if(mUsingTrustSettings) {
		/* Append system anchors for use with Trust Settings */
		try {
			dlDbList.push_back(trustKeychains().rootStoreHandle());
			actionDataP->ActionFlags |= CSSM_TP_ACTION_TRUST_SETTINGS;
		} catch (...) {
			// no root store or system keychain; don't use trust settings but continue
			mUsingTrustSettings = false;
		}
		try {
			dlDbList.push_back(trustKeychains().systemKcHandle());		
		}
		catch(...) {
			/* Oh well, at least we got the root store DB */
		}
	}
	context.setDlDbList(dlDbList.size(), &dlDbList[0]);

    // verification time
    char timeString[15];
    if (mVerifyTime) {
        CssmUniformDate(static_cast<CFDateRef>(mVerifyTime)).convertTo(
			timeString, sizeof(timeString));
        context.time(timeString);
    }

	// to avoid keychain open/close thrashing, hold a copy of the search list
	StorageManager::KeychainList holdSearchList;
	globals().storageManager.getSearchList(holdSearchList);

    // Go TP!
    try {
        mTP->certGroupVerify(subjectCertGroup, context, &mTpResult);
        mTpReturn = noErr;
    } catch (CommonError &err) {
        mTpReturn = err.osStatus();
    }
    mResult = diagnoseOutcome();

    // see if we can use the evidence
    if (mTpResult.count() > 0
            && mTpResult[0].form() == CSSM_EVIDENCE_FORM_APPLE_HEADER
            && mTpResult[0].as<CSSM_TP_APPLE_EVIDENCE_HEADER>()->Version == CSSM_TP_APPLE_EVIDENCE_VERSION
            && mTpResult.count() == 3
            && mTpResult[1].form() == CSSM_EVIDENCE_FORM_APPLE_CERTGROUP
            && mTpResult[2].form() == CSSM_EVIDENCE_FORM_APPLE_CERT_INFO) {
        evaluateUserTrust(*mTpResult[1].as<CertGroup>(),
            mTpResult[2].as<CSSM_TP_APPLE_EVIDENCE_INFO>(), anchors);
    } else {
        // unexpected evidence information. Can't use it
        secdebug("trusteval", "unexpected evidence ignored");
    }
	
	/* do post-processing for EV candidate chain */
	if (isEVCandidate) {
		CFArrayRef fullChain = makeCFArray(convert, mCertChain);
		CFDictionaryRef evResult = extendedValidationResults(fullChain, mResult, mTpReturn);
		mExtendedResult = evResult; // assignment to CFRef type is an implicit retain
		if (evResult)
			CFRelease(evResult);
		CFRelease(fullChain);
		secdebug("evTrust", "Trust::evaluate() post-processing complete");
	}
	
	/* Clean up Policies we created implicitly */
	if(numSpecAdded) {
		freeSpecifiedRevocationPolicies(allPolicies, numSpecAdded, context.allocator);
	}
	if(numPrefAdded) {
		Trust::freePreferenceRevocationPolicies(allPolicies, numPrefAdded, context.allocator);
	}
}

// CSSM_RETURN values that map to kSecTrustResultRecoverableTrustFailure.
static const CSSM_RETURN recoverableErrors[] = 
{
	CSSMERR_TP_INVALID_ANCHOR_CERT,
	CSSMERR_TP_NOT_TRUSTED,
	CSSMERR_TP_VERIFICATION_FAILURE,
	CSSMERR_TP_VERIFY_ACTION_FAILED,
	CSSMERR_TP_INVALID_CERTIFICATE,
	CSSMERR_TP_INVALID_REQUEST_INPUTS,
	CSSMERR_TP_CERT_EXPIRED,
	CSSMERR_TP_CERT_NOT_VALID_YET,
	CSSMERR_TP_CERTIFICATE_CANT_OPERATE,
	CSSMERR_TP_INVALID_CERT_AUTHORITY,
	CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK,
	CSSMERR_APPLETP_HOSTNAME_MISMATCH,
	CSSMERR_TP_VERIFY_ACTION_FAILED,
	CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND,
	CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS,
	CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE,
	CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH,
	CSSMERR_APPLETP_CS_NO_BASIC_CONSTRAINTS,
	CSSMERR_APPLETP_CS_BAD_PATH_LENGTH,
	CSSMERR_APPLETP_CS_NO_EXTENDED_KEY_USAGE,
	CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE,
	CSSMERR_APPLETP_CODE_SIGN_DEVELOPMENT,
	CSSMERR_APPLETP_RS_BAD_CERT_CHAIN_LENGTH,
	CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN,
	CSSMERR_APPLETP_CRL_NOT_FOUND,
	CSSMERR_APPLETP_CRL_SERVER_DOWN,
	CSSMERR_APPLETP_CRL_NOT_VALID_YET,
	CSSMERR_APPLETP_OCSP_UNAVAILABLE,
	CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK,
	CSSMERR_APPLETP_NETWORK_FAILURE,
	CSSMERR_APPLETP_OCSP_RESP_TRY_LATER,
};
#define NUM_RECOVERABLE_ERRORS	(sizeof(recoverableErrors) / sizeof(CSSM_RETURN))

//
// Classify the TP outcome in terms of a SecTrustResultType
//
SecTrustResultType Trust::diagnoseOutcome()
{
	StLock<Mutex>_(mMutex);
    switch (mTpReturn) {
    case noErr:									// peachy
		if (mUsingTrustSettings)
		{
			uint32 chainLength = 0;
			if (mTpResult.count() == 3 &&
				mTpResult[1].form() == CSSM_EVIDENCE_FORM_APPLE_CERTGROUP &&
				mTpResult[2].form() == CSSM_EVIDENCE_FORM_APPLE_CERT_INFO)
			{
				const CertGroup &chain = *mTpResult[1].as<CertGroup>();
				chainLength = chain.count();
			}
            
			if (chainLength)
			{
				const CSSM_TP_APPLE_EVIDENCE_INFO *infoList = mTpResult[2].as<CSSM_TP_APPLE_EVIDENCE_INFO>();
				const TPEvidenceInfo &info = TPEvidenceInfo::overlay(infoList[chainLength-1]);
				const CSSM_TP_APPLE_CERT_STATUS resultCertStatus = info.status();
				bool hasUserDomainTrust = ((resultCertStatus & CSSM_CERT_STATUS_TRUST_SETTINGS_TRUST) &&
						(resultCertStatus & CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_USER));
				bool hasAdminDomainTrust = ((resultCertStatus & CSSM_CERT_STATUS_TRUST_SETTINGS_TRUST) &&
						(resultCertStatus & CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_ADMIN));
				if (hasUserDomainTrust || hasAdminDomainTrust)
				{
					return kSecTrustResultProceed;		// explicitly allowed
				}
			}
		}
		return kSecTrustResultUnspecified;		// cert evaluates OK
    case CSSMERR_TP_INVALID_CERTIFICATE:		// bad certificate
        return kSecTrustResultFatalTrustFailure;
	case CSSMERR_APPLETP_TRUST_SETTING_DENY:	// authoritative denial
		return kSecTrustResultDeny;
    default:
		break;
    }
	
	// a known list of returns maps to kSecTrustResultRecoverableTrustFailure
	const CSSM_RETURN *errp=recoverableErrors;
	for(unsigned dex=0; dex<NUM_RECOVERABLE_ERRORS; dex++, errp++) {
		if(*errp == mTpReturn) {
			return kSecTrustResultRecoverableTrustFailure;
		}
	}
	return kSecTrustResultOtherError;			// unknown
}


//
// Assuming a good evidence chain, check user trust
// settings and set mResult accordingly.
//
void Trust::evaluateUserTrust(const CertGroup &chain,
    const CSSM_TP_APPLE_EVIDENCE_INFO *infoList, CFCopyRef<CFArrayRef> anchors)
{
	StLock<Mutex>_(mMutex);
    // extract cert chain as Certificate objects
    mCertChain.resize(chain.count());
    for (uint32 n = 0; n < mCertChain.size(); n++) {
        const TPEvidenceInfo &info = TPEvidenceInfo::overlay(infoList[n]);
        if (info.recordId()) {
			Keychain keychain = keychainByDLDb(info.DlDbHandle);
			DbUniqueRecord uniqueId(keychain->database()->newDbUniqueRecord());
			secdebug("trusteval", "evidence #%lu from keychain \"%s\"", (unsigned long)n, keychain->name());
			*static_cast<CSSM_DB_UNIQUE_RECORD_PTR *>(uniqueId) = info.UniqueRecord;
			uniqueId->activate(); // transfers ownership
			Item ii = keychain->item(CSSM_DL_DB_RECORD_X509_CERTIFICATE, uniqueId);
			Certificate* cert = dynamic_cast<Certificate*>(ii.get());
			if (cert == NULL)
			{
				CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
			}
			
			mCertChain[n] = cert;
        } else if (info.status(CSSM_CERT_STATUS_IS_IN_INPUT_CERTS)) {
            secdebug("trusteval", "evidence %lu from input cert %lu", (unsigned long)n, (unsigned long)info.index());
            assert(info.index() < uint32(CFArrayGetCount(mCerts)));
            SecCertificateRef cert = SecCertificateRef(CFArrayGetValueAtIndex(mCerts,
                info.index()));
            mCertChain[n] = Certificate::required(cert);
        } else if (info.status(CSSM_CERT_STATUS_IS_IN_ANCHORS)) {
            secdebug("trusteval", "evidence %lu from anchor cert %lu", (unsigned long)n, (unsigned long)info.index());
            assert(info.index() < uint32(CFArrayGetCount(anchors)));
            SecCertificateRef cert = SecCertificateRef(CFArrayGetValueAtIndex(anchors,
                info.index()));
            mCertChain[n] = Certificate::required(cert);
        } else {
            // unknown source; make a new Certificate for it
            secdebug("trusteval", "evidence %lu from unknown source", (unsigned long)n);
            mCertChain[n] =
                new Certificate(chain.blobCerts()[n],
					CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER);
        }
    }
    
    // now walk the chain, leaf-to-root, checking for user settings
	TrustStore &store = gStore();
	SecPointer<Policy> policy =
		Policy::required(SecPolicyRef(CFArrayGetValueAtIndex(mPolicies, 0)));
	for (mResultIndex = 0;
			mResult == kSecTrustResultUnspecified && mResultIndex < mCertChain.size();
			mResultIndex++)
	{
		if (!mCertChain[mResultIndex])
		{
			assert(false);
			continue;
		}
		mResult = store.find(mCertChain[mResultIndex], policy, mSearchLibs);
	}
}


//
// Release TP evidence information.
// This information is severely under-defined by CSSM, so we proceed
// as follows:
//  (a) If the evidence matches an Apple-defined pattern, use specific
//      knowledge of that format.
//  (b) Otherwise, assume that the void * are flat blocks of memory.
//
void Trust::releaseTPEvidence(TPVerifyResult &result, Allocator &allocator)
{
	if (result.count() > 0) {	// something to do
		if (result[0].form() == CSSM_EVIDENCE_FORM_APPLE_HEADER) {
			// Apple defined evidence form -- use intimate knowledge
			if (result[0].as<CSSM_TP_APPLE_EVIDENCE_HEADER>()->Version == CSSM_TP_APPLE_EVIDENCE_VERSION
				&& result.count() == 3
				&& result[1].form() == CSSM_EVIDENCE_FORM_APPLE_CERTGROUP
				&& result[2].form() == CSSM_EVIDENCE_FORM_APPLE_CERT_INFO) {
				// proper format
				CertGroup& certs = *result[1].as<CertGroup>();
				CSSM_TP_APPLE_EVIDENCE_INFO *evidence = result[2].as<CSSM_TP_APPLE_EVIDENCE_INFO>();
				uint32 count = certs.count();
				allocator.free(result[0].data());	// just a struct
				certs.destroy(allocator);		// certgroup contents
				allocator.free(result[1].data());	// the CertGroup itself
				for (uint32 n = 0; n < count; n++)
					allocator.free(evidence[n].StatusCodes);
				allocator.free(result[2].data());	// array of (flat) info structs
			} else {
				secdebug("trusteval", "unrecognized Apple TP evidence format");
				// drop it -- better leak than kill
			}
		} else {
			// unknown format -- blindly assume flat blobs
			secdebug("trusteval", "destroying unknown TP evidence format");
			for (uint32 n = 0; n < result.count(); n++)
			{
				allocator.free(result[n].data());
			}
		}
		
		allocator.free (result.Evidence);
	}
}


//
// Clear evaluation results unless state is initial (invalid)
//
void Trust::clearResults()
{
	StLock<Mutex>_(mMutex);
	if (mResult != kSecTrustResultInvalid) {
		releaseTPEvidence(mTpResult, mTP.allocator());
		mResult = kSecTrustResultInvalid;
	}
}


//
// Build evidence information
//
void Trust::buildEvidence(CFArrayRef &certChain, TPEvidenceInfo * &statusChain)
{
	StLock<Mutex>_(mMutex);
	if (mResult == kSecTrustResultInvalid)
		MacOSError::throwMe(errSecTrustNotAvailable);
    certChain = mEvidenceReturned =
        makeCFArray(convert, mCertChain);
	if(mTpResult.count() >= 3) {
		statusChain = mTpResult[2].as<TPEvidenceInfo>();
	}
	else {
		statusChain = NULL;
	}
}


//
// Return extended result dictionary
//
void Trust::extendedResult(CFDictionaryRef &result)
{
	if (mResult == kSecTrustResultInvalid)
		MacOSError::throwMe(errSecTrustNotAvailable);
	if (mExtendedResult)
		CFRetain(mExtendedResult); // retain before handing out to caller
    result = mExtendedResult;
}


//
// Given a DL_DB_HANDLE, locate the Keychain object (from the search list)
//
Keychain Trust::keychainByDLDb(const CSSM_DL_DB_HANDLE &handle)
{
	StLock<Mutex>_(mMutex);
	for (StorageManager::KeychainList::const_iterator it = mSearchLibs.begin();
			it != mSearchLibs.end(); it++)
	{
		try
		{
			if ((*it)->database()->handle() == handle)
				return *it;
		}
		catch (...)
		{
		}
	}
	if(mUsingTrustSettings) {
		try {
			if(trustKeychains().rootStoreHandle() == handle) {
				return trustKeychains().rootStore();
			}
			if(trustKeychains().systemKcHandle() == handle) {
				return trustKeychains().systemKc();
			}
		}
		catch(...) {
			/* one of those is missing; proceed */
		}
	}

	// could not find in search list - internal error
	
	// we now throw an error here rather than assert and silently fail.  That way our application won't crash...
	MacOSError::throwMe(errSecInternal);
}
