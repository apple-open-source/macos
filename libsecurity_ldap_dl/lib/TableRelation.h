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

#ifndef __TABLE_RELATION__
#define __TABLE_RELATION__


#include "PartialRelation.h"

/* 
	a table relation is a relation which is completely stored in memory -- used for
	indexes, lists of relations, lists of attributes, etc.
*/

// a unique identifier for a table
class TableUniqueIdentifier : public UniqueIdentifier
{
protected:
	uint32 mTupleNumber;

public:
	TableUniqueIdentifier (CSSM_DB_RECORDTYPE recordType, int mTupleNumber);
	virtual void Export (CSSM_DB_UNIQUE_RECORD &record);
	
	int GetTupleNumber () {return mTupleNumber;}
};



// a table relation.  Uses PartialRelation to track basic info.
class TableRelation : public PartialRelation
{
protected:
	int mNumberOfTuples;
	Value** mData;

public:
	TableRelation (CSSM_DB_RECORDTYPE recordType, int numberOfColumns, columnInfoLoader *theColumnInfo);
	virtual ~TableRelation ();
	
	void AddTuple (Value* column0Value, ...);

	virtual Query* MakeQuery (const CSSM_QUERY* query);
	virtual Tuple* GetTupleFromUniqueIdentifier (UniqueIdentifier* uniqueID);
	virtual Tuple* GetTuple (int i);
	virtual UniqueIdentifier* ImportUniqueIdentifier (CSSM_DB_UNIQUE_RECORD *uniqueRecord);
	int GetNumberOfTuples () {return mNumberOfTuples;}
};



// a tuple for a TableRelation
class TableTuple : public Tuple
{
protected:
	Value** mValues;
	int mNumValues;

public:
	TableTuple (Value** offset, int numValues);
	virtual int GetNumberOfValues () {return mNumValues;}
	virtual Value* GetValue (int which) {return mValues[which];}
	virtual void GetData (CSSM_DATA &data);
};



// a query for a TableRelation
class TableQuery : public Query
{
protected:
	TableRelation* mRelation;
	int mCurrentRecord;

public:
	TableQuery (TableRelation* relation, const CSSM_QUERY *queryBase);
	~TableQuery ();
	
	virtual Tuple* GetNextTuple (UniqueIdentifier *&id);
};



#endif
