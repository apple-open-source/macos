/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
#include "CSCommon.h"

using namespace UnixPlusPlus;


namespace Security {
namespace CodeSigning {


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
	case cdResourceDirSlot:
		return kSecCS_RESOURCEDIRFILE;
	case cdCodeDirectorySlot:
		return kSecCS_CODEDIRECTORYFILE;
	case cdSignatureSlot:
		return kSecCS_SIGNATUREFILE;
	case cdApplicationSlot:
		return kSecCS_APPLICATIONFILE;
	case cdEntitlementSlot:
		return kSecCS_ENTITLEMENTFILE;
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
		return cdComponentPerArchitecture | cdComponentIsBlob;
	case cdSignatureSlot:
		return cdComponentPerArchitecture; // raw
	case cdEntitlementSlot:
		return cdComponentIsBlob; // global
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
	"application"
};
#endif //NDEBUG


//
// Check the version of this CodeDirectory for basic sanity.
// Throws if the directory is corrupted or out of versioning bounds.
// Returns if the version is usable (perhaps with degraded features due to
// compatibility hacks).
//
void CodeDirectory::checkVersion() const
{
	if (!this->validateBlob())
		MacOSError::throwMe(errSecCSSignatureInvalid);	// busted
	if (version > compatibilityLimit)
		MacOSError::throwMe(errSecCSSignatureUnsupported);	// too new - no clue
	if (version > currentVersion)
		secdebug("codedir", "%p version 0x%x newer than current 0x%x",
			this, uint32_t(version), currentVersion);
}


//
// Validate a slot against data in memory.
//
bool CodeDirectory::validateSlot(const void *data, size_t length, Slot slot) const
{
	secdebug("codedir", "%p validating slot %d", this, int(slot));
	Hash::Byte digest[Hash::digestLength];
	hash(data, length, digest);
	return memcmp(digest, (*this)[slot], Hash::digestLength) == 0;
}


//
// Validate a slot against the contents of an open file. At most 'length' bytes
// will be read from the file.
//
bool CodeDirectory::validateSlot(FileDesc fd, size_t length, Slot slot) const
{
	Hash::Digest digest;
	hash(fd, digest, length);
	return memcmp(digest, (*this)[slot], Hash::digestLength) == 0;
}


//
// Check whether a particular slot is present.
// Absense is indicated by either a zero hash, or by lying outside
// the slot range.
//
bool CodeDirectory::slotIsPresent(Slot slot) const
{
	if (slot >= -Slot(nSpecialSlots) && slot < Slot(nCodeSlots)) {
		const Hash::Byte *digest = (*this)[slot];
		for (unsigned n = 0; n < Hash::digestLength; n++)
			if (digest[n])
				return true;	// non-zero digest => present
	}
	return false;	// absent
}


//
// Hash the next limit bytes of a file and return the digest.
// If the file is shorter, hash as much as you can.
// Limit==0 means unlimited (to end of file).
// Return how many bytes were actually hashed.
// Throw on any errors.
//
size_t CodeDirectory::hash(FileDesc fd, Hash::Byte *digest, size_t limit)
{
	IFDEBUG(size_t hpos = fd.position());
	IFDEBUG(size_t hlimit = limit);
	unsigned char buffer[4096];
	Hash hash;
	size_t total = 0;
	for (;;) {
		size_t size = sizeof(buffer);
		if (limit && limit < size)
			size = limit;
		size_t got = fd.read(buffer, size);
		total += got;
		if (fd.atEnd())
			break;
		hash(buffer, got);
		if (limit && (limit -= got) == 0)
			break;
	}
	hash.finish(digest);
	secdebug("cdhash", "fd %d %zd@0x%zx => %2x.%2x.%2x...",
		fd.fd(), hpos, hlimit, digest[0], digest[1], digest[2]);
	return total;
}


//
// Ditto, but hash a memory buffer instead.
//
size_t CodeDirectory::hash(const void *data, size_t length, Hash::Byte *digest)
{
	Hash hash;
	hash(data, length);
	hash.finish(digest);
	return length;
}


//
// Canonical text form for user-settable code directory flags
//
const CodeDirectory::FlagItem CodeDirectory::flagItems[] = {
	{ "host",		kSecCodeSignatureHost,			true },
	{ "adhoc",		kSecCodeSignatureAdhoc,			false },
	{ "hard",		kSecCodeSignatureForceHard,		true },
	{ "kill",		kSecCodeSignatureForceKill,		true },
	{ "expires",	kSecCodeSignatureForceExpiration, true },
	{ NULL }
};


//
// Parse a canonical text description of code flags, in the form
//	flag,...,flag
// where each flag can be a prefix of a known flag name.
// Internally set flags are not accepted.
//
uint32_t CodeDirectory::textFlags(std::string text)
{
	uint32_t flags = 0;
	for (string::size_type comma = text.find(','); ; text = text.substr(comma+1), comma = text.find(',')) {
		string word = (comma == string::npos) ? text : text.substr(0, comma);
		const CodeDirectory::FlagItem *item;
		for (item = CodeDirectory::flagItems; item->name; item++)
			if (item->external && !strncmp(word.c_str(), item->name, word.size())) {
				flags |= item->value;
				break;
			}
		if (!item)	// not found
			MacOSError::throwMe(errSecCSInvalidFlags);
		if (comma == string::npos)	// last word
			break;
	}
	return flags;
}


}	// CodeSigning
}	// Security
