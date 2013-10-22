/*
 * Copyright (c) 2000-2001, 2003 Apple Computer, Inc. All Rights Reserved.
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
//  AppleDatabase.cpp - Description t.b.d.
//
#include "AppleDatabase.h"
#include <security_cdsa_plugin/DatabaseSession.h>
#include <security_cdsa_plugin/DbContext.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <Security/cssmapple.h>
#include <security_utilities/trackingallocator.h>
#include <security_utilities/logging.h>
#include <fcntl.h>
#include <memory>
#include <libkern/OSAtomic.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <Security/cssmapplePriv.h>
#include <syslog.h>

static const char *kAppleDatabaseChanged = "com.apple.AppleDatabaseChanged";

/* Number of seconds after which we open/pread/close a db to check it's
   version number even if we didn't get any notifications.  Note that we always
   check just after we take a write lock and whenever we get a notification
   that any db on the system has changed. */
static const CFTimeInterval kForceReReadTime = 15.0;

/* Token on which we receive notifications and the pthread_once_t protecting
   it's initialization. */
pthread_once_t gCommonInitMutex = PTHREAD_ONCE_INIT;

/* Global counter of how many notifications we have received and a lock to
   protect the counter. */
static int kSegmentSize = 4;
int32_t* gSegment = NULL;

/* Registration routine for notifcations. Called inside a pthread_once(). */
static void initCommon(void)
{
	// open the file
	int segmentDescriptor = shm_open (kAppleDatabaseChanged, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
	if (segmentDescriptor < 0)
	{
		return;
	}
	
	// set the segment size
	ftruncate (segmentDescriptor, kSegmentSize);
	
	// map it into memory
	int32_t* tmp = (int32_t*) mmap (NULL, kSegmentSize, PROT_READ | PROT_WRITE, MAP_SHARED, segmentDescriptor, 0);
	close (segmentDescriptor);

	if (tmp == (int32_t*) -1) // can't map the memory?
	{
		gSegment = NULL;
	}
	else
	{
		gSegment = tmp;
	}
}

//
// Table
//
Table::Table(const ReadSection &inTableSection) :
	mMetaRecord(inTableSection[OffsetId]),
	mTableSection(inTableSection),
	mRecordsCount(inTableSection[OffsetRecordsCount]),
	mFreeListHead(inTableSection[OffsetFreeListHead]),
	mRecordNumbersCount(inTableSection[OffsetRecordNumbersCount])
{
	// can't easily initialize indexes here, since meta record is incomplete
	// until much later... see DbVersion::open()
}

Table::~Table() 
{
	for_each_map_delete(mIndexMap.begin(), mIndexMap.end());
}

void
Table::readIndexSection()
{
	uint32 indexSectionOffset = mTableSection.at(OffsetIndexesOffset);

	uint32 numIndexes = mTableSection.at(indexSectionOffset + AtomSize);

	for (uint32 i = 0; i < numIndexes; i++) {
		uint32 indexOffset = mTableSection.at(indexSectionOffset + (i + 2) * AtomSize);
		ReadSection indexSection(mTableSection.subsection(indexOffset));
	
		auto_ptr<DbConstIndex> index(new DbConstIndex(*this, indexSection));
		mIndexMap.insert(ConstIndexMap::value_type(index->indexId(), index.get()));
		index.release();
	}
}

Cursor *
Table::createCursor(const CSSM_QUERY *inQuery, const DbVersion &inDbVersion) const
{
	// if an index matches the query, return a cursor which uses the index

	ConstIndexMap::const_iterator it;
	DbQueryKey *queryKey;
	
	for (it = mIndexMap.begin(); it != mIndexMap.end(); it++)
		if (it->second->matchesQuery(*inQuery, queryKey)) {
			IndexCursor *cursor = new IndexCursor(queryKey, inDbVersion, *this, it->second);
			return cursor;
		}

	// otherwise, return a cursor that iterates over all table records

	return new LinearCursor(inQuery, inDbVersion, *this);
}

const ReadSection
Table::getRecordSection(uint32 inRecordNumber) const
{
	if (inRecordNumber >= mRecordNumbersCount)
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);

	uint32 aRecordOffset = mTableSection[OffsetRecordNumbers + AtomSize
										 * inRecordNumber];

	// Check if this RecordNumber has been deleted.
	if (aRecordOffset & 1 || aRecordOffset == 0)
		CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND);

	return MetaRecord::readSection(mTableSection, aRecordOffset);
}

const RecordId
Table::getRecord(const RecordId &inRecordId,
				 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
				 CssmData *inoutData,
				 Allocator &inAllocator) const
{
	const ReadSection aRecordSection = getRecordSection(inRecordId.mRecordNumber);
	const RecordId aRecordId = MetaRecord::unpackRecordId(aRecordSection);

	// Make sure the RecordNumber matches that in the RecordId we just retrived.
	if (aRecordId.mRecordNumber != inRecordId.mRecordNumber)
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	if (aRecordId.mCreateVersion != inRecordId.mCreateVersion)
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);

	// XXX Figure out which value to pass for inQueryFlags (5th) argument
	mMetaRecord.unpackRecord(aRecordSection, inAllocator, inoutAttributes,
							 inoutData, 0);
	return aRecordId;
}

uint32
Table::popFreeList(uint32 &aFreeListHead) const
{
	assert(aFreeListHead | 1);
	uint32 anOffset = aFreeListHead ^ 1;
	uint32 aRecordNumber = (anOffset - OffsetRecordNumbers) / AtomSize;
	aFreeListHead = mTableSection[anOffset];
	return aRecordNumber;
}

const ReadSection
Table::getRecordsSection() const
{
	return mTableSection.subsection(mTableSection[OffsetRecords]);
}

bool
Table::matchesTableId(Id inTableId) const
{
	Id anId = mMetaRecord.dataRecordType();
	if (inTableId == CSSM_DL_DB_RECORD_ANY) // All non schema tables.
		return !(CSSM_DB_RECORDTYPE_SCHEMA_START <= anId
				 && anId < CSSM_DB_RECORDTYPE_SCHEMA_END);

	if (inTableId == CSSM_DL_DB_RECORD_ALL_KEYS) // All key tables.
		return (anId == CSSM_DL_DB_RECORD_PUBLIC_KEY
				|| anId == CSSM_DL_DB_RECORD_PRIVATE_KEY
				|| anId == CSSM_DL_DB_RECORD_SYMMETRIC_KEY);

	return inTableId == anId; // Only if exact match.
}


//
// ModifiedTable
//
ModifiedTable::ModifiedTable(const Table *inTable) :
	mTable(inTable),
	mNewMetaRecord(nil),
	mRecordNumberCount(inTable->recordNumberCount()),
	mFreeListHead(inTable->freeListHead()),
	mIsModified(false)
{
}

ModifiedTable::ModifiedTable(MetaRecord *inMetaRecord) :
	mTable(nil),
	mNewMetaRecord(inMetaRecord),
	mRecordNumberCount(0),
	mFreeListHead(0),
	mIsModified(true)
{
}

ModifiedTable::~ModifiedTable()
{
	for_each_map_delete(mIndexMap.begin(), mIndexMap.end());
	for_each_map_delete(mInsertedMap.begin(), mInsertedMap.end());

	delete mNewMetaRecord;
}

void
ModifiedTable::deleteRecord(const RecordId &inRecordId)
{
	modifyTable();

    uint32 aRecordNumber = inRecordId.mRecordNumber;
	
	// remove the record from all the indexes
	MutableIndexMap::iterator it;
	for (it = mIndexMap.begin(); it != mIndexMap.end(); it++)
		it->second->removeRecord(aRecordNumber);

	InsertedMap::iterator anIt = mInsertedMap.find(aRecordNumber);
	if (anIt == mInsertedMap.end())
	{
		// If we have no old table than this record can not exist yet.
		if (!mTable)
			CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND);

#if RECORDVERSIONCHECK
		const RecordId aRecordId = MetaRecord::unpackRecordId(mTable->getRecordSection(aRecordNumber));
		if (aRecordId.mRecordVersion != inRecordId.mRecordVersion)
			CssmError::throwMe(CSSMERR_DL_RECORD_MODIFIED);
#endif

		// Schedule the record for deletion
		if (!mDeletedSet.insert(aRecordNumber).second)
			CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND); // It was already deleted
	}
	else
	{
		const RecordId aRecordId = MetaRecord::unpackRecordId(*anIt->second);
		if (aRecordId.mCreateVersion != inRecordId.mCreateVersion)
			CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND);

#if RECORDVERSIONCHECK
		if (aRecordId.mRecordVersion != inRecordId.mRecordVersion)
			CssmError::throwMe(CSSMERR_DL_RECORD_MODIFIED);
#endif

		// Remove the inserted (but uncommited) record.  It should already be in mDeletedSet
		// if it existed previously in mTable.
        delete anIt->second;		
		mInsertedMap.erase(anIt);
	}
}

const RecordId
ModifiedTable::insertRecord(uint32 inVersionId,
							const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
							const CssmData *inData)
{
	modifyTable();
	
	auto_ptr<WriteSection> aWriteSection(new WriteSection());
	getMetaRecord().packRecord(*aWriteSection, inAttributes, inData);
    uint32 aRecordNumber = nextRecordNumber();
	
	// add the record to all the indexes; this will throw if the new record
	// violates a unique index
	MutableIndexMap::iterator it;
	for (it = mIndexMap.begin(); it != mIndexMap.end(); it++)
		it->second->insertRecord(aRecordNumber, *(aWriteSection.get()));

	// schedule the record for insertion
	RecordId aRecordId(aRecordNumber, inVersionId);
	MetaRecord::packRecordId(aRecordId, *aWriteSection);
    mInsertedMap.insert(InsertedMap::value_type(aRecordNumber, aWriteSection.get()));

    aWriteSection.release();
	
    return aRecordId;
}

const RecordId
ModifiedTable::updateRecord(const RecordId &inRecordId,
							const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
							const CssmData *inData,
							CSSM_DB_MODIFY_MODE inModifyMode)
{
	modifyTable();

    uint32 aRecordNumber = inRecordId.mRecordNumber;
	InsertedMap::iterator anIt = mInsertedMap.find(aRecordNumber);

	// aReUpdate is true iff we are updating an already updated record.
	bool aReUpdate = anIt != mInsertedMap.end();

	// If we are not re-updating and there is no old table than this record does not exist yet.
	if (!aReUpdate && !mTable)
		CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND);

	const ReadSection &anOldDbRecord = aReUpdate ? *anIt->second : mTable->getRecordSection(aRecordNumber);
	const RecordId aRecordId = MetaRecord::unpackRecordId(anOldDbRecord);

	// Did someone else delete the record we are trying to update.
	if (aRecordId.mCreateVersion != inRecordId.mCreateVersion)
		CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND);

#if RECORDVERSIONCHECK
	// Is the record we that our update is based on current?
	if (aRecordId.mRecordVersion != inRecordId.mRecordVersion)
		CssmError::throwMe(CSSMERR_DL_STALE_UNIQUE_RECORD);
