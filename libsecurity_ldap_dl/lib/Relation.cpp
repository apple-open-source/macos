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

#include "Relation.h"
#include <time.h>
#include "CommonCode.h"

Value::Value (CSSM_DB_ATTRIBUTE_FORMAT format) : mBaseFormat (format)
{
}



Value::~Value ()
{
}



Value* Value::MakeValueFromAttributeData (const CSSM_DB_ATTRIBUTE_DATA& info)
{
	switch (info.Info.AttributeFormat) {
	case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
		return new StringValue ((char*) info.Value->Data, info.Value->Length);
	
	case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
		return new SInt32Value (*(sint32*) info.Value->Data);
	
	case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
		return new UInt32Value (*(uint32*) info.Value->Data);

	case CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM:
		return new BigNumValue (info.Value->Data, info.Value->Length);
	
	case CSSM_DB_ATTRIBUTE_FORMAT_REAL:
		return new RealValue (*(double*) info.Value->Data);
	
	case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE:
		return new TimeDateValue ((char*) info.Value->Data);
	
	case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
		return new BlobValue (info.Value->Data, info.Value->Length);
	}
	CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_FIELD_FORMAT);
}



StringValue::StringValue (const char* value) : Value (CSSM_DB_ATTRIBUTE_FORMAT_STRING), mValue (value)
{
}



StringValue::StringValue (const char* value, uint32 length) : Value (CSSM_DB_ATTRIBUTE_FORMAT_STRING), mValue (value, length)
{
}



uint8* StringValue::CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size)
{
	// clone the string
	size = mValue.length ();
	char* s = (char*) ai->malloc (size + 1);
	strcpy (s, mValue.c_str ());
	numberOfItems = 1;
	return (uint8*) s;
}



bool StringValue::Compare (Value* v, CSSM_DB_OPERATOR op)
{
	if (v->GetValueType () != mBaseFormat)
		CSSMError::ThrowCSSMError (CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	
	StringValue* sv = (StringValue*) v;
	
	const char* vRaw = sv->GetRawValue ();
	const char* myRaw = GetRawValue ();

	switch (op) {
	case CSSM_DB_EQUAL:
		return strcmp (myRaw, vRaw) == 0;
	case CSSM_DB_NOT_EQUAL:
		return strcmp (myRaw, vRaw) != 0;
	case CSSM_DB_LESS_THAN:
		return strcmp (myRaw, vRaw) < 0;
	case CSSM_DB_GREATER_THAN:
		return strcmp (myRaw, vRaw) > 0;
	default:
		break;
	}
		
	const char* strLocation = strstr (vRaw, myRaw);

	switch (op) {
	case CSSM_DB_CONTAINS:
		return strLocation != NULL;
	case CSSM_DB_CONTAINS_INITIAL_SUBSTRING:
		return strLocation == myRaw;
	case CSSM_DB_CONTAINS_FINAL_SUBSTRING:
		int vRawLen = strlen (vRaw);
		int myRawLen = strlen (myRaw);
		return strLocation == myRaw + myRawLen - vRawLen;
	}
	
	CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_OPERATOR);
}



SInt32Value::SInt32Value (const sint32 value) : Value (CSSM_DB_ATTRIBUTE_FORMAT_SINT32), mValue (value) {}



bool SInt32Value::Compare (Value* v, CSSM_DB_OPERATOR op)
{
	if (v->GetValueType () != mBaseFormat)
		CSSMError::ThrowCSSMError (CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	
	SInt32Value* sv = (SInt32Value*) v;
	
	sint32 vRaw = sv->GetRawValue ();
	sint32 myRaw = GetRawValue ();

	switch (op) {
	case CSSM_DB_EQUAL:
		return vRaw == myRaw;
	case CSSM_DB_NOT_EQUAL:
		return vRaw != myRaw;
	case CSSM_DB_LESS_THAN:
		return myRaw < vRaw;
	case CSSM_DB_GREATER_THAN:
		return myRaw > vRaw;
	}
	
	CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_OPERATOR);
}



uint8* SInt32Value::CloneContents (AttachedInstance* ai, uint32 &numberOfItems, uint32 &size)
{
	sint32* result = (sint32*) ai->malloc (sizeof (sint32));
	*result = mValue;
	size = sizeof (sint32);
	numberOfItems = 1;
	return (uint8*) result;
}



