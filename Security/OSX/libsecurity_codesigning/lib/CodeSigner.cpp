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
// CodeSigner - SecCodeSigner API objects
//
#include "CodeSigner.h"
#include "signer.h"
#include "csdatabase.h"
#include "drmaker.h"
#include "csutilities.h"
#include <security_utilities/unix++.h>
#include <security_utilities/unixchild.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <vector>

namespace Security {

__SEC_CFTYPE(SecIdentity)

namespace CodeSigning {

using namespace UnixPlusPlus;


//
// A helper for parsing out a CFDictionary signing-data specification
//
class SecCodeSigner::Parser : CFDictionary {
public:
	Parser(SecCodeSigner &signer, CFDictionaryRef parameters);
	
	bool getBool(CFStringRef key) const
	{
		if (CFBooleanRef flag = get<CFBooleanRef>(key))
			return flag == kCFBooleanTrue;
		else
			return false;
	}
};


//
// Construct a SecCodeSigner
//
SecCodeSigner::SecCodeSigner(SecCSFlags flags)
	: mOpFlags(flags), mLimitedAsync(NULL)
{
}


//
// Clean up a SecCodeSigner
//
SecCodeSigner::~SecCodeSigner() throw()
try {
	delete mLimitedAsync;
} catch (...) {
	return;
}


//
// Parse an input parameter dictionary and set ready-to-use parameters
//
void SecCodeSigner::parameters(CFDictionaryRef paramDict)
{
	Parser(*this, paramDict);
	if (!valid())
		MacOSError::throwMe(errSecCSInvalidObjectRef);
}

//
// Retrieve the team ID from the signing certificate if and only if
// it is an apple developer signing cert
//
std::string SecCodeSigner::getTeamIDFromSigner(CFArrayRef certs)
{
	if (mSigner && mSigner != SecIdentityRef(kCFNull)) {
		CFRef<SecCertificateRef> signerCert;
		MacOSError::check(SecIdentityCopyCertificate(mSigner, &signerCert.aref()));

		/* Make sure the certificate looks like an Apple certificate, because we do not
			extract the team ID from a non Apple certificate */
		if (SecStaticCode::isAppleDeveloperCert(certs)) {
			CFRef<CFStringRef> teamIDFromCert;

			MacOSError::check(SecCertificateCopySubjectComponent(signerCert.get(), &CSSMOID_OrganizationalUnitName, &teamIDFromCert.aref()));

			if (teamIDFromCert)
				return cfString(teamIDFromCert);
		}
	}

	return "";
}

//
// Roughly check for validity.
// This isn't thorough; it just sees if if looks like we've set up the object appropriately.
//
bool SecCodeSigner::valid() const
{
	if (mOpFlags & kSecCSRemoveSignature)
		return true;
	return mSigner;
}


//
// Sign code
//
void SecCodeSigner::sign(SecStaticCode *code, SecCSFlags flags)
{
	code->setValidationFlags(flags);
	if (code->isSigned() && (flags & kSecCSSignPreserveSignature))
		return;
	Signer operation(*this, code);
	if ((flags | mOpFlags) & kSecCSRemoveSignature) {
		secinfo("signer", "%p will remove signature from %p", this, code);
		operation.remove(flags);
	} else {
		if (!valid())
			MacOSError::throwMe(errSecCSInvalidObjectRef);
		secinfo("signer", "%p will sign %p (flags 0x%x)", this, code, flags);
		operation.sign(flags);
	}
	code->resetValidity();
}


//
// ReturnDetachedSignature is called by writers or editors that try to return
// detached signature data (rather than annotate the target).
//
void SecCodeSigner::returnDetachedSignature(BlobCore *blob, Signer &signer)
{
	assert(mDetached);
	if (CFGetTypeID(mDetached) == CFURLGetTypeID()) {
		// URL to destination file
		AutoFileDesc fd(cfString(CFURLRef(mDetached.get())), O_WRONLY | O_CREAT | O_TRUNC);
		fd.writeAll(*blob);
	} else if (CFGetTypeID(mDetached) == CFDataGetTypeID()) {
		CFDataAppendBytes(CFMutableDataRef(mDetached.get()),
			(const UInt8 *)blob, blob->length());
	} else if (CFGetTypeID(mDetached) == CFNullGetTypeID()) {
		SignatureDatabaseWriter db;
		db.storeCode(blob, signer.path().c_str());
	} else
		assert(false);
}


//
// The actual parsing operation is done in the Parser class.
//
// Note that we need to copy or retain all incoming data. The caller has no requirement
// to keep the parameters dictionary around.
//
SecCodeSigner::Parser::Parser(SecCodeSigner &state, CFDictionaryRef parameters)
	: CFDictionary(parameters, errSecCSBadDictionaryFormat)
{
	// the signer may be an identity or null
	state.mSigner = SecIdentityRef(get<CFTypeRef>(kSecCodeSignerIdentity));
	if (state.mSigner)
		if (CFGetTypeID(state.mSigner) != SecIdentityGetTypeID() && !CFEqual(state.mSigner, kCFNull))
			MacOSError::throwMe(errSecCSInvalidObjectRef);

	// the flags need some augmentation
	if (CFNumberRef flags = get<CFNumberRef>(kSecCodeSignerFlags)) {
		state.mCdFlagsGiven = true;
		state.mCdFlags = cfNumber<uint32_t>(flags);
	} else
		state.mCdFlagsGiven = false;
	
	// digest algorithms are specified as a numeric code
	if (CFCopyRef<CFTypeRef> digestAlgorithms = get<CFTypeRef>(kSecCodeSignerDigestAlgorithm)) {
		CFRef<CFArrayRef> array = cfArrayize(digestAlgorithms);
		CFToVector<CodeDirectory::HashAlgorithm, CFNumberRef, cfNumber<CodeDirectory::HashAlgorithm> > digests(array);
		std::copy(digests.begin(), digests.end(), std::inserter(state.mDigestAlgorithms, state.mDigestAlgorithms.begin()));
	}

	if (CFNumberRef cmsSize = get<CFNumberRef>(CFSTR("cmssize")))
		state.mCMSSize = cfNumber<size_t>(cmsSize);
	else
		state.mCMSSize = 9000;	// likely big enough

	// metadata preservation options
	if (CFNumberRef preserve = get<CFNumberRef>(kSecCodeSignerPreserveMetadata)) {
		state.mPreserveMetadata = cfNumber<uint32_t>(preserve);
	} else
		state.mPreserveMetadata = 0;

	// signing time can be a CFDateRef or null
	if (CFTypeRef time = get<CFTypeRef>(kSecCodeSignerSigningTime)) {
		if (CFGetTypeID(time) == CFDateGetTypeID() || time == kCFNull)
			state.mSigningTime = CFDateRef(time);
		else
			MacOSError::throwMe(errSecCSInvalidObjectRef);
	}
	
	if (CFStringRef ident = get<CFStringRef>(kSecCodeSignerIdentifier))
		state.mIdentifier = cfString(ident);
	
	if (CFStringRef teamid = get<CFStringRef>(kSecCodeSignerTeamIdentifier))
		state.mTeamID = cfString(teamid);
	
	if (CFNumberRef platform = get<CFNumberRef>(kSecCodeSignerPlatformIdentifier)) {
		int64_t ident = cfNumber<int64_t>(platform);
		if (ident < 0 || ident > maxPlatform)	// overflow
			MacOSError::throwMe(errSecCSInvalidPlatform);
		state.mPlatform = ident;
	}
	
	if (CFStringRef prefix = get<CFStringRef>(kSecCodeSignerIdentifierPrefix))
		state.mIdentifierPrefix = cfString(prefix);
	
	// Requirements can be binary or string (to be compiled).
	// We must pass them along to the signer for possible text substitution
	if (CFTypeRef reqs = get<CFTypeRef>(kSecCodeSignerRequirements)) {
		if (CFGetTypeID(reqs) == CFDataGetTypeID() || CFGetTypeID(reqs) == CFStringGetTypeID())
			state.mRequirements = reqs;
		else
			MacOSError::throwMe(errSecCSInvalidObjectRef);
	} else
		state.mRequirements = NULL;
	
	state.mNoMachO = getBool(CFSTR("no-macho"));
	
	state.mPageSize = get<CFNumberRef>(kSecCodeSignerPageSize);
	
	// detached can be (destination) file URL or (mutable) Data to be appended-to
	if ((state.mDetached = get<CFTypeRef>(kSecCodeSignerDetached))) {
		CFTypeID type = CFGetTypeID(state.mDetached);
		if (type != CFURLGetTypeID() && type != CFDataGetTypeID() && type != CFNullGetTypeID())
			MacOSError::throwMe(errSecCSInvalidObjectRef);
	}
	
	state.mDryRun = getBool(kSecCodeSignerDryRun);

	state.mResourceRules = get<CFDictionaryRef>(kSecCodeSignerResourceRules);
	
	state.mApplicationData = get<CFDataRef>(kSecCodeSignerApplicationData);
	state.mEntitlementData = get<CFDataRef>(kSecCodeSignerEntitlements);
	
	state.mSDKRoot = get<CFURLRef>(kSecCodeSignerSDKRoot);
    
	if (CFBooleanRef timestampRequest = get<CFBooleanRef>(kSecCodeSignerRequireTimestamp)) {
		state.mWantTimeStamp = timestampRequest == kCFBooleanTrue;
	} else {	// pick default
		state.mWantTimeStamp = false;
		if (state.mSigner && state.mSigner != SecIdentityRef(kCFNull)) {
			CFRef<SecCertificateRef> signerCert;
			MacOSError::check(SecIdentityCopyCertificate(state.mSigner, &signerCert.aref()));
			if (certificateHasField(signerCert, devIdLeafMarkerOID))
				state.mWantTimeStamp = true;
		}
	}
	state.mTimestampAuthentication = get<SecIdentityRef>(kSecCodeSignerTimestampAuthentication);
	state.mTimestampService = get<CFURLRef>(kSecCodeSignerTimestampServer);
	state.mNoTimeStampCerts = getBool(kSecCodeSignerTimestampOmitCertificates);
}


} // end namespace CodeSigning
} // end namespace Security