#endif

	// Update the actual packed record.
    auto_ptr<WriteSection> aDbRecord(new WriteSection());
	getMetaRecord().updateRecord(anOldDbRecord, *aDbRecord,
		CssmDbRecordAttributeData::overlay(inAttributes), inData, inModifyMode);


	// Bump the RecordVersion of this record.
	RecordId aNewRecordId(aRecordNumber, inRecordId.mCreateVersion, inRecordId.mRecordVersion + 1);
	// Store the RecordVersion in the packed aDbRecord.
	MetaRecord::packRecordId(aNewRecordId, *aDbRecord);

	if (!aReUpdate && !mDeletedSet.insert(aRecordNumber).second)
		CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND); // Record was already in mDeletedSet

	// remove the original record from all the indexes
	MutableIndexMap::iterator it;
	for (it = mIndexMap.begin(); it != mIndexMap.end(); it++)
		it->second->removeRecord(aRecordNumber);

	try
	{
		// Add the updated record to all the indexes; this will throw if the new record
		// violates a unique index
		for (it = mIndexMap.begin(); it != mIndexMap.end(); it++)
			it->second->insertRecord(aRecordNumber, *(aDbRecord.get()));

		if (aReUpdate)
		{
			// Get rid of anOldDbRecord from the inserted map and replace it
			// with aDbRecord.
			delete anIt->second;
			anIt->second = aDbRecord.get();
		}
		else
		{
			// First time though so let's just put the new value in the map.
			mInsertedMap.insert(InsertedMap::value_type(aRecordNumber, aDbRecord.get()));
		}
		aDbRecord.release();
	}
	catch(...)
	{
		// We only remove aRecordNumber from mDeletedSet if we added it above.
		if (!aReUpdate)
			mDeletedSet.erase(aRecordNumber);

		// The 2 operations below are an attempt to preserve the indices when
		// an insert fails.

		// Remove the updated record from all the indexes
		MutableIndexMap::iterator it;
		for (it = mIndexMap.begin(); it != mIndexMap.end(); it++)
			it->second->removeRecord(aRecordNumber);

		// Add the original record back to all the indexes
		for (it = mIndexMap.begin(); it != mIndexMap.end(); it++)
			it->second->insertRecord(aRecordNumber, anOldDbRecord);

		throw;
	}

	return aNewRecordId;
}

const RecordId
ModifiedTable::getRecord(const RecordId &inRecordId,
						 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
						 CssmData *inoutData,
						 Allocator &inAllocator) const
{
	if (mIsModified)
	{
		uint32 aRecordNumber = inRecordId.mRecordNumber;
		InsertedMap::const_iterator anIt = mInsertedMap.find(aRecordNumber);
		if (anIt != mInsertedMap.end())
		{
			// We found the record in mInsertedMap so we use the inserted
			// record.
			const ReadSection &aRecordSection = *(anIt->second);
			const RecordId aRecordId = MetaRecord::unpackRecordId(aRecordSection);

			// Make sure the RecordNumber matches that in the RecordId we just retrived.
			if (aRecordId.mRecordNumber != aRecordNumber)
				CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

			if (aRecordId.mCreateVersion != inRecordId.mCreateVersion)
				CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);

			// XXX Figure out which value to pass for inQueryFlags (5th) argument
			getMetaRecord().unpackRecord(aRecordSection, inAllocator,
				inoutAttributes, inoutData, 0);

			return aRecordId;
		}
		else if (mDeletedSet.find(aRecordNumber) != mDeletedSet.end())
		{
			 // If aRecordNumber was not in mInsertedMap but it was in
			 // mDeletedSet then it was deleted but not yet commited.
			CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND);
		}
	}

	if (!mTable)
		CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND);

	// Either this table wasn't modified yet or we didn't find aRecordNumber in
	// mInsertedMap nor mDeletedSet so just ask mTable for it.
	return mTable->getRecord(inRecordId, inoutAttributes, inoutData,
		inAllocator);
}

uint32
ModifiedTable::nextRecordNumber()
{
	// If we still have unused free records in mTable get the next one.
	if (mFreeListHead)
		return mTable->popFreeList(mFreeListHead);

	// Bump up the mRecordNumberCount so we don't reuse the same one.
	return mRecordNumberCount++;
}

uint32
ModifiedTable::recordNumberCount() const
{
	uint32 anOldMax = !mTable ? 0 : mTable->recordNumberCount() - 1;
	uint32 anInsertedMax = mInsertedMap.empty() ? 0 : mInsertedMap.rbegin()->first;

	DeletedSet::reverse_iterator anIt = mDeletedSet.rbegin();
	DeletedSet::reverse_iterator anEnd = mDeletedSet.rend();
	for (; anIt != anEnd; anIt++)
	{
		if (*anIt != anOldMax || anOldMax <= anInsertedMax)
			break;
		anOldMax--;
 	}

	return max(anOldMax,anInsertedMax) + 1;
}

const MetaRecord &
ModifiedTable::getMetaRecord() const
{
	return mNewMetaRecord ? *mNewMetaRecord : mTable->getMetaRecord();
}

// prepare to modify the table

void
ModifiedTable::modifyTable()
{
	if (!mIsModified) {
		createMutableIndexes();
		mIsModified = true;
	}
}

// create mutable indexes from the read-only indexes in the underlying table

void
ModifiedTable::createMutableIndexes()
{
	if (mTable == NULL)
		return;
	
	Table::ConstIndexMap::const_iterator it;
	for (it = mTable->mIndexMap.begin(); it != mTable->mIndexMap.end(); it++) {
		auto_ptr<DbMutableIndex> mutableIndex(new DbMutableIndex(*it->second));
		mIndexMap.insert(MutableIndexMap::value_type(it->first, mutableIndex.get()));
		mutableIndex.release();
	}
}

// find, and create if needed, an index with the given id

DbMutableIndex &
ModifiedTable::findIndex(uint32 indexId, const MetaRecord &metaRecord, bool isUniqueIndex)
{
	MutableIndexMap::iterator it = mIndexMap.find(indexId);

	if (it == mIndexMap.end()) {
		// create the new index
		auto_ptr<DbMutableIndex> index(new DbMutableIndex(metaRecord, indexId, isUniqueIndex));
		it = mIndexMap.insert(MutableIndexMap::value_type(indexId, index.get())).first;
		index.release();
	}

	return *it->second;
}

uint32
ModifiedTable::writeIndexSection(WriteSection &tableSection, uint32 offset)
{
	MutableIndexMap::iterator it;
	
	tableSection.put(Table::OffsetIndexesOffset, offset);
	
	// leave room for the size, to be written later
	uint32 indexSectionOffset = offset;
	offset += AtomSize;
	
	offset = tableSection.put(offset, (uint32)mIndexMap.size());
	
	// leave room for the array of offsets to the indexes
	uint32 indexOffsetOffset = offset;
	offset += mIndexMap.size() * AtomSize;
	
	// write the indexes
	for (it = mIndexMap.begin(); it != mIndexMap.end(); it++) {
		indexOffsetOffset = tableSection.put(indexOffsetOffset, offset);
		offset = it->second->writeIndex(tableSection, offset);
	}
	
	// write the total index section size
	tableSection.put(indexSectionOffset, offset - indexSectionOffset);

	return offset;
}

uint32
ModifiedTable::writeTable(AtomicTempFile &inAtomicTempFile, uint32 inSectionOffset)
{
	if (mTable && !mIsModified) {
		// the table has not been modified, so we can just dump the old table
		// section into the new database

		const ReadSection &tableSection = mTable->getTableSection();
		uint32 tableSize = tableSection.at(Table::OffsetSize);
		
		inAtomicTempFile.write(AtomicFile::FromStart, inSectionOffset,
			tableSection.range(Range(0, tableSize)), tableSize);

		return inSectionOffset + tableSize;
	}

	// We should have an old mTable or a mNewMetaRecord but not both.
	assert(mTable != nil ^ mNewMetaRecord != nil);
	const MetaRecord &aNewMetaRecord = getMetaRecord();
	
	uint32 aRecordsCount = 0;
	uint32 aRecordNumbersCount = recordNumberCount();
	uint32 aRecordsOffset = Table::OffsetRecordNumbers + AtomSize * aRecordNumbersCount;
	WriteSection aTableSection(Allocator::standard(), aRecordsOffset);
	aTableSection.size(aRecordsOffset);
	aTableSection.put(Table::OffsetId, aNewMetaRecord.dataRecordType());
	aTableSection.put(Table::OffsetRecords, aRecordsOffset);
	aTableSection.put(Table::OffsetRecordNumbersCount, aRecordNumbersCount);
	
	uint32 anOffset = inSectionOffset + aRecordsOffset;

	if (mTable)
	{
		// XXX Handle schema changes in the future.
		assert(mNewMetaRecord == nil);
		
		// We have a modified old table so copy all non deleted records
		// The code below is rather elaborate, but this is because it attempts
		// to copy large ranges of non deleted records with single calls
		// to AtomicFile::write()
		uint32 anOldRecordsCount = mTable->getRecordsCount();
		ReadSection aRecordsSection = mTable->getRecordsSection();
		uint32 aReadOffset = 0;					// Offset of current record
		uint32 aWriteOffset = aRecordsOffset;	// Offset for current write record
		uint32 aBlockStart = aReadOffset;		// Starting point for read 
		uint32 aBlockSize = 0;					// Size of block to read
		for (uint32 aRecord = 0; aRecord < anOldRecordsCount; aRecord++)
		{
			ReadSection aRecordSection = MetaRecord::readSection(aRecordsSection, aReadOffset);
			uint32 aRecordNumber = MetaRecord::unpackRecordNumber(aRecordSection);
			uint32 aRecordSize = aRecordSection.size();
			aReadOffset += aRecordSize;
			if (mDeletedSet.find(aRecordNumber) == mDeletedSet.end())
			{
				// This record has not been deleted.  Register the offset
				// at which it will be in the new file in aTableSection.
				aTableSection.put(Table::OffsetRecordNumbers
								  + AtomSize * aRecordNumber,
								  aWriteOffset);
				aWriteOffset += aRecordSize;
				aBlockSize += aRecordSize;
				aRecordsCount++;
				// XXX update all indexes being created.
			}
			else
			{
				// The current record has been deleted.  Copy all records up
				// to but not including the current one to the new file.
				if (aBlockSize > 0)
				{
					inAtomicTempFile.write(AtomicFile::FromStart, anOffset,
									   aRecordsSection.range(Range(aBlockStart,
																   aBlockSize)),
									   aBlockSize);
					anOffset += aBlockSize;
				}

				// Set the start of the next block to the start of the next
				// record, and the size of the block to 0.
				aBlockStart = aReadOffset;
				aBlockSize = 0;
			} // if (mDeletedSet..)
		} // for (aRecord...)

		// Copy all records that have not yet been copied to the new file.
		if (aBlockSize > 0)
		{
			inAtomicTempFile.write(AtomicFile::FromStart, anOffset,
							   aRecordsSection.range(Range(aBlockStart,
														   aBlockSize)),
							   aBlockSize);
			anOffset += aBlockSize;
		}
	} // if (mTable)

	// Now add all inserted records to the table.
	InsertedMap::const_iterator anIt = mInsertedMap.begin();
	InsertedMap::const_iterator anEnd = mInsertedMap.end();
	// Iterate over all inserted objects.
	for (; anIt != anEnd; anIt++)
	{
		// Write out each inserted/modified record
		const WriteSection &aRecord = *anIt->second;
		uint32 aRecordNumber = anIt->first;
		// Put offset relative to start of this table in recordNumber array.
		aTableSection.put(Table::OffsetRecordNumbers + AtomSize * aRecordNumber,
						  anOffset - inSectionOffset);
		inAtomicTempFile.write(AtomicFile::FromStart, anOffset,
						   aRecord.address(), aRecord.size());
		anOffset += aRecord.size();
		aRecordsCount++;
		// XXX update all indexes being created.
	}

	// Reconstruct the freelist (this is O(N) where N is the number of recordNumbers)
	// We could implement it faster by using the old freelist and skipping the records
	// that have been inserted.  However building the freelist for the newly used
	// recordNumbers (not in mTable) would look like the code below anyway (starting
	// from mTable->recordNumberCount()).
	// The first part of this would be O(M Log(N))  (where M is the old number of
	// free records, and N is the number of newly inserted records)
	// The second part would be O(N) where N is the currently max RecordNumber
	// in use - the old max RecordNumber in use.
	uint32 aFreeListHead = 0;	// Link to previous free record
	for (uint32 aRecordNumber = 0; aRecordNumber < aRecordNumbersCount; aRecordNumber++)
	{
		// Make the freelist a list of all records with 0 offset (non existing).
		if (!aTableSection.at(Table::OffsetRecordNumbers + AtomSize * aRecordNumber))
		{
			aTableSection.put(Table::OffsetRecordNumbers
								+ AtomSize * aRecordNumber,
								aFreeListHead);
			// Make aFreeListHead point to the previous free recordNumber slot in the table.
			aFreeListHead = (Table::OffsetRecordNumbers + AtomSize * aRecordNumber) | 1;
		}
	}
	aTableSection.put(Table::OffsetFreeListHead, aFreeListHead);

	anOffset -= inSectionOffset;
	
	// Write out indexes, which are part of the table section

	{	
		uint32 indexOffset = anOffset;
		anOffset = writeIndexSection(aTableSection, anOffset);
		inAtomicTempFile.write(AtomicFile::FromStart, inSectionOffset + indexOffset,
			aTableSection.address() + indexOffset, anOffset - indexOffset);
	}

	// Set the section size and recordCount.
	aTableSection.put(Table::OffsetSize, anOffset);
	aTableSection.put(Table::OffsetRecordsCount, aRecordsCount);

	// Write out aTableSection header.
	inAtomicTempFile.write(AtomicFile::FromStart, inSectionOffset,
					   aTableSection.address(), aTableSection.size());

    return anOffset + inSectionOffset;
}



