/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
#include "StaticCode.h"
#include "Code.h"
#include "reqmaker.h"
#include "reqdumper.h"
#include "sigblob.h"
#include "resources.h"
#include "renum.h"
#include "csutilities.h"
#include <CoreFoundation/CFURLAccess.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/CMSPrivate.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecCmsSignedData.h>
#include <security_utilities/unix++.h>
#include <security_codesigning/cfmunge.h>
#include <Security/CMSDecoder.h>


namespace Security {
namespace CodeSigning {

using namespace UnixPlusPlus;


//
// We use a DetachedRep to interpose (filter) the genuine DiskRep representing
// the code on disk, *if* a detached signature was set on this object. In this
// situation, mRep will point to a (2 element) chain of DiskReps.
//
// This is a neat way of dealing with the (unusual) detached-signature case
// without disturbing things unduly. Consider DetachedDiskRep to be closely
// married to SecStaticCode; it's unlikely to work right if you use it elsewhere.
//
class DetachedRep : public DiskRep {
public:
	DetachedRep(CFDataRef sig, DiskRep *orig);
	
	const RefPointer<DiskRep> original;		// underlying representation
	
	DiskRep *base()							{ return original; }
	CFDataRef component(CodeDirectory::SpecialSlot slot);
	std::string mainExecutablePath()		{ return original->mainExecutablePath(); }
	CFURLRef canonicalPath()				{ return original->canonicalPath(); }
	std::string recommendedIdentifier()		{ return original->recommendedIdentifier(); }
	std::string resourcesRootPath()			{ return original->resourcesRootPath(); }
	CFDictionaryRef defaultResourceRules()	{ return original->defaultResourceRules(); }
	Universal *mainExecutableImage()		{ return original->mainExecutableImage(); }
	size_t signingBase()					{ return original->signingBase(); }
	size_t signingLimit()					{ return original->signingLimit(); }
	std::string format()					{ return original->format(); }
	FileDesc &fd()							{ return original->fd(); }
	void flush()							{ return original->flush(); }

private:
	CFCopyRef<CFDataRef> mSignature;
	const EmbeddedSignatureBlob *mArch;		// current architecture; points into mSignature
	const EmbeddedSignatureBlob *mGlobal;	// shared elements; points into mSignature
};


//
// Construct a SecStaticCode object given a disk representation object
//
SecStaticCode::SecStaticCode(DiskRep *rep)
	: mRep(rep),
	  mValidated(false), mExecutableValidated(false),
	  mDesignatedReq(NULL), mGotResourceBase(false), mEvalDetails(NULL)
{
}


//
// Clean up a SecStaticCode object
//
SecStaticCode::~SecStaticCode() throw()
{
	::free(const_cast<Requirement *>(mDesignatedReq));
}


//
// Attach a detached signature.
//
void SecStaticCode::detachedSignature(CFDataRef sigData)
{
	if (sigData)
		mRep = new DetachedRep(sigData, mRep->base());
	else
		mRep = mRep->base();
}


//
// Do ::required, but convert incoming SecCodeRefs to their SecStaticCodeRefs
// (if possible).
//
SecStaticCode *SecStaticCode::requiredStatic(SecStaticCodeRef ref)
{
	SecCFObject *object = SecCFObject::required(ref, errSecCSInvalidObjectRef);
	if (SecStaticCode *scode = dynamic_cast<SecStaticCode *>(object))
		return scode;
	else if (SecCode *code = dynamic_cast<SecCode *>(object))
		return code->staticCode();
	else	// neither (a SecSomethingElse)
		MacOSError::throwMe(errSecCSInvalidObjectRef);
}

SecCode *SecStaticCode::optionalDynamic(SecStaticCodeRef ref)
{
	SecCFObject *object = SecCFObject::required(ref, errSecCSInvalidObjectRef);
	if (dynamic_cast<SecStaticCode *>(object))
		return NULL;
	else if (SecCode *code = dynamic_cast<SecCode *>(object))
		return code;
	else	// neither (a SecSomethingElse)
		MacOSError::throwMe(errSecCSInvalidObjectRef);
}


//
// Void all cached validity data.
//
// We also throw out cached components, because the new signature data may have
// a different idea of what components should be present. We could reconcile the
// cached data instead, if performance seems to be impacted.
//
void SecStaticCode::resetValidity()
{
	secdebug("staticCode", "%p resetting validity status", this);
	mValidated = false;
	mExecutableValidated = false;
	mDir = NULL;
	mSignature = NULL;
	for (unsigned n = 0; n < cdSlotCount; n++)
		mCache[n] = NULL;
	mInfoDict = NULL;
	mEntitlements = NULL;
	mResourceDict = NULL;
	mDesignatedReq = NULL;
	mTrust = NULL;
	mCertChain = NULL;
	mEvalDetails = NULL;
	mRep->flush();
}


//
// Retrieve a sealed component by special slot index.
// If the CodeDirectory has already been validated, validate against that.
// Otherwise, retrieve the component without validation (but cache it). Validation
// will go through the cache and validate all cached components.
//
CFDataRef SecStaticCode::component(CodeDirectory::SpecialSlot slot)
{
	assert(slot <= cdSlotMax);
	
	CFRef<CFDataRef> &cache = mCache[slot];
	if (!cache) {
		if (CFRef<CFDataRef> data = mRep->component(slot)) {
			if (validated()) // if the directory has been validated...
				if (!codeDirectory()->validateSlot(CFDataGetBytePtr(data), // ... and it's no good
						CFDataGetLength(data), -slot))
					MacOSError::throwMe(errSecCSSignatureFailed); // ... then bail
			cache = data;	// it's okay, cache it
		} else {	// absent, mark so
			if (validated())	// if directory has been validated...
				if (codeDirectory()->slotIsPresent(-slot)) // ... and the slot is NOT missing
					MacOSError::throwMe(errSecCSSignatureFailed);	// was supposed to be there
			cache = CFDataRef(kCFNull);		// white lie
		}
	}
	return (cache == CFDataRef(kCFNull)) ? NULL : cache.get();
}


//
// Get the CodeDirectory.
// Throws (if check==true) or returns NULL (check==false) if there is none.
// Always throws if the CodeDirectory exists but is invalid.
// NEVER validates against the signature.
//
const CodeDirectory *SecStaticCode::codeDirectory(bool check /* = true */)
{
	if (!mDir) {
		if (mDir.take(mRep->codeDirectory())) {
			const CodeDirectory *dir = reinterpret_cast<const CodeDirectory *>(CFDataGetBytePtr(mDir));
			dir->checkVersion();
		}
	}
	if (mDir)
		return reinterpret_cast<const CodeDirectory *>(CFDataGetBytePtr(mDir));
	if (check)
		MacOSError::throwMe(errSecCSUnsigned);
	return NULL;
}


//
// Return the CMS signature blob; NULL if none found.
//
CFDataRef SecStaticCode::signature()
{
	if (!mSignature)
		mSignature.take(mRep->signature());
	if (mSignature)
		return mSignature;
	MacOSError::throwMe(errSecCSUnsigned);
}


//
// Verify the signature on the CodeDirectory.
// If this succeeds (doesn't throw), the CodeDirectory is statically trustworthy.
// Any outcome (successful or not) is cached for the lifetime of the StaticCode.
//
void SecStaticCode::validateDirectory()
{
	// echo previous outcome, if any
	if (!validated())
		try {
			// perform validation (or die trying)
			secdebug("staticCode", "%p validating directory", this);
			mValidationExpired = verifySignature();
			component(cdInfoSlot);		// force load of Info Dictionary (if any)
			for (CodeDirectory::SpecialSlot slot = codeDirectory()->nSpecialSlots;
					slot >= 1; --slot)
				if (mCache[slot])	// if we already loaded that resource...
					validateComponent(slot); // ... then check it now
			mValidated = true;			// we've done the deed...
			mValidationResult = noErr;	// ... and it was good
		} catch (const CommonError &err) {
			mValidated = true;
			mValidationResult = err.osStatus();
			throw;
		} catch (...) {
			secdebug("staticCode", "%p validation threw non-common exception", this);
			mValidated = true;
			mValidationResult = errSecCSInternalError;
			throw;
		}
	assert(validated());
	if (mValidationResult == noErr) {
		if (mValidationExpired)
			if ((apiFlags() & kSecCSConsiderExpiration)
					|| (codeDirectory()->flags & kSecCodeSignatureForceExpiration))
				MacOSError::throwMe(CSSMERR_TP_CERT_EXPIRED);
	} else
		MacOSError::throwMe(mValidationResult);
}


//
// Get the (signed) signing date from the code signature.
// Sadly, we need to validate the signature to get the date (as a side benefit).
// This means that you can't get the signing time for invalidly signed code.
//
// We could run the decoder "almost to" verification to avoid this, but there seems
// little practical point to such a duplication of effort.
//
CFAbsoluteTime SecStaticCode::signingTime()
{
	validateDirectory();
	return mSigningTime;
}


//
// Verify the CMS signature on the CodeDirectory.
// This performs the cryptographic tango. It returns if the signature is valid,
// or throws if it is not. As a side effect, a successful return sets up the
// cached certificate chain for future use.
//
bool SecStaticCode::verifySignature()
{
	// ad-hoc signed code is considered validly signed by definition
	if (flag(kSecCodeSignatureAdhoc)) {
		secdebug("staticCode", "%p considered verified since it is ad-hoc", this);
		return false;
	}

	// decode CMS and extract SecTrust for verification
	secdebug("staticCode", "%p verifying signature", this);
	CFRef<CMSDecoderRef> cms;
	MacOSError::check(CMSDecoderCreate(&cms.aref())); // create decoder
	CFDataRef sig = this->signature();
	MacOSError::check(CMSDecoderUpdateMessage(cms, CFDataGetBytePtr(sig), CFDataGetLength(sig)));
	this->codeDirectory();	// load CodeDirectory (sets mDir)
	MacOSError::check(CMSDecoderSetDetachedContent(cms, mDir));
	MacOSError::check(CMSDecoderFinalizeMessage(cms));
	MacOSError::check(CMSDecoderSetSearchKeychain(cms, cfEmptyArray()));
    CMSSignerStatus status;
    MacOSError::check(CMSDecoderCopySignerStatus(cms, 0, verificationPolicy(),
		false, &status, &mTrust.aref(), NULL));
	
	// get signing date (we've got the decoder handle right here)
	mSigningTime = 0;		// "not present" marker (nobody could code sign on Jan 1, 2001 :-)
	SecCmsMessageRef cmsg;
	MacOSError::check(CMSDecoderGetCmsMessage(cms, &cmsg));
	SecCmsSignedDataRef signedData = NULL;
    int numContentInfos = SecCmsMessageContentLevelCount(cmsg);
    for(int dex = 0; !signedData && dex < numContentInfos; dex++) {
        SecCmsContentInfoRef ci = SecCmsMessageContentLevel(cmsg, dex);
        SECOidTag tag = SecCmsContentInfoGetContentTypeTag(ci);
        switch(tag) {
            case SEC_OID_PKCS7_SIGNED_DATA:
				if (SecCmsSignedDataRef signedData = SecCmsSignedDataRef(SecCmsContentInfoGetContent(ci)))
					if (SecCmsSignerInfoRef signerInfo = SecCmsSignedDataGetSignerInfo(signedData, 0))
						SecCmsSignerInfoGetSigningTime(signerInfo, &mSigningTime);
                break;
            default:
                break;
        }
    }

	// set up the environment for SecTrust
	MacOSError::check(SecTrustSetAnchorCertificates(mTrust, cfEmptyArray())); // no anchors
	CSSM_APPLE_TP_ACTION_DATA actionData = {
		CSSM_APPLE_TP_ACTION_VERSION,	// version of data structure
		CSSM_TP_ACTION_IMPLICIT_ANCHORS	// action flags
	};
	
	for (;;) {
		MacOSError::check(SecTrustSetParameters(mTrust,
			CSSM_TP_ACTION_DEFAULT, CFTempData(&actionData, sizeof(actionData))));
	
		// evaluate trust and extract results
		SecTrustResultType trustResult;
		MacOSError::check(SecTrustEvaluate(mTrust, &trustResult));
		MacOSError::check(SecTrustGetResult(mTrust, &trustResult, &mCertChain.aref(), &mEvalDetails));
		secdebug("staticCode", "%p verification result=%d chain=%ld",
			this, trustResult, mCertChain ? CFArrayGetCount(mCertChain) : -1);
		switch (trustResult) {
		case kSecTrustResultProceed:
		case kSecTrustResultConfirm:
		case kSecTrustResultUnspecified:
			break;				// success
		case kSecTrustResultDeny:
			MacOSError::throwMe(CSSMERR_APPLETP_TRUST_SETTING_DENY);	// user reject
		case kSecTrustResultInvalid:
			assert(false);		// should never happen
			MacOSError::throwMe(CSSMERR_TP_NOT_TRUSTED);
		case kSecTrustResultRecoverableTrustFailure:
		case kSecTrustResultFatalTrustFailure:
		case kSecTrustResultOtherError:
			{
				OSStatus result;
				MacOSError::check(SecTrustGetCssmResultCode(mTrust, &result));
				if (result == CSSMERR_TP_CERT_EXPIRED && !(actionData.ActionFlags & CSSM_TP_ACTION_ALLOW_EXPIRED)) {
					secdebug("staticCode", "expired certificate(s); retrying validation");
					actionData.ActionFlags |= CSSM_TP_ACTION_ALLOW_EXPIRED;
					continue;		// retry validation
				}
				MacOSError::throwMe(result);
			}
		}
		return actionData.ActionFlags & CSSM_TP_ACTION_ALLOW_EXPIRED;
	}
}


//
// Return the TP policy used for signature verification.
// This policy object is cached and reused.
//
SecPolicyRef SecStaticCode::verificationPolicy()
{
	if (!mPolicy)
		MacOSError::check(SecPolicyCopy(CSSM_CERT_X_509v3,
			&CSSMOID_APPLE_TP_CODE_SIGNING, &mPolicy.aref()));
	return mPolicy;
}


//
// Validate a particular sealed, cached resource against its (special) CodeDirectory slot.
// The resource must already have been placed in the cache.
// This does NOT perform basic validation.
//
void SecStaticCode::validateComponent(CodeDirectory::SpecialSlot slot)
{
	CFDataRef data = mCache[slot];
	assert(data);		// must be cached
	if (data == CFDataRef(kCFNull)) {
		if (codeDirectory()->slotIsPresent(-slot)) // was supposed to be there...
				MacOSError::throwMe(errSecCSSignatureFailed);	// ... and is missing
	} else {
		if (!codeDirectory()->validateSlot(CFDataGetBytePtr(data), CFDataGetLength(data), -slot))
			MacOSError::throwMe(errSecCSSignatureFailed);
	}
}


//
// Perform static validation of the main executable.
// This reads the main executable from disk and validates it against the
// CodeDirectory code slot array.
// Note that this is NOT an in-memory validation, and is thus potentially
// subject to timing attacks.
//
void SecStaticCode::validateExecutable()
{
	secdebug("staticCode", "%p performing static main exec validate of %s",
		this, mainExecutablePath().c_str());
	const CodeDirectory *cd = this->codeDirectory();
	if (!cd)
		MacOSError::throwMe(errSecCSUnsigned);
	AutoFileDesc fd(mainExecutablePath(), O_RDONLY);
	fd.fcntl(F_NOCACHE, true);		// turn off page caching (one-pass)
	if (Universal *fat = mRep->mainExecutableImage())
		fd.seek(fat->archOffset());
	size_t pageSize = cd->pageSize ? (1 << cd->pageSize) : 0;
	size_t remaining = cd->codeLimit;
	for (size_t slot = 0; slot < cd->nCodeSlots; ++slot) {
		size_t size = min(remaining, pageSize);
		if (!cd->validateSlot(fd, size, slot)) {
			secdebug("staticCode", "%p failed static validation of code page %zd", this, slot);
			mExecutableValidated = true;	// we tried
			mExecutableValid = false;		// it failed
			MacOSError::throwMe(errSecCSSignatureFailed);
		}
		remaining -= size;
	}
	secdebug("staticCode", "%p validated full executable (%d pages)",
		this, int(cd->nCodeSlots));
	mExecutableValidated = true;	// we tried
	mExecutableValid = true;		// it worked
}


//
// Perform static validation of sealed resources.
//
// This performs a whole-code static resource scan and effectively
// computes a concordance between what's on disk and what's in the ResourceDirectory.
// Any unsanctioned difference causes an error.
//
void SecStaticCode::validateResources()
{
	// sanity first
	CFDictionaryRef sealedResources = resourceDictionary();
	if (this->resourceBase())		// disk has resources
		if (sealedResources)
			/* go to work below */;
		else
			MacOSError::throwMe(errSecCSResourcesNotFound);
	else							// disk has no resources
		if (sealedResources)
			MacOSError::throwMe(errSecCSResourcesNotFound);
		else
			return;					// no resources, not sealed - fine (no work)

	// found resources, and they are sealed
	CFDictionaryRef rules = cfget<CFDictionaryRef>(sealedResources, "rules");
	CFDictionaryRef files = cfget<CFDictionaryRef>(sealedResources, "files");
	secdebug("staticCode", "%p verifying %d sealed resources",
		this, int(CFDictionaryGetCount(files)));

	// make a shallow copy of the ResourceDirectory so we can "check off" what we find
	CFRef<CFMutableDictionaryRef> resourceMap = CFDictionaryCreateMutableCopy(NULL,
		CFDictionaryGetCount(files), files);
	if (!resourceMap)
		CFError::throwMe();

	// scan through the resources on disk, checking each against the resourceDirectory
	CollectingContext ctx(*this);		// collect all failures in here
	ResourceBuilder resources(cfString(this->resourceBase()), rules);
	mRep->adjustResources(resources);
	string path;
	ResourceBuilder::Rule *rule;

	while (resources.next(path, rule)) {
		if (CFDataRef value = resource(path, ctx))
			CFRelease(value);
		CFDictionaryRemoveValue(resourceMap, CFTempString(path));
		secdebug("staticCode", "%p validated %s", this, path.c_str());
	}
	
	if (CFDictionaryGetCount(resourceMap) > 0) {
		secdebug("staticCode", "%p sealed resource(s) not found in code", this);
		CFDictionaryApplyFunction(resourceMap, SecStaticCode::checkOptionalResource, &ctx);
	}
	
	// now check for any errors found in the reporting context
	if (ctx)
		ctx.throwMe();

	secdebug("staticCode", "%p sealed resources okay", this);
}


void SecStaticCode::checkOptionalResource(CFTypeRef key, CFTypeRef value, void *context)
{
	CollectingContext *ctx = static_cast<CollectingContext *>(context);
	ResourceSeal seal(value);
	if (!seal.optional())
		if (key && CFGetTypeID(key) == CFStringGetTypeID()) {
			ctx->reportProblem(errSecCSBadResource, kSecCFErrorResourceMissing,
				CFTempURL(CFStringRef(key), false, ctx->code.resourceBase()));
		} else
			ctx->reportProblem(errSecCSBadResource, kSecCFErrorResourceSeal, key);
}


//
// Load, validate, cache, and return CFDictionary forms of sealed resources.
//
CFDictionaryRef SecStaticCode::infoDictionary()
{
	if (!mInfoDict) {
		mInfoDict.take(getDictionary(cdInfoSlot));
		secdebug("staticCode", "%p loaded InfoDict %p", this, mInfoDict.get());
	}
	return mInfoDict;
}

CFDictionaryRef SecStaticCode::entitlements()
{
	if (!mEntitlements) {
		validateDirectory();
		if (CFDataRef entitlementData = component(cdEntitlementSlot)) {
			validateComponent(cdEntitlementSlot);
			const EntitlementBlob *blob = reinterpret_cast<const EntitlementBlob *>(CFDataGetBytePtr(entitlementData));
			if (blob->validateBlob()) {
				mEntitlements.take(blob->entitlements());
				secdebug("staticCode", "%p loaded Entitlements %p", this, mEntitlements.get());
			}
			// we do not consider a different blob type to be an error. We think it's a new format we don't understand
		}
	}
	return mEntitlements;
}

CFDictionaryRef SecStaticCode::resourceDictionary()
{
	if (mResourceDict)	// cached
		return mResourceDict;
	if (CFRef<CFDictionaryRef> dict = getDictionary(cdResourceDirSlot))
		if (cfscan(dict, "{rules=%Dn,files=%Dn}")) {
			secdebug("staticCode", "%p loaded ResourceDict %p",
				this, mResourceDict.get());
			return mResourceDict = dict;
		}
	// bad format
	return NULL;
}


//
// Load and cache the resource directory base.
// Note that the base is optional for each DiskRep.
//
CFURLRef SecStaticCode::resourceBase()
{
	if (!mGotResourceBase) {
		string base = mRep->resourcesRootPath();
		if (!base.empty())
			mResourceBase.take(makeCFURL(base, true));
		mGotResourceBase = true;
	}
	return mResourceBase;
}


//
// Load a component, validate it, convert it to a CFDictionary, and return that.
// This will force load and validation, which means that it will perform basic
// validation if it hasn't been done yet.
//
CFDictionaryRef SecStaticCode::getDictionary(CodeDirectory::SpecialSlot slot)
{
	validateDirectory();
	if (CFDataRef infoData = component(slot)) {
		validateComponent(slot);
		if (CFDictionaryRef dict = makeCFDictionaryFrom(infoData))
			return dict;
		else
			MacOSError::throwMe(errSecCSBadDictionaryFormat);
	}
	return NULL;
}


//
// Load, validate, and return a sealed resource.
// The resource data (loaded in to memory as a blob) is returned and becomes
// the responsibility of the caller; it is NOT cached by SecStaticCode.
//
// A resource that is not sealed will not be returned, and an error will be thrown.
// A missing resource will cause an error unless it's marked optional in the Directory.
// Under no circumstances will a corrupt resource be returned.
// NULL will only be returned for a resource that is neither sealed nor present
// (or that is sealed, absent, and marked optional).
// If the ResourceDictionary itself is not sealed, this function will always fail.
//
// There is currently no interface for partial retrieval of the resource data.
// (Since the ResourceDirectory does not currently support segmentation, all the
// data would have to be read anyway, but it could be read into a reusable buffer.)
//
CFDataRef SecStaticCode::resource(string path, ValidationContext &ctx)
{
	if (CFDictionaryRef rdict = resourceDictionary()) {
		if (CFTypeRef file = cfget(rdict, "files.%s", path.c_str())) {
			ResourceSeal seal = file;
			if (!resourceBase())	// no resources in DiskRep
				MacOSError::throwMe(errSecCSResourcesNotFound);
			CFRef<CFURLRef> fullpath = makeCFURL(path, false, resourceBase());
			if (CFRef<CFDataRef> data = cfLoadFile(fullpath)) {
				SHA1 hasher;
				hasher(CFDataGetBytePtr(data), CFDataGetLength(data));
				if (hasher.verify(seal.hash()))
					return data.yield();	// good
				else
					ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceAltered, fullpath); // altered
			} else {
				if (!seal.optional())
					ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceMissing, fullpath); // was sealed but is now missing
				else
					return NULL;	// validly missing
			}
		} else
			ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceAdded, CFTempURL(path, false, resourceBase()));
		return NULL;
	} else
		MacOSError::throwMe(errSecCSResourcesNotSealed);
}

