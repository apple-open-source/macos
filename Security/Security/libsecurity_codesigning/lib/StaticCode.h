/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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
// StaticCode - SecStaticCode API objects
//
#ifndef _H_STATICCODE
#define _H_STATICCODE

#include "cs.h"
#include "csutilities.h"
#include "Requirements.h"
#include "requirement.h"
#include "diskrep.h"
#include "codedirectory.h"
#include <Security/SecTrust.h>
#include <CoreFoundation/CFData.h>
#include <security_utilities/dispatch.h>

namespace Security {
namespace CodeSigning {


class SecCode;


//
// A SecStaticCode object represents the file system version of some code.
// There's a lot of pieces to this, and we'll bring them all into
// memory here (lazily) and let you fondle them with ease.
//
// Note that concrete knowledge of where stuff is stored resides in the DiskRep
// object we hold. DiskReps allocate, retrieve, and return data to us. We are
// responsible for interpreting, caching, and validating them. (In other words,
// DiskReps know where stuff is and how it is stored, but we know what it means.)
//
// Data accessors (returning CFDataRef, CFDictionaryRef, various pointers, etc.)
// cache those values internally and return unretained(!) references ("Get" style)
// that are valid as long as the SecStaticCode object's lifetime, or until
// resetValidity() is called, whichever is sooner. If you need to keep them longer,
// retain or copy them as needed.
//
class SecStaticCode : public SecCFObject {
	NOCOPY(SecStaticCode)
	
protected:
	//
	// A context for resource validation operations, to tailor error response.
	// The base class throws an exception immediately and ignores detail data.
	// 
	class ValidationContext {
	public:
		ValidationContext(SecStaticCode &c) : code(c) { }
		virtual ~ValidationContext();
		virtual void reportProblem(OSStatus rc, CFStringRef type, CFTypeRef value);
		
		virtual OSStatus osStatus()	{ return noErr; }
		virtual void throwMe()		{ }
		
		SecStaticCode &code;
	};
	
	//
	// A CollectingContext collects all error details and throws an annotated final error.
	//
	class CollectingContext : public ValidationContext {
	public:
		CollectingContext(SecStaticCode &c) : ValidationContext(c), mStatus(errSecSuccess) { }
		void reportProblem(OSStatus rc, CFStringRef type, CFTypeRef value);
		
		OSStatus osStatus()		{ return mStatus; }
		operator OSStatus () const		{ return mStatus; }
		void throwMe() __attribute__((noreturn));

	private:
		CFRef<CFMutableDictionaryRef> mCollection;
		OSStatus mStatus;
		Mutex mLock;
	};

public:
	SECCFFUNCTIONS(SecStaticCode, SecStaticCodeRef,
		errSecCSInvalidObjectRef, gCFObjects().StaticCode)
	
	// implicitly convert SecCodeRefs to their SecStaticCodeRefs
	static SecStaticCode *requiredStatic(SecStaticCodeRef ref);	// convert SecCodeRef
	static SecCode *optionalDynamic(SecStaticCodeRef ref); // extract SecCodeRef or NULL if static

	SecStaticCode(DiskRep *rep);
    virtual ~SecStaticCode() throw();

    void initializeFromParent(const SecStaticCode& parent);

    bool equal(SecCFObject &other);
    CFHashCode hash();
	
	void detachedSignature(CFDataRef sig);		// attach an explicitly given detached signature
	void checkForSystemSignature();				// check for and attach system-supplied detached signature

	const CodeDirectory *codeDirectory(bool check = true);
	CFDataRef cdHash();
	CFDataRef signature();
	CFAbsoluteTime signingTime();
	CFAbsoluteTime signingTimestamp();
	bool isSigned() { return codeDirectory(false) != NULL; }
	DiskRep *diskRep() { return mRep; }
	bool isDetached() const { return mRep->base() != mRep; }
	std::string mainExecutablePath() { return mRep->mainExecutablePath(); }
	CFURLRef copyCanonicalPath() const { return mRep->copyCanonicalPath(); }
	std::string identifier() { return codeDirectory()->identifier(); }
	const char *teamID() { return codeDirectory()->teamID(); }
	std::string format() const { return mRep->format(); }
	std::string signatureSource();
 	virtual CFDataRef component(CodeDirectory::SpecialSlot slot, OSStatus fail = errSecCSSignatureFailed);
 	virtual CFDictionaryRef infoDictionary();

	CFDictionaryRef entitlements();

	CFDictionaryRef resourceDictionary(bool check = true);
	CFURLRef resourceBase();
	CFDataRef resource(std::string path);
	CFDataRef resource(std::string path, ValidationContext &ctx);
	void validateResource(CFDictionaryRef files, std::string path, bool isSymlink, ValidationContext &ctx, SecCSFlags flags, uint32_t version);
	
	bool flag(uint32_t tested);

	SecCodeCallback monitor() const { return mMonitor; }
	void setMonitor(SecCodeCallback monitor) { mMonitor = monitor; }
	CFTypeRef reportEvent(CFStringRef stage, CFDictionaryRef info);
	void reportProgress(unsigned amount = 1);
	
	void setValidationFlags(SecCSFlags flags) { mValidationFlags = flags; }
	void setValidationModifiers(CFDictionaryRef modifiers);
	
	void resetValidity();						// clear validation caches (if something may have changed)
	
	bool validated() const	{ return mValidated; }
	bool revocationChecked() const { return mRevocationChecked; }
	bool valid() const
		{ assert(validated()); return mValidated && (mValidationResult == errSecSuccess); }
	bool validatedExecutable() const	{ return mExecutableValidated; }
	bool validatedResources() const	{ return mResourcesValidated; }
	