//
// Metadata
//

// Attribute definitions

static const CSSM_DB_ATTRIBUTE_INFO RelationID =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "RelationID"},
    CSSM_DB_ATTRIBUTE_FORMAT_UINT32
};
static const CSSM_DB_ATTRIBUTE_INFO RelationName =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "RelationName"},
    CSSM_DB_ATTRIBUTE_FORMAT_STRING
};
static const CSSM_DB_ATTRIBUTE_INFO AttributeID =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "AttributeID"},
    CSSM_DB_ATTRIBUTE_FORMAT_UINT32
};
static const CSSM_DB_ATTRIBUTE_INFO AttributeNameFormat =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "AttributeNameFormat"},
    CSSM_DB_ATTRIBUTE_FORMAT_UINT32
};
static const CSSM_DB_ATTRIBUTE_INFO AttributeName =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "AttributeName"},
    CSSM_DB_ATTRIBUTE_FORMAT_STRING
};
static const CSSM_DB_ATTRIBUTE_INFO AttributeNameID =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "AttributeNameID"},
    CSSM_DB_ATTRIBUTE_FORMAT_BLOB
};
static const CSSM_DB_ATTRIBUTE_INFO AttributeFormat =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "AttributeFormat"},
    CSSM_DB_ATTRIBUTE_FORMAT_UINT32
};
static const CSSM_DB_ATTRIBUTE_INFO IndexID =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "IndexID"},
    CSSM_DB_ATTRIBUTE_FORMAT_UINT32
};
static const CSSM_DB_ATTRIBUTE_INFO IndexType =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "IndexType"},
    CSSM_DB_ATTRIBUTE_FORMAT_UINT32
};
static const CSSM_DB_ATTRIBUTE_INFO IndexedDataLocation =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "IndexedDataLocation"},
    CSSM_DB_ATTRIBUTE_FORMAT_UINT32
};
static const CSSM_DB_ATTRIBUTE_INFO ModuleID =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "ModuleID"},
    CSSM_DB_ATTRIBUTE_FORMAT_BLOB
};
static const CSSM_DB_ATTRIBUTE_INFO AddinVersion =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "AddinVersion"},
    CSSM_DB_ATTRIBUTE_FORMAT_STRING
};
static const CSSM_DB_ATTRIBUTE_INFO SSID =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "SSID"},
    CSSM_DB_ATTRIBUTE_FORMAT_UINT32
};
static const CSSM_DB_ATTRIBUTE_INFO SubserviceType =
{
    CSSM_DB_ATTRIBUTE_NAME_AS_STRING,
    {(char*) "SubserviceType"},
    CSSM_DB_ATTRIBUTE_FORMAT_UINT32
};

