/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All Rights Reserved.
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
// cssmdata.h -- Manager different CssmData types
//
#ifndef _H_CDSA_UTILITIES_CSSMDATA
#define _H_CDSA_UTILITIES_CSSMDATA

#include <security_utilities/alloc.h>
#include <security_utilities/refcount.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssmerr.h>

namespace Security {


//
// User-friendlier CSSM_DATA thingies.
// CssmData is a PODWrapper for CSSM_DATA, but is also used throughout
// the security code as a "byte blob" representation.
//
class CssmData : public PodWrapper<CssmData, CSSM_DATA> {
public:
    CssmData() { Data = 0; Length = 0; }

    size_t length() const { return Length; }
    void *data() const { return Data; }
    void *end() const { return Data + Length; }
	
	//
	// Create a CssmData from any pointer-to-byte-sized-object and length.
	//
	CssmData(void *data, size_t length)
	{ Data = reinterpret_cast<UInt8 *>(data); Length = length; }
	CssmData(char *data, size_t length)
	{ Data = reinterpret_cast<UInt8 *>(data); Length = length; }
	CssmData(unsigned char *data, size_t length)
	{ Data = reinterpret_cast<UInt8 *>(data); Length = length; }
	CssmData(signed char *data, size_t length)
	{ Data = reinterpret_cast<UInt8 *>(data); Length = length; }
		
	// the void * form accepts too much; explicitly deny all other types
	private: template <class T> CssmData(T *, size_t); public:
	
	// explicitly construct from a data-oid source
	template <class T>
	explicit CssmData(const T &obj)
	{ Data = (UInt8 *)obj.data(); Length = obj.length(); }
	
	//
	// Do allow generic "wrapping" of any data structure, but make it conspicuous
	// since it's not necessarily the Right Thing (alignment and byte order wise).
	// Also note that the T & form removes const-ness, since there is no ConstCssmData.
	//
	template <class T>
	static CssmData wrap(const T &it)
	{ return CssmData(const_cast<void *>(reinterpret_cast<const void *>(&it)), sizeof(it)); }
	
	template <class T>
	static CssmData wrap(T *data, size_t length)
	{ return CssmData(const_cast<void *>(static_cast<const void *>(data)), length); }

	//
	// Automatically convert a CssmData to any pointer-to-byte-sized-type.
	//
	operator signed char * () const { return reinterpret_cast<signed char *>(Data); }
	operator unsigned char * () const { return reinterpret_cast<unsigned char *>(Data); }
	operator char * () const { return reinterpret_cast<char *>(Data); }
	operator void * () const { return reinterpret_cast<void *>(Data); }
	
	//
	// If you want to interpret the contents of a CssmData blob as a particular
	// type, you have to be more explicit to show that you know what you're doing.
	// See wrap() above.
	//
	template <class T>
	T *interpretedAs() const		{ return reinterpret_cast<T *>(Data); }

	template <class T>
	T *interpretedAs(CSSM_RETURN error) const
	{ return interpretedAs<T>(sizeof(T), error); }
	
	template <class T>
	T *interpretedAs(size_t len, CSSM_RETURN error) const
	{
		if (data() == NULL || length() != len) CssmError::throwMe(error);
		return interpretedAs<T>();
	}
	
public:
    void length(size_t newLength)	// shorten only
    { assert(newLength <= Length); Length = newLength; }

	void *at(off_t offset) const
	{ assert(offset >= 0 && (CSSM_SIZE)offset <= Length); return Data + offset; }
	void *at(off_t offset, size_t size) const	// length-checking version
	{ assert(offset >= 0 && (CSSM_SIZE)offset + size <= Length); return Data + offset; }
	
	template <class T> T *at(off_t offset) const { return reinterpret_cast<T *>(at(offset)); }
	template <class T> T *at(off_t offset, size_t size) const
	{ return reinterpret_cast<T *>(at(offset, size)); }
	
	unsigned char byte(off_t offset) const { return *at<unsigned char>(offset); }
	unsigned char &byte(off_t offset) { return *at<unsigned char>(offset); }
	
    void *use(size_t taken)			// logically remove some bytes
    { assert(taken <= Length); void *r = Data; Length -= taken; Data += taken; return r; }
	
	void clear()
	{ Data = NULL; Length = 0; }

    string toString () const;	// convert to string type (no trailing null)
	string toHex() const;		// hex string of binary blob
	string toOid() const;		// standard OID string encoding (1.2.3...)
	void fromHex(const char *digits); // fill myself with hex data (no allocation)

