/*
 * Copyright (c) 2007 Apple Inc. All Rights Reserved.
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
// foreigndiskrep - foreign executable disk representation
//
#ifndef _H_FOREIGNDISKREP
#define _H_FOREIGNDISKREP

#include "singlediskrep.h"
#include "sigblob.h"
#include "signerutils.h"
#include <security_utilities/unix++.h>
#include <security_utilities/cfutilities.h>

namespace Security {
namespace CodeSigning {


//
//
//
class ForeignDiskRep : public SingleDiskRep {
public:
	ForeignDiskRep(const char *path);
	~ForeignDiskRep();
	
	CFDataRef component(CodeDirectory::SpecialSlot slot);
	size_t pageSize();
	std::string format();
	void flush();
	
	static bool candidate(UnixPlusPlus::FileDesc &fd); // could this reasonably be a CFM code?

public:
	DiskRep::Writer *writer();
	class Writer;
	friend class Writer;
	
protected:
	void readSigningData();					// read and cache signing data
	string cspath();						// path to sidecar
	
private:
	bool mTriedRead;						// tried to get signing data
	size_t mSigningOffset;					// where we found the signing data
	EmbeddedSignatureBlob *mSigningData;	// cached signing data
};


//
// The write side of a FileDiskRep
//
class ForeignDiskRep::Writer : public DiskRep::Writer, private EmbeddedSignatureBlob::Maker {
	friend class ForeignDiskRep;
public:
	Writer(ForeignDiskRep *r) : rep(r), mSigningData(NULL) { }
	~Writer();
	
	void component(CodeDirectory::SpecialSlot slot, CFDataRef data);
	virtual void flush();

protected:
	RefPointer<ForeignDiskRep> rep;
	EmbeddedSignatureBlob *mSigningData;
};


} // end namespace CodeSigning
} // end namespace Security

#endif // !_H_FOREIGNDISKREP
