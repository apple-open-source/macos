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
// DatabaseSession.cpp - DL Session.
//
#ifdef __MWERKS__
#define _CPP_DATABASESESSION
#endif
#include <Security/DatabaseSession.h>

#include <Security/Database.h>
#include <Security/DbContext.h>
#include <memory>
#include <Security/debugging.h>

/* log open/close events */
#define DOCDebug(args...)	debug("DBOpen", ## args)


using namespace std;

//
// Session constructor
//
DatabaseSession::DatabaseSession(DatabaseManager &inDatabaseManager)
: mDatabaseManager(inDatabaseManager)
{
}

DatabaseSession::~DatabaseSession()
{
}


// Utility functions
void
DatabaseSession::GetDbNames(CSSM_NAME_LIST_PTR &outNameList)
{
	outNameList = mDatabaseManager.getDbNames (*this);
}


void
DatabaseSession::FreeNameList(CSSM_NAME_LIST &inNameList)
{
	mDatabaseManager.freeNameList (*this, inNameList);
}


void
DatabaseSession::DbDelete(const char *inDbName,
                          const CSSM_NET_ADDRESS *inDbLocation,
                          const AccessCredentials *inAccessCred)
{
    // The databaseManager will notify all its DbContext instances
    // that the database is question is being deleted.
    mDatabaseManager.dbDelete(*this, DbName(inDbName, CssmNetAddress::optional(inDbLocation)), inAccessCred);
}

// DbContext creation and destruction.
void
DatabaseSession::DbCreate(const char *inDbName,
                          const CSSM_NET_ADDRESS *inDbLocation,
                          const CSSM_DBINFO &inDBInfo,
                          CSSM_DB_ACCESS_TYPE inAccessRequest,
                          const CSSM_RESOURCE_CONTROL_CONTEXT *inCredAndAclEntry,
                          const void *inOpenParameters,
                          CSSM_DB_HANDLE &outDbHandle)
{
	outDbHandle = CSSM_INVALID_HANDLE;	// CDSA 2.0 says to set this if we fail 
    outDbHandle = insertDbContext(mDatabaseManager.dbCreate(*this,
                                                            DbName(inDbName, CssmNetAddress::optional(inDbLocation)),
                                                            inDBInfo,
                                                            inAccessRequest,
                                                            inCredAndAclEntry,
                                                            inOpenParameters));

}

void
DatabaseSession::DbOpen(const char *inDbName,
                        const CSSM_NET_ADDRESS *inDbLocation,
                        CSSM_DB_ACCESS_TYPE inAccessRequest,
                        const AccessCredentials *inAccessCred,
                        const void *inOpenParameters,
                        CSSM_DB_HANDLE &outDbHandle)
{
	DOCDebug("DatabaseSession::DbOpen: dbName %s", inDbName);
	outDbHandle = CSSM_INVALID_HANDLE;	// CDSA 2.0 says to set this if we fail 
    outDbHandle = insertDbContext(mDatabaseManager.dbOpen(*this,
                                                          DbName(inDbName, CssmNetAddress::optional(inDbLocation)),
                                                          inAccessRequest,
                                                          inAccessCred,
                                                          inOpenParameters));
}

CSSM_DB_HANDLE
DatabaseSession::insertDbContext(DbContext &inDbContext)
{
    CSSM_DB_HANDLE aDbHandle;
    try
    {
        aDbHandle = inDbContext.handle ();
        StLock<Mutex> _(mDbContextMapLock);
        mDbContextMap.insert(DbContextMap::value_type(aDbHandle, &inDbContext));
    }
    catch (...)
    {
        // Close the context
        mDatabaseManager.dbClose(inDbContext);
        throw;
    }

    return aDbHandle;
}

DbContext &
DatabaseSession::findDbContext(CSSM_DB_HANDLE inDbHandle)
{
    StLock<Mutex> _(mDbContextMapLock);
    DbContextMap::iterator it = mDbContextMap.find(inDbHandle);
    if (it == mDbContextMap.end())
        CssmError::throwMe(CSSM_ERRCODE_INVALID_DB_HANDLE);
    return *it->second;
}

