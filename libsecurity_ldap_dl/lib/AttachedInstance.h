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



#ifndef __ATTACHED_INSTANCE_H__
#define __ATTACHED_INSTANCE_H__



#include <Security/Security.h>
#include <map>
#include "Mutex.h"


class Database;

typedef std::map<CSSM_MODULE_HANDLE, Database*> DatabaseMap;

/*
	class which provides the main entry points for a DL
*/

class AttachedInstance
{
protected:
	static CSSM_PROC_ADDR gServiceFunctions[];
	static CSSM_MODULE_FUNCS gFunctionTable;

	static Database* GetDatabaseFromDLDBHandle (const CSSM_DL_DB_HANDLE &dldbHandle);

	static CSSM_RETURN StubDbOpen (CSSM_DL_HANDLE dlHandle,
								   const char* DbName,
								   const CSSM_NET_ADDRESS *dbLocation,
								   const CSSM_DB_ACCESS_TYPE accessRequest,
								   const CSSM_ACCESS_CREDENTIALS *accessCredentials,
								   const void* openParameters,
								   CSSM_DB_HANDLE *dbHandle);

	static CSSM_RETURN StubDbCreate (CSSM_DL_HANDLE dlHandle,
									 const char* dbName,
									 const CSSM_NET_ADDRESS *dbLocation,
									 const CSSM_DBINFO *dbInfo,
									 const CSSM_DB_ACCESS_TYPE accessRequest,
									 const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry,
									 const void *openParameters,
									 CSSM_DB_HANDLE *dbHandle);

	static CSSM_RETURN StubDbDelete (CSSM_DL_HANDLE dlHandle,
									 const char* dbName,
									 const CSSM_NET_ADDRESS *dbLocation,
									 const CSSM_ACCESS_CREDENTIALS *accessCredentials);
	
	static CSSM_RETURN StubDbClose (CSSM_DL_DB_HANDLE dldbHandle);
	
	static CSSM_RETURN StubCreateRelation (CSSM_DL_DB_HANDLE dldbHandle,
										   CSSM_DB_RECORDTYPE relationID,
										   const char* relationName,
										   uint32 numberOfAttributes,
										   const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *pAttributeInfo,
										   uint32 numberOfIndexes,
										   const CSSM_DB_SCHEMA_INDEX_INFO *pIndexInfo);
	
	static CSSM_RETURN StubDestroyRelation (CSSM_DL_DB_HANDLE dldbHandle,
											CSSM_DB_RECORDTYPE relationID);

	static CSSM_RETURN StubAuthenticate (CSSM_DL_DB_HANDLE dldbHandle,
										 CSSM_DB_ACCESS_TYPE accessRequest,
										 const CSSM_ACCESS_CREDENTIALS *accessCred);

	static CSSM_RETURN StubGetDbAcl (CSSM_DL_DB_HANDLE dldbHandle,
									 CSSM_STRING* selectionTag,
									 uint32 *numberOfAclInfos);
	
	static CSSM_RETURN StubChangeDbAcl (CSSM_DL_DB_HANDLE dldbHandle,
									    const CSSM_ACCESS_CREDENTIALS *accessCred,
									    const CSSM_ACL_EDIT *aclEdit);

	static CSSM_RETURN StubGetDbOwner (CSSM_DL_DB_HANDLE dldbHandle,
									   CSSM_ACL_OWNER_PROTOTYPE_PTR owner);
	
	static CSSM_RETURN StubChangeDbOwner (CSSM_DL_DB_HANDLE dldbHandle,
										  const CSSM_ACCESS_CREDENTIALS *accessCred,
										  const CSSM_ACL_OWNER_PROTOTYPE *newOwner);
	
	static CSSM_RETURN StubGetDbNames (CSSM_DL_HANDLE dlHandle,
									   CSSM_NAME_LIST_PTR nameList);

	static CSSM_RETURN StubGetDbNameFromHandle (CSSM_DL_DB_HANDLE dldbHandle,
											    char** dbName);
	
	static CSSM_RETURN StubFreeNameList (CSSM_DL_HANDLE dlHandle,
										 CSSM_NAME_LIST_PTR nameList);

