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


// cssmdb.h
//
// classes for the DL related data structures
//

#ifndef _H_CDSA_UTILITIES_CSSMDB
#define _H_CDSA_UTILITIES_CSSMDB

#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmpods.h>
#include <security_cdsa_utilities/cssmalloc.h>
#include <security_cdsa_utilities/walkers.h>
#include <security_cdsa_utilities/cssmdbname.h>


namespace Security {


//
// Template class to build and maintain external arrays.
// Feel free to add and vector<> member functions and behaviours as needed.
//
// This class differs from vector mainly because it does not construct or
// destruct any of the elements it contains.  Rather it zero fills the
// storage and returns references to elements.
// Also it does not implement insert(), erase() or assign().  It does implement
// which is equivalent to calling *insert(end()) on a vector.
//
template <class _Tp>
class ArrayBuilder {
public:
	typedef _Tp value_type;
	typedef value_type* pointer;
	typedef const value_type* const_pointer;
	typedef value_type* iterator;
	typedef const value_type* const_iterator;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef uint32 size_type;
	typedef ptrdiff_t difference_type;

	typedef reverse_iterator<const_iterator> const_reverse_iterator;
	typedef reverse_iterator<iterator> reverse_iterator;

protected:
  void insert_aux(iterator __position, const _Tp& __x);
  void insert_aux(iterator __position);

public:
	iterator begin() { return mArray; }
	const_iterator begin() const { return mArray; }
	iterator end() { return &mArray[mSize]; }
	const_iterator end() const { return &mArray[mSize]; }
	
	reverse_iterator rbegin()
	{ return reverse_iterator(end()); }
	const_reverse_iterator rbegin() const
	{ return const_reverse_iterator(end()); }
	reverse_iterator rend()
	{ return reverse_iterator(begin()); }
	const_reverse_iterator rend() const
	{ return const_reverse_iterator(begin()); }

	// Must be defined in base class.
	//size_type size() const
	//{ return mSize; }
	size_type max_size() const
	{ return size_type(-1) / sizeof(_Tp); }
	size_type capacity() const
	{ return mCapacity; }
	bool empty() const
	{ return begin() == end(); }

	ArrayBuilder(pointer &array, size_type &size, size_type capacity = 0, Allocator &allocator = Allocator::standard()) :
	mArray(array), mSize(size), mCapacity(capacity), mAllocator(allocator)
	{
#if BUG_GCC
		mArray = reinterpret_cast<pointer>(mAllocator.malloc(sizeof(value_type) * mCapacity));
#else
		mArray = reinterpret_cast<pointer>(mAllocator.malloc(sizeof(value_type) * mCapacity));
		//mArray = mAllocator.alloc(mCapacity);
#endif
		memset(mArray, 0, sizeof(value_type) * mCapacity);
		mSize = 0;
	}
	~ArrayBuilder() { mAllocator.free(mArray); }

	reference front() { return *begin(); }
	const_reference front() const { return *begin(); }
	reference back() { return *(end() - 1); }
	const_reference back() const { return *(end() - 1); }

	void reserve(size_type newCapacity)
	{
		if (newCapacity > mCapacity)
		{
#if BUG_GCC
			mArray = reinterpret_cast<pointer>(mAllocator.realloc(mArray, sizeof(value_type) * newCapacity));
#else
			mArray = reinterpret_cast<pointer>(mAllocator.realloc(mArray, sizeof(value_type) * newCapacity));
			//mArray = mAllocator.realloc<value_type>(mArray, newCapacity));
#endif
			memset(&mArray[mCapacity], 0, sizeof(value_type) * (newCapacity - mCapacity));
			mCapacity = newCapacity;
		}
	}

	// XXX Replace by push_back and insert.
	reference add()
	{
		if (mSize >= mCapacity)
			reserve(max(mSize + 1, mCapacity ? 2 * mCapacity : 1));

		return mArray[mSize++];
	}

