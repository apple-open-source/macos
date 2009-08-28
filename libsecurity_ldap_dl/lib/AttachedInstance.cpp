/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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



#include "AttachedInstance.h"
#include "DataStorageLibrary.h"
#include "Database.h"
#include "CommonCode.h"
#include <security_utilities/debugging.h>


#define BEGIN_EXCEPTION_BLOCK try{

#define END_EXCEPTION_BLOCK }					\
	catch (CSSMError &error)						\
	{											\
		return error.GetCode ();				\
	}											\
	catch (...)									\
	{											\
		return CSSMERR_CSSM_INTERNAL_ERROR;		\
	}											\
	return 0;
	
	

Database* AttachedInstance::GetDatabaseFromDLDBHandle (const CSSM_DL_DB_HANDLE &dldbHandle)
{
	AttachedInstance* ai = DataStorageLibrary::gDL->HandleToInstance (dldbHandle.DLHandle);
	Database* d = ai->LookupDatabase (dldbHandle.DBHandle);
	return d;
}


/****************************************************************************/

CSSM_RETURN AttachedInstance::StubDbOpen (CSSM_DL_HANDLE dlHandle,
										  const char* dbName,
										  const CSSM_NET_ADDRESS *dbLocation,
										  const CSSM_DB_ACCESS_TYPE accessRequest,
										  const CSSM_ACCESS_CREDENTIALS *accessCredentials,
										  const void* openParameters,
										  CSSM_DB_HANDLE *dbHandle)
{
	BEGIN_EXCEPTION_BLOCK
	
	// get the instance for the database
	AttachedInstance* ai = DataStorageLibrary::gDL->HandleToInstance (dlHandle);
	
	// ask that instance for a database instance
	Database *db = ai->MakeDatabaseObject ();
	
	// register the database instance
	*dbHandle = ai->RegisterDatabase (db);
	
	db->DbOpen (dbName, dbLocation, accessRequest, accessCredentials, openParameters);

	END_EXCEPTION_BLOCK
}



CSSM_RETURN AttachedInstance::StubDbClose (CSSM_DL_DB_HANDLE dldbHandle)
{
	BEGIN_EXCEPTION_BLOCK
	
	// get the instance for the database
	// get the instance for the database
	AttachedInstance* ai = DataStorageLibrary::gDL->HandleToInstance (dldbHandle.DLHandle);
	
	// ask that instance for a database instance
	Database *db = ai->MakeDatabaseObject ();
	db->DbClose ();
	ai->DeregisterDatabase (dldbHandle.DBHandle);
	delete db;

	END_EXCEPTION_BLOCK
}



CSSM_RETURN AttachedInstance::StubGetDbNameFromHandle (CSSM_DL_DB_HANDLE dldbHandle,
													   char** dbName)
{
	BEGIN_EXCEPTION_BLOCK
	
	Database *d = GetDatabaseFromDLDBHandle (dldbHandle);
	d->DbGetDbNameFromHandle (dbName);
	
	END_EXCEPTION_BLOCK
}



CSSM_RETURN AttachedInstance::StubDataGetFirst (CSSM_DL_DB_HANDLE dldbHandle,
												const CSSM_QUERY *query,
												CSSM_HANDLE_PTR resultsHandle,
												CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
												CSSM_DATA_PTR data,
												CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	BEGIN_EXCEPTION_BLOCK
	
	Database *d = GetDatabaseFromDLDBHandle (dldbHandle);
	*resultsHandle = d->DbDataGetFirst (query, attributes, data, uniqueID);
	
	END_EXCEPTION_BLOCK
}



CSSM_RETURN AttachedInstance::StubDataGetNext (CSSM_DL_DB_HANDLE dldbHandle,
											   CSSM_HANDLE resultsHandle,
											   CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
											   CSSM_DATA_PTR data,
											   CSSM_DB_UNIQUE_RECORD_PTR *uniqueID)
{
	BEGIN_EXCEPTION_BLOCK
	
	Database *d = GetDatabaseFromDLDBHandle (dldbHandle);
	d->DbDataGetNext (resultsHandle, attributes, data, uniqueID);
	
	END_EXCEPTION_BLOCK
}



CSSM_RETURN AttachedInstance::StubDataAbortQuery (CSSM_DL_DB_HANDLE dldbHandle,
												  CSSM_HANDLE resultsHandle)
{
	BEGIN_EXCEPTION_BLOCK
	
	Database *d = GetDatabaseFromDLDBHandle (dldbHandle);
	d->DbDataAbortQuery (resultsHandle);
	
	END_EXCEPTION_BLOCK
}



