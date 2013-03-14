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
#include <security_cdsa_client/dlclient.h>
#include <security_cdsa_client/aclclient.h>
#include <Security/cssmapple.h>
#include <Security/cssmapplePriv.h>

using namespace CssmClient;


// blob type for blobs created by these classes -- done so that we can change the formats later
const uint32 kBlobType = 0x1;


//
// Abstract classes
//
DbMaker::~DbMaker()
{ /* virtual */ }

DbCursorMaker::~DbCursorMaker()
{ /* virtual */ }

DbUniqueRecordMaker::~DbUniqueRecordMaker()
{ /* virtual */ }


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
	mUseNameFromHandle(!inDbName), mNameFromHandle(NULL),
	mAccessRequest(CSSM_DB_ACCESS_READ), mAccessCredentials(NULL),
	mDefaultCredentials(NULL), mOpenParameters(NULL), mDbInfo(NULL),
	mResourceControlContext(NULL)
{
}

DbImpl::~DbImpl()
{
	try
	{
		if (mNameFromHandle)
			allocator().free(mNameFromHandle);
		deactivate();
	}
	catch(...) {}
}

void
DbImpl::open()
{
    {
        StLock<Mutex> _(mActivateMutex);

        if (!mActive)
        {
            assert(mDbInfo == nil);
            mHandle.DLHandle = dl()->handle();
            check(CSSM_DL_DbOpen(mHandle.DLHandle, mDbName.canonicalName(), dbLocation(),
                                    mAccessRequest, mAccessCredentials,
                                    mOpenParameters, &mHandle.DBHandle));
            mActive = true;
        }
    }
    
    if (!mAccessCredentials && mDefaultCredentials)
        if (const AccessCredentials *creds = mDefaultCredentials->makeCredentials())
            CSSM_DL_Authenticate(handle(), mAccessRequest, creds);	// ignore error
}

void
DbImpl::createWithBlob(CssmData &blob)
{
	if (mActive)
		CssmError::throwMe(CSSMERR_DL_DATASTORE_ALREADY_EXISTS);

	if (mDbInfo == nil) {
		// handle a missing (null) mDbInfo as an all-zero one
		static const CSSM_DBINFO nullDbInfo = { };
		mDbInfo = &nullDbInfo;
	}

	mHandle.DLHandle = dl()->handle();

	// create a parameter block for our call to the passthrough
	CSSM_APPLE_CSPDL_DB_CREATE_WITH_BLOB_PARAMETERS params;
	
	params.dbName = mDbName.canonicalName ();
	params.dbLocation = dbLocation ();
	params.dbInfo = mDbInfo;
	params.accessRequest = mAccessRequest;
	params.credAndAclEntry = NULL;
	params.openParameters = mOpenParameters;
	params.blob = &blob;

	check(CSSM_DL_PassThrough (mHandle, CSSM_APPLECSPDL_DB_CREATE_WITH_BLOB, &params, (void**) &mHandle.DBHandle));
}

void
DbImpl::create()
{
    StLock<Mutex> _(mActivateMutex);
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
		check(CSSM_DL_DbCreate(mHandle.DLHandle, mDbName.canonicalName(), dbLocation(),
							mDbInfo, mAccessRequest, &ctx,
							mOpenParameters, &mHandle.DBHandle));
	} else {
		check(CSSM_DL_DbCreate(mHandle.DLHandle, mDbName.canonicalName(), dbLocation(),
							mDbInfo, mAccessRequest, mResourceControlContext,
							mOpenParameters, &mHandle.DBHandle));
	}
	mActive = true;
}

void
DbImpl::close()
{
    StLock<Mutex> _(mActivateMutex);
	if (mActive)
	{
		check(CSSM_DL_DbClose (mHandle));
		mActive = false;
	}
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
    StLock<Mutex> _(mActivateMutex);
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
	check(CSSM_DL_DbDelete(dl()->handle(), mDbName.canonicalName(), dbLocation(),
						   mAccessCredentials));
}

