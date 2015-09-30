/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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


#ifndef _DATABASE_H_
#define _DATABASE_H_  1

#include <security_cdsa_utilities/cssmacl.h>
#include <security_utilities/threading.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <list>
#include <map>
#include <set>


// @@@ Should not use using in headers.
using namespace std;

namespace Security
{

class Database;
class DatabaseFactory;
class DatabaseSession;
class DbContext;

/* DatabaseManager class.  */
class DatabaseManager
{
	NOCOPY(DatabaseManager)
public:
    DatabaseManager ();
    virtual ~DatabaseManager ();

    // Create and return a new DbContext instance which is owned by us and must be discared by calling dbClose.
    virtual DbContext &dbOpen(DatabaseSession &inDatabaseSession,
                              const DbName &inDbName,
                              CSSM_DB_ACCESS_TYPE inAccessRequest,
                              const AccessCredentials *inAccessCred,
                              const void *inOpenParameters);
    virtual DbContext &dbCreate(DatabaseSession &inDatabaseSession,
                                const DbName &inDbName,
                                const CSSM_DBINFO &inDBInfo,
                                CSSM_DB_ACCESS_TYPE inAccessRequest,
                                const CSSM_RESOURCE_CONTROL_CONTEXT *inCredAndAclEntry,
                                const void *inOpenParameters);

    // Delete a DbContext instance created by calling dbOpen or dbCreate.
    virtual void dbClose(DbContext &inDbContext);

    // Delete a database.
    virtual void dbDelete(DatabaseSession &inDatabaseSession,
                          const DbName &inDbName,
                          const AccessCredentials *inAccessCred);

    // List all available databases.
    virtual CSSM_NAME_LIST_PTR getDbNames(DatabaseSession &inDatabaseSession);
    virtual void freeNameList(DatabaseSession &inDatabaseSession,
                              CSSM_NAME_LIST &inNameList);
protected:
    virtual void removeIfUnused(Database &inDatabase);
    virtual Database *get (const DbName &inDbName); // Get existing instance or make a new one.
    virtual Database *make (const DbName &inDbName) = 0; // Create a new database instance subclass must implement.
private:
    typedef map<DbName, Database *> DatabaseMap;
    DatabaseMap mDatabaseMap;
    Mutex mDatabaseMapLock;
};


/* Database is an abstract class.  Each Database subclass should implement all the
   pure virtual methods listed below.  The constructor for a particular Database
   subclass should create the Database object.  A subsequent call to dBOpen or
   dBCreate should be is made.  This returns a DbContext.  All other methods take
   a DbContext as an argument.
 */
class Database
{
public:
    virtual void
    dbCreate (DbContext &inDbContext, const CSSM_DBINFO &inDBInfo,
              const CSSM_ACL_ENTRY_INPUT *inInitialAclEntry) = 0;

    // Don't override this method in subclasses.
    virtual DbContext &
    _dbCreate(DatabaseSession &inDatabaseSession,
             const CSSM_DBINFO &inDBInfo,
             CSSM_DB_ACCESS_TYPE inAccessRequest,
             const CSSM_RESOURCE_CONTROL_CONTEXT *inCredAndAclEntry,
             const void *inOpenParameters);

    virtual void
    dbOpen (DbContext &inDbContext) = 0;

    // Don't override this method in subclasses.
    virtual DbContext &
    _dbOpen (DatabaseSession &inDatabaseSession,
            CSSM_DB_ACCESS_TYPE inAccessRequest,
            const AccessCredentials *inAccessCred,
            const void *inOpenParameters);

    virtual void
    dbClose () = 0;

    // Don't override this method in subclasses.
    virtual void
    _dbClose (DbContext &dbContext);

    virtual void
    dbDelete(DatabaseSession &inDatabaseSession,
             const AccessCredentials *inAccessCred) = 0;

    virtual void
    createRelation (DbContext &dbContext,
                    CSSM_DB_RECORDTYPE inRelationID,
                    const char *inRelationName,
                    uint32 inNumberOfAttributes,
                    const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo,
                    uint32 inNumberOfIndexes,
                    const CSSM_DB_SCHEMA_INDEX_INFO &inIndexInfo) = 0;

