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
#include <Security/dlclient.h>
#include <Security/aclclient.h>
#include <Security/ssclient.h>

using namespace CssmClient;


//
// Manage DL attachments
//
DLImpl::DLImpl(const Guid &guid) : AttachmentImpl(guid, CSSM_SERVICE_DL)
{
}

DLImpl::DLImpl(const Module &module) : AttachmentImpl(module, CSSM_SERVICE_DL)
{
}

DLImpl::~DLImpl()
{
}

void
DLImpl::getDbNames(char **)
{
	CssmError::throwMe(CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED);
}

void
DLImpl::freeNameList(char **)
{
	CssmError::throwMe(CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED);
}

DbImpl *
DLImpl::newDb(const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation)
{
	return new DbImpl(DL(this), inDbName, inDbLocation);
}


//
// Db (database)
//
DbImpl::DbImpl(const DL &dl, const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation)
: ObjectImpl(dl), mDbName(inDbName, inDbLocation),
mAccessRequest(CSSM_DB_ACCESS_READ), mAccessCredentials(NULL),
mOpenParameters(NULL), mDbInfo(NULL), mResourceControlContext(NULL)
{
}

DbImpl::~DbImpl()
{
	try
	{
		deactivate();
	}
	catch(...) {}
}

void
DbImpl::open()
{
	if (!mActive)
	{
		assert(mDbInfo == nil);
		mHandle.DLHandle = dl()->handle();
		check(CSSM_DL_DbOpen(mHandle.DLHandle, name(), dbLocation(),
								mAccessRequest, mAccessCredentials,
								mOpenParameters, &mHandle.DBHandle));
		mActive = true;
	}
}

void
DbImpl::create()
{
	if (mActive)
		CssmError::throwMe(CSSMERR_DL_DATASTORE_ALREADY_EXISTS);

	if (mDbInfo == nil) {
		// handle a missing (null) mDbInfo as an all-zero one
		static const CSSM_DBINFO nullDbInfo = { };
		mDbInfo = &nullDbInfo;
	}
	mHandle.DLHandle = dl()->handle();
	
	if (!mResourceControlContext && mAccessCredentials) {
		AclFactory::AnyResourceContext ctx(mAccessCredentials);
		check(CSSM_DL_DbCreate(mHandle.DLHandle, name(), dbLocation(), mDbInfo,
							mAccessRequest, &ctx,
							mOpenParameters, &mHandle.DBHandle));
	} else {
		check(CSSM_DL_DbCreate(mHandle.DLHandle, name(), dbLocation(), mDbInfo,
							mAccessRequest, mResourceControlContext,
							mOpenParameters, &mHandle.DBHandle));
	}
	mActive = true;
}

void
DbImpl::close()
{
	check(CSSM_DL_DbClose(mHandle));
}

void
DbImpl::activate()
{
	if (!mActive)
	{
		if (mDbInfo)
			create();
		else
			open();
	}
}

void
DbImpl::deactivate()
{
	if (mActive)
	{
		mActive = false;
		close();
	}
}

void
DbImpl::deleteDb()
{
	// Deactivate so the db gets closed if it was open.
	deactivate();
	// This call does not require the receiver to be active.
	check(CSSM_DL_DbDelete(dl()->handle(), name(), dbLocation(),
						   mAccessCredentials));
}

void
DbImpl::rename(const char *newName)
{
	// Deactivate so the db gets closed if it was open.
	deactivate();
    if (::rename(name(), newName))
		UnixError::throwMe(errno);

	// Change our DbName to reflect this rename.
	mDbName = DbName(newName, mDbName.dbLocation());
}

void
DbImpl::authenticate(CSSM_DB_ACCESS_TYPE inAccessRequest,
					 const CSSM_ACCESS_CREDENTIALS *inAccessCredentials)
{
	if (!mActive)
	{
		// XXX Could do the same for create but this would require sticking
		// inAccessCredentials into mResourceControlContext.
		if (!mDbInfo)
		{
			// We were not yet active.  Just do an open.
			accessRequest(inAccessRequest);
			accessCredentials(inAccessCredentials);
			activate();
			return;
		}
	}

	check(CSSM_DL_Authenticate(handle(), inAccessRequest, inAccessCredentials));
}

void
DbImpl::name(char *&outDbName)
{
	check(CSSM_DL_GetDbNameFromHandle(handle(), &outDbName));
}

void
DbImpl::createRelation(CSSM_DB_RECORDTYPE inRelationID,
							 const char *inRelationName,
							 uint32 inNumberOfAttributes,
							 const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *pAttributeInfo,
							 uint32 inNumberOfIndexes,
							 const CSSM_DB_SCHEMA_INDEX_INFO *pIndexInfo)
{
	check(CSSM_DL_CreateRelation(handle(), inRelationID, inRelationName,
								 inNumberOfAttributes, pAttributeInfo,
								 inNumberOfIndexes, pIndexInfo));
}