CFDataRef SecStaticCode::resource(string path)
{
	ValidationContext ctx;
	return resource(path, ctx);
}


//
// Test a CodeDirectory flag.
// Returns false if there is no CodeDirectory.
// May throw if the CodeDirectory is present but somehow invalid.
//
bool SecStaticCode::flag(uint32_t tested)
{
	if (const CodeDirectory *cd = this->codeDirectory(false))
		return cd->flags & tested;
	else
		return false;
}


//
// Retrieve the full SuperBlob containing all internal requirements.
//
const Requirements *SecStaticCode::internalRequirements()
{
	if (CFDataRef req = component(cdRequirementsSlot))
		return (const Requirements *)CFDataGetBytePtr(req);
	else
		return NULL;
}


//
// Retrieve a particular internal requirement by type.
//
const Requirement *SecStaticCode::internalRequirement(SecRequirementType type)
{
	if (const Requirements *reqs = internalRequirements())
		return reqs->find<Requirement>(type);
	else
		return NULL;
}


//
// Return the Designated Requirement. This can be either explicit in the
// Internal Requirements resource, or implicitly generated.
//
const Requirement *SecStaticCode::designatedRequirement()
{
	if (const Requirement *req = internalRequirement(kSecDesignatedRequirementType)) {
		return req;		// explicit in signing data
	} else {
		if (!mDesignatedReq)
			mDesignatedReq = defaultDesignatedRequirement();
		return mDesignatedReq;
	}
}


