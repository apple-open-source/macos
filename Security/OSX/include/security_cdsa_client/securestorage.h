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
// securestorage - client interface to CSP DLs and their operations
//
#ifndef _H_CDSA_CLIENT_SECURESTORAGE
#define _H_CDSA_CLIENT_SECURESTORAGE  1

#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_client/dlclient.h>
#include <security_cdsa_client/keyclient.h>

namespace Security
{

namespace CssmClient
{

//
// A CSP and a DL attachment of the same subservice
//
// This gives us 2 Object instances, but we make sure that have the same
// mImpl.  Currently this class has no behaviour, but it will get some in
// the future.
//
class CSPDLImpl : public CSPImpl, public DLImpl
{
public:
	CSPDLImpl(const Guid &guid);
	CSPDLImpl(const Module &module);
	virtual ~CSPDLImpl();

	// Object methods.
	bool isActive() const { return CSPImpl::isActive() || DLImpl::isActive(); }

	virtual Allocator &allocator() const;
	virtual void allocator(Allocator &alloc);

	virtual bool operator <(const CSPDLImpl &other) const;
	virtual bool operator ==(const CSPDLImpl &other) const;

	// Attachment methods.
	virtual CSSM_SERVICE_MASK subserviceMask() const;
	virtual void subserviceId(uint32 id);

	uint32 subserviceId() const { return CSPImpl::subserviceId(); }
	CSSM_ATTACH_FLAGS cspFlags() const { return CSPImpl::flags(); }
	void cspFlags(CSSM_ATTACH_FLAGS f) { CSPImpl::flags(f); }
	CSSM_ATTACH_FLAGS dlFlags() const { return DLImpl::flags(); }
	void dlFlags(CSSM_ATTACH_FLAGS f) { DLImpl::flags(f); }

	void attach() { CSPImpl::attach(); DLImpl::attach(); }
	void detach() { CSPImpl::detach(); DLImpl::detach(); }
	bool attached() const { return CSPImpl::attached() || DLImpl::attached(); }

	Module module() const { return CSPImpl::module(); }
	const Guid &guid() const { return CSPImpl::guid(); }
	CSSM_MODULE_HANDLE cspHandle() { return CSPImpl::handle(); }
	CSSM_MODULE_HANDLE dlHandle() { return DLImpl::handle(); }

	CssmSubserviceUid subserviceUid() const
	{ return CSPImpl::subserviceUid(); }

private:
};


class CSPDL : public CSP, public DL
{
public:
	typedef CSPDLImpl Impl;

	explicit CSPDL(Impl *impl) : CSP(impl), DL(impl) {}
	CSPDL(const Guid &guid) : CSP(new Impl(guid)), DL(&CSP::impl<Impl>()) {}
	CSPDL(const Module &module)
		: CSP(new Impl(module)), DL(&CSP::impl<Impl>()) {}

	//template <class _Impl> _Impl &impl() const
	//{ return CSP::impl<_Impl>(); }

	Impl *get() const { return &CSP::impl<Impl>(); }
	Impl *operator ->() const { return &CSP::impl<Impl>(); }
	Impl &operator *() const { return CSP::impl<Impl>(); }

	// Conversion operators must be here
	bool operator !() const { return !get(); }
	operator bool() const { return get(); }

	bool operator <(const CSPDL &other) const
	{ return *this && other ? **this < *other : get() < other.get(); }
	bool operator ==(const CSPDL &other) const
	{ return *this && other ? **this == *other : get() == other.get(); }
};


//
// SSCSPDL -- Secure storage class
//
class SSCSPDLImpl : public CSPDLImpl
{
public:
	SSCSPDLImpl(const Guid &guid);
	SSCSPDLImpl(const Module &module);
	virtual ~SSCSPDLImpl();

	// DbMaker
	DbImpl *newDb(const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation);
private:
};

class SSCSPDL : public CSPDL 
{
public:
	typedef SSCSPDLImpl Impl;

	explicit SSCSPDL(Impl *impl) : CSPDL(impl) {}
	SSCSPDL(const Guid &guid) : CSPDL(new Impl(guid)) {}
	SSCSPDL(const Module &module) : CSPDL(new Impl(module)) {}

	Impl *operator ->() const { return &CSP::impl<Impl>(); }
	Impl &operator *() const { return CSP::impl<Impl>(); }
};


//
// SSDbImpl --  A Security Storage Db object.
//
class SSGroup;
class SSDbUniqueRecord;

class SSDbImpl : public DbImpl
{
public:
	SSDbImpl(const SSCSPDL &cspdl,
			 const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation);
	virtual ~SSDbImpl();

	void create();
	void open();

    // This insert is here to explicitly catch calls to DbImpl's insert. You probably want the ssInsert calls below.
    DbUniqueRecord insert(CSSM_DB_RECORDTYPE recordType,
                              const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
                              const CSSM_DATA *data);

	SSDbUniqueRecord ssInsert(CSSM_DB_RECORDTYPE recordType,
							const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
							const CSSM_DATA *data,
							const CSSM_RESOURCE_CONTROL_CONTEXT *rc = NULL);

	SSDbUniqueRecord ssInsert(CSSM_DB_RECORDTYPE recordType,
							const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
							const CSSM_DATA *data, const SSGroup &group,
							const CSSM_ACCESS_CREDENTIALS *cred);

	// DbCursorMaker
	DbCursorImpl *newDbCursor(const CSSM_QUERY &query,
							  Allocator &allocator);
	DbCursorImpl *newDbCursor(uint32 capacity, Allocator &allocator);