#define ATTRIBUTE(type, name) \
	{ CSSM_DB_ATTRIBUTE_NAME_AS_STRING, { (char*) #name }, CSSM_DB_ATTRIBUTE_FORMAT_ ## type }
	
static const CSSM_DB_ATTRIBUTE_INFO AttrSchemaRelations[] =
{
	//RelationID, RelationName
	ATTRIBUTE(UINT32, RelationID),
	ATTRIBUTE(STRING, RelationName)
};

static const CSSM_DB_ATTRIBUTE_INFO AttrSchemaAttributes[] =
{
	//RelationID, AttributeID,
    //AttributeNameFormat, AttributeName, AttributeNameID,
    //AttributeFormat
	ATTRIBUTE(UINT32, RelationID),
	ATTRIBUTE(UINT32, AttributeID),
	ATTRIBUTE(UINT32, AttributeNameFormat),
	ATTRIBUTE(STRING, AttributeName),
	ATTRIBUTE(BLOB, AttributeNameID),
	ATTRIBUTE(UINT32, AttributeFormat)
};

static const CSSM_DB_ATTRIBUTE_INFO AttrSchemaIndexes[] =
{
	ATTRIBUTE(UINT32, RelationID),
	ATTRIBUTE(UINT32, IndexID),
	ATTRIBUTE(UINT32, AttributeID),
	ATTRIBUTE(UINT32, IndexType),
	ATTRIBUTE(UINT32, IndexedDataLocation)
    //RelationID, IndexID, AttributeID,
    //IndexType, IndexedDataLocation
};

static const CSSM_DB_ATTRIBUTE_INFO AttrSchemaParsingModule[] =
{
	ATTRIBUTE(UINT32, RelationID),
	ATTRIBUTE(UINT32, AttributeID),
	ATTRIBUTE(BLOB, ModuleID),
	ATTRIBUTE(STRING, AddinVersion),
	ATTRIBUTE(UINT32, SSID),
	ATTRIBUTE(UINT32, SubserviceType)
    //RelationID, AttributeID,
    //ModuleID, AddinVersion, SSID, SubserviceType
};

#undef ATTRIBUTE

//
// DbVersion
//
DbVersion::DbVersion(const AppleDatabase &db, const RefPointer <AtomicBufferedFile> &inAtomicBufferedFile) :
	mDatabase(reinterpret_cast<const uint8 *>(NULL), 0),
	mDb(db),
	mBufferedFile(inAtomicBufferedFile)
{
	off_t aLength = mBufferedFile->length();
	off_t bytesRead = 0;
	const uint8 *ptr = mBufferedFile->read(0, aLength, bytesRead);
	mBufferedFile->close();
	mDatabase = ReadSection(ptr, (size_t)bytesRead);
	open();
}

DbVersion::~DbVersion()
{
	try
	{
		for_each_map_delete(mTableMap.begin(), mTableMap.end());
	}
	catch(...) {}
}

void
DbVersion::open()
{
	try
	{
		// This is the oposite of DbModifier::commit()
		mVersionId = mDatabase[mDatabase.size() - AtomSize];

		const ReadSection aHeaderSection = mDatabase.subsection(HeaderOffset,
																HeaderSize);
		if (aHeaderSection.at(OffsetMagic) != HeaderMagic)
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	
		// We currently only support one version.  If we support additional
		// file format versions in the future fix this.
		uint32 aVersion = aHeaderSection.at(OffsetVersion);
		if (aVersion != HeaderVersion)
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	
		//const ReadSection anAuthSection =
		//	mDatabase.subsection(HeaderOffset + aHeaderSection.at(OffsetAuthOffset));
		// XXX Do something with anAuthSection.
	
		uint32 aSchemaOffset = aHeaderSection.at(OffsetSchemaOffset);
		const ReadSection aSchemaSection =
			mDatabase.subsection(HeaderOffset + aSchemaOffset);
	
		uint32 aSchemaSize = aSchemaSection[OffsetSchemaSize];
		// Make sure that the given range exists.
		aSchemaSection.subsection(0, aSchemaSize);
		uint32 aTableCount = aSchemaSection[OffsetTablesCount];
	
		// Assert that the size of this section is big enough.
		if (aSchemaSize < OffsetTables + AtomSize * aTableCount)
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	
		for (uint32 aTableNumber = 0; aTableNumber < aTableCount;
			 aTableNumber++)
		{
			uint32 aTableOffset = aSchemaSection.at(OffsetTables + AtomSize
													* aTableNumber);
			// XXX Set the size boundary on aTableSection.
			const ReadSection aTableSection =
				aSchemaSection.subsection(aTableOffset);
			auto_ptr<Table> aTable(new Table(aTableSection));
			Table::Id aTableId = aTable->getMetaRecord().dataRecordType();
			mTableMap.insert(TableMap::value_type(aTableId, aTable.get()));
			aTable.release();
		}

		// Fill in the schema for the meta tables.
		
		findTable(mDb.schemaRelations.DataRecordType).getMetaRecord().
			setRecordAttributeInfo(mDb.schemaRelations);
		findTable(mDb.schemaIndexes.DataRecordType).getMetaRecord().
			setRecordAttributeInfo(mDb.schemaIndexes);
		findTable(mDb.schemaParsingModule.DataRecordType).getMetaRecord().
			setRecordAttributeInfo(mDb.schemaParsingModule);

		// OK, we have created all the tables in the tableMap.  Now
		// lets read the schema and proccess it accordingly.
		// Iterate over all schema records.
		Table &aTable = findTable(mDb.schemaAttributes.DataRecordType);
		aTable.getMetaRecord().setRecordAttributeInfo(mDb.schemaAttributes);
		uint32 aRecordsCount = aTable.getRecordsCount();
		ReadSection aRecordsSection = aTable.getRecordsSection();
		uint32 aReadOffset = 0;
		const MetaRecord &aMetaRecord = aTable.getMetaRecord();

		CSSM_DB_ATTRIBUTE_DATA aRelationIDData =
		{
			RelationID,
			0,
			NULL
		};
		CSSM_DB_ATTRIBUTE_DATA aAttributeIDData =
		{
			AttributeID,
			0,
			NULL
		};
		CSSM_DB_ATTRIBUTE_DATA aAttributeNameFormatData =
		{
			AttributeNameFormat,
			0,
			NULL
		};
		CSSM_DB_ATTRIBUTE_DATA aAttributeNameData =
		{
			AttributeName,
			0,
			NULL
		};
		CSSM_DB_ATTRIBUTE_DATA aAttributeNameIDData =
		{
			AttributeNameID,
			0,
			NULL
		};
		CSSM_DB_ATTRIBUTE_DATA aAttributeFormatData =
		{
			AttributeFormat,
			0,
			NULL
		};
		CSSM_DB_ATTRIBUTE_DATA aRecordAttributes[] =
		{
			aRelationIDData,
			aAttributeIDData,
			aAttributeNameFormatData,
			aAttributeNameData,
			aAttributeNameIDData,
			aAttributeFormatData
		};
		CSSM_DB_RECORD_ATTRIBUTE_DATA aRecordAttributeData =
		{
			aMetaRecord.dataRecordType(),
			0,
			sizeof(aRecordAttributes) / sizeof(CSSM_DB_ATTRIBUTE_DATA),
			aRecordAttributes
		};
		CssmDbRecordAttributeData &aRecordData = CssmDbRecordAttributeData::overlay(aRecordAttributeData);

		TrackingAllocator recordAllocator(Allocator::standard());
		for (uint32 aRecord = 0; aRecord != aRecordsCount; aRecord++)
		{
			ReadSection aRecordSection = MetaRecord::readSection(aRecordsSection, aReadOffset);
			uint32 aRecordSize = aRecordSection.size();
			aReadOffset += aRecordSize;
			aMetaRecord.unpackRecord(aRecordSection, recordAllocator,
										&aRecordAttributeData, NULL, 0);
			// Create the attribute coresponding to this entry
			if (aRecordData[0].size() != 1 || aRecordData[0].format() != CSSM_DB_ATTRIBUTE_FORMAT_UINT32)
				CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
			uint32 aRelationId = aRecordData[0];

			// Skip the schema relations for the meta tables themselves.
			// FIXME: this hard-wires the meta-table relation IDs to be
			// within {CSSM_DB_RECORDTYPE_SCHEMA_START...
			// CSSM_DB_RECORDTYPE_SCHEMA_END} (which is {0..4}). 
			// Bogus - the MDS schema relation IDs start at 
			// CSSM_DB_RELATIONID_MDS_START which is 0x40000000.
			// Ref. Radar 2817921.
			if (CSSM_DB_RECORDTYPE_SCHEMA_START <= aRelationId && aRelationId < CSSM_DB_RECORDTYPE_SCHEMA_END)
				continue;

			// Get the MetaRecord corresponding to the specified RelationId
			MetaRecord &aMetaRecord = findTable(aRelationId).getMetaRecord();

			if (aRecordData[1].size() != 1
				|| aRecordData[1].format() != CSSM_DB_ATTRIBUTE_FORMAT_UINT32
				|| aRecordData[2].size() != 1
				|| aRecordData[2].format() != CSSM_DB_ATTRIBUTE_FORMAT_UINT32
				|| aRecordData[5].size() != 1
				|| aRecordData[5].format() != CSSM_DB_ATTRIBUTE_FORMAT_UINT32)
				CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

			uint32 anAttributeId = aRecordData[1];
			uint32 anAttributeNameFormat = aRecordData[2];
			uint32 anAttributeFormat = aRecordData[5];
			auto_ptr<string> aName;
			const CssmData *aNameID = NULL;

			if (aRecordData[3].size() == 1)
			{
				if (aRecordData[3].format() != CSSM_DB_ATTRIBUTE_FORMAT_STRING)
					CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

				auto_ptr<string> aName2(new string(static_cast<string>(aRecordData[3])));
				aName = aName2;
			}

			if (aRecordData[4].size() == 1)
			{
				if (aRecordData[4].format() != CSSM_DB_ATTRIBUTE_FORMAT_BLOB)
					CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

									// @@@ Invoking conversion operator to CssmData & on aRecordData[4]
									// And taking address of result.
				aNameID = &static_cast<const CssmData &>(aRecordData[4]);
			}

			// Make sure that the attribute specified by anAttributeNameFormat is present. 
			switch (anAttributeNameFormat)
			{
			case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
				if (aRecordData[3].size() != 1)
					CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
				break;
			case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
				if (aRecordData[4].size() != 1)
					CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
				break;
			case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
				break;
			default:
				CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
			}

			// Create the attribute
			aMetaRecord.createAttribute(aName.get(), aNameID, anAttributeId, anAttributeFormat);
        }
		
		// initialize the indexes associated with each table
		{
			TableMap::iterator it;
			for (it = mTableMap.begin(); it != mTableMap.end(); it++)
				it->second->readIndexSection();
		}
	}
	catch(...)
	{
		for_each_map_delete(mTableMap.begin(), mTableMap.end());
		mTableMap.clear();
		throw;
	}
}

const RecordId
DbVersion::getRecord(Table::Id inTableId, const RecordId &inRecordId,
							CSSM_DB_RECORD_ATTRIBUTE_DATA *inoutAttributes,
							CssmData *inoutData,
							Allocator &inAllocator) const
{
	return findTable(inTableId).getRecord(inRecordId, inoutAttributes,
										  inoutData, inAllocator);
}

Cursor *
DbVersion::createCursor(const CSSM_QUERY *inQuery) const
{
	// XXX We should add support for these special query types
	// By Creating a Cursor that iterates over multiple tables
	if (!inQuery || inQuery->RecordType == CSSM_DL_DB_RECORD_ANY
		|| inQuery->RecordType == CSSM_DL_DB_RECORD_ALL_KEYS)
	{
		return new MultiCursor(inQuery, *this);
	}

	return findTable(inQuery->RecordType).createCursor(inQuery, *this);
}

bool DbVersion::hasTable(Table::Id inTableId) const
{
    TableMap::const_iterator it = mTableMap.find(inTableId);
	return it != mTableMap.end();
}

const Table &
DbVersion::findTable(Table::Id inTableId) const
{
    TableMap::const_iterator it = mTableMap.find(inTableId);
    if (it == mTableMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORDTYPE);
	return *it->second;
}

Table &
DbVersion::findTable(Table::Id inTableId)
{
    TableMap::iterator it = mTableMap.find(inTableId);
    if (it == mTableMap.end())
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	return *it->second;
}

//
// Cursor implemetation
//
Cursor::Cursor()
{
}

Cursor::Cursor(const DbVersion &inDbVersion) : mDbVersion(&inDbVersion)
{
}

Cursor::~Cursor()
{
}

bool
Cursor::next(Table::Id &outTableId,
			 CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR outAttributes,
			 CssmData *outData,
			 Allocator &inAllocator,
			 RecordId &recordId)
{
	return false;
}

//
// LinearCursor implemetation
//
LinearCursor::LinearCursor(const CSSM_QUERY *inQuery, const DbVersion &inDbVersion,
						   const Table &inTable) :
    Cursor(inDbVersion),
    mRecordsCount(inTable.getRecordsCount()),
    mRecord(0),
    mRecordsSection(inTable.getRecordsSection()),
    mReadOffset(0),
    mMetaRecord(inTable.getMetaRecord())
{
	if (inQuery)
	{
	    mConjunctive = inQuery->Conjunctive;
	    mQueryFlags = inQuery->QueryFlags;
	    // XXX Do something with inQuery->QueryLimits?
	    uint32 aPredicatesCount = inQuery->NumSelectionPredicates;
	    mPredicates.resize(aPredicatesCount);
		try
		{
			for (uint32 anIndex = 0; anIndex < aPredicatesCount; anIndex++)
			{
				CSSM_SELECTION_PREDICATE &aPredicate = inQuery->SelectionPredicate[anIndex];
				mPredicates[anIndex] = new SelectionPredicate(mMetaRecord, aPredicate);
			}
		}
		catch(...)
		{
			for_each_delete(mPredicates.begin(), mPredicates.end());
			throw;
		}
	}
}

LinearCursor::~LinearCursor()
{
	for_each_delete(mPredicates.begin(), mPredicates.end());
}

bool
LinearCursor::next(Table::Id &outTableId,
				   CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
				   CssmData *inoutData, Allocator &inAllocator, RecordId &recordId)
{
	while (mRecord++ < mRecordsCount)
	{
		ReadSection aRecordSection = MetaRecord::readSection(mRecordsSection, mReadOffset);
		uint32 aRecordSize = aRecordSection.size();
		mReadOffset += aRecordSize;

        PredicateVector::const_iterator anIt = mPredicates.begin();
        PredicateVector::const_iterator anEnd = mPredicates.end();
		bool aMatch;
		if (anIt == anEnd)
		{
			// If there are no predicates we have a match.
			aMatch = true;
		}
		else if (mConjunctive == CSSM_DB_OR)
		{
			// If mConjunctive is OR, the first predicate that returns
			// true indicates a match. Dropthough means no match
			aMatch = false;
			for (; anIt != anEnd; anIt++)
			{
				if ((*anIt)->evaluate(aRecordSection))
				{
					aMatch = true;
                    break;
				}
			}
		}
		else if (mConjunctive == CSSM_DB_AND || mConjunctive == CSSM_DB_NONE)
		{
			// If mConjunctive is AND (or NONE), the first predicate that returns
			// false indicates a mismatch. Dropthough means a match
			aMatch = true;
			for (; anIt != anEnd; anIt++)
			{
				if (!(*anIt)->evaluate(aRecordSection))
				{
					aMatch = false;
                    break;
				}
			}
		}
		else
		{
			// XXX Should be CSSMERR_DL_INVALID_QUERY (or CSSMERR_DL_INVALID_CONJUNTIVE).
			CssmError::throwMe(CSSMERR_DL_UNSUPPORTED_QUERY);
		}

        if (aMatch)
        {
            // Get the actual record.
            mMetaRecord.unpackRecord(aRecordSection, inAllocator,
									 inoutAttributes, inoutData,
									 mQueryFlags);
			outTableId = mMetaRecord.dataRecordType();
			recordId = MetaRecord::unpackRecordId(aRecordSection);
			return true;
        }
    }

	return false;
}

//
// IndexCursor
// 

IndexCursor::IndexCursor(DbQueryKey *queryKey, const DbVersion &inDbVersion,
	const Table &table, const DbConstIndex *index) :
	Cursor(inDbVersion), mQueryKey(queryKey), mTable(table), mIndex(index)
{
	index->performQuery(*queryKey, mBegin, mEnd);
}

IndexCursor::~IndexCursor()
{
	// the query key will be deleted automatically, since it's an auto_ptr
}

bool
IndexCursor::next(Table::Id &outTableId,
	CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR outAttributes,
	CssmData *outData,
	Allocator &inAllocator, RecordId &recordId)
{
	if (mBegin == mEnd)
		return false;
		
	ReadSection rs = mIndex->getRecordSection(mBegin++);
	const MetaRecord &metaRecord = mTable.getMetaRecord();

	outTableId = metaRecord.dataRecordType();
	metaRecord.unpackRecord(rs, inAllocator, outAttributes, outData, 0);
	
	recordId = MetaRecord::unpackRecordId(rs);
	return true;
}

//
// MultiCursor
//
MultiCursor::MultiCursor(const CSSM_QUERY *inQuery, const DbVersion &inDbVersion) :
    Cursor(inDbVersion), mTableIterator(inDbVersion.begin())
{
	if (inQuery)
		mQuery.reset(new CssmAutoQuery(*inQuery));
	else
	{
		mQuery.reset(new CssmAutoQuery());
		mQuery->recordType(CSSM_DL_DB_RECORD_ANY);
	}
}

MultiCursor::~MultiCursor()
{
}

bool
MultiCursor::next(Table::Id &outTableId,
				  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
				  CssmData *inoutData, Allocator &inAllocator, RecordId &recordId)
{
	for (;;)
	{
		if (!mCursor.get())
		{
			if (mTableIterator == mDbVersion->end())
				return false;

			const Table &aTable = *mTableIterator++;
			if (!aTable.matchesTableId(mQuery->recordType()))
				continue;

			mCursor.reset(aTable.createCursor(mQuery.get(), *mDbVersion));
		}

		if (mCursor->next(outTableId, inoutAttributes, inoutData, inAllocator, recordId))
			return true;
			
		mCursor.reset(NULL);
	}
}


//
// DbModifier
//
DbModifier::DbModifier(AtomicFile &inAtomicFile, const AppleDatabase &db) :
	Metadata(),
	mDbVersion(),
    mAtomicFile(inAtomicFile),
	mDb(db)
{
}

DbModifier::~DbModifier()
{
    try
    {
		for_each_map_delete(mModifiedTableMap.begin(), mModifiedTableMap.end());
		// mAtomicTempFile will do automatic rollback on destruction.
    }
    catch(...) {}
}

const RefPointer<const DbVersion>
DbModifier::getDbVersion(bool force)
{
    StLock<Mutex> _(mDbVersionLock);

    /* Initialize the shared memory file change mechanism */
    pthread_once(&gCommonInitMutex, initCommon);

    /* If we don't have a mDbVersion yet, or we are force to re-read the file
       before a write transaction, or we have received any notifications after
       the last time we read the file, or more than kForceReReadTime seconds
       have passed since the last time we read the file, we open the file and
       check if it has changed. */
    if (!mDbVersion ||
        force ||
		gSegment == NULL ||
        mNotifyCount != *gSegment ||
        CFAbsoluteTimeGetCurrent() > mDbLastRead + kForceReReadTime)
    {
        RefPointer <AtomicBufferedFile> atomicBufferedFile(mAtomicFile.read());
        off_t length = atomicBufferedFile->open();
        /* Record the number of notifications we've seen and when we last
           opened the file.  */
		if (gSegment != NULL)
		{
			mNotifyCount = *gSegment;
		}
		
        mDbLastRead = CFAbsoluteTimeGetCurrent();

        /* If we already have a mDbVersion, let's check if we can reuse it. */
        if (mDbVersion)
        {
            if (length < AtomSize)
                CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

            off_t bytesRead = 0;
            const uint8 *ptr = atomicBufferedFile->read(length - AtomSize,
                AtomSize, bytesRead);
            ReadSection aVersionSection(ptr, (size_t)bytesRead);
            uint32 aVersionId = aVersionSection[0];

            /* If the version stamp hasn't changed the old mDbVersion is still
               current. */
            if (aVersionId == mDbVersion->getVersionId())
                return mDbVersion;
        }

	mDbVersion = new DbVersion(mDb, atomicBufferedFile);
    }

    return mDbVersion;
}

void
DbModifier::createDatabase(const CSSM_DBINFO &inDbInfo,
						   const CSSM_ACL_ENTRY_INPUT *inInitialAclEntry,
						   mode_t mode)
{
	// XXX This needs better locking.  There is a possible race condition between
	// two concurrent creators.  Or a writer/creator or a close/create etc.
	if (mAtomicTempFile || !mModifiedTableMap.empty())
		CssmError::throwMe(CSSMERR_DL_DATASTORE_ALREADY_EXISTS);

    mAtomicTempFile = mAtomicFile.create(mode);
	// Set mVersionId to one since this is the first version of the database.
	mVersionId = 1;

	// we need to create the meta tables first, because inserting tables
	// (including the meta tables themselves) relies on them being there
    createTable(new MetaRecord(mDb.schemaRelations));
    createTable(new MetaRecord(mDb.schemaAttributes));
    createTable(new MetaRecord(mDb.schemaIndexes));
    createTable(new MetaRecord(mDb.schemaParsingModule));
	
	// now add the meta-tables' schema to the meta tables themselves
	insertTableSchema(mDb.schemaRelations);
	insertTableSchema(mDb.schemaAttributes);
	insertTableSchema(mDb.schemaIndexes);
	insertTableSchema(mDb.schemaParsingModule);

    if (inInitialAclEntry != NULL)
    {
        //createACL(*inInitialAclEntry);
    }

    if (inDbInfo.NumberOfRecordTypes == 0)
        return;
    if (inDbInfo.RecordAttributeNames == NULL)
        CssmError::throwMe(CSSMERR_DL_INVALID_RECORDTYPE);
    if (inDbInfo.RecordIndexes == NULL)
        CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_INDEX);
    if (inDbInfo.DefaultParsingModules == NULL)
        CssmError::throwMe(CSSMERR_DL_INVALID_PARSING_MODULE);

    for (uint32 anIndex = 0; anIndex < inDbInfo.NumberOfRecordTypes; anIndex++)
    {
        insertTable(CssmDbRecordAttributeInfo::overlay(inDbInfo.RecordAttributeNames[anIndex]),
					&inDbInfo.RecordIndexes[anIndex],
					&inDbInfo.DefaultParsingModules[anIndex]);
    }
}

void DbModifier::openDatabase()
{
	// No need to do anything on open if we are already writing the database.
	if (!mAtomicTempFile)
		getDbVersion(false);
}

void DbModifier::closeDatabase()
{
	commit(); // XXX Requires write lock.
	StLock<Mutex> _(mDbVersionLock);
	mDbVersion = NULL;
}

void DbModifier::deleteDatabase()
{
	bool isDirty = mAtomicTempFile;
	rollback(); // XXX Requires write lock.
	StLock<Mutex> _(mDbVersionLock);

	// Clean up mModifiedTableMap in case this object gets reused again for
	// a new create.
	for_each_map_delete(mModifiedTableMap.begin(), mModifiedTableMap.end());
	mModifiedTableMap.clear();

	// If the database was dirty and we had no mDbVersion yet then rollback()
	// would have deleted the db.
	if (!isDirty || mDbVersion)
	{
		mDbVersion = NULL;
		mAtomicFile.performDelete();
	}
}

void
DbModifier::modifyDatabase()
{
	if (mAtomicTempFile)
		return;

	try
	{
		mAtomicTempFile = mAtomicFile.write();
		// Now we are holding the write lock make sure we get the latest greatest version of the db.
		// Also set mVersionId to one more that that of the old database.
		mVersionId = getDbVersion(true)->getVersionId() + 1;

		// Never make a database with mVersionId 0 since it makes bad things happen to Jaguar and older systems
		if (mVersionId == 0)
			mVersionId = 1;

		// Remove all old modified tables
		for_each_map_delete(mModifiedTableMap.begin(), mModifiedTableMap.end());
		mModifiedTableMap.clear();

		// Setup the new tables
		DbVersion::TableMap::const_iterator anIt =
			mDbVersion->mTableMap.begin();
		DbVersion::TableMap::const_iterator anEnd =
			mDbVersion->mTableMap.end();
		for (; anIt != anEnd; ++anIt)
		{
			auto_ptr<ModifiedTable> aTable(new ModifiedTable(anIt->second));
			mModifiedTableMap.insert(ModifiedTableMap::value_type(anIt->first,
																  aTable.get()));
			aTable.release();
		}
	}
	catch(...)
	{
		for_each_map_delete(mModifiedTableMap.begin(), mModifiedTableMap.end());
		mModifiedTableMap.clear();
		rollback();
		throw;
	}
}

void
DbModifier::deleteRecord(Table::Id inTableId, const RecordId &inRecordId)
{
	modifyDatabase();
	findTable(inTableId).deleteRecord(inRecordId);
}

const RecordId
DbModifier::insertRecord(Table::Id inTableId,
						 const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
						 const CssmData *inData)
{
	modifyDatabase();
	return findTable(inTableId).insertRecord(mVersionId, inAttributes, inData);
}

const RecordId
DbModifier::updateRecord(Table::Id inTableId, const RecordId &inRecordId,
						 const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
						 const CssmData *inData,
						 CSSM_DB_MODIFY_MODE inModifyMode)
{
	modifyDatabase();
	return findTable(inTableId).updateRecord(inRecordId, inAttributes, inData, inModifyMode);
}

// Create a table associated with a given metarecord, and add the table
// to the database.

ModifiedTable *
DbModifier::createTable(MetaRecord *inMetaRecord)
{
	auto_ptr<MetaRecord> aMetaRecord(inMetaRecord);
	auto_ptr<ModifiedTable> aModifiedTable(new ModifiedTable(inMetaRecord));
	// Now that aModifiedTable is fully constructed it owns inMetaRecord
	aMetaRecord.release();

	if (!mModifiedTableMap.insert
		(ModifiedTableMap::value_type(inMetaRecord->dataRecordType(),
									  aModifiedTable.get())).second)
	{
		// XXX Should be CSSMERR_DL_DUPLICATE_RECORDTYPE.  Since that
		// doesn't exist we report that the metatable's unique index would
		// no longer be valid
        CssmError::throwMe(CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA);
	}

	return aModifiedTable.release();
}

void
DbModifier::deleteTable(Table::Id inTableId)
{
	modifyDatabase();
    // Can't delete schema tables.
    if (CSSM_DB_RECORDTYPE_SCHEMA_START <= inTableId
		&& inTableId < CSSM_DB_RECORDTYPE_SCHEMA_END)
        CssmError::throwMe(CSSMERR_DL_INVALID_RECORDTYPE);

	// Find the ModifiedTable and delete it
    ModifiedTableMap::iterator it = mModifiedTableMap.find(inTableId);
    if (it == mModifiedTableMap.end())
        CssmError::throwMe(CSSMERR_DL_INVALID_RECORDTYPE);

    delete it->second;
    mModifiedTableMap.erase(it);
}

uint32
DbModifier::writeAuthSection(uint32 inSectionOffset)
{
	WriteSection anAuthSection;

    // XXX Put real data into the authsection.
	uint32 anOffset = anAuthSection.put(0, 0);
	anAuthSection.size(anOffset);

	mAtomicTempFile->write(AtomicFile::FromStart, inSectionOffset,
					anAuthSection.address(), anAuthSection.size());
    return inSectionOffset + anOffset;
}

uint32
DbModifier::writeSchemaSection(uint32 inSectionOffset)
{
	uint32 aTableCount = (uint32) mModifiedTableMap.size();
	WriteSection aTableSection(Allocator::standard(),
							   OffsetTables + AtomSize * aTableCount);
	// Set aTableSection to the correct size.
	aTableSection.size(OffsetTables + AtomSize * aTableCount);
	aTableSection.put(OffsetTablesCount, aTableCount);

	uint32 anOffset = inSectionOffset + OffsetTables + AtomSize * aTableCount;
	ModifiedTableMap::const_iterator anIt = mModifiedTableMap.begin();
	ModifiedTableMap::const_iterator anEnd = mModifiedTableMap.end();
	for (uint32 aTableNumber = 0; anIt != anEnd; anIt++, aTableNumber++)
	{
		// Put the offset to the current table relative to the start of
		// this section into the tables array
		aTableSection.put(OffsetTables + AtomSize * aTableNumber,
						  anOffset - inSectionOffset);
		anOffset = anIt->second->writeTable(*mAtomicTempFile, anOffset);
	}

	aTableSection.put(OffsetSchemaSize, anOffset - inSectionOffset);
	mAtomicTempFile->write(AtomicFile::FromStart, inSectionOffset,
					aTableSection.address(), aTableSection.size());

	return anOffset;
}

void
DbModifier::commit()
{
    if (!mAtomicTempFile)
        return;
    try
    {
		WriteSection aHeaderSection(Allocator::standard(), size_t(HeaderSize));
		// Set aHeaderSection to the correct size.
		aHeaderSection.size(HeaderSize);

        // Start writing sections after the header
        uint32 anOffset = HeaderOffset + HeaderSize;

        // Write auth section
		aHeaderSection.put(OffsetAuthOffset, anOffset);
        anOffset = writeAuthSection(anOffset);
        // Write schema section
		aHeaderSection.put(OffsetSchemaOffset, anOffset);
        anOffset = writeSchemaSection(anOffset);

		// Write out the file header.
		aHeaderSection.put(OffsetMagic, HeaderMagic);
		aHeaderSection.put(OffsetVersion, HeaderVersion);
        mAtomicTempFile->write(AtomicFile::FromStart, HeaderOffset,
							   aHeaderSection.address(), aHeaderSection.size());

		// Write out the versionId.
		WriteSection aVersionSection(Allocator::standard(), size_t(AtomSize));
		anOffset = aVersionSection.put(0, mVersionId);
		aVersionSection.size(anOffset);

        mAtomicTempFile->write(AtomicFile::FromEnd, 0,
							   aVersionSection.address(), aVersionSection.size());

		mAtomicTempFile->commit();
		mAtomicTempFile = NULL;
	   /* Initialize the shared memory file change mechanism */
	   pthread_once(&gCommonInitMutex, initCommon);
       
	   if (gSegment != NULL)
		{
			/*
				PLEASE NOTE:
				
				The following operation is endian safe because we are not looking 
				for monotonic increase. I have tested every possible value of 
				*gSegment, and there is no value for which alternating
				big and little endian increments will produce the original value.
			*/
       
			OSAtomicIncrement32Barrier (gSegment);
		}
    }
    catch(...)
    {
		rollback();
		throw;
    }
}

void
DbModifier::rollback() throw()
{
	// This will destroy the AtomicTempFile if we have one causing it to rollback.
	mAtomicTempFile = NULL;
}

const RecordId
DbModifier::getRecord(Table::Id inTableId, const RecordId &inRecordId,
					  CSSM_DB_RECORD_ATTRIBUTE_DATA *inoutAttributes,
					  CssmData *inoutData, Allocator &inAllocator)
{
    if (mAtomicTempFile)
	{
		// We are in the midst of changing the database.
		return findTable(inTableId).getRecord(inRecordId, inoutAttributes,
			inoutData, inAllocator);
	}
	else
	{
		return getDbVersion(false)->getRecord(inTableId, inRecordId,
			inoutAttributes, inoutData, inAllocator);
	}
}

Cursor *
DbModifier::createCursor(const CSSM_QUERY *inQuery)
{
	if (mAtomicTempFile)
	{
		// We are modifying this database.
		
		// If we have a mDbVersion already then it's a snapshot of the database
		// right before the modifications started.  So return a cursor using
		// that.
		if (mDbVersion)
			return mDbVersion->createCursor(inQuery);

		// This is a newly created but never commited database.  Return a
		// Cursor that will not return any matches.
		return new Cursor();
	}

	// Get the latest and greatest version of the db and create the cursor
	// on that.
	return getDbVersion(false)->createCursor(inQuery);
}

// Insert schema records for a new table into the metatables of the database. This gets
// called while a database is being created.

void
DbModifier::insertTableSchema(const CssmDbRecordAttributeInfo &inInfo,
	const CSSM_DB_RECORD_INDEX_INFO *inIndexInfo /* = NULL */)
{
	ModifiedTable &aTable = findTable(inInfo.DataRecordType);
	const MetaRecord &aMetaRecord = aTable.getMetaRecord();

	CssmAutoDbRecordAttributeData aRecordBuilder(5); // Set capacity to 5 so we don't need to grow

	// Create the entry for the SchemaRelations table.
	aRecordBuilder.add(RelationID, inInfo.recordType());
	aRecordBuilder.add(RelationName, mDb.recordName(inInfo.recordType()));

	// Insert the record into the SchemaRelations ModifiedTable
    findTable(mDb.schemaRelations.DataRecordType).insertRecord(mVersionId,
		&aRecordBuilder, NULL);
	
	ModifiedTable &anAttributeTable = findTable(mDb.schemaAttributes.DataRecordType);
    for (uint32 anIndex = 0; anIndex < inInfo.size(); anIndex++)
    {
		// Create an entry for the SchemaAttributes table.
		aRecordBuilder.clear();
		aRecordBuilder.add(RelationID, inInfo.recordType());
		aRecordBuilder.add(AttributeNameFormat, inInfo.at(anIndex).nameFormat());

		uint32 attributeId = aMetaRecord.metaAttribute(inInfo.at(anIndex)).attributeId();
		
        switch (inInfo.at(anIndex).nameFormat())
        {
            case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
				aRecordBuilder.add(AttributeName, inInfo.at(anIndex).Label.AttributeName);
				break;
            case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
				aRecordBuilder.add(AttributeNameID, inInfo.at(anIndex).Label.AttributeOID);
				break;
            case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
                break;
            default:
                CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);
		}

		aRecordBuilder.add(AttributeID, attributeId);
		aRecordBuilder.add(AttributeFormat, inInfo.at(anIndex).format());

		// Insert the record into the SchemaAttributes ModifiedTable
		anAttributeTable.insertRecord(mVersionId, &aRecordBuilder, NULL);
    }

	if (inIndexInfo != NULL) {
	
		if (inIndexInfo->DataRecordType != inInfo.DataRecordType &&
			inIndexInfo->NumberOfIndexes > 0)
			CssmError::throwMe(CSSMERR_DL_INVALID_RECORDTYPE);
	
		ModifiedTable &indexMetaTable = findTable(mDb.schemaIndexes.DataRecordType);
		uint32 aNumberOfIndexes = inIndexInfo->NumberOfIndexes;
		
		for (uint32 anIndex = 0; anIndex < aNumberOfIndexes; anIndex++)
		{
			const CssmDbIndexInfo &thisIndex = CssmDbIndexInfo::overlay(inIndexInfo->IndexInfo[anIndex]);
			
			// make sure the index is supported
			if (thisIndex.dataLocation() != CSSM_DB_INDEX_ON_ATTRIBUTE)
				CssmError::throwMe(CSSMERR_DL_INVALID_INDEX_INFO);
			
			// assign an index ID: the unique index is ID 0, all others are ID > 0
			uint32 indexId;
			if (thisIndex.IndexType == CSSM_DB_INDEX_UNIQUE)
				indexId = 0;
			else
				indexId = anIndex + 1;
				
			// figure out the attribute ID
			uint32 attributeId =
				aMetaRecord.metaAttribute(thisIndex.Info).attributeId();
			
			// Create an entry for the SchemaIndexes table.
			aRecordBuilder.clear();
			aRecordBuilder.add(RelationID, inInfo.DataRecordType);
			aRecordBuilder.add(IndexID, indexId);
			aRecordBuilder.add(AttributeID, attributeId);
			aRecordBuilder.add(IndexType, thisIndex.IndexType);
			aRecordBuilder.add(IndexedDataLocation, thisIndex.IndexedDataLocation);

			// Insert the record into the SchemaIndexes ModifiedTable
			indexMetaTable.insertRecord(mVersionId, &aRecordBuilder, NULL);
			
			// update the table's index objects
			DbMutableIndex &index = aTable.findIndex(indexId, aMetaRecord, indexId == 0);
			index.appendAttribute(attributeId);
		}
	}
}

