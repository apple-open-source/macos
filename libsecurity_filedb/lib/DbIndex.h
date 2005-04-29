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
// DbIndex.h
//

#ifndef _H_APPLEDL_DBINDEX
#define _H_APPLEDL_DBINDEX

#include "MetaRecord.h"

namespace Security
{

class Table;
class DbConstIndex;
class DbIndex;

typedef constVector<Atom> DbOffsetVector;

typedef DbOffsetVector::const_iterator DbIndexIterator;

//
// An object that represents a key being used as part of a query.
//

class DbQueryKey
{
	friend class DbConstIndex;
	friend class DbKeyComparator;
	
public:
	DbQueryKey(const DbConstIndex &index);
	
private:
	WriteSection mKeyData;
	uint32 mNumKeyValues;
	const DbConstIndex &mIndex;
	const ReadSection &mTableSection;
	CSSM_DB_OPERATOR mOp;
};

//
// An object which performs comparison between keys, either stored
// in a database or provided as part of a query.
//

class DbKeyComparator
{
public:
	DbKeyComparator(const DbQueryKey &key) : mKey(key) {}

	bool operator () (uint32 keyOffset1, uint32 keyOffset2) const;

	// Pass this value as an argument to
	// operator()(uint32,uint32) to compare against mKey.
	static const uint32 kUseQueryKeyOffset = 0;

private:
	const DbQueryKey &mKey;
};

//
// A key as stored in an index.
//

class DbIndexKey {
public:
	DbIndexKey(const ReadSection &key, const Range &keyRange, const DbIndex &index)
		: mKeySection(key), mKeyRange(keyRange), mIndex(index) {}

	bool operator < (const DbIndexKey &other) const;
	
	uint32 keySize() const { return mKeyRange.mSize; }
	const uint8 *keyData() const { return mKeySection.range(mKeyRange); }
	
private:
	// the key data, expressed as a subsection of a read section
	const ReadSection &mKeySection;
	Range mKeyRange;
	
	// the index that knows how to interpret the key data
	const DbIndex &mIndex;
};

// Base class containing stuff shared between const and mutable indexes.

class DbIndex
{
	friend class DbIndexKey;
	
public:
	uint32 indexId() const { return mIndexId; }

	// append an attribute to the index key
	void appendAttribute(uint32 attributeId);

protected:
	DbIndex(const MetaRecord &metaRecord, uint32 indexId, bool isUniqueIndex);

	// meta record for table associated with this index
	const MetaRecord &mMetaRecord;

	// vector of indexed attributes
	typedef vector<const MetaAttribute *> AttributeVector;
	AttributeVector mAttributes;
	
	uint32 mIndexId;
	bool mIsUniqueIndex;
};

// Read-only index.

class DbConstIndex : public DbIndex
{
	friend class DbMutableIndex;
	friend class DbQueryKey;
	friend class DbKeyComparator;

public:
	DbConstIndex(const Table &table, uint32 indexId, bool isUniqueIndex);
	DbConstIndex(const Table &table, const ReadSection &indexSection);
	
	const Table &table() const { return mTable; }

	// check if this index can be used for a given query, and if so, generate
	// the appropriate index key from the query
	bool matchesQuery(const CSSM_QUERY &query, DbQueryKey *&queryKey) const;

	// perform a query on the index
	void performQuery(const DbQueryKey &queryKey,
		DbIndexIterator &begin, DbIndexIterator &end) const;
		
	// given an iterator as returned by performQuery(), return the read section for the record
	ReadSection getRecordSection(DbIndexIterator iter) const;

private:
	// sorted vector of offsets to index key data
	DbOffsetVector mKeyOffsetVector;
	
	// vector, in same order as key vector, of corresponding record numbers
	DbOffsetVector mRecordNumberVector;

	const Table &mTable;
};

// A memory-resident index that can be modified, but not used for a query.

class DbMutableIndex : public DbIndex
{
public:
	DbMutableIndex(const DbConstIndex &index);
	DbMutableIndex(const MetaRecord &metaRecord, uint32 indexId, bool isUniqueIndex);
	~DbMutableIndex();
	
	// insert a record into the index
	void insertRecord(uint32 recordNumber, const ReadSection &packedRecord);
	
	// remove a record from the index
	void removeRecord(uint32 recordNumber);
	
	// write the index
	uint32 writeIndex(WriteSection &ws, uint32 offset);
	
private:
	// helper methods called by insertRecord()
	void insertRecordSingle(uint32 recordOffset, const ReadSection &packedRecord);
	void insertRecordMulti(uint32 recordOffset, const ReadSection &packedRecord,
		uint32 attributeIndex, WriteSection &keyData, uint32 keySize);

	// a single write section which stores generated index key data
	WriteSection mIndexData;
	uint32 mIndexDataSize;
	
	// a map from index keys to record numbers
	typedef multimap<DbIndexKey, uint32> IndexMap;
	IndexMap mMap;
};

} // end namespace Security

#endif // _H_APPLEDL_DBINDEX
