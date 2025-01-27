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
// MetaAttribute.cpp
//

#include "MetaAttribute.h"
#include "MetaRecord.h"

MetaAttribute::~MetaAttribute()
{
}

// Construct an instance of an appropriate subclass of MetaAttribute
// based on the given format.

MetaAttribute *
MetaAttribute::create(Format format, uint32 attributeIndex,
	uint32 attributeId)
{
	switch (format)
	{
		case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
			return new TypedMetaAttribute<StringValue>(format, attributeIndex, attributeId);

		case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
			return new TypedMetaAttribute<SInt32Value>(format, attributeIndex, attributeId);
			
		case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
			return new TypedMetaAttribute<UInt32Value>(format, attributeIndex, attributeId);

		case CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM:
			return new TypedMetaAttribute<BigNumValue>(format, attributeIndex, attributeId);
			
		case CSSM_DB_ATTRIBUTE_FORMAT_REAL:
			return new TypedMetaAttribute<DoubleValue>(format, attributeIndex, attributeId);

		case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE:
			return new TypedMetaAttribute<TimeDateValue>(format, attributeIndex, attributeId);

		case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
			return new TypedMetaAttribute<BlobValue>(format, attributeIndex, attributeId);
			
		case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:
			return new TypedMetaAttribute<MultiUInt32Value>(format, attributeIndex, attributeId);
														
	    case CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX:
		default:
			CssmError::throwMe(CSSMERR_DL_UNSUPPORTED_FIELD_FORMAT);
	}
}

void
MetaAttribute::packNumberOfValues(WriteSection &ws, uint32 numValues, uint32 &valueOffset) const
{
	uint32 offset = MetaRecord::OffsetAttributeOffsets + mAttributeIndex * AtomSize;
	
	if (numValues == 0) {
		// a zero offset means the attribute has no values
		ws.put(offset, 0);
	}
	else if (numValues == 1) {
		// setting the low bit of the offset means that there is exactly one value
		 ws.put(offset, valueOffset | 1);
	}
	else {
		// write the offset, then write the number of values at that position
		ws.put(offset, valueOffset);
		valueOffset = ws.put(valueOffset, numValues);
	}
}

void
MetaAttribute::unpackNumberOfValues(const ReadSection &rs, uint32 &numValues,
	uint32 &valueOffset) const
{
	uint32 offset = MetaRecord::OffsetAttributeOffsets + mAttributeIndex * AtomSize;
	valueOffset = rs[offset];
	
	if (valueOffset == 0)
		// a zero offset means no values
		numValues = 0;
	else if (valueOffset & 1) {
		// setting the LSB means exactly one value
		valueOffset ^= 1;
		numValues = 1;
	}
	else {
		// otherwise, the number of values is at the offset, and the values follow
		numValues = rs[valueOffset];
		valueOffset += AtomSize;
	}
}

void
MetaAttribute::packAttribute(WriteSection &ws, uint32 &valueOffset, uint32 numValues,
	const CSSM_DATA *values) const
{
	packNumberOfValues(ws, numValues, valueOffset);
	for (uint32 i = 0; i < numValues; i++)
		packValue(ws, valueOffset, values[i]);
}

void
MetaAttribute::unpackAttribute(const ReadSection &rs, Allocator &allocator,
	uint32 &numValues, CSSM_DATA *&values) const
{
	uint32 valueOffset;
	unpackNumberOfValues(rs, numValues, valueOffset);
	
	// Rough check for number of values; will be more like 10 or 20
	if (numValues > 1024)
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	values = reinterpret_cast<CSSM_DATA *>(allocator.malloc(numValues * sizeof(CSSM_DATA)));
	
	for (uint32 i = 0; i < numValues; i++)
		unpackValue(rs, valueOffset, values[i], allocator);
}

uint32
MetaAttribute::getNumberOfValues(const ReadSection &rs) const
{
	uint32 numValues, valueOffset;
	unpackNumberOfValues(rs, numValues, valueOffset);
	return numValues;
}

void
MetaAttribute::copyValueBytes(uint32 valueIndex, const ReadSection &rs, WriteSection &ws,
	uint32 &writeOffset) const
{
	uint32 numValues, valueOffset;
	unpackNumberOfValues(rs, numValues, valueOffset);

	// skip bytes before the desired value
	for (uint32 i = 0; i < valueIndex; i++)
		skipValue(rs, valueOffset);
		
	// copy the value bytes into the write section
	copyValue(rs, valueOffset, ws, writeOffset);
}