	const_pointer get() const { return mArray; }
	pointer release() { const_pointer array = mArray; mArray = NULL; return array; }
	void clear() { if (mSize) { memset(mArray, 0, sizeof(value_type) * mSize); } mSize = 0; }

	// Must be defined in base class.
	//reference at(size_type ix) { return mArray[ix]; }
	//const_reference at(size_type ix) const { return mArray[ix]; }
	//reference operator[] (size_type ix) { assert(ix < size()); return at(ix); }
	//const_reference operator[] (size_type ix) const { assert(ix < size()); return at(ix); }
protected:
	Allocator &allocator() const { return mAllocator; }

private:

	pointer &mArray;
	size_type &mSize;
	size_type mCapacity;
	Allocator &mAllocator;
};


//
// A CSSM_DL_DB_LIST wrapper.
// Note that there is a DLDBList class elsewhere that is quite
// unrelated to this structure.
//
class CssmDlDbHandle : public PodWrapper<CssmDlDbHandle, CSSM_DL_DB_HANDLE> {
public:
	CssmDlDbHandle() { clearPod(); }
	CssmDlDbHandle(CSSM_DL_HANDLE dl, CSSM_DB_HANDLE db) { DLHandle = dl; DBHandle = db; }
	
	CSSM_DL_HANDLE dl() const	{ return DLHandle; }
	CSSM_DB_HANDLE db() const	{ return DBHandle; }
	
	operator bool() const		{ return DLHandle && DBHandle; }
};

inline bool operator < (const CSSM_DL_DB_HANDLE &h1, const CSSM_DL_DB_HANDLE &h2)
{
	return h1.DLHandle < h2.DLHandle
		|| (h1.DLHandle == h2.DLHandle && h1.DBHandle < h2.DBHandle);
}

inline bool operator == (const CSSM_DL_DB_HANDLE &h1, const CSSM_DL_DB_HANDLE &h2)
{
	return h1.DLHandle == h2.DLHandle && h1.DBHandle == h2.DBHandle;
}

inline bool operator != (const CSSM_DL_DB_HANDLE &h1, const CSSM_DL_DB_HANDLE &h2)
{
	return h1.DLHandle != h2.DLHandle || h1.DBHandle != h2.DBHandle;
}


class CssmDlDbList : public PodWrapper<CssmDlDbList, CSSM_DL_DB_LIST> {
public:
	uint32 count() const		{ return NumHandles; }
	uint32 &count()				{ return NumHandles; }
	CssmDlDbHandle *handles() const { return CssmDlDbHandle::overlay(DLDBHandle); }
	CssmDlDbHandle * &handles()	{ return CssmDlDbHandle::overlayVar(DLDBHandle); }

	CssmDlDbHandle &operator [] (uint32 ix) const
	{ assert(ix < count()); return CssmDlDbHandle::overlay(DLDBHandle[ix]); }
	
	void setDlDbList(uint32 n, CSSM_DL_DB_HANDLE *list)
	{ count() = n; handles() = CssmDlDbHandle::overlay(list); }
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
// CssmDbAttributeInfo pod wrapper for CSSM_DB_ATTRIBUTE_INFO
//
class CssmDbAttributeInfo : public PodWrapper<CssmDbAttributeInfo, CSSM_DB_ATTRIBUTE_INFO>
{
public:
	CssmDbAttributeInfo(const CSSM_DB_ATTRIBUTE_INFO &attr)
	{ assignPod(attr); }
	
	CssmDbAttributeInfo(const char *name,
		CSSM_DB_ATTRIBUTE_FORMAT vFormat = CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX);
	CssmDbAttributeInfo(const CSSM_OID &oid,
		CSSM_DB_ATTRIBUTE_FORMAT vFormat = CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX);
	CssmDbAttributeInfo(uint32 id,
		CSSM_DB_ATTRIBUTE_FORMAT vFormat = CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX);

	CSSM_DB_ATTRIBUTE_NAME_FORMAT nameFormat() const { return AttributeNameFormat; }
	void nameFormat(CSSM_DB_ATTRIBUTE_NAME_FORMAT nameFormat) { AttributeNameFormat = nameFormat; }

