/*
 * Copyright (c) 2006-2015 Apple Inc. All Rights Reserved.
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
#include "SecTask.h"
#include "Code.h"
#include "reqmaker.h"
#include "machorep.h"
#if TARGET_OS_OSX
#include "drmaker.h"
#include "notarization.h"
#endif
#include "reqdumper.h"
#include "reqparser.h"
#include "sigblob.h"
#include "resources.h"
#include "detachedrep.h"
#include "signerutils.h"
#if TARGET_OS_OSX
#include "csdatabase.h"
#endif
#include "dirscanner.h"
#include <CoreFoundation/CFURLAccess.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecCertificatePriv.h>
#if TARGET_OS_OSX
#include <Security/CMSPrivate.h>
#endif
#import <Security/SecCMS.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecCmsSignedData.h>
#if TARGET_OS_OSX
#include <Security/cssmapplePriv.h>
#endif
#include <security_utilities/unix++.h>
#include <security_utilities/cfmunge.h>
#include <security_utilities/casts.h>
#include <Security/CMSDecoder.h>
#include <security_utilities/logging.h>
#include <dirent.h>
#include <sys/xattr.h>
#include <sstream>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <dispatch/private.h>
#include <os/assumes.h>
#include <os/feature_private.h>
#include <os/variant_private.h>
#include <regex.h>
#import <utilities/entitlements.h>


namespace Security {
namespace CodeSigning {

using namespace UnixPlusPlus;

// A requirement representing a Mac or iOS dev cert, a Mac or iOS distribution cert, or a developer ID
static const char WWDRRequirement[] = "anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.2] exists";
static const char MACWWDRRequirement[] = "anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.12] exists";
static const char developerID[] = "anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] exists"
											" and certificate leaf[field.1.2.840.113635.100.6.1.13] exists";
static const char distributionCertificate[] =	"anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.7] exists";
static const char iPhoneDistributionCert[] =	"anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.4] exists";

//
// Map a component slot number to a suitable error code for a failure
//
static inline OSStatus errorForSlot(CodeDirectory::SpecialSlot slot)
{
	switch (slot) {
	case cdInfoSlot:
		return errSecCSInfoPlistFailed;
	case cdResourceDirSlot:
		return errSecCSResourceDirectoryFailed;
	default:
		return errSecCSSignatureFailed;
	}
}

/// Determines if the current process is marked as platform, cached with dispatch_once.
static bool isCurrentProcessPlatform(void)
{
	static dispatch_once_t sOnceToken;
	static bool sIsPlatform = false;

	dispatch_once(&sOnceToken, ^{
		SecTaskRef task = SecTaskCreateFromSelf(NULL);
		if (task) {
			uint32_t flags = SecTaskGetCodeSignStatus(task);
			if (flags & kSecCodeStatusPlatform) {
				sIsPlatform = true;
			}
			CFRelease(task);
		}
	});

	return sIsPlatform;
}

/// Determines if the item qualifies for a resource validity exemption based on its filesystem location.
static bool itemQualifiesForResourceExemption(string &item)
{
	if (isOnRootFilesystem(item.c_str())) {
		return true;
	}
	if (os_variant_allows_internal_security_policies("com.apple.security.codesigning")) {
		if (isPathPrefix("/AppleInternal/", item)) {
			return true;
		}
	}
	return false;
}

//
// Construct a SecStaticCode object given a disk representation object
//
SecStaticCode::SecStaticCode(DiskRep *rep, uint32_t flags)
	: mCheckfix30814861builder1(NULL),
	  mRep(rep),
	  mValidated(false), mExecutableValidated(false), mResourcesValidated(false), mResourcesValidContext(NULL),
	  mProgressQueue("com.apple.security.validation-progress", false, QOS_CLASS_UNSPECIFIED),
	  mOuterScope(NULL), mResourceScope(NULL),
	  mDesignatedReq(NULL), mGotResourceBase(false), mMonitor(NULL), mLimitedAsync(NULL),
	  mFlags(flags), mNotarizationChecked(false), mStaplingChecked(false), mNotarizationDate(NAN),
	  mNetworkEnabledByDefault(true), mTrustedSigningCertChain(false)

{
	CODESIGN_STATIC_CREATE(this, rep);
#if TARGET_OS_OSX
	checkForSystemSignature();

	// By default, platform code will no longer use the network.
	if (os_feature_enabled(Security, SecCodeOCSPDefault)) {
		if (isCurrentProcessPlatform()) {
			mNetworkEnabledByDefault = false;
		}
	}
	secinfo("staticCode", "SecStaticCode network default: %s", mNetworkEnabledByDefault ? "YES" : "NO");
#endif
}


//
// Clean up a SecStaticCode object
//
SecStaticCode::~SecStaticCode() _NOEXCEPT
try {
	::free(const_cast<Requirement *>(mDesignatedReq));
	delete mResourcesValidContext;
	delete mLimitedAsync;
	delete mCheckfix30814861builder1;
} catch (...) {
	return;
}

//
// Initialize a nested SecStaticCode object from its parent
//
void SecStaticCode::initializeFromParent(const SecStaticCode& parent) {
	mOuterScope = &parent;
	setMonitor(parent.monitor());
	if (parent.mLimitedAsync)
		mLimitedAsync = new LimitedAsync(*parent.mLimitedAsync);
}

//
// CF-level comparison of SecStaticCode objects compares CodeDirectory hashes if signed,
// and falls back on comparing canonical paths if (both are) not.
//
bool SecStaticCode::equal(SecCFObject &secOther)
{
	SecStaticCode *other = static_cast<SecStaticCode *>(&secOther);
	CFDataRef mine = this->cdHash();
	CFDataRef his = other->cdHash();
	if (mine || his)
		return mine && his && CFEqual(mine, his);
	else
		return CFEqual(CFRef<CFURLRef>(this->copyCanonicalPath()), CFRef<CFURLRef>(other->copyCanonicalPath()));
}

CFHashCode SecStaticCode::hash()
{
	if (CFDataRef h = this->cdHash())
		return CFHash(h);
	else
		return CFHash(CFRef<CFURLRef>(this->copyCanonicalPath()));
}


//
// Invoke a stage monitor if registered
//
CFTypeRef SecStaticCode::reportEvent(CFStringRef stage, CFDictionaryRef info)
{
	if (mMonitor)
		return mMonitor(this->handle(false), stage, info);
	else
		return NULL;
}

void SecStaticCode::prepareProgress(unsigned int workload)
{
	dispatch_sync(mProgressQueue, ^{
		mCancelPending = false;			// not canceled
	});
	if (mValidationFlags & kSecCSReportProgress) {
		mCurrentWork = 0;				// nothing done yet
		mTotalWork = workload;			// totally fake - we don't know how many files we'll get to chew
	}
}

void SecStaticCode::reportProgress(unsigned amount /* = 1 */)
{
	if (mMonitor && (mValidationFlags & kSecCSReportProgress)) {
		// update progress and report
		__block bool cancel = false;
		dispatch_sync(mProgressQueue, ^{
			if (mCancelPending)
				cancel = true;
			mCurrentWork += amount;
			mMonitor(this->handle(false), CFSTR("progress"), CFTemp<CFDictionaryRef>("{current=%d,total=%d}", mCurrentWork, mTotalWork));
		});
		// if cancellation is pending, abort now
		if (cancel)
			MacOSError::throwMe(errSecCSCancelled);
	}
}


//
// Set validation conditions for fine-tuning legacy tolerance
//
static void addError(CFTypeRef cfError, void* context)
{
	if (CFGetTypeID(cfError) == CFNumberGetTypeID()) {
		int64_t error;
		CFNumberGetValue(CFNumberRef(cfError), kCFNumberSInt64Type, (void*)&error);
		MacOSErrorSet* errors = (MacOSErrorSet*)context;
		errors->insert(OSStatus(error));
	}
}

void SecStaticCode::setValidationModifiers(CFDictionaryRef conditions)
{
	if (conditions) {
		CFDictionary source(conditions, errSecCSDbCorrupt);
		mAllowOmissions = source.get<CFArrayRef>("omissions");
		if (CFArrayRef errors = source.get<CFArrayRef>("errors"))
			CFArrayApplyFunction(errors, CFRangeMake(0, CFArrayGetCount(errors)), addError, &this->mTolerateErrors);
	}
}


//
// Request cancellation of a validation in progress.
// We do this by posting an abort flag that is checked periodically.
//
void SecStaticCode::cancelValidation()
{
	if (!(mValidationFlags & kSecCSReportProgress))		// not using progress reporting; cancel won't make it through
		MacOSError::throwMe(errSecCSInvalidFlags);
	dispatch_assert_queue(mProgressQueue);
	mCancelPending = true;
}


//
// Attach a detached signature.
//
void SecStaticCode::detachedSignature(CFDataRef sigData)
{
	if (sigData) {
		mDetachedSig = sigData;
		mRep = new DetachedRep(sigData, mRep->base(), "explicit detached");
		CODESIGN_STATIC_ATTACH_EXPLICIT(this, mRep);
	} else {
		mDetachedSig = NULL;
		mRep = mRep->base();
		CODESIGN_STATIC_ATTACH_EXPLICIT(this, NULL);
	}
}


//
// Consult the system detached signature database to see if it contains
// a detached signature for this StaticCode. If it does, fetch and attach it.
// We do this only if the code has no signature already attached.
//
void SecStaticCode::checkForSystemSignature()
{
#if TARGET_OS_OSX
	if (!this->isSigned()) {
		SignatureDatabase db;
		if (db.isOpen())
			try {
				if (RefPointer<DiskRep> dsig = db.findCode(mRep)) {
					CODESIGN_STATIC_ATTACH_SYSTEM(this, dsig);
					mRep = dsig;
				}
			} catch (...) {
			}
	}
#else
    MacOSError::throwMe(errSecUnimplemented);
#endif
}


//
// Return a descriptive string identifying the source of the code signature
//
string SecStaticCode::signatureSource()
{
	if (!isSigned())
		return "unsigned";
	if (DetachedRep *rep = dynamic_cast<DetachedRep *>(mRep.get()))
		return rep->source();
	return "embedded";
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
	CODESIGN_EVAL_STATIC_RESET(this);
	mValidated = false;
	mExecutableValidated = mResourcesValidated = false;
	if (mResourcesValidContext) {
		delete mResourcesValidContext;
		mResourcesValidContext = NULL;
	}
	mDir = NULL;
	mCodeDirectories.clear();
	mSignature = NULL;
	for (unsigned n = 0; n < cdSlotCount; n++)
		mCache[n] = NULL;
	mInfoDict = NULL;
	mEntitlements = NULL;
	mResourceDict = NULL;
	mDesignatedReq = NULL;
	mCDHash = NULL;
	mGotResourceBase = false;
	mTrust = NULL;
	mCertChain = NULL;
	mNotarizationChecked = false;
	mStaplingChecked = false;
	mNotarizationDate = NAN;
	mRep->flush();

#if TARGET_OS_OSX
	// we may just have updated the system database, so check again
	checkForSystemSignature();
#endif
}


//
// Retrieve a sealed component by special slot index.
// If the CodeDirectory has already been validated, validate against that.
// Otherwise, retrieve the component without validation (but cache it). Validation
// will go through the cache and validate all cached components.
//
CFDataRef SecStaticCode::component(CodeDirectory::SpecialSlot slot, OSStatus fail /* = errSecCSSignatureFailed */)
{
	assert(slot <= cdSlotMax);

	CFRef<CFDataRef> &cache = mCache[slot];
	if (!cache) {
		if (CFRef<CFDataRef> data = mRep->component(slot)) {
			if (validated()) { // if the directory has been validated...
				if (!codeDirectory()->slotIsPresent(-slot))
					return NULL;

				if (!codeDirectory()->validateSlot(CFDataGetBytePtr(data), // ... and it's no good
						CFDataGetLength(data), -slot, false))
					MacOSError::throwMe(errorForSlot(slot)); // ... then bail
			}
			cache = data;	// it's okay, cache it
		} else {	// absent, mark so
			if (validated())	// if directory has been validated...
				if (codeDirectory()->slotIsPresent(-slot)) // ... and the slot is NOT missing
					MacOSError::throwMe(errorForSlot(slot));	// was supposed to be there
			cache = CFDataRef(kCFNull);		// white lie
		}
	}
	return (cache == CFDataRef(kCFNull)) ? NULL : cache.get();
}


