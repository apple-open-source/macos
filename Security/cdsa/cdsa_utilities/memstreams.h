/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All Rights Reserved.
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
// memstreams - primitive memory block streaming support
//
#ifndef _H_MEMSTREAMS
#define _H_MEMSTREAMS

#include <Security/memutils.h>
#include <Security/endian.h>


namespace Security
{

//
// Encapsulate these very sharp tools in a separate namespace
//
namespace LowLevelMemoryUtilities
{


//
// A simple utility for incremental creation of a contiguous memory block.
//
// Note that Writer and Writer::Counter go together. They use the same alignment
// and padding rules, so Writer::Counter will correctly calculate total buffer
// size for Writer, *presuming* that they are called in the same order.
//
// This layer allocates no memory; that's up to the caller (you).
//
// WARNING: There is no check for overflow. If you write too much, you will die.
// Writer::Counter can tell you how much you need.
//
class Writer {
private:
	void *advance(size_t size)
	{
		void *here = alignUp(writePos);
		writePos = increment(here, size);
		return here;
	}
    
public:    
	Writer() { }
	Writer(void *base) : writePos(base) { }
	void operator = (void *base) { writePos = base; }

	template <class T>
	T *operator () (const T &obj)
	{ T *here = (T *)advance(sizeof(T)); *here = obj; return here; }

	void *operator () (const void *addr, size_t size)
	{ void *here = advance(size); return memcpy(here, addr, size); }

	char *operator () (const char *s)
	{ return (char *)(*this)(s, strlen(s) + 1); }
	
	void countedData(const void *data, uint32 length)
	{ Endian<uint32> temp = length; (*this)(temp); (*this)(data, length); }
	
	template <class Data>
	void countedData(const Data &data)
	{ countedData(data.data(), data.length()); }

	class Counter;

private:
	void *writePos;			// next byte address
};

class Writer::Counter {
private:
	void align() { totalSoFar = alignUp(totalSoFar); }

public:
	Counter() : totalSoFar(0) { }
	operator size_t () { return totalSoFar; }

	template <class T> size_t operator () (const T &) { align(); return totalSoFar += sizeof(T); }
	size_t insert(size_t size) { align(); return totalSoFar += size; }
	size_t operator () (const char *s) { align(); return totalSoFar += strlen(s) + 1; }
	
	void countedData(const void *, uint32 length)
	{ insert(sizeof(uint32)); insert(length); }
	
	template <class Data>
	void countedData(const Data &data)
	{ countedData(data.data(), data.length()); }

private:
	size_t totalSoFar;	// total size counted so far
};


//
// The Reader counter-part for a Writer.
// Again, Reader and Writer share alignment and representation rules, so what was
// Written shall be Read again, just fine.
//
class Reader {
private:
    const void *advance(size_t size = 0)
    {
        const void *here = alignUp(readPos);
        readPos = increment(here, size);
        return here;
    }

public:
    Reader() { }
    Reader(const void *base) : readPos(base) { }
    void operator = (const void *base) { readPos = base; }
    
    template <class T>
    void operator () (T &obj) {	obj = *reinterpret_cast<const T *>(advance(sizeof(T))); }
    void operator () (void *addr, size_t size)	{ memcpy(addr, advance(size), size); }
    void operator () (const char * &s)
    { s = reinterpret_cast<const char *>(advance()); advance(strlen(s) + 1); }
    template <class T>
    const T *get(size_t size)
	{ return reinterpret_cast<const T *>(advance(size)); }
	
	void countedData(const void * &data, uint32 &length)
	{ Endian<uint32> temp; (*this)(temp); length = temp; data = advance(length); }

private:
	// Explicitly forbid some invocations that are likely to be wrong.
	void operator () (char * &s);	// can't get writable string in-place
    
private:
    const void *readPos;	// next byte address
};


} // end namespace LowLevelMemoryUtilities

} // end namespace Security

#endif //_H_MEMUTILS
