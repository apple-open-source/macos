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


//
// MetaRecord.cpp
//

#include "MetaRecord.h"
#include <security_utilities/trackingallocator.h>
#include <security_cdsa_utilities/cssmbridge.h>


MetaRecord::MetaRecord(CSSM_DB_RECORDTYPE inRecordType) :
    mRecordType(inRecordType)
{
}

MetaRecord::MetaRecord(const CSSM_DB_RECORD_ATTRIBUTE_INFO &inInfo)
:	mRecordType(inInfo.DataRecordType)
{
	try
	{
		setRecordAttributeInfo(inInfo);
	}
	catch (...)
	{
		for_each_delete(mAttributeVector.begin(), mAttributeVector.end());
	}
}

MetaRecord::MetaRecord(CSSM_DB_RECORDTYPE inRelationID,
                       uint32 inNumberOfAttributes,
					   const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo) :
    mRecordType(inRelationID)
{
	try {
		for (uint32 anIndex = 0; anIndex < inNumberOfAttributes; anIndex++)
		{
			string aName;
			if (inAttributeInfo[anIndex].AttributeName)
				aName = string(inAttributeInfo[anIndex].AttributeName);
				
			const CssmData *aNameID = NULL;
			if (inAttributeInfo[anIndex].AttributeNameID.Length > 0)
				aNameID = &CssmData::overlay(inAttributeInfo[anIndex].AttributeNameID);

			uint32 aNumber = inAttributeInfo[anIndex].AttributeId;
			createAttribute(
				inAttributeInfo[anIndex].AttributeName ? &aName : NULL,
				aNameID, aNumber,
				inAttributeInfo[anIndex].DataType);
		}
	}
	catch (...)
	{
		for_each_delete(mAttributeVector.begin(), mAttributeVector.end());
	}
}

MetaRecord::~MetaRecord()
{
	// for_each_delete(mAttributeVector.begin(), mAttributeVector.end());
	AttributeVector::iterator it = mAttributeVector.begin();
	while (it != mAttributeVector.end())
	{
		MetaAttribute* mat = *it++;
		if (mat != NULL)
		{
			delete mat;
		}
	}
}

void
MetaRecord::setRecordAttributeInfo(const CSSM_DB_RECORD_ATTRIBUTE_INFO &inInfo)
{
    for (uint32 anIndex = 0; anIndex < inInfo.NumberOfAttributes; anIndex++)
    {
        switch (inInfo.AttributeInfo[anIndex].AttributeNameFormat)
        {
            case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
            {
                string aName(inInfo.AttributeInfo[anIndex].Label.AttributeName);
                createAttribute(&aName, nil, anIndex,
								inInfo.AttributeInfo[anIndex].AttributeFormat);
                break;
            }
            case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
            {
                const CssmData &aNameID = CssmOid::overlay(inInfo.AttributeInfo[anIndex].Label.AttributeOID);
                createAttribute(nil, &aNameID, anIndex,
								inInfo.AttributeInfo[anIndex].AttributeFormat);
                break;
            }
            case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER:
            {
                uint32 aNumber = inInfo.AttributeInfo[anIndex].Label.AttributeID;
                createAttribute(nil, nil, aNumber,
								inInfo.AttributeInfo[anIndex].AttributeFormat);
                break;
            }
            default:
                CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);
                break;
        }
    }
}

