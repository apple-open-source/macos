/*
 * Copyright (c) 2000-2001,2003,2011,2014 Apple Inc. All Rights Reserved.
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
//  AppleDatabase.h - Description t.b.d.
//
#ifndef _H_APPLEDATABASE
#define _H_APPLEDATABASE

#include "MetaRecord.h"
#include "SelectionPredicate.h"
#include "DbIndex.h"

#include <security_filedb/AtomicFile.h>
#include <security_cdsa_plugin/Database.h>
#include <security_cdsa_plugin/DbContext.h>
#include <security_cdsa_utilities/handleobject.h>
#include <security_utilities/refcount.h>
#include <memory>
#include <vector>
#include <CoreFoundation/CFDate.h>

namespace Security
{

// Abstract database Cursor class.
class Cursor;
class DbVersion;
class CssmAutoQuery;

struct AppleDatabaseTableName
{
	uint32 mTableId;
	const char *mTableName;
	
	// indices of meta-table entries in an array of table names

	enum {
		kSchemaInfo = 0,
		kSchemaAttributes,
		kSchemaIndexes,
		kSchemaParsingModule,
		kNumRequiredTableNames
	};
};

//
// This is what the CDSA standard refers to as a Relation.  We use
// the more conventional term Table.
//
class Table
{
	NOCOPY(Table)
public:
	// Type used to refer to a table.
	typedef CSSM_DB_RECORDTYPE Id;

    Table(const ReadSection &inTableSection);
	~Table();

	// Return a newly created cursor satisfying inQuery on the receiving table
	// The returned Cursor may or may not use indexes depending on their availability.
	Cursor *createCursor(const CSSM_QUERY *inQuery, const DbVersion &inDbVersion) const;

	const ReadSection getRecordSection(uint32 inRecordNumber) const;

	const RecordId getRecord(const RecordId &inRecordId,
							 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
							 CssmData *inoutData,
							 Allocator &inAllocator) const;

	// Return the number of recordNumbers in use by this table including empty slots.
    uint32 recordNumberCount() const { return mRecordNumbersCount; }
    uint32 freeListHead() const { return mFreeListHead; }

	// Return the record number corresponding to aFreeListHead and update
	// aFreeListHead to point to the next availble recordNumber slot.
	uint32 popFreeList(uint32 &aFreeListHead) const;

	MetaRecord &getMetaRecord() { return mMetaRecord; }
	const MetaRecord &getMetaRecord() const { return mMetaRecord; }

	uint32 getRecordsCount() const { return mRecordsCount; }
	const ReadSection getRecordsSection() const;
	
	const ReadSection &getTableSection() const { return mTableSection; }

	bool matchesTableId(Id inTableId) const;

	void readIndexSection();
	
	enum
	{
		OffsetSize					= AtomSize * 0,
		OffsetId					= AtomSize * 1,
		OffsetRecordsCount			= AtomSize * 2,
		OffsetRecords				= AtomSize * 3,
		OffsetIndexesOffset			= AtomSize * 4,
		OffsetFreeListHead			= AtomSize * 5,
		OffsetRecordNumbersCount	= AtomSize * 6,
		OffsetRecordNumbers			= AtomSize * 7
	};
protected:
	friend class ModifiedTable;
	
	MetaRecord mMetaRecord;
	const ReadSection mTableSection;

	uint32 mRecordsCount;
	uint32 mFreeListHead;
	// Number of record numbers (including freelist slots) in this table.
	uint32 mRecordNumbersCount;

	// all the table's indexes, mapped by index id
	typedef map<uint32, DbConstIndex *> ConstIndexMap;
	ConstIndexMap mIndexMap;
};

class ModifiedTable
{
	NOCOPY(ModifiedTable)
public:
	ModifiedTable(const Table *inTable);
	ModifiedTable(MetaRecord *inMetaRecord); // Take over ownership of inMetaRecord
	~ModifiedTable();

	// Mark the record with inRecordId as deleted.
    void deleteRecord(const RecordId &inRecordId);
    const RecordId insertRecord(uint32 inVersionId,
								const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
								const CssmData *inData);
    const RecordId updateRecord(const RecordId &inRecordId,
								const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
								const CssmData *inData,
								CSSM_DB_MODIFY_MODE inModifyMode);
	const RecordId getRecord(const RecordId &inRecordId,
							 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
							 CssmData *inoutData,
							 Allocator &inAllocator) const;

	// Return the MetaRecord this table should use for writes.
	const MetaRecord &getMetaRecord() const;

	// find, and create if needed, an index with the given id
	DbMutableIndex &findIndex(uint32 indexId, const MetaRecord &metaRecord, bool isUniqueIndex);

	// Write this table to inOutputFile at inSectionOffset and return the new offset.
    uint32 writeTable(AtomicTempFile &inAtomicTempFile, uint32 inSectionOffset);

private:
	// Return the next available record number for this table.
    uint32 nextRecordNumber();

	// Return the number of recordNumbers in use by this table including empty slots.
    uint32 recordNumberCount() const;

	void modifyTable();
	void createMutableIndexes();
	uint32 writeIndexSection(WriteSection &tableSection, uint32 offset);

	// Optional, this is merly a reference, we do not own this object.
	const Table *mTable;
	
	// Optional, New MetaRecord.  This is only present if it is different from the
	// MetaRecord of mTable or mTable is nil.
	const MetaRecord *mNewMetaRecord;

	// Set of Records that have been deleted or modified.
    typedef set<uint32> DeletedSet;
    DeletedSet mDeletedSet;

	// Set of Records that have been inserted or modified.
    typedef map<uint32, WriteSection *> InsertedMap;
    InsertedMap mInsertedMap;

	// Next lowest available RecordNumber
    uint32 mRecordNumberCount;
	// Head of the free list (if there is one) or 0 if either we have no
	// mTable of the free list has been exhausted.
    uint32 mFreeListHead;
	
	// has this table actually been modified?
	bool mIsModified;

	typedef map<uint32, DbMutableIndex *> MutableIndexMap;
	MutableIndexMap mIndexMap;
};

//
// Read only snapshot of a database.
//
class Metadata
{
	NOCOPY(Metadata)
protected:
	Metadata() {}
    enum
    {
		HeaderOffset		= 0,	// Absolute offset of header.
		OffsetMagic			= AtomSize * 0,
		OffsetVersion		= AtomSize * 1,
		OffsetAuthOffset	= AtomSize * 2,
		OffsetSchemaOffset	= AtomSize * 3,
        HeaderSize			= AtomSize * 4,

        HeaderMagic			= FOUR_CHAR_CODE('kych'),
        HeaderVersion		= 0x00010000
    };

	enum
	{
		OffsetSchemaSize	= AtomSize * 0,
		OffsetTablesCount	= AtomSize * 1,
		OffsetTables		= AtomSize * 2
	};
};

//
// Read only representation of a database
//
class DbVersion : public Metadata, public RefCount
{
	NOCOPY(DbVersion)
public:
    DbVersion(const class AppleDatabase &db, const RefPointer <AtomicBufferedFile> &inAtomicBufferedFile);
    ~DbVersion();

	uint32 getVersionId() const { return mVersionId; }
	const RecordId getRecord(Table::Id inTableId, const RecordId &inRecordId,
							 CSSM_DB_RECORD_ATTRIBUTE_DATA *inoutAttributes,
							 CssmData *inoutData, Allocator &inAllocator) const;
	Cursor *createCursor(const CSSM_QUERY *inQuery) const;
protected:
	const Table &findTable(Table::Id inTableId) const;
	Table &findTable(Table::Id inTableId);

private:
    void open(); // Part of constructor contract.

	ReadSection mDatabase;
    uint32 mVersionId;

	friend class DbModifier; // XXX Fixme
    typedef map<Table::Id, Table *> TableMap;
    TableMap mTableMap;
	const class AppleDatabase &mDb;
	RefPointer<AtomicBufferedFile> mBufferedFile;

public:
	typedef Table value_type;
	typedef const Table &const_reference;
	typedef const Table *const_pointer;

	// A const forward iterator.
	class const_iterator
	{
	public:
		const_iterator(const TableMap::const_iterator &it) : mIterator(it) {}

		// Use default copy consturctor and assignment operator.
		//const_iterator(const const_iterator &it) : mIterator(it.mIterator) {}
		//const_iterator &operator=(const const_iterator &it) { mIterator = it.mIterator; return *this; }
		const_reference operator*() const { return *mIterator->second; }
		const_iterator &operator++() { mIterator.operator++(); return *this; }
		const_iterator operator++(int i) { return const_iterator(mIterator.operator++(i)); }
		bool operator!=(const const_iterator &other) const { return mIterator != other.mIterator; }
		bool operator==(const const_iterator &other) const { return mIterator == other.mIterator; }

		const_pointer operator->() const { return mIterator->second; } // Not really needed.

	private:
		TableMap::const_iterator mIterator;
	};

	const_iterator begin() const { return const_iterator(mTableMap.begin()); }
	const_iterator end() const { return const_iterator(mTableMap.end()); }

	bool hasTable(Table::Id inTableId) const;
};

//
// Cursor
//
class Cursor : public HandleObject
{
public:
    Cursor();
    Cursor(const DbVersion &inDbVersion);
    virtual ~Cursor();
    virtual bool next(Table::Id &outTableId,
					CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR outAttributes,
					CssmData *outData,
					Allocator &inAllocator,
					RecordId &recordId);
protected:
	const RefPointer<const DbVersion> mDbVersion;
};


//
// LinearCursor
//
class LinearCursor : public Cursor
{
    NOCOPY(LinearCursor)
public:
    LinearCursor(const CSSM_QUERY *inQuery, const DbVersion &inDbVersion,
		   const Table &inTable);
    virtual ~LinearCursor();
    virtual bool next(Table::Id &outTableId,
					CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR outAttributes,
					CssmData *outData,
					Allocator &inAllocator,
					RecordId &recordId);
					
private:
	uint32 mRecordsCount;
    uint32 mRecord;
	const ReadSection mRecordsSection;
	uint32 mReadOffset;
    const MetaRecord &mMetaRecord;

    CSSM_DB_CONJUNCTIVE mConjunctive;
    CSSM_QUERY_FLAGS mQueryFlags; // If CSSM_QUERY_RETURN_DATA is set return the raw key bits;
    typedef vector<SelectionPredicate *> PredicateVector;

    PredicateVector mPredicates;
};

//
// A cursor that uses an index.
//

class IndexCursor : public Cursor
{
	NOCOPY(IndexCursor)
public:
	IndexCursor(DbQueryKey *queryKey, const DbVersion &inDbVersion,
		const Table &table, const DbConstIndex *index);
	virtual ~IndexCursor();

    virtual bool next(Table::Id &outTableId,
					CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR outAttributes,
					CssmData *outData,
					Allocator &inAllocator,
					RecordId &recordId);

private:
	auto_ptr<DbQueryKey> mQueryKey;
	const Table &mTable;
	const DbConstIndex *mIndex;
	
	DbIndexIterator mBegin, mEnd;
};

//
// MultiCursor
//
class MultiCursor : public Cursor
{
    NOCOPY(MultiCursor)
public:
    MultiCursor(const CSSM_QUERY *inQuery, const DbVersion &inDbVersion);
    virtual ~MultiCursor();
    virtual bool next(Table::Id &outTableId,
					CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR outAttributes,
					CssmData *outData,
					Allocator &inAllocator,
					RecordId &recordId);
private:
	auto_ptr<CssmAutoQuery> mQuery;

	DbVersion::const_iterator mTableIterator;
	auto_ptr<Cursor> mCursor;
};

//
// A DbModifier contains all pending changes to be made to a DB.
// It also contains a DbVersion representing the state of the Database before any such changes
// No read-style operations are supported by DbModifier.  If a DbModifier exists for a
// particular Database and a client wishes to perform a query commit() must be called and
// the client should perform the new query on the current database version after the commit.
// Otherwise a client will not see changes made since the DbModifier was instanciated.
//
class DbModifier : public Metadata
{
	NOCOPY(DbModifier)
public:
    DbModifier(AtomicFile &inAtomicFile, const class AppleDatabase &db);
    ~DbModifier();

	// Whole database affecting members.
    void createDatabase(const CSSM_DBINFO &inDbInfo,
						const CSSM_ACL_ENTRY_INPUT *inInitialAclEntry,
						mode_t mode);
	void openDatabase(); // This is optional right now.
	void closeDatabase();
	void deleteDatabase();

    void commit();
    void rollback() throw();

	// Record changing members
	void deleteRecord(Table::Id inTableId, const RecordId &inRecordId);
	const RecordId insertRecord(Table::Id inTableId,
								const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
								const CssmData *inData);
	const RecordId updateRecord(Table::Id inTableId, const RecordId &inRecordId,
								const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
								const CssmData *inData,
								CSSM_DB_MODIFY_MODE inModifyMode);

	// Schema changing members
    void insertTable(Table::Id inTableId, const string &inTableName,
					 uint32 inNumberOfAttributes,
					 const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo,
					 uint32 inNumberOfIndexes,
					 const CSSM_DB_SCHEMA_INDEX_INFO *inIndexInfo);
	void deleteTable(Table::Id inTableId);

	// Record reading members
	const RecordId getRecord(Table::Id inTableId, const RecordId &inRecordId,
							 CSSM_DB_RECORD_ATTRIBUTE_DATA *inoutAttributes,
							 CssmData *inoutData, Allocator &inAllocator);
	Cursor *createCursor(const CSSM_QUERY *inQuery);

	bool hasTable(Table::Id inTableid);

protected:
    void modifyDatabase();
    const RefPointer<const DbVersion> getDbVersion(bool force);

    ModifiedTable *createTable(MetaRecord *inMetaRecord); // Takes over ownership of inMetaRecord
	
	void insertTableSchema(const CssmDbRecordAttributeInfo &inInfo,
		const CSSM_DB_RECORD_INDEX_INFO *inIndexInfo = NULL);
		
	void insertTable(const CssmDbRecordAttributeInfo &inInfo,
		const CSSM_DB_RECORD_INDEX_INFO * inIndexInfo = NULL,
		const CSSM_DB_PARSING_MODULE_INFO * inParsingModule = NULL);

    ModifiedTable &findTable(Table::Id inTableId);

    uint32 writeAuthSection(uint32 inSectionOffset);
    uint32 writeSchemaSection(uint32 inSectionOffset);
	
private:
	
	/* mDbVersion is the current DbVersion of this database before any changes
       we are going to make.  mNotifyCount holds the value of gNotifyCount at
       the time mDbVersion was created.  mDbLastRead is the time at which we
       last checked if the file from which mDbVersion was read has changed.
       mDbVersionLock protects the other 3 fields.  */
	RefPointer<const DbVersion> mDbVersion;
    int32_t mNotifyCount;
    CFAbsoluteTime mDbLastRead;
	Mutex mDbVersionLock;

    AtomicFile &mAtomicFile;
    uint32 mVersionId;
	RefPointer<AtomicTempFile> mAtomicTempFile;

    typedef map<Table::Id, ModifiedTable *> ModifiedTableMap;
    ModifiedTableMap mModifiedTableMap;
	
	const class AppleDatabase &mDb;
};

