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
// dlclient - client interface to CSSM DLs and their operations
//

#ifndef _H_CDSA_CLIENT_DLCLIENT
#define _H_CDSA_CLIENT_DLCLIENT  1

#include <security_cdsa_client/cssmclient.h>
#include <security_cdsa_client/dliterators.h>
#include <security_cdsa_client/aclclient.h>
#include <security_cdsa_client/DLDBList.h>
#include <security_cdsa_utilities/cssmacl.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <security_cdsa_utilities/cssmdata.h>


namespace Security
{

namespace CssmClient
{

#define CSSM_DB_ATTR(ATTR) ATTR
#define CSSM_DB_ATTR_SCHEMA(ATTR) ATTR ## Schema

#define CSSM_DB_INDEX(ATTR) ATTR ## Index
#define CSSM_DB_UNIQUE(ATTR) ATTR ## Unique

//
// Helper macro for declaring and defining a Db index unique and non-unique attributes
//
#define CSSM_DB_INDEX_DECL(ATTR)  static const CSSM_DB_INDEX_INFO CSSM_DB_INDEX(ATTR)
#define CSSM_DB_UNIQUE_DECL(ATTR)  static const CSSM_DB_INDEX_INFO CSSM_DB_UNIQUE(ATTR)


//
// Use this macro for defining a non-unique attribute
//
#define CSSM_DB_INDEX_DEF(ATTR) \
const CSSM_DB_INDEX_INFO CSSM_DB_INDEX(ATTR) = \
{ \
	CSSM_DB_INDEX_NONUNIQUE, \
	CSSM_DB_INDEX_ON_ATTRIBUTE, \
	CSSM_DB_ATTR(ATTR) \
}

//
//  Use this macro for defining a unique attribute

//
#define CSSM_DB_UNIQUE_DEF(ATTR) \
const CSSM_DB_INDEX_INFO CSSM_DB_UNIQUE(ATTR) = \
{ \
	CSSM_DB_INDEX_UNIQUE, \
	CSSM_DB_INDEX_ON_ATTRIBUTE, \
	CSSM_DB_ATTR(ATTR) \
}



//
// Helper macro for declaring and defining a Db schema attributes
// Use this macro in your header to declare each attribute you require.
//
#define CSSM_DB_ATTR_DECL(ATTR) \
static const CSSM_DB_ATTRIBUTE_INFO CSSM_DB_ATTR(ATTR); \
static const CSSM_DB_SCHEMA_ATTRIBUTE_INFO CSSM_DB_ATTR_SCHEMA(ATTR)

//
// Don't directly use this macro use one of the below instead.
//
#define CSSM_DB_ATTR_DEFINE_SCHEMA(ATTR, INTEGER, NAME, OID_LEN, OID_DATA, VALUETYPE) \
const CSSM_DB_SCHEMA_ATTRIBUTE_INFO CSSM_DB_ATTR_SCHEMA(ATTR) = \
{ \
	INTEGER, \
	NAME, \
	{ OID_LEN, OID_DATA }, \
	CSSM_DB_ATTRIBUTE_FORMAT_ ## VALUETYPE \
}


//
// Use one of the following macros to defined each declared attribute required by your application.
//
//
// Use this macro to define attributes which are looked up by integer AttributeID.
//
#define CSSM_DB_INTEGER_ATTR(ATTR, INTEGER, NAME, OID_LEN, OID_DATA, VALUETYPE) \
const CSSM_DB_ATTRIBUTE_INFO ATTR = \
{ \
    CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER, \
    {(char *)INTEGER}, \
    CSSM_DB_ATTRIBUTE_FORMAT_ ## VALUETYPE \
};\
\
CSSM_DB_ATTR_DEFINE_SCHEMA(ATTR, INTEGER, NAME, OID_LEN, OID_DATA, VALUETYPE)

//
// Use this macro to define attributes which are looked up by string AttributeName.
//
#define CSSM_DB_NAME_ATTR(ATTR, INTEGER, NAME, OID_LEN, OID_DATA, VALUETYPE) \
const CSSM_DB_ATTRIBUTE_INFO ATTR = \
{ \
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING, \
    {NAME}, \
    CSSM_DB_ATTRIBUTE_FORMAT_ ## VALUETYPE \
};\
\
CSSM_DB_ATTR_DEFINE_SCHEMA(ATTR, INTEGER, NAME, OID_LEN, OID_DATA, VALUETYPE)

//
// Use this macro to define attributes which are looked up by OID AttributeNameID.
// XXX This does not work yet.
//
#define CSSM_DB_OID_ATTR(ATTR, INTEGER, NAME, OID_LEN, OID_DATA, VALUETYPE) \
const CSSM_DB_ATTRIBUTE_INFO ATTR = \
{ \
    CSSM_DB_ATTRIBUTE_NAME_AS_OID, \
    {{OID_LEN, OID_DATA}}, \
    CSSM_DB_ATTRIBUTE_FORMAT_ ## VALUETYPE \
};\
\
CSSM_DB_ATTR_DEFINE_SCHEMA(ATTR, INTEGER, NAME, OID_LEN, OID_DATA, VALUETYPE)


//
// Use this macro to define attributes which are part of the primary key.
//
#define CSSM_DB_PRIMARKEY_ATTR(ATTR, NAME) \
const CSSM_DB_ATTRIBUTE_INFO ATTR = \
{ \
    CSSM_DB_INDEX_UNIQUE, \
    CSSM_DB_INDEX_ON_ATTRIBUTE, \
    CSSM_DB_ATTRIBUTE_FORMAT_ ## VALUETYPE \
};\
\
CSSM_DB_ATTR_DEFINE_SCHEMA(ATTR, INTEGER, NAME, OID_LEN, OID_DATA, VALUETYPE)



//
// Maker interfaces used by various Impl objects
//

// DbMaker -- someone who can create a new DbImpl.
class DbImpl;
class DbMaker
{
public:
	virtual ~DbMaker();
	virtual DbImpl *newDb(const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation) = 0;
};

// DbCursorMaker -- someone who can create a new DbCursorImpl.
class DbCursorImpl;
class DbCursorMaker
{
public:
	virtual ~DbCursorMaker();
	virtual DbCursorImpl *newDbCursor(const CSSM_QUERY &query, Allocator &allocator) = 0;
	virtual DbCursorImpl *newDbCursor(uint32 capacity, Allocator &allocator) = 0;
};

// DbUniqueRecordMaker -- someone who can create a new DbUniqueRecordImpl.
class DbUniqueRecordImpl;
class DbUniqueRecordMaker
{
public:
	virtual ~DbUniqueRecordMaker();
	virtual DbUniqueRecordImpl *newDbUniqueRecord() = 0;
};


//
// A DL attachment
//
class DLImpl : public AttachmentImpl, public DbMaker
{
public:
	DLImpl(const Guid &guid);
	DLImpl(const Module &module);
	virtual ~DLImpl();