	CSSM_DB_ATTRIBUTE_FORMAT format() const { return AttributeFormat; }
	void format(CSSM_DB_ATTRIBUTE_FORMAT format) { AttributeFormat = format; }

	const char *stringName() const
	{
		assert(nameFormat() == CSSM_DB_ATTRIBUTE_NAME_AS_STRING);
		return Label.AttributeName;
	}
	const CssmOid &oidName() const
	{
		assert(nameFormat() == CSSM_DB_ATTRIBUTE_NAME_AS_OID);
		return CssmOid::overlay(Label.AttributeOID);
	}
	uint32 intName() const
	{
		assert(nameFormat() == CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER);
		return Label.AttributeID;
	}

	operator const char *() const { return stringName(); }
	operator const CssmOid &() const { return oidName(); }
	operator uint32() const { return intName(); }

	bool operator <(const CssmDbAttributeInfo& other) const;
	bool operator ==(const CssmDbAttributeInfo& other) const;
	bool operator !=(const CssmDbAttributeInfo& other) const
	{ return !(*this == other); }
};

//
// CssmDbRecordAttributeInfo pod wrapper for CSSM_DB_RECORD_ATTRIBUTE_INFO
//
class CssmDbRecordAttributeInfo : public PodWrapper<CssmDbRecordAttributeInfo, CSSM_DB_RECORD_ATTRIBUTE_INFO>
{
public:
	CssmDbRecordAttributeInfo()
	{ DataRecordType = CSSM_DL_DB_RECORD_ANY; }
	
	CssmDbRecordAttributeInfo(CSSM_DB_RECORDTYPE recordType, uint32 numberOfAttributes,
							  CSSM_DB_ATTRIBUTE_INFO_PTR attributeInfo)
	{
		DataRecordType = recordType;
		NumberOfAttributes = numberOfAttributes;
		AttributeInfo = attributeInfo;
	}

	CSSM_DB_RECORDTYPE recordType() const { return DataRecordType; }
	void recordType(CSSM_DB_RECORDTYPE recordType) { DataRecordType = recordType; }

	uint32 size() const { return NumberOfAttributes; }

	// attribute access
	CssmDbAttributeInfo *&attributes()
	{ return CssmDbAttributeInfo::overlayVar(AttributeInfo); }
	CssmDbAttributeInfo *attributes() const
	{ return CssmDbAttributeInfo::overlay(AttributeInfo); }
    CssmDbAttributeInfo &at(uint32 ix) const
	{ assert(ix < size()); return attributes()[ix]; }

    CssmDbAttributeInfo &operator [] (uint32 ix) const { return at(ix); }
};

//
// CssmAutoDbRecordAttributeInfo pod wrapper for CSSM_DB_RECORD_ATTRIBUTE_INFO
//
class CssmAutoDbRecordAttributeInfo: public CssmDbRecordAttributeInfo, public ArrayBuilder<CssmDbAttributeInfo>
{
public:
	CssmAutoDbRecordAttributeInfo(uint32 capacity = 0, Allocator &allocator = Allocator::standard()) :
	CssmDbRecordAttributeInfo(),
	ArrayBuilder<CssmDbAttributeInfo>(CssmDbAttributeInfo::overlayVar(AttributeInfo),
									  NumberOfAttributes, capacity, allocator) {}
};


//
// CssmDbAttributeData pod wrapper for CSSM_DB_ATTRIBUTE_DATA
//
class CssmDbAttributeData : public PodWrapper<CssmDbAttributeData, CSSM_DB_ATTRIBUTE_DATA>
{
public:
	CssmDbAttributeData() { NumberOfValues = 0; Value = NULL; }
	CssmDbAttributeData(const CSSM_DB_ATTRIBUTE_DATA &attr)
	{ assignPod(attr); }
	CssmDbAttributeData(const CSSM_DB_ATTRIBUTE_INFO &info)
	{ Info = info; NumberOfValues = 0; Value = NULL; }

