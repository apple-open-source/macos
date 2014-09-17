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
#include <security_utilities/globalizer.h>
#include <CoreFoundation/CoreFoundation.h>
#include <algorithm>
#include <Security/SecBase.h>
#undef check


namespace Security {


//
// Traits of popular CF types
//
template <class CFType> struct CFTraits { };

template <> struct CFTraits<CFTypeRef> {
	static bool check(CFTypeRef ref) { return true; }
};

#define __SEC_CFTYPE(name) \
	template <> struct CFTraits<name##Ref> { \
		static CFTypeID cfid() { return name##GetTypeID(); } \
		static bool check(CFTypeRef ref) { return CFGetTypeID(ref) == cfid(); } \
	};
	
__SEC_CFTYPE(CFNull)
__SEC_CFTYPE(CFBoolean)
__SEC_CFTYPE(CFNumber)
__SEC_CFTYPE(CFString)
__SEC_CFTYPE(CFData)
__SEC_CFTYPE(CFDate)
__SEC_CFTYPE(CFURL)
__SEC_CFTYPE(CFBundle)
__SEC_CFTYPE(CFArray)
__SEC_CFTYPE(CFDictionary)
__SEC_CFTYPE(CFSet)


//
// Initialize-only self-releasing CF object handler (lightweight).
//
template <class CFType> class CFRef {
public:
    CFRef() : mRef(NULL) { }
    CFRef(CFType ref) : mRef(ref) { }
    ~CFRef() { this->release(); }
	CFRef(const CFRef &ref) : mRef(ref) {}
	template <class _T> CFRef(const CFRef<_T> &ref) : mRef(ref) {}
	
	CFRef(CFTypeRef ref, OSStatus err)
		: mRef(CFType(ref))
	{
		if (ref && !CFTraits<CFType>::check(ref))
			MacOSError::throwMe(err);
	}		
	
	CFRef &take(CFType ref)
	{ this->release(); mRef = ref; return *this; }
	
	CFType yield()
	{ CFType r = mRef; mRef = NULL; return r; }

    CFRef &operator = (CFType ref)
    { if (ref) CFRetain(ref); return take(ref); }
	
	CFRef &operator = (const CFRef &ref)
	{ if (ref) CFRetain(ref); return take(ref); }

	// take variant for when newly created CFType is returned
	// via a ptr-to-CFType argument.
	CFType *take()
	{ if (mRef) CFRelease(mRef); mRef = NULL; return &mRef; }

    operator CFType () const { return mRef; }
    operator bool () const { return mRef != NULL; }
    bool operator ! () const { return mRef == NULL; }

	CFType get() const { return mRef; }
	
	CFType &aref()
	{ take(NULL); return mRef; }
	
	CFType retain() const
	{ if (mRef) CFRetain(mRef); return mRef; }
	
	void release() const
	{ if (mRef) CFRelease(mRef); }
	
	template <class NewType>
	bool is() const { return CFTraits<NewType>::check(mRef); }
	
	template <class OldType>
	static CFType check(OldType cf, OSStatus err)
	{
		if (cf && !CFTraits<CFType>::check(cf))
			MacOSError::throwMe(err);
		return CFType(cf);
	}
	
	template <class NewType>
	NewType as() const { return NewType(mRef); }
	
	template <class NewType>
	NewType as(OSStatus err) const { return CFRef<NewType>::check(mRef, err); }

private:
    CFType mRef;
};


template <class CFType> class CFCopyRef : public CFRef<CFType> {
	typedef CFRef<CFType> _Base;
public:
    CFCopyRef() { }
    CFCopyRef(CFType ref) : _Base(ref) { this->retain(); }
    CFCopyRef(const CFCopyRef &ref) : _Base(ref) { this->retain(); }
	template <class _T> CFCopyRef(const CFRef<_T> &ref) : _Base(ref) { this->retain(); }
	CFCopyRef(CFTypeRef ref, OSStatus err) : _Base(ref, err) { this->retain(); }
	
	CFCopyRef &take(CFType ref)
	{ _Base::take(ref); return *this; }

    CFCopyRef &operator = (CFType ref)
    { if (ref) CFRetain(ref); return take(ref); }
	
	CFCopyRef &operator = (const CFCopyRef &ref)
	{ _Base::operator = (ref); return *this; }
	
	template <class _T> CFCopyRef &operator = (const CFRef<_T> &ref)
	{ _Base::operator = (ref); return *this; }
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
        return CFArrayCreate(NULL,
            (const void **)&arrayOrItem, 1, &kCFTypeArrayCallBacks);
    }
}


