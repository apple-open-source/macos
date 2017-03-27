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
// DbIndex.cpp
//

#include "DbIndex.h"
#include "AppleDatabase.h"
#include <stdio.h>

DbQueryKey::DbQueryKey(const DbConstIndex &index)
:	mIndex(index),
	mTableSection(index.table().getTableSection())
{
}

// Perform a less-than comparison between two keys. An offset of
// kUseQueryKeyOffset means to use the key provided as part of the
// query; otherwise, the key comes from the database.

const uint32 DbKeyComparator::kUseQueryKeyOffset;

bool
DbKeyComparator::operator () (uint32 offset1, uint32 offset2) const
{
	ReadSection rs1, rs2;
	const ReadSection *key1, *key2;

	// get the read sections to compare
	
	if (offset1 == kUseQueryKeyOffset)
		key1 = &mKey.mKeyData;
	else {
		rs1 = mKey.mTableSection.subsection(offset1);
		key1 = &rs1;
	}
	
	if (offset2 == kUseQueryKeyOffset)
		key2 = &mKey.mKeyData;
	else {
		rs2 = mKey.mTableSection.subsection(offset2);
		key2 = &rs2;
	}

	// compare the values of the attributes in the keys
	
	uint32 valueOffset1 = sizeof(uint32), valueOffset2 = sizeof(uint32);
	
	for (uint32 i = 0; i < mKey.mNumKeyValues; i++) {
		const MetaAttribute &metaAttribute = *mKey.mIndex.mAttributes[i];
		auto_ptr<DbValue> value1(metaAttribute.createValue(*key1, valueOffset1));
		auto_ptr<DbValue> value2(metaAttribute.createValue(*key2, valueOffset2));
		
		if (metaAttribute.evaluate(value1.get(), value2.get(), CSSM_DB_LESS_THAN))
			return true;
			
		else if (metaAttribute.evaluate(value2.get(), value1.get(), CSSM_DB_LESS_THAN))
			return false;
	}

	// if we are here, the keys are equal

	return false;
}

// Comparison used when inserting an item into an index, but otherwise
// similar to the version above.

bool
DbIndexKey::operator < (const DbIndexKey &other) const
{
	// compare the values of the attributes in the keys
	
	uint32 numAttributes = (uint32) mIndex.mAttributes.size();
	uint32 valueOffset1 = 0, valueOffset2 = 0;
	
	for (uint32 i = 0; i < numAttributes; i++) {
		const MetaAttribute &metaAttribute = *mIndex.mAttributes[i];
		auto_ptr<DbValue> value1(metaAttribute.createValue(mKeySection.subsection(mKeyRange),
			valueOffset1));
		auto_ptr<DbValue> value2(metaAttribute.createValue(other.mKeySection.subsection(other.mKeyRange),
			valueOffset2));
		
		if (metaAttribute.evaluate(value1.get(), value2.get(), CSSM_DB_LESS_THAN))
			return true;
			
		else if (metaAttribute.evaluate(value2.get(), value1.get(), CSSM_DB_LESS_THAN))
			return false;
	}

	// if we are here, the keys are equal

	return false;
}

DbIndex::DbIndex(const MetaRecord &metaRecord, uint32 indexId, bool isUniqueIndex)
:	mMetaRecord(metaRecord),
	mIndexId(indexId),
	mIsUniqueIndex(isUniqueIndex)
{
}

// Append an attribute to the vector used to form index keys.

void
DbIndex::appendAttribute(uint32 attributeId)
{
	CSSM_DB_ATTRIBUTE_INFO info;
	info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
	info.Label.AttributeID = attributeId;

	mAttributes.push_back(&(mMetaRecord.metaAttribute(info)));
}

// Construct a new read-only index.

DbConstIndex::DbConstIndex(const Table &table, uint32 indexId, bool isUniqueIndex)
:	DbIndex(table.getMetaRecord(), indexId, isUniqueIndex),
	mTable(table)
{
}