//
// Generate the default (implicit) Designated Requirement for this StaticCode.
// This is a heuristic of sorts, and may change over time (for the better, we hope).
//
// The current logic is this:
// * If the code is ad-hoc signed, use the CodeDirectory hash directory.
// * Otherwise, use the form anchor (anchor) and identifier (CodeDirectory identifier).
// ** If the root CA is Apple's, we use the "anchor apple" construct. Otherwise,
//	  we default to the leaf (directly signing) certificate.
//
const Requirement *SecStaticCode::defaultDesignatedRequirement()
{
	validateDirectory();		// need the cert chain
	Requirement::Maker maker;
	
	// if this is an ad-hoc (unsigned) object, return a cdhash requirement
	if (flag(kSecCodeSignatureAdhoc)) {
		SHA1 hash;
		hash(codeDirectory(), codeDirectory()->length());
		SHA1::Digest digest;
		hash.finish(digest);
		maker.cdhash(digest);
	} else {
		// always require the identifier
		maker.put(opAnd);
		maker.ident(codeDirectory()->identifier());
		
		SHA1::Digest anchorHash;
		hashOfCertificate(cert(Requirement::anchorCert), anchorHash);
		if (!memcmp(anchorHash, Requirement::appleAnchorHash(), SHA1::digestLength)
#if	defined(TEST_APPLE_ANCHOR)
			|| !memcmp(anchorHash, Requirement::testAppleAnchorHash(), SHA1::digestLength)
#endif
			)
			defaultDesignatedAppleAnchor(maker);
		else
			defaultDesignatedNonAppleAnchor(maker);
	}
		
	return maker();
}

