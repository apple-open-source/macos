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

#ifndef __LDAP_DL_MODULE_H__
#define __LDAP_DL_MODULE_H__



#include "Database.h"
#include "DataStorageLibrary.h"
#include "AttachedInstance.h"
#include "TableRelation.h"
#include "DSX509Relation.h"

#include <map>

/*
	classes to implement the open directory DL
*/

// implements the functionality for the open directory DL
class LDAPDLModule : public DataStorageLibrary
{
protected:
	static TableRelation *mSchemaRelationRelation,
						 *mSchemaAttributeRelation,
						 *mSchemaIndexRelation,
						 *mSchemaParsingModuleRelation;		// the "housekeeping" relations we support
	static DSX509Relation *mX509Relation;					// the open directory relation

	static RelationMap *mRelationMap;						// a map which enables efficient lookup of relations

	// initialization functions
	static void SetupSchemaRelationRelation ();
	static void SetupSchemaAttributeRelation ();
	static void SetupSchemaIndexRelation ();
	static void SetupSchemaParsingModuleRelation ();
	static void SetupX509Relation ();
	static void InitializeRelations ();

public:
	LDAPDLModule (pthread_mutex_t *globalLock, CSSM_SPI_ModuleEventHandler CssmNotifyCallback, void* CssmNotifyCallbackCtx);
	~LDAPDLModule ();
	
	AttachedInstance* MakeAttachedInstance ();				// make an instance of this DL
	static Relation* LookupRelation (CSSM_DB_RECORDTYPE relationID); // find a relation for this DL
};



// holds an instance of the LDAPDatabase
class LDAPAttachedInstance : public AttachedInstance
{
public:
	Database* MakeDatabaseObject ();
};


// allows efficient lookup of queries
typedef std::map<CSSM_HANDLE, Query*> QueryMap;



// the open directory database instance
class LDAPDatabase : public Database
{
protected:
	std::string mDatabaseName;								// name of the database
	QueryMap mQueryMap;										// map of ongoing queries
	CSSM_HANDLE mNextHandle;								// next handle id that we will hand out

	void CopyAttributes (Relation* r, Tuple *t, CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes); // copy attributes into a realtion
	void ExportUniqueID (UniqueIdentifier *id, CSSM_DB_UNIQUE_RECORD_PTR *uniqueID); // export a unique ID
	void GetDataFromTuple (Tuple *t, CSSM_DATA &data);
	void processNext(CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,CSSM_DATA_PTR data,CSSM_DB_UNIQUE_RECORD_PTR *uniqueID, Query *q, Relation *r);

public:
	LDAPDatabase (AttachedInstance *ai);
	~LDAPDatabase ();

	// standard calls -- see the CDSA documentation for more info
	
	virtual void DbOpen (const char* DbName,
						 const CSSM_NET_ADDRESS *dbLocation,
						 const CSSM_DB_ACCESS_TYPE accessRequest,
						 const CSSM_ACCESS_CREDENTIALS *accessCredentials,
						 const void* openParameters);
	virtual void DbClose ();

	virtual void DbGetDbNameFromHandle (char** dbName);

	virtual CSSM_HANDLE DbDataGetFirst (const CSSM_QUERY *query,
									    CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
									    CSSM_DATA_PTR data,
									    CSSM_DB_UNIQUE_RECORD_PTR *uniqueID);
	
	virtual bool DbDataGetNext (CSSM_HANDLE resultsHandle,
							    CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
							    CSSM_DATA_PTR data,
							    CSSM_DB_UNIQUE_RECORD_PTR *uniqueID);
	
	virtual void DbDataAbortQuery (CSSM_HANDLE resultsHandle);
	
	virtual void DbFreeUniqueRecord (CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord);

	virtual void DbDataGetFromUniqueRecordID (const CSSM_DB_UNIQUE_RECORD_PTR uniqueRecord,
											  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR attributes,
											  CSSM_DATA_PTR data);
};



#endif