void
MetaRecord::createAttribute(const string *inAttributeName,
							const CssmOid *inAttributeOID,
                            uint32 inAttributeID,
							CSSM_DB_ATTRIBUTE_FORMAT inAttributeFormat)
{
	// Index of new element is current size of vector
    uint32 anAttributeIndex = mAttributeVector.size();
    bool aInsertedAttributeName = false;
    bool aInsertedAttributeOID = false;
    bool aInsertedAttributeID = false;

    if (inAttributeName)
    {
        if (!mNameStringMap.insert(NameStringMap::value_type(*inAttributeName, anAttributeIndex)).second)
            CssmError::throwMe(CSSMERR_DL_FIELD_SPECIFIED_MULTIPLE);
        aInsertedAttributeName = true;
    }
    try
    {
        if (inAttributeOID)
        {
            if (!mNameOIDMap.insert(NameOIDMap::value_type(*inAttributeOID, anAttributeIndex)).second)
                CssmError::throwMe(CSSMERR_DL_FIELD_SPECIFIED_MULTIPLE);
            aInsertedAttributeOID = true;
        }

		if (!mNameIntMap.insert(NameIntMap::value_type(inAttributeID, anAttributeIndex)).second)
			CssmError::throwMe(CSSMERR_DL_FIELD_SPECIFIED_MULTIPLE);
		aInsertedAttributeID = true;

		// Note: this no longer throws INVALID_FIELD_NAME since the attribute will always have
		// an attribute ID by which it is known

		mAttributeVector.push_back(MetaAttribute::create(inAttributeFormat,
			anAttributeIndex, inAttributeID));
    }
    catch(...)
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


// Create a packed record from the given inputs.
void
MetaRecord::packRecord(WriteSection &inWriteSection,
                       const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
                       const CssmData *inData) const
{
    uint32 aDataSize;
    if (inData)
        aDataSize = inData->Length;
    else
        aDataSize = 0;

    inWriteSection.put(OffsetDataSize, aDataSize);
    uint32 anOffset = OffsetAttributeOffsets + AtomSize * mAttributeVector.size();
    if (aDataSize)
        anOffset = inWriteSection.put(anOffset, aDataSize, inData->Data);

    vector<uint32> aNumValues(mAttributeVector.size(), ~(uint32)0);
    vector<CSSM_DATA_PTR> aValues(mAttributeVector.size());
    uint32 anIndex;

    if (inAttributes == NULL)
        inWriteSection.put(OffsetSemanticInformation, 0);
    else
    {
        inWriteSection.put(OffsetSemanticInformation, inAttributes->SemanticInformation);

        // Put the supplied attribute values into the list of attributes
        // and values.
        anIndex = inAttributes->NumberOfAttributes;
        // Make sure that AttributeData is a valid array.
		if (anIndex > 0)
			Required(inAttributes->AttributeData);

        while (anIndex-- > 0)
        {
            CSSM_DB_ATTRIBUTE_DATA &anAttribute = inAttributes->AttributeData[anIndex];
            uint32 anAttributeIndex = attributeIndex(anAttribute.Info);
			// Make sure that the caller specified the attribute values in the correct format.
			if (anAttribute.Info.AttributeFormat != mAttributeVector[anAttributeIndex]->attributeFormat())
				CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);

            // If this attribute was specified before, throw.
            if (aNumValues[anAttributeIndex] != ~(uint32)0)
                CssmError::throwMe(CSSMERR_DL_FIELD_SPECIFIED_MULTIPLE);

            aNumValues[anAttributeIndex] = anAttribute.NumberOfValues;
            aValues[anAttributeIndex] = anAttribute.Value;
        }
    }

    for (anIndex = 0; anIndex < mAttributeVector.size(); ++anIndex)
    {
        const MetaAttribute &aMetaAttribute = *mAttributeVector[anIndex];
        uint32 aNumberOfValues = aNumValues[anIndex];
        // Now call the parsingmodule for each attribute that
        // wasn't explicitly specified and that has a parsingmodule.
        if (aNumberOfValues == ~(uint32)0)
            aNumberOfValues = aDataSize == 0 ? 0 : aMetaAttribute.parse(*inData, aValues[anIndex]);

        // XXX When do we throw CSSMERR_DL_MISSING_VALUE?  Maybe if an
		// attribute is part of a unique index.

        // Now we have a valuelist for this attribute.  Let's encode it.
        aMetaAttribute.packAttribute(inWriteSection, anOffset, aNumberOfValues, aValues[anIndex]);
    }

	inWriteSection.put(OffsetRecordSize, anOffset);
    inWriteSection.size(anOffset);
}

inline void
MetaRecord::unpackAttribute(const ReadSection &inReadSection,
							Allocator &inAllocator,
                            CSSM_DB_ATTRIBUTE_DATA &inoutAttribute) const
{
    const MetaAttribute &aMetaAttribute = metaAttribute(inoutAttribute.Info);
    // XXX: See ISSUES on whether AttributeFormat should be an outputvalue or not.
	inoutAttribute.Info.AttributeFormat = aMetaAttribute.attributeFormat();
    aMetaAttribute.unpackAttribute(inReadSection, inAllocator,
                                   inoutAttribute.NumberOfValues,
								   inoutAttribute.Value);
}