//
// Get the CodeDirectories.
// Throws (if check==true) or returns NULL (check==false) if there are none.
// Always throws if the CodeDirectories exist but are invalid.
// NEVER validates against the signature.
//
const SecStaticCode::CodeDirectoryMap *
SecStaticCode::codeDirectories(bool check /* = true */) const
{
	if (mCodeDirectories.empty()) {
		try {
			loadCodeDirectories(mCodeDirectories);
		} catch (...) {
			if (check)
				throw;
			// We wanted a NON-checked peek and failed to safely decode the existing CodeDirectories.
			// Pretend this is unsigned, but make sure we didn't somehow cache an invalid CodeDirectory.
			if (!mCodeDirectories.empty()) {
				assert(false);
				Syslog::warning("code signing internal problem: mCodeDirectories set despite exception exit");
				MacOSError::throwMe(errSecCSInternalError);
			}
		}
	} else {
		return &mCodeDirectories;
	}
	if (!mCodeDirectories.empty()) {
		return &mCodeDirectories;
	}
	if (check) {
		MacOSError::throwMe(errSecCSUnsigned);
	}
	return NULL;
}

//
// Get the CodeDirectory.
// Throws (if check==true) or returns NULL (check==false) if there is none.
// Always throws if the CodeDirectory exists but is invalid.
// NEVER validates against the signature.
//
const CodeDirectory *SecStaticCode::codeDirectory(bool check /* = true */) const
{
	if (!mDir) {
		// pick our favorite CodeDirectory from the choices we've got
		try {
			CodeDirectoryMap const *candidates = codeDirectories(check);
			if (candidates != NULL) {
				CodeDirectory::HashAlgorithm type = CodeDirectory::bestHashOf(mHashAlgorithms);
				mDir = candidates->at(type);	// and the winner is...
			}
		} catch (...) {
			if (check)
				throw;
			// We wanted a NON-checked peek and failed to safely decode the existing CodeDirectory.
			// Pretend this is unsigned, but make sure we didn't somehow cache an invalid CodeDirectory.
			if (mDir) {
				assert(false);
				Syslog::warning("code signing internal problem: mDir set despite exception exit");
				MacOSError::throwMe(errSecCSInternalError);
			}
		}
	}
	if (mDir)
		return reinterpret_cast<const CodeDirectory *>(CFDataGetBytePtr(mDir));
	if (check)
		MacOSError::throwMe(errSecCSUnsigned);
	return NULL;
}


//
// Fetch an array of all available CodeDirectories.
// Returns false if unsigned (no classic CD slot), true otherwise.
//
bool SecStaticCode::loadCodeDirectories(CodeDirectoryMap& cdMap) const
{
	__block CodeDirectoryMap candidates;
	__block CodeDirectory::HashAlgorithms hashAlgorithms;
	__block CFRef<CFDataRef> baseDir;
	auto add = ^bool (CodeDirectory::SpecialSlot slot){
		CFRef<CFDataRef> cdData = diskRep()->component(slot);
		if (!cdData)
			return false;
		const CodeDirectory* cd = reinterpret_cast<const CodeDirectory*>(CFDataGetBytePtr(cdData));
		if (!cd->validateBlob(CFDataGetLength(cdData)))
			MacOSError::throwMe(errSecCSSignatureFailed);	// no recovery - any suspect CD fails
		cd->checkIntegrity();
		auto result = candidates.insert(make_pair(cd->hashType, cdData.get()));
		if (!result.second)
			MacOSError::throwMe(errSecCSSignatureInvalid);	// duplicate hashType, go to heck
		hashAlgorithms.insert(cd->hashType);
		if (slot == cdCodeDirectorySlot)
			baseDir = cdData;
		return true;
	};
	if (!add(cdCodeDirectorySlot))
		return false;		// no classic slot CodeDirectory -> unsigned
	for (CodeDirectory::SpecialSlot slot = cdAlternateCodeDirectorySlots; slot < cdAlternateCodeDirectoryLimit; slot++)
		if (!add(slot))		// no CodeDirectory at this slot -> end of alternates
			break;
	if (candidates.empty())
		MacOSError::throwMe(errSecCSSignatureFailed);		// no viable CodeDirectory in sight
	// commit to cached values
	cdMap.swap(candidates);
	mHashAlgorithms.swap(hashAlgorithms);
	mBaseDir = baseDir;
	return true;
}


//
// Get the hash of the CodeDirectory.
// Returns NULL if there is none.
//
CFDataRef SecStaticCode::cdHash()
{
	if (!mCDHash) {
		if (const CodeDirectory *cd = codeDirectory(false)) {
			mCDHash.take(cd->cdhash());
			CODESIGN_STATIC_CDHASH(this, CFDataGetBytePtr(mCDHash), (unsigned int)CFDataGetLength(mCDHash));
		}
	}
	return mCDHash;
}
	
	
//
// Get an array of the cdhashes for all digest types in this signature
// The array is sorted by cd->hashType.
//
CFArrayRef SecStaticCode::cdHashes()
{
	if (!mCDHashes) {
		CFRef<CFMutableArrayRef> cdList = makeCFMutableArray(0);
		for (auto it = mCodeDirectories.begin(); it != mCodeDirectories.end(); ++it) {
			const CodeDirectory *cd = (const CodeDirectory *)CFDataGetBytePtr(it->second);
			if (CFRef<CFDataRef> hash = cd->cdhash())
				CFArrayAppendValue(cdList, hash);
		}
		mCDHashes = cdList.get();
	}
	return mCDHashes;
}

//
// Get a dictionary of untruncated cdhashes for all digest types in this signature.
//
CFDictionaryRef SecStaticCode::cdHashesFull()
{
	if (!mCDHashFullDict) {
		CFRef<CFMutableDictionaryRef> cdDict = makeCFMutableDictionary();
		for (auto const &it : mCodeDirectories) {
			CodeDirectory::HashAlgorithm alg = it.first;
			const CodeDirectory *cd = (const CodeDirectory *)CFDataGetBytePtr(it.second);
			CFRef<CFDataRef> hash = cd->cdhash(false);
			if (hash) {
				CFDictionaryAddValue(cdDict, CFTempNumber(alg), hash);
			}
		}
		mCDHashFullDict = cdDict.get();
	}
	return mCDHashFullDict;
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
	// track revocation separately, as it may not have been checked
	// during the initial validation
	if (!validated() || ((mValidationFlags & kSecCSEnforceRevocationChecks) && !revocationChecked()))
		try {
			// perform validation (or die trying)
			CODESIGN_EVAL_STATIC_DIRECTORY(this);
			mValidationExpired = verifySignature();
			if (mValidationFlags & kSecCSEnforceRevocationChecks)
				mRevocationChecked = true;

			for (CodeDirectory::SpecialSlot slot = codeDirectory()->maxSpecialSlot(); slot >= 1; --slot)
				if (mCache[slot])	// if we already loaded that resource...
					validateComponent(slot, errorForSlot(slot)); // ... then check it now
			mValidated = true;			// we've done the deed...
			mValidationResult = errSecSuccess;	// ... and it was good
		} catch (const CommonError &err) {
			mValidated = true;
			mValidationResult = err.osStatus();
			throw;
		} catch (...) {
			secinfo("staticCode", "%p validation threw non-common exception", this);
			mValidated = true;
			Syslog::notice("code signing internal problem: unknown exception thrown by validation");
			mValidationResult = errSecCSInternalError;
			throw;
		}
	assert(validated());
    // XXX: Embedded doesn't have CSSMERR_TP_CERT_EXPIRED so we can't throw it
    // XXX: This should be implemented for embedded once we implement
    // XXX: verifySignature and see how we're going to handle expired certs
#if TARGET_OS_OSX
	if (mValidationResult == errSecSuccess) {
		if (mValidationExpired)
			if ((mValidationFlags & kSecCSConsiderExpiration)
					|| (codeDirectory()->flags & kSecCodeSignatureForceExpiration))
				MacOSError::throwMe(CSSMERR_TP_CERT_EXPIRED);
	} else
		MacOSError::throwMe(mValidationResult);
#endif
}


