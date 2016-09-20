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


#ifndef _MDSDATABASE_H_
#define _MDSDATABASE_H_  1

#include <security_filedb/AppleDatabase.h>
#include <security_utilities/threading.h>
#include <map>

/* This is the concrete DatabaseFactory subclass that creates MDSDatabase instances.
   Specifically with MDS there are always exactly 2 databases with fixed names.  These
   are both created whenever the first Database is requested from this factory.  The
   contents of these databases is constructed on the fly by scanning the CSSM bundle
   path for plugins and reading the mds segments from there. Asking
   for a Database with a name different from the 2 allowed ones will result in an
   exception being thrown.
 */
 
class MDSDatabaseManager: public AppleDatabaseManager
{
public:
	MDSDatabaseManager(const AppleDatabaseTableName *tableNames);
    Database *make(const DbName &inDbName);
};

/* This is the class which represents each of the two MDS databases. */

class MDSDatabase: public AppleDatabase
{
public:
    MDSDatabase(const DbName &inDbName, const AppleDatabaseTableName *tableNames);

    virtual
    ~MDSDatabase();

    DbContext *
    makeDbContext(DatabaseSession &inDatabaseSession,
		CSSM_DB_ACCESS_TYPE inAccessRequest,
		const AccessCredentials *inAccessCred,
		const void *inOpenParameters);

    virtual void
    dbOpen(DbContext &inDbContext);

    virtual void
    dbClose();

    virtual void
    dbCreate(DbContext &inDbContext, const CSSM_DBINFO &inDBInfo,
		const CSSM_ACL_ENTRY_INPUT *inInitialAclEntry);

    virtual void
    dbDelete(DatabaseSession &inDatabaseSession,
		const AccessCredentials *inAccessCred);

    virtual void
    createRelation (DbContext &dbContext,
                    CSSM_DB_RECORDTYPE inRelationID,
                    const char *inRelationName,
                    uint32 inNumberOfAttributes,
                    const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo,
                    uint32 inNumberOfIndexes,
                    const CSSM_DB_SCHEMA_INDEX_INFO &inIndexInfo);

    virtual void
    destroyRelation (DbContext &dbContext, CSSM_DB_RECORDTYPE inRelationID);

    virtual void
    authenticate(DbContext &dbContext,
                 CSSM_DB_ACCESS_TYPE inAccessRequest,
                 const AccessCredentials &inAccessCred);

    virtual void
    getDbAcl(DbContext &dbContext,
             const CSSM_STRING *inSelectionTag,
             uint32 &outNumberOfAclInfos,
             CSSM_ACL_ENTRY_INFO_PTR &outAclInfos);

    virtual void
    changeDbAcl(DbContext &dbContext,
                const AccessCredentials &inAccessCred,
                const CSSM_ACL_EDIT &inAclEdit);

    virtual void
    getDbOwner(DbContext &dbContext, CSSM_ACL_OWNER_PROTOTYPE &outOwner);

    virtual void
    changeDbOwner(DbContext &dbContext,
                  const AccessCredentials &inAccessCred,
                  const CSSM_ACL_OWNER_PROTOTYPE &inNewOwner);

    virtual char *
    getDbNameFromHandle (const DbContext &dbContext) const;

    virtual CSSM_DB_UNIQUE_RECORD_PTR
    dataInsert (DbContext &dbContext,
                CSSM_DB_RECORDTYPE RecordType,
                const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
                const CssmData *inData);

    virtual void
    dataDelete (DbContext &dbContext,
                const CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier);

    virtual void
    dataModify (DbContext &dbContext,
                CSSM_DB_RECORDTYPE RecordType,
                CSSM_DB_UNIQUE_RECORD &inoutUniqueRecordIdentifier,
                const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributesToBeModified,
                const CssmData *inDataToBeModified,
                CSSM_DB_MODIFY_MODE ModifyMode);

    virtual CSSM_HANDLE
    dataGetFirst (DbContext &dbContext,
                  const CssmQuery *inQuery,
                  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                  CssmData *inoutData,
                  CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord);

    virtual bool
    dataGetNext (DbContext &dbContext,
                 CSSM_HANDLE inResultsHandle,
                 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                 CssmData *inoutData,
                 CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord);

    virtual void
    dataAbortQuery (DbContext &dbContext,
                    CSSM_HANDLE inResultsHandle);

    virtual void
    dataGetFromUniqueRecordId (DbContext &dbContext,
                               const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
                               CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                               CssmData *inoutData);

    virtual void
    freeUniqueRecord (DbContext &dbContext,
                      CSSM_DB_UNIQUE_RECORD &inUniqueRecord);
};

#endif //_MDSDATABASE_H_
