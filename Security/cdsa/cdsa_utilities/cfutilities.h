/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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
//CoreFoundation related utilities
//
#ifndef _H_CFUTILITIES
#define _H_CFUTILITIES

#include <Security/utilities.h>
#include <CoreFoundation/CoreFoundation.h>
#include <algorithm>


namespace Security {


//
// Initialize-only self-releasing CF object handler (lightweight).
// Does not support assignment.
//
template <class CFType> class CFRef {
public:
    CFRef() : mRef(NULL) { }
    CFRef(CFType ref) : mRef(ref) { }
    CFRef(const CFRef &ref) : mRef(ref) { if (ref) CFRetain(ref); }
    ~CFRef() { if (mRef) CFRelease(mRef); }

    CFRef &operator = (CFType ref)
    { if (ref) CFRetain(ref); if (mRef) CFRelease(mRef); mRef = ref; return *this; }

    operator CFType () const { return mRef; }
    operator bool () const { return mRef != NULL; }
    bool operator ! () const { return mRef == NULL; }

private:
    CFType mRef;
};


template <class CFType> class CFCopyRef {
public:
    CFCopyRef() : mRef(NULL) { }
    explicit CFCopyRef(CFType ref) : mRef(ref) { if (ref) CFRetain(ref); }
    CFCopyRef(const CFCopyRef &ref) : mRef(ref) { if (ref) CFRetain(ref); }
    ~CFCopyRef() { if (mRef) CFRelease(mRef); }

    CFCopyRef &operator = (CFType ref)
    { if (ref) CFRetain(ref); if (mRef) CFRelease(mRef); mRef = ref; return *this; }

    operator CFType () const { return mRef; }
    operator bool () const { return mRef != NULL; }
    bool operator ! () const { return mRef == NULL; }

private:
    CFType mRef;
};


//
// A simple function that turns a non-array CFTypeRef into
// an array of one with that element.
//
inline CFArrayRef cfArrayize(CFTypeRef arrayOrItem)
{
    if (arrayOrItem == NULL)
        return NULL;		// NULL is NULL
    else if (CFGetTypeID(arrayOrItem) == CFArrayGetTypeID())
        return CFArrayRef(arrayOrItem);		// already an array
    else {
        CFArrayRef array = CFArrayCreate(NULL,
            (const void **)&arrayOrItem, 1, &kCFTypeArrayCallBacks);
        CFRelease(arrayOrItem);	// was retained by ArrayCreate
        return array;
    }
}


//
// Translate CFDataRef to CssmData. The output shares the input's buffer.
//
inline CssmData cfData(CFDataRef data)
{
	return CssmData(const_cast<UInt8 *>(CFDataGetBytePtr(data)),
		CFDataGetLength(data));
}


//
// Translate CFStringRef to (UTF8-encoded) C++ string
//
string cfString(CFStringRef str);


//
// Translate any Data-oid source to a CFDataRef. The contents are copied.
//
template <class Data>
inline CFDataRef makeCFData(const Data &source)
{
	return CFDataCreate(NULL, reinterpret_cast<const UInt8 *>(source.data()), source.length());
}


//
// Translate strings into CFStrings
//
inline CFStringRef makeCFString(const char *s)
{
	return CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8);
}

inline CFStringRef makeCFString(const string &s)
{
	return CFStringCreateWithCString(NULL, s.c_str(), kCFStringEncodingUTF8);
}


//
// Internally used STL adapters. Should probably be in utilities.h.
//
template <class Self>
Self projectPair(const Self &me)
{ return me; }

template <class First, class Second>
Second projectPair(const pair<First, Second> &me)
{ return me.second; }


//
// A CFToVector turns a CFArrayRef of items into a flat
// C vector of some type, using a conversion function
// (from CFTypeRef) specified. As a special bonus, if
// you provide a CFTypeRef (other than CFArrayRef), it
// will be transparently handled as an array-of-one.
// The array will be automatically released on destruction
// of the CFToVector object. Any internal structure shared
// with the CFTypeRef inputs will be left alone.
//
template <class VectorBase, class CFRefType, VectorBase convert(CFRefType)>
class CFToVector {
public:
    CFToVector(CFArrayRef arrayRef);
    ~CFToVector()						{ delete[] mVector; }
    operator uint32 () const			{ return mCount; }
    operator VectorBase *() const		{ return mVector; }
    bool empty() const					{ return mCount == 0; }
	
	VectorBase *begin() const			{ return mVector; }
	VectorBase *end() const				{ return mVector + mCount; }
    
    VectorBase &operator [] (uint32 ix) const { assert(ix < mCount); return mVector[ix]; }

private:
    VectorBase *mVector;
    uint32 mCount;
};

template <class VectorBase, class CFRefType, VectorBase convert(CFTypeRef)>
CFToVector<VectorBase, CFRefType, convert>::CFToVector(CFArrayRef arrayRef)
{
    if (arrayRef == NULL) {
        mCount = 0;
        mVector = NULL;
    } else {
        mCount = CFArrayGetCount(arrayRef);
        mVector = new VectorBase[mCount];
        for (uint32 n = 0; n < mCount; n++)
            mVector[n] = convert(CFRefType(CFArrayGetValueAtIndex(arrayRef, n)));
    }
}


//
// Generate a CFArray of CFTypeId things generated from iterators.
// @@@ This should be cleaned up with partial specializations based
// @@@ on iterator_traits.
//
template <class Iterator, class Generator>
inline CFArrayRef makeCFArray(Generator &generate, Iterator first, Iterator last)
{
	// how many elements?
	size_t size = distance(first, last);
	
	// do the CFArrayCreate tango
    auto_array<CFTypeRef> vec(size);
    for (uint32 n = 0; n < size; n++)
        vec[n] = generate(projectPair(*first++));
    assert(first == last);
    return CFArrayCreate(NULL, (const void **)vec.get(), size, &kCFTypeArrayCallBacks);
}

template <class Container, class Generator>
inline CFArrayRef makeCFArray(Generator &generate, const Container &container)
{
	return makeCFArray(generate, container.begin(), container.end());
}


} // end namespace Security

#endif //_H_CFUTILITIES