	void prepareProgress(unsigned workload);
	void cancelValidation();

	void validateDirectory();
	virtual void validateComponent(CodeDirectory::SpecialSlot slot, OSStatus fail = errSecCSSignatureFailed);
	void validateNonResourceComponents();
	unsigned estimateResourceWorkload();
	void validateResources(SecCSFlags flags);
	void validateExecutable();
	void validateNestedCode(CFURLRef path, const ResourceSeal &seal, SecCSFlags flags, bool isFramework);
	
	const Requirements *internalRequirements();
	const Requirement *internalRequirement(SecRequirementType type);
	const Requirement *designatedRequirement();
	const Requirement *defaultDesignatedRequirement();		// newly allocated (caller owns)
	
	void validateRequirements(SecRequirementType type, SecStaticCode *target,
		OSStatus nullError = errSecSuccess);										// target against my [type], throws
	void validateRequirement(const Requirement *req, OSStatus failure);		// me against [req], throws
	bool satisfiesRequirement(const Requirement *req, OSStatus failure);	// me against [req], returns on clean miss
	
	// certificates are available after signature validation (they are stored in the CMS signature)
	SecCertificateRef cert(int ix);		// get a cert from the cert chain
	CFArrayRef certificates();			// get the entire certificate chain
	
	CFDictionaryRef signingInformation(SecCSFlags flags); // omnibus information-gathering API (creates new dictionary)

	static bool isAppleDeveloperCert(CFArrayRef certs); // determines if this is an apple developer certificate for libraray validation

public:
	void staticValidate(SecCSFlags flags, const SecRequirement *req);
	void staticValidateCore(SecCSFlags flags, const SecRequirement *req);
	
protected:
	CFDictionaryRef getDictionary(CodeDirectory::SpecialSlot slot, bool check = true); // component value as a dictionary
	bool verifySignature();
	CFArrayRef verificationPolicies();

	static void checkOptionalResource(CFTypeRef key, CFTypeRef value, void *context);
	bool hasWeakResourceRules(CFDictionaryRef rulesDict, uint32_t version, CFArrayRef allowedOmissions);

	void handleOtherArchitectures(void (^handle)(SecStaticCode* other));

private:
	void validateOtherVersions(CFURLRef path, SecCSFlags flags, SecRequirementRef req, SecStaticCode *code);

private:
	RefPointer<DiskRep> mRep;			// on-disk representation
	CFRef<CFDataRef> mDetachedSig;		// currently applied explicit detached signature
	
	// private validation modifiers (only used by Gatekeeper checkfixes)
	MacOSErrorSet mTolerateErrors;		// soft error conditions to ignore
	CFRef<CFArrayRef> mAllowOmissions;	// additionally allowed resource omissions
	
	// master validation state
	bool mValidated;					// core validation was attempted
	bool mRevocationChecked;			// the signature was checked for revocation
	OSStatus mValidationResult;			// outcome of core validation
	bool mValidationExpired;			// outcome had expired certificates
	
	// static executable validation state (nested within mValidated/mValid)
	bool mExecutableValidated;			// tried to validate executable file
	OSStatus mExecutableValidResult;		// outcome if mExecutableValidated

	// static resource validation state (nested within mValidated/mValid)
	bool mResourcesValidated;			// tried to validate resources
	bool mResourcesDeep;				// cached validation was deep
	OSStatus mResourcesValidResult;			// outcome if mResourceValidated or...
	ValidationContext *mResourcesValidContext; // resource error reporting funnel
	
	// validation progress state (set when static validation starts)
	SecCSFlags mValidationFlags;		// API flags passed to static validation
	unsigned mTotalWork;				// total expected work (arbitrary units)
	unsigned mCurrentWork;				// currently completed work
	bool mCancelPending;				// cancellation was requested
	Dispatch::Queue mProgressQueue;		// progress reporting queue

	// cached contents
	CFRef<CFDataRef> mDir;				// code directory data
	CFRef<CFDataRef> mSignature;		// CMS signature data
	CFAbsoluteTime mSigningTime;		// (signed) signing time
	CFAbsoluteTime mSigningTimestamp;		// Timestamp time (from timestamping authority)
	CFRef<CFDataRef> mCache[cdSlotCount]; // NULL => not tried, kCFNull => absent, other => present
	
	// alternative cache forms (storage may depend on cached contents above)
	CFRef<CFDictionaryRef> mInfoDict;	// derived from mCache slot
	CFRef<CFDictionaryRef> mEntitlements; // derived from mCache slot
	CFRef<CFDictionaryRef> mResourceDict; // derived from mCache slot
	const Requirement *mDesignatedReq;	// cached designated req if we made one up
	CFRef<CFDataRef> mCDHash;			// hash of CodeDirectory
	
	bool mGotResourceBase;				// asked mRep for resourceBasePath
	CFRef<CFURLRef> mResourceBase;		// URL form of resource base directory

	SecCodeCallback mMonitor;			// registered monitor callback

	LimitedAsync *mLimitedAsync;		// limited async workers for verification

	// signature verification outcome (mTrust == NULL => not done yet)
	CFRef<SecTrustRef> mTrust;			// outcome of crypto validation (valid or not)
	CFRef<CFArrayRef> mCertChain;
	CSSM_TP_APPLE_EVIDENCE_INFO *mEvalDetails;
};


} // end namespace CodeSigning
} // end namespace Security

#endif // !_H_STATICCODE
