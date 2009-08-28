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



#ifndef __DATABASE_H__
#define __DATABASE_H__


#include "AttachedInstance.h"

/*
	Abstract base class for a database -- provides stubs for CSSM calls
*/



class Database
{
protected:
	AttachedInstance *mAttachedInstance;

public:
	Database (AttachedInstance *ai);
	virtual ~Database ();
	
	virtual void DbOpen (const char* DbName,
						 const CSSM_NET_ADDRESS *dbLocation,
						 const CSSM_DB_ACCESS_TYPE accessRequest,
						 const CSSM_ACCESS_CREDENTIALS *accessCredentials,
						 const void* openParameters);
	
	virtual void DbClose ();
	
	virtual void DbCreate (const char* dbName,
						   const CSSM_NET_ADDRESS *dbLocation,
						   const CSSM_DBINFO *dbInfo,
						   const CSSM_DB_ACCESS_TYPE accessRequest,
						   const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry,
						   const void *openParameters);

	virtual void DbCreateRelation (CSSM_DB_RECORDTYPE relationID,
								   const char* relationName,
								   uint32 numberOfAttributes,
								   const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *pAttributeInfo,
								   uint32 numberOfIndexes,
								   const CSSM_DB_SCHEMA_INDEX_INFO *pIndexInfo);
	
	virtual void DbDestroyRelation (CSSM_DB_RECORDTYPE relationID);

	virtual void DbAuthenticate (CSSM_DB_ACCESS_TYPE accessRequest,
								 const CSSM_ACCESS_CREDENTIALS *accessCred);
	
	virtual void DbGetDbAcl (CSSM_STRING* selectionTag,
							 uint32 *numberOfAclInfos);

	virtual void DbChangeDbAcl (const CSSM_ACCESS_CREDENTIALS *accessCred,
								const CSSM_ACL_EDIT *aclEdit);
	
	virtual void DbGetDbOwner (CSSM_ACL_OWNER_PROTOTYPE_PTR owner);
	
	virtual void DbChangeDbOwner (const CSSM_ACCESS_CREDENTIALS *accessCred,
								  const CSSM_ACL_OWNER_PROTOTYPE *newOwner);
	
	virtual void DbGetDbNameFromHandle (char** dbName);
	
	virtual void DbDataInsert (CSSM_DB_RECORDTYPE recordType,
							   const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
							   const CSSM_DATA *data,
							   CSSM_DB_UNIQUE_RECORD_PTR *uniqueId);
	
	virtual void DbDataDelete (const CSSM_DB_UNIQUE_RECORD *uniqueRecordIdentifier);
	
	virtual void DbDataModify (CSSM_DB_RECORDTYPE recordType,
							   CSSM_DB_UNIQUE_RECORD_PTR uniqueRecordIdentifier,
							   const CSSM_DB_RECORD_ATTRIBUTE_DATA attributesToBeModified,
							   const CSSM_DATA *dataToBeModified,
							   CSSM_DB_MODIFY_MODE modifyMode);
	
	virtual CSSM_HANDLE DbDataGetFirst (const CSSM_QUERY *query,
									    CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
									    CSSM_DATA_PTR data,
									    CSSM_DB_UNIQUE_RECORD_PTR *uniqueID);
	
	virtual bool DbDataGetNext (CSSM_HANDLE resultsHandle,
							    CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
							    CSSM_DATA_PTR data,
							    CSSM_DB_UNIQUE_RECORD_PTR *uniqueID);
	
	virtual void DbDataAbortQuery (CSSM_HANDLE resultsHandle);
	
	virtual void DbDataGetFromUniqueRecordID (const CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord,
											  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
											  CSSM_DATA_PTR data);
	
	virtual void DbFreeUniqueRecord (CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord);
	
	virtual void DbPassThrough (uint32 passThroughID,
							    const void* inputParams,
							    void **outputParams);
};

#endif
