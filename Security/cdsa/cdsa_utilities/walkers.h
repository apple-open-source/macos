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
// walkers - facilities for traversing and manipulating recursive data structures
//
// Very briefly, this facility allows for deep traversals of (potentially) recursive
// data structures through templated structure "walkers." Standard operations include
// deep copying to a contiguous memory buffer, size calculation, and reconstitution
// after relocation (e.g. via IPC). You can add other operations (e.g. scattered deep
// copy, debug dumping, etc.) by defining operations classes and applying them to the
// existing walkers. You can also extend the reach of the facility to new data structures
// by writing appropriate walker functions for them.
//
// For more detailed rules and regulations, see the accompanying documentation.
//
#ifndef _H_WALKERS
#define _H_WALKERS

#include <Security/utilities.h>
#include <Security/cssmalloc.h>
#include <Security/memutils.h>
#include <set>

#ifdef _CPP_WALKERS
# pragma export on
#endif


namespace Security
{

namespace DataWalkers
{

//
// Standard operators for sizing, copying, and reinflating
//
class SizeWalker : public LowLevelMemoryUtilities::Writer::Counter {
public:
    template <class T>
    void operator () (T *, size_t size = sizeof(T))
    { LowLevelMemoryUtilities::Writer::Counter::insert(size); }
    
    void reserve(size_t space)
    { LowLevelMemoryUtilities::Writer::Counter::insert(space); }
    
    static const bool needsRelinking = false;
    static const bool needsSize = true;
};

class CopyWalker : public LowLevelMemoryUtilities::Writer {
public:
    CopyWalker() { }
    CopyWalker(void *base) : LowLevelMemoryUtilities::Writer(base) { }
    
    template <class T>
    void operator () (T * &addr, size_t size = sizeof(T))
    {
        if (addr)
            addr = reinterpret_cast<T *>(LowLevelMemoryUtilities::Writer::operator () (addr, size));
    }
    
    static const bool needsRelinking = true;
    static const bool needsSize = true;
};

class ReconstituteWalker {
public:
    ReconstituteWalker(off_t offset) : mOffset(offset) { }
    ReconstituteWalker(void *ptr, void *base)
    : mOffset(LowLevelMemoryUtilities::difference(ptr, base)) { }

    template <class T>
    void operator () (T * &addr, size_t = 0)
    {
        if (addr)
            addr = LowLevelMemoryUtilities::increment<T>(addr, mOffset);
    }
    
    static const bool needsRelinking = true;
    static const bool needsSize = false;
    
private:
    off_t mOffset;
};

class ChunkCopyWalker {
public:
    ChunkCopyWalker(CssmAllocator &alloc = CssmAllocator::standard()) : allocator(alloc) { }
    
    CssmAllocator &allocator;
    
    template <class T>
    void operator () (T * &addr, size_t size = sizeof(T))
    {
#if BUG_GCC
        T *copy = reinterpret_cast<T *>(allocator.malloc(size));
#else
        T *copy = allocator.malloc<T>(size);
#endif
        memcpy(copy, addr, size);
        addr = copy;
    }
    
    static const bool needsRelinking = true;
    static const bool needsSize = true;
};

class ChunkFreeWalker {
public:
    ChunkFreeWalker(CssmAllocator &alloc = CssmAllocator::standard()) : allocator(alloc) { }
    
    CssmAllocator &allocator;
    
    template <class T>
    void operator () (T *addr, size_t = sizeof(T))
    {
        freeSet.insert(addr);
    }
    
    void free();
    ~ChunkFreeWalker() { free(); }
    
    static const bool needsRelinking = false;
    static const bool needsSize = false;

private:
    std::set<void *> freeSet;
};


//
// The VirtualWalker class is a generic walker interface
// for dynamic passing around.
//
class VirtualWalker {
public:
    VirtualWalker(bool nrl = false) : needsRelinking(nrl) { }
    virtual ~VirtualWalker();
    
    virtual void operator () (void * &addr, size_t size) = 0;