	static CSSM_RETURN StubDataInsert (CSSM_DL_DB_HANDLE dldbHandle,
									   CSSM_DB_RECORDTYPE recordType,
									   const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
									   const CSSM_DATA *data,
									   CSSM_DB_UNIQUE_RECORD_PTR *uniqueId);
	
	static CSSM_RETURN StubDataDelete (CSSM_DL_DB_HANDLE dldbHandle,
									   const CSSM_DB_UNIQUE_RECORD *uniqueRecordIdentifier);

	static CSSM_RETURN StubDataModify (CSSM_DL_DB_HANDLE dldbHandle,
									   CSSM_DB_RECORDTYPE recordType,
									   CSSM_DB_UNIQUE_RECORD_PTR uniqueRecordIdentifier,
									   const CSSM_DB_RECORD_ATTRIBUTE_DATA attributesToBeModified,
									   const CSSM_DATA *dataToBeModified,
									   CSSM_DB_MODIFY_MODE modifyMode);

	static CSSM_RETURN StubDataGetFirst (CSSM_DL_DB_HANDLE dldbHandle,
										 const CSSM_QUERY *query,
										 CSSM_HANDLE_PTR resultsHandle,
										 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
										 CSSM_DATA_PTR data,
										 CSSM_DB_UNIQUE_RECORD_PTR *uniqueID);
	
	static CSSM_RETURN StubDataGetNext (CSSM_DL_DB_HANDLE dldbHandle,
									    CSSM_HANDLE resultsHandle,
									    CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
									    CSSM_DATA_PTR data,
									    CSSM_DB_UNIQUE_RECORD_PTR *uniqueID);

	static CSSM_RETURN StubDataAbortQuery (CSSM_DL_DB_HANDLE dldbHandle,
										   CSSM_HANDLE resultsHandle);

	static CSSM_RETURN StubDataGetFromUniqueRecordID (CSSM_DL_DB_HANDLE dldbHandle,
													  const CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord,
													  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
													  CSSM_DATA_PTR data);
	
	static CSSM_RETURN StubFreeUniqueRecord (CSSM_DL_DB_HANDLE dldbHandle,
											 CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord);

	static CSSM_RETURN StubPassThrough (CSSM_DL_DB_HANDLE dldbHandle,
									    uint32 passThroughID,
									    const void* inputParams,
									    void **outputParams);
protected:
	uint32 mNextDatabaseHandle;							// next database handle
	CSSM_MODULE_HANDLE mModuleHandle;					// handle for our module
	CSSM_UPCALLS mUpcalls;								// callback table for memory management
	DatabaseMap mDatabaseMap;							// map database to instances
	DynamicMutex mDatabaseMapMutex;						// make sure we don't do nasty stuff to the database map when multi-threaded
	
public:
	AttachedInstance () {}
	virtual ~AttachedInstance ();

	void* malloc (uint32 size);							// malloc using the user's allocator
	void free (void* memblock);							// dealloc using the user's allocator
	void* realloc (void* memblock, uint32 size);		// realloc using the user's allocator
	void* calloc (uint32 num, uint32 size);				// calloc using the user's allocator

	virtual Database* MakeDatabaseObject () = 0;
	CSSM_MODULE_HANDLE RegisterDatabase (Database* d);	// save a newly created database so that we can find it again
	Database* LookupDatabase (CSSM_MODULE_HANDLE mh);	// find a database
	void DeregisterDatabase (CSSM_DB_HANDLE d);			// remove from map

	void SetUpcalls (CSSM_MODULE_HANDLE moduleHandle, const CSSM_UPCALLS *upcalls); // save off the user's upcalls
	
	virtual void Initialize (const CSSM_GUID *ModuleGuid,
							 const CSSM_VERSION *Version,
							 uint32 SubserviceID,
							 CSSM_SERVICE_TYPE SubserviceType,
							 CSSM_ATTACH_FLAGS AttachFlags,
							 CSSM_KEY_HIERARCHY KeyHierarchy,
							 const CSSM_GUID *CssmGuid,
							 const CSSM_GUID *ModuleManagerGuid,
							 const CSSM_GUID *CallerGuid);
	


	static CSSM_MODULE_FUNCS_PTR gFunctionTablePtr;
};



#endif