void
MetaRecord::unpackRecord(const ReadSection &inReadSection,
						 Allocator &inAllocator,
                         CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
                         CssmData *inoutData,
						 CSSM_QUERY_FLAGS inQueryFlags) const
{
	// XXX Use POD wrapper for inoutAttributes here.
	TrackingAllocator anAllocator(inAllocator);
	
	try
	{
		if (inoutData)
		{
			// XXX Treat KEY records specially.

			// If inQueryFlags & CSSM_QUERY_RETURN_DATA is true return the raw
			// key bits in the CSSM_KEY structure
			Range aDataRange = dataRange(inReadSection);
			inoutData->Length = aDataRange.mSize;
			inoutData->Data = inReadSection.allocCopyRange(aDataRange, anAllocator);
		}

		if (inoutAttributes)
		{
			inoutAttributes->DataRecordType = dataRecordType();
			inoutAttributes->SemanticInformation = semanticInformation(inReadSection);
			uint32 anIndex = inoutAttributes->NumberOfAttributes;

			// Make sure that AttributeData is a valid array.
			if (anIndex > 0 && inoutAttributes->AttributeData == NULL)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_POINTER);

			while (anIndex-- > 0)
			{
				unpackAttribute(inReadSection, anAllocator,
								inoutAttributes->AttributeData[anIndex]);
			}
		}
	}
	catch (CssmError e)
	{
		if (e.osStatus() != CSSMERR_DL_DATABASE_CORRUPT)
		{
			// clear all pointers so that nothing dangles back to the user
			if (inoutData)
			{
				inoutData->Data = NULL;
			}
			
			if (inoutAttributes)
			{
				unsigned i;
				for (i = 0; i < inoutAttributes->NumberOfAttributes; ++i)
				{
					CSSM_DB_ATTRIBUTE_DATA& data = inoutAttributes->AttributeData[i];
					
					unsigned j;
					for (j = 0; j < data.NumberOfValues; ++j)
					{
						data.Value[j].Data = NULL;
					}
					
					data.Value = NULL;
					
					if (data.Info.AttributeNameFormat == CSSM_DB_ATTRIBUTE_NAME_AS_STRING)
					{
						data.Info.Label.AttributeName = NULL;
					}
				}
			}
		}
		
		throw;
	}
	catch (...)
	{
		// clear all pointers so that nothing dangles back to the user
		if (inoutData)
		{
			inoutData->Data = NULL;
		}
		
		if (inoutAttributes)
		{
			unsigned i;
			for (i = 0; i < inoutAttributes->NumberOfAttributes; ++i)
			{
				CSSM_DB_ATTRIBUTE_DATA& data = inoutAttributes->AttributeData[i];
				
				unsigned j;
				for (j = 0; j < data.NumberOfValues; ++j)
				{
					data.Value[j].Data = NULL;
				}
				
				data.Value = NULL;
				
				if (data.Info.AttributeNameFormat == CSSM_DB_ATTRIBUTE_NAME_AS_STRING)
				{
					data.Info.Label.AttributeName = NULL;
				}
			}
		}
		
		throw;
	}
	

	// Don't free anything the trackingAllocator allocated when it is destructed.
	anAllocator.commit();
}

// Return the index (0 though NumAttributes - 1) of the attribute
// represented by inAttributeInfo