//
// Load and validate the CodeDirectory and all components *except* those related to the resource envelope.
// Those latter components are checked by validateResources().
//
void SecStaticCode::validateNonResourceComponents()
{
	this->validateDirectory();
	for (CodeDirectory::SpecialSlot slot = codeDirectory()->maxSpecialSlot(); slot >= 1; --slot)
		switch (slot) {
		case cdResourceDirSlot:		// validated by validateResources
			break;
		default:
			this->component(slot);		// loads and validates
			break;
		}
}
	
	
//
// Check that any "top index" sealed into the signature conforms to what's actually here.
//
void SecStaticCode::validateTopDirectory()
{
	assert(mDir);		// must already have loaded CodeDirectories
	if (CFDataRef topDirectory = component(cdTopDirectorySlot)) {
		const auto topData = (const Endian<uint32_t> *)CFDataGetBytePtr(topDirectory);
		const auto topDataEnd = topData + CFDataGetLength(topDirectory) / sizeof(*topData);
		std::vector<uint32_t> signedVector(topData, topDataEnd);
		
		std::vector<uint32_t> foundVector;
		foundVector.push_back(cdCodeDirectorySlot);	// mandatory
		for (CodeDirectory::Slot slot = 1; slot <= cdSlotMax; ++slot)
			if (component(slot))
				foundVector.push_back(slot);
		int alternateCount = int(mCodeDirectories.size() - 1);		// one will go into cdCodeDirectorySlot
		for (int n = 0; n < alternateCount; n++)
			foundVector.push_back(cdAlternateCodeDirectorySlots + n);
		foundVector.push_back(cdSignatureSlot);		// mandatory (may be empty)
		
		if (signedVector != foundVector)
			MacOSError::throwMe(errSecCSSignatureFailed);
	}
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

CFAbsoluteTime SecStaticCode::signingTimestamp()
{
	validateDirectory();
	return mSigningTimestamp;
}

#if TARGET_OS_OSX
#define kSecSHA256HashSize 32
// subject:/C=US/ST=California/L=San Jose/O=Adobe Systems Incorporated/OU=Information Systems/OU=Digital ID Class 3 - Microsoft Software Validation v2/CN=Adobe Systems Incorporated
// issuer :/C=US/O=VeriSign, Inc./OU=VeriSign Trust Network/OU=Terms of use at https://www.verisign.com/rpa (c)10/CN=VeriSign Class 3 Code Signing 2010 CA
// Not Before: Dec 15 00:00:00 2010 GMT
// Not After : Dec 14 23:59:59 2012 GMT
static const unsigned char ASI_CS_12[] = {
	0x77,0x82,0x9C,0x64,0x33,0x45,0x2E,0x4A,0xD3,0xA8,0xE4,0x6F,0x00,0x6C,0x27,0xEA,
	0xFB,0xD3,0xF2,0x6D,0x50,0xF3,0x6F,0xE0,0xE9,0x6D,0x06,0x59,0x19,0xB5,0x46,0xFF
};

bool SecStaticCode::checkfix41082220(OSStatus cssmTrustResult)
{
	// only applicable to revoked results
	if (cssmTrustResult != CSSMERR_TP_CERT_REVOKED) {
		return false;
	}

	// only this leaf certificate
	if (CFArrayGetCount(mCertChain) == 0) {
		return false;
	}
	CFRef<CFDataRef> leafHash(SecCertificateCopySHA256Digest((SecCertificateRef)CFArrayGetValueAtIndex(mCertChain, 0)));
	if (memcmp(ASI_CS_12, CFDataGetBytePtr(leafHash), kSecSHA256HashSize) != 0) {
		return false;
	}

	// detached dmg signature
	if (!isDetached() || format() != std::string("disk image")) {
		return false;
	}

	// sha-1 signed
	if (hashAlgorithms().size() != 1 || hashAlgorithm() != kSecCodeSignatureHashSHA1) {
		return false;
	}

	// not a privileged binary - no TeamID and no entitlements
	if (component(cdEntitlementSlot) || teamID()) {
		return false;
	}

	// no flags and old version
	if (codeDirectory()->version != 0x20100 || codeDirectory()->flags != 0) {
		return false;
	}

	Security::Syslog::warning("CodeSigning: Check-fix enabled for dmg '%s' with identifier '%s' signed with revoked certificates",
							  mainExecutablePath().c_str(), identifier().c_str());
	return true;
}
#endif // TARGET_OS_OSX

//
// Verify the CMS signature.
// This performs the cryptographic tango. It returns if the signature is valid,
// or throws if it is not. As a side effect, a successful return sets up the
// cached certificate chain for future use.
// Returns true if the signature is expired (the X.509 sense), false if it's not.
// Expiration is fatal (throws) if a secure timestamp is included, but not otherwise.
//
bool SecStaticCode::verifySignature()
{
	// ad-hoc signed code is considered validly signed by definition
	if (flag(kSecCodeSignatureAdhoc)) {
		CODESIGN_EVAL_STATIC_SIGNATURE_ADHOC(this);
		return false;
	}

	DTRACK(CODESIGN_EVAL_STATIC_SIGNATURE, this, (char*)this->mainExecutablePath().c_str());
#if TARGET_OS_OSX
	if (!(mValidationFlags & kSecCSApplyEmbeddedPolicy)) {
		// decode CMS and extract SecTrust for verification
		CFRef<CMSDecoderRef> cms;
		MacOSError::check(CMSDecoderCreate(&cms.aref())); // create decoder
		CFDataRef sig = this->signature();
		MacOSError::check(CMSDecoderUpdateMessage(cms, CFDataGetBytePtr(sig), CFDataGetLength(sig)));
		this->codeDirectory();	// load CodeDirectory (sets mDir)
		MacOSError::check(CMSDecoderSetDetachedContent(cms, mBaseDir));
		MacOSError::check(CMSDecoderFinalizeMessage(cms));
		MacOSError::check(CMSDecoderSetSearchKeychain(cms, cfEmptyArray()));
		CFRef<CFArrayRef> vf_policies(createVerificationPolicies());
		CFRef<CFArrayRef> ts_policies(createTimeStampingAndRevocationPolicies());

		CMSSignerStatus status;
		MacOSError::check(CMSDecoderCopySignerStatus(cms, 0, vf_policies,
					false, &status, &mTrust.aref(), NULL));

		if (status != kCMSSignerValid) {
			const char *reason;
			switch (status) {
				case kCMSSignerUnsigned: reason="kCMSSignerUnsigned"; break;
				case kCMSSignerNeedsDetachedContent: reason="kCMSSignerNeedsDetachedContent"; break;
				case kCMSSignerInvalidSignature: reason="kCMSSignerInvalidSignature"; break;
				case kCMSSignerInvalidCert: reason="kCMSSignerInvalidCert"; break;
				case kCMSSignerInvalidIndex: reason="kCMSSignerInvalidIndex"; break;
				default: reason="unknown"; break;
			}
			Security::Syslog::error("CMSDecoderCopySignerStatus failed with %s error (%d)",
									reason, (int)status);
			MacOSError::throwMe(errSecCSSignatureFailed);
		}

		// retrieve auxiliary v1 data bag and verify against current state
		CFRef<CFDataRef> hashAgilityV1;
		switch (OSStatus rc = CMSDecoderCopySignerAppleCodesigningHashAgility(cms, 0, &hashAgilityV1.aref())) {
		case noErr:
			if (hashAgilityV1) {
				CFRef<CFDictionaryRef> hashDict = makeCFDictionaryFrom(hashAgilityV1);
				CFArrayRef cdList = CFArrayRef(CFDictionaryGetValue(hashDict, CFSTR("cdhashes")));
				CFArrayRef myCdList = this->cdHashes();

				/* Note that this is not very "agile": There's no way to calculate the exact
				 * list for comparison if it contains hash algorithms we don't know yet... */
				if (cdList == NULL || !CFEqual(cdList, myCdList))
					MacOSError::throwMe(errSecCSSignatureFailed);
			}
			break;
		case -1:	/* CMS used to return this for "no attribute found", so tolerate it. Now returning noErr/NULL */
			break;
		default:
			MacOSError::throwMe(rc);
		}

		// retrieve auxiliary v2 data bag and verify against current state
		CFRef<CFDictionaryRef> hashAgilityV2;
		switch (OSStatus rc = CMSDecoderCopySignerAppleCodesigningHashAgilityV2(cms, 0, &hashAgilityV2.aref())) {
			case noErr:
				if (hashAgilityV2) {
					/* Require number of code directoris and entries in the hash agility
					 * dict to be the same size (no stripping out code directories).
					 */
					if (CFDictionaryGetCount(hashAgilityV2) != mCodeDirectories.size()) {
						MacOSError::throwMe(errSecCSSignatureFailed);
					}

					/* Require every cdhash of every code directory whose hash
					 * algorithm we know to be in the agility dictionary.
					 *
					 * We check untruncated cdhashes here because we can.
					 */
					bool foundOurs = false;
					for (auto& entry : mCodeDirectories) {
						SECOidTag tag = CodeDirectorySet::SECOidTagForAlgorithm(entry.first);

						if (tag == SEC_OID_UNKNOWN) {
							// Unknown hash algorithm, ignore.
							continue;
						}

						CFRef<CFNumberRef> key = makeCFNumber(int(tag));
						CFRef<CFDataRef> entryCdhash;
						entryCdhash = (CFDataRef)CFDictionaryGetValue(hashAgilityV2, (void*)key.get());

						CodeDirectory const *cd = (CodeDirectory const*)CFDataGetBytePtr(entry.second);
						CFRef<CFDataRef> ourCdhash = cd->cdhash(false); // Untruncated cdhash!
						if (!CFEqual(entryCdhash, ourCdhash)) {
							MacOSError::throwMe(errSecCSSignatureFailed);
						}

						if (entry.first == this->hashAlgorithm()) {
							foundOurs = true;
						}
					}

					/* Require the cdhash of our chosen code directory to be in the dictionary.
					 * In theory, the dictionary could be full of unsupported cdhashes, but we
					 * really want ours, which is bound to be supported, to be covered.
					 */
					if (!foundOurs) {
						MacOSError::throwMe(errSecCSSignatureFailed);
					}
				}
				break;
			case -1:	/* CMS used to return this for "no attribute found", so tolerate it. Now returning noErr/NULL */
				break;
			default:
				MacOSError::throwMe(rc);
		}

		// internal signing time (as specified by the signer; optional)
		mSigningTime = 0;       // "not present" marker (nobody could code sign on Jan 1, 2001 :-)
		switch (OSStatus rc = CMSDecoderCopySignerSigningTime(cms, 0, &mSigningTime)) {
		case errSecSuccess:
		case errSecSigningTimeMissing:
			break;
		default:
			Security::Syslog::error("Could not get signing time (error %d)", (int)rc);
			MacOSError::throwMe(rc);
		}

		// certified signing time (as specified by a TSA; optional)
		mSigningTimestamp = 0;
		switch (OSStatus rc = CMSDecoderCopySignerTimestampWithPolicy(cms, ts_policies, 0, &mSigningTimestamp)) {
		case errSecSuccess:
		case errSecTimestampMissing:
			break;
		default:
			Security::Syslog::error("Could not get timestamp (error %d)", (int)rc);
			MacOSError::throwMe(rc);
		}

		// set up the environment for SecTrust
		if (validationCannotUseNetwork()) {
			MacOSError::check(SecTrustSetNetworkFetchAllowed(mTrust, false)); // no network?
		}
		MacOSError::check(SecTrustSetKeychainsAllowed(mTrust, false));

		CSSM_APPLE_TP_ACTION_DATA actionData = {
			CSSM_APPLE_TP_ACTION_VERSION,	// version of data structure
			0	// action flags
		};

		if (!(mValidationFlags & kSecCSCheckTrustedAnchors)) {
			/* no need to evaluate anchor trust when building cert chain */
			MacOSError::check(SecTrustSetAnchorCertificates(mTrust, cfEmptyArray())); // no anchors
			actionData.ActionFlags |= CSSM_TP_ACTION_IMPLICIT_ANCHORS;	// action flags
		}

		for (;;) {	// at most twice
			MacOSError::check(SecTrustSetParameters(mTrust,
				CSSM_TP_ACTION_DEFAULT, CFTempData(&actionData, sizeof(actionData))));

			// evaluate trust and extract results
			SecTrustResultType trustResult;
			MacOSError::check(SecTrustEvaluate(mTrust, &trustResult));
			mCertChain.take(copyCertChain(mTrust));

			// if this is an Apple developer cert....
			if (teamID() && SecStaticCode::isAppleDeveloperCert(mCertChain)) {
				CFRef<CFStringRef> teamIDFromCert;
				if (CFArrayGetCount(mCertChain) > 0) {
					SecCertificateRef leaf = (SecCertificateRef)CFArrayGetValueAtIndex(mCertChain, Requirement::leafCert);
					CFArrayRef organizationalUnits = SecCertificateCopyOrganizationalUnit(leaf);
					if (organizationalUnits) {
						teamIDFromCert.take((CFStringRef)CFRetain(CFArrayGetValueAtIndex(organizationalUnits, 0)));
						CFRelease(organizationalUnits);
					} else {
						teamIDFromCert = NULL;
					}

					if (teamIDFromCert) {
						CFRef<CFStringRef> teamIDFromCD = CFStringCreateWithCString(NULL, teamID(), kCFStringEncodingUTF8);
						if (!teamIDFromCD) {
							Security::Syslog::error("Could not get team identifier (%s)", teamID());
							MacOSError::throwMe(errSecCSInvalidTeamIdentifier);
						}

						if (CFStringCompare(teamIDFromCert, teamIDFromCD, 0) != kCFCompareEqualTo) {
							Security::Syslog::error("Team identifier in the signing certificate (%s) does not match the team identifier (%s) in the code directory",
													cfString(teamIDFromCert).c_str(), teamID());
							MacOSError::throwMe(errSecCSBadTeamIdentifier);
						}
					}
				}
			}

			CODESIGN_EVAL_STATIC_SIGNATURE_RESULT(this, trustResult, mCertChain ? (int)CFArrayGetCount(mCertChain) : 0);
			switch (trustResult) {
			case kSecTrustResultProceed:
			case kSecTrustResultUnspecified:
				break;				// success
			case kSecTrustResultDeny:
				MacOSError::throwMe(CSSMERR_APPLETP_TRUST_SETTING_DENY);	// user reject
			case kSecTrustResultInvalid:
				assert(false);		// should never happen
				MacOSError::throwMe(CSSMERR_TP_NOT_TRUSTED);
			default:
				{
					OSStatus result;
					MacOSError::check(SecTrustGetCssmResultCode(mTrust, &result));
					// if we have a valid timestamp, CMS validates against (that) signing time and all is well.
					// If we don't have one, may validate against *now*, and must be able to tolerate expiration.
					if (mSigningTimestamp == 0) { // no timestamp available
						if (((result == CSSMERR_TP_CERT_EXPIRED) || (result == CSSMERR_TP_CERT_NOT_VALID_YET))
								&& !(actionData.ActionFlags & CSSM_TP_ACTION_ALLOW_EXPIRED)) {
							CODESIGN_EVAL_STATIC_SIGNATURE_EXPIRED(this);
							actionData.ActionFlags |= CSSM_TP_ACTION_ALLOW_EXPIRED; // (this also allows postdated certs)
							continue;		// retry validation while tolerating expiration
						}
					}
					if (checkfix41082220(result)) {
						break; // success
					}
					Security::Syslog::error("SecStaticCode: verification failed (trust result %d, error %d)", trustResult, (int)result);
					MacOSError::throwMe(result);
				}
			}

			if (mSigningTimestamp) {
				CFIndex rootix = CFArrayGetCount(mCertChain);
				if (SecCertificateRef mainRoot = SecCertificateRef(CFArrayGetValueAtIndex(mCertChain, rootix-1)))
					if (isAppleCA(mainRoot)) {
						// impose policy: if the signature itself draws to Apple, then so must the timestamp signature
						CFRef<CFArrayRef> tsCerts;
						OSStatus result = CMSDecoderCopySignerTimestampCertificates(cms, 0, &tsCerts.aref());
						if (result) {
							Security::Syslog::error("SecStaticCode: could not get timestamp certificates (error %d)", (int)result);
							MacOSError::check(result);
						}
						CFIndex tsn = CFArrayGetCount(tsCerts);
						bool good = tsn > 0 && isAppleCA(SecCertificateRef(CFArrayGetValueAtIndex(tsCerts, tsn-1)));
						if (!good) {
							result = CSSMERR_TP_NOT_TRUSTED;
							Security::Syslog::error("SecStaticCode: timestamp policy verification failed (error %d)", (int)result);
							MacOSError::throwMe(result);
						}
					}
			}

			return actionData.ActionFlags & CSSM_TP_ACTION_ALLOW_EXPIRED;
		}

	} else
#endif
	{
		// Do some pre-verification initialization
		CFDataRef sig = this->signature();
		this->codeDirectory();	// load CodeDirectory (sets mDir)
		mSigningTime = 0;	// "not present" marker (nobody could code sign on Jan 1, 2001 :-)

		CFRef<CFDictionaryRef> attrs;
		CFRef<CFArrayRef> vf_policies(createVerificationPolicies());

		// Verify the CMS signature against mBaseDir (SHA1)
		MacOSError::check(SecCMSVerifyCopyDataAndAttributes(sig, mBaseDir, vf_policies, &mTrust.aref(), NULL, &attrs.aref()));

		// Copy the signing time
		mSigningTime = SecTrustGetVerifyTime(mTrust);

		// Validate the cert chain
		SecTrustResultType trustResult;
		MacOSError::check(SecTrustEvaluate(mTrust, &trustResult));

		// retrieve auxiliary data bag and verify against current state
		CFRef<CFDataRef> hashBag;
		hashBag = CFDataRef(CFDictionaryGetValue(attrs, kSecCMSHashAgility));
		if (hashBag) {
			CFRef<CFDictionaryRef> hashDict = makeCFDictionaryFrom(hashBag);
			CFArrayRef cdList = CFArrayRef(CFDictionaryGetValue(hashDict, CFSTR("cdhashes")));
			CFArrayRef myCdList = this->cdHashes();
			if (cdList == NULL || !CFEqual(cdList, myCdList))
				MacOSError::throwMe(errSecCSSignatureFailed);
		}

		/*
		 * Populate mCertChain with the certs.  If we failed validation, the
		 * signer's cert will be checked installed provisioning profiles as an
		 * alternative to verification against the policy for store-signed binaries
		 */
		mCertChain.take(copyCertChain(mTrust));

		// Did we implicitly trust the signer?
		mTrustedSigningCertChain = (trustResult == kSecTrustResultUnspecified || trustResult == kSecTrustResultProceed);

		return false; // XXX: Not checking for expired certs
	}
}

#if TARGET_OS_OSX
//
// Return the TP policy used for signature verification.
// This may be a simple SecPolicyRef or a CFArray of policies.
// The caller owns the return value.
//
static SecPolicyRef makeRevocationPolicy(CFOptionFlags flags)
{
	CFRef<SecPolicyRef> policy(SecPolicyCreateRevocation(flags));
	return policy.yield();
}
#endif

bool SecStaticCode::validationCannotUseNetwork()
{
	bool blockNetwork = false;
	bool validationEnablesNetwork = ((mValidationFlags & kSecCSAllowNetworkAccess) != 0);
	bool validationDisablesNetwork = ((mValidationFlags & kSecCSNoNetworkAccess) != 0);

	if (mNetworkEnabledByDefault) {
		// If network is enabled by default, block it only if the flags explicitly block.
		blockNetwork = validationDisablesNetwork;
	} else {
		// If network is disabled by default, block it if the flags don't explicitly enable it.
		blockNetwork = !validationEnablesNetwork;
	}
	secinfo("staticCode", "SecStaticCode network allowed: %s", blockNetwork ? "NO" : "YES");
	return blockNetwork;
}

CFArrayRef SecStaticCode::createVerificationPolicies()
{
	if (mValidationFlags & kSecCSUseSoftwareSigningCert) {
		CFRef<SecPolicyRef> ssRef = SecPolicyCreateAppleSoftwareSigning();
		return makeCFArray(1, ssRef.get());
	}
#if TARGET_OS_OSX
	if (mValidationFlags & kSecCSApplyEmbeddedPolicy) {
		CFRef<SecPolicyRef> iOSRef = SecPolicyCreateiPhoneApplicationSigning();
		return makeCFArray(1, iOSRef.get());
	}

	CFRef<SecPolicyRef> core;
	MacOSError::check(SecPolicyCopy(CSSM_CERT_X_509v3,
									&CSSMOID_APPLE_TP_CODE_SIGNING, &core.aref()));
	if (validationCannotUseNetwork()) {
		// Skips all revocation since they require network connectivity
		// therefore annihilates kSecCSEnforceRevocationChecks if present
		CFRef<SecPolicyRef> no_revoc = makeRevocationPolicy(kSecRevocationNetworkAccessDisabled);
		return makeCFArray(2, core.get(), no_revoc.get());
	}
	else if (mValidationFlags & kSecCSEnforceRevocationChecks) {
		// Add CRL and OCSP policies
		CFRef<SecPolicyRef> revoc = makeRevocationPolicy(kSecRevocationUseAnyAvailableMethod);
		return makeCFArray(2, core.get(), revoc.get());
	} else {
		return makeCFArray(1, core.get());
	}
#elif TARGET_OS_TV
	CFRef<SecPolicyRef> tvOSRef = SecPolicyCreateAppleTVOSApplicationSigning();
	return makeCFArray(1, tvOSRef.get());
#else
	CFRef<SecPolicyRef> iOSRef = SecPolicyCreateiPhoneApplicationSigning();
	return makeCFArray(1, iOSRef.get());
#endif

}

CFArrayRef SecStaticCode::createTimeStampingAndRevocationPolicies()
{
	CFRef<SecPolicyRef> tsPolicy = SecPolicyCreateAppleTimeStamping();
#if TARGET_OS_OSX
	if (validationCannotUseNetwork()) {
		// Skips all revocation since they require network connectivity
		// therefore annihilates kSecCSEnforceRevocationChecks if present
		CFRef<SecPolicyRef> no_revoc = makeRevocationPolicy(kSecRevocationNetworkAccessDisabled);
		return makeCFArray(2, tsPolicy.get(), no_revoc.get());
	}
	else if (mValidationFlags & kSecCSEnforceRevocationChecks) {
		// Add CRL and OCSP policies
		CFRef<SecPolicyRef> revoc = makeRevocationPolicy(kSecRevocationUseAnyAvailableMethod);
		return makeCFArray(2, tsPolicy.get(), revoc.get());
	}
	else {
		return makeCFArray(1, tsPolicy.get());
	}
#else
	return makeCFArray(1, tsPolicy.get());
#endif

}

CFArrayRef SecStaticCode::copyCertChain(SecTrustRef trust)
{
	SecCertificateRef leafCert = SecTrustGetCertificateAtIndex(trust, 0);
	if (leafCert != NULL) {
		CFIndex count = SecTrustGetCertificateCount(trust);

		CFMutableArrayRef certs = CFArrayCreateMutable(kCFAllocatorDefault, count,
													   &kCFTypeArrayCallBacks);

		CFArrayAppendValue(certs, leafCert);
		for (CFIndex i = 1; i < count; ++i) {
			CFArrayAppendValue(certs, SecTrustGetCertificateAtIndex(trust, i));
		}

		return certs;
	}
	return NULL;
}


//
// Validate a particular sealed, cached resource against its (special) CodeDirectory slot.
// The resource must already have been placed in the cache.
// This does NOT perform basic validation.
//
void SecStaticCode::validateComponent(CodeDirectory::SpecialSlot slot, OSStatus fail /* = errSecCSSignatureFailed */)
{
	assert(slot <= cdSlotMax);
	CFDataRef data = mCache[slot];
	assert(data);		// must be cached
	if (data == CFDataRef(kCFNull)) {
		if (codeDirectory()->slotIsPresent(-slot)) // was supposed to be there...
				MacOSError::throwMe(fail);	// ... and is missing
	} else {
		if (!codeDirectory()->validateSlot(CFDataGetBytePtr(data), CFDataGetLength(data), -slot, false))
			MacOSError::throwMe(fail);
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
	if (!validatedExecutable()) {
		try {
			DTRACK(CODESIGN_EVAL_STATIC_EXECUTABLE, this,
				(char*)this->mainExecutablePath().c_str(), codeDirectory()->nCodeSlots);
			const CodeDirectory *cd = this->codeDirectory();
			if (!cd)
				MacOSError::throwMe(errSecCSUnsigned);
			AutoFileDesc fd(mainExecutablePath(), O_RDONLY);
			fd.fcntl(F_NOCACHE, true);		// turn off page caching (one-pass)
			if (Universal *fat = mRep->mainExecutableImage())
				fd.seek(fat->archOffset());
			size_t pageSize = cd->pageSize ? (1 << cd->pageSize) : 0;
			size_t remaining = cd->signingLimit();
			for (uint32_t slot = 0; slot < cd->nCodeSlots; ++slot) {
				size_t thisPage = remaining;
				if (pageSize)
					thisPage = min(thisPage, pageSize);
				__block bool good = true;
				CodeDirectory::multipleHashFileData(fd, thisPage, hashAlgorithms(), ^(CodeDirectory::HashAlgorithm type, Security::DynamicHash *hasher) {
					const CodeDirectory* cd = (const CodeDirectory*)CFDataGetBytePtr(mCodeDirectories[type]);
					if (!hasher->verify(cd->getSlot(slot,
													mValidationFlags & kSecCSValidatePEH)))
						good = false;
				});
				if (!good) {
					CODESIGN_EVAL_STATIC_EXECUTABLE_FAIL(this, (int)slot);
					MacOSError::throwMe(errSecCSSignatureFailed);
				}
				remaining -= thisPage;
			}
			assert(remaining == 0);
			mExecutableValidated = true;
			mExecutableValidResult = errSecSuccess;
		} catch (const CommonError &err) {
			mExecutableValidated = true;
			mExecutableValidResult = err.osStatus();
			throw;
		} catch (...) {
			secinfo("staticCode", "%p executable validation threw non-common exception", this);
			mExecutableValidated = true;
			mExecutableValidResult = errSecCSInternalError;
			Syslog::notice("code signing internal problem: unknown exception thrown by validation");
			throw;
		}
	}
	assert(validatedExecutable());
	if (mExecutableValidResult != errSecSuccess)
		MacOSError::throwMe(mExecutableValidResult);
}

//
// Perform static validation of sealed resources and nested code.
//
// This performs a whole-code static resource scan and effectively
// computes a concordance between what's on disk and what's in the ResourceDirectory.
// Any unsanctioned difference causes an error.
//
unsigned SecStaticCode::estimateResourceWorkload()
{
	// workload estimate = number of sealed files
	CFDictionaryRef sealedResources = resourceDictionary();
	CFDictionaryRef files = cfget<CFDictionaryRef>(sealedResources, "files2");
	if (files == NULL)
		files = cfget<CFDictionaryRef>(sealedResources, "files");
	return files ? unsigned(CFDictionaryGetCount(files)) : 0;
}

void SecStaticCode::validateResources(SecCSFlags flags)
{
	// do we have a superset of this requested validation cached?
	bool doit = true;
	if (mResourcesValidated) {	// have cached outcome
		if (!(flags & kSecCSCheckNestedCode) || mResourcesDeep)	// was deep or need no deep scan
			doit = false;
	}

	if (doit) {
		string root = cfStringRelease(copyCanonicalPath());
		bool itemIsOnRootFS = itemQualifiesForResourceExemption(root);
		bool skipRootVolumeExceptions = (mValidationFlags & kSecCSSkipRootVolumeExceptions);
		bool useRootFSPolicy = itemIsOnRootFS && !skipRootVolumeExceptions;

		bool itemMightUseXattrFiles = pathFileSystemUsesXattrFiles(root.c_str());
		bool skipXattrFiles = itemMightUseXattrFiles && (mValidationFlags & kSecCSSkipXattrFiles);

		secinfo("staticCode", "performing resource validation for %s (%d, %d, %d, %d, %d)", root.c_str(),
				itemIsOnRootFS, skipRootVolumeExceptions, useRootFSPolicy, itemMightUseXattrFiles, skipXattrFiles);

		if (mLimitedAsync == NULL) {
			bool runMultiThreaded = ((flags & kSecCSSingleThreaded) == kSecCSSingleThreaded) ? false :
					(diskRep()->fd().mediumType() == kIOPropertyMediumTypeSolidStateKey);
			mLimitedAsync = new LimitedAsync(runMultiThreaded);
		}

		try {
			CFDictionaryRef rules;
			CFDictionaryRef files;
			uint32_t version;
			if (!loadResources(rules, files, version))
				return;		// validly no resources; nothing to do (ok)

			// found resources, and they are sealed
			DTRACK(CODESIGN_EVAL_STATIC_RESOURCES, this,
				(char*)this->mainExecutablePath().c_str(), 0);

			// scan through the resources on disk, checking each against the resourceDirectory
			mResourcesValidContext = new CollectingContext(*this);		// collect all failures in here

			// check for weak resource rules
			bool strict = flags & kSecCSStrictValidate;
			if (!useRootFSPolicy) {
				if (strict) {
					if (hasWeakResourceRules(rules, version, mAllowOmissions))
						if (mTolerateErrors.find(errSecCSWeakResourceRules) == mTolerateErrors.end())
							MacOSError::throwMe(errSecCSWeakResourceRules);
					if (version == 1)
						if (mTolerateErrors.find(errSecCSWeakResourceEnvelope) == mTolerateErrors.end())
							MacOSError::throwMe(errSecCSWeakResourceEnvelope);
				}
			}

			Dispatch::Group group;
			Dispatch::Group &groupRef = group;  // (into block)

			// scan through the resources on disk, checking each against the resourceDirectory
			__block CFRef<CFMutableDictionaryRef> resourceMap = makeCFMutableDictionary(files);
			string base = cfString(this->resourceBase());
			ResourceBuilder resources(base, base, rules, strict, mTolerateErrors);
			this->mResourceScope = &resources;
			diskRep()->adjustResources(resources);

			void (^unhandledScanner)(FTSENT *, uint32_t , const string, ResourceBuilder::Rule *) = nil;

			if (isFlagSet(flags, kSecCSEnforceRevocationChecks)) {
				unhandledScanner = ^(FTSENT *ent, uint32_t ruleFlags, const string relpath, ResourceBuilder::Rule *rule) {
					bool userControlledRule = ((ruleFlags & ResourceBuilder::user_controlled) == ResourceBuilder::user_controlled);
					secinfo("staticCode", "Visiting unhandled file: %d, %s", userControlledRule, relpath.c_str());
					if (!userControlledRule) {
						// No need to look at exemptions added by the runtime rule adjustments (ex. main executable).
						return;
					}

					CFRef<CFURLRef> validationURL;
					bool doValidation = false;
					switch (ent->fts_info) {
						case FTS_SL:
							char resolved[PATH_MAX];
							if (realpath(ent->fts_path, resolved)) {
								doValidation = true;
								validationURL.take(makeCFURL(resolved));
								secinfo("staticCode", "Checking symlink target: %s", resolved);
							} else {
								secerror("realpath failed checking symlink: %d", errno);
							}
							break;
						case FTS_F:
							doValidation = true;
							validationURL.take(makeCFURL(relpath, false, resourceBase()));
							break;
						default:
							// Unexpected type for the unhandled scanner.
							doValidation = false;
							secerror("Unexpected scan input: %d, %s", ent->fts_info, relpath.c_str());
							break;
					}

					if (doValidation) {
						// Here we yield our reference to hand over to the block's CFRef object, which will
						// hold it until the block is complete and also handle releasing in case of an exception.
						CFURLRef transferURL = validationURL.yield();

						void (^validate)() = ^{
							CFRef<CFURLRef> localURL = transferURL;
							AutoFileDesc fd(cfString(localURL), O_RDONLY, FileDesc::modeMissingOk);
							checkRevocationOnNestedBinary(fd, localURL, flags);
						};
						mLimitedAsync->perform(groupRef, validate);
					}
				};
			}

			void (^validationScanner)(FTSENT *, uint32_t , const string, ResourceBuilder::Rule *) = ^(FTSENT *ent, uint32_t ruleFlags, const string relpath, ResourceBuilder::Rule *rule) {
				CFDictionaryRemoveValue(resourceMap, CFTempString(relpath));
				bool isSymlink = (ent->fts_info == FTS_SL);

				void (^validate)() = ^{
					bool needsValidation = true;

					if (skipXattrFiles && pathIsValidXattrFile(cfString(resourceBase()) + "/" + relpath, "staticCode")) {
						secinfo("staticCode", "resource validation on xattr file skipped: %s", relpath.c_str());
						needsValidation = false;
					}

					if (useRootFSPolicy) {
						CFRef<CFURLRef> itemURL = makeCFURL(relpath, false, resourceBase());
						string itemPath = cfString(itemURL);
						if (itemQualifiesForResourceExemption(itemPath)) {
							secinfo("staticCode", "resource validation on root volume skipped: %s", itemPath.c_str());
							needsValidation = false;
						}
					}

					if (needsValidation) {
						secinfo("staticCode", "performing resource validation on item: %s", relpath.c_str());
						validateResource(files, relpath, isSymlink, *mResourcesValidContext, flags, version);
					}
					reportProgress();
				};

				mLimitedAsync->perform(groupRef, validate);
			};

			resources.scan(validationScanner, unhandledScanner);
			group.wait();	// wait until all async resources have been validated as well

			if (useRootFSPolicy) {
				// It's ok to allow leftovers on the root filesystem for now.
			} else {
				// Look through the leftovers and make sure they're all properly optional resources.
				unsigned leftovers = unsigned(CFDictionaryGetCount(resourceMap));
				if (leftovers > 0) {
					secinfo("staticCode", "%d sealed resource(s) not found in code", int(leftovers));
					CFDictionaryApplyFunction(resourceMap, SecStaticCode::checkOptionalResource, mResourcesValidContext);
				}
			}

			// now check for any errors found in the reporting context
			mResourcesValidated = true;
			mResourcesDeep = flags & kSecCSCheckNestedCode;
			if (mResourcesValidContext->osStatus() != errSecSuccess)
				mResourcesValidContext->throwMe();
		} catch (const CommonError &err) {
			mResourcesValidated = true;
			mResourcesDeep = flags & kSecCSCheckNestedCode;
			mResourcesValidResult = err.osStatus();
			throw;
		} catch (...) {
			secinfo("staticCode", "%p executable validation threw non-common exception", this);
			mResourcesValidated = true;
			mResourcesDeep = flags & kSecCSCheckNestedCode;
			mResourcesValidResult = errSecCSInternalError;
			Syslog::notice("code signing internal problem: unknown exception thrown by validation");
			throw;
		}
	}
	assert(validatedResources());
	if (mResourcesValidResult)
		MacOSError::throwMe(mResourcesValidResult);
	if (mResourcesValidContext->osStatus() != errSecSuccess)
		mResourcesValidContext->throwMe();
}


bool SecStaticCode::loadResources(CFDictionaryRef& rules, CFDictionaryRef& files, uint32_t& version)
{
	// sanity first
	CFDictionaryRef sealedResources = resourceDictionary();
	if (this->resourceBase()) {	// disk has resources
		if (sealedResources)
			/* go to work below */;
		else
			MacOSError::throwMe(errSecCSResourcesNotFound);
	} else {							// disk has no resources
		if (sealedResources)
			MacOSError::throwMe(errSecCSResourcesNotFound);
		else
			return false;					// no resources, not sealed - fine (no work)
	}
	
	// use V2 resource seal if available, otherwise fall back to V1
	if (CFDictionaryGetValue(sealedResources, CFSTR("files2"))) {	// have V2 signature
		rules = cfget<CFDictionaryRef>(sealedResources, "rules2");
		files = cfget<CFDictionaryRef>(sealedResources, "files2");
		version = 2;
	} else {	// only V1 available
		rules = cfget<CFDictionaryRef>(sealedResources, "rules");
		files = cfget<CFDictionaryRef>(sealedResources, "files");
		version = 1;
	}
	if (!rules || !files)
		MacOSError::throwMe(errSecCSResourcesInvalid);
	return true;
}
	

void SecStaticCode::checkOptionalResource(CFTypeRef key, CFTypeRef value, void *context)
{
	ValidationContext *ctx = static_cast<ValidationContext *>(context);
	ResourceSeal seal(value);
	if (!seal.optional()) {
		if (key && CFGetTypeID(key) == CFStringGetTypeID()) {
			CFTempURL tempURL(CFStringRef(key), false, ctx->code.resourceBase());
			if (!tempURL.get()) {
				ctx->reportProblem(errSecCSBadDictionaryFormat, kSecCFErrorResourceSeal, key);
			} else {
				ctx->reportProblem(errSecCSBadResource, kSecCFErrorResourceMissing, tempURL);
			}
		} else {
			ctx->reportProblem(errSecCSBadResource, kSecCFErrorResourceSeal, key);
		}
	}
}


static bool isOmitRule(CFTypeRef value)
{
	if (CFGetTypeID(value) == CFBooleanGetTypeID())
		return value == kCFBooleanFalse;
	CFDictionary rule(value, errSecCSResourceRulesInvalid);
	return rule.get<CFBooleanRef>("omit") == kCFBooleanTrue;
}

bool SecStaticCode::hasWeakResourceRules(CFDictionaryRef rulesDict, uint32_t version, CFArrayRef allowedOmissions)
{
	// compute allowed omissions
	CFRef<CFArrayRef> defaultOmissions = this->diskRep()->allowedResourceOmissions();
	if (!defaultOmissions) {
		Syslog::notice("code signing internal problem: diskRep returned no allowedResourceOmissions");
		MacOSError::throwMe(errSecCSInternalError);
	}
	CFRef<CFMutableArrayRef> allowed = CFArrayCreateMutableCopy(NULL, 0, defaultOmissions);
	if (allowedOmissions)
		CFArrayAppendArray(allowed, allowedOmissions, CFRangeMake(0, CFArrayGetCount(allowedOmissions)));
	CFRange range = CFRangeMake(0, CFArrayGetCount(allowed));

	// check all resource rules for weakness
	string catchAllRule = (version == 1) ? "^Resources/" : "^.*";
	__block bool coversAll = false;
	__block bool forbiddenOmission = false;
	CFArrayRef allowedRef = allowed.get();	// (into block)
	CFDictionary rules(rulesDict, errSecCSResourceRulesInvalid);
	rules.apply(^(CFStringRef key, CFTypeRef value) {
		string pattern = cfString(key, errSecCSResourceRulesInvalid);
		if (pattern == catchAllRule && value == kCFBooleanTrue) {
			coversAll = true;
			return;
		}
		if (isOmitRule(value))
			forbiddenOmission |= !CFArrayContainsValue(allowedRef, range, key);
	});

	return !coversAll || forbiddenOmission;
}


//
// Load, validate, cache, and return CFDictionary forms of sealed resources.
//
CFDictionaryRef SecStaticCode::infoDictionary()
{
	if (!mInfoDict) {
		mInfoDict.take(getDictionary(cdInfoSlot, errSecCSInfoPlistFailed));
		secinfo("staticCode", "%p loaded InfoDict %p", this, mInfoDict.get());
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
				secinfo("staticCode", "%p loaded Entitlements %p", this, mEntitlements.get());
			}
			// we do not consider a different blob type to be an error. We think it's a new format we don't understand
		}
	}
	return mEntitlements;
}

