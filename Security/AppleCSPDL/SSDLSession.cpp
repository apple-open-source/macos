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
// SSDLSession.h - DL session for security server CSP/DL.
//
#include "SSDLSession.h"

#include "CSPDLPlugin.h"
#include "SSKey.h"

using namespace CssmClient;
using namespace SecurityServer;
using namespace std;

//
// SSDLSession -- Security Server DL session
//
SSDLSession::SSDLSession(CSSM_MODULE_HANDLE handle,
						 CSPDLPlugin &plug,
						 const CSSM_VERSION &version,
						 uint32 subserviceId,
						 CSSM_SERVICE_TYPE subserviceType,
						 CSSM_ATTACH_FLAGS attachFlags,
						 const CSSM_UPCALLS &upcalls,
						 DatabaseManager &databaseManager,
						 SSCSPDLSession &ssCSPDLSession)
: DLPluginSession(handle, plug, version, subserviceId, subserviceType,
				  attachFlags, upcalls, databaseManager),
  mSSCSPDLSession(ssCSPDLSession),
  mDL(Module(gGuidAppleFileDL, Cssm::standard())),
  mClientSession(CssmAllocator::standard(), static_cast<PluginSession &>(*this))
{
	mClientSession.registerForAclEdits(SSCSPDLSession::didChangeKeyAclCallback, &mSSCSPDLSession);
	// @@@ mDL.allocator(*static_cast<DatabaseSession *>(this));
	mDL->allocator(allocator());
	mDL->version(version);
	mDL->subserviceId(subserviceId);
	mDL->flags(attachFlags);
}

SSDLSession::~SSDLSession()
{
	// @@@ What about a catch?
	StLock<Mutex> _1(mSSUniqueRecordLock);
	mSSUniqueRecordMap.clear();

	StLock<Mutex> _2(mDbHandleLock);
	DbHandleMap::iterator end = mDbHandleMap.end();
	for (DbHandleMap::iterator it = mDbHandleMap.begin(); it != end; ++it)
		it->second->close();

	mDbHandleMap.clear();
	mDL->detach();
}

// Utility functions
void
SSDLSession::GetDbNames(CSSM_NAME_LIST_PTR &outNameList)
{
	// @@@ Fix client lib
	CSSM_DL_GetDbNames(mDL->handle(), &outNameList);
}


void
SSDLSession::FreeNameList(CSSM_NAME_LIST &inNameList)
{
	// @@@ Fix client lib
	CSSM_DL_FreeNameList(mDL->handle(), &inNameList);
}


void
SSDLSession::DbDelete(const char *inDbName,
					  const CSSM_NET_ADDRESS *inDbLocation,
					  const AccessCredentials *inAccessCred)
{
	SSDatabase db(mClientSession, mDL, inDbName, inDbLocation);
	db->accessCredentials(inAccessCred);
	db->deleteDb();
}

// DbContext creation and destruction.
void
SSDLSession::DbCreate(const char *inDbName,
					  const CSSM_NET_ADDRESS *inDbLocation,
					  const CSSM_DBINFO &inDBInfo,
					  CSSM_DB_ACCESS_TYPE inAccessRequest,
					  const CSSM_RESOURCE_CONTROL_CONTEXT *inCredAndAclEntry,
					  const void *inOpenParameters,
					  CSSM_DB_HANDLE &outDbHandle)
{
	SSDatabase db(mClientSession, mDL, inDbName, inDbLocation);
	db->dbInfo(&inDBInfo);
	db->accessRequest(inAccessRequest);
	db->resourceControlContext(inCredAndAclEntry);
	db->openParameters(inOpenParameters);
	db->create(DLDbIdentifier(CssmSubserviceUid(plugin.myGuid(), &version(), subserviceId(),
												CSSM_SERVICE_DL | CSSM_SERVICE_CSP),
							  inDbName, inDbLocation));
	db->dbInfo(NULL);
	outDbHandle = makeDbHandle(db);
}