// Insert a new table. The attribute info is required; the index and parsing module
// descriptions are optional. This version gets called during the creation of a
// database.

void
DbModifier::insertTable(const CssmDbRecordAttributeInfo &inInfo,
						const CSSM_DB_RECORD_INDEX_INFO *inIndexInfo /* = NULL */,
						const CSSM_DB_PARSING_MODULE_INFO *inParsingModule /* = NULL */)
{
	modifyDatabase();
	createTable(new MetaRecord(inInfo));
	insertTableSchema(inInfo, inIndexInfo);
}

// Insert a new table. This is the version that gets called when a table is added
// after a database has been created.

void
DbModifier::insertTable(Table::Id inTableId, const string &inTableName,
						uint32 inNumberOfAttributes,
						const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo,
						uint32 inNumberOfIndexes,
						const CSSM_DB_SCHEMA_INDEX_INFO *inIndexInfo)
{
	modifyDatabase();
	ModifiedTable *aTable = createTable(new MetaRecord(inTableId, inNumberOfAttributes, inAttributeInfo));

	CssmAutoDbRecordAttributeData aRecordBuilder(6); // Set capacity to 6 so we don't need to grow

	// Create the entry for the SchemaRelations table.
	aRecordBuilder.add(RelationID, inTableId);
	aRecordBuilder.add(RelationName, inTableName);

	// Insert the record into the SchemaRelations ModifiedTable
    findTable(mDb.schemaRelations.DataRecordType).insertRecord(mVersionId,
		&aRecordBuilder, NULL);

	ModifiedTable &anAttributeTable = findTable(mDb.schemaAttributes.DataRecordType);
    for (uint32 anIndex = 0; anIndex < inNumberOfAttributes; anIndex++)
    {
		// Create an entry for the SchemaAttributes table.
		aRecordBuilder.clear();
		aRecordBuilder.add(RelationID, inTableId);
		// XXX What should this be?  We set it to CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER for now
		// since the AttributeID is always valid.
		aRecordBuilder.add(AttributeNameFormat, uint32(CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER));
		aRecordBuilder.add(AttributeID, inAttributeInfo[anIndex].AttributeId);
		if (inAttributeInfo[anIndex].AttributeName)
			aRecordBuilder.add(AttributeName, inAttributeInfo[anIndex].AttributeName);
		if (inAttributeInfo[anIndex].AttributeNameID.Length > 0)
			aRecordBuilder.add(AttributeNameID, inAttributeInfo[anIndex].AttributeNameID);
		aRecordBuilder.add(AttributeFormat, inAttributeInfo[anIndex].DataType);

		// Insert the record into the SchemaAttributes ModifiedTable
		anAttributeTable.insertRecord(mVersionId, &aRecordBuilder, NULL);
    }

	ModifiedTable &anIndexTable = findTable(mDb.schemaIndexes.DataRecordType);
    for (uint32 anIndex = 0; anIndex < inNumberOfIndexes; anIndex++)
    {
		// Create an entry for the SchemaIndexes table.
		aRecordBuilder.clear();
		aRecordBuilder.add(RelationID, inTableId);
		aRecordBuilder.add(IndexID, inIndexInfo[anIndex].IndexId);
		aRecordBuilder.add(AttributeID, inIndexInfo[anIndex].AttributeId);
		aRecordBuilder.add(IndexType, inIndexInfo[anIndex].IndexType);
		aRecordBuilder.add(IndexedDataLocation, inIndexInfo[anIndex].IndexedDataLocation);

		// Insert the record into the SchemaIndexes ModifiedTable
		anIndexTable.insertRecord(mVersionId, &aRecordBuilder, NULL);
		
		// update the table's index objects
		DbMutableIndex &index = aTable->findIndex(inIndexInfo[anIndex].IndexId,
			aTable->getMetaRecord(), inIndexInfo[anIndex].IndexType == CSSM_DB_INDEX_UNIQUE);
		index.appendAttribute(inIndexInfo[anIndex].AttributeId);
    }
}