	virtual void getDbNames(char **);
	virtual void freeNameList(char **);

	// DbMaker
	virtual DbImpl *newDb(const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation);
private:
};

class DL : public Attachment
{
public:
	typedef DLImpl Impl;

	explicit DL(Impl *impl) : Attachment(impl) {}
	DL() : Attachment(NULL) {}
	DL(const Guid &guid) : Attachment(new Impl(guid)) {}
	DL(const Module &module) : Attachment(new Impl(module)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }

	// Conversion to DbMaker.
	operator DbMaker &() const { return impl<Impl>(); }
};


class DbAttributes;
class DbUniqueRecord;
class Db;


//
// A CSSM_DLDB handle.
// Dbs always belong to DLs (DL attachments)
//
class DbImpl : public ObjectImpl, public AclBearer,
	public DbCursorMaker, public DbUniqueRecordMaker
{
public:
	DbImpl(const DL &dl, const char *inDbName = NULL, const CSSM_NET_ADDRESS *inDbLocation = NULL);
	virtual ~DbImpl();

	DL dl() const { return parent<DL>(); }
	Module module() const { return dl()->module(); }

	virtual void open();
	virtual void create();
	virtual void createWithBlob (CssmData &blob);
	virtual void close();
	virtual void deleteDb();
	virtual void rename(const char *newName);
	virtual void authenticate(CSSM_DB_ACCESS_TYPE inAccessRequest,
								const CSSM_ACCESS_CREDENTIALS *inAccessCredentials);
	virtual void name(char *&outName);	// CSSM_DL_GetDbNameFromHandle()

	virtual void createRelation(CSSM_DB_RECORDTYPE inRelationID,
								const char *inRelationName,
								uint32 inNumberOfAttributes,
								const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *pAttributeInfo,
								uint32 inNumberOfIndexes,
								const CSSM_DB_SCHEMA_INDEX_INFO *pIndexInfo);
	virtual void destroyRelation(CSSM_DB_RECORDTYPE inRelationID);

	virtual DbUniqueRecord insert(CSSM_DB_RECORDTYPE recordType,
									const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
									const CSSM_DATA *data);
									
	virtual DbUniqueRecord insertWithoutEncryption(CSSM_DB_RECORDTYPE recordType,
													const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
													CSSM_DATA *data);

	const CSSM_DL_DB_HANDLE &handle() { activate(); return mHandle; }

	const DbName &dbName() { return mDbName; }
	void dbName(const DbName &dbName) { mDbName = dbName; }

	// Attempt to get a (cached) name from CSSM_DL_GetDbNameFromHandle(), falls
	// back to the name passed in to the constructor if this fails.
	const char *name();

	const CSSM_NET_ADDRESS *dbLocation() const { return mDbName.dbLocation(); }

	CSSM_DB_ACCESS_TYPE accessRequest() const { return mAccessRequest; }
	void accessRequest(CSSM_DB_ACCESS_TYPE inAccessRequest)
	{ mAccessRequest = inAccessRequest; }

	const CSSM_ACCESS_CREDENTIALS *accessCredentials() const
	{ return mAccessCredentials; }
	void accessCredentials(const CSSM_ACCESS_CREDENTIALS *inAccessCredentials)
	{ mAccessCredentials = inAccessCredentials; }

	const void *openParameters() const { return mOpenParameters; }
	void openParameters(const void *inOpenParameters)
	{ mOpenParameters = inOpenParameters; }

	const CSSM_DBINFO *dbInfo() const { return mDbInfo; }
	void dbInfo(const CSSM_DBINFO *inDbInfo) { mDbInfo = inDbInfo; }

	const ResourceControlContext *resourceControlContext() const
	{ return mResourceControlContext; }
	void resourceControlContext(const CSSM_RESOURCE_CONTROL_CONTEXT *inResourceControlContext)
	{ mResourceControlContext = ResourceControlContext::overlay(inResourceControlContext); }
	
	void passThrough(uint32 passThroughId, const void *in, void **out = NULL);
	
	template <class TIn, class TOut>
	void passThrough(uint32 passThroughId, const TIn *in, TOut *out = NULL)
	{ passThrough(passThroughId, (const void *)in, (void **)out); }

	// Passthrough functions (only implemented by AppleCSPDL).
	virtual void lock();
	virtual void unlock();
	virtual void unlock(const CSSM_DATA &password);
    virtual void stash();
    virtual void stashCheck();
	virtual void getSettings(uint32 &outIdleTimeout, bool &outLockOnSleep);
	virtual void setSettings(uint32 inIdleTimeout, bool inLockOnSleep);
	virtual bool isLocked();
	virtual void changePassphrase(const CSSM_ACCESS_CREDENTIALS *cred);
	virtual void recode(const CSSM_DATA &data, const CSSM_DATA &extraData);
	virtual void copyBlob(CssmData &data);
	virtual void setBatchMode(Boolean mode, Boolean rollback);

	// Utility methods

	// Always use the dbName and dbLocation that were passed in during
	// construction.
	virtual DLDbIdentifier dlDbIdentifier();

	// DbCursorMaker
	virtual DbCursorImpl *newDbCursor(const CSSM_QUERY &query, Allocator &allocator);
	virtual DbCursorImpl *newDbCursor(uint32 capacity, Allocator &allocator);

	// DbUniqueRecordMaker
	virtual DbUniqueRecordImpl *newDbUniqueRecord();

	// Acl manipulation
	void getAcl(AutoAclEntryInfoList &aclInfos, const char *selectionTag = NULL) const;
	void changeAcl(const CSSM_ACL_EDIT &aclEdit,
		const CSSM_ACCESS_CREDENTIALS *accessCred);

	// Acl owner manipulation
	void getOwner(AutoAclOwnerPrototype &owner) const;
	void changeOwner(const CSSM_ACL_OWNER_PROTOTYPE &newOwner,
		const CSSM_ACCESS_CREDENTIALS *accessCred = NULL);
	
	// default-credential hook
	class DefaultCredentialsMaker {
	public:
		virtual ~DefaultCredentialsMaker();
		virtual const AccessCredentials *makeCredentials() = 0;
	};
	
	void defaultCredentials(DefaultCredentialsMaker *maker);	// NULL to turn off

	void activate();

protected:
	void deactivate();

private:
	CSSM_DL_DB_HANDLE mHandle;			// CSSM DLDB handle

	DbName mDbName;
	bool mUseNameFromHandle; // false if CSSM_DL_GetDbNameFromHandle failed
	char *mNameFromHandle; // Cached CSSM_DL_GetDbNameFromHandle result.
	CSSM_DB_ACCESS_TYPE mAccessRequest;
	const CSSM_ACCESS_CREDENTIALS *mAccessCredentials;
	DefaultCredentialsMaker *mDefaultCredentials;
	const void *mOpenParameters;

	// Arguments to create
	const CSSM_DBINFO *mDbInfo;
	const ResourceControlContext *mResourceControlContext;
};


class Db : public Object, public DLAccess
{
public:
	typedef DbImpl Impl;
	typedef Impl::DefaultCredentialsMaker DefaultCredentialsMaker;