DbConstIndex::DbConstIndex(const Table &table, const ReadSection &indexSection)
:	DbIndex(table.getMetaRecord(), indexSection.at(AtomSize), indexSection.at(2 * AtomSize)),
	mTable(table)
{
	uint32 numAttributes = indexSection.at(3 * AtomSize);

	for (uint32 i = 0; i < numAttributes; i++) {
		uint32 attributeId = indexSection.at((4 + i) * AtomSize);
		appendAttribute(attributeId);
	}

	uint32 offset = (4 + numAttributes) * AtomSize;
	uint32 numRecords = indexSection.at(offset);
	offset += AtomSize;
	mKeyOffsetVector.overlay(numRecords,
		reinterpret_cast<const Atom *>(indexSection.range(Range(offset, numRecords * AtomSize))));

	offset += numRecords * AtomSize;
	mRecordNumberVector.overlay(numRecords,
		reinterpret_cast<const Atom *>(indexSection.range(Range(offset, numRecords * AtomSize))));
}

// Check to see if this index can be used to perform a given query, based on
// the attributes used in the query and their order. They must be a prefix
// of the index key attributes. If there is more than one attribute, all of the
// operators must be EQUAL and the conjunctive must be AND; this is needed to
// ensure that the results are a contiguous segment of the index. On success,
// the appropriate index key is generated from the query.

bool
DbConstIndex::matchesQuery(const CSSM_QUERY &query, DbQueryKey *&queryKey) const
{
	uint32 numPredicates = query.NumSelectionPredicates;

	if (numPredicates == 0 || numPredicates > mAttributes.size())
		return false;
	
	// determine which index attributes are used in the query
	
	auto_array<uint32> attributeUsed(mAttributes.size());
	for (uint32 i = 0; i < mAttributes.size(); attributeUsed[i++] = ~(uint32)0);
	
	for (uint32 i = 0, j; i < numPredicates; i++) {
		const MetaAttribute &tableAttribute =
			mMetaRecord.metaAttribute(query.SelectionPredicate[i].Attribute.Info);
		
		for (j = 0; j < mAttributes.size(); j++) {
			if (tableAttribute.attributeId() == mAttributes[j]->attributeId()) {
				if (attributeUsed[j] != ~(uint32)0)
					// invalid query: attribute appears twice
					CssmError::throwMe(CSSMERR_DL_INVALID_QUERY);
				else {
					// the jth index component is the ith predicate in the query
					attributeUsed[j] = i;
					break;
				}
			}
		}
		
		if (j == mAttributes.size()) {
			// the predicate attribute is not in the index, so return failure
			return false;
		}
	}
	
	// check that the query predicates form a prefix of the index key, which means that
	// the first N index components are the N query predicates in some order
	
	long lastIndex;
	for (lastIndex = mAttributes.size() - 1; (lastIndex >= 0) && (attributeUsed[lastIndex] == ~(uint32)0);
		lastIndex--);
		
	if (lastIndex != numPredicates - 1)
		return false;

	// if there is more than one predicate, the conjunctive must be AND and all the
	// operators must be EQUAL for the compound index to be useful
	
	CSSM_DB_OPERATOR op;
	
	if (numPredicates > 1) {
		if (query.Conjunctive != CSSM_DB_AND)
			return false;
			
		for (uint32 i = 0; i < numPredicates; i++)
			if (query.SelectionPredicate[i].DbOperator != CSSM_DB_EQUAL)
				return false;
				
		op = CSSM_DB_EQUAL;
	}
		
	// for a single predicate, check the operator
	
	else {
		op = query.SelectionPredicate[0].DbOperator;
		if (op != CSSM_DB_EQUAL && op != CSSM_DB_LESS_THAN && op != CSSM_DB_GREATER_THAN)
			return false;
	}

	// ok, after all that, we can use this index, so generate an object used as a key
	// for this query on this index
	
	queryKey = new DbQueryKey(*this);
	queryKey->mNumKeyValues = numPredicates;
	queryKey->mOp = op;
	
	uint32 keyLength = sizeof(uint32);
	for (uint32 i = 0; i < numPredicates; i++)
		mAttributes[i]->packValue(queryKey->mKeyData, keyLength,
			*(query.SelectionPredicate[attributeUsed[i]].Attribute.Value));
	queryKey->mKeyData.put(0, keyLength - sizeof(uint32));
	queryKey->mKeyData.size(keyLength);
	
	return true;
}

