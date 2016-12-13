/*
 * Copyright (c) 2000-2001,2011-2012,2014 Apple Inc. All Rights Reserved.
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
#include <security_cdsa_plugin/DatabaseSession.h>

#include <security_cdsa_plugin/Database.h>
#include <security_cdsa_plugin/DbContext.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <memory>
#include <security_utilities/debugging.h>

/* log open/close events */
#define DOCDebug(args...)	secinfo("DBOpen", ## args)


using namespace std;

extern "C" char* cssmErrorString(CSSM_RETURN errCode);

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
	secinfo("dbsession", "GetDbNames");
	outNameList = mDatabaseManager.getDbNames (*this);

#ifndef NDEBUG
	// dump the returned names
	uint32 n;
	secinfo("dbsession", "GetDbNames returned %d names", outNameList->NumStrings);
	for (n = 0; n < outNameList->NumStrings; ++n)
	{
		secinfo("dbsession", "%d: %s", n, outNameList->String[n]);
	}
#endif
}


void
DatabaseSession::FreeNameList(CSSM_NAME_LIST &inNameList)
{
	secinfo("dbsession", "FreeNameList");
	mDatabaseManager.freeNameList (*this, inNameList);
}


void
DatabaseSession::DbDelete(const char *inDbName,
                          const CSSM_NET_ADDRESS *inDbLocation,
                          const AccessCredentials *inAccessCred)
{
    // The databaseManager will notify all its DbContext instances
    // that the database is question is being deleted.
	secinfo("dbsession", "DbDelete of %s", inDbName);
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
	secinfo("dbsession", "DbCreate of %s", inDbName);
	
    outDbHandle = insertDbContext(mDatabaseManager.dbCreate(*this,
                                                            DbName(inDbName, CssmNetAddress::optional(inDbLocation)),
                                                            inDBInfo,
                                                            inAccessRequest,
                                                            inCredAndAclEntry,
                                                            inOpenParameters));
	secinfo("dbsession", "DbCreate returned handle %#lx", outDbHandle);
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
	secinfo("dbsession", "DbOpen of %s", inDbName);
	outDbHandle = CSSM_INVALID_HANDLE;	// CDSA 2.0 says to set this if we fail 
    outDbHandle = insertDbContext(mDatabaseManager.dbOpen(*this,
                                                          DbName(inDbName, CssmNetAddress::optional(inDbLocation)),
                                                          inAccessRequest,
                                                          inAccessCred,
                                                          inOpenParameters));
	secinfo("dbsession", "DbOpen returned handle %#lx", outDbHandle);
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
	secinfo("dbsession", "DbClose of handle %ld", inDbHandle);
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
                                const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo,
                                uint32 inNumberOfIndexes,
                                const CSSM_DB_SCHEMA_INDEX_INFO &inIndexInfo)
{
	secinfo("dbsession", "CreateRelation from handle %ld of record type %X with relation name %s", inDbHandle, inRelationID, inRelationName);
	secinfo("dbsession", "number of attributes = %d", inNumberOfAttributes);
#ifndef NDEBUG
	unsigned n;
	for (n = 0; n < inNumberOfAttributes; ++n)
	{
		secinfo("dbsession", "%d: id %d name %s, data type %d", n, inAttributeInfo[n].AttributeId,
																	inAttributeInfo[n].AttributeName,
																	inAttributeInfo[n].DataType);
	}
#endif

	secinfo("dbsession", "number of indexes: %d", inNumberOfIndexes);
#ifndef NDEBUG
	for (n = 0; n < inNumberOfIndexes; ++n)
	{
		secinfo("dbsession", "%d: id %d indexid %d indextype %d location %d", n, inIndexInfo.AttributeId,
																				  inIndexInfo.IndexedDataLocation,
																				  inIndexInfo.IndexId,
																				  inIndexInfo.IndexType);
	}
#endif

    DbContext &aDbContext = findDbContext(inDbHandle);
    return aDbContext.mDatabase.createRelation(aDbContext, inRelationID, inRelationName,
                                                inNumberOfAttributes, inAttributeInfo,
                                                inNumberOfIndexes, inIndexInfo);
}

void
DatabaseSession::DestroyRelation(CSSM_DB_HANDLE inDbHandle,
                                 CSSM_DB_RECORDTYPE inRelationID)
{
	secinfo("dbsession", "DestroyRelation (handle %ld) %d", inDbHandle, inRelationID);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.destroyRelation(aDbContext, inRelationID);
}

