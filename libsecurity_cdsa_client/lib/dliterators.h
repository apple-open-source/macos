/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// dliterators - DL/MDS table access as C++ iterators
//
// This is currently an almost read-only implementation.
// (You can erase but you can't create or modify.)
//
#ifndef _H_CDSA_CLIENT_DLITERATORS
#define _H_CDSA_CLIENT_DLITERATORS

#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/refcount.h>
#include <security_cdsa_utilities/cssmalloc.h>
#include <security_cdsa_utilities/cssmpods.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <security_cdsa_client/dlquery.h>


namespace Security {
namespace CssmClient {


//
// An abstract interface to a (partial) DLDb-style object.
// This is a particular (open) database that you can perform CSSM database
// operations on.
//
class DLAccess {
public:
	virtual ~DLAccess();
	
	virtual CSSM_HANDLE dlGetFirst(const CSSM_QUERY &query,
		CSSM_DB_RECORD_ATTRIBUTE_DATA &attributes, CSSM_DATA *data,
		CSSM_DB_UNIQUE_RECORD *&id) = 0;
	virtual bool dlGetNext(CSSM_HANDLE handle,
		CSSM_DB_RECORD_ATTRIBUTE_DATA &attributes, CSSM_DATA *data,
		CSSM_DB_UNIQUE_RECORD *&id) = 0;
	virtual void dlAbortQuery(CSSM_HANDLE handle) = 0;
	virtual void dlFreeUniqueId(CSSM_DB_UNIQUE_RECORD *id) = 0;
	virtual void dlDeleteRecord(CSSM_DB_UNIQUE_RECORD *id) = 0;
	virtual Allocator &allocator() = 0;
};


//
// Abstract Database Records.
// Each database record type has a subclass of this.
// These are RefCounted; you can hang on to them as long as you like,
// stick (RefPointers to) them into maps, and so on. Just go for it.
//
class Record : public RefCount, public CssmAutoData {
public:
	Record() : CssmAutoData(Allocator::standard(Allocator::sensitive)) { }
	Record(const char * const * attributeNames);	// sets mAttributes
	virtual ~Record();
	static const CSSM_DB_RECORDTYPE recordType = CSSM_DL_DB_RECORD_ANY;

	void addAttributes(const char * const * attributeNames); // add more

	// raw attribute access
	CssmDbRecordAttributeData &attributes() { return mAttributes; }
	const CssmDbRecordAttributeData &attributes() const { return mAttributes; }
	CSSM_DB_RECORDTYPE actualRecordType() const { return mAttributes.recordType(); }
	
	CssmAutoData &recordData() { return *this; }	// my data nature

protected:
	CssmAutoDbRecordAttributeData mAttributes;
};


//
// TableBase is an implementation class for template Table below.
// Do not use it directly (you'll be sorry).
// Continue reading at template Table below.
//
class TableBase {
public:
	DLAccess &database;

	CSSM_DB_RECORDTYPE recordType() const { return mRecordType; }
	void recordType(CSSM_DB_RECORDTYPE t) { mRecordType = t; }	// override
	
	// erase all elements matching a query
	uint32 erase(const CSSM_QUERY &query);
	uint32 erase(const Query &query);

protected:
	TableBase(DLAccess &source, CSSM_DB_RECORDTYPE type, bool getData = true);
	
	class AccessRef : public RefCount {
	protected:
		AccessRef() : mAccess(NULL) { }
		AccessRef(DLAccess *ac) : mAccess(ac) { }
		DLAccess *mAccess;
	};
	
	struct Handle : public AccessRef {
		CSSM_HANDLE query;
		Handle(DLAccess *ac, CSSM_HANDLE q) : AccessRef(ac), query(q) { }
		~Handle();
	};
	
	struct Uid : public AccessRef {
		CSSM_DB_UNIQUE_RECORD *uid;
		Uid(DLAccess *ac, CSSM_DB_UNIQUE_RECORD *id) : AccessRef(ac), uid(id) { }
		~Uid();
	};
	
	class Iterator {
	public:
		const CSSM_DB_UNIQUE_RECORD *recordHandle() const
		{ assert(mUid); return mUid->uid; }
	
	protected:
		Iterator() { }
		Iterator(DLAccess *ac, CSSM_HANDLE query, CSSM_DB_UNIQUE_RECORD *id,
			Record *record, bool getData);
		void advance(Record *newRecord); // generic operator ++ helper
	
