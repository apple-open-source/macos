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
// A CodeDirectory is a contiguous binary blob containing a two-tiered
// hash of (much of) the contents of a real StaticCode object.
// It consists of a header followed by an array of hash vector elements.
//
// This structure is meant to be self-describing, binary stable, and endian independent.
//
#ifndef _H_CODEDIRECTORY
#define _H_CODEDIRECTORY

#include <security_utilities/unix++.h>
#include <security_utilities/blob.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/hashing.h>


namespace Security {
namespace CodeSigning {


//
// Types of hashes supported.
// Actually, right now, only SHA1 is really supported.
//
enum {
	cdHashTypeSHA1 = 1,
	cdHashTypeSHA256 = 2,
	
	cdHashTypeDefault = cdHashTypeSHA1
};


//
// Conventional string names for various code signature components.
// Depending on storage, these may end up as filenames, extended attribute names, etc.
//
#define kSecCS_CODEDIRECTORYFILE	"CodeDirectory"		// CodeDirectory
#define kSecCS_SIGNATUREFILE		"CodeSignature"		// CMS Signature
#define kSecCS_REQUIREMENTSFILE		"CodeRequirements"	// internal requirements
#define kSecCS_RESOURCEDIRFILE		"CodeResources"		// resource directory
#define kSecCS_APPLICATIONFILE		"CodeApplication"	// application-specific resource
#define kSecCS_ENTITLEMENTFILE		"CodeEntitlements"	// entitlement configuration (just in case)


//
// Special hash slot values. In a CodeDirectory, these show up at negative slot
// indices. This enumeration is also used widely in various internal APIs, and as
// type values in embedded SuperBlobs.
//
enum {
	//
	// Primary slot numbers.
	// These values are potentially present in the CodeDirectory hash array
	// under their negative values. They are also used in APIs and SuperBlobs.
	// Note that zero must not be used for these (it's page 0 of the main code array),
	// and it's good to assign contiguous (very) small values for them.
	//
	cdInfoSlot = 1,						// Info.plist
	cdRequirementsSlot = 2,				// internal requirements
	cdResourceDirSlot = 3,				// resource directory
	cdApplicationSlot = 4,				// Application specific slot
	cdEntitlementSlot = 5,				// embedded entitlement configuration
	// (add further primary slot numbers here)

	cdSlotCount,						// total number of special slots (+1 for slot 0)
	cdSlotMax = cdSlotCount - 1,		// highest special slot number
	
