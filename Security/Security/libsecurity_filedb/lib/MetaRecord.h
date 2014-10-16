/*
 * Copyright (c) 2000-2001,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// MetaRecord.h
//

#ifndef _H_APPLEDL_METARECORD
#define _H_APPLEDL_METARECORD

#include "MetaAttribute.h"

namespace Security
{

//
// Part of the Unique record identifier needed to identify the actual record.
//
class RecordId
{
public:
	RecordId() : mRecordNumber(~(uint32)0), mCreateVersion(~(uint32)0), mRecordVersion(~(uint32)0) {}
    RecordId(uint32 inRecordNumber, uint32 inCreateVersion, uint32 inRecordVersion = 0)
	  : mRecordNumber(inRecordNumber),
        mCreateVersion(inCreateVersion),
		mRecordVersion(inRecordVersion) {}
    bool operator <(const RecordId &inRecordId) const
    {
        return (mRecordNumber < inRecordId.mRecordNumber
                || (mRecordNumber == inRecordId.mRecordNumber
                    && (mCreateVersion < inRecordId.mCreateVersion
						|| (mCreateVersion == inRecordId.mCreateVersion
							&& mRecordVersion < inRecordId.mRecordVersion))));
    }
    uint32 mRecordNumber;
    uint32 mCreateVersion;
	uint32 mRecordVersion;
};

//
// Meta (or Schema) representation of an a Record.  Used for packing and unpacking objects.
//

class MetaRecord
{
	NOCOPY(MetaRecord)
	
public:
    MetaRecord(CSSM_DB_RECORDTYPE inRecordType);
	MetaRecord(const CSSM_DB_RECORD_ATTRIBUTE_INFO &inInfo);
    MetaRecord(CSSM_DB_RECORDTYPE inRelationID,
               uint32 inNumberOfAttributes,
			   const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *inAttributeInfo);
	~MetaRecord();

    void setRecordAttributeInfo(const CSSM_DB_RECORD_ATTRIBUTE_INFO &inInfo);
	
    void createAttribute(const string *inAttributeName,
						 const CssmOid *inAttributeOID,
                         uint32 inAttributeID,
						 CSSM_DB_ATTRIBUTE_FORMAT inAttributeFormat);

    // Create a packed record from the given inputs.
    void packRecord(WriteSection &inWriteSection,
                    const CSSM_DB_RECORD_ATTRIBUTE_DATA *inAttributes,
                    const CssmData *inData) const;

	// Unpack a record from the given inputs and return the RecordId of the record.
    void unpackRecord(const ReadSection &inReadSection,
					  Allocator &inAllocator,
					  CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR inoutAttributes,
					  CssmData *inoutData,
					  CSSM_QUERY_FLAGS inQueryFlags) const;

	const MetaAttribute &metaAttribute(const CSSM_DB_ATTRIBUTE_INFO &inAttributeInfo) const;

	void updateRecord(const ReadSection &inReadSection,
					  WriteSection &inWriteSection,
					  const CssmDbRecordAttributeData *inAttributes,
					  const CssmData *inData,
					  CSSM_DB_MODIFY_MODE inModifyMode) const;

    CSSM_DB_RECORDTYPE dataRecordType() const { return mRecordType; }

	Range dataRange(const ReadSection &inReadSection) const
	{
        return Range((uint32)(OffsetAttributeOffsets + mAttributeVector.size() * AtomSize),
					 inReadSection[OffsetDataSize]);
	}

	// Currently this is not a real attribute.  We should probably fix this.
	uint32 semanticInformation(const ReadSection &inReadSection) const
	{
		return inReadSection[OffsetSemanticInformation];
	}

    // Return the ReadSection for record at offset
	static const ReadSection readSection(const ReadSection &inTableSection, uint32 inOffset)
	{
		return inTableSection.subsection(inOffset,
										 inTableSection[inOffset + OffsetRecordSize]);
	}

	// Set the RecordId of the record in inWriteSection
	static void packRecordId(const RecordId &inRecordId,
							 WriteSection &inWriteSection)
	{
		inWriteSection.put(OffsetRecordNumber, inRecordId.mRecordNumber);
		inWriteSection.put(OffsetCreateVersion, inRecordId.mCreateVersion);
		inWriteSection.put(OffsetRecordVersion, inRecordId.mRecordVersion);
	}

	// Return the RecordId for the record inRecordSection
	static const uint32 unpackRecordNumber(const ReadSection &inRecordSection)
	{
		return inRecordSection[OffsetRecordNumber];
	}

	// Return the RecordId for the record inRecordSection
	static const RecordId unpackRecordId(const ReadSection &inRecordSection)
	{
		return RecordId(inRecordSection[OffsetRecordNumber],
						inRecordSection[OffsetCreateVersion],
						inRecordSection[OffsetRecordVersion]);
	}

private:
    // Return the index (0 though NumAttributes - 1) of the attribute
    // represented by inAttributeInfo
    uint32 attributeIndex(const CSSM_DB_ATTRIBUTE_INFO &inAttributeInfo) const;

    void unpackAttribute(const ReadSection &inReadSection, Allocator &inAllocator,
                         CSSM_DB_ATTRIBUTE_DATA &inoutAttribute) const;

    friend class MetaAttribute;
	enum
	{
		OffsetRecordSize			= AtomSize * 0,
		OffsetRecordNumber			= AtomSize * 1,
		OffsetCreateVersion			= AtomSize * 2,
		OffsetRecordVersion			= AtomSize * 3,
		OffsetDataSize				= AtomSize * 4,
		OffsetSemanticInformation	= AtomSize * 5,
		OffsetAttributeOffsets		= AtomSize * 6
	};

	CSSM_DB_RECORDTYPE mRecordType;
	typedef std::map<string, uint32> NameStringMap;
	typedef std::map<CssmBuffer<CssmOidContainer>, uint32> NameOIDMap;
	typedef std::map<uint32, uint32> NameIntMap;
	typedef std::vector<MetaAttribute *> AttributeVector;
	NameStringMap mNameStringMap;
	NameOIDMap mNameOIDMap;
	NameIntMap mNameIntMap;
	AttributeVector mAttributeVector;
};

} // end namespace Security

#endif // _H_APPLEDL_METARECORD

