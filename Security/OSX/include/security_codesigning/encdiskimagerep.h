/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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
#ifndef _H_ENCDISKIMAGEREP
#define _H_ENCDISKIMAGEREP

#include "singlediskrep.h"
#include "sigblob.h"
#include <DiskImages/DiskImages.h>
#undef check	// sadness is having to live with C #defines of this kind...
#include <security_utilities/unix++.h>

namespace Security {
namespace CodeSigning {

/// The serialized format of an encrypted disk image header, taken from CEncryptedEncoding.h
/// The overall format can be roughly described as:
///		__Encrypted_Header_V2
///		AuthTable
///			uint32_t - count of table entries
///			AuthTableEntry
///			...
///			AuthTableEntry
///		Authentication body - arbitrary data
///			NOTE: disk images are created with a large amount of padding here (~100kB), which prevents
///			the need to push out the actual data when adding small bits of data.  All data here is referenced by
///			offsets within the AuthTableEntries.
///		Start of disk image from dataForkOffset
struct __Encrypted_Header_V2 {
	uint32_t				signature1;
	uint32_t				signature2;
	uint32_t				version;

	uint32_t				encryptionIVSize;
	CSSM_ENCRYPT_MODE		encryptionMode;
	CSSM_ALGORITHMS			encryptionAlgorithm;
	uint32_t				encryptionKeySizeInBits;
	CSSM_ALGORITHMS			prngAlgorithm;
	uint32_t				prngKeySizeInBits;

	CFUUIDBytes				uuid;

	uint32_t				dataBlockSize;
	di_filepos_t			dataForkSize;
	di_filepos_t			dataForkStartOffset;
} __attribute__((packed));

struct __AuthTableEntry {
	uint32_t				mechanism;
	di_filepos_t			offset;
	di_filepos_t			length;
} __attribute__((packed));

/// Represents a single authentication table entry, inside an AuthTable, from an encrypted disk image header.
class AuthTableEntry {
public:
	AuthTableEntry(UnixPlusPlus::FileDesc &fd);
	AuthTableEntry(uint32_t mechanism, uint64_t offset, uint64_t length);
	~AuthTableEntry();

	void loadData(UnixPlusPlus::FileDesc &fd);
	void setOffset(uint64_t newOffset);
	void setData(void *data, size_t length);
	void serialize(UnixPlusPlus::FileDesc &fd);

	uint32_t mechanism() 	{ return mMechanism; }
	uint64_t offset() 		{ return mOffset; }
	uint64_t length() 		{ return mLength; }

private:
	void clearData();

private:
	uint32_t mMechanism;
	uint64_t mOffset;
	uint64_t mLength;
	bool mFreeData;
	void *mData;
};

/// Represents the authentication table inside an encrypted disk image header.
class AuthTable {
public:
	AuthTable(UnixPlusPlus::FileDesc &fd);
	AuthTable() { }
	~AuthTable() { }

	std::vector<std::shared_ptr<AuthTableEntry>> &getEntries() { return mEntries; }
	void serialize(UnixPlusPlus::FileDesc &fd);
	void addEntry(uint32_t mechanism, void *data, size_t length);
	void prepareEntries();
	uint64_t findFirstEmptyDataOffset();

private:
	std::vector<std::shared_ptr<AuthTableEntry>> mEntries;
};

class EncDiskImageRep : public SingleDiskRep {
public:
	EncDiskImageRep(const char *path);
	virtual ~EncDiskImageRep();

	CFDataRef identification();
	CFDataRef component(CodeDirectory::SpecialSlot slot);
	size_t signingLimit();
	size_t signingBase();
	void strictValidate(const CodeDirectory* cd, const ToleratedErrors& tolerated, SecCSFlags flags);
	std::string format();
	void prepareForSigning(SigningContext& state);

	static bool candidate(UnixPlusPlus::FileDesc &fd);
	void registerStapledTicket();

	static CFDataRef identificationFor(MachO *macho);
	void flush();

	DiskRep::Writer *writer();
	class Writer;
	friend class Writer;

private:
	void setup();
	static bool readHeader(UnixPlusPlus::FileDesc& fd, struct __Encrypted_Header_V2& header);

private:
	struct __Encrypted_Header_V2 mHeader;		// disk image header (all fields NBO)
	const EmbeddedSignatureBlob *mSigningData;	// pointer to signature SuperBlob (malloc'd memory during setup)
	AuthTable 					 mAuthTable;
};

class EncDiskImageRep::Writer : public SingleDiskRep::Writer, private EmbeddedSignatureBlob::Maker {
	friend class EncDiskImageRep;
public:
	Writer(EncDiskImageRep *r) : SingleDiskRep::Writer(r, writerNoGlobal), rep(r), mSigningData(NULL) { }
	void component(CodeDirectory::SpecialSlot slot, CFDataRef data);
	void flush();
	void remove();
	void addDiscretionary(CodeDirectory::Builder &builder) { }

private:
	EncDiskImageRep *rep;
	EmbeddedSignatureBlob *mSigningData;
};

} // end namespace CodeSigning
} // end namespace Security

#endif /* _H_ENCDISKIMAGEREP */
