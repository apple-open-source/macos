/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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


// cssmdb.cpp
//
//
#include <security_cdsa_utilities/cssmdb.h>

bool DLDbIdentifier::Impl::operator < (const DLDbIdentifier::Impl &other) const
{
    if (mCssmSubserviceUid < other.mCssmSubserviceUid)
        return true;
    if (mCssmSubserviceUid != other.mCssmSubserviceUid) // i.e. greater than
        return false;
    
    // Murf correctly points out that this test will produce unreproducible results,
    // depending on what items are being compared.  To do this properly, we need to
    // assign a lexical value to NULL.
    //   
    // if (mDbName.canonicalName() == NULL || other.mDbName.canonicalName() == NULL)
    // {
    //     return false;
    // }
    
    // this is the correct way
    const char* a = mDbName.canonicalName();
    const char* b = other.mDbName.canonicalName();
    
    if (a == NULL && b != NULL)
    {
        return true; // NULL is always < something
    }
    
    if (a != NULL && b == NULL)
    {
        return false; // something is always >= NULL
    }
    
    if (a == NULL && b == NULL)
    {
        return false; // since == is not <
    }
    
    // if we get to this point, both are not null.  No crash and the lexical value is correct.
    return strcmp(a, b) < 0;
}

bool DLDbIdentifier::Impl::operator == (const Impl &other) const
{
    bool subserviceIdEqual = mCssmSubserviceUid == other.mCssmSubserviceUid;
    if (!subserviceIdEqual)
    {
        return false;
    }
    
    const char* a = mDbName.canonicalName();
    const char* b = other.mDbName.canonicalName();

    if (a == NULL && b != NULL)
    {
        return false;
    }
    
    if (a !=  NULL && b == NULL)
    {
        return false;
    }
    
    if (a == NULL && b == NULL)
    {
        return true;
    }
    
    bool namesEqual = strcmp(a, b) == 0;
    return namesEqual;
}

//
// CssmDLPolyData
//
CssmDLPolyData::operator CSSM_DATE () const
{
	assert(mFormat == CSSM_DB_ATTRIBUTE_FORMAT_BLOB);
	if (mData.Length != 8)
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	CSSM_DATE date;
	memcpy(date.Year, mData.Data, 4);
	memcpy(date.Month, mData.Data + 4, 2);
	memcpy(date.Day, mData.Data + 6, 2);
	return date;
}

CssmDLPolyData::operator Guid () const
{
	assert(mFormat == CSSM_DB_ATTRIBUTE_FORMAT_BLOB);
	if (mData.Length != Guid::stringRepLength + 1)
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	return Guid(reinterpret_cast<const char *>(mData.Data));
}


//
// CssmDbAttributeInfo
//
CssmDbAttributeInfo::CssmDbAttributeInfo(const char *name, CSSM_DB_ATTRIBUTE_FORMAT vFormat)
{
	clearPod();
	AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	Label.AttributeName = const_cast<char *>(name); // silly CDSA
	AttributeFormat = vFormat;
}

CssmDbAttributeInfo::CssmDbAttributeInfo(const CSSM_OID &oid, CSSM_DB_ATTRIBUTE_FORMAT vFormat)
{
	clearPod();
	AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_OID;
	Label.AttributeOID = oid;
	AttributeFormat = vFormat;
}

CssmDbAttributeInfo::CssmDbAttributeInfo(uint32 id, CSSM_DB_ATTRIBUTE_FORMAT vFormat)
{
	clearPod();
	AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;
	Label.AttributeID = id;
	AttributeFormat = vFormat;
}


bool
CssmDbAttributeInfo::operator <(const CssmDbAttributeInfo& other) const
{
	if (nameFormat() < other.nameFormat()) return true;
	if (other.nameFormat() < nameFormat()) return false;
	// nameFormat's are equal.
	switch (nameFormat())
	{
	case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
	{
		int res = strcmp(static_cast<const char *>(*this), static_cast<const char *>(other));
		if (res < 0) return true;
		if (res > 0) return false;
		break;
	}
	case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
		if (static_cast<const CssmOid &>(*this) < static_cast<const CssmOid &>(other)) return true;
		if (static_cast<const CssmOid &>(other) < static_cast<const CssmOid &>(*this)) return false;
		break;
	case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
		if (static_cast<uint32>(*this) < static_cast<uint32>(other)) return true;
		if (static_cast<uint32>(other) < static_cast<uint32>(*this)) return false;
		break;
	default:
		CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);
	}

	return format() < other.format();
}

