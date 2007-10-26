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
 *  Schema.h
 *  TokendMuscle
 */

#ifndef _TOKEND_SCHEMA_H_
#define _TOKEND_SCHEMA_H_

#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmdb.h>
#include <map>

#include "MetaRecord.h"
#include "Relation.h"
#include "AttributeCoder.h"

namespace Tokend
{

class Relation;

//
// Schema
//
class Schema
{
	NOCOPY(Schema)
public:
	typedef std::map<RelationId, Relation *> RelationMap;
    typedef RelationMap::const_iterator ConstRelationMapIterator;

    Schema();
    virtual ~Schema();

	virtual void create();

	const Relation &findRelation(RelationId inRelationId) const;
	Relation &findRelation(RelationId inRelationId);
	MetaRecord &findMetaRecord(RelationId inRelationId);

    ConstRelationMapIterator begin() const { return mRelationMap.begin(); }
    ConstRelationMapIterator end() const { return mRelationMap.end(); }

    const LinkedRecordAttributeCoder &publicKeyHashCoder() const
		{ return mPublicKeyHashCoder; }
protected:
    Relation *createRelation(const std::string &relationName,
		RelationId relationId);
	Relation *createStandardRelation(RelationId relationId);

    MetaAttribute &createAttribute(Relation &relation,
		const std::string *name, uint32 attributeId,
		CSSM_DB_ATTRIBUTE_FORMAT attributeFormat, bool isIndex);
private:
	Relation *createRelation(MetaRecord *inMetaRecord);

    void registerRelation(const std::string &relationName,
		RelationId relationId);
    void registerAttribute(RelationId relationId, const std::string *name,
		uint32 attributeId, CSSM_DB_ATTRIBUTE_FORMAT attributeFormat,
		bool isIndex);

private:
    Relation *mInfo, *mAttributes, *mIndices;
    RelationMap mRelationMap;

	// AttributeIndices for attributes of CSSM_DL_DB_SCHEMA_INFO relation.
	uint32 io_rid;
	uint32 io_rn;

	// AttributeIndices for attributes of CSSM_DL_DB_SCHEMA_ATTRIBUTES
	// relation.
	uint32 as_rid;
	uint32 as_aid;
	uint32 as_anf;
	uint32 as_an;
	uint32 as_anid;
	uint32 as_af;

	// AttributeIndices for attributes of CSSM_DL_DB_SCHEMA_INDEXES relation.
	uint32 ix_rid;
	uint32 ix_iid;
	uint32 ix_aid;
	uint32 ix_it;
	uint32 ix_idl;
protected:
	// Coders for some standard attributes
	ConstAttributeCoder mTrueCoder;
	ConstAttributeCoder mFalseCoder;
	ConstAttributeCoder mCertEncodingBERCoder;
	GuidAttributeCoder mSdCSPDLGuidCoder;
	CertificateAttributeCoder mCertificateCoder;
	ZeroAttributeCoder mZeroCoder;
	ConstAttributeCoder mPublicKeyClassCoder;
	ConstAttributeCoder mPrivateKeyClassCoder;
	ConstAttributeCoder mSessionKeyClassCoder;
	KeyDataAttributeCoder mKeyDataCoder;
	LinkedRecordAttributeCoder mPublicKeyHashCoder;
	DataAttributeCoder mDataAttributeCoder;
	DescriptionAttributeCoder mDescriptionCoder;
};


} // end namespace Tokend

#endif /* !_TOKEND_SCHEMA_H_ */