#ifndef	NDEBUG
#define LOG_NAME_AS_STRING_FAIL		
#endif
uint32
MetaRecord::attributeIndex(const CSSM_DB_ATTRIBUTE_INFO &inAttributeInfo) const
{
	uint32 anIndex;
	switch (inAttributeInfo.AttributeNameFormat)
	{
	    case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
		{
			string aName(inAttributeInfo.Label.AttributeName);
			assert(aName.size() < 500);		// MDS leak debug
			NameStringMap::const_iterator it = mNameStringMap.find(aName);
			if (it == mNameStringMap.end()) {
				#ifdef	LOG_NAME_AS_STRING_FAIL
				printf("NAME_AS_STRING failure; attrName %s\n", 
					inAttributeInfo.Label.AttributeName);
				for(it = mNameStringMap.begin();
				    it != mNameStringMap.end();
					it++) {
						printf("name %s val %d\n", it->first.c_str(), it->second);
				}
				#endif
				CssmError::throwMe(CSSMERR_DL_INVALID_FIELD_NAME);
			}
			anIndex = it->second;
			break;
		}
	    case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
	    {
			const CssmOid &aName = CssmOid::overlay(inAttributeInfo.Label.AttributeOID);
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

const MetaAttribute &
MetaRecord::metaAttribute(const CSSM_DB_ATTRIBUTE_INFO &inAttributeInfo) const
{
	return *mAttributeVector[attributeIndex(inAttributeInfo)];
}

// Create a packed record from the given inputs and the old packed record inReadSection.
void
MetaRecord::updateRecord(const ReadSection &inReadSection,
						 WriteSection &inWriteSection,
						 const CssmDbRecordAttributeData *inAttributes,
						 const CssmData *inData,
						 CSSM_DB_MODIFY_MODE inModifyMode) const
{
	TrackingAllocator anAllocator(Allocator::standard());

	// modify the opaque data associated with the record
	
    uint32 aDataSize;
	const uint8 *aDataData = NULL;
    
	if (inData)
	{
		// prepare to write new data
        aDataSize = inData->Length;
		aDataData = inData->Data;
	}
    else
	{
		// prepare to copy old data
        Range aDataRange = dataRange(inReadSection);
    	aDataSize = aDataRange.mSize;
		if (aDataSize)
			aDataData = inReadSection.range(aDataRange);
	}

	// compute the data offset; this will keep a running total of the record size
    uint32 anOffset = OffsetAttributeOffsets + AtomSize * mAttributeVector.size();
	
	// write the appropriate data to the new record
	inWriteSection.put(OffsetDataSize, aDataSize);
	if (aDataSize)
		anOffset = inWriteSection.put(anOffset, aDataSize, aDataData);

	// unpack the old attributes since some of them may need to be preserved
	
	auto_array<CssmDbAttributeData> attributeData(mAttributeVector.size());

	for (uint32 anAttributeIndex = mAttributeVector.size(); anAttributeIndex-- > 0; )
	{
		// unpack the old attribute data for this attribute index
		const MetaAttribute &attribute = *mAttributeVector[anAttributeIndex];
		attribute.unpackAttribute(inReadSection, anAllocator,
								  attributeData[anAttributeIndex].NumberOfValues,
								  attributeData[anAttributeIndex].Value);
	}
	
	// retrieve the currrent semantic information
	
	uint32 oldSemanticInformation = semanticInformation(inReadSection);
	
	// process each input attribute as necessary, based on the modification mode
	
	if (inAttributes == NULL)
	{
		// make sure the modification mode is NONE, otherwise it's an
		// error accordining to the spec
		if (inModifyMode != CSSM_DB_MODIFY_ATTRIBUTE_NONE)
			CssmError::throwMe(CSSMERR_DL_INVALID_MODIFY_MODE);
	}

	else {
	
		// modify the semantic information

		uint32 inSemanticInformation = inAttributes ? inAttributes->SemanticInformation : 0;

		if (inModifyMode == CSSM_DB_MODIFY_ATTRIBUTE_ADD)
			oldSemanticInformation |= inSemanticInformation;

		else if (inModifyMode == CSSM_DB_MODIFY_ATTRIBUTE_DELETE)
			oldSemanticInformation &= ~inSemanticInformation;
		
		else if (inModifyMode == CSSM_DB_MODIFY_ATTRIBUTE_REPLACE)
			oldSemanticInformation = inSemanticInformation;

		uint32 anIndex = inAttributes->NumberOfAttributes;
		if (anIndex > 0)
			Required(inAttributes->AttributeData);

		// modify the attributes

		while (anIndex-- > 0) {
	
			const CssmDbAttributeData &anAttribute = inAttributes->at(anIndex);
			uint32 anAttributeIndex = attributeIndex(anAttribute.info());
			if (anAttribute.format() != mAttributeVector[anAttributeIndex]->attributeFormat())
				CssmError::throwMe(CSSMERR_DL_INCOMPATIBLE_FIELD_FORMAT);

			CssmDbAttributeData &oldAttribute = attributeData[anAttributeIndex];
		
			// if the modify mode is ADD, merge new values with pre-existing values
		
			if (inModifyMode == CSSM_DB_MODIFY_ATTRIBUTE_ADD)
				oldAttribute.add(anAttribute, anAllocator);

			// if the modify mode is DELETE, remove the indicated values, or remove
			// all values if none are specified

			else if (inModifyMode == CSSM_DB_MODIFY_ATTRIBUTE_DELETE)
			{
				if (anAttribute.size() == 0)
					oldAttribute.deleteValues(anAllocator);
				else
					oldAttribute.deleteValues(anAttribute, anAllocator);
			}
		
			// if the modify mode is REPLACE, then replace the specified values, or
			// delete all values if no values are specified
		
			else if (inModifyMode == CSSM_DB_MODIFY_ATTRIBUTE_REPLACE)
			{
				oldAttribute.deleteValues(anAllocator);
				if (anAttribute.size() > 0)
					oldAttribute.add(anAttribute, anAllocator);
				else
					// The spec says "all values are deleted or the the value is replaced
					// with the default" but doesn't say which. We could call the parsing
					// module for the attribute here...if they were implemented! But instead
					// we choose "all values are deleted" and leave it at that.
					;
			}
		}
	}

	// write the resulting attributes into the new record
	
	inWriteSection.put(OffsetSemanticInformation, oldSemanticInformation);

	for (uint32 anIndex = 0; anIndex < mAttributeVector.size(); ++anIndex)
	{
		const MetaAttribute &metaAttribute = *mAttributeVector[anIndex];
		metaAttribute.packAttribute(inWriteSection, anOffset,
								    attributeData[anIndex].NumberOfValues,
									attributeData[anIndex].Value);
	}
	
	inWriteSection.put(OffsetRecordSize, anOffset);
	inWriteSection.size(anOffset);
}