bool
CssmDbAttributeInfo::operator ==(const CssmDbAttributeInfo& other) const
{
	if (nameFormat() != other.nameFormat()) return false;
	if (format() != other.format()) return false;
	switch (nameFormat())
	{
	case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		return !strcmp(static_cast<const char *>(*this), static_cast<const char *>(other));
	case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
		return static_cast<const CssmOid &>(*this) == static_cast<const CssmOid &>(other);
	case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
		return static_cast<uint32>(*this) == static_cast<uint32>(other);
	default:
		CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);
	}
}

//
// CssmDbAttributeData
//
CssmDbAttributeData::operator string() const
{
	switch (format()) {
	case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
	case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
		return at(0).toString();
	default:
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	}
}
CssmDbAttributeData::operator const Guid &() const
{
	if (format() == CSSM_DB_ATTRIBUTE_FORMAT_BLOB)
		return *at(0).interpretedAs<Guid>();
	else
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
}

CssmDbAttributeData::operator bool() const
{
	switch (format()) {
	case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
	case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
		return *at(0).interpretedAs<uint32>();
	default:
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	}
}

CssmDbAttributeData::operator uint32() const
{
	if (format() == CSSM_DB_ATTRIBUTE_FORMAT_UINT32)
		return *at(0).interpretedAs<uint32>();
	else
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
}

CssmDbAttributeData::operator const uint32 *() const
{
	if (format() == CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32)
		return reinterpret_cast<const uint32 *>(Value[0].Data);
	else
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
}

CssmDbAttributeData::operator sint32() const
{
	if (format() == CSSM_DB_ATTRIBUTE_FORMAT_SINT32)
		return *at(0).interpretedAs<sint32>();
	else
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
}

CssmDbAttributeData::operator double() const
{
	if (format() == CSSM_DB_ATTRIBUTE_FORMAT_REAL)
		return *at(0).interpretedAs<double>();
	else
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
}

CssmDbAttributeData::operator const CssmData &() const
{
	switch (format()) {
	case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
	case CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM:
	case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE:
	case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
	case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:
		return at(0);
	default:
		CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);
	}
}

void CssmDbAttributeData::set(const CSSM_DB_ATTRIBUTE_INFO &inInfo, const CssmPolyData &inValue,
		 Allocator &inAllocator)
{
	info(inInfo);
	NumberOfValues = 0;
	Value = inAllocator.alloc<CSSM_DATA>();
	Value[0].Length = 0;
	Value[0].Data = inAllocator.alloc<uint8>((UInt32)inValue.Length);
	Value[0].Length = inValue.Length;
	memcpy(Value[0].Data, inValue.Data, inValue.Length);
	NumberOfValues = 1;
}

void CssmDbAttributeData::add(const CssmPolyData &inValue, Allocator &inAllocator)
{
	Value = reinterpret_cast<CSSM_DATA *>(inAllocator.realloc(Value, sizeof(*Value) * (NumberOfValues + 1)));
	CssmAutoData valueCopy(inAllocator, inValue);
	Value[NumberOfValues++] = valueCopy.release();
}


void CssmDbAttributeData::copyValues(const CssmDbAttributeData &source, Allocator &alloc)
{
	assert(size() == 0);	// must start out empty

	// we're too lazy to arrange for exception safety here
	CssmData *vector = alloc.alloc<CssmData>(source.size());
	for (uint32 n = 0; n < source.size(); n++)
		vector[n] = CssmAutoData(alloc, source[n]).release();

	// atomic set results
	info().format(source.info().format());
	NumberOfValues = source.size();
	values() = vector;
}

