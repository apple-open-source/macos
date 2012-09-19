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
// MultiDLDb implementation.
//

#include <security_cdsa_client/multidldb.h>
#include <security_cdsa_client/securestorage.h>



namespace Security
{

using namespace CssmClient;

namespace CssmClient
{

//
// MultiDLDbDbCursorImpl declaration
//
class MultiDLDbDbCursorImpl : public DbCursorImpl
{
public:
	MultiDLDbDbCursorImpl(const MultiDLDb &parent, const CSSM_QUERY &query, Allocator &allocator);
	MultiDLDbDbCursorImpl(const MultiDLDb &parent, uint32 capacity, Allocator &allocator);
	virtual ~MultiDLDbDbCursorImpl();

	bool next(DbAttributes *attributes, ::CssmDataContainer *data, DbUniqueRecord &uniqueId);
private:
	MultiDLDb multiDLDb() { return parent<MultiDLDb>(); }
	void activate();
	void deactivate();

	MultiDLDbImpl::ListRef mListRef;
	MultiDLDbImpl::List::const_iterator mNext;
	MultiDLDbImpl::List::const_iterator mEnd;
	DbCursor mCursor;
};

} // end namespace CssmClient

} // end namespace Security

//
// MultiDLDbImpl
//
MultiDLDbImpl::MultiDLDbImpl(const vector<DLDbIdentifier> &list, bool useSecureStorage, const Cssm &cssm)
: ObjectImpl(cssm), mListRef(list), mUseSecureStorage(useSecureStorage)
{
}

MultiDLDbImpl::MultiDLDbImpl(const vector<DLDbIdentifier> &list, bool useSecureStorage)
: ObjectImpl(Cssm::standard()), mListRef(list), mUseSecureStorage(useSecureStorage)
{
}

MultiDLDbImpl::~MultiDLDbImpl()
{
	deactivate();
}

Db
MultiDLDbImpl::database(const DLDbIdentifier &dlDbIdentifier)
{
	StLock<Mutex> _(mLock);
	DbMap::const_iterator it = mDbMap.find(dlDbIdentifier);
	if (it != mDbMap.end())
		return it->second;

	Module module(dlDbIdentifier.ssuid().guid(), cssm());
	DL dl;
	if (dlDbIdentifier.ssuid().subserviceType() & CSSM_SERVICE_CSP)
	{
		if (mUseSecureStorage)
			dl = SSCSPDL(module);
		else
			dl = CSPDL(module);
	}
	else
		dl = DL(module);

	dl->subserviceId(dlDbIdentifier.ssuid().subserviceId());
	dl->version(dlDbIdentifier.ssuid().version());
	Db db(dl, dlDbIdentifier.dbName());
	if (find(mListRef->begin(), mListRef->end(), dlDbIdentifier) != mListRef->end())
		mDbMap.insert(DbMap::value_type(dlDbIdentifier, db));

	return db;
}

void
MultiDLDbImpl::list(const vector<DLDbIdentifier> &list)
{
	StLock<Mutex> _(mLock);
	set<DLDbIdentifier> oldList(mListRef->begin(), mListRef->end());
	mListRef = ListRef(list);
	set<DLDbIdentifier> newList(mListRef->begin(), mListRef->end());
	vector<DLDbIdentifier> obsolete;
	back_insert_iterator<vector<DLDbIdentifier> > ii(obsolete);
	// Remove all db's from the map that were in oldList but are not in mListRef.
	set_difference(oldList.begin(), oldList.end(), newList.begin(), newList.end(), ii);
	for (vector<DLDbIdentifier>::const_iterator it = obsolete.begin(); it != obsolete.end(); ++it)
		mDbMap.erase(*it);
}

DbCursorImpl *
MultiDLDbImpl::newDbCursor(const CSSM_QUERY &query, Allocator &allocator)
{
	return new MultiDLDbDbCursorImpl(MultiDLDb(this), query, allocator);
}

DbCursorImpl *
MultiDLDbImpl::newDbCursor(uint32 capacity, Allocator &allocator)
{
	return new MultiDLDbDbCursorImpl(MultiDLDb(this), capacity, allocator);
}

void
MultiDLDbImpl::activate()
{
}

void
MultiDLDbImpl::deactivate()
{
	StLock<Mutex> _(mLock);
	mDbMap.erase(mDbMap.begin(), mDbMap.end());
}


//
// MultiDLDbDbCursorImpl
//
MultiDLDbDbCursorImpl::MultiDLDbDbCursorImpl(const MultiDLDb &parent,
											 const CSSM_QUERY &query, Allocator &allocator)
: DbCursorImpl(parent, query, allocator)
{
}

MultiDLDbDbCursorImpl::MultiDLDbDbCursorImpl(const MultiDLDb &parent,
											 uint32 capacity, Allocator &allocator)
: DbCursorImpl(parent, capacity, allocator)
{
}

MultiDLDbDbCursorImpl::~MultiDLDbDbCursorImpl()
{
	try
	{
		deactivate();
	}
	catch(...) {}
}

bool
MultiDLDbDbCursorImpl::next(DbAttributes *attributes, ::CssmDataContainer *data, DbUniqueRecord &uniqueId)
{
	activate();
	for (;;)
	{
		if (!mCursor)
		{
			if (mNext == mEnd)
			{
				// This is how it ends.
				deactivate();
				return false;
			}

			mCursor = DbCursor(multiDLDb()->database(*mNext++), *this);
		}

		try
		{
			if (mCursor->next(attributes, data, uniqueId))
				return true;
		}

		catch(const CommonError &err)
		{
			OSStatus status = err.osStatus();
			if(status != CSSMERR_DL_DATASTORE_DOESNOT_EXIST)
				throw;
		}



		mCursor = DbCursor();
	}
}

void
MultiDLDbDbCursorImpl::activate()
{
    StLock<Mutex> _(mActivateMutex);
	if (!mActive)
	{
		mListRef = multiDLDb()->listRef();
		mNext = mListRef->begin();
		mEnd = mListRef->end();
		mActive = true;
	}
}

void
MultiDLDbDbCursorImpl::deactivate()
{
    StLock<Mutex> _(mActivateMutex);
	if (mActive)
	{
		mActive = false;
		mListRef = MultiDLDbImpl::ListRef();
		mNext = mEnd;
		mCursor = DbCursor();
	}
}