void
DbImpl::destroyRelation(CSSM_DB_RECORDTYPE inRelationID)
{
	check(CSSM_DL_DestroyRelation(handle(), inRelationID));
}

DbUniqueRecord
DbImpl::insert(CSSM_DB_RECORDTYPE recordType, const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
				 const CSSM_DATA *data)
{
	DbUniqueRecord uniqueId(Db(this));
	check(CSSM_DL_DataInsert(handle(), recordType,
							 attributes,
							 data, uniqueId));
	// Activate uniqueId so CSSM_DL_FreeUniqueRecord() gets called when it goes out of scope.
	uniqueId->activate();
	return uniqueId;
}


//
// Generic Passthrough interface
//
void DbImpl::passThrough(uint32 passThroughId, const void *in, void **out)
{
	check(CSSM_DL_PassThrough(handle(), passThroughId, in, out));
}


//
// Passthrough functions (only implemented by AppleCSPDL).
//
void
DbImpl::lock()
{
	check(CSSM_DL_PassThrough(handle(), CSSM_APPLECSPDL_DB_LOCK, NULL, NULL));
}

void
DbImpl::unlock()
{
	check(CSSM_DL_PassThrough(handle(), CSSM_APPLECSPDL_DB_UNLOCK, NULL, NULL));
}

void
DbImpl::unlock(const CSSM_DATA &password)
{
	check(CSSM_DL_PassThrough(handle(), CSSM_APPLECSPDL_DB_UNLOCK, &password, NULL));
}

void
DbImpl::getSettings(uint32 &outIdleTimeout, bool &outLockOnSleep)
{
	CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS_PTR settings;
	check(CSSM_DL_PassThrough(handle(), CSSM_APPLECSPDL_DB_GET_SETTINGS,
							  NULL, reinterpret_cast<void **>(&settings)));
	outIdleTimeout = settings->idleTimeout;
	outLockOnSleep = settings->lockOnSleep;
	allocator().free(settings);
}

void
DbImpl::setSettings(uint32 inIdleTimeout, bool inLockOnSleep)
{
	CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS settings;
	settings.idleTimeout = inIdleTimeout;
	settings.lockOnSleep = inLockOnSleep;
	check(CSSM_DL_PassThrough(handle(), CSSM_APPLECSPDL_DB_SET_SETTINGS, &settings, NULL));
}

bool
DbImpl::isLocked()
{
	CSSM_APPLECSPDL_DB_IS_LOCKED_PARAMETERS_PTR params;
	check(CSSM_DL_PassThrough(handle(), CSSM_APPLECSPDL_DB_IS_LOCKED,
							  NULL, reinterpret_cast<void **>(&params)));
	bool isLocked = params->isLocked;
	allocator().free(params);
	return isLocked;
}

void
DbImpl::changePassphrase(const CSSM_ACCESS_CREDENTIALS *cred)
{
	CSSM_APPLECSPDL_DB_CHANGE_PASSWORD_PARAMETERS params;
	params.accessCredentials = const_cast<CSSM_ACCESS_CREDENTIALS *>(cred);
	check(CSSM_DL_PassThrough(handle(), CSSM_APPLECSPDL_DB_CHANGE_PASSWORD, &params, NULL));
}

bool
DbImpl::getUnlockKeyIndex(CssmData &index)
{
	try {
		SecurityServer::DbHandle dbHandle;
		if (CSSM_DL_PassThrough(handle(),
			CSSM_APPLECSPDL_DB_GET_HANDLE, NULL, (void **)&dbHandle))
				return false;	// can't get index
		SecurityServer::ClientSession ss(allocator(), allocator());
		ss.getDbSuggestedIndex(dbHandle, index);
		return true;
	} catch (const CssmError &error) {
		if (error.cssmError() == CSSMERR_DL_DATASTORE_DOESNOT_EXIST)
			return false;
		throw;
	}
}


//
// DbCursorMaker
//
DbCursorImpl *
DbImpl::newDbCursor(const CSSM_QUERY &query, CssmAllocator &allocator)
{
	return new DbDbCursorImpl(Db(this), query, allocator);
}

DbCursorImpl *
DbImpl::newDbCursor(uint32 capacity, CssmAllocator &allocator)
{
	return new DbDbCursorImpl(Db(this), capacity, allocator);
}

//
// DbUniqueRecordMaker
//
DbUniqueRecordImpl *
DbImpl::newDbUniqueRecord()
{
	return new DbUniqueRecordImpl(Db(this));
}


//
// Utility methods
//
DLDbIdentifier
DbImpl::dlDbIdentifier() const
{
	return DLDbIdentifier(dl()->subserviceUid(), name(), dbLocation());
}


