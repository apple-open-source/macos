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
// cssmdata.h -- Manager different CssmData types
//
#ifndef _H_CDSA_UTILITIES_CSSMDATA
#define _H_CDSA_UTILITIES_CSSMDATA

#include <Security/utilities.h>
#include <Security/cssmalloc.h>
#include <Security/refcount.h>


namespace Security {


//
// A convenient way to make a CssmData from a (const) string.
// Note that the underlying string is not memory-managed, so it
// should either be static or of sufficient (immutable) lifetime.
//
class StringData : public CssmData {
public:
    StringData(const char *s) : CssmData(const_cast<char *>(s), strlen(s)) { }
	StringData(const std::string &s) : CssmData(const_cast<char *>(s.c_str()), s.size()) { }
};


//
// A CssmData bundled up with a data buffer it refers to
//
template <size_t size>
struct DataBuffer : public CssmData {
	unsigned char buffer[size];
	DataBuffer() : CssmData(buffer, size) { }
};


//
// Comparing CssmDatas for equality.
// Note: No ordering is established here.
// Both CSSM_DATAs have to exist.
//
bool operator == (const CSSM_DATA &d1, const CSSM_DATA &d2);
inline bool operator != (const CSSM_DATA &d1, const CSSM_DATA &d2)
{ return !(d1 == d2); }


//
// The following pseudo-code describes what (at minimum) is required for a class
// to be a "PseudoData". PseudoData arguments are used in templates.
//
// class PseudoData {
//	void *data() const ...
//  size_t length() const ...
//  operator const CssmData &() const ...
// }
//
// All this can be satisfied, of course, by inheriting from CssmData.
//


//
// A common virtual parent for CssmData-like objects that actively manage the
// allocation status of their data blob. Note that this is about allocating
// the data(), not the CssmData structure itself.
// The ManagedData layer provides for little active memory management, since
// the underlying strategies are potentially very disparate. It does however
// have a well defined interface for *yielding up* its data for copying or transfer.
//
class CssmManagedData {
public:
	CssmManagedData(CssmAllocator &alloc) : allocator(alloc) { }
	virtual ~CssmManagedData();
	
	CssmAllocator &allocator;
	
	virtual operator const CssmData & () const { return get(); }
	template <class T> T *data() const	{ return reinterpret_cast<T *>(data()); }
	void *data() const					{ return get().data(); }
	size_t length() const				{ return get().length(); }
	
	virtual CssmData &get() const throw() = 0; // get shared copy, no ownership change
	virtual CssmData release() = 0;		// give up copy, ownership is transferred
	virtual void reset() = 0;			// give up copy, data is discarded
};


inline bool operator == (const CssmManagedData &d1, const CssmData &d2)
{ return d1.get() == d2; }

inline bool operator == (const CssmData &d1, const CssmManagedData &d2)
{ return d1 == d2.get(); }

inline bool operator == (const CssmManagedData &d1, const CssmManagedData &d2)
{ return d1.get() == d2.get(); }

inline bool operator != (const CssmManagedData &d1, const CssmData &d2)
{ return d1.get() != d2; }

inline bool operator != (const CssmData &d1, const CssmManagedData &d2)
{ return d1 != d2.get(); }

inline bool operator != (const CssmManagedData &d1, const CssmManagedData &d2)
{ return d1.get() != d2.get(); }


//
// A CssmOwnedData is a CssmManagedData that unilaterally owns its data storage.
// It has its CssmData object provided during construction.
//
class CssmOwnedData : public CssmManagedData {
public:
	CssmOwnedData(CssmAllocator &alloc, CssmData &mine) : CssmManagedData(alloc), referent(mine) { }

	CssmOwnedData(CssmAllocator &alloc, CSSM_DATA &mine)
	: CssmManagedData(alloc), referent(CssmData::overlay(mine)) { referent.clear(); }
	
	//
	// Basic retrievals (this echoes features of CssmData)
	//
	operator void * () const		{ return referent; }
	operator char * () const		{ return referent; }
	operator signed char * () const	 { return referent; }
	operator unsigned char * () const { return referent; }
	