void CssmDbAttributeData::deleteValues(Allocator &alloc)
{
	// Loop over all values and delete each one.
	if (values())
	{
		for (uint32 n = 0; n < size(); n++)
		{
			alloc.free(at(n).data());
		}
		alloc.free(values());
	}
	NumberOfValues = 0;
	values() = NULL;
}

bool CssmDbAttributeData::operator <(const CssmDbAttributeData &other) const
{
	if (info() < other.info()) return true;
	if (other.info() < info()) return false;

	uint32 minSize = min(size(), other.size());
	for (uint32 ix = 0; ix < minSize; ++ix)
	{
		if (at<const CssmData &>(ix) < other.at<const CssmData &>(ix))
			return true;
		if (other.at<const CssmData &>(ix) < at<const CssmData &>(ix))
			return false;
	}

	return size() < other.size();
}

void
CssmDbAttributeData::add(const CssmDbAttributeData &src, Allocator &inAllocator)
{
	// Add all the values from another attribute into this attribute.

	Value = reinterpret_cast<CSSM_DATA *>(inAllocator.realloc(Value,
		sizeof(*Value) * (NumberOfValues + src.NumberOfValues)));
		
	for (uint32 srcIndex = 0; srcIndex < src.NumberOfValues; srcIndex++) {
		uint32 destIndex = NumberOfValues + srcIndex;
		
		Value[destIndex].Length = 0;
		Value[destIndex].Data = inAllocator.alloc<uint8>((UInt32)src.Value[srcIndex].Length);
		Value[destIndex].Length = src.Value[srcIndex].Length;
		memcpy(Value[destIndex].Data, src.Value[srcIndex].Data, src.Value[srcIndex].Length);
	}
	
	NumberOfValues += src.NumberOfValues;
}

bool
CssmDbAttributeData::deleteValue(const CssmData &src, Allocator &inAllocator)
{
	// Delete a single value from this attribute, if it is present.

	for (uint32 i = 0; i < NumberOfValues; i++)
		if (CssmData::overlay(Value[i]) == src)
		{
			inAllocator.free(Value[i].Data);
			Value[i].Length = 0;
			
			NumberOfValues--;
			Value[i].Data = Value[NumberOfValues].Data;
			Value[i].Length = Value[NumberOfValues].Length;
		
			return true;
		}
		
	return false;
}

// Delete those values found in src from this object, if they are present.
// Warning: This is O(N^2) worst case; if this becomes a performance bottleneck
// then it will need to be changed.

void
CssmDbAttributeData::deleteValues(const CssmDbAttributeData &src, Allocator &inAllocator)
{
	for (uint32 i = 0; i < src.NumberOfValues; i++)
		deleteValue(CssmData::overlay(src.Value[i]), inAllocator);
}

//
// CssmDbRecordAttributeData
//
CssmDbAttributeData *
CssmDbRecordAttributeData::find(const CSSM_DB_ATTRIBUTE_INFO &inInfo)
{
	const CssmDbAttributeInfo &anInfo = CssmDbAttributeInfo::overlay(inInfo);
	for (uint32 ix = 0; ix < size(); ++ix)
	{
		if (at(ix).info() == anInfo)
			return &at(ix);
	}

	return NULL;
}

bool
CssmDbRecordAttributeData::operator <(const CssmDbRecordAttributeData &other) const
{
	if (recordType() < other.recordType()) return true;
	if (other.recordType() < recordType()) return false;
	if (semanticInformation() < other.semanticInformation()) return true;
	if (other.semanticInformation() < semanticInformation()) return false;

	uint32 minSize = min(size(), other.size());
	for (uint32 ix = 0; ix < minSize; ++ix)
	{
		if (at(ix) < other.at(ix))
			return true;
		if (other.at(ix) < at(ix))
			return false;
	}

	return size() < other.size();
}


//
// CssmAutoDbRecordAttributeData
//
CssmAutoDbRecordAttributeData::~CssmAutoDbRecordAttributeData()
{
	clear();
}

void
CssmAutoDbRecordAttributeData::invalidate()
{
	NumberOfAttributes = 0;
}