CFDictionaryRef SecStaticCode::resourceDictionary(bool check /* = true */)
{
	if (mResourceDict)	// cached
		return mResourceDict;
	if (CFRef<CFDictionaryRef> dict = getDictionary(cdResourceDirSlot, check))
		if (cfscan(dict, "{rules=%Dn,files=%Dn}")) {
			secinfo("staticCode", "%p loaded ResourceDict %p",
				this, mResourceDict.get());
			return mResourceDict = dict;
		}
	// bad format
	return NULL;
}
	
	
CFDataRef SecStaticCode::copyComponent(CodeDirectory::SpecialSlot slot, CFDataRef hash)
{
	const CodeDirectory* cd = this->codeDirectory();
	if (CFCopyRef<CFDataRef> component = this->component(slot)) {
		if (hash) {
			const void *slotHash = cd->getSlot(slot, false);
			if (cd->hashSize != CFDataGetLength(hash) || 0 != memcmp(slotHash, CFDataGetBytePtr(hash), cd->hashSize)) {
				Syslog::notice("copyComponent hash mismatch slot %d length %d", slot, int(CFDataGetLength(hash)));
				return NULL;	// mismatch
			}
		}
		return component.yield();
	}
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
CFDictionaryRef SecStaticCode::getDictionary(CodeDirectory::SpecialSlot slot, bool check /* = true */)
{
	if (check)
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
//
//
CFDictionaryRef SecStaticCode::copyDiskRepInformation()
{
	return mRep->copyDiskRepInformation();
}

bool SecStaticCode::checkfix30814861(string path, bool addition) {
	// <rdar://problem/30814861> v2 resource rules don't match v1 resource rules

	//// Condition 1: Is the app an iOS app that was built with an SDK lower than 9.0?

	// We started signing correctly in 2014, 9.0 was first seeded mid-2016.

	CFRef<CFDictionaryRef> inf = copyDiskRepInformation();
	try {
		CFDictionary info(inf.get(), errSecCSNotSupported);
		uint32_t platform =
			cfNumber(info.get<CFNumberRef>(kSecCodeInfoDiskRepVersionPlatform, errSecCSNotSupported), 0);
		uint32_t sdkVersion =
			cfNumber(info.get<CFNumberRef>(kSecCodeInfoDiskRepVersionSDK, errSecCSNotSupported), 0);

		if (platform != PLATFORM_IOS || sdkVersion >= 0x00090000) {
			return false;
		}
	} catch (const MacOSError &error) {
		return false;
	}

	//// Condition 2: Is it a .sinf/.supf/.supp file at the right location?

	static regex_t pathre_sinf;
	static regex_t pathre_supp_supf;
	static dispatch_once_t once;

	dispatch_once(&once, ^{
		os_assert_zero(regcomp(&pathre_sinf,
							   "^(Frameworks/[^/]+\\.framework/|PlugIns/[^/]+\\.appex/|())SC_Info/[^/]+\\.sinf$",
							   REG_EXTENDED | REG_NOSUB));
		os_assert_zero(regcomp(&pathre_supp_supf,
							   "^(Frameworks/[^/]+\\.framework/|PlugIns/[^/]+\\.appex/|())SC_Info/[^/]+\\.(supf|supp)$",
							   REG_EXTENDED | REG_NOSUB));
	});

	// .sinf is added, .supf/.supp are modified.
	const regex_t &pathre = addition ? pathre_sinf : pathre_supp_supf;

	const int result = regexec(&pathre, path.c_str(), 0, NULL, 0);

	if (result == REG_NOMATCH) {
		return false;
	} else if (result != 0) {
		// Huh?
		secerror("unexpected regexec result %d for path '%s'", result, path.c_str());
		return false;
	}

	//// Condition 3: Do the v1 rules actually exclude the file?

	dispatch_once(&mCheckfix30814861builder1_once, ^{
		// Create the v1 resource builder lazily.
		CFDictionaryRef rules1 = cfget<CFDictionaryRef>(resourceDictionary(), "rules");
		const string base = cfString(resourceBase());

		mCheckfix30814861builder1 = new ResourceBuilder(base, base, rules1, false, mTolerateErrors);
	});

	ResourceBuilder::Rule const * const matchingRule = mCheckfix30814861builder1->findRule(path);

	if (matchingRule == NULL || !(matchingRule->flags & ResourceBuilder::omitted)) {
		return false;
	}

	//// All matched, this file is a check-fixed sinf/supf/supp.

	return true;

}

void SecStaticCode::validateResource(CFDictionaryRef files, string path, bool isSymlink, ValidationContext &ctx, SecCSFlags flags, uint32_t version)
{
	if (!resourceBase())	// no resources in DiskRep
		MacOSError::throwMe(errSecCSResourcesNotFound);
	CFRef<CFURLRef> fullpath = makeCFURL(path, false, resourceBase());
	if (version > 1 && ((flags & (kSecCSStrictValidate|kSecCSRestrictSidebandData)) == (kSecCSStrictValidate|kSecCSRestrictSidebandData))) {
		AutoFileDesc fd(cfString(fullpath));
		if (fd.hasExtendedAttribute(XATTR_RESOURCEFORK_NAME) || fd.hasExtendedAttribute(XATTR_FINDERINFO_NAME))
			ctx.reportProblem(errSecCSInvalidAssociatedFileData, kSecCFErrorResourceSideband, fullpath);
	}
	if (CFTypeRef file = CFDictionaryGetValue(files, CFTempString(path))) {
		ResourceSeal seal(file);
		const ResourceSeal& rseal = seal;
		if (seal.nested()) {
			if (isSymlink) {
				return ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceAltered, fullpath); // changed type
			}
			string suffix = ".framework";
			bool isFramework = (path.length() > suffix.length()) &&
							   (path.compare(path.length()-suffix.length(), suffix.length(), suffix) == 0);
			validateNestedCode(fullpath, seal, flags, isFramework);
		} else if (seal.link()) {
			if (!isSymlink) {
				return ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceAltered, fullpath); // changed type
			}
			validateSymlinkResource(cfString(fullpath), cfString(seal.link()), ctx, flags);
		} else if (seal.hash(hashAlgorithm())) {	// genuine file
			if (isSymlink) {
				return ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceAltered, fullpath); // changed type
			}
			AutoFileDesc fd(cfString(fullpath), O_RDONLY, FileDesc::modeMissingOk);	// open optional file
			if (fd) {
				__block bool good = true;
				CodeDirectory::multipleHashFileData(fd, 0, hashAlgorithms(), ^(CodeDirectory::HashAlgorithm type, Security::DynamicHash *hasher) {
					if (!hasher->verify(rseal.hash(type)))
						good = false;
				});
				if (!good) {
					if (version == 2 && checkfix30814861(path, false)) {
						secinfo("validateResource", "%s check-fixed (altered).", path.c_str());
					} else {
						ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceAltered, fullpath); // altered
					}
				}

				if (good && isFlagSet(flags, kSecCSEnforceRevocationChecks)) {
					checkRevocationOnNestedBinary(fd, fullpath, flags);
				}
			} else {
				if (!seal.optional()) {
					ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceMissing, fullpath); // was sealed but is now missing
				} else {
					return;			// validly missing
				}
			}
		} else {
			ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceAltered, fullpath); // changed type
		}
		return;
	}
	if (version == 1) {		// version 1 ignores symlinks altogether
		char target[PATH_MAX];
		if (::readlink(cfString(fullpath).c_str(), target, sizeof(target)) > 0)
			return;
	}
	if (version == 2 && checkfix30814861(path, true)) {
		secinfo("validateResource", "%s check-fixed (added).", path.c_str());
	} else {
		ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceAdded, CFTempURL(path, false, resourceBase()));
	}
}