	explicit Db(Impl *impl) : Object(impl) {}
	Db() : Object(NULL) {}
	Db(DbMaker &maker, const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation = NULL)
	: Object(maker.newDb(inDbName, inDbLocation)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }

	// Conversion to DbCursorMaker.
	operator DbCursorMaker &() const { return impl<Impl>(); }
	// Conversion to DbUniqueRecordMaker.
	operator DbUniqueRecordMaker &() const { return impl<Impl>(); }

	const CSSM_DL_DB_HANDLE &handle() { return impl<Impl>().handle(); }
	
protected:
	// DLAccess adapters
	CSSM_HANDLE dlGetFirst(const CSSM_QUERY &query,
		CSSM_DB_RECORD_ATTRIBUTE_DATA &attributes, CSSM_DATA *data,
		CSSM_DB_UNIQUE_RECORD *&id);
	bool dlGetNext(CSSM_HANDLE handle,
		CSSM_DB_RECORD_ATTRIBUTE_DATA &attributes, CSSM_DATA *data,
		 CSSM_DB_UNIQUE_RECORD *&id);
	void dlAbortQuery(CSSM_HANDLE handle);
	void dlFreeUniqueId(CSSM_DB_UNIQUE_RECORD *id);
	void dlDeleteRecord(CSSM_DB_UNIQUE_RECORD *id);
	Allocator &allocator();
};

//
// DbCursor
//

// This class is still abstract.  You must subclass it in order to be able to instantiate an instance.
class DbCursorImpl : public ObjectImpl, public CssmAutoQuery
{
public:
	DbCursorImpl(const Object &parent, const CSSM_QUERY &query, Allocator &allocator);
	DbCursorImpl(const Object &parent, uint32 capacity, Allocator &allocator);