void
DatabaseSession::Authenticate(CSSM_DB_HANDLE inDbHandle,
                              CSSM_DB_ACCESS_TYPE inAccessRequest,
                              const AccessCredentials &inAccessCred)
{
	secinfo("dbsession", "Authenticate (handle %ld) inAccessRequest %d", inDbHandle, inAccessRequest);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.authenticate(aDbContext, inAccessRequest, inAccessCred);
}


void
DatabaseSession::GetDbAcl(CSSM_DB_HANDLE inDbHandle,
                          const CSSM_STRING *inSelectionTag,
                          uint32 &outNumberOfAclInfos,
                          CSSM_ACL_ENTRY_INFO_PTR &outAclInfos)
{
	secinfo("dbsession", "GetDbAcl (handle %ld)", inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.getDbAcl(aDbContext, inSelectionTag, outNumberOfAclInfos, outAclInfos);
}

void
DatabaseSession::ChangeDbAcl(CSSM_DB_HANDLE inDbHandle,
                             const AccessCredentials &inAccessCred,
                             const CSSM_ACL_EDIT &inAclEdit)
{
	secinfo("dbsession", "ChangeDbAcl (handle %ld)", inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.changeDbAcl(aDbContext, inAccessCred, inAclEdit);
}

void
DatabaseSession::GetDbOwner(CSSM_DB_HANDLE inDbHandle,
                            CSSM_ACL_OWNER_PROTOTYPE &outOwner)
{
	secinfo("dbsession", "GetDbOwner (handle %ld)", inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.getDbOwner(aDbContext, outOwner);
}

void
DatabaseSession::ChangeDbOwner(CSSM_DB_HANDLE inDbHandle,
                               const AccessCredentials &inAccessCred,
                               const CSSM_ACL_OWNER_PROTOTYPE &inNewOwner)
{
	secinfo("dbsession", "ChangeDbOwner (handle %ld)", inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.changeDbOwner(aDbContext, inAccessCred, inNewOwner);
}

void
DatabaseSession::GetDbNameFromHandle(CSSM_DB_HANDLE inDbHandle,
                                     char **outDbName)
{
	secinfo("dbsession", "GetDbNameFromHandle (handle %ld)", inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
    Required(outDbName) = aDbContext.mDatabase.getDbNameFromHandle(aDbContext);
	secinfo("dbsession", "name: %s", *outDbName);
}


#ifndef NDEBUG

#if 0 /* unusued functions */

static
void DumpAttributeInfo(const CSSM_DB_ATTRIBUTE_INFO &info)
{
	const char* attrNameType;
	switch (info.AttributeFormat)
	{
		case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
			attrNameType = "CSSM_DB_ATTRIBUTE_NAME_AS_STRING";
			break;
		
		case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
			attrNameType = "CSSM_DB_ATTRIBUTE_NAME_AS_OID";
			break;
		
		case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
			attrNameType = "CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER";
			break;
	}
	
	secinfo("dbsession", "  Attribute name type: %s", attrNameType);
	switch (info.AttributeFormat)
	{
		case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
			secinfo("dbsession", "  name: %s", info.Label.AttributeName);
			break;

		case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
			secinfo("dbsession", "  name: %d", info.Label.AttributeID);
			break;
		
		case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
			secinfo("dbsession", "  name is oid");
			break;
	}
	
	const char* s;
	switch (info.AttributeFormat)
	{
		case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
			s = "CSSM_DB_ATTRIBUTE_FORMAT_STRING";
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
			s = "CSSM_DB_ATTRIBUTE_FORMAT_SINT32";
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
			s = "CSSM_DB_ATTRIBUTE_FORMAT_UINT32";
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM:
			s = "CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM";
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_REAL:
			s = "CSSM_DB_ATTRIBUTE_FORMAT_REAL";
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE:
			s = "CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE";
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
			s = "CSSM_DB_ATTRIBUTE_FORMAT_BLOB";
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:
			s = "CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32";
			break;
		case CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX:
			s = "CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX";
			break;
	}
	
	secinfo("dbsession", "  attribute format: %s", s);
}


static
void DumpAttributes(const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes)
{
	if (!inAttributes)
	{
		secinfo("dbsession", "No attributes defined.");
		return;
	}
	
	secinfo("dbsession", "insert into %d", inAttributes->DataRecordType);
	secinfo("dbsession", "Semantic information %d", inAttributes->SemanticInformation);
	secinfo("dbsession", "Number of attributes: %d", inAttributes->NumberOfAttributes);

	unsigned n;
	for (n = 0; n < inAttributes->NumberOfAttributes; ++n)
	{
		DumpAttributeInfo(inAttributes->AttributeData[n].Info);
		secinfo("dbsession", "Attribute %d\n", n);
		secinfo("dbsession", "  number of values: %d", inAttributes->AttributeData[n].NumberOfValues);
		unsigned i;
		for (i = 0; i < inAttributes->AttributeData[n].NumberOfValues; ++i)
		{
			switch (inAttributes->AttributeData[n].Info.AttributeFormat)
			{
				case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
				{
					std::string ss((char*) inAttributes->AttributeData[n].Value[i].Data, inAttributes->AttributeData[n].Value[i].Length);
					secinfo("dbsession", "    Value %d: %s", i, ss.c_str());
					break;
				}
				case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
					secinfo("dbsession", "    Value %d: %d", i, *(sint32*)inAttributes->AttributeData[n].Value[i].Data);
					break;
				case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
					secinfo("dbsession", "    Value %d: %u", i, *(uint32*)inAttributes->AttributeData[n].Value[i].Data);
					break;
				case CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM:
					secinfo("dbsession", "    Value %d: (bignum)", i);
					break;
				case CSSM_DB_ATTRIBUTE_FORMAT_REAL:
					secinfo("dbsession", "    Value %d: %f", i, *(double*)inAttributes->AttributeData[n].Value[i].Data);
					break;
				case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE:
					secinfo("dbsession", "    Value %d: %s", i, (char*)inAttributes->AttributeData[n].Value[i].Data);
					break;
				case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
					secinfo("dbsession", "    Value %d: (blob)", i);
					break;
				case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:
				{
					unsigned long j;
					unsigned long numInts = inAttributes->AttributeData[n].Value[i].Length / sizeof(UInt32);
					for (j = 0; j < numInts; ++j)
					{
						uint32* nums = (uint32*) inAttributes->AttributeData[n].Value[i].Data;
						secinfo("dbsession", "      %d", nums[j]);
					}
					
					break;
				}
				
				case CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX:
					secinfo("dbsession", "    Value %d: (complex)", i);
					break;
			}
		}
	}
}
#endif 


static void
DumpUniqueRecord(const CSSM_DB_UNIQUE_RECORD &record)
{
/*
	const char* s;

	switch (record.RecordLocator.IndexType)
	{
		case CSSM_DB_INDEX_UNIQUE:
		{
			s = "CSSM_DB_INDEX_UNIQUE";
			break;
		}
		
		case CSSM_DB_INDEX_NONUNIQUE:
		{
			s = "CSSM_DB_INDEX_NONUNIQUE";
			break;
		}
	}
	
	secinfo("dbsession", "RecordLocator.IndexType: %s", s);
	
	switch (record.RecordLocator.IndexedDataLocation)
	{
		case CSSM_DB_INDEX_ON_UNKNOWN:
		{
			s = "CSSM_DB_INDEX_ON_UNKNOWN";
			break;
		}
		
		case CSSM_DB_INDEX_ON_ATTRIBUTE:
		{
			s = "CSSM_DB_INDEX_ON_ATTRIBUTE";
			break;
		}
		
		case CSSM_DB_INDEX_ON_RECORD:
		{
			s = "CSSM_DB_INDEX_ON_RECORD";
			break;
		}
	}
	
	secinfo("dbsession", "RecordLocator.IndexedDataLocation: %s", s);
	
	secinfo("dbsession", "Attribute info:");
	
	DumpAttributeInfo(record.RecordLocator.Info);
*/

	// put the record ID into hex
	std::string output;
	char hexBuffer[4];
	unsigned i;
	for (i = 0; i < record.RecordIdentifier.Length; ++i)
	{
		sprintf(hexBuffer, "%02X", record.RecordIdentifier.Data[i]);
		output += hexBuffer;
	}
	
	secinfo("dbsession", "    RecordIdentifier.Data: %s", output.c_str());
}
#endif /* NDEBUG */

void
DatabaseSession::DataInsert(CSSM_DB_HANDLE inDbHandle,
                            CSSM_DB_RECORDTYPE inRecordType,
                            const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
                            const CssmData *inData,
                            CSSM_DB_UNIQUE_RECORD_PTR &outUniqueId)
{
	secinfo("dbsession", "%p DataInsert(%lx,%x)", this, inDbHandle, inRecordType);
    DbContext &aDbContext = findDbContext(inDbHandle);
    outUniqueId = aDbContext.mDatabase.dataInsert(aDbContext, inRecordType, inAttributes, inData);

#ifndef NDEBUG
	secinfo("dbsession", "Returned unique id:");
	DumpUniqueRecord(*outUniqueId);
#endif
}


void
DatabaseSession::DataDelete(CSSM_DB_HANDLE inDbHandle,
                            const CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier)
{
	secinfo("dbsession", "%p DataDelete(%lx)", this, inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.dataDelete(aDbContext, inUniqueRecordIdentifier);

#ifndef NDEBUG
	secinfo("dbsession", "Record identifier:");
	DumpUniqueRecord(inUniqueRecordIdentifier);
#endif
}


void
DatabaseSession::DataModify(CSSM_DB_HANDLE inDbHandle,
                            CSSM_DB_RECORDTYPE inRecordType,
                            CSSM_DB_UNIQUE_RECORD &inoutUniqueRecordIdentifier,
                            const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributesToBeModified,
                            const CssmData *inDataToBeModified,
                            CSSM_DB_MODIFY_MODE inModifyMode)
{
	secinfo("dbsession", "%p DataModify(%lx,%x)", this, inDbHandle, inRecordType);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.dataModify(aDbContext, inRecordType, inoutUniqueRecordIdentifier,
                                     inAttributesToBeModified, inDataToBeModified, inModifyMode);
#ifndef NDEBUG
	secinfo("dbsession", "Out record identifier:");
	DumpUniqueRecord(inoutUniqueRecordIdentifier);
#endif
}

CSSM_HANDLE
DatabaseSession::DataGetFirst(CSSM_DB_HANDLE inDbHandle,
                              const CssmQuery *inQuery,
                              CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                              CssmData *inoutData,
                              CSSM_DB_UNIQUE_RECORD_PTR &outUniqueId)
{
	secinfo("dbsession", "%p DataGetFirst(%lx)", this, inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
	
	CSSM_HANDLE result = aDbContext.mDatabase.dataGetFirst(aDbContext, inQuery,
														   inoutAttributes, inoutData, outUniqueId);
#ifndef NDEBUG
	secinfo("dbsession", "result handle: %lx", result);
	if (result != 0)
	{
		secinfo("dbsession", "Returned ID:");
		DumpUniqueRecord(*outUniqueId);
	}
#endif

	return result;
}

bool
DatabaseSession::DataGetNext(CSSM_DB_HANDLE inDbHandle,
                             CSSM_HANDLE inResultsHandle,
                             CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                             CssmData *inoutData,
                             CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
	secinfo("dbsession", "DataGetNext(%lx)", inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);

	bool result = aDbContext.mDatabase.dataGetNext(aDbContext, inResultsHandle, inoutAttributes,
			inoutData, outUniqueRecord);

#ifndef NDEBUG
	if (result)
	{
		secinfo("dbsession", "Returned ID:");
		DumpUniqueRecord(*outUniqueRecord);
	}
#endif

	return result;
}

void
DatabaseSession::DataAbortQuery(CSSM_DB_HANDLE inDbHandle,
                                CSSM_HANDLE inResultsHandle)
{
	secinfo("dbsession", "%p DataAbortQuery(%lx)", this, inDbHandle);
    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.dataAbortQuery(aDbContext, inResultsHandle);
}

void
DatabaseSession::DataGetFromUniqueRecordId(CSSM_DB_HANDLE inDbHandle,
                                           const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
                                           CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                                           CssmData *inoutData)
{
	secinfo("dbsession", "%p DataGetFromUniqueId(%lx)", this, inDbHandle);
#ifndef NDEBUG
	secinfo("dbsession", "inUniqueRecord:");
	DumpUniqueRecord(inUniqueRecord);
#endif

    DbContext &aDbContext = findDbContext(inDbHandle);
    aDbContext.mDatabase.dataGetFromUniqueRecordId(aDbContext, inUniqueRecord,
                                                   inoutAttributes, inoutData);
}

void
DatabaseSession::FreeUniqueRecord(CSSM_DB_HANDLE inDbHandle,
                                  CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier)
{
	secinfo("dbsession", "FreeUniqueRecord: %lx", inDbHandle);
#ifndef NDEBUG
	secinfo("dbsession", "inUniqueRecordIdentifier follows:");
	DumpUniqueRecord(inUniqueRecordIdentifier);
#endif
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