CSSM_RETURN AttachedInstance::StubDataGetFromUniqueRecordID (CSSM_DL_DB_HANDLE dldbHandle,
															 const CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord,
															 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
															 CSSM_DATA_PTR data)
{
	BEGIN_EXCEPTION_BLOCK
	
	Database *d = GetDatabaseFromDLDBHandle (dldbHandle);
	d->DbDataGetFromUniqueRecordID (uniqueRecord, attributes, data);
	
	END_EXCEPTION_BLOCK
}



CSSM_RETURN AttachedInstance::StubFreeUniqueRecord (CSSM_DL_DB_HANDLE dldbHandle,
													CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord)
{
	BEGIN_EXCEPTION_BLOCK
	
	Database *d = GetDatabaseFromDLDBHandle (dldbHandle);
	d->DbFreeUniqueRecord (uniqueRecord);
	
	END_EXCEPTION_BLOCK
}


CSSM_RETURN AttachedInstance::StubDbCreate (CSSM_DL_HANDLE dlHandle,
										   const char* dbName,
										   const CSSM_NET_ADDRESS *dbLocation,
										   const CSSM_DBINFO *dbInfo,
										   const CSSM_DB_ACCESS_TYPE accessRequest,
										   const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry,
										   const void *openParameters,
										   CSSM_DB_HANDLE *dbHandle)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubDbDelete (CSSM_DL_HANDLE dlHandle,
											const char* dbName,
											const CSSM_NET_ADDRESS *dbLocation,
											const CSSM_ACCESS_CREDENTIALS *accessCredentials)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubCreateRelation (CSSM_DL_DB_HANDLE dldbHandle,
												  CSSM_DB_RECORDTYPE relationID,
												  const char* relationName,
												  uint32 numberOfAttributes,
												  const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *pAttributeInfo,
												  uint32 numberOfIndexes,
												  const CSSM_DB_SCHEMA_INDEX_INFO *pIndexInfo)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubDestroyRelation (CSSM_DL_DB_HANDLE dldbHandle,
												   CSSM_DB_RECORDTYPE relationID)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubAuthenticate (CSSM_DL_DB_HANDLE dldbHandle,
												CSSM_DB_ACCESS_TYPE accessRequest,
												const CSSM_ACCESS_CREDENTIALS *accessCred)
{
	return CSSM_OK;  // Not implemented for this module.
}



CSSM_RETURN AttachedInstance::StubGetDbAcl (CSSM_DL_DB_HANDLE dldbHandle,
											CSSM_STRING* selectionTag,
											uint32 *numberOfAclInfos)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubChangeDbAcl (CSSM_DL_DB_HANDLE dldbHandle,
											   const CSSM_ACCESS_CREDENTIALS *accessCred,
											   const CSSM_ACL_EDIT *aclEdit)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubGetDbOwner (CSSM_DL_DB_HANDLE dldbHandle,
											  CSSM_ACL_OWNER_PROTOTYPE_PTR owner)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubChangeDbOwner (CSSM_DL_DB_HANDLE dldbHandle,
												 const CSSM_ACCESS_CREDENTIALS *accessCred,
												 const CSSM_ACL_OWNER_PROTOTYPE *newOwner)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubGetDbNames (CSSM_DL_HANDLE dlHandle,
											  CSSM_NAME_LIST_PTR nameList)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}





CSSM_RETURN AttachedInstance::StubFreeNameList (CSSM_DL_HANDLE dlHandle,
												CSSM_NAME_LIST_PTR nameList)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubDataInsert (CSSM_DL_DB_HANDLE dldbHandle,
											  CSSM_DB_RECORDTYPE recordType,
											  const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
											  const CSSM_DATA *data,
											  CSSM_DB_UNIQUE_RECORD_PTR *uniqueId)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubDataDelete (CSSM_DL_DB_HANDLE dldbHandle,
											  const CSSM_DB_UNIQUE_RECORD *uniqueRecordIdentifier)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_RETURN AttachedInstance::StubDataModify (CSSM_DL_DB_HANDLE dldbHandle,
											  CSSM_DB_RECORDTYPE recordType,
											  CSSM_DB_UNIQUE_RECORD_PTR uniqueRecordIdentifier,
											  const CSSM_DB_RECORD_ATTRIBUTE_DATA attributesToBeModified,
											  const CSSM_DATA *dataToBeModified,
											  CSSM_DB_MODIFY_MODE modifyMode)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}