static const uint8_t adcSdkMarker[] = { APPLE_EXTENSION_OID, 2, 1 };
static const CSSM_DATA adcSdkMarkerOID = { sizeof(adcSdkMarker), (uint8_t *)adcSdkMarker };

void SecStaticCode::defaultDesignatedAppleAnchor(Requirement::Maker &maker)
{
	if (isAppleSDKSignature()) {
		// get the Common Name DN element for the leaf
		CFRef<CFStringRef> leafCN;
		MacOSError::check(SecCertificateCopySubjectComponent(cert(Requirement::leafCert),
			&CSSMOID_CommonName, &leafCN.aref()));
		
		// apple anchor generic and ...
		maker.put(opAnd);
		maker.anchorGeneric();			// apple generic anchor and...
		// ... leaf[subject.CN] = <leaf's subject> and ...
		maker.put(opAnd);
		maker.put(opCertField);			// certificate
		maker.put(0);					// leaf
		maker.put("subject.CN");		// [subject.CN]
		maker.put(matchEqual);			// =
		maker.putData(leafCN);			// <leaf CN>
		// ... cert 1[field.<marker>] exists
		maker.put(opCertGeneric);		// certificate
		maker.put(1);					// 1
		maker.putData(adcSdkMarkerOID.Data, adcSdkMarkerOID.Length); // [field.<marker>]
		maker.put(matchExists);			// exists
		return;
	}

	// otherwise, claim this program for Apple
	maker.anchor();
}