void SecStaticCode::validatePlainMemoryResource(string path, CFDataRef fileData, SecCSFlags flags)
{
	CFDictionaryRef rules;
	CFDictionaryRef files;
	uint32_t version;
	if (!loadResources(rules, files, version))
		MacOSError::throwMe(errSecCSResourcesNotFound);		// no resources sealed; this can't be right
	if (CFTypeRef file = CFDictionaryGetValue(files, CFTempString(path))) {
		ResourceSeal seal(file);
		const Byte *sealHash = seal.hash(hashAlgorithm());
		if (sealHash) {
			if (codeDirectory()->verifyMemoryContent(fileData, sealHash))
				return;		// success
		}
	}
	MacOSError::throwMe(errSecCSBadResource);
}
	
void SecStaticCode::validateSymlinkResource(std::string fullpath, std::string seal, ValidationContext &ctx, SecCSFlags flags)
{
	static const char* const allowedDestinations[] = {
		"/System/",
		"/Library/",
		NULL
	};
	char target[PATH_MAX];
	ssize_t len = ::readlink(fullpath.c_str(), target, sizeof(target)-1);
	if (len < 0)
		UnixError::check(-1);
	target[len] = '\0';
	std::string fulltarget = target;
	if (target[0] != '/') {
		size_t lastSlash = fullpath.rfind('/');
		fulltarget = fullpath.substr(0, lastSlash) + '/' + target;
	}
	if (seal != target) {
		ctx.reportProblem(errSecCSBadResource, kSecCFErrorResourceAltered, CFTempString(fullpath));
		return;
	}
	if ((mValidationFlags & (kSecCSStrictValidate|kSecCSRestrictSymlinks)) == (kSecCSStrictValidate|kSecCSRestrictSymlinks)) {
		char resolved[PATH_MAX];
		if (realpath(fulltarget.c_str(), resolved)) {
			assert(resolved[0] == '/');
			size_t rlen = strlen(resolved);
			if (target[0] == '/') {
				// absolute symlink; only allow absolute links to system locations
				for (const char* const* pathp = allowedDestinations; *pathp; pathp++) {
					size_t dlen = strlen(*pathp);
					if (rlen > dlen && strncmp(resolved, *pathp, dlen) == 0)
					return;		// target inside /System, deemed okay
				}
			} else {
				// everything else must be inside the bundle(s)
				for (const SecStaticCode* code = this; code; code = code->mOuterScope) {
					string root = code->mResourceScope->root();
					if (strncmp(resolved, root.c_str(), root.size()) == 0) {
						if (code->mResourceScope->includes(resolved + root.length() + 1))
							return;		// located in resource stack && included in envelope
						else
							break;		// located but excluded from envelope (deny)
					}
				}
			}
		}
		// if we fell through, flag a symlink error
		if (mTolerateErrors.find(errSecCSInvalidSymlink) == mTolerateErrors.end())
			ctx.reportProblem(errSecCSInvalidSymlink, kSecCFErrorResourceAltered, CFTempString(fullpath));
	}
}

