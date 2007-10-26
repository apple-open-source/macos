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
 *  MetaAttribute.cpp
 *  TokendMuscle
 */

#include "MetaAttribute.h"
#include "MetaRecord.h"
#include "Record.h"
#include "DbValue.h"
#include "DbValue.h"

namespace Tokend
{

MetaAttribute::~MetaAttribute()
{
}

// Construct an instance of an appropriate subclass of MetaAttribute based on
// the given format.  Called in MetaRecord.cpp createAttribute.
MetaAttribute *MetaAttribute::create(MetaRecord& metaRecord, Format format,
	uint32 attributeIndex, uint32 attributeId)
{
	switch (format)
	{
	case kAF_STRING:
		return new TypedMetaAttribute<StringValue>(metaRecord, format,
			attributeIndex, attributeId);

	case kAF_SINT32:
		return new TypedMetaAttribute<SInt32Value>(metaRecord, format,
			attributeIndex, attributeId);
		
	case kAF_UINT32:
		return new TypedMetaAttribute<UInt32Value>(metaRecord, format,
			attributeIndex, attributeId);

	case kAF_BIG_NUM:
		return new TypedMetaAttribute<BigNumValue>(metaRecord, format,
			attributeIndex, attributeId);
		
	case kAF_REAL:
		return new TypedMetaAttribute<DoubleValue>(metaRecord, format,
			attributeIndex, attributeId);

	case kAF_TIME_DATE:
		return new TypedMetaAttribute<TimeDateValue>(metaRecord, format,
			attributeIndex, attributeId);

	case kAF_BLOB:
		return new TypedMetaAttribute<BlobValue>(metaRecord, format,
			attributeIndex, attributeId);
		
	case kAF_MULTI_UINT32:
		return new TypedMetaAttribute<MultiUInt32Value>(metaRecord, format,
			attributeIndex, attributeId);
													
	case kAF_COMPLEX:
	default:
		CssmError::throwMe(CSSMERR_DL_UNSUPPORTED_FIELD_FORMAT);
	}
}

const Attribute &
MetaAttribute::attribute(TokenContext *tokenContext, Record &record) const
{
	if (!record.hasAttributeAtIndex(mAttributeIndex))
	{
		if (!mCoder)
		{
			secdebug("coder",
				"No coder for r: %p rid: 0x%08X aid: %u aix: %u",
				&record, mMetaRecord.relationId(), mAttributeId,
				mAttributeIndex);
			CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);
		}

		secdebug("coder",
			"Asking coder %p for r: %p rid: 0x%08X aid: %u aix: %u",
			mCoder, &record, mMetaRecord.relationId(), mAttributeId,
			mAttributeIndex);
		mCoder->decode(tokenContext, *this, record);

		// The coder had better put something useful in the attribute we asked it to.
		if (!record.hasAttributeAtIndex(mAttributeIndex))
		{
			secdebug("coder",
				"Coder %p did not set r: %p rid: 0x%08X aid: %u aix: %u",
				mCoder, &record, mMetaRecord.relationId(), mAttributeId,
				mAttributeIndex);
			CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);
		}
	}

	const Attribute &attribute = record.attributeAtIndex(mAttributeIndex);
#ifndef NDEBUG
	if (attribute.size() == 1)
		secdebug("mscread",
			"r: %p rid: 0x%08X aid: %u aix: %u has: 1 value of length: %lu",
			&record, mMetaRecord.relationId(), mAttributeId, mAttributeIndex,
			attribute[0].Length);
	else
		secdebug("mscread",
			"r: %p rid: 0x%08X aid: %u aix: %u has: %u values",
			&record, mMetaRecord.relationId(), mAttributeId, mAttributeIndex,
			attribute.size());
#endif		
		
	return attribute;
}


}	// end namespace Tokend