bool DbModifier::hasTable(Table::Id inTableId)
{
	return getDbVersion(false)->hasTable(inTableId);
}



ModifiedTable &
DbModifier::findTable(Table::Id inTableId)
{
    ModifiedTableMap::iterator it = mModifiedTableMap.find(inTableId);
    if (it == mModifiedTableMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORDTYPE);
	return *it->second;
}


//
// AppleDatabaseManager implementation
//

AppleDatabaseManager::AppleDatabaseManager(const AppleDatabaseTableName *tableNames)
	:	DatabaseManager(),
		mTableNames(tableNames)
{
	// make sure that a proper set of table ids and names has been provided
	
	if (!mTableNames)
		CssmError::throwMe(CSSMERR_DL_INTERNAL_ERROR);
	else {
		uint32 i;
		for (i = 0; mTableNames[i].mTableName; i++) {}
		if (i < AppleDatabaseTableName::kNumRequiredTableNames)
			CssmError::throwMe(CSSMERR_DL_INTERNAL_ERROR);
	}
}

Database *
AppleDatabaseManager::make(const DbName &inDbName)
{
    return new AppleDatabase(inDbName, mTableNames);
}


//
// AppleDbContext implementation
//

/* This is the version 0 CSSM_APPLEDL_OPEN_PARAMETERS struct used up to 10.2.x. */
extern "C" {

typedef struct cssm_appledl_open_parameters_v0
{
		uint32 length;  /* Should be sizeof(CSSM_APPLEDL_OPEN_PARAMETERS_V0). */
		uint32 version; /* Should be 0. */
		CSSM_BOOL autoCommit;
} CSSM_APPLEDL_OPEN_PARAMETERS_V0;

};

