/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
//CoreFoundation related utilities
//
#ifndef _H_CFUTILITIES
#define _H_CFUTILITIES

#include <security_utilities/utilities.h>
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
	
	CFRef &take(CFType ref)
	{ if (mRef) CFRelease(mRef); mRef = ref; return *this; }

    CFRef &operator = (CFType ref)
    { if (ref) CFRetain(ref); return take(ref); }

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

	CFCopyRef &take(CFType ref)
	{ if (mRef) CFRelease(mRef); mRef = ref; return *this; }

    CFCopyRef &operator = (CFType ref)
    { if (ref) CFRetain(ref); return take(ref); }

    operator CFType () const { return mRef; }
    operator bool () const { return mRef != NULL; }
    bool operator ! () const { return mRef == NULL; }

private:
    CFType mRef;
};


//
// A simple function that turns a non-array CFTypeRef into
// an array of one with that element. This will retain its argument
// (directly or indirectly).
//
inline CFArrayRef cfArrayize(CFTypeRef arrayOrItem)
{
    if (arrayOrItem == NULL)
        return NULL;		// NULL is NULL
    else if (CFGetTypeID(arrayOrItem) == CFArrayGetTypeID()) {
		CFRetain(arrayOrItem);
        return CFArrayRef(arrayOrItem);		// already an array
    } else {
        CFArrayRef array = CFArrayCreate(NULL,
            (const void **)&arrayOrItem, 1, &kCFTypeArrayCallBacks);
        return array;
    }
}


//
// Translate CFStringRef or CFURLRef to (UTF8-encoded) C++ string.
// If release==true, a CFRelease will be performed on the CFWhatever argument
// whether the call succeeds or not(!).
//
string cfString(CFStringRef str, bool release = false);	// extract UTF8 string
string cfString(CFURLRef url, bool release = false);	// path of file: URL (only)
string cfString(CFBundleRef url, bool release = false);	// path to bundle root


//
// Get the number out of a CFNumber
//
uint32_t cfNumber(CFNumberRef number);
uint32_t cfNumber(CFNumberRef number, uint32_t defaultValue);


//
// Turn a string or const char * into a CFStringRef, yield is as needed, and
// release it soon thereafter.
//
class CFTempString : public CFRef<CFStringRef> {
public:
	template <class Source>
	CFTempString(Source s) : CFRef<CFStringRef>(makeCFString(s)) { }
};

class CFTempURL : public CFRef<CFURLRef> {
public:
	template <class Source>
	CFTempURL(Source s, bool isDirectory = false) : CFRef<CFURLRef>(makeCFURL(s, isDirectory)) { }
};




//
// A temporary CFNumber
//
class CFTempNumber : public CFRef<CFNumberRef> {
public:
	template <class Value>
	CFTempNumber(Value value) : CFRef<CFNumberRef>(makeCFNumber(value)) { }
};


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
	return s ? CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8) : NULL;
}

inline CFStringRef makeCFString(const string &s)
{
	return CFStringCreateWithCString(NULL, s.c_str(), kCFStringEncodingUTF8);
}

CFURLRef makeCFURL(const char *s, bool isDirectory = false);

inline CFURLRef makeCFURL(const string &s, bool isDirectory = false)
{
	return makeCFURL(s.c_str(), isDirectory);
}


//
// Translate numeric values into CFNumbers
//
inline CFNumberRef makeCFNumber(uint32_t value)
{
	return CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
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
    operator UInt32 () const			{ return mCount; }
    operator VectorBase *() const		{ return mVector; }
    bool empty() const					{ return mCount == 0; }
	
	VectorBase *begin() const			{ return mVector; }
	VectorBase *end() const				{ return mVector + mCount; }
    
    VectorBase &operator [] (UInt32 ix) const { assert(ix < mCount); return mVector[ix]; }

private:
    VectorBase *mVector;
    UInt32 mCount;
};

template <class VectorBase, class CFRefType, VectorBase convert(CFRefType)>
CFToVector<VectorBase, CFRefType, convert>::CFToVector(CFArrayRef arrayRef)
{
    if (arrayRef == NULL) {
        mCount = 0;
        mVector = NULL;
    } else {
        mCount = CFArrayGetCount(arrayRef);
        mVector = new VectorBase[mCount];
        for (UInt32 n = 0; n < mCount; n++)
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
    for (UInt32 n = 0; n < size; n++)
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