UInt32Value::UInt32Value (const uint32 value) : Value (CSSM_DB_ATTRIBUTE_FORMAT_UINT32)
{
	mValue = value;
}



uint8* UInt32Value::CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size)
{
	uint32* result = (uint32*) ai->malloc (sizeof (uint32));
	*result = mValue;
	size = sizeof (uint32);
	numberOfItems = 1;
	return (uint8*) result;
}



bool UInt32Value::Compare (Value* v, CSSM_DB_OPERATOR op)
{
	if (v->GetValueType () != mBaseFormat)
		CSSMError::ThrowCSSMError (CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	
	UInt32Value* sv = (UInt32Value*) v;
	
	uint32 vRaw = sv->GetRawValue ();
	uint32 myRaw = GetRawValue ();

	switch (op) {
		case CSSM_DB_EQUAL:
			return vRaw == myRaw;
		case CSSM_DB_NOT_EQUAL:
			return vRaw != myRaw;
		case CSSM_DB_LESS_THAN:
			return myRaw < vRaw;
		case CSSM_DB_GREATER_THAN:
			return myRaw > vRaw;
	}
	
	CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_OPERATOR);
}



BigNumValue::BigNumValue (const uint8* value, size_t size) : Value (CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM), mLengthCache (-1)
{
	mSize = size;
	mValue = new uint8[size];
	memmove (mValue, value, size);
}



BigNumValue::~BigNumValue ()
{
	delete mValue;
}



uint8* BigNumValue::CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size)
{
	uint8* returnValue = (uint8*) ai->malloc (mSize);
	size = mSize;
	memmove (returnValue, mValue, size);
	numberOfItems = 1;
	return (uint8*) returnValue;
}




bool BigNumValue::GetSignBit ()
{
	return mValue[mSize - 1] & 0x80 != 0;
}



int BigNumValue::GetAdjustedLength ()
{
	if (mLengthCache != -1)
		return mLengthCache;
	
	// find the first non-zero byte.
	
	// Handle the sign bit properly
	int i = mSize - 1;
	int value = mValue[i--] & 0x7F;
	
	// search for the first non-zero byte
	while (i >= 0 && value == 0)
		value = mValue[i--];
	
	i += 1;
	if (i == 0) // zero length?
		mLengthCache = 1;
	else
		mLengthCache = i;
	
	return mLengthCache;
}



int BigNumValue::GetByte (int i)
{
	// return the bytes of the bignum, compensating for the sign bit
	if (i == (int) (mSize - 1))
		return mValue[i] & 0x7F;
	return mValue[i];
}



bool BigNumValue::CompareSignBits (BigNumValue *v, int &result)
{
	bool compareSignBit = v->GetSignBit ();
	bool mySignBit = GetSignBit ();
	
	if (!compareSignBit && mySignBit) {
		result = 1;
		return true;
	} else if (compareSignBit && !mySignBit) {
		result = -1;
		return true;
	}
	
	return false;
}



bool BigNumValue::CompareLengths (BigNumValue *v, int &result)
{
	int vSize = v->GetAdjustedLength ();
	int mySize = GetAdjustedLength ();
	
	if (vSize == mySize)
		return false;
	
	result = vSize - mySize;
	return true;
}



bool BigNumValue::CompareValues (BigNumValue *v, int &result)
{
	// handle the first byte specially, since it contains the sign bit
	int offset = GetAdjustedLength () - 1;

	result = v->GetByte(offset) - GetByte (offset);
	offset -= 1;

	while (offset >= 0 && result == 0)
		result = v->GetByte(offset) - GetByte (offset);
		offset -= 1;
	
	return true;
}



bool BigNumValue::Compare (Value *v, CSSM_DB_OPERATOR op)
{
	if (v->GetValueType () != mBaseFormat)
		CSSMError::ThrowCSSMError (CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	
	BigNumValue* sv = (BigNumValue*) v;
	
	int result;
	
	if (!CompareSignBits (sv, result))
		if (!CompareLengths (sv, result))
			CompareValues (sv, result);
	
	switch (op) {
	case CSSM_DB_EQUAL:
		return result == 0;
	case CSSM_DB_NOT_EQUAL:
		return result != 0;
	case CSSM_DB_LESS_THAN:
		return result < 0;
	case CSSM_DB_GREATER_THAN:
		return result > 0;
	}
	
	CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_OPERATOR);
}



