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
// codedirectory - format and operations for code signing "code directory" structures
//
#include "codedirectory.h"
#include "csutilities.h"
#include "CSCommonPriv.h"
#include <vector>

using namespace UnixPlusPlus;


namespace Security {
namespace CodeSigning {


//
// Highest understood special slot in this CodeDirectory.
//
CodeDirectory::SpecialSlot CodeDirectory::maxSpecialSlot() const
{
	SpecialSlot slot = this->nSpecialSlots;
	if (slot > cdSlotMax)
		slot = cdSlotMax;
	return slot;
}


//
// Canonical filesystem names for select slot numbers.
// These are variously used for filenames, extended attribute names, etc.
// to get some consistency in naming. These are for storing signing-related
// data; they have no bearing on the actual hash slots in the CodeDirectory.
//
const char *CodeDirectory::canonicalSlotName(SpecialSlot slot)
{
	switch (slot) {
	case cdRequirementsSlot:
		return kSecCS_REQUIREMENTSFILE;
	case cdAlternateCodeDirectorySlots:
		return kSecCS_REQUIREMENTSFILE "-1";
	case cdAlternateCodeDirectorySlots+1:
		return kSecCS_REQUIREMENTSFILE "-2";
	case cdAlternateCodeDirectorySlots+2:
		return kSecCS_REQUIREMENTSFILE "-3";
	case cdAlternateCodeDirectorySlots+3:
		return kSecCS_REQUIREMENTSFILE "-4";
	case cdAlternateCodeDirectorySlots+4:
		return kSecCS_REQUIREMENTSFILE "-5";
	case cdResourceDirSlot:
		return kSecCS_RESOURCEDIRFILE;
	case cdCodeDirectorySlot:
		return kSecCS_CODEDIRECTORYFILE;
	case cdSignatureSlot:
		return kSecCS_SIGNATUREFILE;
	case cdTopDirectorySlot:
		return kSecCS_TOPDIRECTORYFILE;
	case cdEntitlementSlot:
		return kSecCS_ENTITLEMENTFILE;
	case cdRepSpecificSlot:
		return kSecCS_REPSPECIFICFILE;
	default:
		return NULL;
	}
}


//
// Canonical attributes of SpecialSlots.
//
unsigned CodeDirectory::slotAttributes(SpecialSlot slot)
{
	switch (slot) {
	case cdRequirementsSlot:
		return cdComponentIsBlob; // global
	case cdCodeDirectorySlot:
	case cdAlternateCodeDirectorySlots:
	case cdAlternateCodeDirectorySlots+1:
	case cdAlternateCodeDirectorySlots+2:
	case cdAlternateCodeDirectorySlots+3:
	case cdAlternateCodeDirectorySlots+4:
			return cdComponentPerArchitecture | cdComponentIsBlob;
	case cdSignatureSlot:
		return cdComponentPerArchitecture; // raw
	case cdEntitlementSlot:
		return cdComponentIsBlob; // global
	case cdIdentificationSlot:
		return cdComponentPerArchitecture; // raw
	default:
		return 0; // global, raw
	}
}


//
// Symbolic names for code directory special slots.
// These are only used for debug output. They are not API-official.
// Needs to be coordinated with the cd*Slot enumeration in codedirectory.h.
//
#if !defined(NDEBUG)
const char * const CodeDirectory::debugSlotName[] = {
	"codedirectory",
	"info",
	"requirements",
	"resources",
	"rep-specific",
	"entitlement"
};
#endif //NDEBUG


//
// Check a CodeDirectory for basic integrity. This should ensure that the
// version is understood by our code, and that the internal structure
// (offsets etc.) is intact. In particular, it must make sure that no offsets
// point outside the CodeDirectory.
// Throws if the directory is corrupted or out of versioning bounds.
// Returns if the version is usable (perhaps with degraded features due to
// compatibility hacks).
//
// Note: There are some things we don't bother checking because they won't
// cause crashes, and will just be flagged as nonsense later. For example,
// a Bad Guy could overlap the identifier and hash fields, which is nonsense
// but not dangerous.
//
void CodeDirectory::checkIntegrity() const
{
	// check version for support
	if (!this->validateBlob())
		MacOSError::throwMe(errSecCSSignatureInvalid);	// busted
	if (version > compatibilityLimit)
		MacOSError::throwMe(errSecCSSignatureUnsupported);	// too new - no clue
	if (version < earliestVersion)
		MacOSError::throwMe(errSecCSSignatureUnsupported);	// too old - can't support
	if (version > currentVersion)
		secinfo("codedir", "%p version 0x%x newer than current 0x%x",
			this, uint32_t(version), currentVersion);
	
	// now check interior offsets for validity
	if (!stringAt(identOffset))
		MacOSError::throwMe(errSecCSSignatureFailed); // identifier out of blob range
	if (version >= supportsTeamID && teamIDOffset != 0 && !stringAt(teamIDOffset))
			MacOSError::throwMe(errSecCSSignatureFailed); // identifier out of blob range
	if (!contains(hashOffset - int64_t(hashSize) * nSpecialSlots, hashSize * (int64_t(nSpecialSlots) + nCodeSlots)))
		MacOSError::throwMe(errSecCSSignatureFailed); // hash array out of blob range
	if (const Scatter *scatter = this->scatterVector()) {
		// the optional scatter vector is terminated with an element having (count == 0)
		unsigned int pagesConsumed = 0;
		for (;; scatter++) {
			if (!contains(scatter, sizeof(Scatter)))
				MacOSError::throwMe(errSecCSSignatureFailed);
			if (scatter->count == 0)
				break;
			pagesConsumed += scatter->count;
		}
		if (!contains((*this)[pagesConsumed-1], hashSize))	// referenced too many main hash slots
			MacOSError::throwMe(errSecCSSignatureFailed);
	}
	
	// check consistency between the page-coverage fields
	size_t limit = signingLimit();
	if (pageSize) {
		if (limit == 0)									// can't have paged signatures with no covered data
			MacOSError::throwMe(errSecCSSignatureFailed);
		size_t coveredPages = ((limit-1) >> pageSize) + 1; // page slots required to cover signingLimit
		if (coveredPages != nCodeSlots)
			MacOSError::throwMe(errSecCSSignatureFailed);
	} else {
		if ((limit > 0) != nCodeSlots)	// must have one code slot, or none if no code
			MacOSError::throwMe(errSecCSSignatureFailed);
	}
}


//
// Validate a slot against data in memory.
//
bool CodeDirectory::validateSlot(const void *data, size_t length, Slot slot) const
{
	secinfo("codedir", "%p validating slot %d", this, int(slot));
	MakeHash<CodeDirectory> hasher(this);
	Hashing::Byte digest[hasher->digestLength()];
	generateHash(hasher, data, length, digest);
	return memcmp(digest, (*this)[slot], hasher->digestLength()) == 0;
}


//
// Validate a slot against the contents of an open file. At most 'length' bytes
// will be read from the file.
//
bool CodeDirectory::validateSlot(FileDesc fd, size_t length, Slot slot) const
{
	MakeHash<CodeDirectory> hasher(this);
	Hashing::Byte digest[hasher->digestLength()];
	generateHash(hasher, fd, digest, length);
	return memcmp(digest, (*this)[slot], hasher->digestLength()) == 0;
}


//
// Check whether a particular slot is present.
// Absense is indicated by either a zero hash, or by lying outside
// the slot range.
//
bool CodeDirectory::slotIsPresent(Slot slot) const
{
	if (slot >= -Slot(nSpecialSlots) && slot < Slot(nCodeSlots)) {
		const Hashing::Byte *digest = (*this)[slot];
		for (unsigned n = 0; n < hashSize; n++)
			if (digest[n])
				return true;	// non-zero digest => present
	}
	return false;	// absent
}


//
// Given a hash type code, create an appropriate subclass of DynamicHash
// and return it. The caller owns the object and  must delete it when done.
// This function never returns NULL. It throws if the hashType is unsuupported,
// or if there's an error creating the hasher.
//
DynamicHash *CodeDirectory::hashFor(HashAlgorithm hashType)
{
	switch (hashType) {
	case kSecCodeSignatureHashSHA1:						return new CCHashInstance(kCCDigestSHA1);
	case kSecCodeSignatureHashSHA256:					return new CCHashInstance(kCCDigestSHA256);
	case kSecCodeSignatureHashSHA384:					return new CCHashInstance(kCCDigestSHA384);
	case kSecCodeSignatureHashSHA256Truncated:			return new CCHashInstance(kCCDigestSHA256, SHA1::digestLength);
	default:
		MacOSError::throwMe(errSecCSSignatureUnsupported);
	}
}
	
	
//
// Determine which of a set of possible digest types should be chosen as the "best" one
//
static const CodeDirectory::HashAlgorithm hashPriorities[] = {
	kSecCodeSignatureHashSHA384,
	kSecCodeSignatureHashSHA256,
	kSecCodeSignatureHashSHA256Truncated,
	kSecCodeSignatureHashSHA1,
	kSecCodeSignatureNoHash		// sentinel
};
	
bool CodeDirectory::viableHash(HashAlgorithm type)
{
	for (const HashAlgorithm* tp = hashPriorities; *tp != kSecCodeSignatureNoHash; tp++)
		if (*tp == type)
			return true;
	return false;

}

CodeDirectory::HashAlgorithm CodeDirectory::bestHashOf(const HashAlgorithms &types)
{
	for (const HashAlgorithm* type = hashPriorities; *type != kSecCodeSignatureNoHash; type++)
		if (types.find(*type) != types.end())
			return *type;
	MacOSError::throwMe(errSecCSUnsupportedDigestAlgorithm);
}
	

//
// Hash a file range with multiple digest algorithms and then pass the resulting
// digests to a per-algorithm block.
//
void CodeDirectory::multipleHashFileData(FileDesc fd, size_t limit, CodeDirectory::HashAlgorithms types, void (^action)(HashAlgorithm type, DynamicHash* hasher))
{
	assert(!types.empty());
	vector<RefPointer<DynamicHash> > hashers;
	for (auto it = types.begin(); it != types.end(); ++it) {
		if (CodeDirectory::viableHash(*it))
			hashers.push_back(CodeDirectory::hashFor(*it));
	}
	scanFileData(fd, limit, ^(const void *buffer, size_t size) {
		unsigned n = 0;
		for (auto it = types.begin(); it != types.end(); ++it, ++n) {
			hashers[n]->update(buffer, size);
		}
	});
	CFRef<CFMutableDictionaryRef> result = makeCFMutableDictionary();
	unsigned n = 0;
	for (auto it = types.begin(); it != types.end(); ++it, ++n) {
		action(*it, hashers[n]);
	}
}
    
    
    //
    // Hash data in memory using our hashAlgorithm()
    //
bool CodeDirectory::verifyMemoryContent(CFDataRef data, const Byte* digest) const
{
    RefPointer<DynamicHash> hasher = CodeDirectory::hashFor(this->hashType);
    hasher->update(CFDataGetBytePtr(data), CFDataGetLength(data));
    return hasher->verify(digest);
}
	
	
//
// Generate the canonical cdhash - the internal hash of the CodeDirectory itself.
// We currently truncate to 20 bytes because that's what the kernel can deal with.
//
CFDataRef CodeDirectory::cdhash() const
{
	MakeHash<CodeDirectory> hash(this);
	Hashing::Byte digest[hash->digestLength()];
	hash->update(this, this->length());
	hash->finish(digest);
	return makeCFData(digest, min(hash->digestLength(), size_t(kSecCodeCDHashLength)));
}


//
// Hash the next limit bytes of a file and return the digest.
// If the file is shorter, hash as much as you can.
// Limit==0 means unlimited (to end of file).
// Return how many bytes were actually hashed.
// Throw on any errors.
//
size_t CodeDirectory::generateHash(DynamicHash *hasher, FileDesc fd, Hashing::Byte *digest, size_t limit)
{
	size_t size = hashFileData(fd, hasher, limit);
	hasher->finish(digest);
	return size;
}


//
// Ditto, but hash a memory buffer instead.
//
size_t CodeDirectory::generateHash(DynamicHash *hasher, const void *data, size_t length, Hashing::Byte *digest)
{
	hasher->update(data, length);
	hasher->finish(digest);
	return length;
}


//
// Turn a hash of canonical type into a hex string
//
std::string CodeDirectory::hexHash(const unsigned char *hash) const
{
	size_t size = this->hashSize;
	char result[2*size+1];
	for (unsigned n = 0; n < size; n++)
		sprintf(result+2*n, "%02.2x", hash[n]);
	return result;
}


//
// Generate a screening code string from a (complete) CodeDirectory.
// This can be used to make a lightweight pre-screening code from (just) a CodeDirectory.
//
std::string CodeDirectory::screeningCode() const
{
	if (slotIsPresent(-cdInfoSlot))		// has Info.plist
		return "I" + hexHash((*this)[-cdInfoSlot]); // use Info.plist hash
	if (slotIsPresent(-cdRepSpecificSlot))		// has Info.plist
		return "R" + hexHash((*this)[-cdRepSpecificSlot]); // use Info.plist hash
	if (pageSize == 0)					// good-enough proxy for "not a Mach-O file"
		return "M" + hexHash((*this)[0]); // use hash of main executable
	return "N";							// no suitable screening code
}


}	// CodeSigning
}	// Security


//
// Canonical text form for user-settable code directory flags.
// Note: This table is actually exported from Security.framework.
//
const SecCodeDirectoryFlagTable kSecCodeDirectoryFlagTable[] = {
	{ "host",		kSecCodeSignatureHost,			true },
	{ "adhoc",		kSecCodeSignatureAdhoc,			false },
	{ "hard",		kSecCodeSignatureForceHard,		true },
	{ "kill",		kSecCodeSignatureForceKill,		true },
	{ "expires",		kSecCodeSignatureForceExpiration,	true },
	{ "restrict",		kSecCodeSignatureRestrict,		true },
	{ "enforcement",	kSecCodeSignatureEnforcement,		true },
	{ "library-validation", kSecCodeSignatureLibraryValidation,		true },
	{ NULL }
};