	CssmDbAttributeInfo &info() { return CssmDbAttributeInfo::overlay(Info); }
	const CssmDbAttributeInfo &info() const { return CssmDbAttributeInfo::overlay(Info); }
	void info (const CSSM_DB_ATTRIBUTE_INFO &inInfo) { Info = inInfo; }

	CSSM_DB_ATTRIBUTE_FORMAT format() const { return info().format(); }
	void format(CSSM_DB_ATTRIBUTE_FORMAT f) { info().format(f); }

	uint32 size() const { return NumberOfValues; }
	CssmData *&values() { return CssmData::overlayVar(Value); }
	CssmData *values() const { return CssmData::overlay(Value); }

	CssmData &at(unsigned int ix) const
	{
		if (ix >= size()) CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);
		return values()[ix];
	}
	
	CssmData &operator [] (unsigned int ix) const { return at(ix); }

	template <class T>
    T at(unsigned int ix) const { return CssmDLPolyData(Value[ix], format()); }

	// this is intentionally unspecified since it could lead to bugs; the
	// data is not guaranteed to be NULL-terminated
	// operator const char *() const;

	operator string() const;
	operator const Guid &() const;
	operator bool() const;
	operator uint32() const;
	operator const uint32 *() const;
	operator sint32() const;
	operator double() const;
	operator const CssmData &() const;
	
	// set values without allocation (caller owns the data contents)
	void set(CssmData &data)	{ set(1, &data); }
	void set(uint32 count, CssmData *datas) { NumberOfValues = count; Value = datas; }

	// Set the value of this Attr (assuming it was not set before).
	void set(const CSSM_DB_ATTRIBUTE_INFO &inInfo, const CssmPolyData &inValue,
			 Allocator &inAllocator);

	// copy (just) the return-value part from another AttributeData to this one
	void copyValues(const CssmDbAttributeData &source, Allocator &alloc);

	// Set the value of this Attr (which must be unset so far)
	void set(const CSSM_DB_ATTRIBUTE_DATA &source, Allocator &alloc)
	{
		info(source.Info);
		copyValues(source, alloc);
	}

	// Add a value to this attribute.
	void add(const CssmPolyData &inValue, Allocator &inAllocator);

	void add(const char *value, Allocator &alloc)
	{ format(CSSM_DB_ATTRIBUTE_FORMAT_STRING); add(CssmPolyData(value), alloc); }
	
	void add(const std::string &value, Allocator &alloc)
	{ format(CSSM_DB_ATTRIBUTE_FORMAT_STRING); add(CssmPolyData(value), alloc); }
	
	void add(uint32 value, Allocator &alloc)
	{ format(CSSM_DB_ATTRIBUTE_FORMAT_UINT32); add(CssmPolyData(value), alloc); }
	
	void add(sint32 value, Allocator &alloc)
	{ format(CSSM_DB_ATTRIBUTE_FORMAT_SINT32); add(CssmPolyData(value), alloc); }
	
	void add(const CssmData &value, Allocator &alloc)
	{ format(CSSM_DB_ATTRIBUTE_FORMAT_BLOB); add(CssmPolyData(value), alloc); }

	void add(const CssmDbAttributeData &src, Allocator &inAllocator);

	// delete specific values if they are present in this attribute data
	bool deleteValue(const CssmData &src, Allocator &inAllocator);
	void deleteValues(const CssmDbAttributeData &src, Allocator &inAllocator);

	void deleteValues(Allocator &inAllocator);

	bool operator <(const CssmDbAttributeData& other) const;
};


//
// CssmDbRecordAttributeData pod wrapper for CSSM_DB_RECORD_ATTRIBUTE_DATA
//
class CssmDbRecordAttributeData : public PodWrapper<CssmDbRecordAttributeData, CSSM_DB_RECORD_ATTRIBUTE_DATA>
{
public:
	CssmDbRecordAttributeData()
	{ clearPod(); DataRecordType = CSSM_DL_DB_RECORD_ANY; }