// Perform a query on an index, returning the iterators that bound the
// returned results.

void
DbConstIndex::performQuery(const DbQueryKey &queryKey,
	DbIndexIterator &begin, DbIndexIterator &end) const
{
	DbKeyComparator cmp(queryKey);
	
	switch (queryKey.mOp) {
	
	case CSSM_DB_EQUAL:
		{
			pair<DbIndexIterator, DbIndexIterator> result;
			result = equal_range(mKeyOffsetVector.begin(), mKeyOffsetVector.end(),
				DbKeyComparator::kUseQueryKeyOffset, cmp);
			begin = result.first;
			end = result.second;
		}
		break;
		
	case CSSM_DB_LESS_THAN:
		begin = mKeyOffsetVector.begin();
		end = lower_bound(begin, mKeyOffsetVector.end(),
				DbKeyComparator::kUseQueryKeyOffset, cmp);
		break;
		
	case CSSM_DB_GREATER_THAN:
		end = mKeyOffsetVector.end();
		begin = lower_bound(mKeyOffsetVector.begin(), end,
				DbKeyComparator::kUseQueryKeyOffset, cmp);
		break;
		
	default:
		CssmError::throwMe(CSSMERR_DL_INTERNAL_ERROR);
	}
}

// Given an iterator as returned by performQuery(), return the read section for the record.

ReadSection
DbConstIndex::getRecordSection(DbIndexIterator iter) const
{
	uint32 recordNumber = mRecordNumberVector[iter - mKeyOffsetVector.begin()];
	return mTable.getRecordSection(recordNumber);
}

// Construct a mutable index from a read-only index.

DbMutableIndex::DbMutableIndex(const DbConstIndex &index)
:	DbIndex(index),
	mIndexDataSize(0)
{
	// go through the const index and copy all the entries into the
	// mutable index
	
	const ReadSection &tableSection = index.mTable.getTableSection();
	
	size_t numRecords = index.mKeyOffsetVector.size();
	for (size_t i = 0; i < numRecords; i++) {
		uint32 recordNumber = index.mRecordNumberVector.at(i);
		uint32 keyOffset = index.mKeyOffsetVector.at(i);
		uint32 keySize = tableSection.at(keyOffset);
		DbIndexKey key(tableSection, Range(keyOffset + AtomSize, keySize), *this);
		mMap.insert(IndexMap::value_type(key, recordNumber));
	}
}

DbMutableIndex::DbMutableIndex(const MetaRecord &metaRecord, uint32 indexId, bool isUniqueIndex)
:	DbIndex(metaRecord, indexId, isUniqueIndex),
	mIndexDataSize(0)
{
}

DbMutableIndex::~DbMutableIndex()
{
}

// Remove all entries for a record from an index. This is not an ideal implementation,
// since it walks the entire index. In a perfect world, we'd generate all the record's
// keys and lookup matching entries, deleting only those with the correct record number.
// But this is not a perfect world.

void
DbMutableIndex::removeRecord(uint32 recordNumber)
{
	IndexMap::iterator it, temp;
	for (it = mMap.begin(); it != mMap.end(); ) {
		temp = it; it++;
		if (temp->second == recordNumber)
			mMap.erase(temp);
	}
}

// Insert a record into an index.