/// Uses the provided file descriptor to check if the file is macho, and if so validates the file at the url as a binary to check for a revoked certificate.
void SecStaticCode::checkRevocationOnNestedBinary(UnixPlusPlus::FileDesc &fd, CFURLRef url, SecCSFlags flags)
{
#if TARGET_OS_OSX
	secinfo("staticCode", "validating embedded resource: %@", url);

	try {
		SecPointer<SecStaticCode> code;

		if (MachORep::candidate(fd)) {
			DiskRep *rep = new MachORep(cfString(url).c_str(), NULL);
			if (rep) {
				code = new SecStaticCode(rep);
			}
		}

		if (code) {
			code->initializeFromParent(*this);
			code->setValidationFlags(flags);
			// Validate just the code directory, which performs signature validation.
			code->validateDirectory();
			secinfo("staticCode", "successfully validated nested resource binary: %@", url);
		}
	} catch (const MacOSError &err) {
		if (err.error == CSSMERR_TP_CERT_REVOKED) {
			secerror("Rejecting binary with revoked certificate: %@", url);
			throw;
		} else {
			// Any other errors, but only revocation checks are fatal so just continue.
			secinfo("staticCode", "Found unexpected error other error validating resource binary: %d, %@", err.error, url);
		}
	}
#else
	// This type of resource checking doesn't make sense on embedded devices right now, so just do nothing.
	return;
#endif // TARGET_OS_OSX
}

void SecStaticCode::validateNestedCode(CFURLRef path, const ResourceSeal &seal, SecCSFlags flags, bool isFramework)
{
	CFRef<SecRequirementRef> req;
	if (SecRequirementCreateWithString(seal.requirement(), kSecCSDefaultFlags, &req.aref()))
		MacOSError::throwMe(errSecCSResourcesInvalid);

	// recursively verify this nested code
	try {
		if (!(flags & kSecCSCheckNestedCode)) {
			flags |= kSecCSBasicValidateOnly | kSecCSQuickCheck;
		}
		SecPointer<SecStaticCode> code = new SecStaticCode(DiskRep::bestGuess(cfString(path)));
		code->initializeFromParent(*this);
		code->staticValidate(flags & (~kSecCSRestrictToAppLike), SecRequirement::required(req));

		if (isFramework && (flags & kSecCSStrictValidate))
			try {
				validateOtherVersions(path, flags & (~kSecCSRestrictToAppLike), req, code);
			} catch (const CSError &err) {
				MacOSError::throwMe(errSecCSBadFrameworkVersion);
			} catch (const MacOSError &err) {
				MacOSError::throwMe(errSecCSBadFrameworkVersion);
			}

	} catch (CSError &err) {
		if (err.error == errSecCSReqFailed) {
			mResourcesValidContext->reportProblem(errSecCSBadNestedCode, kSecCFErrorResourceAltered, path);
			return;
		}
		err.augment(kSecCFErrorPath, path);
		throw;
	} catch (const MacOSError &err) {
		if (err.error == errSecCSReqFailed) {
			mResourcesValidContext->reportProblem(errSecCSBadNestedCode, kSecCFErrorResourceAltered, path);
			return;
		}
		CSError::throwMe(err.error, kSecCFErrorPath, path);
	}
}