	operator bool () const			{ return referent; }
	bool operator ! () const		{ return !referent; }
	
	size_t length() const			{ return referent.length(); }

	
	//
	// Basic allocators
	//
	void *malloc(size_t len)
	{
		// pseudo-atomic reallocation semantics
		CssmAutoPtr<uint8> alloc(allocator, allocator.malloc<uint8>(len));
		reset();
		return referent = CssmData(alloc.release(), len);
	}
	
	void *realloc(size_t newLen)
	{
		// CssmAllocator::realloc() should be pseudo-atomic (i.e. throw on error)
		return referent = CssmData(allocator.realloc<uint8>(referent.data(), newLen), newLen);
	}
    
	void length(size_t len)			{ realloc(len); }


	//
	// Manipulate existing data
	//
	void *append(const void *addData, size_t addLength)
	{
		size_t oldLength = length();
		realloc(oldLength + addLength);
		return memcpy(referent.at(oldLength), addData, addLength);
	}
	
	void *append(const CssmData &data)
	{ return append(data.data(), data.length()); }
	
	//
	// set() replaces current data with new, taking over ownership to the extent possible.
	//
	template <class T>
	void set(T *data, size_t length)
	{
		// assume that data was allocated by our allocator -- we can't be sure
		reset();
		referent = CssmData(data, length);
	}
	
	void set(CssmManagedData &source);
	void set(const CSSM_DATA &source)	{ set(source.Data, source.Length); }
	// NOTE: General template set() cannot be used because all subclasses of CssmManagedData
	// need to receive the special handling above. Use set(*.data(), *.length()) instead.


	//
	// copy() replaces current data with new, making a copy and leaving
	// the source intact.
	//
	template <class T>
	void copy(const T *data, size_t length)
	{
		// don't leave any open windows for Mr. Murphy
		CssmAutoPtr<void> newData(allocator, memcpy(allocator.malloc(length), data, length));
		reset();
		referent = CssmData(newData.release(), length);
	}
	
	void copy(const CssmData &source)
	{ if (&source != &referent) copy(source.data(), source.length()); }
	void copy(const CSSM_DATA &source)
	{ if (&source != &referent) copy(source.Data, source.Length); }
	void copy(CssmManagedData &source)		{ copy(source.get()); }
	template <class Data>
	void copy(const Data &source)			{ copy(source.data(), source.length()); }

	
	//
	// Assignment conservatively uses copy if allocator unknown, set if known
	//
	void operator = (CssmManagedData &source) { set(source); }
	void operator = (CssmOwnedData &source) { set(source); }
	void operator = (const CSSM_DATA &source) { copy(source); }
	
	CssmData &get() const throw()			{ return referent; }
	
protected:
	CssmData &referent;
};


//
// A CssmAutoData is a CssmOwnedData that includes its CssmData object.
// This is the very simple case: The object includes ownership, data object,
// and data storage.
//
class CssmAutoData : public CssmOwnedData {
public:
	CssmAutoData(CssmAllocator &alloc) : CssmOwnedData(alloc, mData) { }
	
	template <class Data>
	CssmAutoData(CssmAllocator &alloc, const Data &source) : CssmOwnedData(alloc, mData)
	{ *this = source; }

    CssmAutoData(CssmAutoData &source) : CssmOwnedData(source.allocator, mData)
    { set(source); }

	explicit CssmAutoData(CssmManagedData &source) : CssmOwnedData(source.allocator, mData)
	{ set(source); }
	
	CssmAutoData(CssmAllocator &alloc, const void *data, size_t length)
	: CssmOwnedData(alloc, mData)	{ copy(data, length); }
	
	~CssmAutoData()					{ allocator.free(mData); }
	
	CssmData release();
	void reset();
	
