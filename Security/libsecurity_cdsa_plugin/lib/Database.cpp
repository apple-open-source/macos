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


#ifdef __MWERKS__
#define _CPP_DATABASE
#endif
#include <security_cdsa_plugin/Database.h>
#include <Security/cssmerr.h>
#include <security_cdsa_plugin/DbContext.h>
#include <memory>

DatabaseManager::DatabaseManager ()
{
}

DatabaseManager::~DatabaseManager ()
{
}

Database *
DatabaseManager::get (const DbName &inDbName)
{
    StLock<Mutex> _(mDatabaseMapLock);
    DatabaseMap::iterator anIterator = mDatabaseMap.find (inDbName);
    if (anIterator == mDatabaseMap.end())
    {
        auto_ptr<Database> aDatabase(make(inDbName));
        mDatabaseMap.insert(DatabaseMap::value_type(aDatabase->mDbName, aDatabase.get()));
        return aDatabase.release();
    }

    return anIterator->second;
}

void
DatabaseManager::removeIfUnused(Database &inDatabase)
{
    StLock<Mutex> _(mDatabaseMapLock);
    if (!inDatabase.hasDbContexts()) {
        mDatabaseMap.erase(inDatabase.mDbName);
		delete &inDatabase;
	}
}

DbContext &
DatabaseManager::dbOpen(DatabaseSession &inDatabaseSession,
                        const DbName &inDbName,
                        CSSM_DB_ACCESS_TYPE inAccessRequest,
                        const AccessCredentials *inAccessCred,
                        const void *inOpenParameters)
{
    Database &aDatabase = *get(inDbName);
    try
    {
        return aDatabase._dbOpen(inDatabaseSession, inAccessRequest, inAccessCred, inOpenParameters);
    }
    catch (...)
    {
        removeIfUnused(aDatabase);
        throw;
    }
}

DbContext &
DatabaseManager::dbCreate(DatabaseSession &inDatabaseSession,
                          const DbName &inDbName,
                          const CSSM_DBINFO &inDBInfo,
                          CSSM_DB_ACCESS_TYPE inAccessRequest,
                          const CSSM_RESOURCE_CONTROL_CONTEXT *inCredAndAclEntry,
                          const void *inOpenParameters)
{
    Database &aDatabase = *get(inDbName);
    try
    {
        return aDatabase._dbCreate(inDatabaseSession, inDBInfo, inAccessRequest,
                                   inCredAndAclEntry, inOpenParameters);
    }
    catch (...)
    {
        removeIfUnused(aDatabase);
        throw;
    }
}

// Delete a DbContext instance created by calling dbOpen or dbCreate.
void
DatabaseManager::dbClose(DbContext &inDbContext)
{
    Database &aDatabase = inDbContext.mDatabase;
    aDatabase._dbClose(inDbContext);
    removeIfUnused(aDatabase);
}

// Delete a database.
void
DatabaseManager::dbDelete(DatabaseSession &inDatabaseSession,
                          const DbName &inDbName,
                          const AccessCredentials *inAccessCred)
{
    Database &aDatabase = *get(inDbName);
    try
    {
        aDatabase.dbDelete(inDatabaseSession, inAccessCred);
    }
    catch (...)
    {
        removeIfUnused(aDatabase);
        throw;
    }

    removeIfUnused(aDatabase);
}

// List all available databases.
CSSM_NAME_LIST_PTR
DatabaseManager::getDbNames(DatabaseSession &inDatabaseSession)
{
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
DatabaseManager::freeNameList(DatabaseSession &inDatabaseSession,
                  CSSM_NAME_LIST &inNameList)
{
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

// Start of Database implementation.

Database::Database (const DbName &inDbName)
: mDbName(inDbName)
{
}

Database::~Database ()
{
}

bool
Database::hasDbContexts()
{
    StLock<Mutex> _(mDbContextSetLock);
    return !mDbContextSet.empty();
}

DbContext &
Database::_dbOpen(DatabaseSession &inDatabaseSession,
                  CSSM_DB_ACCESS_TYPE inAccessRequest,
                  const AccessCredentials *inAccessCred,
                  const void *inOpenParameters)
{
    auto_ptr<DbContext>aDbContext(makeDbContext(inDatabaseSession,
                                                inAccessRequest,
                                                inAccessCred,
                                                inOpenParameters));
    {
        StLock<Mutex> _(mDbContextSetLock);
        mDbContextSet.insert(aDbContext.get());
        // Release the mDbContextSetLock
    }

    try
    {
        dbOpen(*aDbContext);
    }
    catch (...)
    {
        StLock<Mutex> _(mDbContextSetLock);
        mDbContextSet.erase(aDbContext.get());
        throw;
    }

    return *aDbContext.release();
}

DbContext &
Database::_dbCreate(DatabaseSession &inDatabaseSession,
                    const CSSM_DBINFO &inDBInfo,
                    CSSM_DB_ACCESS_TYPE inAccessRequest,
                    const CSSM_RESOURCE_CONTROL_CONTEXT *inCredAndAclEntry,
                    const void *inOpenParameters)
{
    auto_ptr<DbContext>aDbContext(makeDbContext(inDatabaseSession,
                                                inAccessRequest,
                                                (inCredAndAclEntry
												 ? AccessCredentials::optional(inCredAndAclEntry->AccessCred)
												 : NULL),
                                                inOpenParameters));
    {
        StLock<Mutex> _(mDbContextSetLock);
        mDbContextSet.insert(aDbContext.get());
        // Release the mDbContextSetLock
    }

    try
    {
        dbCreate(*aDbContext, inDBInfo,
                 inCredAndAclEntry ? &inCredAndAclEntry->InitialAclEntry : NULL);
    }
    catch (...)
    {
        StLock<Mutex> _(mDbContextSetLock);
        mDbContextSet.erase(aDbContext.get());
        throw;
    }

    return *aDbContext.release();
}

void
Database::_dbClose(DbContext &dbContext)
{
    StLock<Mutex> _(mDbContextSetLock);
    mDbContextSet.erase(&dbContext);
    if (mDbContextSet.empty())
        dbClose();
}
