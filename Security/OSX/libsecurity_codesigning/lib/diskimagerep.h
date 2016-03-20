/*
 * Copyright (c) 20015 Apple Inc. All Rights Reserved.
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
// diskimagerep - DiskRep representing a single read-only compressed disk image file
//
#ifndef _H_DISKIMAGEREP
#define _H_DISKIMAGEREP

#include "singlediskrep.h"
#include "sigblob.h"
#include <DiskImages/DiskImages.h>
#undef check	// sadness is having to live with C #defines of this kind...
#include <security_utilities/unix++.h>

namespace Security {
namespace CodeSigning {


//
// DiskImageRep implements a single read-only compressed disk image file.
//
class DiskImageRep : public SingleDiskRep {
public:
	DiskImageRep(const char *path);
	
	CFDataRef identification();
	CFDataRef component(CodeDirectory::SpecialSlot slot);
	size_t signingLimit();
	void strictValidate(const CodeDirectory* cd, const ToleratedErrors& tolerated, SecCSFlags flags);
	std::string format();
	void prepareForSigning(SigningContext& state);
	
	static bool candidate(UnixPlusPlus::FileDesc &fd);
	
public:
	static CFDataRef identificationFor(MachO *macho);
	
public:
	DiskRep::Writer *writer();
	class Writer;
	friend class Writer;

private:
	void setup();
	static bool readHeader(UnixPlusPlus::FileDesc& fd, UDIFFileHeader& header);

private:
	UDIFFileHeader mHeader;						// disk image header (all fields NBO)
	size_t mEndOfDataOffset;					// end of payload data (data fork + XML)
	size_t mHeaderOffset;						// trailing header offset
	const EmbeddedSignatureBlob *mSigningData;	// pointer to signature SuperBlob (in mapped memory)
};


//
// The write side of a FileDiskRep
//
class DiskImageRep::Writer : public SingleDiskRep::Writer, private EmbeddedSignatureBlob::Maker {
	friend class FileDiskRep;
public:
	Writer(DiskImageRep *r) : SingleDiskRep::Writer(r, writerNoGlobal), rep(r), mSigningData(NULL) { }
	void component(CodeDirectory::SpecialSlot slot, CFDataRef data);
	void flush();
	void addDiscretionary(CodeDirectory::Builder &builder);
	
private:
	DiskImageRep *rep;
	EmbeddedSignatureBlob *mSigningData;
};


} // end namespace CodeSigning
} // end namespace Security

#endif // !_H_DISKIMAGEREP