	CSSM_DB_RECORDTYPE recordType() const { return DataRecordType; }
	void recordType(CSSM_DB_RECORDTYPE recordType) { DataRecordType = recordType; }

	uint32 semanticInformation() const { return SemanticInformation; }
	void semanticInformation(uint32 semanticInformation) { SemanticInformation = semanticInformation; }

	uint32 size() const { return NumberOfAttributes; }
	CssmDbAttributeData *&attributes()
	{ return CssmDbAttributeData::overlayVar(AttributeData); }
	CssmDbAttributeData *attributes() const
	{ return CssmDbAttributeData::overlay(AttributeData); }

	// Attributes by position
    CssmDbAttributeData &at(unsigned int ix) const
	{ assert(ix < size()); return attributes()[ix]; }

    CssmDbAttributeData &operator [] (unsigned int ix) const { return at(ix); }

    void deleteValues(Allocator &allocator)
	{ for (uint32 ix = 0; ix < size(); ++ix) at(ix).deleteValues(allocator); }

	CssmDbAttributeData *find(const CSSM_DB_ATTRIBUTE_INFO &inInfo);

	bool operator <(const CssmDbRecordAttributeData& other) const;
};


//
// CssmAutoDbRecordAttributeData
//
class CssmAutoDbRecordAttributeData : public CssmDbRecordAttributeData, public ArrayBuilder<CssmDbAttributeData>
{
public:
	CssmAutoDbRecordAttributeData(uint32 capacity = 0,
								  Allocator &valueAllocator = Allocator::standard(),
								  Allocator &dataAllocator = Allocator::standard()) :
	CssmDbRecordAttributeData(),
	ArrayBuilder<CssmDbAttributeData>(CssmDbAttributeData::overlayVar(AttributeData),
									  NumberOfAttributes, capacity, dataAllocator),
	mValueAllocator(valueAllocator) {}
	~CssmAutoDbRecordAttributeData();

	void clear();
    void deleteValues() { CssmDbRecordAttributeData::deleteValues(mValueAllocator); }
	void invalidate();

	CssmDbAttributeData &add() { return ArrayBuilder<CssmDbAttributeData>::add(); } // XXX using doesn't work here.
	CssmDbAttributeData &add(const CSSM_DB_ATTRIBUTE_INFO &info);
	CssmDbAttributeData &add(const CSSM_DB_ATTRIBUTE_INFO &info, const CssmPolyData &value);

	// So clients can pass this as the allocator argument to add()
	operator Allocator &() const { return mValueAllocator; }
private:
	Allocator &mValueAllocator;
	
	CssmDbAttributeData* findAttribute (const CSSM_DB_ATTRIBUTE_INFO &info);
	CssmDbAttributeData& getAttributeReference (const CSSM_DB_ATTRIBUTE_INFO &info);
};


//
// CssmSelectionPredicate a PodWrapper for CSSM_SELECTION_PREDICATE
//
class CssmSelectionPredicate : public PodWrapper<CssmSelectionPredicate, CSSM_SELECTION_PREDICATE> {
public:
	CssmSelectionPredicate() { clearPod(); }

	CSSM_DB_OPERATOR dbOperator() const { return DbOperator; }
	void dbOperator(CSSM_DB_OPERATOR dbOperator) { DbOperator = dbOperator; }

	CssmSelectionPredicate(CSSM_DB_OPERATOR inDbOperator)
	{ dbOperator(inDbOperator); Attribute.NumberOfValues = 0; Attribute.Value = NULL; }

	CssmDbAttributeData &attribute() { return CssmDbAttributeData::overlay(Attribute); }
	const CssmDbAttributeData &attribute() const { return CssmDbAttributeData::overlay(Attribute); }