    const bool needsRelinking;
};

template <class Walker>
class VirtualWalkerFor : public VirtualWalker {
public:
    VirtualWalkerFor(Walker &w, bool nrl = false) : VirtualWalker(nrl), walker(w) { }
    void operator () (void * &addr, size_t size)
    { walker(addr, size); }
    
    Walker &walker;
};


//
// Stand-alone operations for a single structure web
//
template <class T>
size_t size(T obj)
{
    SizeWalker w;
    walk(w, obj);
    return w;
}


template <class T>
T *copy(const T *obj, void *addr)
{
    if (obj == NULL)
        return NULL;
    CopyWalker w(addr);
    walk(w, obj);
    return reinterpret_cast<T *>(addr);
}

template <class T>
T *copy(const T *obj, CssmAllocator &alloc = CssmAllocator::standard())
{
    return obj ? copy(obj, alloc, size(obj)) : NULL;
}

template <class T>
T *copy(const T *obj, CssmAllocator &alloc, size_t size)
{
    if (obj == NULL)
        return NULL;
    return copy(obj, alloc.malloc(size));
}

template <class T>
void copy(const T *obj, CssmAllocator &alloc, CssmData &data)
{
    if (obj == NULL) {
        data.Length = 0;
        return;
    }
    if (data.data() == NULL) {
        size_t length = size(obj);
        data = CssmData(alloc.malloc(length), length);
    } else
        assert(size(obj) <= data.length());
    copy(obj, data.data());
}


template <class T>
void relocate(T *obj, T *base)
{
	if (obj) {
		ReconstituteWalker w(LowLevelMemoryUtilities::difference(obj, base));
		walk(w, base);
	}
}


template <class T>
T *chunkCopy(const T *obj, CssmAllocator &alloc = CssmAllocator::standard())
{
	if (obj) {
		ChunkCopyWalker w(alloc);
		return walk(w, obj);
	} else
		return NULL;
}

template <class T>
void chunkFree(const T *obj, CssmAllocator &alloc = CssmAllocator::standard())
{
    if (obj) {
		ChunkFreeWalker w(alloc);
		walk(w, obj);
	}
}

template <class T>
class Copier {
public:
    Copier(const T *obj, CssmAllocator &alloc = CssmAllocator::standard()) : allocator(alloc)
    {
        if (obj == NULL) {
            mValue = NULL;
            mLength = 0;
        } else {
            mLength = size(obj);
#if BUG_GCC
            mValue = reinterpret_cast<T *>(alloc.malloc(mLength));
#else
            mValue = alloc.malloc<T>(mLength);
#endif
            mValue = copy(obj, mValue);
        }
    }
    
    Copier(const T *obj, uint32 count, CssmAllocator &alloc = CssmAllocator::standard())
		: allocator(alloc)
    {
        if (obj == NULL) {
            mValue = NULL;
            mLength = 0;
        } else {
            SizeWalker sizer;
            sizer.reserve(sizeof(T) * count);	// initial vector size
            for (uint32 n = 0; n < count; n++)
                walk(sizer, obj[n]);	// dependent data sizes
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

    CssmAllocator &allocator;

    ~Copier() { allocator.free(mValue); }
    
    operator T *() const { return mValue; }
    size_t length() const { return mLength; }
    
    T *keep() { T *result = mValue; mValue = NULL; return result; }	
    
private:
    T *mValue;
    size_t mLength;
};


//
// Allow const pointer input but cast the const-ness away.
// This is valid because the walkers never directly *write* into *obj;
// they just pass their parts along to operate(). If we are in a writing
// pass, operate() must ensure it copies (and replaces) its argument to
// make it writable.
// (The alternative design calls for three walk() functions for each type,
//  which is ugly and error prone.)
//
template <class Action, class T>
T *walk(Action &operate, const T * &obj)
{ return walk(operate, const_cast<T * &>(obj)); }


//
// The default walker assumes a flat data structure
//
template <class Action, class Type>
Type *walk(Action &operate, Type * &obj)
{
    operate(obj);
    return obj;
}


} // end namespace DataWalkers

} // end namespace Security

#ifdef _CPP_WALKERS
# pragma export off
#endif

#endif //_H_WALKERS