//
// AppleDatabaseManager
//
class AppleDatabaseManager : public DatabaseManager
{
public:
	AppleDatabaseManager(const AppleDatabaseTableName *tableNames);
    Database *make(const DbName &inDbName);
	
protected:
	const AppleDatabaseTableName *mTableNames;
};

//
// AppleDbContext
//
class AppleDbContext : public DbContext
{
public:
	AppleDbContext(Database &inDatabase,
				   DatabaseSession &inDatabaseSession,
				   CSSM_DB_ACCESS_TYPE inAccessRequest,
				   const AccessCredentials *inAccessCred,
				   const void *inOpenParameters);
	virtual ~AppleDbContext();
	bool autoCommit() const { return mAutoCommit; }
	void autoCommit(bool on) { mAutoCommit = on; }
	mode_t mode() const { return mMode; }

private:
	bool mAutoCommit;
	mode_t mMode;
};

//
// AppleDatabase
//
class AppleDatabase : public Database
{
public:
    AppleDatabase(const DbName &inDbName, const AppleDatabaseTableName *tableNames);
    virtual ~AppleDatabase();

    virtual void
        dbCreate(DbContext &inDbContext, const CSSM_DBINFO &inDBInfo,
                 const CSSM_ACL_ENTRY_INPUT *inInitialAclEntry);