	virtual Allocator &allocator() const;
	virtual void allocator(Allocator &alloc);

	virtual bool next(DbAttributes *attributes, ::CssmDataContainer *data, DbUniqueRecord &uniqueId) = 0;
	void abort() { deactivate(); }
};

class DbCursor : public Object
{
public:
	typedef DbCursorImpl Impl;

	explicit DbCursor(Impl *impl) : Object(impl) {}
	DbCursor() : Object(NULL) {}
	DbCursor(DbCursorMaker &maker, const CSSM_QUERY &query,
			 Allocator &allocator = Allocator::standard())
	: Object(maker.newDbCursor(query, allocator)) {}
	DbCursor(DbCursorMaker &maker, uint32 capacity = 0,
			 Allocator &allocator = Allocator::standard())
	: Object(maker.newDbCursor(capacity, allocator)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
};


//
// DbUniqueRecord
//
class DbUniqueRecordImpl : public ObjectImpl
{
public:
	DbUniqueRecordImpl(const Db &db);
	virtual ~DbUniqueRecordImpl();

	virtual void deleteRecord();
	virtual void modify(CSSM_DB_RECORDTYPE recordType,
						const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
						const CSSM_DATA *data,
						CSSM_DB_MODIFY_MODE modifyMode);

	virtual void modifyWithoutEncryption (CSSM_DB_RECORDTYPE recordType,
											const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
											const CSSM_DATA *data,
											CSSM_DB_MODIFY_MODE modifyMode);
											