	// Set the value of this CssmSelectionPredicate (assuming it was not set before).
	void set(const CSSM_DB_ATTRIBUTE_INFO &inInfo,
			 const CssmPolyData &inValue, Allocator &inAllocator)
	{ attribute().set(inInfo, inValue, inAllocator); }

	// Set the value of this CssmSelectionPredicate using another CssmSelectionPredicate's value.
	void set(const CSSM_SELECTION_PREDICATE &other, Allocator &inAllocator)
	{ DbOperator = other.DbOperator; attribute().set(other.Attribute, inAllocator); }

	// Add a value to the list of values for this CssmSelectionPredicate.
	void add(const CssmPolyData &inValue, Allocator &inAllocator)
	{ attribute().add(inValue, inAllocator); }

	void deleteValues(Allocator &inAllocator) { attribute().deleteValues(inAllocator); }
};

class CssmQuery : public PodWrapper<CssmQuery, CSSM_QUERY> {
public:
    CssmQuery(CSSM_DB_RECORDTYPE type = CSSM_DL_DB_RECORD_ANY)
    { clearPod(); RecordType = type; }
	
	// copy or assign flat from CSSM_QUERY
	CssmQuery(const CSSM_QUERY &q) { assignPod(q); }
	CssmQuery &operator = (const CSSM_QUERY &q) { assignPod(q); return *this; }
	
	// flat copy and change record type
	CssmQuery(const CssmQuery &q, CSSM_DB_RECORDTYPE type)
	{ *this = q; RecordType = type; }

	CSSM_DB_RECORDTYPE recordType() const { return RecordType; }
	void recordType(CSSM_DB_RECORDTYPE recordType)  { RecordType = recordType; }

	CSSM_DB_CONJUNCTIVE conjunctive() const { return Conjunctive; }
	void conjunctive(CSSM_DB_CONJUNCTIVE conjunctive)  { Conjunctive = conjunctive; }

	CSSM_QUERY_LIMITS queryLimits() const { return QueryLimits; }
	void queryLimits(CSSM_QUERY_LIMITS queryLimits)  { QueryLimits = queryLimits; }

	CSSM_QUERY_FLAGS queryFlags() const { return QueryFlags; }
	void queryFlags(CSSM_QUERY_FLAGS queryFlags)  { QueryFlags = queryFlags; }

	uint32 size() const { return NumSelectionPredicates; }
	
	CssmSelectionPredicate *&predicates()
	{ return CssmSelectionPredicate::overlayVar(SelectionPredicate); }
	CssmSelectionPredicate *predicates() const
	{ return CssmSelectionPredicate::overlay(SelectionPredicate); }

	CssmSelectionPredicate &at(uint32 ix) const
	{ assert(ix < size()); return predicates()[ix]; }

	CssmSelectionPredicate &operator[] (uint32 ix) const { return at(ix); }
	
	void set(uint32 count, CSSM_SELECTION_PREDICATE *preds)
	{ NumSelectionPredicates = count; SelectionPredicate = preds; }

    void deleteValues(Allocator &allocator)
	{ for (uint32 ix = 0; ix < size(); ++ix) at(ix).deleteValues(allocator); }
};


class CssmAutoQuery : public CssmQuery, public ArrayBuilder<CssmSelectionPredicate> {
public:
	CssmAutoQuery(const CSSM_QUERY &query, Allocator &allocator = Allocator::standard());
	CssmAutoQuery(uint32 capacity = 0, Allocator &allocator = Allocator::standard()) :
	ArrayBuilder<CssmSelectionPredicate>(CssmSelectionPredicate::overlayVar(SelectionPredicate),
										 NumSelectionPredicates,
										 capacity, allocator) {}
	~CssmAutoQuery();
	void clear();
    void deleteValues() { CssmQuery::deleteValues(allocator()); }

	CssmSelectionPredicate &add() { return ArrayBuilder<CssmSelectionPredicate>::add(); }
	CssmSelectionPredicate &add(CSSM_DB_OPERATOR dbOperator, const CSSM_DB_ATTRIBUTE_INFO &info, const CssmPolyData &value);