    virtual void
        dbOpen(DbContext &inDbContext);

    virtual void
        dbClose();

    virtual void
        dbDelete(DatabaseSession &inDatabaseSession,
                 const AccessCredentials *inAccessCred);

    virtual void
        createRelation(DbContext &inDbContext,
                       CSSM_DB_RECORDTYPE inRelationID,
                       const char *inRelationName,
                       uint32 inNumberOfAttributes,
                       const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo,
                       uint32 inNumberOfIndexes,
                       const CSSM_DB_SCHEMA_INDEX_INFO &inIndexInfo);

    virtual void
        destroyRelation(DbContext &inDbContext,
                        CSSM_DB_RECORDTYPE inRelationID);

    virtual void
        authenticate(DbContext &inDbContext,
                     CSSM_DB_ACCESS_TYPE inAccessRequest,
                     const AccessCredentials &inAccessCred);

    virtual void
        getDbAcl(DbContext &inDbContext,
                 const CSSM_STRING *inSelectionTag,
                 uint32 &outNumberOfAclInfos,
                 CSSM_ACL_ENTRY_INFO_PTR &outAclInfos);

    virtual void
        changeDbAcl(DbContext &inDbContext,
                    const AccessCredentials &inAccessCred,
                    const CSSM_ACL_EDIT &inAclEdit);