void
DbImpl::rename(const char *newName)
{
	// Deactivate so the db gets closed if it was open.
	deactivate();
    if (::rename(mDbName.canonicalName(), newName))
		UnixError::throwMe(errno);

	// Change our DbName to reflect this rename.
	mDbName = DbName(newName, dbLocation());
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

const char *
DbImpl::name()
{
	if (mUseNameFromHandle)
	{
		if (mNameFromHandle
		    || !CSSM_DL_GetDbNameFromHandle(handle(), &mNameFromHandle))
		{
			return mNameFromHandle;
		}

		// We failed to get the name from the handle so use the passed
		// in name instead
		mUseNameFromHandle = false;
	}

	return mDbName.canonicalName();
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


DbUniqueRecord
DbImpl::insertWithoutEncryption(CSSM_DB_RECORDTYPE recordType, const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
								CSSM_DATA *data)
{
	DbUniqueRecord uniqueId(Db(this));

	// fill out the parameters
	CSSM_APPLECSPDL_DB_INSERT_WITHOUT_ENCRYPTION_PARAMETERS params;
	params.recordType = recordType;
	params.attributes = const_cast<CSSM_DB_RECORD_ATTRIBUTE_DATA*>(attributes);
	params.data = *data;

	// for clarity, call the overloaded operator to produce a unique record pointer
	CSSM_DB_UNIQUE_RECORD_PTR *uniquePtr = uniqueId;

	// make the call
	passThrough (CSSM_APPLECSPDL_DB_INSERT_WITHOUT_ENCRYPTION, &params, (void**) uniquePtr);
	
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

void DbImpl::recode(const CSSM_DATA &data, const CSSM_DATA &extraData)
{
	// setup parameters for the recode call
	CSSM_APPLECSPDL_RECODE_PARAMETERS params;
	params.dbBlob = data;
	params.extraData = extraData;

	// do the call
	check(CSSM_DL_PassThrough(handle(), CSSM_APPLECSPDL_CSP_RECODE, &params, NULL));
}

void DbImpl::copyBlob (CssmData &data)
{
	// do the call
	check(CSSM_DL_PassThrough(handle(), CSSM_APPLECSPDL_DB_COPY_BLOB, NULL, (void**) (CSSM_DATA*) &data));
}

void DbImpl::setBatchMode(Boolean mode, Boolean rollback)
{
	//
	// We need the DL_DB_Handle of the underyling DL in order to use CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT
	// 
	CSSM_RETURN result;
	CSSM_DL_DB_HANDLE dldbHandleOfUnderlyingDL;
	result = CSSM_DL_PassThrough(handle(),
								 CSSM_APPLECSPDL_DB_GET_HANDLE,
								 NULL,
								 (void **)&dldbHandleOfUnderlyingDL);
	//
	// Now, toggle the autocommit...
	//
	if ( result == noErr )
	{
		CSSM_BOOL modeToUse = !mode;
		if (rollback)
		{
			result = (OSStatus)CSSM_DL_PassThrough(dldbHandleOfUnderlyingDL,
										CSSM_APPLEFILEDL_ROLLBACK, NULL, NULL);
		}

		result = CSSM_DL_PassThrough(dldbHandleOfUnderlyingDL,
									 CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT,
									 (void *)(modeToUse),
									 NULL);
		if (!rollback && modeToUse)
			result = CSSM_DL_PassThrough(dldbHandleOfUnderlyingDL,
									 CSSM_APPLEFILEDL_COMMIT,
									 NULL,
									 NULL);
	}
}

//
// DbCursorMaker
//
DbCursorImpl *
DbImpl::newDbCursor(const CSSM_QUERY &query, Allocator &allocator)
{
	return new DbDbCursorImpl(Db(this), query, allocator);
}

DbCursorImpl *
DbImpl::newDbCursor(uint32 capacity, Allocator &allocator)
{
	return new DbDbCursorImpl(Db(this), capacity, allocator);
}


//
// Db adapters for AclBearer
//
void DbImpl::getAcl(AutoAclEntryInfoList &aclInfos, const char *selectionTag) const
{
	aclInfos.allocator(allocator());
	check(CSSM_DL_GetDbAcl(const_cast<DbImpl*>(this)->handle(),
		reinterpret_cast<const CSSM_STRING *>(selectionTag), aclInfos, aclInfos));
}

void DbImpl::changeAcl(const CSSM_ACL_EDIT &aclEdit,
	const CSSM_ACCESS_CREDENTIALS *accessCred)
{
	check(CSSM_DL_ChangeDbAcl(handle(), AccessCredentials::needed(accessCred), &aclEdit));
}

void DbImpl::getOwner(AutoAclOwnerPrototype &owner) const
{
	owner.allocator(allocator());
	check(CSSM_DL_GetDbOwner(const_cast<DbImpl*>(this)->handle(), owner));
}

void DbImpl::changeOwner(const CSSM_ACL_OWNER_PROTOTYPE &newOwner,
	const CSSM_ACCESS_CREDENTIALS *accessCred)
{
	check(CSSM_DL_ChangeDbOwner(handle(),
		AccessCredentials::needed(accessCred), &newOwner));
}

void DbImpl::defaultCredentials(DefaultCredentialsMaker *maker)
{
	mDefaultCredentials = maker;
}


//
// Abstract DefaultCredentialsMakers
//
DbImpl::DefaultCredentialsMaker::~DefaultCredentialsMaker()
{ /* virtual */ }


//
// Db adapters for DLAccess
//
CSSM_HANDLE Db::dlGetFirst(const CSSM_QUERY &query,	CSSM_DB_RECORD_ATTRIBUTE_DATA &attributes, 
	CSSM_DATA *data, CSSM_DB_UNIQUE_RECORD *&id)
{
	CSSM_HANDLE result;
	switch (CSSM_RETURN rc = CSSM_DL_DataGetFirst(handle(), &query, &result, &attributes, data, &id)) {
	case CSSM_OK:
		return result;
	case CSSMERR_DL_ENDOFDATA:
		return CSSM_INVALID_HANDLE;
	default:
		CssmError::throwMe(rc);
		return CSSM_INVALID_HANDLE; // placebo
	}
}

bool Db::dlGetNext(CSSM_HANDLE query, CSSM_DB_RECORD_ATTRIBUTE_DATA &attributes,
	CSSM_DATA *data, CSSM_DB_UNIQUE_RECORD *&id)
{
	CSSM_RETURN rc = CSSM_DL_DataGetNext(handle(), query, &attributes, data, &id);
	switch (rc) {
	case CSSM_OK:
		return true;
	case CSSMERR_DL_ENDOFDATA:
		return false;
	default:
		CssmError::throwMe(rc);
		return false;   // placebo
	}
}

void Db::dlAbortQuery(CSSM_HANDLE query)
{
	CssmError::check(CSSM_DL_DataAbortQuery(handle(), query));
}

void Db::dlFreeUniqueId(CSSM_DB_UNIQUE_RECORD *id)
{
	CssmError::check(CSSM_DL_FreeUniqueRecord(handle(), id));
}

void Db::dlDeleteRecord(CSSM_DB_UNIQUE_RECORD *id)
{
	CssmError::check(CSSM_DL_DataDelete(handle(), id));
}

Allocator &Db::allocator()
{
	return Object::allocator();
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
DbImpl::dlDbIdentifier()
{
	// Always use the same dbName and dbLocation that were passed in during
	// construction
	return DLDbIdentifier(dl()->subserviceUid(), mDbName.canonicalName(), dbLocation());
}


//
// DbDbCursorImpl
//
DbDbCursorImpl::DbDbCursorImpl(const Db &db, const CSSM_QUERY &query, Allocator &allocator)
: DbCursorImpl(db, query, allocator), mResultsHandle(CSSM_INVALID_HANDLE)
{
}

DbDbCursorImpl::DbDbCursorImpl(const Db &db, uint32 capacity, Allocator &allocator)
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
		// ask the CSP/DL if the requested  record type exists
		CSSM_BOOL boolResult;
		CSSM_DL_PassThrough(db->handle(), CSSM_APPLECSPDL_DB_RELATION_EXISTS, &RecordType, (void**) &boolResult);
		if (!boolResult)
		{
			if (data != NULL)
			{
				data->invalidate();
			}
			
			return false;
		}
		
		result = CSSM_DL_DataGetFirst(db->handle(),
									  this,
									  &mResultsHandle,
									  attributes,
									  data,
									  unique);

        StLock<Mutex> _(mActivateMutex);
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

	if (result != CSSM_OK && attributes != NULL)
	{
		attributes->invalidate();
	}
	
	if (result == CSSMERR_DL_ENDOFDATA)
	{
        StLock<Mutex> _(mActivateMutex);
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
    StLock<Mutex> _(mActivateMutex);
	if (mActive)
	{
		mActive = false;
		check(CSSM_DL_DataAbortQuery(database()->handle(), mResultsHandle));
	}
}


//
// DbCursorImpl
//
DbCursorImpl::DbCursorImpl(const Object &parent, const CSSM_QUERY &query, Allocator &allocator) :
ObjectImpl(parent), CssmAutoQuery(query, allocator)
{
}

DbCursorImpl::DbCursorImpl(const Object &parent, uint32 capacity, Allocator &allocator) :
ObjectImpl(parent), CssmAutoQuery(capacity, allocator)
{
}

Allocator &
DbCursorImpl::allocator() const
{
	return ObjectImpl::allocator();
}

void
DbCursorImpl::allocator(Allocator &alloc)
{
	ObjectImpl::allocator(alloc);
}


//
// DbUniqueRecord
//
DbUniqueRecordImpl::DbUniqueRecordImpl(const Db &db) : ObjectImpl(db), mDestroyID (false)
{
}

DbUniqueRecordImpl::~DbUniqueRecordImpl()
{
	try
	{
		if (mDestroyID)
		{
			allocator ().free (mUniqueId);
		}
		
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
DbUniqueRecordImpl::modifyWithoutEncryption(CSSM_DB_RECORDTYPE recordType,
											const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
											const CSSM_DATA *data,
											CSSM_DB_MODIFY_MODE modifyMode)
{
	// fill out the parameters
	CSSM_APPLECSPDL_DB_MODIFY_WITHOUT_ENCRYPTION_PARAMETERS params;
	params.recordType = recordType;
	params.uniqueID = mUniqueId;
	params.attributes = const_cast<CSSM_DB_RECORD_ATTRIBUTE_DATA*>(attributes);
	params.data = (CSSM_DATA*) data;
	params.modifyMode = modifyMode;
	
	// modify the data
	check(CSSM_DL_PassThrough(database()->handle(),
		  CSSM_APPLECSPDL_DB_MODIFY_WITHOUT_ENCRYPTION,
		  &params, 
		  NULL));
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
											
	if (result != CSSM_OK)
	{
        if (attributes)
            attributes->invalidate();
		if (data != NULL) // the data returned is no longer valid
		{
			data->invalidate ();
		}
	}
	
	check(result);
}

void
DbUniqueRecordImpl::getWithoutEncryption(DbAttributes *attributes,
										 ::CssmDataContainer *data)
{
	if (attributes)
		attributes->deleteValues();

	if (data)
		data->clear();

	// @@@ Fix the allocators for attributes and data.
	CSSM_RETURN result;
	
	// make the parameter block
	CSSM_APPLECSPDL_DB_GET_WITHOUT_ENCRYPTION_PARAMETERS params;
	params.uniqueID = mUniqueId;
	params.attributes = attributes;
	
	// get the data
	::CssmDataContainer recordData;
	result = CSSM_DL_PassThrough(database()->handle(), CSSM_APPLECSPDL_DB_GET_WITHOUT_ENCRYPTION, &params,
								 (void**) data);
	check (result);
}

void
DbUniqueRecordImpl::activate()
{
    StLock<Mutex> _(mActivateMutex);
	mActive = true;
}

void
DbUniqueRecordImpl::deactivate()
{
    StLock<Mutex> _(mActivateMutex);
	if (mActive)
	{
		mActive = false;
		check(CSSM_DL_FreeUniqueRecord(database()->handle(), mUniqueId));
	}
}

void
DbUniqueRecordImpl::getRecordIdentifier(CSSM_DATA &data)
{
	check(CSSM_DL_PassThrough(database()->handle(), CSSM_APPLECSPDL_DB_GET_RECORD_IDENTIFIER,
		  mUniqueId, (void**) &data));
}

void DbUniqueRecordImpl::setUniqueRecordPtr(CSSM_DB_UNIQUE_RECORD_PTR uniquePtr)
{
	// clone the record
	mUniqueId = (CSSM_DB_UNIQUE_RECORD_PTR) allocator ().malloc (sizeof (CSSM_DB_UNIQUE_RECORD));
	*mUniqueId = *uniquePtr;
	mDestroyID = true;
}

//
// DbAttributes
//
DbAttributes::DbAttributes()
:  CssmAutoDbRecordAttributeData(0, Allocator::standard(), Allocator::standard())
{
}

DbAttributes::DbAttributes(const Db &db, uint32 capacity, Allocator &allocator)
:  CssmAutoDbRecordAttributeData(capacity, db->allocator(), allocator)
{
}