AppleDbContext::AppleDbContext(Database &inDatabase,
							   DatabaseSession &inDatabaseSession,
							   CSSM_DB_ACCESS_TYPE inAccessRequest,
							   const AccessCredentials *inAccessCred,
							   const void *inOpenParameters) :
	DbContext(inDatabase, inDatabaseSession, inAccessRequest, inAccessCred),
	mAutoCommit(true),
	mMode(0666)
{
	const CSSM_APPLEDL_OPEN_PARAMETERS *anOpenParameters =
		reinterpret_cast<const CSSM_APPLEDL_OPEN_PARAMETERS *>(inOpenParameters);

	if (anOpenParameters)
	{
		switch (anOpenParameters->version)
		{
		case 1:
			if (anOpenParameters->length < sizeof(CSSM_APPLEDL_OPEN_PARAMETERS))
				CssmError::throwMe(CSSMERR_APPLEDL_INVALID_OPEN_PARAMETERS);

			if (anOpenParameters->mask & kCSSM_APPLEDL_MASK_MODE)
				mMode = anOpenParameters->mode;
			/*DROPTHROUGH*/
		case 0:
			if (anOpenParameters->length < sizeof(CSSM_APPLEDL_OPEN_PARAMETERS_V0))
				CssmError::throwMe(CSSMERR_APPLEDL_INVALID_OPEN_PARAMETERS);

			mAutoCommit = anOpenParameters->autoCommit == CSSM_FALSE ? false : true;
			break;

		default:
			CssmError::throwMe(CSSMERR_APPLEDL_INVALID_OPEN_PARAMETERS);
		}
	}
}

AppleDbContext::~AppleDbContext()
{
}

//
// AppleDatabase implementation
//
AppleDatabase::AppleDatabase(const DbName &inDbName, const AppleDatabaseTableName *tableNames) :
    Database(inDbName),
	schemaRelations(tableNames[AppleDatabaseTableName::kSchemaInfo].mTableId,
		sizeof(AttrSchemaRelations) / sizeof(CSSM_DB_ATTRIBUTE_INFO),
		const_cast<CSSM_DB_ATTRIBUTE_INFO_PTR>(AttrSchemaRelations)),
	schemaAttributes(tableNames[AppleDatabaseTableName::kSchemaAttributes].mTableId,
		sizeof(AttrSchemaAttributes) / sizeof(CSSM_DB_ATTRIBUTE_INFO),
		const_cast<CSSM_DB_ATTRIBUTE_INFO_PTR>(AttrSchemaAttributes)),
	schemaIndexes(tableNames[AppleDatabaseTableName::kSchemaIndexes].mTableId,
		sizeof(AttrSchemaIndexes) / sizeof(CSSM_DB_ATTRIBUTE_INFO),
		const_cast<CSSM_DB_ATTRIBUTE_INFO_PTR>(AttrSchemaIndexes)),
	schemaParsingModule(tableNames[AppleDatabaseTableName::kSchemaParsingModule].mTableId,
		sizeof(AttrSchemaParsingModule) / sizeof(CSSM_DB_ATTRIBUTE_INFO),
		const_cast<CSSM_DB_ATTRIBUTE_INFO_PTR>(AttrSchemaParsingModule)),
    mAtomicFile(mDbName.dbName()),
	mDbModifier(mAtomicFile, *this),
	mTableNames(tableNames)
{
	/* temp check for X509Anchors access - this should removed before Leopard GM */
	if(!strcmp(inDbName.dbName(), "/System/Library/Keychains/X509Anchors")) {
		Syslog::alert("Warning: accessing obsolete X509Anchors.");
	}
}

AppleDatabase::~AppleDatabase()
{
}

// Return the name of a record type. This uses a table that maps record types
// to record names. The table is provided when the database is created.

const char *AppleDatabase::recordName(CSSM_DB_RECORDTYPE inRecordType) const
{
	if (inRecordType == CSSM_DL_DB_RECORD_ANY || inRecordType == CSSM_DL_DB_RECORD_ALL_KEYS)
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORDTYPE);
		
	for (uint32 i = 0; mTableNames[i].mTableName; i++)
		if (mTableNames[i].mTableId == inRecordType)
			return mTableNames[i].mTableName;
			
	return "";
}

DbContext *
AppleDatabase::makeDbContext(DatabaseSession &inDatabaseSession,
                             CSSM_DB_ACCESS_TYPE inAccessRequest,
                             const AccessCredentials *inAccessCred,
                             const void *inOpenParameters)
{
    return new AppleDbContext(*this, inDatabaseSession, inAccessRequest,
							  inAccessCred, inOpenParameters);
}

void
AppleDatabase::dbCreate(DbContext &inDbContext, const CSSM_DBINFO &inDBInfo,
                        const CSSM_ACL_ENTRY_INPUT *inInitialAclEntry)
{
	AppleDbContext &context = safer_cast<AppleDbContext &>(inDbContext);
    try
    {
		StLock<Mutex> _(mWriteLock);
        mDbModifier.createDatabase(inDBInfo, inInitialAclEntry, context.mode());
    }
    catch(...)
    {
        mDbModifier.rollback();
        throw;
    }
	if (context.autoCommit())
		mDbModifier.commit();
}

void
AppleDatabase::dbOpen(DbContext &inDbContext)
{
	mDbModifier.openDatabase();
}

void
AppleDatabase::dbClose()
{
	StLock<Mutex> _(mWriteLock);
	mDbModifier.closeDatabase();
}

void
AppleDatabase::dbDelete(DatabaseSession &inDatabaseSession,
                        const AccessCredentials *inAccessCred)
{
	StLock<Mutex> _(mWriteLock);
    // XXX Check callers credentials.
	mDbModifier.deleteDatabase();
}

void
AppleDatabase::createRelation(DbContext &inDbContext,
                              CSSM_DB_RECORDTYPE inRelationID,
                              const char *inRelationName,
                              uint32 inNumberOfAttributes,
                              const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo,
                              uint32 inNumberOfIndexes,
                              const CSSM_DB_SCHEMA_INDEX_INFO &inIndexInfo)
{
	try
	{
		StLock<Mutex> _(mWriteLock);
		// XXX Fix the refs here.
		mDbModifier.insertTable(inRelationID, inRelationName,
								inNumberOfAttributes, inAttributeInfo,
								inNumberOfIndexes, &inIndexInfo);
	}
	catch(...)
	{
		if (safer_cast<AppleDbContext &>(inDbContext).autoCommit())
			mDbModifier.rollback();
		throw;
	}
	if (safer_cast<AppleDbContext &>(inDbContext).autoCommit())
		mDbModifier.commit();
}

void
AppleDatabase::destroyRelation(DbContext &inDbContext,
                               CSSM_DB_RECORDTYPE inRelationID)
{
	try
	{
		StLock<Mutex> _(mWriteLock);
		mDbModifier.deleteTable(inRelationID);
	}
	catch(...)
	{
		if (safer_cast<AppleDbContext &>(inDbContext).autoCommit())
			mDbModifier.rollback();
		throw;
	}
	if (safer_cast<AppleDbContext &>(inDbContext).autoCommit())
		mDbModifier.commit();
}