	virtual void get(DbAttributes *attributes, ::CssmDataContainer *data);

	virtual void getWithoutEncryption(DbAttributes *attributes, ::CssmDataContainer *data);

	Db database() const { return parent<Db>(); }

	void free() { deactivate(); }

	// Client must call activate() after calling this function if mUniqueId is successfully set.
	operator CSSM_DB_UNIQUE_RECORD_PTR *() { if (mActive) free(); return &mUniqueId; }

	operator CSSM_DB_UNIQUE_RECORD *() { return mUniqueId; }
	operator const CSSM_DB_UNIQUE_RECORD *() const { return mUniqueId; }

	void activate();
	
	void getRecordIdentifier(CSSM_DATA &data);

	void setUniqueRecordPtr (CSSM_DB_UNIQUE_RECORD_PTR uniquePtr); // because cast overloading is evil!

protected:
	void deactivate();

	CSSM_DB_UNIQUE_RECORD_PTR mUniqueId;
	bool mDestroyID;
    RecursiveMutex mActivateMutex;
};

class DbUniqueRecord : public Object
{
public:
	typedef DbUniqueRecordImpl Impl;

	explicit DbUniqueRecord(Impl *impl) : Object(impl) {}
	DbUniqueRecord() : Object(NULL) {}
	DbUniqueRecord(DbUniqueRecordMaker &maker) : Object(maker.newDbUniqueRecord()) {}

	Impl *operator ->() { return &impl<Impl>(); }
	Impl &operator *() { return impl<Impl>(); }
	const Impl &operator *() const { return impl<Impl>(); }

	// Conversion operators must be here.

	// Client must activate after calling this function if mUniqueId is successfully set.
	operator CSSM_DB_UNIQUE_RECORD_PTR *() { return **this; }

	operator CSSM_DB_UNIQUE_RECORD *() { return **this; }
	operator const CSSM_DB_UNIQUE_RECORD *() const { return **this; }
};


//
// DbAttributes
//
class DbAttributes : public CssmAutoDbRecordAttributeData
{
public:
	DbAttributes();
	DbAttributes(const Db &db, uint32 capacity = 0, Allocator &allocator = Allocator::standard());
};


//
// DbDbCursor -- concrete subclass of DbCursorImpl for querying Db's
//
class DbDbCursorImpl : public DbCursorImpl
{
public:
	DbDbCursorImpl(const Db &db, const CSSM_QUERY &query, Allocator &allocator);
	DbDbCursorImpl(const Db &db, uint32 capacity, Allocator &allocator);
	virtual ~DbDbCursorImpl();

	bool next(DbAttributes *attributes, ::CssmDataContainer *data, DbUniqueRecord &uniqueId);

protected:
	Db database() { return parent<Db>(); }

	void activate();
	void deactivate();

private:
	CSSM_HANDLE mResultsHandle;
    RecursiveMutex mActivateMutex;
};

} // end namespace CssmClient

} // end namespace Security

#endif // _H_CDSA_CLIENT_DLCLIENT