RealValue::RealValue (double value) : Value (CSSM_DB_ATTRIBUTE_FORMAT_REAL), mValue (value) {}



uint8* RealValue::CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size)
{
	double* result = (double*) ai->malloc (sizeof (double));
	*result = (double) mValue;
	size = sizeof (double);
	numberOfItems = 1;
	return (uint8*) result;
}



bool RealValue::Compare (Value* v, CSSM_DB_OPERATOR op)
{
	if (v->GetValueType () != mBaseFormat)
		CSSMError::ThrowCSSMError (CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	
	RealValue* sv = (RealValue*) v;
	
	double vRaw = sv->GetRawValue ();
	double myRaw = GetRawValue ();

	switch (op) {
	case CSSM_DB_EQUAL:
		return vRaw == myRaw;
	case CSSM_DB_NOT_EQUAL:
		return vRaw != myRaw;
	case CSSM_DB_LESS_THAN:
		return myRaw < vRaw;
	case CSSM_DB_GREATER_THAN:
		return myRaw > vRaw;
	}
	
	CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_OPERATOR);
}



TimeDateValue::TimeDateValue (time_t tv) : Value (CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE), mValue (tv) {}



int CharToNum (char c)
{
	return c - '0';
}



TimeDateValue::TimeDateValue (const char* td) : Value (CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE)
{
	struct tm tmStruct;
	memset (&tmStruct, 0, sizeof (tmStruct));
	
	tmStruct.tm_year = CharToNum (td[0]) * 1000 + CharToNum (td[1]) * 100 + CharToNum (td[2]) * 10 + CharToNum (td[3]) - 1900;
	tmStruct.tm_mon = CharToNum (td[4]) * 10 + CharToNum (td[5]) - 1;
	tmStruct.tm_mday = CharToNum (td[6]) * 10 + CharToNum (td[7]);
	tmStruct.tm_hour = CharToNum (td[8]) * 10 + CharToNum (td[9]);
	tmStruct.tm_min = CharToNum (td[10]) * 10 + CharToNum (td[11]);
	tmStruct.tm_sec = CharToNum (td[12]) * 10 + CharToNum (td[13]);
	
	mValue = timegm (&tmStruct);
}



uint8* TimeDateValue::CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size)
{
	struct tm timeStruct;
	gmtime_r (&mValue, &timeStruct);
	
	char buffer[32];
	sprintf (buffer, "%04d%02d%02d%0d2%0d%02dZ", timeStruct.tm_year,
												 timeStruct.tm_mon + 1,
												 timeStruct.tm_mday,
												 timeStruct.tm_hour,
												 timeStruct.tm_min,
												 timeStruct.tm_sec);
	size = strlen (buffer);
	char* result = (char*) ai->malloc (size + 1);
	strcpy (result, buffer);
	numberOfItems = 1;
	return (uint8*) result;
}



bool TimeDateValue::Compare (Value* v, CSSM_DB_OPERATOR op)
{
	if (v->GetValueType () != mBaseFormat)
	{
		CSSMError::ThrowCSSMError (CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	}
	
	TimeDateValue* sv = (TimeDateValue*) v;
	
	time_t vRaw = sv->GetRawValue ();
	time_t myRaw = GetRawValue ();

	switch (op) {
	case CSSM_DB_EQUAL:
		return vRaw == myRaw;
	case CSSM_DB_NOT_EQUAL:
		return vRaw != myRaw;
	case CSSM_DB_LESS_THAN:
		return myRaw < vRaw;
	case CSSM_DB_GREATER_THAN:
		return myRaw > vRaw;
	}
	
	CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_OPERATOR);
}



BlobValue::BlobValue (const uint8* value, size_t size) : Value (CSSM_DB_ATTRIBUTE_FORMAT_BLOB), mSize (size)
{
	mValue = new uint8[size];
	memmove (mValue, value, size);
}



BlobValue::BlobValue (CSSM_DATA	data) : Value (CSSM_DB_ATTRIBUTE_FORMAT_BLOB), mSize (data.Length)
{
	mValue = new uint8[data.Length];
	memmove (mValue, data.Data, data.Length);
}


BlobValue::BlobValue (CFDataRef	data) : Value (CSSM_DB_ATTRIBUTE_FORMAT_BLOB), mSize (CFDataGetLength(data))
{
	mSize = CFDataGetLength(data);
	mValue = new uint8[mSize];
	memmove (mValue, CFDataGetBytePtr(data), mSize);
}