void
DatabaseSession::closeAll()
{
    StLock<Mutex> _(mDbContextMapLock);
    for (DbContextMap::iterator it = mDbContextMap.begin();
         it != mDbContextMap.end();
         it++)
    {
        DbContext *aDbContext = it->second;
        try
        {
            mDatabaseManager.dbClose(*aDbContext);
            // This is done by the database itself which owns the context.
            //delete aDbContext;
        }
        catch (...)
        {
            // Ignore exceptions since we want to close as many DBs as possible.
            // XXX @@@ log an error or something.
        }
    }

    mDbContextMap.clear();
}

// Operations using DbContext instances.
void
DatabaseSession::DbClose(CSSM_DB_HANDLE inDbHandle)
{
    StLock<Mutex> _(mDbContextMapLock);
	DOCDebug("DatabaseSession::Close");
    DbContextMap::iterator it = mDbContextMap.find(inDbHandle);
    if (it == mDbContextMap.end())
        CssmError::throwMe(CSSM_ERRCODE_INVALID_DB_HANDLE);
    auto_ptr<DbContext> aDbContext(it->second);
    mDbContextMap.erase(it);
    mDatabaseManager.dbClose(*aDbContext);
}

void
DatabaseSession::CreateRelation(CSSM_DB_HANDLE inDbHandle,
                                CSSM_DB_RECORDTYPE inRelationID,
                                const char *inRelationName,
                                uint32 inNumberOfAttributes,
                                const CSSM_DB_SCHEMA_ATTRIBUTE_INFO &inAttributeInfo,
                                uint32 inNumberOfIndexes,
                                const CSSM_DB_SCHEMA_INDEX_INFO &inIndexInfo)
{
    DbContext &aDbContext = findDbContext(inDbHandle);
    return aDbContext.mDatabase.createRelation(aDbContext, inRelationID, inRelationName,
                                                inNumberOfAttributes, inAttributeInfo,
                                                inNumberOfIndexes, inIndexInfo);
}

void
DatabaseSession::DestroyRelation(CSSM_DB_HANDLE inDbHandle,
                                 CSSM_DB_RECORDTYPE inRelationID)
{
    DbContext &aDbContext = findDbContext(inDbHandle);
    return aDbContext.mDatabase.destroyRelation(aDbContext, inRelationID);
}

void
DatabaseSession::Authenticate(CSSM_DB_HANDLE inDbHandle,
                              CSSM_DB_ACCESS_TYPE inAccessRequest,
                              const AccessCredentials &inAccessCred)
{
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.authenticate(aDbContext, inAccessRequest, inAccessCred);
}


void
DatabaseSession::GetDbAcl(CSSM_DB_HANDLE inDbHandle,
                          const CSSM_STRING *inSelectionTag,
                          uint32 &outNumberOfAclInfos,
                          CSSM_ACL_ENTRY_INFO_PTR &outAclInfos)
{
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.getDbAcl(aDbContext, inSelectionTag, outNumberOfAclInfos, outAclInfos);
}

void
DatabaseSession::ChangeDbAcl(CSSM_DB_HANDLE inDbHandle,
                             const AccessCredentials &inAccessCred,
                             const CSSM_ACL_EDIT &inAclEdit)
{
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.changeDbAcl(aDbContext, inAccessCred, inAclEdit);
}

void
DatabaseSession::GetDbOwner(CSSM_DB_HANDLE inDbHandle,
                            CSSM_ACL_OWNER_PROTOTYPE &outOwner)
{
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.getDbOwner(aDbContext, outOwner);
}

void
DatabaseSession::ChangeDbOwner(CSSM_DB_HANDLE inDbHandle,
                               const AccessCredentials &inAccessCred,
                               const CSSM_ACL_OWNER_PROTOTYPE &inNewOwner)
{
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.changeDbOwner(aDbContext, inAccessCred, inNewOwner);
}

