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
 *  MetaAttribute.h
 *  TokendMuscle
 */

#ifndef _TOKEND_METAATTRIBUTE_H_
#define _TOKEND_METAATTRIBUTE_H_

#include <Security/cssmtype.h>
#include <security_utilities/utilities.h>
#include "Attribute.h"

namespace Tokend
{

class Attribute;
class AttributeCoder;
class DbValue;
class MetaRecord;
class Record;
class TokenContext;

// A base class for all meta attributes.

class MetaAttribute
{
	NOCOPY(MetaAttribute)
public:
	typedef CSSM_DB_ATTRIBUTE_FORMAT Format;
	
	virtual ~MetaAttribute();
	
	// construct an appropriate subclass of MetaAttribute
	static MetaAttribute *create(MetaRecord& metaRecord, Format format,
		uint32 attributeIndex, uint32 attributeId);

	void attributeCoder(AttributeCoder *coder) { mCoder = coder; }

	Format attributeFormat() const { return mFormat; }
	uint32 attributeIndex() const { return mAttributeIndex; }
	uint32 attributeId() const { return mAttributeId; }

	const Attribute &attribute(TokenContext *tokenContext,
		Record &record) const;

	const MetaRecord &metaRecord() const { return mMetaRecord; }
	
	// interface required of all subclasses, implemented with templates below
	virtual DbValue *createValue(const CSSM_DATA &data) const = 0;

	virtual bool evaluate(TokenContext *tokenContext, const DbValue *value,
		Record& record, CSSM_DB_OPERATOR op) const = 0;

protected:
	MetaAttribute(MetaRecord& metaRecord, Format format, uint32 attributeIndex,
		uint32 attributeId)
		: mCoder(NULL), mMetaRecord(metaRecord), mFormat(format),
		mAttributeIndex(attributeIndex), mAttributeId(attributeId) {}

	AttributeCoder *mCoder;
	MetaRecord &mMetaRecord;
	Format mFormat;
	uint32 mAttributeIndex;
	uint32 mAttributeId;
};

// Template used to describe particular subclasses of MetaAttribute

template <class T>
class TypedMetaAttribute : public MetaAttribute
{
public:
	TypedMetaAttribute(MetaRecord& metaRecord, Format format,
		uint32 attributeIndex, uint32 attributeId)
		: MetaAttribute(metaRecord, format, attributeIndex, attributeId) {}

	DbValue *createValue(const CSSM_DATA &data) const
	{
		return new T(data);
	}

	bool evaluate(TokenContext *tokenContext, const DbValue *value,
		Record &record, CSSM_DB_OPERATOR op) const
	{
		const Attribute &attr = attribute(tokenContext, record);
		uint32 numValues = attr.size();

		/* If any of the values for this attribute match we have a match. */
		for (uint32 ix = 0; ix < numValues; ++ix)
			if (dynamic_cast<const T *>(value)->evaluate(static_cast<const T &>(attr[ix]), op))
				return true;

		return false;
	}

	bool evaluate(const DbValue *value1, const DbValue *value2,
		CSSM_DB_OPERATOR op) const
	{
		return (dynamic_cast<const T *>(value1))->
			evaluate(*dynamic_cast<const T *>(value2), op);
	}
};

}	// end namespace Tokend

#endif /* !_TOKEND_METAATTRIBUTE_H_ */

