/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
// DbValue.h
//

#ifndef _H_APPLEDL_DBVALUE
#define _H_APPLEDL_DBVALUE

#include "ReadWriteSection.h"

#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <Security/cssmerr.h>
#include <map>
#include <vector>

namespace Security
{

//
// DbValue -- A base class for all types of database values.
//
class DbValue
{
public:
	virtual ~DbValue();
};

// A collection of subclasses of DbValue that work for simple
// data types, e.g. uint32, sint32, and double, that have
// the usual C comparison and sizeof operations. Defining this
// template saves typing below.

template <class T>
class BasicValue : public DbValue
{
public:
	BasicValue() {}
	BasicValue(T value) : mValue(value) {}

	bool evaluate(const BasicValue<T> &other, CSSM_DB_OPERATOR op) const
	{
		switch (op) {

		case CSSM_DB_EQUAL:
			return mValue == other.mValue;
			
		case CSSM_DB_NOT_EQUAL:
			return mValue != other.mValue;
			
		case CSSM_DB_LESS_THAN:
			return mValue < other.mValue;

		case CSSM_DB_GREATER_THAN:
			return mValue > other.mValue;			

		default:
			CssmError::throwMe(CSSMERR_DL_UNSUPPORTED_QUERY);
			return false;
		}
	}

	size_t size() const { return sizeof(T); }
	size_t size(const ReadSection &rs, uint32 offset) const { return size(); }
	const uint8 *bytes() const { return reinterpret_cast<const uint8 *>(&mValue); }

protected:
	T mValue;
};

// Actual useful subclasses of DbValue as instances of BasicValue.
// Note that all of these require a constructor of the form
// (const ReadSection &, uint32 &offset) that advances the offset
// to just after the value.

class UInt32Value : public BasicValue<uint32>
{
public:
	UInt32Value(const ReadSection &rs, uint32 &offset);		
	UInt32Value(const CSSM_DATA &data);
	virtual ~UInt32Value();
	void pack(WriteSection &ws, uint32 &offset) const;
};

class SInt32Value : public BasicValue<sint32>
{
public:
	SInt32Value(const ReadSection &rs, uint32 &offset);		
	SInt32Value(const CSSM_DATA &data);
	virtual ~SInt32Value();
	void pack(WriteSection &ws, uint32 &offset) const;
};

class DoubleValue : public BasicValue<double>
{
public:
	DoubleValue(const ReadSection &rs, uint32 &offset);		
	DoubleValue(const CSSM_DATA &data);
	virtual ~DoubleValue();
	void pack(WriteSection &ws, uint32 &offset) const;
};

// Subclasses of Value for more complex types.

class BlobValue : public DbValue, public CssmData
{
public:
	BlobValue() {}
	BlobValue(const ReadSection &rs, uint32 &offset);		
	BlobValue(const CSSM_DATA &data);
	virtual ~BlobValue();
	void pack(WriteSection &ws, uint32 &offset) const;
	bool evaluate(const BlobValue &other, CSSM_DB_OPERATOR op) const;

	size_t size() const { return Length; }
	const uint8 *bytes() const { return Data; }
	
protected:
	class Comparator {
	public:
		virtual ~Comparator();
		virtual int operator () (const uint8 *ptr1, const uint8 *ptr2, uint32 length);
	};

	static bool evaluate(const CssmData &data1, const CssmData &data2, CSSM_DB_OPERATOR op,
		Comparator compare);
};

class TimeDateValue : public BlobValue
{
public:
	enum { kTimeDateSize = 16 };

	TimeDateValue(const ReadSection &rs, uint32 &offset);		
	TimeDateValue(const CSSM_DATA &data);
	virtual ~TimeDateValue();
	void pack(WriteSection &ws, uint32 &offset) const;

	bool isValidDate() const;
	
private:
	uint32 rangeValue(uint32 start, uint32 length) const;
};

class StringValue : public BlobValue
{
public:
	StringValue(const ReadSection &rs, uint32 &offset);		
	StringValue(const CSSM_DATA &data);
	virtual ~StringValue();
	bool evaluate(const StringValue &other, CSSM_DB_OPERATOR op) const;
	
private:
	class Comparator : public BlobValue::Comparator {
	public:
		virtual int operator () (const uint8 *ptr1, const uint8 *ptr2, uint32 length);
	};

};

class BigNumValue : public BlobValue
{
public:
	static const uint8 kSignBit = 0x80;

	BigNumValue(const ReadSection &rs, uint32 &offset);		
	BigNumValue(const CSSM_DATA &data);
	virtual ~BigNumValue();
	bool evaluate(const BigNumValue &other, CSSM_DB_OPERATOR op) const;

private:
	static int compare(const uint8 *a, const uint8 *b, int length);
};

class MultiUInt32Value : public DbValue
{
public:
	MultiUInt32Value(const ReadSection &rs, uint32 &offset);		
	MultiUInt32Value(const CSSM_DATA &data);
	virtual ~MultiUInt32Value();
	void pack(WriteSection &ws, uint32 &offset) const;
	bool evaluate(const MultiUInt32Value &other, CSSM_DB_OPERATOR op) const;

	size_t size() const { return mNumValues * sizeof(uint32); }
	const uint8 *bytes() const { return reinterpret_cast<uint8 *>(mValues); }
	
private:
	uint32 mNumValues;
	uint32 *mValues;
	bool mOwnsValues;
};

} // end namespace Security

#endif // _H_APPLEDL_DBVALUE