	//
	// Virtual slot numbers.
	// These values are NOT used in the CodeDirectory hash array. The are used as
	// internal API identifiers and as types in in-image SuperBlobs.
	// Zero is okay to use here; and we assign that to the CodeDirectory itself so
	// it shows up first in (properly sorted) SuperBlob indices. The rest of the
	// numbers is set Far Away so the primary slot set can expand safely.
	// It's okay to have gaps in these assignments.
	//
	cdCodeDirectorySlot = 0,			// CodeDirectory
	cdSignatureSlot = 0x10000,			// CMS signature
	// (add further virtual slot numbers here)
};


//
// Special hash slot attributes.
// This is a central description of attributes of each slot.
// Various places in Code Signing pick up those attributes and act accordingly.
//
enum {
	cdComponentPerArchitecture = 1,			// slot value differs for each Mach-O architecture
	cdComponentIsBlob = 2,					// slot value is a Blob (need not be BlobWrapped)
};


//
// A CodeDirectory is a typed Blob describing the secured pieces of a program.
// This structure describes the common header and provides access to the variable-size
// elements packed after it. For help in constructing a CodeDirectory, use the nested
// Builder class.
//
// The hashes are stored as an array of digests. The array covers the range
// [-nSpecialSlots .. nCodeSlots-1]. Non-negative indices  denote pages of the main
// executable. Negative indices indicate "special" hashes, each of a different thing
// (see cd*Slot constants above). Special slots that are in range but not present
// are zeroed out. Unallocated special slots are also presumed absent; this is not
// an error. (Thus the range of special slots can be extended at will.)
//
// HOW TO MANAGE COMPATIBILITY:
// Each CodeDirectory has a format (compatibility) version. Three constants control
// versioning:
//	* currentVersion is the version used for newly created CodeDirectories.
//  * compatibilityLimit is the highest version the code will accept as compatible.
// Test for version < currentVersion to detect old formats that may need special
// handling. The current code rejects those; add backward cases to checkVersion().
// Break backward compatibility by rejecting versions that are unsuitable.
// Accept currentVersion < version <= compatibilityLimit as versions newer than
// those understood by this code but engineered (by newer code) to be backward
// compatible. Reject version > compatibilityLimit as incomprehensible gibberish.
//
// When creating a new version, increment currentVersion. When adding new fixed fields,
// just append them; the flex fields will shift to make room. To add new flex fields,
// add a fixed field containing the new field's offset and add suitable computations
// to the Builder to place the new data (right) before the hash array. Older code will
// then simply ignore your new fields on load/read.
// Add flag bits to the existing flags field to add features that step outside
// of the linear versioning stream. Leave the 'spare' fields alone unless you need
// something extraordinarily weird - they're meant to be the final escape when everything
// else fails.
// As you create new versions, consider moving the compatibilityLimit out to open up
// new room for backward compatibility.
// To break backward compatibility intentionally, move currentVersion beyond the
// old compatibilityLimit (and move compatibilityLimit further out).
//
class CodeDirectory: public Blob<CodeDirectory, 0xfade0c02> {
	typedef SHA1 Hash;
public:
	Endian<uint32_t> version;		// compatibility version
	Endian<uint32_t> flags;			// setup and mode flags
	Endian<uint32_t> hashOffset;	// offset of hash slot element at index zero
	Endian<uint32_t> identOffset;	// offset of identifier string
	Endian<uint32_t> nSpecialSlots;	// number of special hash slots
	Endian<uint32_t> nCodeSlots;	// number of ordinary (code) hash slots
	Endian<uint32_t> codeLimit;		// limit to main image signature range
	uint8_t hashSize;				// size of each hash in bytes
	uint8_t hashType;				// type of hash (cdHashType* constants)
	uint8_t spare1;					// unused (must be zero)
	uint8_t	pageSize;				// log2(page size in bytes); 0 => infinite
	Endian<uint32_t> spare2;		// unused (must be zero)
	
	// works with the version field; see comments above
	static const uint32_t currentVersion = 0x20001;		// "version 2"
	static const uint32_t compatibilityLimit = 0x2F000;	// "version 3 with wiggle room"
	
	void checkVersion() const;		// throws if not compatible with this code

	typedef int Slot;				// slot index (negative for special slots)
	typedef unsigned int SpecialSlot; // positive special slot index (not for code slots)
	
	const char *identifier() const { return at<const char>(identOffset); }
	char *identifier() { return at<char>(identOffset); }

	unsigned char *operator [] (Slot slot)
	{
		assert(slot >= int(-nSpecialSlots) && slot < int(nCodeSlots));
		return at<unsigned char>(hashOffset) + hashSize * slot;
	}
	
	const unsigned char *operator [] (Slot slot) const
	{
		assert(slot >= int(-nSpecialSlots) && slot < int(nCodeSlots));
		return at<unsigned char>(hashOffset) + hashSize * slot;
	}
	
	bool validateSlot(const void *data, size_t size, Slot slot) const;
	bool validateSlot(UnixPlusPlus::FileDesc fd, size_t size, Slot slot) const;
	bool slotIsPresent(Slot slot) const;
	
	class Builder;
	
protected:
	static size_t hash(UnixPlusPlus::FileDesc fd, Hash::Byte *digest, size_t limit = 0);
	static size_t hash(const void *data, size_t length, Hash::Byte *digest);
	
public:
	//
	// Information about SpecialSlots
	//
	static const char *canonicalSlotName(SpecialSlot slot);
	static unsigned slotAttributes(SpecialSlot slot);
	IFDEBUG(static const char * const debugSlotName[]);
	
public:
	//
	// Canonical text forms for (only) the user-settable flags
	//
	struct FlagItem {
		const char *name;
		uint32_t value;
		bool external;
	};
	static const FlagItem flagItems[];	// terminated with NULL item

	static uint32_t textFlags(std::string text);
};


}	// CodeSigning
}	// Security


#endif //_H_CODEDIRECTORY