    virtual void
        getDbOwner(DbContext &inDbContext, CSSM_ACL_OWNER_PROTOTYPE &outOwner);

    virtual void
        changeDbOwner(DbContext &inDbContext,
                      const AccessCredentials &inAccessCred,
                      const CSSM_ACL_OWNER_PROTOTYPE &inNewOwner);

    virtual char *
        getDbNameFromHandle(const DbContext &inDbContext) const;

    virtual CSSM_DB_UNIQUE_RECORD_PTR
        dataInsert(DbContext &inDbContext,
                   CSSM_DB_RECORDTYPE RecordType,
                   const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
                   const CssmData *inData);

    virtual void
        dataDelete(DbContext &inDbContext,
                   const CSSM_DB_UNIQUE_RECORD &inUniqueRecordIdentifier);

    virtual void
        dataModify(DbContext &inDbContext,
                   CSSM_DB_RECORDTYPE inRecordType,
                   CSSM_DB_UNIQUE_RECORD &inoutUniqueRecordIdentifier,
                   const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributesToBeModified,
                   const CssmData *inDataToBeModified,
                   CSSM_DB_MODIFY_MODE inModifyMode);

    virtual CSSM_HANDLE
        dataGetFirst(DbContext &inDbContext,
                     const CssmQuery *inQuery,
                     CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                     CssmData *inoutData,
                     CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord);