bool SecStaticCode::isAppleSDKSignature()
{
	if (CFArrayRef certChain = certificates())		// got cert chain
		if (CFArrayGetCount(certChain) == 3)		// leaf, one intermediate, anchor
			if (SecCertificateRef intermediate = cert(1)) // get intermediate
				if (certificateHasField(intermediate, CssmOid::overlay(adcSdkMarkerOID)))
					return true;
	return false;
}


void SecStaticCode::defaultDesignatedNonAppleAnchor(Requirement::Maker &maker)
{
	// get the Organization DN element for the leaf
	CFRef<CFStringRef> leafOrganization;
	MacOSError::check(SecCertificateCopySubjectComponent(cert(Requirement::leafCert),
		&CSSMOID_OrganizationName, &leafOrganization.aref()));

	// now step up the cert chain looking for the first cert with a different one
	int slot = Requirement::leafCert;						// start at leaf
	if (leafOrganization) {
		while (SecCertificateRef ca = cert(slot+1)) {		// NULL if you over-run the anchor slot
			CFRef<CFStringRef> caOrganization;
			MacOSError::check(SecCertificateCopySubjectComponent(ca, &CSSMOID_OrganizationName, &caOrganization.aref()));
			if (CFStringCompare(leafOrganization, caOrganization, 0) != kCFCompareEqualTo)
				break;
			slot++;
		}
		if (slot == CFArrayGetCount(mCertChain) - 1)		// went all the way to the anchor...
			slot = Requirement::anchorCert;					// ... so say that
	}
		
	// nail the last cert with the leaf's Organization value
	SHA1::Digest authorityHash;
	hashOfCertificate(cert(slot), authorityHash);
	maker.anchor(slot, authorityHash);
}


