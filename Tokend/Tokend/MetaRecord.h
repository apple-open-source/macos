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
 *  MetaRecord.h
 *  TokendMuscle
 */

#ifndef _TOKEND_METARECORD_H_
#define _TOKEND_METARECORD_H_

#include <security_cdsa_utilities/cssmdata.h>
#include <map>
#include <string>
#include <vector>
#include <SecurityTokend/SecTokend.h>

namespace Tokend
{

// Shorter names for some long cssm constants
enum
{
	kAF_STRING = CSSM_DB_ATTRIBUTE_FORMAT_STRING,
	kAF_SINT32 = CSSM_DB_ATTRIBUTE_FORMAT_SINT32,
	kAF_UINT32 = CSSM_DB_ATTRIBUTE_FORMAT_UINT32,
	kAF_BIG_NUM = CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM,
	kAF_REAL = CSSM_DB_ATTRIBUTE_FORMAT_REAL,
	kAF_TIME_DATE = CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE,
	kAF_BLOB = CSSM_DB_ATTRIBUTE_FORMAT_BLOB,
	kAF_MULTI_UINT32 = CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32,
	kAF_COMPLEX = CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX
};

typedef CSSM_DB_RECORDTYPE RelationId;


class AttributeCoder;
class KeyHandleFactory;
class MetaAttribute;
class Record;
class TokenContext;
//
// Meta (or Schema) representation of an a Record.  Used for packing and
// unpacking objects.
//

class MetaRecord
{
	NOCOPY(MetaRecord)
public:
	// Used for normal relations
	// dataCoder is the coder which will be used for the "data" value
	// (metaAttributeForData() returns a metaAttribute using this coder.
    MetaRecord(RelationId inRelationId);

	~MetaRecord();

    MetaAttribute &createAttribute(const std::string &inAttributeName,
                                   CSSM_DB_ATTRIBUTE_FORMAT inAttributeFormat);
    MetaAttribute &createAttribute(const std::string *inAttributeName,
						 const CssmOid *inAttributeOID,
                         uint32 inAttributeID,
						 CSSM_DB_ATTRIBUTE_FORMAT inAttributeFormat);

	const MetaAttribute &metaAttribute(
		const CSSM_DB_ATTRIBUTE_INFO &inAttributeInfo) const;
	const MetaAttribute &MetaRecord::metaAttribute(uint32 name) const;
	const MetaAttribute &MetaRecord::metaAttribute(
		const std::string &name) const;
	const MetaAttribute &metaAttributeForData() const;

	void attributeCoder(uint32 name, AttributeCoder *coder);
	void attributeCoder(const std::string &name, AttributeCoder *coder);
	void attributeCoderForData(AttributeCoder *coder);

	RelationId relationId() const { return mRelationId; }

    // Return the index (0 though NumAttributes - 1) of the attribute
	// represented by inAttributeInfo
    uint32 attributeIndex(const CSSM_DB_ATTRIBUTE_INFO &inAttributeInfo) const;

	void get(TokenContext *tokenContext, Record &record,
		TOKEND_RETURN_DATA &data) const;

	void keyHandleFactory(KeyHandleFactory *keyHandleFactory)
		{ mKeyHandleFactory = keyHandleFactory; }
private:

    //friend class MetaAttribute;

	RelationId mRelationId;
	
	typedef std::map<std::string, uint32> NameStringMap;
	typedef std::map<CssmBuffer<CssmOidContainer>, uint32> NameOIDMap;
	typedef std::map<uint32, uint32> NameIntMap;

	NameStringMap mNameStringMap;
	NameOIDMap mNameOIDMap;
	NameIntMap mNameIntMap;

	typedef std::vector<MetaAttribute *> AttributeVector;
    typedef AttributeVector::iterator AttributeIterator;
    typedef AttributeVector::const_iterator ConstAttributeIterator;
	AttributeVector mAttributeVector;
    KeyHandleFactory *mKeyHandleFactory;
};

} // end namespace Tokend

#endif /* !_TOKEND_METARECORD_H_ */

