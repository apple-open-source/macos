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


// cssmdb.cpp
//
//

#ifdef __MWERKS__
#define _CPP_UTILITIES
#endif

#include <Security/cssmdb.h>

#if 0
// XXX Obsolete
CSSM_RETURN	AddFooToIntelList( void** theIntelListToAddItTo, unsigned long* theNumberOfThingsAlreadyInTheList, const void* theThingToAdd, size_t theSizeOfTheThingToAdd)
{	// this is to make adding things to Intel LISTs (also called Arrays by the rest of us) easy! We do it everywhere! Join the fun!
  CSSM_RETURN result = CSSM_OK;
	void*	theReallocatedBuffer = NULL;
	if( *theIntelListToAddItTo == NULL )
	{
		
		*theIntelListToAddItTo = malloc(theSizeOfTheThingToAdd);
		if(!*theIntelListToAddItTo)
		{
			result = CSSMERR_CSSM_MEMORY_ERROR;
		}
	}
	 else
	 {
			theReallocatedBuffer = realloc((void*)*theIntelListToAddItTo, (*theNumberOfThingsAlreadyInTheList+1) * (theSizeOfTheThingToAdd) );
			if(!theReallocatedBuffer)
			{
				result = CSSMERR_CSSM_MEMORY_ERROR;
			}	 
			 else
			 {
			 	*theIntelListToAddItTo = theReallocatedBuffer;
			 }
	 }
	 
	 if(result == CSSM_OK )
	 {
	 	memcpy( (void*)((char*)*theIntelListToAddItTo+(theSizeOfTheThingToAdd * (*theNumberOfThingsAlreadyInTheList))), theThingToAdd, theSizeOfTheThingToAdd);
	 	(*theNumberOfThingsAlreadyInTheList)++;
	 }
	 
   return result;
}
#endif

//
// CssmDbAttributeInfo
//
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
void
CssmDbAttributeData::deleteValues(CssmAllocator &inAllocator)
{
	// Loop over all values and delete each one.
	if (Value)
	{
		for (uint32 anIndex = 0; anIndex < NumberOfValues; anIndex++)
		{
			if (Value[anIndex].Data)
			{
				inAllocator.free(Value[anIndex].Data);
			}

			Value[anIndex].Length = 0;
		}

		inAllocator.free(Value);
		Value = NULL;
	}

	NumberOfValues = 0;
}

bool
CssmDbAttributeData::operator <(const CssmDbAttributeData &other) const
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
CssmDbAttributeData::add(const CssmDbAttributeData &src, CssmAllocator &inAllocator)
{
	// Add all the values from another attribute into this attribute.

	Value = reinterpret_cast<CSSM_DATA *>(inAllocator.realloc(Value,
		sizeof(*Value) * (NumberOfValues + src.NumberOfValues)));
		
	for (uint32 srcIndex = 0; srcIndex < src.NumberOfValues; srcIndex++) {
		uint32 destIndex = NumberOfValues + srcIndex;
		
		Value[destIndex].Length = 0;
		Value[destIndex].Data = inAllocator.alloc<uint8>(src.Value[srcIndex].Length);
		Value[destIndex].Length = src.Value[srcIndex].Length;
		memcpy(Value[destIndex].Data, src.Value[srcIndex].Data, src.Value[srcIndex].Length);
	}
	
	NumberOfValues += src.NumberOfValues;
}

bool
CssmDbAttributeData::deleteValue(const CssmData &src, CssmAllocator &inAllocator)
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
CssmDbAttributeData::deleteValues(const CssmDbAttributeData &src, CssmAllocator &inAllocator)
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
	int i;
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
CssmAutoQuery::CssmAutoQuery(const CSSM_QUERY &query, CssmAllocator &allocator)
: ArrayBuilder<CssmSelectionPredicate>(static_cast<CssmSelectionPredicate *>(SelectionPredicate),
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