	// SSDbUniqueRecordMaker
	DbUniqueRecordImpl *newDbUniqueRecord();

	CSP csp() { return parent<CSP>(); }
};

class SSDb : public Db
{
public:
	typedef SSDbImpl Impl;

	explicit SSDb(Impl *impl) : Db(impl) {}
	SSDb(const SSCSPDL &cspdl, const char *inDbName,
		 const CSSM_NET_ADDRESS *inDbLocation = NULL)
		: Db(cspdl->newDb(inDbName, inDbLocation)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
};


//
// SSGroup -- Group key with acl, used to protect a group of items.
//
class SSGroupImpl : public KeyImpl
{
public:
	SSGroupImpl(const SSDb &ssDb, const CSSM_DATA &dataBlob);
	SSGroupImpl(const SSDb &ssDb,
				const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry);

	static bool isGroup(const CSSM_DATA &dataBlob);

	const CssmData label() const;
	void decodeDataBlob(const CSSM_DATA &dataBlob,
						const CSSM_ACCESS_CREDENTIALS *cred,
						Allocator &allocator, CSSM_DATA &data);
	void encodeDataBlob(const CSSM_DATA *data,
						const CSSM_ACCESS_CREDENTIALS *cred,
						CssmDataContainer &dataBlob);

private:
	// Constants
	enum
	{
		// Label prefix for a secure storage group
		kGroupMagic = FOUR_CHAR_CODE('ssgp'),

		// Size of label (including prefix)
		kLabelSize = 20,

		// Size of IV
		kIVSize = 8
	};

	CSSM_DB_ATTR_DECL(kLabel);

	CssmDataContainer mLabel;
};

class SSGroup : public Key
{
public:
	typedef SSGroupImpl Impl;
	explicit SSGroup(Impl *impl) : Key(impl) {}

	SSGroup() : Key(NULL) {}

	// Create a new group.
	SSGroup(const SSDb &ssDb,
			const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry)
	: Key(new Impl(ssDb, credAndAclEntry)) {}

	// Lookup an existing group based on a dataBlob.
	SSGroup(const SSDb &ssDb, const CSSM_DATA &dataBlob)
	: Key(new Impl(ssDb, dataBlob)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
};


//
// SSDbCursor -- Cursor for iterating over Securely Stored records (or keys)
//
class SSDbCursorImpl : public DbDbCursorImpl
{
public:
	SSDbCursorImpl(const Db &db, const CSSM_QUERY &query,
				   Allocator &allocator);
	SSDbCursorImpl(const Db &db, uint32 capacity,
				   Allocator &allocator);

	bool next(DbAttributes *attributes, ::CssmDataContainer *data,
			  DbUniqueRecord &uniqueId);
	bool next(DbAttributes *attributes, ::CssmDataContainer *data,
			  DbUniqueRecord &uniqueId, const CSSM_ACCESS_CREDENTIALS *cred);
	bool nextKey(DbAttributes *attributes, Key &key, DbUniqueRecord &uniqueId);
	//bool nextGroup(DbAttributes *attributes, SSGroup &group, DbUniqueRecord &uniqueId);

	SSDb database() { return parent<SSDb>(); }
protected:
	void activate();
	void deactivate();
};

class SSDbCursor : public DbCursor
{
public:
	typedef SSDbCursorImpl Impl;

	explicit SSDbCursor(Impl *impl) : DbCursor(impl) {}
	SSDbCursor(const SSDb &ssDb, const CSSM_QUERY &query,
			   Allocator &allocator = Allocator::standard())
		: DbCursor(ssDb->newDbCursor(query, allocator)) {}
	SSDbCursor(const SSDb &ssDb, const uint32 capacity = 0,
			   Allocator &allocator = Allocator::standard())
		: DbCursor(ssDb->newDbCursor(capacity, allocator)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
};


//
// SSDbUniqueRecord
//
class SSDbUniqueRecordImpl : public DbUniqueRecordImpl
{
public:
	SSDbUniqueRecordImpl(const Db &db);
	virtual ~SSDbUniqueRecordImpl();

	void deleteRecord();
	void deleteRecord(const CSSM_ACCESS_CREDENTIALS *cred);
	void modify(CSSM_DB_RECORDTYPE recordType,
				const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
				const CSSM_DATA *data,
				CSSM_DB_MODIFY_MODE modifyMode);
	void modify(CSSM_DB_RECORDTYPE recordType,
				const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
				const CSSM_DATA *data,
				CSSM_DB_MODIFY_MODE modifyMode,
				const CSSM_ACCESS_CREDENTIALS *cred);
	void get(DbAttributes *attributes, ::CssmDataContainer *data);
	void get(DbAttributes *attributes, ::CssmDataContainer *data,
			 const CSSM_ACCESS_CREDENTIALS *cred);

	SSDb database() { return parent<SSDb>(); }

	// Return the group that this record is in.
	SSGroup group();
};

class SSDbUniqueRecord : public DbUniqueRecord
{
public:
	typedef SSDbUniqueRecordImpl Impl;

	explicit SSDbUniqueRecord(Impl *impl) : DbUniqueRecord(impl) {}
	SSDbUniqueRecord(const SSDb &ssDb)
		: DbUniqueRecord(ssDb->newDbUniqueRecord()) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
};

}; // end namespace CssmClient

} // end namespace Security

#endif //_H_CDSA_CLIENT_SECURESTORAGE
