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
// Trust.h - Trust control wrappers
//
#ifndef _SECURITY_TRUST_H_
#define _SECURITY_TRUST_H_

#include <Security/SecRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/StorageManager.h>
#include <Security/tpclient.h>
#include <Security/cfutilities.h>
#include <Security/SecTrust.h>
#include <Security/Certificate.h>
#include <Security/Policies.h>
#include <Security/TrustStore.h>
#include <vector>

using namespace CssmClient;

namespace Security {
namespace KeychainCore {


//
// The Trust object manages trust-verification workflow.
// As such, it represents a somewhat more complex concept than
// a single "object".
//
class Trust : public SecCFObject
{
	NOCOPY(Trust)
public:
    Trust(CFTypeRef certificates, CFTypeRef policies);
    virtual ~Trust();

	// set more input parameters
    void action(CSSM_TP_ACTION action)			{ mAction = action; }
    void actionData(CFDataRef data)				{ mActionData = data; }
    void time(CFDateRef verifyTime)				{ mVerifyTime = verifyTime; }
    void anchors(CFArrayRef anchorList)			{ mAnchors = cfArrayize(anchorList); }
    StorageManager::KeychainList &searchLibs()	{ return mSearchLibs; }
    
	// perform evaluation
    void evaluate();
    
	// get at evaluation results
    void buildEvidence(CFArrayRef &certChain, TPEvidenceInfo * &statusChain);
    CSSM_TP_VERIFY_CONTEXT_RESULT_PTR cssmResult();
    
    SecTrustResultType result() const			{ return mResult; }
    TP getTPHandle() const						{ return mTP; }
    
	// an independent release function for TP evidence results
	// (yes, we could hand this out to the C layer if desired)
	static void releaseTPEvidence(TPVerifyResult &result, CssmAllocator &allocator);

private:
    SecTrustResultType diagnoseOutcome();
    void evaluateUserTrust(const CertGroup &certs,
        const CSSM_TP_APPLE_EVIDENCE_INFO *info);
	void clearResults();

private:
    TP mTP;							// our TP
    
    // input arguments: set up before evaluate()
    CSSM_TP_ACTION mAction;			// TP action to verify
    CFRef<CFDataRef> mActionData;	// action data
    CFRef<CFDateRef> mVerifyTime;	// verification "now"
    CFRef<CFArrayRef> mCerts;		// certificates to verify (item 1 is subject)
    CFRef<CFArrayRef> mPolicies;	// array of policy objects to control verification
    CFRef<CFArrayRef> mAnchors;		// array of anchor certs
    StorageManager::KeychainList mSearchLibs; // array of databases to search
    
    // evaluation results: set as a result of evaluate()
    SecTrustResultType mResult;		// result classification
    uint32 mResultIndex;			// which result cert made the decision?
    OSStatus mTpReturn;				// return code from TP Verify
    TPVerifyResult mTpResult;		// result of latest TP verify

    vector< RefPointer<Certificate> > mCertChain; // distilled certificate chain

    // information returned to caller but owned by us
    CFRef<CFArrayRef> mEvidenceReturned; // evidence chain returned

public:
    static ModuleNexus<TrustStore> Trust::gStore;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_TRUST_H_
