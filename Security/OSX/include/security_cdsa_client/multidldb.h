/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
// multidldb interfaces for searching multiple dls or db with a single cursor.
//
#ifndef _H_CDSA_CLIENT_MULTIDLDB
#define _H_CDSA_CLIENT_MULTIDLDB  1

#include <security_cdsa_client/dlclient.h>
#include <security_cdsa_client/DLDBList.h>

namespace Security
{

namespace CssmClient
{

//
// The MultiDLDb class.
//
class MultiDLDbImpl : public ObjectImpl, public DbCursorMaker
{
public:
	struct List : public vector<DLDbIdentifier>, public RefCount
	{
		List(const vector<DLDbIdentifier> &list) : vector<DLDbIdentifier>(list) {}
	};

	struct ListRef : public RefPointer<List>
	{
		ListRef() {}
		ListRef(const vector<DLDbIdentifier> &list) : RefPointer<List>(new List(list)) {}
	};

	MultiDLDbImpl(const vector<DLDbIdentifier> &list, bool useSecureStorage, const Cssm &cssm);
	MultiDLDbImpl(const vector<DLDbIdentifier> &list, bool useSecureStorage);
	virtual ~MultiDLDbImpl();

	Cssm cssm() const { return parent<Cssm>(); }
	Db database(const DLDbIdentifier &dlDbIdentifier);
	ListRef listRef() { return mListRef; }
	void list(const vector<DLDbIdentifier> &list);
    const vector<DLDbIdentifier> &list() { return *mListRef; }

	// DbCursorMaker
	virtual DbCursorImpl *newDbCursor(const CSSM_QUERY &query, Allocator &allocator);
	virtual DbCursorImpl *newDbCursor(uint32 capacity, Allocator &allocator);

protected:
	void activate();
	void deactivate();

private:
	typedef map<DLDbIdentifier, Db> DbMap;

	// Lock protecting this object during changes.
	Mutex mLock;
	ListRef mListRef;
	DbMap mDbMap;
	bool mUseSecureStorage;
};

class MultiDLDb : public Object
{
public:
	typedef MultiDLDbImpl Impl;

	explicit MultiDLDb(Impl *impl) : Object(impl) {}
	MultiDLDb(const vector<DLDbIdentifier> &list, bool useSecureStorage, const Cssm &cssm)
	: Object(new Impl(list, useSecureStorage, cssm)) {}
	MultiDLDb(const vector<DLDbIdentifier> &list, bool useSecureStorage)
	: Object(new Impl(list, useSecureStorage)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
	
	// Conversion to DbCursorMaker
	operator DbCursorMaker &() { return impl<Impl>(); }
};

}; // end namespace CssmClient

} // end namespace Security

#endif // _H_CDSA_CLIENT_MULTIDLDB
