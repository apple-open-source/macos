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

//
// Trust.cpp
//
#include <Security/Trust.h>
#include <Security/cssmdates.h>
#include <Security/cfutilities.h>
#include <CoreFoundation/CFData.h>
#include <Security/SecCertificate.h>
#include "SecBridge.h"

using namespace KeychainCore;


//
// For now, we use a global TrustStore
//
ModuleNexus<TrustStore> Trust::gStore;


//
// Construct a Trust object with suitable defaults.
// Use setters for additional arguments before calling evaluate().
//
Trust::Trust(CFTypeRef certificates, CFTypeRef policies)
    : mTP(gGuidAppleX509TP), mAction(CSSM_TP_ACTION_DEFAULT),
      mVerifyTime(NULL),
      mCerts(cfArrayize(certificates)), mPolicies(cfArrayize(policies)),
      mResult(kSecTrustResultInvalid)
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
		MacOSError::throwMe(errSecNotAvailable);
    return &mTpResult;
}


// SecCertificateRef -> CssmData
CssmData cfCertificateData(SecCertificateRef certificate)
{
    return gTypes().certificate.required(certificate)->data();
}

// SecPolicyRef -> CssmField (CFDataRef/NULL or oid/value of a SecPolicy)
CssmField cfField(SecPolicyRef item)
{
	RefPointer<Policy> policy = gTypes().policy.required(SecPolicyRef(item));
    return CssmField(policy->oid(), policy->value());
}

// SecKeychain -> CssmDlDbHandle
CSSM_DL_DB_HANDLE cfKeychain(SecKeychainRef ref)
{
	Keychain keychain = gTypes().keychain.required(ref);
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
	// if we have evaluated before, release prior result
	clearResults();

    // build the target cert group
    CFToVector<CssmData, SecCertificateRef, cfCertificateData> subjects(mCerts);
    CertGroup subjectCertGroup(CSSM_CERT_X_509v3,
            CSSM_CERT_ENCODING_BER, CSSM_CERTGROUP_DATA);
    subjectCertGroup.count() = subjects;
    subjectCertGroup.blobCerts() = subjects;
    
    // build a TP_VERIFY_CONTEXT, a veritable nightmare of a data structure
    TPBuildVerifyContext context(mAction);
    if (mActionData)
        context.actionData() = cfData(mActionData);
    
    // policies (one at least, please)
    CFToVector<CssmField, SecPolicyRef, cfField> policies(mPolicies);
    if (policies.empty())
        MacOSError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
    context.setPolicies(policies, policies);

    // anchor certificates
    CFCopyRef<CFArrayRef> anchors(mAnchors);
    if (!anchors)
        anchors = gStore().copyRootCertificates();	// retains
    CFToVector<CssmData, SecCertificateRef, cfCertificateData> roots(anchors);
    context.anchors(roots, roots);
    
	// dlDbList (keychain list)
	vector<CSSM_DL_DB_HANDLE> dlDbList;
	for (StorageManager::KeychainList::const_iterator it = mSearchLibs.begin();
			it != mSearchLibs.end(); it++)
		dlDbList.push_back((*it)->database()->handle());
	context.setDlDbList(dlDbList.size(), &dlDbList[0]);

    // verification time
    char timeString[15];
    if (mVerifyTime) {
        CssmUniformDate(mVerifyTime).convertTo(timeString, sizeof(timeString));
        context.time(timeString);
    }

    // Go TP!
    try {
        mTP->certGroupVerify(subjectCertGroup, context, &mTpResult);
        mTpReturn = noErr;
    } catch (CssmCommonError &err) {
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
            mTpResult[2].as<CSSM_TP_APPLE_EVIDENCE_INFO>());
    } else {
        // unexpected evidence information. Can't use it
        debug("trusteval", "unexpected evidence ignored");
    }
}


//
// Classify the TP outcome in terms of a SecTrustResultType
//
SecTrustResultType Trust::diagnoseOutcome()
{
    switch (mTpReturn) {
    case noErr:									// peachy
        return kSecTrustResultUnspecified;
    case CSSMERR_TP_CERT_EXPIRED:				// expired cert
    case CSSMERR_TP_CERT_NOT_VALID_YET:			// mis-expired cert
    case CSSMERR_TP_NOT_TRUSTED:				// no root, no anchor
    case CSSMERR_TP_VERIFICATION_FAILURE:		// root does not self-verify
    case CSSMERR_TP_INVALID_ANCHOR_CERT:		// valid is not an anchor
    case CSSMERR_TP_VERIFY_ACTION_FAILED:		// policy action failed
        return kSecTrustResultRecoverableTrustFailure;
    case CSSMERR_TP_INVALID_CERTIFICATE:		// bad certificate
        return kSecTrustResultFatalTrustFailure;
    default:
        return kSecTrustResultOtherError;		// unknown
    }
}


