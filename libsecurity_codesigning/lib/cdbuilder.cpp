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
// cdbuilder - constructor for CodeDirectories
//
#include "cdbuilder.h"
#include <security_utilities/memutils.h>
#include <cmath>

using namespace UnixPlusPlus;
using LowLevelMemoryUtilities::alignUp;


namespace Security {
namespace CodeSigning {


//
// Create an (empty) builder
//
CodeDirectory::Builder::Builder()
	: mFlags(0), mSpecialSlots(0), mCodeSlots(0), mDir(NULL)
{
	memset(mSpecial, 0, sizeof(mSpecial));
}


//
// Set the source of the main executable (i.e. the code pages)
//
void CodeDirectory::Builder::executable(string path,
	size_t pagesize, size_t offset, size_t length)
{
	mExec.close();			// any previously opened one
	mExec.open(path);
	mPageSize = pagesize;
	mExecOffset = offset;
	mExecLength = length;
}

void CodeDirectory::Builder::reopen(string path, size_t offset, size_t length)
{
	assert(mExec);					// already called executable()
	mExec.close();
	mExec.open(path);
	mExecOffset = offset;
	mExecLength = length;
}


//
// Set the source for one special slot
//
void CodeDirectory::Builder::special(size_t slot, CFDataRef data)
{
	assert(slot <= cdSlotMax);
	Hash hash;
	hash(CFDataGetBytePtr(data), CFDataGetLength(data));
	hash.finish(mSpecial[slot]);
	if (slot >= mSpecialSlots)
		mSpecialSlots = slot;
}


size_t CodeDirectory::Builder::size()
{
	assert(mExec);			// must have called executable()
	if (mExecLength == 0)
		mExecLength = mExec.fileSize() - mExecOffset;

	// how many code pages?
	if (mPageSize == 0) {	// indefinite - one page
		mCodeSlots = (mExecLength > 0);
	} else {				// finite - calculate from file size
		mCodeSlots = (mExecLength + mPageSize - 1) / mPageSize; // round up
	}
		
	size_t offset = sizeof(CodeDirectory);
	offset += mIdentifier.size() + 1;	// size of identifier (with null byte)
	offset += (mCodeSlots + mSpecialSlots) * Hash::digestLength; // hash vector
	return offset;
}


//
// Take everything added to date and wrap it up in a shiny new CodeDirectory.
//
// Note that this doesn't include or generate the signature. You're free to
// modify the result. But this function determines the memory layout, so the
// sizes and counts should be correct on entry.
//
CodeDirectory *CodeDirectory::Builder::build()
{
	assert(mExec);			// must have (successfully) called executable()

	// size and allocate
	size_t identLength = mIdentifier.size() + 1;
	size_t total = size();
	if (!(mDir = (CodeDirectory *)calloc(1, total)))	// initialize to zero
		UnixError::throwMe(ENOMEM);
	
	// fill header
	mDir->initialize(total);
	mDir->version = currentVersion;
	mDir->flags = mFlags;
	mDir->nSpecialSlots = mSpecialSlots;
	mDir->nCodeSlots = mCodeSlots;
	mDir->codeLimit = mExecLength;
	mDir->hashSize = Hash::digestLength;
	mDir->hashType = cdHashTypeDefault;
	if (mPageSize) {
		int pglog;
		assert(frexp(mPageSize, &pglog) == 0.5); // must be power of 2
		frexp(mPageSize, &pglog);
		assert(pglog < 256);
		mDir->pageSize = pglog - 1;
	} else
		mDir->pageSize = 0;	// means infinite page size

	// locate and fill flex fields
	size_t offset = sizeof(CodeDirectory);
	mDir->identOffset = offset;
	memcpy(mDir->identifier(), mIdentifier.c_str(), identLength);
	offset += identLength;

	// (add new flexibly-allocated fields here)

	mDir->hashOffset = offset + mSpecialSlots * Hash::digestLength;
	offset += (mSpecialSlots + mCodeSlots) * Hash::digestLength;
	assert(offset == total);	// matches allocated size
	
	// fill special slots
	memset((*mDir)[-mSpecialSlots], 0, mDir->hashSize * mSpecialSlots);
	for (size_t slot = 1; slot <= mSpecialSlots; ++slot)
		memcpy((*mDir)[-slot], &mSpecial[slot], Hash::digestLength);
	
	// fill code slots
	mExec.seek(mExecOffset);
	size_t remaining = mExecLength;
	for (unsigned int slot = 0; slot < mCodeSlots; ++slot) {
		size_t thisPage = min(mPageSize, remaining);
		hash(mExec, (*mDir)[slot], thisPage);
		remaining -= thisPage;
	}
	
	// all done. Pass ownership to caller
	return mDir;
}


}	// CodeSigning
}	// Security
