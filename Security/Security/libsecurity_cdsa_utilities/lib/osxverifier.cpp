/*
 * Copyright (c) 2000-2001,2011,2013-2014 Apple Inc. All Rights Reserved.
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
// osxsigner - MacOS X's standard code signing algorithm.
//
#include <security_cdsa_utilities/osxverifier.h>
#include <security_utilities/unix++.h>
#include <security_utilities/hashing.h>
#include <security_utilities/memutils.h>
#include <security_utilities/debugging.h>
#include <security_codesigning/requirement.h>
#include <security_codesigning/reqdumper.h>		// debug only


using namespace CodeSigning;


namespace Security {


//
// Create a Verifier from a code object.
//
// This does not add any auxiliary information blobs. You can do that
// by calling add() after construction, of course.
//
OSXVerifier::OSXVerifier(OSXCode *code)
{
	mPath = code->canonicalPath();
	secdebug("codesign", "building verifier for %s", mPath.c_str());

	// build new-style verifier
	CFRef<SecStaticCodeRef> staticCode = code->codeRef();
	switch (OSStatus rc = SecCodeCopyDesignatedRequirement(staticCode,
			kSecCSDefaultFlags, &mRequirement.aref())) {
	case errSecSuccess:
		secdebug("codesign", "  is signed; canonical requirement loaded");
		break;
	case errSecCSUnsigned:
		secdebug("codesign", "  is unsigned; no requirement");
		break;
	default:
		MacOSError::throwMe(rc);
	}
	
	// build old-style verifier
	makeLegacyHash(code, mLegacyHash);
	secdebug("codesign", "  hash generated");
}


//
// Create a Verifier from hash, path, and requirement.
// Again, this has no auxiliary data when constructed.
//
OSXVerifier::OSXVerifier(const SHA1::Byte *hash, const std::string &path)
	: mPath(path)
{
	secdebug("codesign", "building verifier from hash %p and path=%s", hash, path.c_str());
	if (hash)
		memcpy(mLegacyHash, hash, sizeof(mLegacyHash));
	else
		memset(mLegacyHash, 0, sizeof(mLegacyHash));
}


OSXVerifier::~OSXVerifier()
{
	secdebug("codesign", "%p verifier destroyed", this);
}


//
// Add an auxiliary comment blob.
// Note that we only allow one auxiliary blob for each magic number.
//
void OSXVerifier::add(const BlobCore *blob)
{
	if (blob->is<Requirement>()) {
#if defined(NDEBUG)
		secdebug("codesign", "%p verifier adds requirement", this);
#else
		secdebug("codesign", "%p verifier adds requirement %s", this,
			Dumper::dump(Requirement::specific(blob), true).c_str());
#endif //NDEBUG
		MacOSError::check(SecRequirementCreateWithData(CFTempData(*blob),
			kSecCSDefaultFlags, &mRequirement.aref()));
	} else {
		secdebug("codesign", "%p verifier adds blob (0x%x,%zd)",
			this, blob->magic(), blob->length());
		BlobCore * &slot = mAuxiliary[blob->magic()];
		if (slot)
			::free(slot);
		slot = blob->clone();
	}
}


//
// Find a comment blob, by magic number
//
const BlobCore *OSXVerifier::find(BlobCore::Magic magic)
{
	AuxMap::const_iterator it = mAuxiliary.find(magic);
	return (it == mAuxiliary.end()) ? NULL : it->second;
}


void OSXVerifier::makeLegacyHash(OSXCode *code, SHA1::Digest digest)
{
	secdebug("codesign", "calculating legacy hash for %s", code->canonicalPath().c_str());
	UnixPlusPlus::AutoFileDesc fd(code->executablePath(), O_RDONLY);
	char buffer[legacyHashLimit];
	size_t size = fd.read(buffer, legacyHashLimit);
	SHA1 hash;
	hash(buffer, size);
	hash.finish(digest);
}


//
// The AuxMap helper class provides a map-to-Blob-pointers with automatic memory management.
//
OSXVerifier::AuxMap::AuxMap(const OSXVerifier::AuxMap &src)
{
	for (const_iterator it = src.begin(); it != src.end(); it++)
		this->insert(*it);
}

OSXVerifier::AuxMap::~AuxMap()
{
	for (const_iterator it = this->begin(); it != this->end(); ++it)
		::free(it->second);
}


#if DEBUGDUMP

void OSXVerifier::dump() const
{
	static const SHA1::Digest nullDigest = { 0 };
	if (!memcmp(mLegacyHash, nullDigest, sizeof(mLegacyHash))) {
		Debug::dump("(no hash)");
	} else {
		Debug::dump("oldHash=");
		Debug::dumpData(mLegacyHash, sizeof(mLegacyHash));
	}
	if (mRequirement) {
		CFRef<CFDataRef> reqData;
		if (!SecRequirementCopyData(mRequirement, 0, &reqData.aref())) {
			Debug::dump(" Requirement =>");
			((const Requirement *)CFDataGetBytePtr(reqData))->dump();
		}
	} else {
		Debug::dump(" NO REQ");
	}
}

#endif //DEBUGDUMP

} // end namespace Security
