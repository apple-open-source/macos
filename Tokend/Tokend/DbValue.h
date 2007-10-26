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
 *  DbValue.h
 *  TokendMuscle
 */

#ifndef _TOKEND_DBVALUE_H_
#define _TOKEND_DBVALUE_H_

#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <Security/cssmerr.h>
#include <map>
#include <vector>

namespace Tokend
{

//
// DbValue -- A base class for all types of database values.
//
class DbValue
{
	NOCOPY(DbValue)
public:
	DbValue();
	virtual ~DbValue() = 0;
};

// A collection of subclasses of DbValue that work for simple
// data types, e.g. uint32, sint32, and double, that have
// the usual C comparison and sizeof operations. Defining this
// template saves typing below.

template <class T>
class BasicValue : public DbValue
{
	NOCOPY(BasicValue)
public:
	BasicValue() {}
	BasicValue(T value) : mValue(value) {}

	bool evaluate(const BasicValue<T> &other, CSSM_DB_OPERATOR op) const
	{
		switch (op)
		{
		case CSSM_DB_EQUAL:			return mValue == other.mValue;
		case CSSM_DB_NOT_EQUAL:		return mValue != other.mValue;
		case CSSM_DB_LESS_THAN:		return mValue < other.mValue;
		case CSSM_DB_GREATER_THAN:	return mValue > other.mValue;
		default: CssmError::throwMe(CSSMERR_DL_UNSUPPORTED_QUERY);
		}
	}

	size_t size() const { return sizeof(T); }
	const uint8 *bytes() const
		{ return reinterpret_cast<const uint8 *>(&mValue); }

protected:
	T mValue;
};

// Actual useful subclasses of DbValue as instances of BasicValue.
// Note that all of these require a constructor of the form
// (const ReadSection &, uint32 &offset) that advances the offset
// to just after the value.

class UInt32Value : public BasicValue<uint32>
{
	NOCOPY(UInt32Value)
public:
	UInt32Value(const CSSM_DATA &data);
	virtual ~UInt32Value();
};

class SInt32Value : public BasicValue<sint32>
{
	NOCOPY(SInt32Value)
public:
	SInt32Value(const CSSM_DATA &data);
	virtual ~SInt32Value();
};

class DoubleValue : public BasicValue<double>
{
	NOCOPY(DoubleValue)
public:
	DoubleValue(const CSSM_DATA &data);
	virtual ~DoubleValue();
};

// Subclasses of Value for more complex types.

class BlobValue : public DbValue, public CssmData
{
	NOCOPY(BlobValue)
public:
	BlobValue() {}
	BlobValue(const CSSM_DATA &data);
	virtual ~BlobValue();
	bool evaluate(const BlobValue &other, CSSM_DB_OPERATOR op) const;

	size_t size() const { return Length; }
	const uint8 *bytes() const { return Data; }
	
protected:
	class Comparator {
	public:
		virtual ~Comparator();
		virtual int operator ()(const uint8 *ptr1, const uint8 *ptr2,
			uint32 length);
	};

	static bool evaluate(const CssmData &data1, const CssmData &data2,
		CSSM_DB_OPERATOR op, Comparator compare);
};

class TimeDateValue : public BlobValue
{
	NOCOPY(TimeDateValue)
public:
	enum { kTimeDateSize = 16 };

	TimeDateValue(const CSSM_DATA &data);
	virtual ~TimeDateValue();

	bool isValidDate() const;
	
private:
	uint32 rangeValue(uint32 start, uint32 length) const;
};

class StringValue : public BlobValue
{
	NOCOPY(StringValue)
public:
	StringValue(const CSSM_DATA &data);
	virtual ~StringValue();
	bool evaluate(const StringValue &other, CSSM_DB_OPERATOR op) const;
	
private:
	class Comparator : public BlobValue::Comparator {
	public:
		virtual int operator ()(const uint8 *ptr1, const uint8 *ptr2,
			uint32 length);
	};

};

class BigNumValue : public BlobValue
{
	NOCOPY(BigNumValue)
public:
	static const uint8 kSignBit = 0x80;

	BigNumValue(const CSSM_DATA &data);
	virtual ~BigNumValue();
	bool evaluate(const BigNumValue &other, CSSM_DB_OPERATOR op) const;

private:
	static int compare(const uint8 *a, const uint8 *b, int length);
};

class MultiUInt32Value : public DbValue
{
	NOCOPY(MultiUInt32Value)
public:
	MultiUInt32Value(const CSSM_DATA &data);
	virtual ~MultiUInt32Value();
	bool evaluate(const MultiUInt32Value &other, CSSM_DB_OPERATOR op) const;

	size_t size() const { return mNumValues * sizeof(uint32); }
	const uint8 *bytes() const { return reinterpret_cast<uint8 *>(mValues); }
	
private:
	uint32 mNumValues;
	uint32 *mValues;
	bool mOwnsValues;
};

} // end namespace Tokend

#endif /* !_TOKEND_DBVALUE_H_ */

