/*
 * Copyright (c) 2000-2006,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// walkers - facilities for traversing and manipulating recursive data structures
//
// Very briefly, this facility allows for deep traversals of (potentially) recursive
// data structures through templated structure "walkers." Standard operations include
// deep copying to a contiguous memory buffer, size calculation, deep freeing, reconstitution
// after relocation (e.g. via IPC), and others. You can add other operations (e.g. scattered deep
// copy, debug dumping, etc.) by defining operations classes and applying them to the
// existing walkers. You can also extend the reach of the facility to new data structures
// by writing appropriate walker functions for them.
//
// NOTE: We no longer have a default walker for flat structures. You must define
// a walk(operate, foo * &) function for every data type encountered during a walk
// or you will get compile-time errors.
//
// For more detailed rules and regulations, see the accompanying documentation.
//
#ifndef _H_WALKERS
#define _H_WALKERS

#include <security_utilities/alloc.h>
#include <security_utilities/memstreams.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_utilities/debugging.h>
#include <set>


namespace Security {
namespace DataWalkers {

#define WALKERDEBUG 0


#if WALKERDEBUG
# define DEBUGWALK(who)	secinfo("walkers", "walk " who " %s@%p (%ld)", \
									Debug::typeName(addr).c_str(), addr, size)
#else
# define DEBUGWALK(who)	/* nothing */
#endif


//
// SizeWalker simply walks a structure and calculates how many bytes
// CopyWalker would use to make a flat copy. This is naturally at least
// the sum of all relevant sizes, but can be more due to alignment and
// counting overhead.
//
class SizeWalker : public LowLevelMemoryUtilities::Writer::Counter {
public:
    template <class T>
	void operator () (T &obj, size_t size = sizeof(T)) { }

    template <class T>
    void operator () (T *addr, size_t size = sizeof(T))
    { DEBUGWALK("size"); LowLevelMemoryUtilities::Writer::Counter::insert(size); }
    
	void blob(void *addr, size_t size)
	{ (*this)(addr, size); }

    void reserve(size_t space)
    { LowLevelMemoryUtilities::Writer::Counter::insert(space); }
    
    static const bool needsRelinking = false;
    static const bool needsSize = true;
};


//
// CopyWalker makes a deep, flat copy of a structure. The result will work
// just like the original (with all elements recursively copied), except that
// it occupies contiguous memory.
//
class CopyWalker : public LowLevelMemoryUtilities::Writer {
public:
    CopyWalker() { }
    CopyWalker(void *base) : LowLevelMemoryUtilities::Writer(base) { }
    
public:
    template <class T>
	void operator () (T &obj, size_t size = sizeof(T))
	{ }

    template <class T>
    void operator () (T * &addr, size_t size = sizeof(T))
    {
		DEBUGWALK("copy");
        if (addr)
            addr = reinterpret_cast<T *>(LowLevelMemoryUtilities::Writer::operator () (addr, size));
    }
    
	template <class T>
	void blob(T * &addr, size_t size)
	{ (*this)(addr, size); }
    
    static const bool needsRelinking = true;
    static const bool needsSize = true;
};



//
// Walk a structure and apply a constant linear shift to all pointers
// encountered. This is useful when a structure and its deep components
// have been linearly shifted by something (say, an IPC transit).
//
class ReconstituteWalker {
public:
    ReconstituteWalker(off_t offset) : mOffset(offset) { }
    ReconstituteWalker(void *ptr, void *base)
    : mOffset(LowLevelMemoryUtilities::difference(ptr, base)) { }

    template <class T>
	void operator () (T &obj, size_t size = sizeof(T))
	{ }

    template <class T>
    void operator () (T * &addr, size_t size = 0)
    {
		DEBUGWALK("reconstitute");
        if (addr)
            addr = LowLevelMemoryUtilities::increment<T>(addr, (ptrdiff_t)mOffset);
    }
    
	template <class T>
	void blob(T * &addr, size_t size)
	{ (*this)(addr, size); }
	
    static const bool needsRelinking = true;
    static const bool needsSize = false;
    
private:
    off_t mOffset;
};


//
// Make an element-by-element copy of a structure. Each pointer followed
// uses a separate allocation for its pointed-to storage.
//
class ChunkCopyWalker {
public:
    ChunkCopyWalker(Allocator &alloc = Allocator::standard()) : allocator(alloc) { }
    
    Allocator &allocator;
    
    template <class T>
	void operator () (T &obj, size_t size = sizeof(T))
	{ }

    template <class T>
    void operator () (T * &addr, size_t size = sizeof(T))
    {
		DEBUGWALK("chunkcopy");
#if BUG_GCC
        T *copy = reinterpret_cast<T *>(allocator.malloc(size));
#else
        T *copy = allocator.malloc<T>(size);
#endif
        memcpy(copy, addr, size);
        addr = copy;
    }
    
	template <class T>
	void blob(T * &addr, size_t size)
	{ (*this)(addr, size); }
	
    static const bool needsRelinking = true;
    static const bool needsSize = true;
};