    virtual bool
        dataGetNext(DbContext &inDbContext,
                    CSSM_HANDLE inResultsHandle,
                    CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                    CssmData *inoutData,
                    CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord);

    virtual void
        dataAbortQuery(DbContext &inDbContext,
                       CSSM_HANDLE inResultsHandle);

    virtual void
        dataGetFromUniqueRecordId(DbContext &inDbContext,
                                  const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
                                  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                                  CssmData *inoutData);

    virtual void
        freeUniqueRecord(DbContext &inDbContext,
                         CSSM_DB_UNIQUE_RECORD &inUniqueRecord);
						 
	virtual void passThrough(DbContext &dbContext,
							 uint32 passThroughId,
							 const void *inputParams,
							 void **outputParams);

    // Subclasses must implement this method.
    virtual DbContext *makeDbContext(DatabaseSession &inDatabaseSession,
                                     CSSM_DB_ACCESS_TYPE inAccessRequest,
                                     const AccessCredentials *inAccessCred,
                                     const void *inOpenParameters);
									 
	const CssmDbRecordAttributeInfo schemaRelations;
	const CssmDbRecordAttributeInfo schemaAttributes;
	const CssmDbRecordAttributeInfo schemaIndexes;
	const CssmDbRecordAttributeInfo schemaParsingModule;

	const char *recordName(CSSM_DB_RECORDTYPE inRecordType) const;

private:
	static void
	updateUniqueRecord(DbContext &inDbContext,
									CSSM_DB_RECORDTYPE inTableId,
									const RecordId &inRecordId,
									CSSM_DB_UNIQUE_RECORD &inoutUniqueRecord);

	CSSM_DB_UNIQUE_RECORD_PTR
	createUniqueRecord(DbContext &inDbContext, CSSM_DB_RECORDTYPE inTableId,
					   const RecordId &inRecordId);
	const RecordId parseUniqueRecord(const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
									 CSSM_DB_RECORDTYPE &outTableId);

    Mutex mWriteLock;
    AtomicFile mAtomicFile;
	DbModifier mDbModifier;
	const AppleDatabaseTableName *mTableNames;
};

} // end namespace Security

#endif //_H_APPLEDATABASE
