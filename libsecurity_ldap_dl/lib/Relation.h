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

#ifndef __RELATION__
#define __RELATION__


#include <Security/Security.h>
#include <string>
#include <sys/time.h>
#include <map>
#include "AttachedInstance.h"

// abstract class that defines the internal storage used by the relation classes
class Value
{
protected:
	CSSM_DB_ATTRIBUTE_FORMAT mBaseFormat;

public:
	Value (CSSM_DB_ATTRIBUTE_FORMAT format);
	virtual ~Value ();

	virtual CSSM_DB_ATTRIBUTE_FORMAT GetValueType () {return mBaseFormat;}	// get the value type
	virtual bool Compare (Value *v, CSSM_DB_OPERATOR op) = 0;				// compare to another type
	virtual uint8* CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size) = 0; // copy with user's allocator (stored in attached instance)
	static Value* MakeValueFromAttributeData (const CSSM_DB_ATTRIBUTE_DATA& info); // make from attribute data
};



// represents a text string
class StringValue : public Value
{
protected:
	std::string mValue;

public:
	StringValue (const char* value);								// make from a c-style string
	StringValue (const char* value, uint32 length);					// make from data and length
	const char* GetRawValue () {return mValue.c_str ();}			// get the internal storage as a c string
	std::string& GetRawValueAsStdString () {return mValue;}			// get the internal storage as a c++ string
	uint8* CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size);
	virtual bool Compare (Value *v, CSSM_DB_OPERATOR op);
};



// represents a signed integer
class SInt32Value : public Value
{
protected:
	sint32 mValue;

public:
	SInt32Value (const sint32 value);
	const sint32 GetRawValue () {return mValue;}
	uint8* CloneContents (AttachedInstance* ai, uint32 &numberOfItems, uint32 &size);
	virtual bool Compare (Value *v, CSSM_DB_OPERATOR op);
};



// represents an signed integer
class UInt32Value : public Value
{
protected:
	uint32 mValue;

public:
	UInt32Value (const uint32 value);
	const uint32 GetRawValue () {return mValue;}
	uint8* CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size);
	virtual bool Compare (Value *v, CSSM_DB_OPERATOR op);
};



// represents a CSSM-style BigNum
class BigNumValue : public Value
{
protected:
	uint8* mValue;
	size_t mSize;
	int mLengthCache;

	bool GetSignBit ();
	int GetAdjustedLength ();
	int GetByte (int which);

	bool CompareSignBits (BigNumValue* compare, int &result);
	bool CompareLengths (BigNumValue* compare, int &result);
	bool CompareValues (BigNumValue* compare, int &result);

public:
	BigNumValue (const uint8* value, size_t size);
	~BigNumValue ();
	
	const uint8* GetRawValue (size_t &size) {size = mSize; return mValue;}
	uint8* CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size);
	virtual bool Compare (Value *v, CSSM_DB_OPERATOR op);
};


// represents an IEEE double
class RealValue : public Value
{
protected:
	double mValue;

public:
	RealValue (double value);
	const double GetRawValue () {return mValue;}
	uint8* CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size);
	virtual bool Compare (Value *v, CSSM_DB_OPERATOR op);
};



// represents a CSSM time/date
class TimeDateValue : public Value
{
protected:
	time_t mValue;

public:
	TimeDateValue (time_t tv);
	TimeDateValue (const char* td);
	time_t GetRawValue () {return mValue;}
	const char* GetValueAsTimeDate (size_t &size);
	virtual bool Compare (Value *v, CSSM_DB_OPERATOR op);
	uint8* CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size);
};



// represents a data blob
class BlobValue : public Value
{
protected:
	uint8* mValue;
	size_t mSize;

public:
	BlobValue (const uint8* value, size_t size);
	BlobValue (CSSM_DATA	data);
	BlobValue (CFDataRef	data);
	BlobValue (CFStringRef	data);
	const uint8* GetRawValue (size_t &size) {size = mSize; return mValue;}
	virtual ~BlobValue ();
	uint8* CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size);
	virtual bool Compare (Value *v, CSSM_DB_OPERATOR op);
};