    operator bool () const { return Data != NULL; }
    bool operator ! () const { return Data == NULL; }
    bool operator < (const CssmData &other) const;
	bool operator == (const CssmData &other) const
	{ return length() == other.length() && !memcmp(data(), other.data(), length()); }
	bool operator != (const CssmData &other) const
	{ return !(*this == other); }
    
    // Extract fixed-format data from a CssmData. Fixes any alignment trouble for you.
    template <class T>
    void extract(T &destination, CSSM_RETURN error = CSSM_ERRCODE_INVALID_DATA) const
    {
        if (length() != sizeof(destination) || data() == NULL)
            CssmError::throwMe(error);
        memcpy(&destination, data(), sizeof(destination));
    }
};


inline bool CssmData::operator < (const CssmData &other) const
{
    if (Length != other.Length) // If lengths are not equal the shorter data is smaller.
        return Length < other.Length;
    if (Length == 0) // If lengths are both zero ignore the Data.
        return false;
    if (Data == NULL || other.Data == NULL)	// arbitrary (but consistent) ordering
        return Data < other.Data;
    return memcmp(Data, other.Data, Length) < 0; // Do a lexicographic compare on equal sized Data.
}


//
// CSSM_OIDs are CSSM_DATAs but will probably have different wrapping characteristics.
//
typedef CssmData CssmOid;


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
// to be a "PseudoData". PseudoData arguments ("DataOids") are used in templates.
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
	CssmManagedData(Allocator &alloc) : allocator(alloc) { }
	virtual ~CssmManagedData();
	
	Allocator &allocator;
	
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
	CssmOwnedData(Allocator &alloc, CssmData &mine) : CssmManagedData(alloc), referent(mine) { }

	CssmOwnedData(Allocator &alloc, CSSM_DATA &mine)
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
		// Allocator::realloc() should be pseudo-atomic (i.e. throw on error)
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
	
	CssmData &get() const throw();
	
public:
	void fromOid(const char *oid);		// fill from text OID form (1.2.3...)
	
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
	CssmAutoData(Allocator &alloc) : CssmOwnedData(alloc, mData) { }
	
	template <class Data>
	CssmAutoData(Allocator &alloc, const Data &source) : CssmOwnedData(alloc, mData)
	{ *this = source; }

    CssmAutoData(CssmAutoData &source) : CssmOwnedData(source.allocator, mData)
    { set(source); }

	explicit CssmAutoData(CssmManagedData &source) : CssmOwnedData(source.allocator, mData)
	{ set(source); }
	
	CssmAutoData(Allocator &alloc, const void *data, size_t length)
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
	CssmRemoteData(Allocator &alloc, CssmData &mine)
	: CssmOwnedData(alloc, mine), iOwnTheData(true) { }
	
	CssmRemoteData(Allocator &alloc, CSSM_DATA &mine)
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
	CssmPolyData(const unsigned long &t) : CssmData(set(t), sizeof(t)) { }
	CssmPolyData(const CSSM_GUID &t) : CssmData(set(t), sizeof(t)) { }
	CssmPolyData(const StringPtr s) : CssmData (reinterpret_cast<char*>(s + 1), uint32 (s[0])) {}
};

class CssmDateData : public CssmData
{
public:
	CssmDateData(const CSSM_DATE &date);
private:
	uint8 buffer[8];
};


//
// Non POD refcounted CssmData wrapper that own the data it refers to.
//
class CssmDataContainer : public CssmData, public RefCount
{
public:
    CssmDataContainer(Allocator &inAllocator = Allocator::standard()) :
	CssmData(), mAllocator(inAllocator) {}
	template <class T>
    CssmDataContainer(const T *data, size_t length, Allocator &inAllocator = Allocator::standard()) :
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
	Allocator &mAllocator;

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
    CssmBuffer(const T *data, size_t length, Allocator &inAllocator = Allocator::standard()) :
	RefPointer<Container>(new Container(data, length, inAllocator)) {}
    CssmBuffer(const CSSM_DATA &data, Allocator &inAllocator = Allocator::standard()) :
	RefPointer<Container>(new Container(data.Data, data.Length, inAllocator)) {}
	CssmBuffer(const CssmBuffer& other) : RefPointer<Container>(other) {}
	CssmBuffer(Container *p) : RefPointer<Container>(p) {}
	bool operator < (const CssmBuffer &other) const { return (**this) < (*other); }
};


} // end namespace Security

#endif // _H_CDSA_UTILITIES_CSSMDATA