	// assignment (not usefully inherited)
	void operator = (CssmManagedData &source)		{ set(source); }
	void operator = (CssmOwnedData &source)			{ set(source); }
	void operator = (CssmAutoData &source)			{ set(source); }
	template <class Data>
	void operator = (const Data &source)			{ copy(source); }

private:
	CssmData mData;
};


//
// A CssmRemoteData is a CssmOwnedData that uses an external CssmData object.
// Its release operation clears an internal ownership flag but does not clear
// the CssmData values so they can be used to return values to an outside scope.
//
class CssmRemoteData : public CssmOwnedData {
public:
	CssmRemoteData(CssmAllocator &alloc, CssmData &mine)
	: CssmOwnedData(alloc, mine), iOwnTheData(true) { }
	
	CssmRemoteData(CssmAllocator &alloc, CSSM_DATA &mine)
	: CssmOwnedData(alloc, mine), iOwnTheData(true) { }
	
	~CssmRemoteData()
	{ if (iOwnTheData) allocator.free(referent); }
	
	CssmData release();
	void reset();
	
	// assignment (not usefully inherited)
	void operator = (CssmManagedData &source)		{ set(source); }
	void operator = (CssmOwnedData &source)			{ set(source); }
	void operator = (CssmAutoData &source)			{ set(source); }
	template <class Data>
	void operator = (const Data &source)			{ copy(source); }
	
private:
	bool iOwnTheData;
};


//
// CssmPolyData
//
// Used by functions that take a CssmData and would like to allow it to be
// initialized with a static string, int or other basic type.  The function *must*
// copy the Data of the CssmPolyData when doing so if it is to be used
// after the function returns.  (For example by creating a CssmDataContainer from it).
class CssmPolyData : public CssmData {
	template <class T>
	uint8 *set(const T &it)
	{ return const_cast<uint8 *>(reinterpret_cast<const uint8 *>(&it)); }
public:
	template <class char_T>
	CssmPolyData(const char_T *s) : CssmData(const_cast<char_T *>(s), strlen(s)) {}
	CssmPolyData(const string &s) : CssmData(const_cast<char *>(s.c_str()), s.size()) {}
	CssmPolyData(const CSSM_DATA &data) : CssmData(data.Data, data.Length) {}

	// Don't use a template constructor (for T &) here - it would eat way too much
	CssmPolyData(const bool &t) : CssmData(set(t), sizeof(t)) { }
	CssmPolyData(const uint32 &t) : CssmData(set(t), sizeof(t)) { }
	CssmPolyData(const sint32 &t) : CssmData(set(t), sizeof(t)) { }
	CssmPolyData(const sint64 &t) : CssmData(set(t), sizeof(t)) { }
	CssmPolyData(const double &t) : CssmData(set(t), sizeof(t)) { }
	CssmPolyData(const StringPtr s) : CssmData (reinterpret_cast<char*>(s + 1), uint32 (s[0])) {}
};

class CssmDateData : public CssmData
{
public:
	CssmDateData(const CSSM_DATE &date);
private:
	uint8 buffer[8];
};

class CssmGuidData : public CssmData
{
public:
	CssmGuidData(const CSSM_GUID &guid);
private:
	char buffer[Guid::stringRepLength + 1];
};


//
// CssmDLPolyData
//
class CssmDLPolyData
{
public:
	CssmDLPolyData(const CSSM_DATA &data, CSSM_DB_ATTRIBUTE_FORMAT format)
	: mData(CssmData::overlay(data)), mFormat(format) {}

	// @@@ Don't use assert, but throw an exception.
	// @@@ Do a size check on mData as well.

