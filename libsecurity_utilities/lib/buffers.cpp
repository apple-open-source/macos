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
// buffer - simple data buffers with convenience
//
#include "buffers.h"
#include <security_utilities/debugging.h>
#include <algorithm>


namespace Security {


//
// Construct an empty Buffer from newly allocated memory
//
Buffer::Buffer(size_t size)
    : mBase(new char[size]), mTop(mBase + size), mOwningMemory(true)
{
    mStart = mEnd = mBase;
}


//
// Construct a buffer from given memory, with given fill or ownership
//
Buffer::Buffer(void *base, size_t size, bool filled, bool owned) 
    : mBase(reinterpret_cast<char *>(base)), mTop(mBase + size), mOwningMemory(owned)
{
    mStart = mBase;
    mEnd = filled ? mTop : mBase;
}


//
// Destroying a buffer deallocates its memory iff it owns it.
//
Buffer::~Buffer()
{
    if (mOwningMemory)
        delete[] mBase;
}


//
// Shuffle buffer contents to make more room.
// Takes minimum size needed. Returns size available.
//
size_t Buffer::shuffle(size_t needed)
{
    assert(available() < needed);	// shouldn't be called otherwise
    size_t length = this->length();
    memmove(mBase, mStart, length);
    mStart = mBase;
    mEnd = mStart + length;
    return min(needed, available());
}


//
// Formatted append to buffer
//
void Buffer::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void Buffer::vprintf(const char *format, va_list args)
{
    unsigned int written = vsnprintf(mEnd, mTop - mEnd, format, args);
    if (written < available()) {
        // overflow on formatting. Reshuffle and try again
        shuffle();
        written = vsnprintf(mEnd, available(), format, args);
        assert(written < available());	//@@@ throw here?
    }
    mEnd += written;	// note: zero terminator discarded here
}


}	// end namespace Security