	// So clients can pass this as the allocator argument to add()
	operator Allocator &() const { return allocator(); }
};


//
// DLDbIdentifier
//
class DLDbIdentifier
{
protected:
    class Impl : public RefCount
    {
        NOCOPY(Impl)
    public:
        Impl(const CSSM_SUBSERVICE_UID &ssuid,const char *DbName,const CSSM_NET_ADDRESS *DbLocation) :
            mCssmSubserviceUid(ssuid),mDbName(DbName,DbLocation) {}

        ~Impl() {} // Must be public since RefPointer uses it.

        // Accessors
        const CssmSubserviceUid &ssuid() const { return mCssmSubserviceUid; }
        const char *dbName() const { return mDbName.dbName(); }
        const CssmNetAddress *dbLocation() const { return mDbName.dbLocation(); }

        // comparison (simple lexicographic)
        bool operator < (const Impl &other) const;
        bool operator == (const Impl &other) const;
    private:
        // Private member variables
        CssmSubserviceUid mCssmSubserviceUid;
        DbName mDbName;
    };

public:
    // Constructors
    DLDbIdentifier() {}
    DLDbIdentifier(const CSSM_SUBSERVICE_UID &ssuid, const char *DbName, const CSSM_NET_ADDRESS *DbLocation)
        : mImpl(new Impl(ssuid, DbName, DbLocation)) {}
	DLDbIdentifier(const char *name, const Guid &guid, uint32 ssid, uint32 sstype,
		const CSSM_NET_ADDRESS *location = NULL)
		: mImpl(new Impl(CssmSubserviceUid(guid, NULL, ssid, sstype), name, location)) { }

	// Conversion Operators
	bool operator !() const { return !mImpl; }
	operator bool() const { return mImpl; }

    // Operators
	bool operator <(const DLDbIdentifier &other) const
	{ return mImpl && other.mImpl ? *mImpl < *other.mImpl : mImpl.get() < other.mImpl.get(); }
	bool operator ==(const DLDbIdentifier &other) const
	{ return mImpl && other.mImpl ? *mImpl == *other.mImpl : mImpl.get() == other.mImpl.get(); }
	DLDbIdentifier &operator =(const DLDbIdentifier &other)
	{ mImpl = other.mImpl; return *this; }

    // Accessors
    const CssmSubserviceUid &ssuid() const { return mImpl->ssuid(); }
    const char *dbName() const { return mImpl->dbName(); }
    const CssmNetAddress *dbLocation() const { return mImpl->dbLocation(); }
    bool IsImplEmpty() const {return mImpl == NULL;}
    
    RefPointer<Impl> mImpl;
};

// Wrappers for index-related CSSM objects.

class CssmDbIndexInfo : public PodWrapper<CssmDbIndexInfo, CSSM_DB_INDEX_INFO>
{
public:
	CssmDbIndexInfo(const CSSM_DB_INDEX_INFO &attr)
	{ (CSSM_DB_INDEX_INFO &)*this = attr; }

	CSSM_DB_INDEX_TYPE indexType() const { return IndexType; }
	void indexType(CSSM_DB_INDEX_TYPE indexType) { IndexType = indexType; }

	CSSM_DB_INDEXED_DATA_LOCATION dataLocation() const { return IndexedDataLocation; }
	void dataLocation(CSSM_DB_INDEXED_DATA_LOCATION dataLocation)
	{
		IndexedDataLocation = dataLocation;
	}
	
	const CssmDbAttributeInfo &attributeInfo() const
	{
		return CssmDbAttributeInfo::overlay(Info);
	}
};


namespace DataWalkers {


//
// DLDbIdentifiers don't walk directly because they have Impl structure and use strings.
// Happily, they are easily transcribed into a walkable form.
//
struct DLDbFlatIdentifier {
    CssmSubserviceUid *uid;		// module reference
    char *name;					// string name
    CssmNetAddress *address;	// optional network address