void
SSDLSession::DbOpen(const char *inDbName,
					const CSSM_NET_ADDRESS *inDbLocation,
					CSSM_DB_ACCESS_TYPE inAccessRequest,
					const AccessCredentials *inAccessCred,
					const void *inOpenParameters,
					CSSM_DB_HANDLE &outDbHandle)
{
	SSDatabase db(mClientSession, mDL, inDbName, inDbLocation);
	db->accessRequest(inAccessRequest);
	db->accessCredentials(inAccessCred);
	db->openParameters(inOpenParameters);
	db->open(DLDbIdentifier(CssmSubserviceUid(plugin.myGuid(), &version(), subserviceId(),
											  CSSM_SERVICE_DL | CSSM_SERVICE_CSP),
							inDbName, inDbLocation));
	outDbHandle = makeDbHandle(db);
}

// Operations using DbContext instances.
void
SSDLSession::DbClose(CSSM_DB_HANDLE inDbHandle)
{
	killDbHandle(inDbHandle)->close();
}

void
SSDLSession::CreateRelation(CSSM_DB_HANDLE inDbHandle,
							CSSM_DB_RECORDTYPE inRelationID,
							const char *inRelationName,
							uint32 inNumberOfAttributes,
							const CSSM_DB_SCHEMA_ATTRIBUTE_INFO &inAttributeInfo,
							uint32 inNumberOfIndexes,
							const CSSM_DB_SCHEMA_INDEX_INFO &inIndexInfo)
{
	SSDatabase db = findDbHandle(inDbHandle);
	// @@@ Fix inAttributeInfo and inIndexInfo arguments (might be NULL if NumberOf = 0)
	db->createRelation(inRelationID, inRelationName,
					  inNumberOfAttributes, &inAttributeInfo,
					  inNumberOfIndexes, &inIndexInfo);
}

void
SSDLSession::DestroyRelation(CSSM_DB_HANDLE inDbHandle,
							 CSSM_DB_RECORDTYPE inRelationID)
{
	// @@@ Check credentials.
	SSDatabase db = findDbHandle(inDbHandle);
	db->destroyRelation(inRelationID);
}

void
SSDLSession::Authenticate(CSSM_DB_HANDLE inDbHandle,
						  CSSM_DB_ACCESS_TYPE inAccessRequest,
						  const AccessCredentials &inAccessCred)
{
	SSDatabase db = findDbHandle(inDbHandle);
	db->authenticate(inAccessRequest, &inAccessCred);
}


