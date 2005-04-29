/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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
// streams.h - lightweight source and sink objects
//
#include "streams.h"
#include <security_utilities/memutils.h>


namespace Security {


//
// Source and Sink abstract superclasses
//
Source::State Source::state() const
{
    return mState;
}

size_t Source::getSize()
{
    return unknownSize;
}

void Sink::setSize(size_t)
{
    // ignored
}


//
// Null sources and sinks
//
void NullSource::produce(void *, size_t &length)
{
    length = 0;
}

Source::State NullSource::state() const
{
    return endOfData;
}

void NullSink::consume(const void *, size_t)
{
    // ignore the data
}


//
// File sources and sinks
//
void FileSource::produce(void *data, size_t &length)
{
    if ((length = read(data, length)) == 0)
        mState = endOfData;
}

size_t FileSource::getSize()
{
    return fileSize();
}


void FileSink::consume(const void *data, size_t length)
{
	write(data, length);
}


//
// Memory sources
//
void MemorySource::produce(void *data, size_t &length)
{
    if (mRemaining < length)
        length = mRemaining;
    memcpy(data, mData, length);
    mData = LowLevelMemoryUtilities::increment(mData, length);
    mRemaining -= length;
}

size_t MemorySource::getSize()
{
    return mRemaining;
}

Source::State MemorySource::state() const
{
    return mRemaining ? producing : endOfData;
}


//
// Memory sinks
//
void MemorySink::consume(const void *data, size_t length)
{
    if (mSize + length > mMax)
        grow(mSize * 3 / 2);
    assert(mSize + length <= mMax);
    memcpy(((char *)mBuffer) + mSize, data, length);
    mSize += length;
}

void MemorySink::setSize(size_t expectedSize)
{
    grow(expectedSize);
}

void MemorySink::grow(size_t newSize)
{
    if (void *p = realloc(mBuffer, newSize)) {
        mBuffer = p;
        mMax = newSize;
    } else
        UnixError::throwMe();
}


}	// end namespace Security