    DLDbFlatIdentifier(const DLDbIdentifier &ident) :
		uid(const_cast<CssmSubserviceUid *>(&ident.ssuid())),
		name(const_cast<char *>(ident.dbName())),
		address(const_cast<CssmNetAddress *>(ident.dbLocation()))
		{ }

    operator DLDbIdentifier ()	{ return DLDbIdentifier(*uid, name, address); }
};

template<class Action>
DLDbFlatIdentifier *walk(Action &operate, DLDbFlatIdentifier * &ident)
{
    operate(ident);
    if (ident->uid)
        walk(operate, ident->uid);
    walk(operate, ident->name);
    if (ident->address)
        walk(operate, ident->address);
    return ident;
}


//
// Walkers for the byzantine data structures of the DL universe.
// Geez, what WERE they smoking when they invented this?
//

// DbAttributeInfos
template<class Action>
void enumerate(Action &operate, CssmDbAttributeInfo &info)
{
	switch (info.nameFormat()) {
	case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		walk(operate, info.Label.AttributeName);
		break;
	case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
		walk(operate, info.Label.AttributeOID);
		break;
	default:
		break;
	}
}

template <class Action>
void walk(Action &operate, CssmDbAttributeInfo &info)
{
	operate(info);
	enumerate(operate, info);
}

template <class Action>
CssmDbAttributeInfo *walk(Action &operate, CssmDbAttributeInfo * &info)
{
	operate(info);
	enumerate(operate, *info);
	return info;
}

// DbRecordAttributeInfo
template <class Action>
void walk(Action &operate, CssmDbRecordAttributeInfo &info)
{
	operate(info);
	enumerateArray(operate, info, &CssmDbRecordAttributeInfo::attributes);
}

template <class Action>
CssmDbRecordAttributeInfo *walk(Action &operate, CssmDbRecordAttributeInfo * &info)
{
	operate(info);
	enumerateArray(operate, *info, &CssmDbRecordAttributeInfo::attributes);
	return info;
}

// DbAttributeData (Info + value vector)
template <class Action>
void walk(Action &operate, CssmDbAttributeData &data)
{
	operate(data);
	walk(operate, data.info());
	enumerateArray(operate, data, &CssmDbAttributeData::values);
}

template <class Action>
CssmDbAttributeData *walk(Action &operate, CssmDbAttributeData * &data)
{
	operate(data);
	walk(operate, data->info());
	enumerateArray(operate, *data, &CssmDbAttributeData::values);
	return data;
}

// DbRecordAttributeData (array of ...datas)
template <class Action>
void walk(Action &operate, CssmDbRecordAttributeData &data)
{
	operate(data);
	enumerateArray(operate, data, &CssmDbRecordAttributeData::attributes);
}

template <class Action>
CssmDbRecordAttributeData *walk(Action &operate, CssmDbRecordAttributeData * &data)
{
	operate(data);
	enumerateArray(operate, *data, &CssmDbRecordAttributeData::attributes);
	return data;
}

// SelectionPredicates
template <class Action>
CssmSelectionPredicate *walk(Action &operate, CssmSelectionPredicate * &predicate)
{
	operate(predicate);
	walk(operate, predicate->attribute());
	return predicate;
}

template<class Action>
void walk(Action &operate, CssmSelectionPredicate &predicate)
{
	operate(predicate);
	walk(operate, predicate.attribute());
}

// Queries
template <class Action>
void walk(Action &operate, CssmQuery &query)
{
	operate(query);
	enumerateArray(operate, query, &CssmQuery::predicates);
}

template <class Action>
CssmQuery *walk(Action &operate, CssmQuery * &query)
{
	operate(query);
	enumerateArray(operate, *query, &CssmQuery::predicates);
	return query;
}

template <class Action>
CSSM_QUERY *walk(Action &operate, CSSM_QUERY * &query)
{
	return walk(operate, CssmQuery::overlayVar(query));
}


} // end namespace DataWalkers
} // end namespace Security


#endif // _H_CDSA_UTILITIES_CSSMDB