// abstract class which represents a record in a relation
class Tuple
{
public:
	Tuple ();
	virtual ~Tuple ();
	
	virtual int GetNumberOfValues () = 0;
	virtual Value* GetValue (int which) = 0;
	virtual void GetData (CSSM_DATA& data) = 0;
};



// abstract class which represents a unique identifier for a record
class UniqueIdentifier
{
protected:
	CSSM_DB_RECORDTYPE mRecordType;

public:
	UniqueIdentifier (CSSM_DB_RECORDTYPE recordType);
	virtual ~UniqueIdentifier ();

	virtual void Export (CSSM_DB_UNIQUE_RECORD &record) = 0;

	CSSM_DB_RECORDTYPE GetRecordType () {return mRecordType;}
};



// wrapper around a CSSM_SELECTION_PREDICATE
class CssmSelectionPredicate : public CSSM_SELECTION_PREDICATE
{
protected:
	void CloneCssmSelectionPredicate (CssmSelectionPredicate &a, const CssmSelectionPredicate &b);

public:
	CssmSelectionPredicate () {}
	CssmSelectionPredicate (const CssmSelectionPredicate& pred);
	~CssmSelectionPredicate ();
	
	void operator= (const CssmSelectionPredicate& pred);
	
	CSSM_DB_ATTRIBUTE_NAME_FORMAT GetAttributeNameFormat () {return Attribute.Info.AttributeNameFormat;}
	uint32 GetAttributeID () {return Attribute.Info.Label.AttributeID;}
	char* GetAttributeName () {return Attribute.Info.Label.AttributeName;}
	uint32 GetNumAttributes () {return Attribute.NumberOfValues;}
	CSSM_DATA& GetValue (int i) {return Attribute.Value[i];}
	CSSM_DB_OPERATOR GetOperator () {return DbOperator;}
};



class Relation;



// a partially abstract class which represents a query.  Provides partial support for queries, including
// tuple evaluation
class Query
{
protected:
	uint32 mNumSelectionPredicates;							// number of selection predicates
	CssmSelectionPredicate *mSelectionPredicates;			// the selection predicates
	CSSM_DB_CONJUNCTIVE mConjunction;						// AND or OR
	Value** mValues;										// values to compare against
	int *mColumnIDs;										// id's for the columns
	Relation* mRelation;									// relation being searched

	bool EvaluateTuple (Tuple *t);							// filter a tuple based on the selection predicates

public:
	Query (Relation* relation, const CSSM_QUERY *queryBase);
	virtual ~Query ();
	Relation* GetRelation () {return mRelation;}
	
	virtual Tuple* GetNextTuple (UniqueIdentifier *&id) = 0; // overload to get the next tuple in the relation that
															 // matches the query
};



// abstract base class for a relation
class Relation
{
protected:
	CSSM_DB_RECORDTYPE mRecordType;

public:
	Relation (CSSM_DB_RECORDTYPE recordType);
	virtual ~Relation ();
	
	CSSM_DB_RECORDTYPE GetRecordType () {return mRecordType;}

	// virtual Tuple* GetColumnNames () = 0;
	virtual uint32 GetColumnIDs (int i) = 0;
	virtual int GetNumberOfColumns () = 0;
	virtual int GetColumnNumber (const char* columnName) = 0;
	virtual int GetColumnNumber (uint32 columnID) = 0;
	virtual Query* MakeQuery (const CSSM_QUERY* query) = 0;
	virtual Tuple* GetTupleFromUniqueIdentifier (UniqueIdentifier* uniqueID) = 0;
	virtual UniqueIdentifier* ImportUniqueIdentifier (CSSM_DB_UNIQUE_RECORD *uniqueRecord) = 0;
};



typedef std::map<CSSM_DB_RECORDTYPE, Relation*> RelationMap;



#endif