CSSM_RETURN AttachedInstance::StubPassThrough (CSSM_DL_DB_HANDLE dldbHandle,
											   uint32 passThroughID,
											   const void* inputParams,
											   void **outputParams)
{
	return CSSMERR_DL_FUNCTION_NOT_IMPLEMENTED;
}



CSSM_PROC_ADDR AttachedInstance::gServiceFunctions[] =
{
	(CSSM_PROC_ADDR) StubDbOpen,
	(CSSM_PROC_ADDR) StubDbClose,
	(CSSM_PROC_ADDR) StubDbCreate,
	(CSSM_PROC_ADDR) StubDbDelete,
	(CSSM_PROC_ADDR) StubCreateRelation,
	(CSSM_PROC_ADDR) StubDestroyRelation,
	(CSSM_PROC_ADDR) StubAuthenticate,
	(CSSM_PROC_ADDR) StubGetDbAcl,
	(CSSM_PROC_ADDR) StubChangeDbAcl,
	(CSSM_PROC_ADDR) StubGetDbOwner,
	(CSSM_PROC_ADDR) StubChangeDbOwner,
	(CSSM_PROC_ADDR) StubGetDbNames,
	(CSSM_PROC_ADDR) StubGetDbNameFromHandle,
	(CSSM_PROC_ADDR) StubFreeNameList,
	(CSSM_PROC_ADDR) StubDataInsert,
	(CSSM_PROC_ADDR) StubDataDelete,
	(CSSM_PROC_ADDR) StubDataModify,
	(CSSM_PROC_ADDR) StubDataGetFirst,
	(CSSM_PROC_ADDR) StubDataGetNext,
	(CSSM_PROC_ADDR) StubDataAbortQuery,
	(CSSM_PROC_ADDR) StubDataGetFromUniqueRecordID,
	(CSSM_PROC_ADDR) StubFreeUniqueRecord,
	(CSSM_PROC_ADDR) StubPassThrough
};



CSSM_MODULE_FUNCS AttachedInstance::gFunctionTable =
{
	CSSM_SERVICE_DL,
	sizeof (gServiceFunctions) / sizeof (CSSM_PROC_ADDR),
	gServiceFunctions
};



CSSM_MODULE_FUNCS_PTR AttachedInstance::gFunctionTablePtr = &gFunctionTable;



void AttachedInstance::SetUpcalls (CSSM_MODULE_HANDLE moduleHandle, const CSSM_UPCALLS *upcalls)
{
	mUpcalls = *upcalls;
	mModuleHandle = moduleHandle;
}



AttachedInstance::~AttachedInstance ()
{
}



void* AttachedInstance::malloc (uint32 size)
{
	return mUpcalls.malloc_func (mModuleHandle, size);
}



void AttachedInstance::free (void* ptr)
{
	return mUpcalls.free_func (mModuleHandle, ptr);
}



void* AttachedInstance::realloc (void* memblock, uint32 size)
{
	return mUpcalls.realloc_func (mModuleHandle, memblock, size);
}



void* AttachedInstance::calloc (uint32 num, uint32 size)
{
	return mUpcalls.calloc_func (mModuleHandle, num, size);
}



void AttachedInstance::Initialize (const CSSM_GUID *ModuleGuid,
								   const CSSM_VERSION *Version,
								   uint32 SubserviceID,
								   CSSM_SERVICE_TYPE SubserviceType,
								   CSSM_ATTACH_FLAGS AttachFlags,
								   CSSM_KEY_HIERARCHY KeyHierarchy,
								   const CSSM_GUID *CssmGuid,
								   const CSSM_GUID *ModuleManagerGuid,
								   const CSSM_GUID *CallerGuid)
{
}



CSSM_MODULE_HANDLE AttachedInstance::RegisterDatabase (Database* d)
{
	MutexLocker _ml (mDatabaseMapMutex);
	
	CSSM_MODULE_HANDLE nextHandle = ++mNextDatabaseHandle;
	mDatabaseMap[nextHandle] = d;
	return nextHandle;
}



Database* AttachedInstance::LookupDatabase (CSSM_MODULE_HANDLE mh)
{
	MutexLocker _ml (mDatabaseMapMutex);
	return mDatabaseMap[mh];
}



void AttachedInstance::DeregisterDatabase (CSSM_DB_HANDLE d)
{
	mDatabaseMap.erase (d);
}


