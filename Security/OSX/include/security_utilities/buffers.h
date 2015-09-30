/*
 * Copyright (c) 2000-2001,2003-2004,2011,2014 Apple Inc. All Rights Reserved.
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
#ifndef _H_BUFFER
#define _H_BUFFER

#include <security_utilities/utilities.h>
#include <cstdarg>
#include <limits.h>


namespace Security {


class Buffer {
public:
    Buffer(size_t size);			// allocate empty buffer
    ~Buffer();
    
    static Buffer reader(void *base, size_t size, bool owned = false)
    { return Buffer(base, size, true, owned); }
    static Buffer writer(void *base, size_t size, bool owned = false)
    { return Buffer(base, size, false, owned); }
    
    size_t available(bool heavy = false) const
    { return heavy ? ((mTop - mEnd) + (mStart - mBase)): (mTop - mEnd); }
    bool isFull(bool heavy = false) const
    { return heavy ? (mEnd == mTop && mStart == mBase) : (mEnd == mTop); }
    bool isEmpty() const		{ return mStart == mEnd; }

    size_t length() const		{ return mEnd - mStart; }
    void *data()				{ assert(mStart == mBase); return mStart; }
    
    void clear()				{ mStart = mEnd = mBase; }
    
protected:
    // private constructor with full flexibility
    Buffer(void *base, size_t size, bool filled, bool owned = false);
    
    // perform expensive realignment to coalesce freespace
    size_t shuffle(size_t needed = UINT_MAX);
    
    // perform cheap adjustments after data was taken out
    void adjustGet()
    {
        if (isEmpty())	// empty buffer. Reset pointers to base
            mStart = mEnd = mBase;
    }
    
public:
    // elementary put: copy mode
    size_t put(const void *data, size_t length)
    {
        if (length > available())
            length = shuffle(length);
        memcpy(mEnd, data, length);
        mEnd += length;
        return length;
    }

    // elementary put: locate mode. Remember that each can shuffle memory
    template <class T> void locatePut(T * &addr, size_t &length)
    {
        if (length > available())
            length = shuffle(length);
        addr = reinterpret_cast<T *>(mEnd);
    }
    
    void usePut(size_t length)
    {
        assert(length <= available());
        mEnd += length;
    }
    
    // elementary get: locate mode
    template <class T> void locateGet(T * &addr, size_t &length)
    {
        if (length > size_t(mEnd - mStart))
            length = mEnd - mStart;
        addr = reinterpret_cast<T *>(mStart);
    }
    
    void useGet(size_t length)
    {
        assert(length <= this->length());
        mStart += length;
        adjustGet();
    }
    
    //
    // I/O via FileDescoid objects
    //
    template <class IO>
    size_t read(IO &io, size_t length)
    {
        if (length > available())
            length = shuffle(length);
        size_t bytesRead = io.read(mEnd, length);
        mEnd += bytesRead;
        return bytesRead;
    }
    
    template <class IO>
    size_t write(IO &io, size_t length)
    {
        length = min(this->length(), length);
        size_t bytesWritten = io.write(mStart, length);
        mStart += bytesWritten;
        adjustGet();
        return bytesWritten;
    }

    template <class IO> size_t read(IO &io, bool heavy = false)
    { return read(io, available(heavy)); }
    
    template <class IO> size_t write(IO &io)
    { return write(io, length()); }

    // printf-style output to a buffer
    void printf(const char *format, ...);
    void vprintf(const char *format, va_list args);

    // memory ownership
    void own()		{ mOwningMemory = true; }
    
private:
    char *const mBase;			// base pointer
    char *const mTop;			// end pointer + 1
    char *mStart;				// start of used area
    char *mEnd;					// end of used area + 1
    bool mOwningMemory;			// true if we own the memory (free on destruction)
};


}	// end namespace Security


#endif //_H_BUFFER