void
SSDLSession::GetDbAcl(CSSM_DB_HANDLE inDbHandle,
					  const CSSM_STRING *inSelectionTag,
					  uint32 &outNumberOfAclInfos,
					  CSSM_ACL_ENTRY_INFO_PTR &outAclInfos)
{
	SSDatabase db = findDbHandle(inDbHandle);
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
SSDLSession::ChangeDbAcl(CSSM_DB_HANDLE inDbHandle,
						 const AccessCredentials &inAccessCred,
						 const CSSM_ACL_EDIT &inAclEdit)
{
	SSDatabase db = findDbHandle(inDbHandle);
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
SSDLSession::GetDbOwner(CSSM_DB_HANDLE inDbHandle,
						CSSM_ACL_OWNER_PROTOTYPE &outOwner)
{
	SSDatabase db = findDbHandle(inDbHandle);
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
SSDLSession::ChangeDbOwner(CSSM_DB_HANDLE inDbHandle,
						   const AccessCredentials &inAccessCred,
						   const CSSM_ACL_OWNER_PROTOTYPE &inNewOwner)
{
	SSDatabase db = findDbHandle(inDbHandle);
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
SSDLSession::GetDbNameFromHandle(CSSM_DB_HANDLE inDbHandle,
                                 char **outDbName)
{
	SSDatabase db = findDbHandle(inDbHandle);
	// @@@ Fix this functions signature.
	db->name(*outDbName);
}

void
SSDLSession::DataInsert(CSSM_DB_HANDLE inDbHandle,
						CSSM_DB_RECORDTYPE inRecordType,
						const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
						const CssmData *inData,
						CSSM_DB_UNIQUE_RECORD_PTR &outUniqueId)
{
	SSDatabase db = findDbHandle(inDbHandle);
	// @@@ Fix client lib.
    SSUniqueRecord uniqueId = db->insert(inRecordType, inAttributes, inData, true); // @@@ Fix me
	outUniqueId = makeSSUniqueRecord(uniqueId);
	// @@@ If this is a key do the right thing.
}

void
SSDLSession::DataDelete(CSSM_DB_HANDLE inDbHandle,
						const CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier)
{
	SSDatabase db = findDbHandle(inDbHandle);
	SSUniqueRecord uniqueId = findSSUniqueRecord(inUniqueRecordIdentifier);
	uniqueId->deleteRecord();
	// @@@ If this is a key do the right thing.
}


void
SSDLSession::DataModify(CSSM_DB_HANDLE inDbHandle,
						CSSM_DB_RECORDTYPE inRecordType,
						CSSM_DB_UNIQUE_RECORD &inoutUniqueRecordIdentifier,
						const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributesToBeModified,
						const CssmData *inDataToBeModified,
						CSSM_DB_MODIFY_MODE inModifyMode)
{
	SSDatabase db = findDbHandle(inDbHandle);
	SSUniqueRecord uniqueId = findSSUniqueRecord(inoutUniqueRecordIdentifier);
	uniqueId->modify(inRecordType, inAttributesToBeModified, inDataToBeModified, inModifyMode);
	// @@@ If this is a key do the right thing.
}

CSSM_HANDLE
SSDLSession::DataGetFirst(CSSM_DB_HANDLE inDbHandle,
						  const DLQuery *inQuery,
						  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
						  CssmData *inoutData,
						  CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
	SSDatabase db = findDbHandle(inDbHandle);
	CSSM_HANDLE resultsHandle = CSSM_INVALID_HANDLE;
    SSUniqueRecord uniqueId(db);

	// Setup so we always retrive the attributes even if the client
	// doesn't want them so we can figure out if we just retrived a key.
	CSSM_DB_RECORD_ATTRIBUTE_DATA attributes;
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR pAttributes;
	if (inoutAttributes)
		pAttributes = inoutAttributes;
	else
	{
		pAttributes = &attributes;
		memset(pAttributes, 0, sizeof(attributes));
	}

	// Retrive the record.
	CSSM_RETURN result = CSSM_DL_DataGetFirst(db->handle(), inQuery, &resultsHandle,
											  pAttributes, inoutData, uniqueId);
	if (result)
	{
		if (result == CSSMERR_DL_ENDOFDATA)
			return CSSM_INVALID_HANDLE;

		CssmError::throwMe(result);
	}

	uniqueId->activate();

	// If we the client didn't ask for data then it doesn't matter
	// if this record is a key or not, just return it.
	if (inoutData)
	{
		if (pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PUBLIC_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PRIVATE_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
		{
			// This record is a key, do the right thing (tm).
			// Allocate storage for the key.
			CssmKey *outKey = allocator().alloc<CssmKey>();
			new SSKey(*this, *outKey, db, uniqueId, pAttributes->DataRecordType, *inoutData);

			// Free the data we retrived (keyblob)
			allocator().free(inoutData->Data);

			// Set the length and data on the data we return to the client
			inoutData->Length = sizeof(*outKey);
			inoutData->Data = reinterpret_cast<uint8 *>(outKey);			
		}
	}

	outUniqueRecord = makeSSUniqueRecord(uniqueId);
	return resultsHandle;
}

bool
SSDLSession::DataGetNext(CSSM_DB_HANDLE inDbHandle,
						 CSSM_HANDLE inResultsHandle,
						 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
						 CssmData *inoutData,
						 CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
	// @@@ If this is a key do the right thing.
	SSDatabase db = findDbHandle(inDbHandle);
    SSUniqueRecord uniqueId(db);

	// Setup so we always retrive the attributes even if the client
	// doesn't want them so we can figure out if we just retrived a key.
	CSSM_DB_RECORD_ATTRIBUTE_DATA attributes;
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR pAttributes;
	if (inoutAttributes)
		pAttributes = inoutAttributes;
	else
	{
		pAttributes = &attributes;
		memset(pAttributes, 0, sizeof(attributes));
	}

	CSSM_RETURN result = CSSM_DL_DataGetNext(db->handle(), inResultsHandle,
											 inoutAttributes, inoutData, uniqueId);
	if (result)
	{
		if (result == CSSMERR_DL_ENDOFDATA)
			return false;

		CssmError::throwMe(result);
	}

	uniqueId->activate();

	// If we the client didn't ask for data then it doesn't matter
	// if this record is a key or not, just return it.
	if (inoutData)
	{
		if (pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PUBLIC_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PRIVATE_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
		{
			// This record is a key, do the right thing (tm).
			// Allocate storage for the key.
			CssmKey *outKey = allocator().alloc<CssmKey>();
			new SSKey(*this, *outKey, db, uniqueId, pAttributes->DataRecordType, *inoutData);

			// Free the data we retrived (keyblob)
			allocator().free(inoutData->Data);

			// Set the length and data on the data we return to the client
			inoutData->Length = sizeof(*outKey);
			inoutData->Data = reinterpret_cast<uint8 *>(outKey);			
		}
	}

	outUniqueRecord = makeSSUniqueRecord(uniqueId);

	return true;
}

void
SSDLSession::DataAbortQuery(CSSM_DB_HANDLE inDbHandle,
							CSSM_HANDLE inResultsHandle)
{
	// @@@ If this is a key do the right thing.
	SSDatabase db = findDbHandle(inDbHandle);
	CSSM_RETURN result = CSSM_DL_DataAbortQuery(db->handle(), inResultsHandle);
	if (result)
		CssmError::throwMe(result);
}

void
SSDLSession::DataGetFromUniqueRecordId(CSSM_DB_HANDLE inDbHandle,
									   const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
									   CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
									   CssmData *inoutData)
{
	SSDatabase db = findDbHandle(inDbHandle);
	const SSUniqueRecord uniqueId = findSSUniqueRecord(inUniqueRecord);
	
	// Setup so we always retrive the attributes even if the client
	// doesn't want them so we can figure out if we just retrived a key.
	CSSM_DB_RECORD_ATTRIBUTE_DATA attributes;
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR pAttributes;
	if (inoutAttributes)
		pAttributes = inoutAttributes;
	else
	{
		pAttributes = &attributes;
		memset(pAttributes, 0, sizeof(attributes));
	}

	CSSM_RETURN result = CSSM_DL_DataGetFromUniqueRecordId(db->handle(), 
		uniqueId, pAttributes, inoutData);
	if (result)
		CssmError::throwMe(result);

	if (inoutData)
	{
		if (pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PUBLIC_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_PRIVATE_KEY
			|| pAttributes->DataRecordType == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)
		{
			// This record is a key, do the right thing (tm).
			// Allocate storage for the key.
			CssmKey *outKey = allocator().alloc<CssmKey>();
			new SSKey(*this, *outKey, db, uniqueId, pAttributes->DataRecordType, *inoutData);

			// Free the data we retrived (keyblob)
			allocator().free(inoutData->Data);

			// Set the length and data on the data we return to the client
			inoutData->Length = sizeof(*outKey);
			inoutData->Data = reinterpret_cast<uint8 *>(outKey);			
		}
	}
}

void
SSDLSession::FreeUniqueRecord(CSSM_DB_HANDLE inDbHandle,
							  CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier)
{
	killSSUniqueRecord(inUniqueRecordIdentifier);
}

void
SSDLSession::PassThrough(CSSM_DB_HANDLE inDbHandle,
						 uint32 inPassThroughId,
						 const void *inInputParams,
						 void **outOutputParams)
{
	SSDatabase db = findDbHandle(inDbHandle);
	switch (inPassThroughId)
	{
		case CSSM_APPLECSPDL_DB_LOCK:
			db->lock();
			break;
		case CSSM_APPLECSPDL_DB_UNLOCK:
			if (inInputParams)
				db->unlock(*reinterpret_cast<const CSSM_DATA *>(inInputParams));
			else
				db->unlock();
			break;
		case CSSM_APPLECSPDL_DB_GET_SETTINGS:
		{
			if (!outOutputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_OUTPUT_POINTER);

			CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS_PTR params =
				allocator().alloc<CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS>();
			try
			{
				uint32 idleTimeout;
				bool lockOnSleep;
				db->getSettings(idleTimeout, lockOnSleep);
				params->idleTimeout = idleTimeout;
				params->lockOnSleep = lockOnSleep;
			}
			catch(...)
			{
				allocator().free(params);
				throw;
			}
			*reinterpret_cast<CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS_PTR *>(outOutputParams) = params;
			break;
		}
		case CSSM_APPLECSPDL_DB_SET_SETTINGS:
		{
			if (!inInputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_INPUT_POINTER);

			const CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS *params =
				reinterpret_cast<const CSSM_APPLECSPDL_DB_SETTINGS_PARAMETERS *>(inInputParams);
			db->setSettings(params->idleTimeout, params->lockOnSleep);
			break;
		}
		case CSSM_APPLECSPDL_DB_IS_LOCKED:
		{
			if (!outOutputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_OUTPUT_POINTER);

			CSSM_APPLECSPDL_DB_IS_LOCKED_PARAMETERS_PTR params =
				allocator().alloc<CSSM_APPLECSPDL_DB_IS_LOCKED_PARAMETERS>();
			try
			{
				params->isLocked = db->isLocked();
			}
			catch(...)
			{
				allocator().free(params);
				throw;
			}
			*reinterpret_cast<CSSM_APPLECSPDL_DB_IS_LOCKED_PARAMETERS_PTR *>(outOutputParams) = params;
			break;
		}
		case CSSM_APPLECSPDL_DB_CHANGE_PASSWORD:
		{
			if (!inInputParams)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_INPUT_POINTER);

			const CSSM_APPLECSPDL_DB_CHANGE_PASSWORD_PARAMETERS *params =
				reinterpret_cast<const CSSM_APPLECSPDL_DB_CHANGE_PASSWORD_PARAMETERS *>(inInputParams);
			db->changePassphrase(params->accessCredentials);
			break;
		}
		case CSSM_APPLECSPDL_DB_GET_HANDLE:
		{
			using SecurityServer::DbHandle;
			Required(outOutputParams, CSSM_ERRCODE_INVALID_OUTPUT_POINTER);
			DbHandle &dbHandle = *(DbHandle *)outOutputParams;
			dbHandle = db->dbHandle();
			break;
		}
		default:
		{
			CSSM_RETURN result = CSSM_DL_PassThrough(db->handle(), inPassThroughId, inInputParams, outOutputParams);
			if (result)
				CssmError::throwMe(result);
			break;
		}
	}
}

CSSM_DB_HANDLE
SSDLSession::makeDbHandle(SSDatabase &inDb)
{
	StLock<Mutex> _(mDbHandleLock);
	CSSM_DB_HANDLE aDbHandle = inDb->handle().DBHandle;
	IFDEBUG(bool inserted =) mDbHandleMap.insert(DbHandleMap::value_type(aDbHandle, inDb)).second;
	assert(inserted);
	return aDbHandle;
}

SSDatabase
SSDLSession::killDbHandle(CSSM_DB_HANDLE inDbHandle)
{
	StLock<Mutex> _(mDbHandleLock);
	DbHandleMap::iterator it = mDbHandleMap.find(inDbHandle);
	if (it == mDbHandleMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_DB_HANDLE);

	SSDatabase db = it->second;
	mDbHandleMap.erase(it);
	return db;
}

SSDatabase
SSDLSession::findDbHandle(CSSM_DB_HANDLE inDbHandle)
{
	StLock<Mutex> _(mDbHandleLock);
	DbHandleMap::iterator it = mDbHandleMap.find(inDbHandle);
	if (it == mDbHandleMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_DB_HANDLE);

	return it->second;
}

CSSM_DB_UNIQUE_RECORD_PTR
SSDLSession::makeSSUniqueRecord(SSUniqueRecord &uniqueId)
{
	StLock<Mutex> _(mSSUniqueRecordLock);
	CSSM_HANDLE ref = CSSM_HANDLE(static_cast<CSSM_DB_UNIQUE_RECORD *>(uniqueId));
	IFDEBUG(bool inserted =) mSSUniqueRecordMap.insert(SSUniqueRecordMap::value_type(ref, uniqueId)).second;
	assert(inserted);
	return createUniqueRecord(ref);
}

SSUniqueRecord
SSDLSession::killSSUniqueRecord(CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
	CSSM_HANDLE ref = parseUniqueRecord(inUniqueRecord);
	StLock<Mutex> _(mSSUniqueRecordLock);
	SSUniqueRecordMap::iterator it = mSSUniqueRecordMap.find(ref);
	if (it == mSSUniqueRecordMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);

	SSUniqueRecord uniqueRecord = it->second;
	mSSUniqueRecordMap.erase(it);
	freeUniqueRecord(inUniqueRecord);
	return uniqueRecord;
}

SSUniqueRecord
SSDLSession::findSSUniqueRecord(const CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
	CSSM_HANDLE ref = parseUniqueRecord(inUniqueRecord);
	StLock<Mutex> _(mSSUniqueRecordLock);
	SSUniqueRecordMap::iterator it = mSSUniqueRecordMap.find(ref);
	if (it == mSSUniqueRecordMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);

	return it->second;
}

CSSM_DB_UNIQUE_RECORD_PTR
SSDLSession::createUniqueRecord(CSSM_HANDLE ref)
{
	CSSM_DB_UNIQUE_RECORD *aUniqueRecord = allocator().alloc<CSSM_DB_UNIQUE_RECORD>();
	memset(aUniqueRecord, 0, sizeof(CSSM_DB_UNIQUE_RECORD));
	aUniqueRecord->RecordIdentifier.Length = sizeof(CSSM_HANDLE);
	try
	{
		aUniqueRecord->RecordIdentifier.Data = allocator().alloc<uint8>(sizeof(CSSM_HANDLE));
		*reinterpret_cast<CSSM_HANDLE *>(aUniqueRecord->RecordIdentifier.Data) = ref;
	}
	catch(...)
	{
		free(aUniqueRecord);
		throw;
	}

	return aUniqueRecord;
}

CSSM_HANDLE
SSDLSession::parseUniqueRecord(const CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
	if (inUniqueRecord.RecordIdentifier.Length != sizeof(CSSM_HANDLE))
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);

	return *reinterpret_cast<CSSM_HANDLE *>(inUniqueRecord.RecordIdentifier.Data);
}

void
SSDLSession::freeUniqueRecord(CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
	if (inUniqueRecord.RecordIdentifier.Length != 0
		&& inUniqueRecord.RecordIdentifier.Data != NULL)
	{
		inUniqueRecord.RecordIdentifier.Length = 0;
		allocator().free(inUniqueRecord.RecordIdentifier.Data);
	}
	allocator().free(&inUniqueRecord);
}