	// @@@ This method is dangerous since the returned string is not guaranteed to be zero terminated.
	operator const char *() const
	{
		assert(mFormat == CSSM_DB_ATTRIBUTE_FORMAT_STRING
               || mFormat == CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE);
		return reinterpret_cast<const char *>(mData.Data);
	}
	operator bool() const
	{
		assert(mFormat == CSSM_DB_ATTRIBUTE_FORMAT_UINT32 || mFormat == CSSM_DB_ATTRIBUTE_FORMAT_SINT32);
		return *reinterpret_cast<uint32 *>(mData.Data);
	}
	operator uint32() const
	{
		assert(mFormat == CSSM_DB_ATTRIBUTE_FORMAT_UINT32);
		return *reinterpret_cast<uint32 *>(mData.Data);
	}
	operator const uint32 *() const
	{
		assert(mFormat == CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32);
		return reinterpret_cast<const uint32 *>(mData.Data);
	}
	operator sint32() const
	{
		assert(mFormat == CSSM_DB_ATTRIBUTE_FORMAT_SINT32);
		return *reinterpret_cast<sint32 *>(mData.Data);
	}
	operator double() const
	{
		assert(mFormat == CSSM_DB_ATTRIBUTE_FORMAT_REAL);
		return *reinterpret_cast<double *>(mData.Data);
	}
	operator CSSM_DATE () const;
	operator Guid () const;
	operator const CssmData &() const
	{
		return mData;
	}

private:
	const CssmData &mData;
	CSSM_DB_ATTRIBUTE_FORMAT mFormat;
};


//
// Non POD refcounted CssmData wrapper that own the data it refers to.
//
class CssmDataContainer : public CssmData, public RefCount
{
public:
    CssmDataContainer(CssmAllocator &inAllocator = CssmAllocator::standard()) :
	CssmData(), mAllocator(inAllocator) {}
	template <class T>
    CssmDataContainer(const T *data, size_t length, CssmAllocator &inAllocator = CssmAllocator::standard()) :
	CssmData(inAllocator.malloc(length), length), mAllocator(inAllocator) 
	{ if (length) ::memcpy(Data, data, length); }
	void clear() { if (Data) { mAllocator.free(Data); Data = NULL; Length = 0; } }
	void invalidate () {Data = NULL; Length = 0;}
	~CssmDataContainer() { if (Data) mAllocator.free(Data); }
	void append(const CssmPolyData &data)
	{
		uint32 newLength = Length + data.Length;
		Data = reinterpret_cast<uint8 *>(mAllocator.realloc(Data, newLength));
		memcpy(Data + Length, data.Data, data.Length);
		Length = newLength;
	}
	CssmDataContainer(const CssmDataContainer &other)
	: mAllocator(other.mAllocator)
	{
		Data = reinterpret_cast<uint8 *>(mAllocator.malloc(other.Length));
		memcpy(Data, other.Data, other.Length);
		Length = other.Length;
	}
	CssmDataContainer & operator = (const CSSM_DATA &other)
	{
		clear();
		Data = reinterpret_cast<uint8 *>(mAllocator.malloc(other.Length));
		memcpy(Data, other.Data, other.Length);
		Length = other.Length;
		return *this;
	}

public:
	CssmAllocator &mAllocator;

private:
	operator CssmDataContainer * () const;  // prohibit conversion-to-my-pointer
};

//
// CSSM_OIDs are CSSM_DATAs but will probably have different wrapping characteristics.
//
typedef CssmDataContainer CssmOidContainer;

template <class Container>
class CssmBuffer : public RefPointer<Container>
{
public:
    CssmBuffer() : RefPointer<Container>(new Container()) {} // XXX This should may just set ptr to NULL.
	template <class T>
    CssmBuffer(const T *data, size_t length, CssmAllocator &inAllocator = CssmAllocator::standard()) :
	RefPointer<Container>(new Container(data, length, inAllocator)) {}
    CssmBuffer(const CSSM_DATA &data, CssmAllocator &inAllocator = CssmAllocator::standard()) :
	RefPointer<Container>(new Container(data.Data, data.Length, inAllocator)) {}
	CssmBuffer(const CssmBuffer& other) : RefPointer<Container>(other) {}
	CssmBuffer(Container *p) : RefPointer<Container>(p) {}
	bool CssmBuffer::operator < (const CssmBuffer &other) const { return (**this) < (*other); }
};


} // end namespace Security

#endif // _H_CDSA_UTILITIES_CSSMDATA