//
// Validate a SecStaticCode against the internal requirement of a particular type.
//
void SecStaticCode::validateRequirements(SecRequirementType type, SecStaticCode *target,
	OSStatus nullError /* = noErr */)
{
	secdebug("staticCode", "%p validating %s requirements for %p",
		this, Requirement::typeNames[type], target);
	if (const Requirement *req = internalRequirement(type))
		target->validateRequirements(req, nullError ? nullError : errSecCSReqFailed);
	else if (nullError) {
		secdebug("staticCode", "%p NULL validate for %s prohibited",
			this, Requirement::typeNames[type]);
		MacOSError::throwMe(nullError);
	} else
		secdebug("staticCode", "%p NULL validate (no requirements for %s)",
			this, Requirement::typeNames[type]);
}


//
// Validate this StaticCode against an external Requirement
//
void SecStaticCode::validateRequirements(const Requirement *req, OSStatus failure)
{
	assert(req);
	validateDirectory();
	req->validate(Requirement::Context(mCertChain, infoDictionary(), entitlements(), codeDirectory()), failure);
}


//
// Retrieve one certificate from the cert chain.
// Positive and negative indices can be used:
//    [ leaf, intermed-1, ..., intermed-n, anchor ]
//        0       1       ...     -2         -1
// Returns NULL if unavailable for any reason.
//	
SecCertificateRef SecStaticCode::cert(int ix)
{
	validateDirectory();		// need cert chain
	if (mCertChain) {
		CFIndex length = CFArrayGetCount(mCertChain);
		if (ix < 0)
			ix += length;
		if (ix >= 0 && ix < length)
			return SecCertificateRef(CFArrayGetValueAtIndex(mCertChain, ix));
	}
	return NULL;
}

