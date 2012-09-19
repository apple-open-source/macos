/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "TableRelation.h"
#include "CommonCode.h"

struct TableRelationStruct
{
	CSSM_DB_RECORDTYPE recordType;
};



void TableTuple::GetData (CSSM_DATA &data)
{
	data.Data = NULL;
	data.Length = 0;
}



void TableUniqueIdentifier::Export (CSSM_DB_UNIQUE_RECORD &record)
{
	// we don't care about any of the fields of this record, so just zero it out.
	memset (&record, 0, sizeof (record));
}



TableUniqueIdentifier::TableUniqueIdentifier (CSSM_DB_RECORDTYPE recordType, int tupleNumber) : UniqueIdentifier (recordType), mTupleNumber (tupleNumber)
{
}



struct TableUniqueIdentifierStruct : public TableRelationStruct
{
	int tupleNumber;
};



TableRelation::TableRelation (CSSM_DB_RECORDTYPE recordType, int numberOfColumns, columnInfoLoader *theColumnInfo) : PartialRelation (recordType, numberOfColumns, theColumnInfo), mNumberOfTuples (0), mData (NULL)
{
}



TableRelation::~TableRelation ()
{
	if (mData != NULL)
	{
		int arraySize = mNumberOfTuples * mNumberOfColumns;
		int i;
		for (i = 0; i < arraySize; ++i)
		{
			delete mData[i];
		}
		
		free (mData);
	}
}



void TableRelation::AddTuple (Value* column0Value, ...)
{
	// extend the tuple array by the number of tuple to be added
	int n = mNumberOfTuples++ * mNumberOfColumns;
	int newArraySize = n + mNumberOfColumns;
	mData = (Value**) realloc (mData, newArraySize * sizeof (Value*));
	
	mData[n++] = column0Value;
	
	va_list argList;
	va_start (argList, column0Value);

	int i;
	for (i = 1; i < mNumberOfColumns; ++i)
	{
		Value* next = va_arg (argList, Value*);
		mData[n++] = next;
	}
	
	va_end (argList);
}



Query* TableRelation::MakeQuery (const CSSM_QUERY* query)
{
	return new TableQuery (this, query);
}



Tuple* TableRelation::GetTuple (int i)
{
	Value** offset = mData + i * mNumberOfColumns;
	TableTuple* tt = new TableTuple (offset, mNumberOfColumns);
	return tt;
}



Tuple* TableRelation::GetTupleFromUniqueIdentifier (UniqueIdentifier* uniqueID)
{
	TableUniqueIdentifier *id = (TableUniqueIdentifier*) uniqueID;
	return GetTuple (id->GetTupleNumber ());
}



UniqueIdentifier* TableRelation::ImportUniqueIdentifier (CSSM_DB_UNIQUE_RECORD *uniqueRecord)
{
	TableUniqueIdentifierStruct *is = (TableUniqueIdentifierStruct *) uniqueRecord->RecordIdentifier.Data;
	TableUniqueIdentifier* it = new TableUniqueIdentifier (is->recordType, is->tupleNumber);
	return it;
}



TableTuple::TableTuple (Value** offset, int numValues) : mValues (offset), mNumValues (numValues)
{
}



TableQuery::TableQuery (TableRelation* relation, const CSSM_QUERY *queryBase) : Query (relation, queryBase), mRelation (relation), mCurrentRecord (0)
{
}



TableQuery::~TableQuery ()
{
}



Tuple* TableQuery::GetNextTuple (UniqueIdentifier *& id)
{
	while (mCurrentRecord < mRelation->GetNumberOfTuples ()) {
		Tuple *t = mRelation->GetTuple (mCurrentRecord);

		if (EvaluateTuple (t)) {
			id = new TableUniqueIdentifier (mRelation->GetRecordType (), mCurrentRecord);
			mCurrentRecord += 1;
			return t;
		}
		mCurrentRecord += 1;
		delete t;
	}
	
	id = NULL;
	return NULL;
}
