/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// ReadWriteSection.h
//

#ifndef _H_APPLEDL_READWRITESECTION
#define _H_APPLEDL_READWRITESECTION

#include <Security/cssmalloc.h>
#include <Security/AtomicFile.h>
#include <Security/endian.h>

namespace Security
{

//
// Atom -- An Atom is a 32-bit unsigned integer value that is always internally
// represented using network byte order.
//
typedef Endian<uint32> Atom;

enum {
	AtomSize = sizeof(uint32) // XXX Why not just use sizeof(Atom)?
};

//
// Class representing a range (or subrange of a buffer).
//
class Range
{
public:
	Range(uint32 inOffset, uint32 inSize) : mOffset(inOffset), mSize(inSize) {}
	uint32 mOffset;
	uint32 mSize;
};

//
// Class representing a packed record.  All the accessors on this class are const since the
// underlying data is read-only 
//
// XXX Should be replaced by Atom::Vector
class ReadSection
{
protected:
    ReadSection(uint8 *inAddress, size_t inLength) : mAddress(inAddress), mLength(inLength) {}
public:
	ReadSection() : mAddress(NULL), mLength(0) {}
    ReadSection(const uint8 *inAddress, size_t inLength) :
	    mAddress(const_cast<uint8 *>(inAddress)), mLength(inLength) {}
		
    uint32 size() const { return mLength; }

    uint32 at(uint32 inOffset) const
    {
        return ntohl(*reinterpret_cast<const uint32 *>(mAddress + inOffset));
    }

    uint32 operator[](uint32 inOffset) const
    {
        if (inOffset + sizeof(uint32) > mLength)
            CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
        return at(inOffset);
    }

	// Return a subsection from inOffset to end of section.
    ReadSection subsection(uint32 inOffset) const
    {
        if (inOffset > mLength)
            CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
        return ReadSection(mAddress + inOffset, mLength - inOffset);
    }

	// Return a subsection from inOffset of inLength bytes.
    ReadSection subsection(uint32 inOffset, uint32 inLength) const
    {
        if (inOffset + inLength > mLength)
            CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
        return ReadSection(mAddress + inOffset, inLength);
    }
	
	ReadSection subsection(const Range &inRange) const
	{
		return subsection(inRange.mOffset, inRange.mSize);
	}

    const uint8 *range(const Range &inRange) const
    {
        if (inRange.mOffset + inRange.mSize > mLength)
            CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
        return mAddress + inRange.mOffset;
    }

	uint8 *allocCopyRange(const Range &inRange, CssmAllocator &inAllocator) const
	{
	    uint8 *aData;
	    if (inRange.mSize == 0)
	        aData = NULL;
	    else
	    {
	        if (inRange.mOffset + inRange.mSize > mLength)
	            CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	        aData = reinterpret_cast<uint8 *>(inAllocator.malloc(inRange.mSize));
	        memcpy(aData, mAddress + inRange.mOffset, inRange.mSize);
	    }

		return aData;
	}

	static uint32 align(uint32 offset) { return (offset + AtomSize - 1) & ~(AtomSize - 1); }

protected:
    uint8 *mAddress;
    size_t mLength;
};

//
// Class representing a packed record (or buffer) used for writing.
//
class WriteSection : public ReadSection
{
public:
	static const size_t DefaultCapacity = 64;

    WriteSection(CssmAllocator &inAllocator, size_t inCapacity) :
        ReadSection(reinterpret_cast<uint8 *>(inAllocator.malloc(inCapacity)), 0),
        mAllocator(inAllocator),
        mCapacity(inCapacity)
	{
		if (mCapacity > 0)
			memset(mAddress, 0, mCapacity);
	}

    WriteSection(CssmAllocator &inAllocator = CssmAllocator::standard()) :
        ReadSection(reinterpret_cast<uint8 *>(inAllocator.malloc(DefaultCapacity)), 0),
        mAllocator(inAllocator),
        mCapacity(DefaultCapacity)
	{
	}
	
	WriteSection(const WriteSection &ws, int length) :
		ReadSection(reinterpret_cast<uint8 *>(ws.mAllocator.malloc(length)), length),
		mAllocator(ws.mAllocator),
		mCapacity(length)
	{
		memcpy(mAddress, ws.mAddress, length);
	}

    ~WriteSection() { mAllocator.free(mAddress); }

private:
    void grow(size_t inNewCapacity)
    {
        size_t aNewCapacity = max(mCapacity * 2, inNewCapacity);
        mAddress = reinterpret_cast<uint8 *>(mAllocator.realloc(mAddress, aNewCapacity));
		memset(mAddress + mCapacity, 0, aNewCapacity - mCapacity);
        mCapacity = aNewCapacity;
    }

public:
#if BUG_GCC
	uint32 size() const { return ReadSection::size(); }
#else
	// XXX This should work but egcs-2.95.2 doesn't like it.
	using ReadSection::size;
#endif

    void size(uint32 inLength) { mLength = inLength; }
    uint32 put(uint32 inOffset, uint32 inValue)
    {
        uint32 aLength = inOffset + sizeof(inValue);
        if (aLength > mCapacity)
            grow(aLength);

        *reinterpret_cast<uint32 *>(mAddress + inOffset) = htonl(inValue);
        return aLength;
    }

    uint32 put(uint32 inOffset, uint32 inLength, const uint8 *inData)
    {
        uint32 aLength = inOffset + inLength;
        // Round up to nearest multiple of 4 bytes, to pad with zeros
        uint32 aNewOffset = align(aLength);
        if (aNewOffset > mCapacity)
            grow(aNewOffset);

        memcpy(mAddress + inOffset, inData, inLength);

        for (uint32 anOffset = aLength; anOffset < aNewOffset; anOffset++)
            mAddress[anOffset] = 0;

        return aNewOffset;
    }

    const uint8 *address() const { return mAddress; }
    uint8 *release()
    {
        uint8 *anAddress = mAddress;
        mAddress = NULL;
        mCapacity = 0;
        return anAddress;
    }

private:
    CssmAllocator &mAllocator;
    size_t mCapacity;
};

} // end namespace Security

#endif // _H_APPLEDL_READWRITESECTION
