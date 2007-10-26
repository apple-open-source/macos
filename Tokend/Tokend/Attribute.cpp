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
 *  Attribute.cpp
 *  TokendMuscle
 */

#include "Attribute.h"

namespace Tokend
{


Attribute::Attribute()
{
	mCount = 0;
	mValues = NULL;
}

Attribute::Attribute(const Attribute &attribute)
{
	set(attribute.mValues, attribute.mCount);
}

Attribute::Attribute(bool value)
{
	uint32 v = value ? 1 : 0;
	set(&v, sizeof(v));
}

Attribute::Attribute(sint32 value)
{
	set(&value, sizeof(value));
}

Attribute::Attribute(uint32 value)
{
	set(&value, sizeof(value));
}

Attribute::Attribute(const char *value)
{
	set(value, strlen(value));
}

Attribute::Attribute(const std::string &value)
{
	set(value.c_str(), value.size());
}

Attribute::Attribute(const void *data, uint32 length)
{
	set(data, length);
}

Attribute::Attribute(const CSSM_DATA *datas, uint32 count)
{
	set(datas, count);
}

Attribute::~Attribute()
{
	if (mValues)
		free(mValues);
}

Attribute &Attribute::operator = (const Attribute &attribute)
{
	if (mValues)
		free(mValues);

	set(attribute.mValues, attribute.mCount);
	return *this;
}

void Attribute::set(const CSSM_DATA *datas, uint32 count)
{
	mCount = count;
	uint32 size = count * sizeof(CSSM_DATA);
	for (uint32 ix = 0; ix < count; ++ix)
		size += datas[ix].Length;

	uint8 *buffer = (uint8 *)malloc(size);
	mValues = CSSM_DATA_PTR(buffer);
	buffer += sizeof(CSSM_DATA) * count;
	for (uint32 ix = 0; ix < count; ++ix)
	{
		uint32 length = datas[ix].Length;
		mValues[ix].Data = buffer;
		mValues[ix].Length = length;
		memcpy(mValues[ix].Data, datas[ix].Data, length);
		buffer += length;
	}
}

void Attribute::set(const void *data, uint32 length)
{
	mCount = 1;
	uint8 *buffer = (uint8 *)malloc(sizeof(CSSM_DATA) + length);
	mValues = CSSM_DATA_PTR(buffer);
	mValues[0].Data = buffer + sizeof(CSSM_DATA);
	mValues[0].Length = length;
	memcpy(mValues[0].Data, data, length);
}

void Attribute::getDateValue(CSSM_DATE &date) const
{
	if (mCount == 0 || mValues[0].Length == 0)
	{
		memset(&date, 0, sizeof(date));
	}
	else if (mCount == 1 && mValues[0].Length == sizeof(date))
	{
		memcpy(&date, mValues[0].Data, sizeof(date));
	}
	else
		CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);
}

uint32 Attribute::uint32Value() const
{
	if (mCount != 1 || mValues[0].Length != 4)
		CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);

	return *reinterpret_cast<uint32 *>(mValues[0].Data);
}


} // end namespace Tokend