//
// An empty CFArray.
// Since CFArrays are type-neutral, a single immutable empty array will
// serve for all uses. So keep it.
//
struct CFEmptyArray {
	operator CFArrayRef () { return mArray; }
	CFEmptyArray();
private:
	CFArrayRef mArray;
};

extern ModuleNexus<CFEmptyArray> cfEmptyArray;


//
// Translate CFStringRef or CFURLRef to (UTF8-encoded) C++ string.
// If release==true, a CFRelease will be performed on the CFWhatever argument
// whether the call succeeds or not(!).
//
string cfString(CFStringRef str);	// extract UTF8 string
string cfString(CFURLRef url);	// path of file: URL (only)
string cfString(CFBundleRef bundle);	// path to bundle root

string cfStringRelease(CFStringRef str CF_CONSUMED);	// extract UTF8 string
string cfStringRelease(CFURLRef url CF_CONSUMED);	// path of file: URL (only)
string cfStringRelease(CFBundleRef bundle CF_CONSUMED);	// path to bundle root

    
string cfString(CFTypeRef anything, OSStatus err);		// dynamic form; throws err on NULL


//
// Handle CFNumberRefs.
// This is nasty because CFNumber does not support unsigned types, and there's really no portably-safe
// way of working around this. So the handling of unsigned numbers is "almost correct."
//
template <class Number>
struct CFNumberTraits;

template <> struct CFNumberTraits<char> {
	static const CFNumberType cfnType = kCFNumberCharType;
	typedef char ValueType;
};
template <> struct CFNumberTraits<short> {
	static const CFNumberType cfnType = kCFNumberShortType;
	typedef short ValueType;
};
template <> struct CFNumberTraits<int> {
	static const CFNumberType cfnType = kCFNumberIntType;
	typedef int ValueType;
};
template <> struct CFNumberTraits<long> {
	static const CFNumberType cfnType = kCFNumberLongType;
	typedef long ValueType;
};
template <> struct CFNumberTraits<long long> {
	static const CFNumberType cfnType = kCFNumberLongLongType;
	typedef long long ValueType;
};
template <> struct CFNumberTraits<float> {
	static const CFNumberType cfnType = kCFNumberFloatType;
	typedef float ValueType;
};
template <> struct CFNumberTraits<double> {
	static const CFNumberType cfnType = kCFNumberDoubleType;
	typedef double ValueType;
};

template <> struct CFNumberTraits<unsigned char> {
	static const CFNumberType cfnType = kCFNumberIntType;
	typedef int ValueType;
};
template <> struct CFNumberTraits<unsigned short> {
	static const CFNumberType cfnType = kCFNumberIntType;
	typedef int ValueType;
};
template <> struct CFNumberTraits<unsigned int> {
	static const CFNumberType cfnType = kCFNumberLongLongType;
	typedef long long ValueType;
};
template <> struct CFNumberTraits<unsigned long> {
	static const CFNumberType cfnType = kCFNumberLongLongType;
	typedef long long ValueType;
};
template <> struct CFNumberTraits<unsigned long long> {
	static const CFNumberType cfnType = kCFNumberLongLongType;
	typedef long long ValueType;
};

template <class Number>
Number cfNumber(CFNumberRef number)
{
	typename CFNumberTraits<Number>::ValueType value;
	if (CFNumberGetValue(number, CFNumberTraits<Number>::cfnType, &value))
		return (Number)value;
	else
		CFError::throwMe();
}

template <class Number>
Number cfNumber(CFNumberRef number, Number defaultValue)
{
	typename CFNumberTraits<Number>::ValueType value;
	if (CFNumberGetValue(number, CFNumberTraits<Number>::cfnType, &value))
		return value;
	else
		return defaultValue;
}

template <class Number>
CFNumberRef makeCFNumber(Number value)
{
	typename CFNumberTraits<Number>::ValueType cfValue = value;
	return CFNumberCreate(NULL, CFNumberTraits<Number>::cfnType, &cfValue);
}