CFArrayRef SecStaticCode::certificates()
{
	validateDirectory();		// need cert chain
	return mCertChain;
}


//
// Gather API-official information about this StaticCode.
//
// This method lives in the twilight between the API and internal layers,
// since it generates API objects (Sec*Refs) for return.
//
CFDictionaryRef SecStaticCode::signingInformation(SecCSFlags flags)
{
	//
	// Start with the pieces that we return even for unsigned code.
	// This makes Sec[Static]CodeRefs useful as API-level replacements
	// of our internal OSXCode objects.
	//
	CFRef<CFMutableDictionaryRef> dict = makeCFMutableDictionary(1,
		kSecCodeInfoMainExecutable, CFTempURL(this->mainExecutablePath()).get()
	);
	
	//
	// If we're not signed, this is all you get
	//
	if (!this->isSigned())
		return dict.yield();
	

	//
	// Now add the generic attributes that we always include
	//
	CFDictionaryAddValue(dict, kSecCodeInfoIdentifier, CFTempString(this->identifier()));
	CFDictionaryAddValue(dict, kSecCodeInfoFormat, CFTempString(this->format()));
	if (CFDictionaryRef info = infoDictionary())
		CFDictionaryAddValue(dict, kSecCodeInfoPList, info);

	//
	// kSecCSSigningInformation adds information about signing certificates and chains
	//
	if (flags & kSecCSSigningInformation) {
		if (CFArrayRef certs = certificates())
		CFDictionaryAddValue(dict, kSecCodeInfoCertificates, certs);
		if (CFDataRef sig = signature())
			CFDictionaryAddValue(dict, kSecCodeInfoCMS, sig);
		if (mTrust)
			CFDictionaryAddValue(dict, kSecCodeInfoTrust, mTrust);
		if (CFAbsoluteTime time = signingTime())
			if (CFRef<CFDateRef> date = CFDateCreate(NULL, time))
				CFDictionaryAddValue(dict, kSecCodeInfoTime, date);
	}
	
	//
	// kSecCSRequirementInformation adds information on requirements
	//
	if (flags & kSecCSRequirementInformation) {
		if (const Requirements *reqs = internalRequirements()) {
			CFDictionaryAddValue(dict, kSecCodeInfoRequirements,
				CFTempString(Dumper::dump(reqs)));
			CFDictionaryAddValue(dict, kSecCodeInfoRequirementData, CFTempData(*reqs));
		}
		const Requirement *dreq = designatedRequirement();
		const Requirement *ddreq = defaultDesignatedRequirement();
		CFRef<SecRequirementRef> ddreqRef = (new SecRequirement(ddreq))->handle();
		if (dreq == ddreq) {
			CFDictionaryAddValue(dict, kSecCodeInfoDesignatedRequirement, ddreqRef);
			CFDictionaryAddValue(dict, kSecCodeInfoImplicitDesignatedRequirement, ddreqRef);
		} else {
			CFDictionaryAddValue(dict, kSecCodeInfoDesignatedRequirement,
				CFRef<SecRequirementRef>((new SecRequirement(dreq))->handle()));
			CFDictionaryAddValue(dict, kSecCodeInfoImplicitDesignatedRequirement, ddreqRef);
		}
		
		if (CFDataRef ent = component(cdEntitlementSlot))
			CFDictionaryAddValue(dict, kSecCodeInfoEntitlements, ent);
	}
	
	//
	// kSecCSInternalInformation adds internal information meant to be for Apple internal
	// use (SPI), and not guaranteed to be stable. Primarily, this is data we want
	// to reliably transmit through the API wall so that code outside the Security.framework
	// can use it without having to play nasty tricks to get it.
	//
	if (flags & kSecCSInternalInformation) {
		if (mDir)
			CFDictionaryAddValue(dict, CFSTR("CodeDirectory"), mDir);
		CFDictionaryAddValue(dict, CFSTR("CodeOffset"), CFTempNumber(mRep->signingBase()));
		if (CFDictionaryRef resources = resourceDictionary())
			CFDictionaryAddValue(dict, CFSTR("ResourceDirectory"), resources);
	}
	
	
	//
	// kSecCSContentInformation adds more information about the physical layout
	// of the signed code. This is (only) useful for packaging or patching-oriented
	// applications.
	//
	if (flags & kSecCSContentInformation)
		if (CFRef<CFArrayRef> files = mRep->modifiedFiles())
			CFDictionaryAddValue(dict, kSecCodeInfoChangedFiles, files);
	
	return dict.yield();
}


