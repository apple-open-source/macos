/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  DbValue.cpp
 *  TokendMuscle
 */

#include "DbValue.h"
#include <ctype.h>

// @@@ missing "pack" methods with WriteSection parameter

namespace Tokend
{

//
// DbValue
//
DbValue::DbValue()
{
}

DbValue::~DbValue()
{
}

UInt32Value::UInt32Value(const CSSM_DATA &data)
{
	switch (data.Length)
	{
	case 1:	mValue = *reinterpret_cast<uint8 *>(data.Data);		break;
	case 2:	mValue = *reinterpret_cast<uint16 *>(data.Data);	break;
	case 4:	mValue = *reinterpret_cast<uint32 *>(data.Data);	break;
	default:
		CssmError::throwMe(CSSMERR_DL_INVALID_VALUE);
	}
}

UInt32Value::~UInt32Value()
{
}

//
// SInt32Value
//

SInt32Value::SInt32Value(const CSSM_DATA &data)
{
	switch (data.Length)
	{
	case 1:	mValue = *reinterpret_cast<sint8 *>(data.Data);		break;
	case 2:	mValue = *reinterpret_cast<sint16 *>(data.Data);	break;
	case 4:	mValue = *reinterpret_cast<sint32 *>(data.Data);	break;
	default:
		CssmError::throwMe(CSSMERR_DL_INVALID_VALUE);
	}
}

SInt32Value::~SInt32Value()
{
}

//
// DoubleValue
//

DoubleValue::DoubleValue(const CSSM_DATA &data)
{
	switch (data.Length)
	{
	case 4:	mValue = *reinterpret_cast<float *>(data.Data);		break;
	case 8:	mValue = *reinterpret_cast<double *>(data.Data);	break;
	default:
		CssmError::throwMe(CSSMERR_DL_INVALID_VALUE);
	}
}

DoubleValue::~DoubleValue()
{
}

//
// BlobValue
//

BlobValue::BlobValue(const CSSM_DATA &data) : CssmData(CssmData::overlay(data))
{
}

BlobValue::~BlobValue()
{
}

BlobValue::Comparator::~Comparator()
{
}

int
BlobValue::Comparator::operator ()(const uint8 *ptr1, const uint8 *ptr2,
	uint32 length)
{
	return memcmp(ptr1, ptr2, length);
}

bool
BlobValue::evaluate(const BlobValue &other, CSSM_DB_OPERATOR op) const
{
	return evaluate(*this, other, op, Comparator());
}

bool
BlobValue::evaluate(const CssmData &inData1, const CssmData &inData2,
	CSSM_DB_OPERATOR op, Comparator compare)
{
	uint32 length1 = inData1.Length, length2 = inData2.Length;
	const uint8 *data1 = inData1.Data;
	const uint8 *data2 = inData2.Data;
	
	switch (op) {
	
	case CSSM_DB_CONTAINS_INITIAL_SUBSTRING:
		if (length1 > length2)
            return false;
        length2 = length1;
        goto DB_EQUAL;
		
	case CSSM_DB_CONTAINS_FINAL_SUBSTRING:
        if (length1 > length2)
            return false;
		data2 += (length2 - length1);
		length2 = length1;
        // dropthrough...

    case CSSM_DB_EQUAL:
	DB_EQUAL:
        if (length1 != length2)
            return false;
        if (length1 == 0)
            return true;
		return compare(data1, data2, length1) == 0;

    case CSSM_DB_NOT_EQUAL:
		if (length1 != length2)
			return true;
		if (length1 == 0)
			return false;
        return compare(data1, data2, length1) != 0;

    case CSSM_DB_LESS_THAN:
    case CSSM_DB_GREATER_THAN:
    {
        uint32 length = min(length1, length2);
		int result = (length == 0) ? 0 : compare(data1, data2, length);
		
		if (result < 0 || (result == 0 && length1 < length2))
			return op == CSSM_DB_LESS_THAN;
		else if (result > 0 || (result == 0 && length1 > length2))
			return op == CSSM_DB_GREATER_THAN;
		break;
	}

    case CSSM_DB_CONTAINS:
        if (length1 > length2)
            return false;
        if (length1 == 0)
            return true;
        // Both buffers are at least 1 byte long.
        for (const uint8 *data = data2; data + length1 <= data2 + length2;
			++data)
			if (compare(data1, data, length1) == 0)
				return true;
		break;

    default:
        CssmError::throwMe(CSSMERR_DL_UNSUPPORTED_QUERY);
    }

    return false;
}

//
// TimeDateValue
//

TimeDateValue::TimeDateValue(const CSSM_DATA &data)
:	BlobValue(data)
{
	if (Length != kTimeDateSize || !isValidDate())
		CssmError::throwMe(CSSMERR_DL_INVALID_VALUE);
}

TimeDateValue::~TimeDateValue()
{
}

bool
TimeDateValue::isValidDate() const
{
	if (Length != kTimeDateSize || Data[kTimeDateSize - 1] != 0 ||
		Data[kTimeDateSize - 2] != 'Z')
		return false;
		
	for (uint32 i = 0; i < kTimeDateSize - 2; i++)
		if (!isdigit(Data[i]))
			return false;
			
	uint32 month = rangeValue(4, 2);
	if (month < 1 || month > 12)
		return false;
		
	uint32 day = rangeValue(6, 2);
	if (day < 1 || day > 31)
		return false;
		
	uint32 hour = rangeValue(8, 2);
	if (hour < 0 || hour > 23)
		return false;
		
	uint32 minute = rangeValue(10, 2);
	if (minute < 0 || minute > 59)
		return false;

	uint32 second = rangeValue(12, 2);
	if (second < 0 || second > 59)
		return false;		

	return true;
}

uint32
TimeDateValue::rangeValue(uint32 start, uint32 length) const
{
	uint32 value = 0;
	for (uint32 i = 0; i < length; i++)
		value = value * 10 + Data[start + i] - '0';
	return value;
}

//
// StringValue
//

StringValue::StringValue(const CSSM_DATA &data)
:	BlobValue(data)
{
}

StringValue::~StringValue()
{
}

int
StringValue::Comparator::operator ()(const uint8 *ptr1, const uint8 *ptr2,
	uint32 length)
{
	return strncmp(reinterpret_cast<const char *>(ptr1),
		reinterpret_cast<const char *>(ptr2), length);
}

bool
StringValue::evaluate(const StringValue &other, CSSM_DB_OPERATOR op) const
{
	return BlobValue::evaluate(*this, other, op, StringValue::Comparator());
}

//
// BigNumValue
//

BigNumValue::BigNumValue(const CSSM_DATA &data)
:	BlobValue(data)
{
	// remove trailing zero bytes
	while (Length > 1 && Data[Length - 1] == 0)
		Length--;
		
	// if the number is zero (positive or negative), make the length zero
	if (Length == 1 && (Data[0] & ~kSignBit) == 0)
		Length = 0;
}

BigNumValue::~BigNumValue()
{
}

// Walk the contents of two equal-sized bignums, moving backward
// from the high-order bytes, and return the comparison result
// ala memcmp.

int
BigNumValue::compare(const uint8 *a, const uint8 *b, int length)
{
	for (int diff, i = length - 1; i >= 1; i--)
		if ((diff = a[i] - b[i]))
			return diff;

	// for the last (i.e. first) byte, mask out the sign bit
	return (a[0] & ~kSignBit) - (b[0] & ~kSignBit);
}

// Compare two bignums, assuming they are in canonical form (i.e.,
// no bytes containing trailing zeros.

bool
BigNumValue::evaluate(const BigNumValue &other, CSSM_DB_OPERATOR op) const
{
	uint32 length1 = Length, length2 = other.Length;
	uint8 sign1 = length1 ? (Data[0] & kSignBit) : 0;
	uint8 sign2 = length2 ? (other.Data[0] & kSignBit) : 0;
	
	switch (op)
	{
	case CSSM_DB_EQUAL:
	case CSSM_DB_NOT_EQUAL:
		return BlobValue::evaluate(other, op);
		
	case CSSM_DB_LESS_THAN:
		if (sign1 ^ sign2)
			// different signs: return true iff left value is the negative one
			return sign1;
		else if (length1 != length2)
			// in canonical form, shorter numbers have smaller absolute value
			return sign1 ? (length1 > length2) : (length1 < length2);
		else {
			// same length, same sign...
			int c = compare(Data, other.Data, length1);
			return sign1 ? (c > 0) : (c < 0);
		}
		break;
		
	case CSSM_DB_GREATER_THAN:
		if (sign1 ^ sign2)
			return sign2;
		else if (length1 != length2)
			return sign1 ? (length1 < length2) : (length1 > length2);
		else {
			int c = compare(Data, other.Data, length1);
			return sign1 ? (c < 0) : (c > 0);
		}
		break;
		
	case CSSM_DB_CONTAINS:
	case CSSM_DB_CONTAINS_INITIAL_SUBSTRING:
	case CSSM_DB_CONTAINS_FINAL_SUBSTRING:
	default:
		CssmError::throwMe(CSSMERR_DL_UNSUPPORTED_QUERY);
	}
}

//
// MultiUInt32Value
//

MultiUInt32Value::MultiUInt32Value(const CSSM_DATA &data)
{
	if (data.Length & (sizeof(uint32) - 1))
		CssmError::throwMe(CSSMERR_DL_INVALID_VALUE);
		
	mNumValues = data.Length / sizeof(uint32);
	mValues = reinterpret_cast<uint32 *>(data.Data);
	mOwnsValues = false;
}

MultiUInt32Value::~MultiUInt32Value()
{
	if (mOwnsValues)
		delete [] mValues;
}

static inline int
uint32cmp(const uint32 *a, const uint32 *b, uint32 length)
{
	return memcmp(a, b, length * sizeof(uint32));
}

bool
MultiUInt32Value::evaluate(const MultiUInt32Value &other,
	CSSM_DB_OPERATOR op) const
{
	uint32 length1 = mNumValues, length2 = other.mNumValues;
	const uint32 *values1 = mValues;
	const uint32 *values2 = other.mValues;
	
	switch (op)
	{
	case CSSM_DB_EQUAL:					
		if (length1 == length2)
			return uint32cmp(values1, values2, length1) == 0;
		break;
		
	case CSSM_DB_NOT_EQUAL:
		if (length1 != length2 || uint32cmp(values1, values2, length1))
			return true;
		break;

	case CSSM_DB_CONTAINS_INITIAL_SUBSTRING:
		if (length1 <= length2)
			return uint32cmp(values1, values2, length1) == 0;
		break;
		
	case CSSM_DB_CONTAINS_FINAL_SUBSTRING:
		if (length1 <= length2)
			return uint32cmp(values1, values2 + (length2 - length1), length1)
				== 0;
		break;
		
	case CSSM_DB_CONTAINS:
		if (length1 <= length2) {
		
			if (length1 == 0)
				return true;
				
			for (const uint32 *values = values2;
				values + length1 < values2 + length2; values++)
				if (uint32cmp(values1, values, length1) == 0)
					return true;
		}
		break;
		
	case CSSM_DB_LESS_THAN:
		// this is not required by the spec, but is required to sort indexes
		// over multi uint32 keys...
		if (length1 < length2)
			return true;
		else if (length1 == length2)
			return uint32cmp(values1, values2, length1) < 0;
		break;

	default:
		CssmError::throwMe(CSSMERR_DL_UNSUPPORTED_QUERY);
	}
	
	return false;
}

} // end namespace Tokend