// legacy form
inline uint32_t cfNumber(CFNumberRef number) { return cfNumber<uint32_t>(number); }


//
// Translate strings into CFStrings
//
inline CFStringRef makeCFString(const char *s, CFStringEncoding encoding = kCFStringEncodingUTF8)
{
	return s ? CFStringCreateWithCString(NULL, s, encoding) : NULL;
}

inline CFStringRef makeCFString(const string &s, CFStringEncoding encoding = kCFStringEncodingUTF8)
{
	return CFStringCreateWithCString(NULL, s.c_str(), encoding);
}

inline CFStringRef makeCFString(CFDataRef data, CFStringEncoding encoding = kCFStringEncodingUTF8)
{
	return CFStringCreateFromExternalRepresentation(NULL, data, encoding);
}


//
// Create CFURL objects from various sources
//
CFURLRef makeCFURL(const char *s, bool isDirectory = false, CFURLRef base = NULL);
CFURLRef makeCFURL(CFStringRef s, bool isDirectory = false, CFURLRef base = NULL);

inline CFURLRef makeCFURL(const string &s, bool isDirectory = false, CFURLRef base = NULL)
{
	return makeCFURL(s.c_str(), isDirectory, base);
}


//
// Make temporary CF objects.
//
class CFTempString : public CFRef<CFStringRef> {
public:
	template <class Source>
	CFTempString(Source s) : CFRef<CFStringRef>(makeCFString(s)) { }
};

class CFTempURL : public CFRef<CFURLRef> {
public:
	template <class Source>
	CFTempURL(Source s, bool isDirectory = false, CFURLRef base = NULL)
		: CFRef<CFURLRef>(makeCFURL(s, isDirectory, base)) { }
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
// A temporary CFData.
//
class CFTempData : public CFRef<CFDataRef> {
public:
	CFTempData(const void *data, size_t length)
		: CFRef<CFDataRef>(CFDataCreate(NULL, (const UInt8 *)data, length)) { }
	
	template <class Dataoid>
	CFTempData(const Dataoid &dataoid)
		: CFRef<CFDataRef>(CFDataCreate(NULL, (const UInt8 *)dataoid.data(), dataoid.length())) { }
};

class CFTempDataWrap : public CFRef<CFDataRef> {
public:
	CFTempDataWrap(const void *data, size_t length)
		: CFRef<CFDataRef>(CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)data, length, kCFAllocatorNull)) { }
	
	template <class Dataoid>
	CFTempDataWrap(const Dataoid &dataoid)
		: CFRef<CFDataRef>(CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)dataoid.data(), dataoid.length(), kCFAllocatorNull)) { }
};


//
// Create CFData objects from various sources.
//
inline CFDataRef makeCFData(const void *data, size_t size)
{
	return CFDataCreate(NULL, (const UInt8 *)data, size);
}

inline CFDataRef makeCFData(CFDictionaryRef dictionary)
{
	return CFPropertyListCreateXMLData(NULL, dictionary);
}

template <class Data>
inline CFDataRef makeCFData(const Data &source)
{
	return CFDataCreate(NULL, reinterpret_cast<const UInt8 *>(source.data()), source.length());
}

inline CFDataRef makeCFDataMalloc(const void *data, size_t size)
{
	return CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)data, size, kCFAllocatorMalloc);
}

template <class Data>
inline CFDataRef makeCFDataMalloc(const Data &source)
{
	return CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)source.data(), source.length(), kCFAllocatorMalloc);
}


//
// Create a CFDataRef from malloc'ed data, exception-safely
//
class CFMallocData {
public:
	CFMallocData(size_t size)
		: mData(::malloc(size)), mSize(size)
	{
		if (!mData)
			UnixError::throwMe();
	}
	
	~CFMallocData()
	{
		if (mData)
			::free(mData);
	}
	
	template <class T>
	operator T * ()
	{ return static_cast<T *>(mData); }
	
	operator CFDataRef ();
		
	void *data()				{ return mData; }
	const void *data() const	{ return mData; }
	size_t length() const		{ return mSize; }
	
private:
	void *mData;
	size_t mSize;
};