void
AppleDatabase::authenticate(DbContext &inDbContext,
                            CSSM_DB_ACCESS_TYPE inAccessRequest,
                            const AccessCredentials &inAccessCred)
{
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
AppleDatabase::getDbAcl(DbContext &inDbContext,
                        const CSSM_STRING *inSelectionTag,
                        uint32 &outNumberOfAclInfos,
                        CSSM_ACL_ENTRY_INFO_PTR &outAclInfos)
{
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
AppleDatabase::changeDbAcl(DbContext &inDbContext,
                           const AccessCredentials &inAccessCred,
                           const CSSM_ACL_EDIT &inAclEdit)
{
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
AppleDatabase::getDbOwner(DbContext &inDbContext,
						  CSSM_ACL_OWNER_PROTOTYPE &outOwner)
{
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void
AppleDatabase::changeDbOwner(DbContext &inDbContext,
                             const AccessCredentials &inAccessCred,
                             const CSSM_ACL_OWNER_PROTOTYPE &inNewOwner)
{
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

char *
AppleDatabase::getDbNameFromHandle(const DbContext &inDbContext) const
{
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

CSSM_DB_UNIQUE_RECORD_PTR
AppleDatabase::dataInsert(DbContext &inDbContext,
                          CSSM_DB_RECORDTYPE inRecordType,
                          const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
                          const CssmData *inData)
{
	CSSM_DB_UNIQUE_RECORD_PTR anUniqueRecordPtr = NULL;
	try
	{
		StLock<Mutex> _(mWriteLock);
		const RecordId aRecordId =
			mDbModifier.insertRecord(inRecordType, inAttributes, inData);

		anUniqueRecordPtr = createUniqueRecord(inDbContext, inRecordType,
											   aRecordId);
		if (safer_cast<AppleDbContext &>(inDbContext).autoCommit())
			mDbModifier.commit();
	}
	catch(...)
	{
		if (anUniqueRecordPtr != NULL)
			freeUniqueRecord(inDbContext, *anUniqueRecordPtr);

		if (safer_cast<AppleDbContext &>(inDbContext).autoCommit())
			mDbModifier.rollback();
		throw;
	}

	return anUniqueRecordPtr;
}

void
AppleDatabase::dataDelete(DbContext &inDbContext,
                          const CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
    try
    {
		// syslog if it's the .Mac password
		CSSM_DB_RECORD_ATTRIBUTE_DATA attrData;
		// we have to do this in two phases -- the first to get the record type, and the second to actually read the attributes.  Otherwise, we might get
		// an exception.
		memset(&attrData, 0, sizeof(attrData));
		dataGetFromUniqueRecordId(inDbContext, inUniqueRecord, &attrData, NULL);
		
		if (attrData.DataRecordType == CSSM_DL_DB_RECORD_GENERIC_PASSWORD)
		{
			CSSM_DB_ATTRIBUTE_DATA attributes;
			
			// setup some attributes and see if we are indeed the .Mac password
			attributes.Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
			attributes.Info.Label.AttributeID = 'svce';
			attributes.Info.AttributeFormat = 0;
			attributes.NumberOfValues = 1;
			attributes.Value = NULL;

			attrData.NumberOfAttributes = 1;
			attrData.AttributeData = &attributes;		
			
			dataGetFromUniqueRecordId(inDbContext, inUniqueRecord, &attrData, NULL);
			
			// now check the results
			std::string dataString((const char*) attrData.AttributeData[0].Value[0].Data, attrData.AttributeData[0].Value[0].Length);
			if (dataString == "iTools")
			{
				syslog(LOG_WARNING, "Warning: Removed .Me password");
			}
			
			free(attrData.AttributeData[0].Value[0].Data);
			free(attrData.AttributeData[0].Value);
		}
		
		StLock<Mutex> _(mWriteLock);
		Table::Id aTableId;
		const RecordId aRecordId(parseUniqueRecord(inUniqueRecord, aTableId));
		mDbModifier.deleteRecord(aTableId, aRecordId);
	}
    catch(...)
    {
		if (safer_cast<AppleDbContext &>(inDbContext).autoCommit())
			mDbModifier.rollback();
        throw;
    }

	if (safer_cast<AppleDbContext &>(inDbContext).autoCommit())
		mDbModifier.commit();
}

void
AppleDatabase::dataModify(DbContext &inDbContext,
                          CSSM_DB_RECORDTYPE inRecordType,
                          CSSM_DB_UNIQUE_RECORD &inoutUniqueRecord,
                          const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributesToBeModified,
                          const CssmData *inDataToBeModified,
                          CSSM_DB_MODIFY_MODE inModifyMode)
{
    try
    {
		StLock<Mutex> _(mWriteLock);
		Table::Id aTableId;
		const RecordId oldRecordId = parseUniqueRecord(inoutUniqueRecord,
			aTableId);
#if 1
		if (inRecordType != aTableId)
#else
		if (inRecordType != aTableId &&
			inRecordType != CSSM_DL_DB_RECORD_ANY &&
			!(inRecordType == CSSM_DL_DB_RECORD_ALL_KEYS &&
			  (aTableId == CSSM_DL_DB_RECORD_PUBLIC_KEY ||
			   aTableId == CSSM_DL_DB_RECORD_PRIVATE_KEY ||
			   aTableId == CSSM_DL_DB_RECORD_SYMMETRIC_KEY)))
#endif
		{
			CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);
		}

		const RecordId newRecordId =
			mDbModifier.updateRecord(aTableId,
									 oldRecordId,
									 inAttributesToBeModified,
									 inDataToBeModified,
									 inModifyMode);
		updateUniqueRecord(inDbContext, aTableId, newRecordId,
			inoutUniqueRecord);
	}
    catch(...)
    {
		if (safer_cast<AppleDbContext &>(inDbContext).autoCommit())
			mDbModifier.rollback();
        throw;
    }

	if (safer_cast<AppleDbContext &>(inDbContext).autoCommit())
		mDbModifier.commit();
}

CSSM_HANDLE
AppleDatabase::dataGetFirst(DbContext &inDbContext,
                            const CssmQuery *inQuery,
                            CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                            CssmData *inoutData,
                            CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
	// XXX: register Cursor with DbContext and have DbContext call
	// dataAbortQuery for all outstanding Query objects on close.
	auto_ptr<Cursor> aCursor(mDbModifier.createCursor(inQuery));
	Table::Id aTableId;
	RecordId aRecordId;
	
	if (!aCursor->next(aTableId, inoutAttributes, inoutData,
					   inDbContext.mDatabaseSession, aRecordId))
		// return a NULL handle, and implicitly delete the cursor
		return CSSM_INVALID_HANDLE;

	outUniqueRecord = createUniqueRecord(inDbContext, aTableId, aRecordId);
	return aCursor.release()->handle(); // We didn't throw so keep the Cursor around.
}

bool
AppleDatabase::dataGetNext(DbContext &inDbContext,
                           CSSM_HANDLE inResultsHandle,
                           CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                           CssmData *inoutData,
                           CSSM_DB_UNIQUE_RECORD_PTR &outUniqueRecord)
{
	auto_ptr<Cursor> aCursor(&HandleObject::find<Cursor>(inResultsHandle, CSSMERR_DL_INVALID_RESULTS_HANDLE));
	Table::Id aTableId;
	RecordId aRecordId;
	
	if (!aCursor->next(aTableId, inoutAttributes, inoutData, inDbContext.mDatabaseSession, aRecordId))
		return false;

	outUniqueRecord = createUniqueRecord(inDbContext, aTableId, aRecordId);

	aCursor.release();
	return true;
}

void
AppleDatabase::dataAbortQuery(DbContext &inDbContext,
                              CSSM_HANDLE inResultsHandle)
{
	delete &HandleObject::find<Cursor>(inResultsHandle, CSSMERR_DL_INVALID_RESULTS_HANDLE);
}

void
AppleDatabase::dataGetFromUniqueRecordId(DbContext &inDbContext,
                                         const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
                                         CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                                         CssmData *inoutData)
{
	Table::Id aTableId;
	const RecordId aRecordId(parseUniqueRecord(inUniqueRecord, aTableId));
	// XXX Change CDSA spec to use new RecordId returned by this function
	mDbModifier.getRecord(aTableId, aRecordId, inoutAttributes, inoutData,
						  inDbContext.mDatabaseSession);
}

void
AppleDatabase::freeUniqueRecord(DbContext &inDbContext,
                                CSSM_DB_UNIQUE_RECORD &inUniqueRecord)
{
	if (inUniqueRecord.RecordIdentifier.Length != 0
		&& inUniqueRecord.RecordIdentifier.Data != NULL)
	{
		inUniqueRecord.RecordIdentifier.Length = 0;
		inDbContext.mDatabaseSession.free(inUniqueRecord.RecordIdentifier.Data);
	}
	inDbContext.mDatabaseSession.free(&inUniqueRecord);
}

void
AppleDatabase::updateUniqueRecord(DbContext &inDbContext,
								  CSSM_DB_RECORDTYPE inTableId,
								  const RecordId &inRecordId,
								  CSSM_DB_UNIQUE_RECORD &inoutUniqueRecord)
{
	uint32 *aBuffer = reinterpret_cast<uint32 *>(inoutUniqueRecord.RecordIdentifier.Data);
	aBuffer[0] = inTableId;
	aBuffer[1] = inRecordId.mRecordNumber;
	aBuffer[2] = inRecordId.mCreateVersion;
	aBuffer[3] = inRecordId.mRecordVersion;
}

CSSM_DB_UNIQUE_RECORD_PTR
AppleDatabase::createUniqueRecord(DbContext &inDbContext,
								  CSSM_DB_RECORDTYPE inTableId,
								  const RecordId &inRecordId)
{
	CSSM_DB_UNIQUE_RECORD_PTR aUniqueRecord =
		inDbContext.mDatabaseSession.alloc<CSSM_DB_UNIQUE_RECORD>();
	memset(aUniqueRecord, 0, sizeof(*aUniqueRecord));
	aUniqueRecord->RecordIdentifier.Length = sizeof(uint32) * 4;
	try
	{
		aUniqueRecord->RecordIdentifier.Data =
			inDbContext.mDatabaseSession.alloc<uint8>(sizeof(uint32) * 4);
		updateUniqueRecord(inDbContext, inTableId, inRecordId, *aUniqueRecord);
	}
	catch(...)
	{
		inDbContext.mDatabaseSession.free(aUniqueRecord);
		throw;
	}

	return aUniqueRecord;
}

const RecordId
AppleDatabase::parseUniqueRecord(const CSSM_DB_UNIQUE_RECORD &inUniqueRecord,
								 CSSM_DB_RECORDTYPE &outTableId)
{
	if (inUniqueRecord.RecordIdentifier.Length != sizeof(uint32) * 4)
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORD_UID);

	uint32 *aBuffer = reinterpret_cast<uint32 *>(inUniqueRecord.RecordIdentifier.Data);
	outTableId = aBuffer[0];
	return RecordId(aBuffer[1], aBuffer[2], aBuffer[3]);
}

void
AppleDatabase::passThrough(DbContext &dbContext,
						   uint32 passThroughId,
						   const void *inputParams,
						   void **outputParams)
{
	switch (passThroughId)
	{
	case CSSM_APPLEFILEDL_TOGGLE_AUTOCOMMIT:
		{
			AppleDbContext &dbc = safer_cast<AppleDbContext &>(dbContext);
			// Return the old state of the autoCommit flag if requested
			if (outputParams)
				*reinterpret_cast<CSSM_BOOL *>(outputParams) = dbc.autoCommit();
			dbc.autoCommit(inputParams ? CSSM_TRUE : CSSM_FALSE);
		}
		break;
		
	case CSSM_APPLEFILEDL_COMMIT:
		mDbModifier.commit();
		break;
		
	case CSSM_APPLEFILEDL_ROLLBACK:
		mDbModifier.rollback();
		break;

	case CSSM_APPLECSPDL_DB_RELATION_EXISTS:
	{
		CSSM_BOOL returnValue;
		
		CSSM_DB_RECORDTYPE recordType = *(CSSM_DB_RECORDTYPE*) inputParams;
		if (recordType == CSSM_DL_DB_RECORD_ANY || recordType == CSSM_DL_DB_RECORD_ALL_KEYS)
		{
			returnValue = CSSM_TRUE;
		}
		else
		{
			returnValue = mDbModifier.hasTable(recordType);
		}
		
		*(CSSM_BOOL*) outputParams = returnValue;
		break;
	}

	default:
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
		break;
	}
}