//
// Assuming a good evidence chain, check user trust
// settings and set mResult accordingly.
//
void Trust::evaluateUserTrust(const CertGroup &chain,
    const CSSM_TP_APPLE_EVIDENCE_INFO *infoList)
{
    // extract cert chain as Certificate objects
    //@@@ once new Evidence is in, use it to build the Certificates
    mCertChain.resize(chain.count());
    for (uint32 n = 0; n < mCertChain.size(); n++) {
        const TPEvidenceInfo &info = TPEvidenceInfo::overlay(infoList[n]);
        if (info.recordId()) {
            debug("trusteval", "evidence %ld from DLDB source", n);
            assert(false);	// from DL/DB search - not yet implemented
        } else if (info.status(CSSM_CERT_STATUS_IS_IN_INPUT_CERTS)) {
            debug("trusteval", "evidence %ld from input cert %ld", n, info.index());
            assert(info.index() < uint32(CFArrayGetCount(mCerts)));
            SecCertificateRef cert = SecCertificateRef(CFArrayGetValueAtIndex(mCerts,
                info.index()));
            mCertChain[n] = gTypes().certificate.required(cert);
        } else if (info.status(CSSM_CERT_STATUS_IS_IN_ANCHORS)) {
            debug("trusteval", "evidence %ld from anchor cert %ld", n, info.index());
            assert(info.index() < uint32(CFArrayGetCount(mAnchors)));
            SecCertificateRef cert = SecCertificateRef(CFArrayGetValueAtIndex(mAnchors,
                info.index()));
            mCertChain[n] = gTypes().certificate.required(cert);
        } else {
            // unknown source; make a new Certificate for it
            debug("trusteval", "evidence %ld from unknown source", n);
            mCertChain[n] =
                new Certificate(chain.blobCerts()[n],
					CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER);
        }
    }
    
    // now walk the chain, leaf-to-root, checking for user settings
    TrustStore &store = gStore();
    RefPointer<Policy> policy =
        gTypes().policy.required(SecPolicyRef(CFArrayGetValueAtIndex(mPolicies, 0)));
    for (mResultIndex = 0;
            mResult == kSecTrustResultUnspecified && mResultIndex < mCertChain.size();
            mResultIndex++)
        mResult = store.find(mCertChain[mResultIndex], policy);
}


//
// Release TP evidence information.
// This information is severely under-defined by CSSM, so we proceed
// as follows:
//  (a) If the evidence matches an Apple-defined pattern, use specific
//      knowledge of that format.
//  (b) Otherwise, assume that the void * are flat blocks of memory.
//
void Trust::releaseTPEvidence(TPVerifyResult &result, CssmAllocator &allocator)
{
	if (result.count() > 0) {	// something to do
		if (result[0].form() == CSSM_EVIDENCE_FORM_APPLE_HEADER) {
			// Apple defined evidence form -- use intimate knowledge
			if (result[0].as<CSSM_TP_APPLE_EVIDENCE_HEADER>()->Version == CSSM_TP_APPLE_EVIDENCE_VERSION
				&& result.count() == 3
				&& result[1].form() == CSSM_EVIDENCE_FORM_APPLE_CERTGROUP
				&& result[2].form() == CSSM_EVIDENCE_FORM_APPLE_CERT_INFO) {
				// proper format
				allocator.free(result[0].data());	// just a struct
				result[1].as<CertGroup>()->destroy(allocator); // CertGroup contents
				allocator.free(result[1].data());	// the CertGroup itself
				allocator.free(result[2].data());	// array of (flat) info structs
			} else {
				debug("trusteval", "unrecognized Apple TP evidence format");
				// drop it -- better leak than kill
			}
		} else {
			// unknown format -- blindly assume flat blobs
			debug("trusteval", "destroying unknown TP evidence format");
			for (uint32 n = 0; n < result.count(); n++)
				allocator.free(result[n].data());
		}
	}
}


//
// Clear evaluation results unless state is initial (invalid)
//
void Trust::clearResults()
{
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
	if (mResult == kSecTrustResultInvalid)
		MacOSError::throwMe(errSecNotAvailable);
    certChain = mEvidenceReturned =
        makeCFArray(gTypes().certificate, mCertChain);
    statusChain = mTpResult[2].as<TPEvidenceInfo>();
}