//
// Make CFDictionaries from stuff
//
CFDictionaryRef makeCFDictionary(unsigned count, ...);					// key/value pairs
CFMutableDictionaryRef makeCFMutableDictionary();						// empty
CFMutableDictionaryRef makeCFMutableDictionary(unsigned count, ...);	// (count) key/value pairs
CFMutableDictionaryRef makeCFMutableDictionary(CFDictionaryRef dict);	// copy of dictionary

CFDictionaryRef makeCFDictionaryFrom(CFDataRef data) CF_RETURNS_RETAINED;// interpret plist form
CFDictionaryRef makeCFDictionaryFrom(const void *data, size_t length) CF_RETURNS_RETAINED; // ditto


//
// Parsing out a CFDictionary without losing your lunch
//
class CFDictionary : public CFCopyRef<CFDictionaryRef> {
	typedef CFCopyRef<CFDictionaryRef> _Base;
public:
	CFDictionary(CFDictionaryRef ref, OSStatus error) : _Base(ref), mDefaultError(error)
	{ if (!ref) MacOSError::throwMe(error); }
	CFDictionary(CFTypeRef ref, OSStatus error) : _Base(ref, error), mDefaultError(error)
	{ if (!ref) MacOSError::throwMe(error); }
	CFDictionary(OSStatus error) : _Base(NULL), mDefaultError(error) { }
	
	using CFCopyRef<CFDictionaryRef>::get;
	
	CFTypeRef get(CFStringRef key)		{ return CFDictionaryGetValue(*this, key); }
	CFTypeRef get(const char *key)		{ return CFDictionaryGetValue(*this, CFTempString(key)); }
	
	template <class CFType>
	CFType get(CFStringRef key, OSStatus err = errSecSuccess) const
	{
		CFTypeRef elem = CFDictionaryGetValue(*this, key);
		return CFRef<CFType>::check(elem, err ? err : mDefaultError);
	}
	
	template <class CFType>
	CFType get(const char *key, OSStatus err = errSecSuccess) const
	{ return get<CFType>(CFTempString(key), err); }
	
	void apply(CFDictionaryApplierFunction func, void *context)
	{ return CFDictionaryApplyFunction(*this, func, context); }
	
private:
	template <class T>
	struct Applier {
		T *object;
		void (T::*func)(CFTypeRef key, CFTypeRef value);
		static void apply(CFTypeRef key, CFTypeRef value, void *context)
		{ Applier *me = (Applier *)context; return ((me->object)->*(me->func))(key, value); }
	};
	
	template <class Key, class Value>
	struct BlockApplier {
		void (^action)(Key key, Value value);
		static void apply(CFTypeRef key, CFTypeRef value, void* context)
		{ BlockApplier *me = (BlockApplier *)context; return me->action(Key(key), Value(value)); }
	};

public:	
	template <class T>
	void apply(T *object, void (T::*func)(CFTypeRef key, CFTypeRef value))
	{ Applier<T> app; app.object = object; app.func = func; return apply(app.apply, &app); }
	
	template <class Key = CFTypeRef, class Value = CFTypeRef>
	void apply(void (^action)(Key key, Value value))
	{ BlockApplier<Key, Value> app; app.action = action; return apply(app.apply, &app); }

private:
	OSStatus mDefaultError;
};


//
// CFURLAccess wrappers for specific purposes
//
CFDataRef cfLoadFile(CFURLRef url);
CFDataRef cfLoadFile(int fd, size_t bytes);
inline CFDataRef cfLoadFile(CFStringRef path) { return cfLoadFile(CFTempURL(path)); }
inline CFDataRef cfLoadFile(const std::string &path) { return cfLoadFile(CFTempURL(path)); }
inline CFDataRef cfLoadFile(const char *path) { return cfLoadFile(CFTempURL(path)); }


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
        mCount = (UInt32)CFArrayGetCount(arrayRef);
        mVector = new VectorBase[mCount];
        for (UInt32 n = 0; n < mCount; n++)
            mVector[n] = convert(CFRefType(CFArrayGetValueAtIndex(arrayRef, n)));
    }
}


//
// Make CFArrays from stuff.
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

CFArrayRef makeCFArray(CFIndex count, ...) CF_RETURNS_RETAINED;
CFMutableArrayRef makeCFMutableArray(CFIndex count, ...) CF_RETURNS_RETAINED;


} // end namespace Security

#endif //_H_CFUTILITIES
