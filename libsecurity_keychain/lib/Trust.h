/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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
// Trust.h - Trust control wrappers
//
#ifndef _SECURITY_TRUST_H_
#define _SECURITY_TRUST_H_

#include <CoreFoundation/CoreFoundation.h>
#include <security_keychain/StorageManager.h>
#include <security_cdsa_client/tpclient.h>
#include <security_utilities/cfutilities.h>
#include <Security/SecTrust.h>
#include <security_keychain/Certificate.h>
#include <security_keychain/Policies.h>
#include <security_keychain/TrustStore.h>
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
	SECCFFUNCTIONS(Trust, SecTrustRef, errSecInvalidItemRef, gTypes().Trust)

    Trust(CFTypeRef certificates, CFTypeRef policies);
    virtual ~Trust();

	// set (or reset) more input parameters
	void policies(CFTypeRef policies)			{ mPolicies.take(cfArrayize(policies)); }
    void action(CSSM_TP_ACTION action)			{ mAction = action; }
    void actionData(CFDataRef data)				{ mActionData = data; }
    void time(CFDateRef verifyTime)				{ mVerifyTime = verifyTime; }
    void anchors(CFArrayRef anchorList)			{ mAnchors.take(cfArrayize(anchorList)); }
    StorageManager::KeychainList &searchLibs()	{ return mSearchLibs; }
    void searchLibs(StorageManager::KeychainList &libs)	{ mSearchLibs = libs; }
    
	// perform evaluation
    void evaluate();
    
	// get at evaluation results
    void buildEvidence(CFArrayRef &certChain, TPEvidenceInfo * &statusChain);
    CSSM_TP_VERIFY_CONTEXT_RESULT_PTR cssmResult();
	void extendedResult(CFDictionaryRef &extendedResult);
    
    SecTrustResultType result() const			{ return mResult; }
	OSStatus cssmResultCode() const				{ return mTpReturn; }
    TP getTPHandle() const						{ return mTP; }
    CFArrayRef policies() const					{ return mPolicies; }
    CFArrayRef anchors() const					{ return mAnchors; }
	CFDateRef time() const						{ return mVerifyTime; }

	// an independent release function for TP evidence results
	// (yes, we could hand this out to the C layer if desired)
	static void releaseTPEvidence(TPVerifyResult &result, Allocator &allocator);

private:
    SecTrustResultType diagnoseOutcome();
    void evaluateUserTrust(const CertGroup &certs,
        const CSSM_TP_APPLE_EVIDENCE_INFO *info,
		CFCopyRef<CFArrayRef> anchors);
	void clearResults();
	
	Keychain keychainByDLDb(const CSSM_DL_DB_HANDLE &handle);

	/* revocation policy support */
	CFMutableArrayRef	addSpecifiedRevocationPolicies(uint32 &numAdded, 
							Allocator &alloc);
	void				freeSpecifiedRevocationPolicies(CFArrayRef policies,
							uint32 numAdded, 
							Allocator &alloc);
	CFMutableArrayRef	addPreferenceRevocationPolicies(uint32 &numAdded,
							Allocator &alloc);
	void				freePreferenceRevocationPolicies(CFArrayRef policies,
							uint32 numAdded, 
							Allocator &alloc);
	bool				revocationPolicySpecified(CFArrayRef policies);
	CFMutableArrayRef	forceOCSPRevocationPolicy(uint32 &numAdded, 
							Allocator &alloc);
	
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
    StorageManager::KeychainList mSearchLibsUsed; // augmented mSearchLibs used

    vector< SecPointer<Certificate> > mCertChain; // distilled certificate chain

    // information returned to caller but owned by us
    CFRef<CFArrayRef> mEvidenceReturned;	// evidence chain returned
	CFRef<CFArrayRef> mAllowedAnchors;		// array of permitted anchor certificates
	CFRef<CFArrayRef> mFilteredCerts;		// array of certificates to verify, post-filtering
    CFRef<CFDictionaryRef> mExtendedResult;	// dictionary of extended results

	bool mUsingTrustSettings;

public:
    static ModuleNexus<TrustStore> gStore;

private:
	Mutex mMutex;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_TRUST_H_
