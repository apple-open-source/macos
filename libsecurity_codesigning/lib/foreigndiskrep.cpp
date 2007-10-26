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
#include "foreigndiskrep.h"
#include <cstring>


namespace Security {
namespace CodeSigning {

using namespace UnixPlusPlus;


//
// Everything's lazy in here
//
ForeignDiskRep::ForeignDiskRep(const char *path)
	: SingleDiskRep(path), mTriedRead(false)
{
}

ForeignDiskRep::~ForeignDiskRep()
{
	if (mTriedRead)
		delete mSigningData;
}


//
// Foreign filter heuristic
//
bool ForeignDiskRep::candidate(FileDesc &fd)
{
	static const char magicMarker[] = "MZ\0\0\0\0\0\0\0\0\0\0PE\0\0 \b";
	static const size_t magicLength = 18;
	char marker[magicLength];
	return fd.read(marker, magicLength, 0) == magicLength
		&& !memcmp(marker, magicMarker, magicLength);
}


//
// Extract and return a component by slot number.
// If we have a Mach-O binary, use embedded components.
// Otherwise, look for and return the extended attribute, if any.
//
CFDataRef ForeignDiskRep::component(CodeDirectory::SpecialSlot slot)
{
	if (!mTriedRead)
		readSigningData();
	if (mSigningData)
		return mSigningData->component(slot);
	else
		return NULL;
}


//
// Default to system-paged signing
//
size_t ForeignDiskRep::pageSize()
{
	return segmentedPageSize;
}


//
// Various other aspects of our DiskRep personality.
//
string ForeignDiskRep::format()
{
	return "foreign binary";
}


//
// Discard cached information
//
void ForeignDiskRep::flush()
{
	mTriedRead = false;
	::free(mSigningData);
}


//
// Locate, read, and cache embedded signing data from the foreign binary.
//
void ForeignDiskRep::readSigningData()
{
	if (!mTriedRead) {				// try it once
		mSigningData = NULL;		// preset failure
		mTriedRead = true;			// we've tried (and perhaps failed)
		
		AutoFileDesc fd(cspath(), O_RDONLY);
		mSigningData = EmbeddedSignatureBlob::readBlob(fd);
		if (mSigningData)
			secdebug("foreignrep", "%zd signing bytes in %d blob(s) from %s(foreign)",
				mSigningData->length(), mSigningData->count(),
				mainExecutablePath().c_str());
		else
			secdebug("foreignrep", "failed to read signing bytes from %s(foreign)",
				mainExecutablePath().c_str());
	}
}


//
// Generate the path to the (default) sidecar file
// This is generated as /path/to/.CS.execfilename.
// We're assuming that we're only dealing with absolute paths here.
//
string ForeignDiskRep::cspath()
{
	string p = this->path();
	string::size_type slash = p.rfind('/');
	assert(slash != string::npos);
	return p.substr(0, slash+1) + ".CS." + p.substr(slash+1);	// => /path/to/.CS.executable
}


//
// ForeignDiskRep::Writers
//
DiskRep::Writer *ForeignDiskRep::writer()
{
	return new Writer(this);
}

ForeignDiskRep::Writer::~Writer()
{
	delete mSigningData;
}


//
// Write a component.
// Note that this isn't concerned with Mach-O writing; this is handled at
// a much higher level. If we're called, it's extended attribute time.
//
void ForeignDiskRep::Writer::component(CodeDirectory::SpecialSlot slot, CFDataRef data)
{
	EmbeddedSignatureBlob::Maker::component(slot, data);
}


//
// Append the superblob we built to the foreign binary.
// Note: Aligning the signing blob to a 16-byte boundary is not strictly necessary,
// but it's what the Mach-O case does, and it probably improves performance a bit.
//
void ForeignDiskRep::Writer::flush()
{
	delete mSigningData;			// ditch previous blob just in case
	mSigningData = Maker::make();	// assemble new signature SuperBlob
	AutoFileDesc fd(rep->cspath(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	fd.writeAll(mSigningData, mSigningData->length());
}


} // end namespace CodeSigning
} // end namespace Security