//
// DbDbCursorImpl
//
DbDbCursorImpl::DbDbCursorImpl(const Db &db, const CSSM_QUERY &query, CssmAllocator &allocator)
: DbCursorImpl(db, query, allocator), mResultsHandle(CSSM_INVALID_HANDLE)
{
}

DbDbCursorImpl::DbDbCursorImpl(const Db &db, uint32 capacity, CssmAllocator &allocator)
: DbCursorImpl(db, capacity, allocator), mResultsHandle(CSSM_INVALID_HANDLE)
{
}

DbDbCursorImpl::~DbDbCursorImpl()
{
	try
	{
		deactivate();
	}
	catch(...) {}
}

bool
DbDbCursorImpl::next(DbAttributes *attributes, ::CssmDataContainer *data, DbUniqueRecord &uniqueId)
{
	if (attributes)
		attributes->deleteValues();

	if (data)
		data->clear();

	CSSM_RETURN result;
	Db db(database());
	DbUniqueRecord unique(db);
	if (!mActive)
	{
		result = CSSM_DL_DataGetFirst(db->handle(),
									  this,
									  &mResultsHandle,
									  attributes,
									  data,
									  unique);
		if (result == CSSM_OK)
			mActive = true;
		else if (data != NULL)
			data->invalidate ();
	}
	else
	{
		result = CSSM_DL_DataGetNext(db->handle(),
									 mResultsHandle,
									 attributes,
									 data,
									 unique);
		
		if (result != CSSM_OK && data != NULL)
		{
			data->invalidate ();
		}
	}

	if (result == CSSMERR_DL_ENDOFDATA)
	{
		mActive = false;
		return false;
	}

	check(result);

	// Activate uniqueId so CSSM_DL_FreeUniqueRecord() gets called when it goes out of scope.
	unique->activate();
	uniqueId = unique;
	return true;
}

void
DbDbCursorImpl::activate()
{
}

void
DbDbCursorImpl::deactivate()
{
	if (mActive)
	{
		mActive = false;
		check(CSSM_DL_DataAbortQuery(database()->handle(), mResultsHandle));
	}
}


//
// DbCursorImpl
//
DbCursorImpl::DbCursorImpl(const Object &parent, const CSSM_QUERY &query, CssmAllocator &allocator) :
ObjectImpl(parent), CssmAutoQuery(query, allocator)
{
}

DbCursorImpl::DbCursorImpl(const Object &parent, uint32 capacity, CssmAllocator &allocator) :
ObjectImpl(parent), CssmAutoQuery(capacity, allocator)
{
}

CssmAllocator &
DbCursorImpl::allocator() const
{
	return ObjectImpl::allocator();
}

void
DbCursorImpl::allocator(CssmAllocator &alloc)
{
	ObjectImpl::allocator(alloc);
}


//
// DbUniqueRecord
//
DbUniqueRecordImpl::DbUniqueRecordImpl(const Db &db) : ObjectImpl(db)
{
}

DbUniqueRecordImpl::~DbUniqueRecordImpl()
{
	try
	{
		deactivate();
	}
	catch(...) {}
}

void
DbUniqueRecordImpl::deleteRecord()
{
	check(CSSM_DL_DataDelete(database()->handle(), mUniqueId));
}

void
DbUniqueRecordImpl::modify(CSSM_DB_RECORDTYPE recordType,
						   const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
						   const CSSM_DATA *data,
						   CSSM_DB_MODIFY_MODE modifyMode)
{
	check(CSSM_DL_DataModify(database()->handle(), recordType, mUniqueId,
							 attributes,
							 data, modifyMode));
}

void
DbUniqueRecordImpl::get(DbAttributes *attributes,
						::CssmDataContainer *data)
{
	if (attributes)
		attributes->deleteValues();

	if (data)
		data->clear();

	// @@@ Fix the allocators for attributes and data.
	CSSM_RETURN result;
	result = CSSM_DL_DataGetFromUniqueRecordId(database()->handle(), mUniqueId,
											attributes,
											data);
											
	if (result != CSSM_OK && data != NULL) // the data returned is no longer valid
	{
		data->invalidate ();
	}
	
	check(result);
}

void
DbUniqueRecordImpl::activate()
{
	mActive = true;
}

void
DbUniqueRecordImpl::deactivate()
{
	if (mActive)
	{
		mActive = false;
		check(CSSM_DL_FreeUniqueRecord(database()->handle(), mUniqueId));
	}
}


//
// DbAttributes
//
DbAttributes::DbAttributes()
:  CssmAutoDbRecordAttributeData(0, CssmAllocator::standard(), CssmAllocator::standard())
{
}

DbAttributes::DbAttributes(const Db &db, uint32 capacity, CssmAllocator &allocator)
:  CssmAutoDbRecordAttributeData(capacity, db->allocator(), allocator)
{
}