    virtual void
    destroyRelation (DbContext &dbContext,
                     CSSM_DB_RECORDTYPE inRelationID) = 0;

    virtual void
    authenticate(DbContext &dbContext,
                 CSSM_DB_ACCESS_TYPE inAccessRequest,
                 const AccessCredentials &inAccessCred) = 0;

    virtual void
    getDbAcl(DbContext &dbContext,
             const CSSM_STRING *inSelectionTag,
             uint32 &outNumberOfAclInfos,
             CSSM_ACL_ENTRY_INFO_PTR &outAclInfos) = 0;

    virtual void
    changeDbAcl(DbContext &dbContext,
                const AccessCredentials &inAccessCred,
                const CSSM_ACL_EDIT &inAclEdit) = 0;

    virtual void
    getDbOwner(DbContext &dbContext, CSSM_ACL_OWNER_PROTOTYPE &outOwner) = 0;

    virtual void
    changeDbOwner(DbContext &dbContext,
                  const AccessCredentials &inAccessCred,
                  const CSSM_ACL_OWNER_PROTOTYPE &inNewOwner) = 0;

    virtual char *
    getDbNameFromHandle (const DbContext &dbContext) const = 0;

    virtual CSSM_DB_UNIQUE_RECORD_PTR
    dataInsert (DbContext &dbContext,
                CSSM_DB_RECORDTYPE RecordType,
                const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
                const CssmData *inData) = 0;

    virtual void
    dataDelete (DbContext &dbContext,
                const CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier) = 0;

    virtual void
    dataModify (DbContext &dbContext,
                CSSM_DB_RECORDTYPE RecordType,
                CSSM_DB_UNIQUE_RECORD &inoutUniqueRecordIdentifier,
                const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributesToBeModified,
                const CssmData *inDataToBeModified,
                CSSM_DB_MODIFY_MODE ModifyMode) = 0;

    virtual CSSM_HANDLE
    dataGetFirst (DbContext &dbContext,
                  const CssmQuery *inQuery,
                  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                  CssmData *inoutData,
                  CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord) = 0;

    virtual bool
    dataGetNext (DbContext &dbContext,
                 CSSM_HANDLE inResultsHandle,
                 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                 CssmData *inoutData,
                 CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord) = 0;

    virtual void
    dataAbortQuery (DbContext &dbContext,
                    CSSM_HANDLE inResultsHandle) = 0;

    virtual void
    dataGetFromUniqueRecordId (DbContext &dbContext,
                               const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
                               CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                               CssmData *inoutData) = 0;

    virtual void
    freeUniqueRecord (DbContext &dbContext,
                      CSSM_DB_UNIQUE_RECORD &inUniqueRecord) = 0;
					  
	virtual void
	passThrough(DbContext &dbContext,
				uint32 passThroughId,
				const void *inputParams,
				void **outputParams) = 0;

    Database (const DbName &inDbName);
    virtual ~Database ();

    virtual bool hasDbContexts();

    // XXX @@@ Think about consequences of race conditions between DbOpen/DbCreate/DbDelete/DbClose
    // on databases with the same name at the same time. 
    //virtual DbContext &insertDbContext();
    //virtual void removeDbContext(DbContext &inDbContext);

    const DbName mDbName;
protected:
    // Subclasses must implement this method.
    virtual DbContext *makeDbContext(DatabaseSession &inDatabaseSession,
                                     CSSM_DB_ACCESS_TYPE inAccessRequest,
                                     const AccessCredentials *inAccessCred,
                                     const void *inOpenParameters) = 0;
private:
    typedef set<DbContext *> DbContextSet;
    DbContextSet mDbContextSet;
    Mutex mDbContextSetLock;
};

} // end namespace Security

#ifdef _CPP_DATABASE
# pragma export off
#endif

#endif //_DATABASE_H_