//
// Walk a structure and call an Allocator to separate free each node.
// This is safe for non-trees (i.e. shared subsidiary nodes); such will
// only be freed once.
//
class ChunkFreeWalker {
public:
    ChunkFreeWalker(Allocator &alloc = Allocator::standard()) : allocator(alloc) { }
    
    Allocator &allocator;
    
    template <class T>
	void operator () (T &obj, size_t size = 0)
	{ }

	template <class T>
    void operator () (T *addr, size_t size = 0)
    {
		DEBUGWALK("chunkfree");
        freeSet.insert(addr);
    }
    
	void blob(void *addr, size_t size)
	{ (*this)(addr, 0); }

    void free();
    ~ChunkFreeWalker() { free(); }
    
    static const bool needsRelinking = false;
    static const bool needsSize = false;

private:
    std::set<void *> freeSet;
};


//
// Stand-alone operations for a single structure web.
// These simply create, use, and discard their operator objects internally.
//
template <class T>
size_t size(T obj)
{
    SizeWalker w;
    walk(w, obj);
    return w;
}

// Special version for const pointer's
template <class T>
size_t size(const T *obj)
{ return size(const_cast<T *>(obj)); }


template <class T>
T *copy(const T *obj, void *addr)
{
    if (obj == NULL)
        return NULL;
    CopyWalker w(addr);
	walk(w, const_cast<T * &>(obj));
	return const_cast<T *>(obj);
}

template <class T>
T *copy(const T *obj, Allocator &alloc, size_t size)
{
    if (obj == NULL)
        return NULL;
    return copy(obj, alloc.malloc(size));
}

template <class T>
T *copy(const T *obj, Allocator &alloc = Allocator::standard())
{
    return obj ? copy(obj, alloc, size(obj)) : NULL;
}


template <class T>
void relocate(T *obj, T *base)
{
	if (obj) {
		ReconstituteWalker w(LowLevelMemoryUtilities::difference(obj, base));
		walk(w, base);
	}
}


//
// chunkCopy and chunkFree can take pointer and non-pointer arguments.
// Don't try to declare the T arguments const (overload resolution will
// mess you over if you try). Just take const and nonconst Ts and take
// the const away internally.
//
template <class T>
typename Nonconst<T>::Type *chunkCopy(T *obj, Allocator &alloc = Allocator::standard())
{
	if (obj) {
		ChunkCopyWalker w(alloc);
		return walk(w, unconst_ref_cast<T *>(obj));
	} else
		return NULL;
}

template <class T>
T chunkCopy(T obj, Allocator &alloc = Allocator::standard())
{
	ChunkCopyWalker w(alloc);
	walk(w, obj);
	return obj;
}

template <class T>
void chunkFree(T *obj, Allocator &alloc = Allocator::standard())
{
    if (obj) {
		ChunkFreeWalker w(alloc);
		walk(w, unconst_ref_cast<T *>(obj));
	}
}

template <class T>
void chunkFree(T &obj, Allocator &alloc = Allocator::standard())
{
	ChunkFreeWalker w(alloc);
	walk(w, obj);
}


//
// Copier combines SizeWalker and CopyWalker into one operational package.
// this is useful if you need both the copy and its size (and don't want
// to re-run size()). Copier (like copy()) only applies to one object.
//
template <class T>
class Copier {
public:
    Copier(const T *obj, Allocator &alloc = Allocator::standard()) : allocator(alloc)
    {
        if (obj == NULL) {
            mValue = NULL;
            mLength = 0;
        } else {
            mLength = size(const_cast<T *>(obj));
#if BUG_GCC
            mValue = reinterpret_cast<T *>(alloc.malloc(mLength));
#else
			mValue = alloc.malloc<T>(mLength);
#endif
            mValue = copy(obj, mValue);
        }
    }
    
    Copier(const T *obj, uint32 count, Allocator &alloc = Allocator::standard())
		: allocator(alloc)
    {
        if (obj == NULL) {
            mValue = NULL;
            mLength = 0;
        } else {
            SizeWalker sizer;
            sizer.reserve(sizeof(T) * count);	// initial vector size
            for (uint32 n = 0; n < count; n++)
                walk(sizer, const_cast<T &>(obj[n]));	// dependent data sizes
            mLength = sizer;
#if BUG_GCC
            mValue = reinterpret_cast<T *>(alloc.malloc(mLength));
#else
            mValue = alloc.malloc<T>(mLength);
#endif
            CopyWalker copier(LowLevelMemoryUtilities::increment(mValue, sizeof(T) * count));
            for (uint32 n = 0; n < count; n++) {
                mValue[n] = obj[n];
                walk(copier, mValue[n]);
            }
        }
    }

    Allocator &allocator;

    ~Copier() { allocator.free(mValue); }
    
	T *value() const		{ return mValue; }
    operator T *() const	{ return value(); }
    size_t length() const	{ return mLength; }
    
    T *keep() { T *result = mValue; mValue = NULL; return result; }	
    
private:
    T *mValue;
    size_t mLength;
};


} // end namespace DataWalkers
} // end namespace Security

#endif //_H_WALKERS
