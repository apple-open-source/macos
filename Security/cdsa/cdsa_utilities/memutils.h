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


} // end namespace LowLevelMemoryUtilities

} // end namespace Security

#endif //_H_MEMUTILS
