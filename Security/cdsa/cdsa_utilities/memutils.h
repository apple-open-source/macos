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
// memutils - memory-related low-level utilities for easier living
//
#ifndef _H_MEMUTILS
#define _H_MEMUTILS

#include <Security/utilities.h>
#include <stdlib.h>
#include <algorithm>


#ifdef _CPP_MEMUTILS
# pragma export on
#endif


namespace Security
{

//
// Encapsulate these very sharp tools in a separate namespace
//
namespace LowLevelMemoryUtilities
{


//
// The default system alignment.
// @@@ We should really get this from somewhere... probably from utility_config.h.
//
static const size_t systemAlignment = 4;
typedef UInt32 PointerInt;


//
// Get the local alignment for a type.
//
template <class T>
inline size_t alignof() { struct { char c; T t; } s; return sizeof(s) - sizeof(T); }


//
// Round up a size or pointer to an alignment boundary.
// Alignment must be a power of two; default is default alignment.
//
inline size_t alignUp(size_t size, size_t alignment = systemAlignment)
{
	return ((size - 1) & ~(alignment - 1)) + alignment;
}

inline void *alignUp(void *p, size_t alignment = systemAlignment)
{
	return reinterpret_cast<void *>(alignUp(PointerInt(p), alignment));
}

inline const void *alignUp(const void *p, size_t alignment = systemAlignment)
{
	return reinterpret_cast<const void *>(alignUp(PointerInt(p), alignment));
}

template <class T>
inline const T *increment(const void *p, ptrdiff_t offset)
{ return reinterpret_cast<const T *>(PointerInt(p) + offset); }

template <class T>
inline T *increment(void *p, ptrdiff_t offset)
{ return reinterpret_cast<T *>(PointerInt(p) + offset); }

inline const void *increment(const void *p, ptrdiff_t offset)
{ return increment<const void>(p, offset); }

inline void *increment(void *p, ptrdiff_t offset)
{ return increment<void>(p, offset); }

template <class T>
inline const T *increment(const void *p, ptrdiff_t offset, size_t alignment)
{ return increment<const T>(alignUp(p, alignment), offset); }

template <class T>
inline T *increment(void *p, ptrdiff_t offset, size_t alignment)
{ return increment<T>(alignUp(p, alignment), offset); }

inline const void *increment(const void *p, ptrdiff_t offset, size_t alignment)
{ return increment<const void>(p, offset, alignment); }

inline void *increment(void *p, ptrdiff_t offset, size_t alignment)
{ return increment<void>(p, offset, alignment); }

inline ptrdiff_t difference(const void *p1, const void *p2)
{ return PointerInt(p1) - PointerInt(p2); }


//
// A simple utility for incremental creation of a contiguous memory block.
//
// Note that Writer and Writer::Counter go together. They use the same alignment
// and padding rules, so Writer::Counter will correctly calculate total buffer
// size for Writer, *presuming* that they are called in the same order.
//
// WARNING: There is no check for overflow. If you write too much, you will die.
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
	{ (*this)(length); (*this)(data, length); }
	
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
	{ (*this)(length); data = advance(length); }

private:
	// Explicitly forbid some invocations that are likely to be wrong.
	void operator () (char * &s);	// can't get writable string in-place
    
private:
    const void *readPos;	// next byte address
};


} // end namespace LowLevelMemoryUtilities

} // end namespace Security

#ifdef _CPP_MEMUTILS
# pragma export off
#endif

#endif //_H_MEMUTILS