void
DbMutableIndex::insertRecord(uint32 recordNumber, const ReadSection &packedRecord)
{
	// The common case is that each indexed attribute has a single value in
	// the record; detect and handle this separately since we can avoid an
	// expensive recursive technique.
	
	size_t numAttributes = mAttributes.size();
	bool allSingleValued = true;
	
	for (size_t i = 0; i < numAttributes; i++) {
		uint32 numValues = mAttributes[i]->getNumberOfValues(packedRecord);
		if (numValues == 0) {
			// record does not have value required by index; for a unique index,
			// this is an error, otherwise just don't index the record
			if (mIsUniqueIndex)
				CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);
			else
				return;
		}
		else if (numValues > 1) {
			allSingleValued = false;
			break;
		}
	}
	
	if (allSingleValued)
		insertRecordSingle(recordNumber, packedRecord);
		
	else {
		// recursively build all appropriate index keys, and add them to the map
		WriteSection keyData;
		insertRecordMulti(recordNumber, packedRecord, 0, keyData, 0);
	}
}

void
DbMutableIndex::insertRecordSingle(uint32 recordNumber, const ReadSection &packedRecord)
{
	// append the key values to the index data
	uint32 offset = mIndexDataSize;
	for (uint32 i = 0; i < mAttributes.size(); i++)
		mAttributes[i]->copyValueBytes(0, packedRecord, mIndexData, mIndexDataSize);
	mIndexData.size(mIndexDataSize);
		
	// make an index key
	DbIndexKey key(mIndexData, Range(offset, mIndexDataSize - offset), *this);
	
	// if this is a unique index, check for a record with the same key
	if (mIsUniqueIndex && (mMap.find(key) != mMap.end()))
		// the key already exists, which is an error
		CssmError::throwMe(CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA);
		
	// insert the item into the map
	mMap.insert(IndexMap::value_type(key, recordNumber));
}

void
DbMutableIndex::insertRecordMulti(uint32 recordNumber, const ReadSection &packedRecord,
	uint32 attributeIndex, WriteSection &keyData, uint32 keySize)
{
	const MetaAttribute &metaAttribute = *(mAttributes[attributeIndex]);
	uint32 numValues = metaAttribute.getNumberOfValues(packedRecord);

	for (uint32 i = 0; i < numValues; i++) {
	
		uint32 newKeySize = keySize;
		metaAttribute.copyValueBytes(i, packedRecord, keyData, newKeySize);
		
		if (attributeIndex + 1 == mAttributes.size()) {
			uint32 offset = mIndexDataSize;
			mIndexDataSize = mIndexData.put(mIndexDataSize, newKeySize, keyData.address());
			mIndexData.size(mIndexDataSize);

			DbIndexKey key(mIndexData, Range(offset, mIndexDataSize - offset), *this);
			if (mIsUniqueIndex && (mMap.find(key) != mMap.end()))
				CssmError::throwMe(CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA);

			mMap.insert(IndexMap::value_type(key, recordNumber));
		}
		else
			// otherwise, recurse with the rest of the attributes
			insertRecordMulti(recordNumber, packedRecord, attributeIndex + 1, keyData, newKeySize);
	}
}

uint32
DbMutableIndex::writeIndex(WriteSection &ws, uint32 offset)
{
	IndexMap::iterator it;
	
	// reserve space for the index size
	uint32 sizeOffset = offset;
	offset += AtomSize;
	
	offset = ws.put(offset, mIndexId);
	offset = ws.put(offset, mIsUniqueIndex ? 1 : 0);
	
	offset = ws.put(offset, (uint32)mAttributes.size());
	for (uint32 i = 0; i < mAttributes.size(); i++)
		offset = ws.put(offset, mAttributes[i]->attributeId());

	offset = ws.put(offset, (uint32)mMap.size());
	
	// reserve space for the array of offsets to key data
	uint32 keyPtrOffset = offset;
	offset += AtomSize * mMap.size();
	
	// write the array of record numbers
	for (it = mMap.begin(); it != mMap.end(); it++) {
		offset = ws.put(offset, it->second);
	}
		
	// write the key data
	for (it = mMap.begin(); it != mMap.end(); it++) {
		keyPtrOffset = ws.put(keyPtrOffset, offset);
		offset = ws.put(offset, it->first.keySize());
		offset = ws.put(offset, it->first.keySize(), it->first.keyData());
	}
	
	// write the index size
	ws.put(sizeOffset, offset - sizeOffset);

	return offset;	
}