BlobValue::BlobValue (CFStringRef	data) : Value (CSSM_DB_ATTRIBUTE_FORMAT_BLOB), mSize (CFStringGetLength(data))
{
	mValue = new uint8[mSize+1];
	CFStringGetCString(data, (char *)mValue, mSize+1, kCFStringEncodingASCII);
}


BlobValue::~BlobValue ()
{
	delete mValue;
}


#ifdef NEVER
template<class T> void ComputeKMPNext (const T* substring, int64_t* nextArray, size_t substringLength)
{
	int i, j;
	nextArray[0] = -1;
	for (i = 0, j = -1; i < (ssize_t) substringLength; i++, j++, nextArray[i] = j)
	{
		while ((j >= 0) && (substring[i] != substring[j]))
		{
			j = nextArray[j];
		}
	}
}



template<class T> int KMPSearch (const T* substring, size_t subLength, const T* mainString, size_t mainLength)
{
	int i, j;
	
	// make a "next" array
	int64_t* nextArray = new int64_t[subLength];
	ComputeKMPNext (substring, nextArray, subLength);
	
	for (i = 0, j = 0; j < (ssize_t) subLength && i < (ssize_t) mainLength; ++i, ++j)
	{
		while ((j >= 0) && (mainString[i] != substring[j]))
		{
			j = nextArray[j];
		}
	}
	
	delete [] nextArray;

	if (j == (ssize_t) subLength)
	{
		return i - subLength;
	}
	
	return i;
}
#endif


static bool CompareBlobs (const uint8* a, size_t aLength, const uint8* b, size_t bLength)
{
	if (aLength != bLength) return false;
	for (size_t i = 0; i < aLength; ++i)
		if (a[i] != b[i]) return false;
	return true;
}



uint8* BlobValue::CloneContents (AttachedInstance *ai, uint32 &numberOfItems, uint32 &size)
{
	// clone the data
	size = mSize;
	uint8* returnValue = (uint8*) ai->malloc (mSize);
	memmove (returnValue, mValue, mSize);
	numberOfItems = 1;
	return (uint8*) returnValue;
}



bool BlobValue::Compare (Value* v, CSSM_DB_OPERATOR op)
{
	if (v->GetValueType () != mBaseFormat)
		CSSMError::ThrowCSSMError (CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	
	BlobValue* sv = (BlobValue*) v;
	
	const uint8 *vRaw, *myRaw;
	size_t vLength, myLength;
	
	vRaw = sv->GetRawValue (vLength);
	myRaw = GetRawValue (myLength);

	switch (op)
	{
		case CSSM_DB_CONTAINS:
		case CSSM_DB_EQUAL:
		case CSSM_DB_CONTAINS_INITIAL_SUBSTRING:
		case CSSM_DB_CONTAINS_FINAL_SUBSTRING:
			return CompareBlobs (vRaw, vLength, myRaw, myLength);
		
		case CSSM_DB_NOT_EQUAL:
			return !CompareBlobs (vRaw, vLength, myRaw, myLength);
	}
	
	CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_OPERATOR);
}



Tuple::Tuple () {}
Tuple::~Tuple () {}



Query::Query (Relation* relation, const CSSM_QUERY *queryBase) : mSelectionPredicates (NULL), mRelation (relation)
{	
	mConjunction = queryBase->Conjunctive;

	// fill out the rest of the fields based on queryBase
	
	mNumSelectionPredicates = queryBase->NumSelectionPredicates;
	if (mNumSelectionPredicates >= 2 && mConjunction == CSSM_DB_NONE)
		CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_QUERY);

	if (mNumSelectionPredicates >= 1) {
		mSelectionPredicates = new CssmSelectionPredicate[mNumSelectionPredicates];

		// copy the selection predicates
		unsigned i;
		for (i = 0; i < mNumSelectionPredicates; ++i)
			mSelectionPredicates[i] = *(CssmSelectionPredicate*) (queryBase->SelectionPredicate + i);
		
		// lookup the number of selection   
		mColumnIDs = new int [mNumSelectionPredicates];
		for (i = 0; i < mNumSelectionPredicates; ++i) {
			uint32 columnID;
			
			switch (mSelectionPredicates[i].GetAttributeNameFormat ()) {
			case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
				// if we have an attribute name format of CSSM_
				columnID = relation->GetColumnNumber (mSelectionPredicates[i].GetAttributeName ());
				break;
			case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
				columnID = mSelectionPredicates[i].GetAttributeID ();
				break;
			case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
				CSSMError::ThrowCSSMError (CSSMERR_DL_UNSUPPORTED_QUERY);
				break;
			}
			
			mColumnIDs[i] = columnID;
		}
		
		mValues = new Value*[mNumSelectionPredicates];
		for (i = 0; i < mNumSelectionPredicates; ++i)
			mValues[i] = Value::MakeValueFromAttributeData (mSelectionPredicates[i].Attribute);
	} else {
		mSelectionPredicates = NULL;
	}
}