//
// Resource validation contexts.
// The default context simply throws a CSError, rudely terminating the operation.
//
SecStaticCode::ValidationContext::~ValidationContext()
{ /* virtual */ }

void SecStaticCode::ValidationContext::reportProblem(OSStatus rc, CFStringRef type, CFTypeRef value)
{
	CSError::throwMe(rc, type, value);
}

void SecStaticCode::CollectingContext::reportProblem(OSStatus rc, CFStringRef type, CFTypeRef value)
{
	if (mStatus == noErr)
		mStatus = rc;			// record first failure for eventual error return
	if (type) {
		if (!mCollection)
			mCollection.take(makeCFMutableDictionary(0));
		CFMutableArrayRef element = CFMutableArrayRef(CFDictionaryGetValue(mCollection, type));
		if (!element) {
			element = makeCFMutableArray(0);
			if (!element)
				CFError::throwMe();
			CFDictionaryAddValue(mCollection, type, element);
			CFRelease(element);
		}
		CFArrayAppendValue(element, value);
	}
}

void SecStaticCode::CollectingContext::throwMe()
{
	assert(mStatus != noErr);
	throw CSError(mStatus, mCollection.yield());
}


//
// DetachedRep construction
//
DetachedRep::DetachedRep(CFDataRef sig, DiskRep *orig)
	: original(orig), mSignature(sig)
{
	const BlobCore *sigBlob = reinterpret_cast<const BlobCore *>(CFDataGetBytePtr(sig));
	if (sigBlob->is<EmbeddedSignatureBlob>()) {		// architecture-less
		mArch = EmbeddedSignatureBlob::specific(sigBlob);
		mGlobal = NULL;
		return;
	} else if (sigBlob->is<DetachedSignatureBlob>()) {	// architecture collection
		const DetachedSignatureBlob *dsblob = DetachedSignatureBlob::specific(sigBlob);
		if (Universal *fat = orig->mainExecutableImage()) {
			if (const BlobCore *blob = dsblob->find(fat->bestNativeArch().cpuType())) {
				mArch = EmbeddedSignatureBlob::specific(blob);
				mGlobal = EmbeddedSignatureBlob::specific(dsblob->find(0));
				return;
			} else
				secdebug("staticcode", "detached signature missing architecture %s",
					fat->bestNativeArch().name());
		} else
			secdebug("staticcode", "detached signature requires Mach-O binary");
	} else
		secdebug("staticcode", "detached signature bad magic 0x%x", sigBlob->magic());
	MacOSError::throwMe(errSecCSSignatureInvalid);
}

CFDataRef DetachedRep::component(CodeDirectory::SpecialSlot slot)
{
	if (CFDataRef result = mArch->component(slot))
		return result;
	if (mGlobal)
		if (CFDataRef result = mGlobal->component(slot))
			return result;
	return original->component(slot);
}


} // end namespace CodeSigning
} // end namespace Security