		DLAccess *mAccess;				// data source
		RefPointer<Handle> mQuery;		// DL/MDS query handle
		RefPointer<Uid> mUid;			// record unique identifier
		RefPointer<Record> mRecord;		// current record value
		bool mGetData;					// ask for data on iteration
	};
	
protected:
	CSSM_DB_RECORDTYPE mRecordType;		// CSSM/MDS record type
	bool mGetData;						// ask for record data on primary iteration
};


//
// A Table represents a single relation in a database (of some kind)
//
template <class RecordType>
class Table : private TableBase {
	typedef RefPointer<RecordType> RecPtr;
public:
	Table(DLAccess &source) : TableBase(source, RecordType::recordType) { }
	Table(DLAccess &source, CSSM_DB_RECORDTYPE type) : TableBase(source, type) { }
	Table(DLAccess &source, bool getData) : TableBase(source, RecordType::recordType, getData) { }
	
public:
	class iterator : public Iterator,
			public std::iterator<forward_iterator_tag, RefPointer<RecordType> > {
		friend class Table;
	public:
		iterator() { }

		bool operator == (const iterator &other) const
		{ return mUid.get() == other.mUid.get(); }
		bool operator != (const iterator &other) const
		{ return mUid.get() != other.mUid.get(); }

		RecPtr operator * () const { return static_cast<RecordType *>(mRecord.get()); }
		RecordType *operator -> () const { return static_cast<RecordType *>(mRecord.get()); }
		iterator operator ++ () { advance(new RecordType); return *this; }
		iterator operator ++ (int) { iterator old = *this; operator ++ (); return old; }
		
		void erase();
		
	private:
		iterator(DLAccess *ac, CSSM_HANDLE query, CSSM_DB_UNIQUE_RECORD *id,
			RecordType *record, bool getData)
			: Iterator(ac, query, id, record, getData) { }
	};
	
public:
	iterator begin();
	iterator find(const CSSM_QUERY &query);
	iterator find(const Query &query);
	iterator end()		{ return iterator(); }
	RecPtr fetch(const Query &query, CSSM_RETURN err = CSSM_OK) // one-stop shopping
	{ return fetchFirst(find(query), err); }
	RecPtr fetch(CSSM_RETURN err = CSSM_OK)	// fetch first of type
	{ return fetchFirst(begin(), err); }

	// erase all records matching a query
	void erase(const CSSM_QUERY &query);
	void erase(const Query &query);
	
	void erase(iterator it) { it.erase(); }
	
private:
	iterator startQuery(const CssmQuery &query, bool getData);
	RecPtr fetchFirst(iterator it, CSSM_RETURN err);
};


//
// Template out-of-line functions
//
template <class RecordType>
typename Table<RecordType>::iterator Table<RecordType>::begin()
{
	return startQuery(CssmQuery(mRecordType), mGetData);
}

template <class RecordType>
typename Table<RecordType>::iterator Table<RecordType>::find(const CSSM_QUERY &query)
{
	return startQuery(CssmQuery(CssmQuery::overlay(query), mRecordType), mGetData);
}

template <class RecordType>
typename Table<RecordType>::iterator Table<RecordType>::find(const Query &query)
{
	return startQuery(CssmQuery(query.cssmQuery(), mRecordType), mGetData);
}

template <class RecordType>
RefPointer<RecordType> Table<RecordType>::fetchFirst(iterator it, CSSM_RETURN err)
{
	if (it == end())
		if (err)
			CssmError::throwMe(err);
		else
			return NULL;
	else
		return *it;
}


template <class RecordType>
typename Table<RecordType>::iterator Table<RecordType>::startQuery(const CssmQuery &query, bool getData)
{
	RefPointer<RecordType> record = new RecordType;
	CSSM_DB_UNIQUE_RECORD *id;
	CssmAutoData data(database.allocator());
	CSSM_HANDLE queryHandle = database.dlGetFirst(query, record->attributes(),
		getData ? &data.get() : NULL, id);
	if (queryHandle == CSSM_INVALID_HANDLE)
		return end();  // not found
	if (getData)
		record->recordData() = data;
	return iterator(&database, queryHandle, id, record, getData);
}


template <class RecordType>
void Table<RecordType>::iterator::erase()
{
	mAccess->dlDeleteRecord(mUid->uid);
	mUid->uid = NULL;
}


} // end namespace CssmClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_DLITERATORS