Query::~Query ()
{
	if (mSelectionPredicates != NULL) {
		delete [] mSelectionPredicates;
		delete [] mColumnIDs;
		
		unsigned i;
		for (i = 0; i < mNumSelectionPredicates; ++i)
			delete mValues[i];
		
		delete [] mValues;
	}
}



bool Query::EvaluateTuple (Tuple *t)
{
	// do the easy case first
	if(!t) return false;
	if (mNumSelectionPredicates <= 0) return true;
	
	for(uint i=0; i < mNumSelectionPredicates; i++) {
		Value* v = t->GetValue (mColumnIDs[i]);
		if (v != NULL && v->Compare (mValues[i], mSelectionPredicates[i].DbOperator)){
			if(mConjunction != CSSM_DB_AND) return true;
		} else if(mConjunction == CSSM_DB_AND) return false;
	}
	
	return true;
}



Relation::Relation (CSSM_DB_RECORDTYPE recordType) : mRecordType (recordType) {}
Relation::~Relation () {}



UniqueIdentifier::UniqueIdentifier (CSSM_DB_RECORDTYPE recordType) : mRecordType (recordType) {}
UniqueIdentifier::~UniqueIdentifier () {}



void CssmSelectionPredicate::CloneCssmSelectionPredicate (CssmSelectionPredicate &a, const CssmSelectionPredicate &b)
{
	a.DbOperator = b.DbOperator;
	a.Attribute.NumberOfValues = b.Attribute.NumberOfValues;
	
	// clone the data
	a.Attribute.Value = new CSSM_DATA;
	a.Attribute.Value->Length = b.Attribute.Value->Length;
	if (b.Attribute.Value->Data != NULL) {
		a.Attribute.Value->Data = (uint8*) malloc (b.Attribute.Value->Length);
		memcpy (a.Attribute.Value->Data, b.Attribute.Value->Data, b.Attribute.Value->Length);
	} else {
		b.Attribute.Value->Data = NULL;
	}
	
	// clone the attribute info
	a.Attribute.Info.AttributeNameFormat = b.Attribute.Info.AttributeNameFormat;
	a.Attribute.Info.AttributeFormat = b.Attribute.Info.AttributeFormat;
	
	switch (b.Attribute.Info.AttributeNameFormat) {
	case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		a.Attribute.Info.Label.AttributeName = strdup (b.Attribute.Info.Label.AttributeName);
		break;
	
	case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
		a.Attribute.Info.Label.AttributeOID.Length = b.Attribute.Info.Label.AttributeOID.Length;
		a.Attribute.Info.Label.AttributeOID.Data = (uint8*) malloc (b.Attribute.Info.Label.AttributeOID.Length);
		memcpy (a.Attribute.Info.Label.AttributeOID.Data, b.Attribute.Info.Label.AttributeOID.Data, b.Attribute.Info.Label.AttributeOID.Length);
		break;
	
	case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
		a.Attribute.Info.Label.AttributeID = b.Attribute.Info.Label.AttributeID;
		break;
	}
}



CssmSelectionPredicate::CssmSelectionPredicate (const CssmSelectionPredicate& pred)
{
	CloneCssmSelectionPredicate (*this, pred);
}



CssmSelectionPredicate::~CssmSelectionPredicate ()
{
	if (Attribute.Value != NULL) {
		free (Attribute.Value->Data);
		delete Attribute.Value;
	}
	
	switch (Attribute.Info.AttributeNameFormat) {
	case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		free (Attribute.Info.Label.AttributeName);
		break;		
	case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
		free (Attribute.Info.Label.AttributeOID.Data);
		break;
	}
}


	
void CssmSelectionPredicate::operator= (const CssmSelectionPredicate& pred)
{
	CloneCssmSelectionPredicate (*this, pred);
}