void
DatabaseSession::GetDbNameFromHandle(CSSM_DB_HANDLE inDbHandle,
                                     char **outDbName)
{
    DbContext &aDbContext = findDbContext(inDbHandle);
    Required(outDbName) = aDbContext.mDatabase.getDbNameFromHandle(aDbContext);
}

void
DatabaseSession::DataInsert(CSSM_DB_HANDLE inDbHandle,
                            CSSM_DB_RECORDTYPE inRecordType,
                            const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
                            const CssmData *inData,
                            CSSM_DB_UNIQUE_RECORD_PTR &outUniqueId)
{
	debug("dbsession", "%p DataInsert(%lx,%lx)", this, inDbHandle, inRecordType);
    DbContext &aDbContext = findDbContext(inDbHandle);
    outUniqueId = aDbContext.mDatabase.dataInsert(aDbContext, inRecordType, inAttributes, inData);
}


void
DatabaseSession::DataDelete(CSSM_DB_HANDLE inDbHandle,
                            const CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier)
{
	debug("dbsession", "%p DataDelete(%lx)", this, inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.dataDelete(aDbContext, inUniqueRecordIdentifier);
}


void
DatabaseSession::DataModify(CSSM_DB_HANDLE inDbHandle,
                            CSSM_DB_RECORDTYPE inRecordType,
                            CSSM_DB_UNIQUE_RECORD &inoutUniqueRecordIdentifier,
                            const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributesToBeModified,
                            const CssmData *inDataToBeModified,
                            CSSM_DB_MODIFY_MODE inModifyMode)
{
	debug("dbsession", "%p DataModify(%lx,%lx)", this, inDbHandle, inRecordType);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.dataModify(aDbContext, inRecordType, inoutUniqueRecordIdentifier,
                                     inAttributesToBeModified, inDataToBeModified, inModifyMode);
}

CSSM_HANDLE
DatabaseSession::DataGetFirst(CSSM_DB_HANDLE inDbHandle,
                              const DLQuery *inQuery,
                              CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                              CssmData *inoutData,
                              CSSM_DB_UNIQUE_RECORD_PTR &outUniqueId)
{
	debug("dbsession", "%p DataGetFirst(%lx)", this, inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
	
	return aDbContext.mDatabase.dataGetFirst(aDbContext, inQuery,
		inoutAttributes, inoutData, outUniqueId);
}

bool
DatabaseSession::DataGetNext(CSSM_DB_HANDLE inDbHandle,
                             CSSM_HANDLE inResultsHandle,
                             CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                             CssmData *inoutData,
                             CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
	debug("dbsession", "%p DataGetNext(%lx)", this, inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);

	return aDbContext.mDatabase.dataGetNext(aDbContext, inResultsHandle, inoutAttributes,
			inoutData, outUniqueRecord);
}

void
DatabaseSession::DataAbortQuery(CSSM_DB_HANDLE inDbHandle,
                                CSSM_HANDLE inResultsHandle)
{
	debug("dbsession", "%p DataAbortQuery(%lx)", this, inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.dataAbortQuery(aDbContext, inResultsHandle);
}

void
DatabaseSession::DataGetFromUniqueRecordId(CSSM_DB_HANDLE inDbHandle,
                                           const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
                                           CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                                           CssmData *inoutData)
{
	debug("dbsession", "%p DataGetFromUniqueId(%lx)", this, inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.dataGetFromUniqueRecordId(aDbContext, inUniqueRecord,
                                                   inoutAttributes, inoutData);
}

void
DatabaseSession::FreeUniqueRecord(CSSM_DB_HANDLE inDbHandle,
                                  CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier)
{
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.freeUniqueRecord(aDbContext, inUniqueRecordIdentifier);
}

void
DatabaseSession::PassThrough(CSSM_DB_HANDLE inDbHandle,
                             uint32 passThroughId,
                             const void *inputParams,
                             void **outputParams)
{
	DbContext &aDbContext = findDbContext(inDbHandle);
	aDbContext.mDatabase.passThrough(aDbContext, passThroughId, inputParams, outputParams);
}