void
CssmAutoDbRecordAttributeData::clear()
{
	deleteValues();
	ArrayBuilder<CssmDbAttributeData>::clear();
}



static bool CompareAttributeInfos (const CSSM_DB_ATTRIBUTE_INFO &a, const CSSM_DB_ATTRIBUTE_INFO &b)
{
	// check the format of the names
	if (a.AttributeNameFormat != b.AttributeNameFormat)
	{
		return false;
	}
	
	switch (a.AttributeNameFormat)
	{
		case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		{
			return strcmp (a.Label.AttributeName, b.Label.AttributeName) == 0;
		}
		
		case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
		{
			if (a.Label.AttributeOID.Length != b.Label.AttributeOID.Length)
			{
				return false;
			}
			
			return memcmp (a.Label.AttributeOID.Data, b.Label.AttributeOID.Data, a.Label.AttributeOID.Length) == 0;
		}
		
		
		case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
		{
			return a.Label.AttributeID == b.Label.AttributeID;
		}
	}
	
	return true; // just to keep the compiler from complaining
}



CssmDbAttributeData* CssmAutoDbRecordAttributeData::findAttribute (const CSSM_DB_ATTRIBUTE_INFO &info)
{
	// walk through the data, looking for an attribute of the same type
	unsigned i;
	for (i = 0; i < size (); ++i)
	{
		CssmDbAttributeData& d = at (i);
		CSSM_DB_ATTRIBUTE_INFO &inInfo = d.info ();
		
		if (CompareAttributeInfos (info, inInfo))
		{
			return &d;
		}
	}
	
	// found nothing?
	return NULL;
}



CssmDbAttributeData& CssmAutoDbRecordAttributeData::getAttributeReference (const CSSM_DB_ATTRIBUTE_INFO &info)
{
	// Either find an existing reference to an attribute in the list, or make a new one.
	CssmDbAttributeData *anAttr = findAttribute (info);
	if (anAttr) // was this already in the list?
	{
		// clean it up
		anAttr->deleteValues (mValueAllocator);
	}
	else
	{
		// make a new one
		anAttr = &add();
	}
	
	return *anAttr;
}



CssmDbAttributeData &
CssmAutoDbRecordAttributeData::add(const CSSM_DB_ATTRIBUTE_INFO &info)
{
	CssmDbAttributeData& anAttr = getAttributeReference (info);
	anAttr.info(info);
	return anAttr;
}

CssmDbAttributeData &
CssmAutoDbRecordAttributeData::add(const CSSM_DB_ATTRIBUTE_INFO &info, const CssmPolyData &value)
{
	CssmDbAttributeData &anAttr = getAttributeReference (info);
	anAttr.set(info, value, mValueAllocator);
	return anAttr;
}

//
// CssmAutoQuery
//
CssmAutoQuery::CssmAutoQuery(const CSSM_QUERY &query, Allocator &allocator)
: ArrayBuilder<CssmSelectionPredicate>(CssmSelectionPredicate::overlayVar(SelectionPredicate),
									   NumSelectionPredicates,
									   query.NumSelectionPredicates, allocator)
{
	RecordType = query.RecordType;
	Conjunctive = query.Conjunctive;
	QueryLimits =  query.QueryLimits;
	QueryFlags = query.QueryFlags;
	for (uint32 ix = 0; ix < query.NumSelectionPredicates; ++ix)
		add().set(query.SelectionPredicate[ix], allocator);
}

CssmAutoQuery::~CssmAutoQuery()
{
	clear();
}

void
CssmAutoQuery::clear()
{
	deleteValues();
	ArrayBuilder<CssmSelectionPredicate>::clear();
}

CssmSelectionPredicate &
CssmAutoQuery::add(CSSM_DB_OPERATOR dbOperator, const CSSM_DB_ATTRIBUTE_INFO &info, const CssmPolyData &value)
{
	CssmSelectionPredicate &predicate = add();
	predicate.dbOperator(dbOperator);
	predicate.set(info, value, allocator());
	return predicate;
}
