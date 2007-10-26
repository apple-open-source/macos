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
 *  MetaRecord.cpp
 *  TokendMuscle
 */

#include "MetaRecord.h"

#include "Attribute.h"
#include "KeyHandle.h"
#include "MetaAttribute.h"
#include "Record.h"
#include <security_utilities/trackingallocator.h>
#include <security_cdsa_utilities/cssmbridge.h>

namespace Tokend
{

#pragma mark ---------------- MetaRecord methods --------------

// Used for normal relations.
MetaRecord::MetaRecord(RelationId inRelationId) : mRelationId(inRelationId),
	mKeyHandleFactory(NULL)
{
    // Passing in a bogus attributeId for the attribute at index 0 (which is
	// the data). It's not possible to look up the attribute by attributeId,
	// nor should any coder rely on it's value.
	mAttributeVector.push_back(MetaAttribute::create(*this, kAF_BLOB, 0,
		'data'));
}

MetaRecord::~MetaRecord()
{
	for_each_delete(mAttributeVector.begin(), mAttributeVector.end());
}

MetaAttribute &MetaRecord::createAttribute(const std::string &inAttributeName,
     CSSM_DB_ATTRIBUTE_FORMAT inAttributeFormat)
{
    uint32 anAttributeId = mAttributeVector.size() - 1;
    return createAttribute(&inAttributeName, NULL, anAttributeId,
		inAttributeFormat);
}

MetaAttribute &MetaRecord::createAttribute(const string *inAttributeName,
	const CssmOid *inAttributeOID, uint32 inAttributeID,
	CSSM_DB_ATTRIBUTE_FORMAT inAttributeFormat)
{
	// Index of new element is current size of vector
    uint32 anAttributeIndex = mAttributeVector.size();
    bool aInsertedAttributeName = false;
    bool aInsertedAttributeOID = false;
    bool aInsertedAttributeID = false;

    if (inAttributeName)
    {
        if (!mNameStringMap.insert(NameStringMap::value_type(*inAttributeName,
			anAttributeIndex)).second)
            CssmError::throwMe(CSSMERR_DL_FIELD_SPECIFIED_MULTIPLE);
        aInsertedAttributeName = true;
    }
    try
    {
        if (inAttributeOID)
        {
            if (!mNameOIDMap.insert(NameOIDMap::value_type(*inAttributeOID,
				anAttributeIndex)).second)
                CssmError::throwMe(CSSMERR_DL_FIELD_SPECIFIED_MULTIPLE);
            aInsertedAttributeOID = true;
        }

		if (!mNameIntMap.insert(NameIntMap::value_type(inAttributeID,
			anAttributeIndex)).second)
			CssmError::throwMe(CSSMERR_DL_FIELD_SPECIFIED_MULTIPLE);
		aInsertedAttributeID = true;

		// Note: this no longer throws INVALID_FIELD_NAME since the attribute
		// will always have an attribute ID by which it is known.
		MetaAttribute *ma = MetaAttribute::create(*this, inAttributeFormat,
			anAttributeIndex, inAttributeID);
		mAttributeVector.push_back(ma);
		return *ma;
    }
    catch (...)
    {
        if (aInsertedAttributeName)
            mNameStringMap.erase(*inAttributeName);
        if (aInsertedAttributeOID)
            mNameOIDMap.erase(*inAttributeOID);
        if (inAttributeID)
            mNameIntMap.erase(inAttributeID);
		
        throw;
    }
}

// Return the index (0 though NumAttributes - 1) of the attribute
// represented by inAttributeInfo

uint32 MetaRecord::attributeIndex(
	const CSSM_DB_ATTRIBUTE_INFO &inAttributeInfo) const
{
	uint32 anIndex;
	switch (inAttributeInfo.AttributeNameFormat)
	{
	    case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		{
			string aName(inAttributeInfo.Label.AttributeName);
			NameStringMap::const_iterator it = mNameStringMap.find(aName);
			if (it == mNameStringMap.end())
				CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);

			anIndex = it->second;
			break;
		}
	    case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
	    {
			const CssmOid &aName =
				CssmOid::overlay(inAttributeInfo.Label.AttributeOID);
			NameOIDMap::const_iterator it = mNameOIDMap.find(aName);
			if (it == mNameOIDMap.end())
				CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);
			anIndex = it->second;
			break;
		}
		case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
		{
			uint32 aName = inAttributeInfo.Label.AttributeID;
			NameIntMap::const_iterator it = mNameIntMap.find(aName);
			if (it == mNameIntMap.end())
				CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);
			anIndex = it->second;
			break;
		}
		default:
			CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);
			break;
	}

	return anIndex;
}

const MetaAttribute &MetaRecord::metaAttribute(
	const CSSM_DB_ATTRIBUTE_INFO &inAttributeInfo) const
{
	return *mAttributeVector[attributeIndex(inAttributeInfo)];
}

const MetaAttribute &MetaRecord::metaAttribute(uint32 name) const
{
	NameIntMap::const_iterator it = mNameIntMap.find(name);
	if (it == mNameIntMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);

	return *mAttributeVector[it->second];
}

const MetaAttribute &MetaRecord::metaAttribute(const std::string &name) const
{
	NameStringMap::const_iterator it = mNameStringMap.find(name);
	if (it == mNameStringMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);

	return *mAttributeVector[it->second];
}

const MetaAttribute &MetaRecord::metaAttributeForData() const
{
	return *mAttributeVector[0];
}

void MetaRecord::attributeCoder(uint32 name, AttributeCoder *coder)
{
	const_cast<MetaAttribute &>(metaAttribute(name)).attributeCoder(coder);
}

void MetaRecord::attributeCoder(const std::string &name, AttributeCoder *coder)
{
	const_cast<MetaAttribute &>(metaAttribute(name)).attributeCoder(coder);
}

void MetaRecord::attributeCoderForData(AttributeCoder *coder)
{
	const_cast<MetaAttribute &>(metaAttributeForData()).attributeCoder(coder);
}

void
MetaRecord::get(TokenContext *tokenContext, Record &record,
	TOKEND_RETURN_DATA &data) const
{
	if (data.attributes)
	{
		// Fetch the requested attributes.
		CSSM_DB_RECORD_ATTRIBUTE_DATA &drad = *data.attributes;
		drad.DataRecordType = mRelationId;
		drad.SemanticInformation = 0;
		for (uint32 ix = 0; ix < drad.NumberOfAttributes; ++ix)
		{
			CSSM_DB_ATTRIBUTE_DATA &dad = drad.AttributeData[ix];
			const MetaAttribute &ma = metaAttribute(dad.Info);
			dad.Info.AttributeFormat = ma.attributeFormat();
			const Attribute &attr = ma.attribute(tokenContext, record);
			dad.NumberOfValues = attr.size();
			dad.Value = const_cast<CSSM_DATA_PTR>(attr.values());
		}
	}

	if (data.data)
	{
		// Fetch the data.
		const MetaAttribute &ma = metaAttributeForData();
		const Attribute &attr = ma.attribute(tokenContext, record);
		if (attr.size() != 1)
			CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);

		(*data.data) = attr.values()[0];
        if (mKeyHandleFactory)
        {
			KeyHandle *keyHandle = mKeyHandleFactory->keyHandle(tokenContext,
				*this, record);
            data.keyhandle = keyHandle ? keyHandle->handle() : 0;
        }
        else
            data.keyhandle = 0;
	}
}


} // end namespace Tokend
