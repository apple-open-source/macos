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
// mdsclient - friendly interface to CDSA MDS API
//
#include <security_cdsa_client/mdsclient.h>


namespace Security {
namespace CssmClient {


//
// DLAccess gets a virtual destructor just in case
//
DLAccess::~DLAccess()
{ }


//
// Basic Record objects (abstract)
//
Record::Record(const char * const * names)
	: CssmAutoData(Allocator::standard(Allocator::sensitive))
{
	addAttributes(names);
}

void Record::addAttributes(const char * const * name)
{
	if (name)
		while (*name)
			mAttributes.add(CssmDbAttributeInfo(*name++));
}

Record::~Record()
{ }


//
// Tables and their components (non-template common features)
//
TableBase::TableBase(DLAccess &source, CSSM_DB_RECORDTYPE type, bool getData /* = true */)
	: database(source), mRecordType(type), mGetData(getData)
{
}

TableBase::Handle::~Handle()
{
	if (query)
		mAccess->dlAbortQuery(query);
}

TableBase::Uid::~Uid()
{
	if (uid)
		mAccess->dlFreeUniqueId(uid);
}

TableBase::Iterator::Iterator(DLAccess *ac, CSSM_HANDLE query,
		CSSM_DB_UNIQUE_RECORD *id, Record *record, bool getData)
	: mAccess(ac), mQuery(new Handle(ac, query)),
	  mUid(new Uid(ac, id)), mRecord(record), mGetData(getData)
{ }


void TableBase::Iterator::advance(Record *fill)
{
	RefPointer<Record> newRecord = fill;	// hold it safely
	CSSM_DB_UNIQUE_RECORD *id;
	CssmAutoData data(mAccess->allocator());
	if (mAccess->dlGetNext(mQuery->query, newRecord->attributes(),
		mGetData ? &data.get() : NULL, id)) {
		if (mGetData)
			newRecord->recordData() = data;
		mUid = new Uid(mAccess, id);
		mRecord = newRecord;
	} else {
		mQuery->query = CSSM_INVALID_HANDLE; // was automatically aborted
		// release all iterator resources and make me == end()
		mQuery = NULL;
		mUid = NULL;
		mRecord = NULL;
	}
}


uint32 TableBase::erase(const CSSM_QUERY &query)
{
	CSSM_DB_UNIQUE_RECORD *id;
	CssmDbRecordAttributeData noAttributes;
	CSSM_HANDLE handle = database.dlGetFirst(query, noAttributes, NULL, id);
	if (handle == CSSM_INVALID_HANDLE)
		return 0;   // no match, nothing erased
	uint32 count = 0;
	do {
		database.dlDeleteRecord(id);
		count++;
		database.dlFreeUniqueId(id);
	} while (database.dlGetNext(handle, noAttributes, NULL, id));
	return count;
}

uint32 TableBase::erase(const Query &query)
{
	return erase(query.cssmQuery());
}


} // end namespace CssmClient
} // end namespace Security