void SecStaticCode::validateOtherVersions(CFURLRef path, SecCSFlags flags, SecRequirementRef req, SecStaticCode *code)
{
	// Find out what current points to and do not revalidate
	std::string mainPath = cfStringRelease(code->diskRep()->copyCanonicalPath());

	char main_path[PATH_MAX];
	bool foundTarget = false;

	/* If it failed to get the target of the symlink, do not fail. It is a performance loss,
	 not a security hole */
	if (realpath(mainPath.c_str(), main_path) != NULL)
		foundTarget = true;

	std::ostringstream versionsPath;
	versionsPath << cfString(path) << "/Versions/";

	DirScanner scanner(versionsPath.str());

	if (scanner.initialized()) {
		struct dirent *entry = NULL;
		while ((entry = scanner.getNext()) != NULL) {
			std::ostringstream fullPath;

			if (entry->d_type != DT_DIR || strcmp(entry->d_name, "Current") == 0)
				continue;

			fullPath << versionsPath.str() << entry->d_name;

			char real_full_path[PATH_MAX];
			if (realpath(fullPath.str().c_str(), real_full_path) == NULL)
				UnixError::check(-1);

			// Do case insensitive comparions because realpath() was called for both paths
			if (foundTarget && strcmp(main_path, real_full_path) == 0)
				continue;

			SecPointer<SecStaticCode> frameworkVersion = new SecStaticCode(DiskRep::bestGuess(real_full_path));
			frameworkVersion->initializeFromParent(*this);
			frameworkVersion->staticValidate(flags, SecRequirement::required(req));
		}
	}
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
	if (CFDataRef reqData = component(cdRequirementsSlot)) {
		const Requirements *req = (const Requirements *)CFDataGetBytePtr(reqData);
		if (!req->validateBlob())
			MacOSError::throwMe(errSecCSReqInvalid);
		return req;
	} else
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
// Return the Designated Requirement (DR). This can be either explicit in the
// Internal Requirements component, or implicitly generated on demand here.
// Note that an explicit DR may have been implicitly generated at signing time;
// we don't distinguish this case.
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
// Generate the default Designated Requirement (DR) for this StaticCode.
// Ignore any explicit DR it may contain.
//
const Requirement *SecStaticCode::defaultDesignatedRequirement()
{
	if (flag(kSecCodeSignatureAdhoc)) {
		// adhoc signature: return a cdhash requirement for all architectures
		__block Requirement::Maker maker;
		Requirement::Maker::Chain chain(maker, opOr);

		// insert cdhash requirement for all architectures
		__block CFRef<CFMutableArrayRef> allHashes = CFArrayCreateMutableCopy(NULL, 0, this->cdHashes());
		handleOtherArchitectures(^(SecStaticCode *other) {
			CFArrayRef hashes = other->cdHashes();
			CFArrayAppendArray(allHashes, hashes, CFRangeMake(0, CFArrayGetCount(hashes)));
		});
		CFIndex count = CFArrayGetCount(allHashes);
		for (CFIndex n = 0; n < count; ++n) {
			chain.add();
			maker.cdhash(CFDataRef(CFArrayGetValueAtIndex(allHashes, n)));
		}
		return maker.make();
	} else {
#if TARGET_OS_OSX
		// full signature: Gin up full context and let DRMaker do its thing
		validateDirectory();		// need the cert chain
		CFRef<CFDateRef> secureTimestamp;
		if (CFAbsoluteTime time = this->signingTimestamp()) {
			secureTimestamp.take(CFDateCreate(NULL, time));
		}
		Requirement::Context context(this->certificates(),
			this->infoDictionary(),
			this->entitlements(),
			this->identifier(),
			this->codeDirectory(),
			NULL,
			kSecCodeSignatureNoHash,
			false,
			secureTimestamp,
			this->teamID()
		);
		return DRMaker(context).make();
#else
        MacOSError::throwMe(errSecCSUnimplemented);
#endif
	}
}


//
// Validate a SecStaticCode against the internal requirement of a particular type.
//
void SecStaticCode::validateRequirements(SecRequirementType type, SecStaticCode *target,
	OSStatus nullError /* = errSecSuccess */)
{
	DTRACK(CODESIGN_EVAL_STATIC_INTREQ, this, type, target, nullError);
	if (const Requirement *req = internalRequirement(type))
		target->validateRequirement(req, nullError ? nullError : errSecCSReqFailed);
	else if (nullError)
		MacOSError::throwMe(nullError);
	else
		/* accept it */;
}

//
// Validate this StaticCode against an external Requirement
//
bool SecStaticCode::satisfiesRequirement(const Requirement *req, OSStatus failure)
{
	bool result = false;
	assert(req);
	validateDirectory();
	CFRef<CFDateRef> secureTimestamp;
	if (CFAbsoluteTime time = this->signingTimestamp()) {
		secureTimestamp.take(CFDateCreate(NULL, time));
	}
	result = req->validates(Requirement::Context(mCertChain, infoDictionary(), entitlements(),
												 codeDirectory()->identifier(), codeDirectory(),
												 NULL, kSecCodeSignatureNoHash, mRep->appleInternalForcePlatform(),
												 secureTimestamp, teamID()),
							failure);
	return result;
}

void SecStaticCode::validateRequirement(const Requirement *req, OSStatus failure)
{
	if (!this->satisfiesRequirement(req, failure))
		MacOSError::throwMe(failure);
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
// Gather (mostly) API-official information about this StaticCode.
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
	// Add the generic attributes that we always include
	//
	CFDictionaryAddValue(dict, kSecCodeInfoIdentifier, CFTempString(this->identifier()));
	CFDictionaryAddValue(dict, kSecCodeInfoFlags, CFTempNumber(this->codeDirectory(false)->flags.get()));
	CFDictionaryAddValue(dict, kSecCodeInfoFormat, CFTempString(this->format()));
	CFDictionaryAddValue(dict, kSecCodeInfoSource, CFTempString(this->signatureSource()));
	CFDictionaryAddValue(dict, kSecCodeInfoUnique, this->cdHash());
	CFDictionaryAddValue(dict, kSecCodeInfoCdHashes, this->cdHashes());
	CFDictionaryAddValue(dict, kSecCodeInfoCdHashesFull, this->cdHashesFull());
	const CodeDirectory* cd = this->codeDirectory(false);
	CFDictionaryAddValue(dict, kSecCodeInfoDigestAlgorithm, CFTempNumber(cd->hashType));
	CFRef<CFArrayRef> digests = makeCFArrayFrom(^CFTypeRef(CodeDirectory::HashAlgorithm type) { return CFTempNumber(type); }, hashAlgorithms());
	CFDictionaryAddValue(dict, kSecCodeInfoDigestAlgorithms, digests);
	if (cd->platform)
		CFDictionaryAddValue(dict, kSecCodeInfoPlatformIdentifier, CFTempNumber(cd->platform));
	if (cd->runtimeVersion()) {
		CFDictionaryAddValue(dict, kSecCodeInfoRuntimeVersion, CFTempNumber(cd->runtimeVersion()));
	}

	//
	// Deliver any Info.plist only if it looks intact
	//
	try {
		if (CFDictionaryRef info = this->infoDictionary())
			CFDictionaryAddValue(dict, kSecCodeInfoPList, info);
	} catch (...) { }		// don't deliver Info.plist if questionable

	//
	// kSecCSSigningInformation adds information about signing certificates and chains
	//
	if (flags & kSecCSSigningInformation)
		try {
			if (CFDataRef sig = this->signature())
				CFDictionaryAddValue(dict, kSecCodeInfoCMS, sig);
			if (const char *teamID = this->teamID())
				CFDictionaryAddValue(dict, kSecCodeInfoTeamIdentifier, CFTempString(teamID));
			if (mTrust)
				CFDictionaryAddValue(dict, kSecCodeInfoTrust, mTrust);
			if (CFArrayRef certs = this->certificates())
				CFDictionaryAddValue(dict, kSecCodeInfoCertificates, certs);
			if (CFAbsoluteTime time = this->signingTime())
				if (CFRef<CFDateRef> date = CFDateCreate(NULL, time))
					CFDictionaryAddValue(dict, kSecCodeInfoTime, date);
			if (CFAbsoluteTime time = this->signingTimestamp())
				if (CFRef<CFDateRef> date = CFDateCreate(NULL, time))
					CFDictionaryAddValue(dict, kSecCodeInfoTimestamp, date);
		} catch (...) { }

	//
	// kSecCSRequirementInformation adds information on requirements
	//
	if (flags & kSecCSRequirementInformation)

//DR not currently supported on iOS
#if TARGET_OS_OSX
		try {
			if (const Requirements *reqs = this->internalRequirements()) {
				CFDictionaryAddValue(dict, kSecCodeInfoRequirements,
					CFTempString(Dumper::dump(reqs)));
				CFDictionaryAddValue(dict, kSecCodeInfoRequirementData, CFTempData(*reqs));
			}

			const Requirement *dreq = this->designatedRequirement();
			CFRef<SecRequirementRef> dreqRef = (new SecRequirement(dreq))->handle();
			CFDictionaryAddValue(dict, kSecCodeInfoDesignatedRequirement, dreqRef);
			if (this->internalRequirement(kSecDesignatedRequirementType)) {	// explicit
				CFRef<SecRequirementRef> ddreqRef = (new SecRequirement(this->defaultDesignatedRequirement(), true))->handle();
				CFDictionaryAddValue(dict, kSecCodeInfoImplicitDesignatedRequirement, ddreqRef);
			} else {	// implicit
				CFDictionaryAddValue(dict, kSecCodeInfoImplicitDesignatedRequirement, dreqRef);
			}
		} catch (...) { }
#endif

	try {
		if (CFDataRef ent = this->component(cdEntitlementSlot)) {
			CFDictionaryAddValue(dict, kSecCodeInfoEntitlements, ent);
			if (CFDictionaryRef entdict = this->entitlements()) {
				if (needsCatalystEntitlementFixup(entdict)) {
					// If this entitlement dictionary needs catalyst entitlements, make a copy and stick that into the
					// output dictionary instead.
					secinfo("staticCode", "%p fixed catalyst entitlements", this);
					CFRef<CFMutableDictionaryRef> tempEntitlements = makeCFMutableDictionary(entdict);
					updateCatalystEntitlements(tempEntitlements);
					CFRef<CFDictionaryRef> newEntitlements = CFDictionaryCreateCopy(NULL, tempEntitlements);
					if (newEntitlements) {
						CFDictionaryAddValue(dict, kSecCodeInfoEntitlementsDict, newEntitlements.get());
					} else {
						secerror("%p unable to fixup entitlement dictionary", this);
						CFDictionaryAddValue(dict, kSecCodeInfoEntitlementsDict, entdict);
					}
				} else {
					CFDictionaryAddValue(dict, kSecCodeInfoEntitlementsDict, entdict);
				}
			}
		}
	} catch (...) { }

	//
	// kSecCSInternalInformation adds internal information meant to be for Apple internal
	// use (SPI), and not guaranteed to be stable. Primarily, this is data we want
	// to reliably transmit through the API wall so that code outside the Security.framework
	// can use it without having to play nasty tricks to get it.
	//
	if (flags & kSecCSInternalInformation) {
		try {
			if (mDir)
				CFDictionaryAddValue(dict, kSecCodeInfoCodeDirectory, mDir);
			CFDictionaryAddValue(dict, kSecCodeInfoCodeOffset, CFTempNumber(mRep->signingBase()));
        if (!(flags & kSecCSSkipResourceDirectory)) {
            if (CFRef<CFDictionaryRef> rdict = getDictionary(cdResourceDirSlot, false))	// suppress validation
                CFDictionaryAddValue(dict, kSecCodeInfoResourceDirectory, rdict);
        }
		if (CFRef<CFDictionaryRef> ddict = copyDiskRepInformation())
			CFDictionaryAddValue(dict, kSecCodeInfoDiskRepInfo, ddict);
		} catch (...) { }
		if (mNotarizationChecked && !isnan(mNotarizationDate)) {
			CFRef<CFDateRef> date = CFDateCreate(NULL, mNotarizationDate);
			if (date) {
				CFDictionaryAddValue(dict, kSecCodeInfoNotarizationDate, date.get());
			} else {
				secerror("Error creating date from timestamp: %f", mNotarizationDate);
			}
		}
		if (this->codeDirectory()) {
			uint32_t version = this->codeDirectory()->version;
			CFDictionaryAddValue(dict, kSecCodeInfoSignatureVersion, CFTempNumber(version));
		}
	}

	if (flags & kSecCSCalculateCMSDigest) {
		try {
			CFDictionaryAddValue(dict, kSecCodeInfoCMSDigestHashType, CFTempNumber(cmsDigestHashType()));
			
			CFRef<CFDataRef> cmsDigest = createCmsDigest();
			if (cmsDigest) {
				CFDictionaryAddValue(dict, kSecCodeInfoCMSDigest, cmsDigest.get());
			}
		} catch (...) { }
	}

	//
	// kSecCSContentInformation adds more information about the physical layout
	// of the signed code. This is (only) useful for packaging or patching-oriented
	// applications.
	//
	if (flags & kSecCSContentInformation && !(flags & kSecCSSkipResourceDirectory))
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
	StLock<Mutex> _(mLock);
	if (mStatus == errSecSuccess)
		mStatus = rc;			// record first failure for eventual error return
	if (type) {
		if (!mCollection)
			mCollection.take(makeCFMutableDictionary());
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
	assert(mStatus != errSecSuccess);
	throw CSError(mStatus, mCollection.retain());
}


//
// Master validation driver.
// This is the static validation (only) driver for the API.
//
// SecStaticCode exposes an a la carte menu of topical validators applying
// to a given object. The static validation API pulls them together reliably,
// but it also adds three matrix dimensions: architecture (for "fat" Mach-O binaries),
// nested code, and multiple digests. This function will crawl a suitable cross-section of this
// validation matrix based on which options it is given, creating temporary
// SecStaticCode objects on the fly to complete the task.
// (The point, of course, is to do as little duplicate work as possible.)
//
void SecStaticCode::staticValidate(SecCSFlags flags, const SecRequirement *req)
{
	setValidationFlags(flags);

#if TARGET_OS_OSX
	if (!mStaplingChecked) {
		mRep->registerStapledTicket();
		mStaplingChecked = true;
	}

	if (isFlagSet(mFlags, kSecCSForceOnlineNotarizationCheck) && !validationCannotUseNetwork()) {
		if (!mNotarizationChecked) {
			if (this->cdHash()) {
				bool is_revoked = checkNotarizationServiceForRevocation(this->cdHash(), (SecCSDigestAlgorithm)this->hashAlgorithm(), &mNotarizationDate);
				if (is_revoked) {
					MacOSError::throwMe(errSecCSRevokedNotarization);
				}
			}
			mNotarizationChecked = true;
		}
	}
#endif // TARGET_OS_OSX

	// initialize progress/cancellation state
	if (flags & kSecCSReportProgress) {
		prepareProgress(estimateResourceWorkload() + 2);	// +1 head, +1 tail
	}

  	// core components: once per architecture (if any)
	this->staticValidateCore(flags, req);
	if (flags & kSecCSCheckAllArchitectures) {
		handleOtherArchitectures(^(SecStaticCode* subcode) {
			if (flags & kSecCSCheckGatekeeperArchitectures) {
				Universal *fat = subcode->diskRep()->mainExecutableImage();
				assert(fat && fat->narrowed());	// handleOtherArchitectures gave us a focused architecture slice
				Architecture arch = fat->bestNativeArch();	// actually, the ONLY one
				if ((arch.cpuType() & ~CPU_ARCH_MASK) == CPU_TYPE_POWERPC)
					return;	// irrelevant to Gatekeeper
			}
			subcode->detachedSignature(this->mDetachedSig);	// carry over explicit (but not implicit) detached signature
			subcode->staticValidateCore(flags, req);
		});
	}
	reportProgress();

	// allow monitor intervention in source validation phase
	reportEvent(CFSTR("prepared"), NULL);

	// resources: once for all architectures
	if (!(flags & kSecCSDoNotValidateResources)) {
		this->validateResources(flags);
	}

	// perform strict validation if desired
	if (flags & kSecCSStrictValidate) {
		mRep->strictValidate(codeDirectory(), mTolerateErrors, mValidationFlags);
		reportProgress();
	} else if (flags & kSecCSStrictValidateStructure) {
		mRep->strictValidateStructure(codeDirectory(), mTolerateErrors, mValidationFlags);
	}

	// allow monitor intervention
	if (CFRef<CFTypeRef> veto = reportEvent(CFSTR("validated"), NULL)) {
		if (CFGetTypeID(veto) == CFNumberGetTypeID())
			MacOSError::throwMe(cfNumber<OSStatus>(veto.as<CFNumberRef>()));
		else
			MacOSError::throwMe(errSecCSBadCallbackValue);
	}
}

void SecStaticCode::staticValidateCore(SecCSFlags flags, const SecRequirement *req)
{
	try {
		this->validateNonResourceComponents();	// also validates the CodeDirectory
		this->validateTopDirectory();
		if (!(flags & kSecCSDoNotValidateExecutable))
			this->validateExecutable();
		if (req)
			this->validateRequirement(req->requirement(), errSecCSReqFailed);
    } catch (CSError &err) {
        if (Universal *fat = this->diskRep()->mainExecutableImage())    // Mach-O
            if (MachO *mach = fat->architecture()) {
                err.augment(kSecCFErrorArchitecture, CFTempString(mach->architecture().displayName()));
                delete mach;
            }
        throw;
    } catch (const MacOSError &err) {
        // add architecture information if we can get it
        if (Universal *fat = this->diskRep()->mainExecutableImage())
            if (MachO *mach = fat->architecture()) {
                CFTempString arch(mach->architecture().displayName());
                delete mach;
                CSError::throwMe(err.error, kSecCFErrorArchitecture, arch);
            }
        throw;
    }
}

void SecStaticCode::staticValidateResource(string resourcePath, SecCSFlags flags, const SecRequirement *req)
{
	// resourcePath is always the absolute path to the resource, each analysis can make a relative path
	// if it needs one.  Passing through relative paths but then needing to re-create the full path is
	// more complicated in the case where a subpath is no longer contained within the resource envelope
	// of the next subcode.

	// Validate the resource is inside the outer bundle by finding the bundle's resource path in the string
	// of the full resource.  If its a prefix match this will also compute the remaining relative path
	// that we'll need later.
	string baseResourcePath;
	string relativePath;

	if (this->mainExecutablePath() == resourcePath) {
		// Nothing to do here, we're just validating the main executable so proceed
		// to the validation below.
	} else {
		baseResourcePath = cfString(resourceBase());
		relativePath = pathRemaining(resourcePath, baseResourcePath);
		if (relativePath == "") {
			// The resource is not a prefix match with the bundle or the arguments are bad.
			secerror("Requested resource was not within the code object: %s, %s", resourcePath.c_str(), baseResourcePath.c_str());
			MacOSError::throwMe(errSecParam);
		}
	}

	// In general, we never want to be validating executables of the bundles as we traverse, so just ensure the
	// bit is always set to skip them as we go.
	flags = addFlags(flags, kSecCSDoNotValidateExecutable);

	// First special case is the main executable, which means we're about to validate it as part of the
	// static validation here.
	bool needsAdditionalValidation = true;
	if (this->mainExecutablePath() == resourcePath) {
		needsAdditionalValidation = false;

		// If the caller did not request fast validation of an executable, ensure we clear the 'do
		// not validate' bit here before validating.
		if (!isFlagSet(flags, kSecCSFastExecutableValidation)) {
			flags = clearFlags(flags, kSecCSDoNotValidateExecutable);
		}
	}

	// The Info.plist is covered by the core validation, so there's no more work to be done.
	if (relativePath == "Info.plist") {
		needsAdditionalValidation = false;
	}

	// Perform basic validation of the code object itself, since thats required for the rest of the comparison
	// to be valid.
	this->staticValidateCore(flags, NULL);
	if (req) {
		// If we have an explicit requirement we must meet and fail, then it should actually
		// be recorded as the resource being modified.
		this->validateRequirement(req->requirement(), errSecCSBadResource);
	}

	if (!needsAdditionalValidation) {
		// staticValidateCore has already validated the main executable so we're all done!
		return;
	}

	if (!isFlagSet(flags, kSecCSSkipRootVolumeExceptions)) {
		if (itemQualifiesForResourceExemption(resourcePath)) {
			secinfo("staticCode", "Requested resource was on root filesystem: %s", resourcePath.c_str());
			return;
		}
	}

	// We need to load resource rules to be able to do a single file resource comparison against.
	CFDictionaryRef rules;
	CFDictionaryRef files;
	uint32_t version;
	if (!loadResources(rules, files, version)) {
		MacOSError::throwMe(errSecCSResourcesNotFound);
	}

	// Load up a full resource builder so we can properly parse all the rules.
	bool strict = (flags & kSecCSStrictValidate);
	MacOSErrorSet toleratedErrors;
	ResourceBuilder resources(baseResourcePath, baseResourcePath, rules, strict, toleratedErrors);
	diskRep()->adjustResources(resources);

	// First, check if the path itself is inside of an omission or exclusion hole.
	ResourceBuilder::Rule *rule = resources.findRule(relativePath);
	if (rule) {
		if (rule->flags & (ResourceBuilder::omitted | ResourceBuilder::exclusion)) {
			secerror("Requested resource was not sealed: %d", rule->flags);
			MacOSError::throwMe(errSecCSResourcesNotSealed);
		}
	}

	// Otherwise look for an exact file match, or find the most deeply nested code.
	CFTypeRef file = CFDictionaryGetValue(files, CFTempString(relativePath));
	if (file) {
		// This item matched a file rule exactly, so just validate it directly with this object.
		AutoFileDesc fd = AutoFileDesc(resourcePath);
		bool isSymlink = fd.isA(S_IFLNK);

		// Since this is a direct file match, if its for a nested bundle then we want to enable executable
		// validation based on whether fast executable validation was requested.
		if (!isFlagSet(flags, kSecCSFastExecutableValidation)) {
			flags = clearFlags(flags, kSecCSDoNotValidateExecutable);
		}

		ResourceSeal seal(file);
		if (seal.nested()) {
			CFRef<SecRequirementRef> req;
			if (SecRequirementCreateWithString(seal.requirement(), kSecCSDefaultFlags, &req.aref())) {
				MacOSError::throwMe(errSecCSResourcesInvalid);
			}

			// If the resource seal indicates this is nested code, create a new code object for this
			// nested code and then validate the resource within that object.
			SecPointer<SecStaticCode> subcode = new SecStaticCode(DiskRep::bestGuess(resourcePath));
			subcode->initializeFromParent(*this);
			// If there was an exact match but its nested code, then the ask is really to validate the
			// main executable of the nested code.
			subcode->staticValidateResource(subcode->mainExecutablePath(), flags, SecRequirement::required(req));
		} else {
			// For other resource types, just a single file resource validation with a ValidationContext that
			// will immediately throw an error if any issues are encountered.
			ValidationContext *context = new ValidationContext(*this);
			validateResource(files, relativePath, isSymlink, *context, flags, version);
		}
	} else {
		// It wasn't a simple file resource within the current code signature, so we're looking for a nested code.
		__block bool itemFound = false;

		// Iterate through the largest possible chunks of paths looking for nested code matches.
		iterateLargestSubpaths(relativePath, ^bool(string subpath) {
			CFTypeRef file = CFDictionaryGetValue(files, CFTempString(subpath));
			if (file) {
				itemFound = true;

				ResourceSeal seal(file);
				if (seal.nested()) {
					CFRef<SecRequirementRef> req;
					if (SecRequirementCreateWithString(seal.requirement(), kSecCSDefaultFlags, &req.aref())) {
						MacOSError::throwMe(errSecCSResourcesInvalid);
					}

					// If the resource seal indicates this is nested code, create a new code object for this
					// nested code and then validate the resource within that object.
					CFRef<CFURLRef> itemURL = makeCFURL(subpath, false, resourceBase());
					string fullPath = cfString(itemURL);
					SecPointer<SecStaticCode> subcode = new SecStaticCode(DiskRep::bestGuess(fullPath));
					subcode->initializeFromParent(*this);
					subcode->staticValidateResource(resourcePath, flags, SecRequirement::required(req));
				} else {
					// Any other type of nested resource is not ok, so just bail.
					secerror("Unexpected item hit traversing resource: %@", file);
					MacOSError::throwMe(errSecCSBadResource);
				}
				// If we find a match, stop walking up for further matching.
				return true;
			}
			return false;
		});

		// If we finished everything and didn't find the item, its not a valid resource.
		if (!itemFound) {
			secerror("Requested resource was not found: %s", resourcePath.c_str());
			MacOSError::throwMe(errSecCSBadResource);
		}
	}
}

//
// A helper that generates SecStaticCode objects for all but the primary architecture
// of a fat binary and calls a block on them.
// If there's only one architecture (or this is an architecture-agnostic code),
// nothing happens quickly.
//
void SecStaticCode::handleOtherArchitectures(void (^handle)(SecStaticCode* other))
{
	if (Universal *fat = this->diskRep()->mainExecutableImage()) {
		Universal::Architectures architectures;
		fat->architectures(architectures);
		if (architectures.size() > 1) {
			DiskRep::Context ctx;
			off_t activeOffset = fat->archOffset();
			for (Universal::Architectures::const_iterator arch = architectures.begin(); arch != architectures.end(); ++arch) {
				try {
					ctx.offset = int_cast<size_t, off_t>(fat->archOffset(*arch));
					ctx.size = fat->lengthOfSlice(int_cast<off_t,size_t>(ctx.offset));
					if (ctx.offset != activeOffset) {	// inactive architecture; check it
						SecPointer<SecStaticCode> subcode = new SecStaticCode(DiskRep::bestGuess(this->mainExecutablePath(), &ctx));

						// There may not actually be a full validation happening, but any operations that do occur should respect the
						// same network settings as the existing validation, so propagate those flags forward here.
						SecCSFlags flagsToPropagate = (kSecCSAllowNetworkAccess | kSecCSNoNetworkAccess);
						subcode->setValidationFlags(mValidationFlags & flagsToPropagate);

						subcode->detachedSignature(this->mDetachedSig); // carry over explicit (but not implicit) detached signature
						if (this->teamID() == NULL || subcode->teamID() == NULL) {
							if (this->teamID() != subcode->teamID())
								MacOSError::throwMe(errSecCSSignatureInvalid);
						} else if (strcmp(this->teamID(), subcode->teamID()) != 0)
							MacOSError::throwMe(errSecCSSignatureInvalid);
						handle(subcode);
					}
				} catch(std::out_of_range e) {
					// some of our int_casts fell over.
					MacOSError::throwMe(errSecCSBadObjectFormat);
				}
			}
		}
	}
}

//
// A method that takes a certificate chain (certs) and evaluates
// if it is a Mac or IPhone developer cert, an app store distribution cert,
// or a developer ID
//
bool SecStaticCode::isAppleDeveloperCert(CFArrayRef certs)
{
	static const std::string appleDeveloperRequirement = "(" + std::string(WWDRRequirement) + ") or (" + MACWWDRRequirement + ") or (" + developerID + ") or (" + distributionCertificate + ") or (" + iPhoneDistributionCert + ")";
	SecPointer<SecRequirement> req = new SecRequirement(parseRequirement(appleDeveloperRequirement), true);
	Requirement::Context ctx(certs, NULL, NULL, "", NULL, NULL, kSecCodeSignatureNoHash, false, NULL, "");

	return req->requirement()->validates(ctx);
}

CFDataRef SecStaticCode::createCmsDigest()
{
	/*
	 * The CMS digest is a hash of the primary (first, most compatible) code directory,
	 * but its hash algorithm is fixed and not related to the code directory's
	 * hash algorithm.
	 */
	
	auto it = codeDirectories()->begin();
	
	if (it == codeDirectories()->end()) {
		return NULL;
	}

	CodeDirectory const * const cd = reinterpret_cast<CodeDirectory const*>(CFDataGetBytePtr(it->second));
	
	RefPointer<DynamicHash> hash = cd->hashFor(mCMSDigestHashType);
	CFMutableDataRef data = CFDataCreateMutable(NULL, hash->digestLength());
	CFDataSetLength(data, hash->digestLength());
	hash->update(cd, cd->length());
	hash->finish(CFDataGetMutableBytePtr(data));
	
	return data;
}
	
} // end namespace CodeSigning
} // end namespace Security
