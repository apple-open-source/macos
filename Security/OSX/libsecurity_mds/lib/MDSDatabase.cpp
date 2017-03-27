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


#include "MDSDatabase.h"
#include "MDSSchema.h"

#include <security_cdsa_plugin/DbContext.h>
#include <Security/mds_schema.h>
#include <Security/cssmerr.h>

//
// MDSDatabaseManager
//

Database *MDSDatabaseManager::make (const DbName &inDbName)
{
    return new MDSDatabase(inDbName, mTableNames);
}

//
// MDSDatabase
//

MDSDatabase::MDSDatabase (const DbName &inDbName, const AppleDatabaseTableName *tableNames)
: AppleDatabase(inDbName, tableNames)
{
}

MDSDatabase::~MDSDatabase ()
{
}

DbContext *
MDSDatabase::makeDbContext (DatabaseSession &inDatabaseSession,
                            CSSM_DB_ACCESS_TYPE inAccessRequest,
                            const AccessCredentials *inAccessCred,
                            const void *inOpenParameters)
{
    return new DbContext (*this, inDatabaseSession, inAccessRequest,
                          inAccessCred);
}

void
MDSDatabase::dbOpen (DbContext &inDbContext)
{
    // XXX Do something more...
}

void
MDSDatabase::dbClose ()
{
    // XXX Do something more...
}

// Creating and destroying relations is not exposed as part of the
// MDS interface, so these two methods should never be called.

void
MDSDatabase::createRelation(DbContext &dbContext,
	CSSM_DB_RECORDTYPE inRelationID,
	const char *inRelationName,
	uint32 inNumberOfAttributes,
	const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo,
	uint32 inNumberOfIndexes,
	const CSSM_DB_SCHEMA_INDEX_INFO &inIndexInfo)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::destroyRelation(DbContext &dbContext,
	CSSM_DB_RECORDTYPE inRelationID)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

char *
MDSDatabase::getDbNameFromHandle (const DbContext &dbContext) const
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

CSSM_DB_UNIQUE_RECORD_PTR
MDSDatabase::dataInsert (DbContext &dbContext,
                         CSSM_DB_RECORDTYPE RecordType,
                         const CSSM_DB_RECORD_ATTRIBUTE_DATA *Attributes,
                         const CssmData *Data)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::dataDelete (DbContext &dbContext,
                         const CSSM_DB_UNIQUE_RECORD &UniqueRecordIdentifier)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::dataModify (DbContext &dbContext,
                         CSSM_DB_RECORDTYPE RecordType,
                         CSSM_DB_UNIQUE_RECORD &UniqueRecordIdentifier,
                         const CSSM_DB_RECORD_ATTRIBUTE_DATA *AttributesToBeModified,
                         const CssmData *DataToBeModified,
                         CSSM_DB_MODIFY_MODE ModifyMode)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

CSSM_HANDLE
MDSDatabase::dataGetFirst (DbContext &dbContext,
                           const CssmQuery *Query,
                           CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR Attributes,
                           CssmData *Data,
                           CSSM_DB_UNIQUE_RECORD_PTR &UniqueRecordIdentifier)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

bool
MDSDatabase::dataGetNext (DbContext &dbContext,
                          CSSM_HANDLE ResultsHandle,
                          CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR Attributes,
                          CssmData *Data,
                          CSSM_DB_UNIQUE_RECORD_PTR &UniqueRecordIdentifier)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::dataAbortQuery (DbContext &dbContext,
                             CSSM_HANDLE ResultsHandle)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::dataGetFromUniqueRecordId (DbContext &dbContext,
                                        const CSSM_DB_UNIQUE_RECORD &UniqueRecord,
                                        CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR Attributes,
                                        CssmData *Data)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::freeUniqueRecord (DbContext &dbContext,
                               CSSM_DB_UNIQUE_RECORD &UniqueRecord)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

// Functions that MDS does not support but we must implement to satisfy Database.
void
MDSDatabase::dbCreate (DbContext &inDbContext, const CSSM_DBINFO &inDBInfo,
                       const CSSM_ACL_ENTRY_INPUT *inInitialAclEntry)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::dbDelete (DatabaseSession &inDatabaseSession,
                       const AccessCredentials *inAccessCred)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::authenticate(DbContext &dbContext,
                          CSSM_DB_ACCESS_TYPE inAccessRequest,
                          const AccessCredentials &inAccessCred)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::getDbAcl(DbContext &dbContext,
                      const CSSM_STRING *inSelectionTag,
                      uint32 &outNumberOfAclInfos,
                      CSSM_ACL_ENTRY_INFO_PTR &outAclInfos)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::changeDbAcl(DbContext &dbContext,
                         const AccessCredentials &inAccessCred,
                         const CSSM_ACL_EDIT &inAclEdit)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::getDbOwner(DbContext &dbContext, CSSM_ACL_OWNER_PROTOTYPE &outOwner)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
MDSDatabase::changeDbOwner(DbContext &dbContext,
                           const AccessCredentials &inAccessCred,
                           const CSSM_ACL_OWNER_PROTOTYPE &inNewOwner)
{
    CssmError ::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}
