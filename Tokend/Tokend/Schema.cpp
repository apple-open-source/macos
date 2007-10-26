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
 *  Schema.cpp
 *  TokendMuscle
 */

#include "Schema.h"

#include "Attribute.h"
#include "MetaRecord.h"
#include "MetaAttribute.h"

#include <Security/SecKey.h>
#include <Security/SecCertificate.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/cssmapple.h>

namespace Tokend
{

#pragma mark ---------------- Schema --------------

Schema::Schema() :
	mTrueCoder(true),
	mFalseCoder(false),
	mCertEncodingBERCoder(CSSM_CERT_ENCODING(CSSM_CERT_ENCODING_BER)),
	mSdCSPDLGuidCoder(gGuidAppleSdCSPDL),
	mPublicKeyClassCoder(CSSM_KEYCLASS(CSSM_KEYCLASS_PUBLIC_KEY)),
	mPrivateKeyClassCoder(CSSM_KEYCLASS(CSSM_KEYCLASS_PRIVATE_KEY)),
	mSessionKeyClassCoder(CSSM_KEYCLASS(CSSM_KEYCLASS_SESSION_KEY))
{
}

Schema::~Schema()
{
	try
	{
		for_each_map_delete(mRelationMap.begin(), mRelationMap.end());
	}
	catch(...) {}
}

void Schema::create()
{
    // Attribute names.
    std::string
        an_RelationID("RelationID"),
        an_RelationName("RelationName"),
        an_AttributeID("AttributeID"),
        an_AttributeNameFormat("AttributeNameFormat"),
        an_AttributeName("AttributeName"),
        an_AttributeNameID("AttributeNameID"),
        an_AttributeFormat("AttributeFormat"),
        an_IndexID("IndexID"),
        an_IndexType("IndexType"),
        an_IndexedDataLocation("IndexedDataLocation");

    // Record the attributeIndex of each created attribute for use by our
	// register functions laster on.
	// Create CSSM_DL_DB_SCHEMA_INFO relation.
    MetaRecord *mrio = new MetaRecord(CSSM_DL_DB_SCHEMA_INFO);
    io_rid = mrio->createAttribute(an_RelationID,
		kAF_UINT32).attributeIndex();
    io_rn  = mrio->createAttribute(an_RelationName,
		kAF_STRING).attributeIndex();
    mInfo = createRelation(mrio);

    // Create CSSM_DL_DB_SCHEMA_ATTRIBUTES relation
    MetaRecord *mras = new MetaRecord(CSSM_DL_DB_SCHEMA_ATTRIBUTES);
    as_rid = mras->createAttribute(an_RelationID,
		kAF_UINT32).attributeIndex();
    as_aid = mras->createAttribute(an_AttributeID,
		kAF_UINT32).attributeIndex();
    as_anf = mras->createAttribute(an_AttributeNameFormat,
		kAF_UINT32).attributeIndex();
    as_an  = mras->createAttribute(an_AttributeName,
		kAF_STRING).attributeIndex();
    as_anid= mras->createAttribute(an_AttributeNameID,
		kAF_BLOB  ).attributeIndex();
    as_af  = mras->createAttribute(an_AttributeFormat,
		kAF_UINT32).attributeIndex();
    mAttributes = createRelation(mras);

    // Create CSSM_DL_DB_SCHEMA_INDEXES relation
    MetaRecord *mrix = new MetaRecord(CSSM_DL_DB_SCHEMA_INDEXES);
    ix_rid = mrix->createAttribute(an_RelationID,
		kAF_UINT32).attributeIndex();
    ix_iid = mrix->createAttribute(an_IndexID,
		kAF_UINT32).attributeIndex();
    ix_aid = mrix->createAttribute(an_AttributeID,
		kAF_UINT32).attributeIndex();
    ix_it  = mrix->createAttribute(an_IndexType,
		kAF_UINT32).attributeIndex();
    ix_idl = mrix->createAttribute(an_IndexedDataLocation,
		kAF_UINT32).attributeIndex();
    mIndices = createRelation(mrix);

#ifdef ADD_SCHEMA_PARSING_MODULE
    // @@@ Skipping CSSM_DL_DB_SCHEMA_PARSING_MODULE relation since no one uses
	// it and it's definition in CDSA is broken anyway

    // Attribute names.
    std::string
        an_ModuleID("ModuleID"),
        an_AddinVersion("AddinVersion"),
        an_SSID("SSID"),
        an_SubserviceType("SubserviceType");

    // Create CSSM_DL_DB_SCHEMA_PARSING_MODULE Relation
    MetaRecord *mr_parsing = new MetaRecord(CSSM_DL_DB_SCHEMA_PARSING_MODULE);
    mr_parsing->createAttribute(an_AttributeID,            kAF_UINT32);
    mr_parsing->createAttribute(an_ModuleID,               kAF_BLOB  );
    mr_parsing->createAttribute(an_AddinVersion,           kAF_STRING);
    mr_parsing->createAttribute(an_SSID,                   kAF_UINT32);
    mr_parsing->createAttribute(an_SubserviceType,         kAF_UINT32);
    createRelation(mr_parsing);
#endif

#ifdef REGISTER_SCHEMA_RELATIONS
	registerRelation("CSSM_DL_DB_SCHEMA_INFO", CSSM_DL_DB_SCHEMA_INFO)
	registerAttribute(CSSM_DL_DB_SCHEMA_INFO, &an_RelationID, 0,
		kAF_UINT32, true);
	registerAttribute(CSSM_DL_DB_SCHEMA_INFO, &an_RelationName, 1,
		kAF_UINT32, false);
	registerRelation("CSSM_DL_DB_SCHEMA_ATTRIBUTES",
		CSSM_DL_DB_SCHEMA_ATTRIBUTES)
	registerAttribute(CSSM_DL_DB_SCHEMA_ATTRIBUTES, &an_RelationID, 0,
		kAF_UINT32, true);
	registerAttribute(CSSM_DL_DB_SCHEMA_ATTRIBUTES, &an_AttributeID, 2,
		kAF_UINT32, true);
	registerAttribute(CSSM_DL_DB_SCHEMA_ATTRIBUTES, &an_AttributeNameFormat, 3,
		kAF_UINT32, false);
	registerAttribute(CSSM_DL_DB_SCHEMA_ATTRIBUTES, &an_AttributeName, 4,
		kAF_STRING, false);
	registerAttribute(CSSM_DL_DB_SCHEMA_ATTRIBUTES, &an_AttributeNameId, 5,
		kAF_BLOB, false);
	registerAttribute(CSSM_DL_DB_SCHEMA_ATTRIBUTES, &an_AttributeFormat, 6,
		kAF_UINT32, false);
	registerRelation("CSSM_DL_DB_SCHEMA_INDEXES", CSSM_DL_DB_SCHEMA_INDEXES)
	registerAttribute(CSSM_DL_DB_SCHEMA_INDEXES, &an_RelationID, 0,
		kAF_UINT32, true);
	registerAttribute(CSSM_DL_DB_SCHEMA_INDEXES, &an_IndexID, 1,
		kAF_UINT32, true);
	registerAttribute(CSSM_DL_DB_SCHEMA_INDEXES, &an_AttributeID, 2,
		kAF_UINT32, true);
	registerAttribute(CSSM_DL_DB_SCHEMA_INDEXES, &an_IndexType, 3,
		kAF_UINT32, false);
	registerAttribute(CSSM_DL_DB_SCHEMA_INDEXES, &an_IndexedDataLocation, 4,
		kAF_UINT32, false);
#endif
}

// Create one of the standard relations conforming to what the SecKeychain
// layer expects.
Relation *Schema::createStandardRelation(RelationId relationId)
{
	std::string relationName;
	// Get the name based on the relation
	switch (relationId)
	{
	case CSSM_DL_DB_RECORD_PRIVATE_KEY:
		relationName = "CSSM_DL_DB_RECORD_PRIVATE_KEY"; break;
	case CSSM_DL_DB_RECORD_PUBLIC_KEY:
		relationName = "CSSM_DL_DB_RECORD_PUBLIC_KEY"; break;
	case CSSM_DL_DB_RECORD_SYMMETRIC_KEY:
		relationName = "CSSM_DL_DB_RECORD_SYMMETRIC_KEY"; break;
	case CSSM_DL_DB_RECORD_X509_CERTIFICATE:
		relationName = "CSSM_DL_DB_RECORD_X509_CERTIFICATE"; break;
	case CSSM_DL_DB_RECORD_GENERIC:
		relationName = "CSSM_DL_DB_RECORD_GENERIC"; break;
	default: CssmError::throwMe(CSSMERR_DL_INVALID_RECORDTYPE);
	}

    Relation *rt = createRelation(relationName, relationId);

	std::string
        an_CertType = "CertType",
        an_CertEncoding = "CertEncoding",
        an_PrintName = "PrintName",
        an_Alias = "Alias",
        an_Subject = "Subject",
        an_Issuer = "Issuer",
        an_SerialNumber = "SerialNumber",
        an_SubjectKeyIdentifier = "SubjectKeyIdentifier",
        an_PublicKeyHash = "PublicKeyHash",
		an_KeyClass = "KeyClass",
		an_Permanent = "Permanent",
		an_Private = "Private",
		an_Modifiable = "Modifiable",
		an_Label = "Label",
		an_ApplicationTag = "ApplicationTag",
		an_KeyCreator = "KeyCreator",
		an_KeyType = "KeyType",
		an_KeySizeInBits = "KeySizeInBits",
		an_EffectiveKeySize = "EffectiveKeySize",
		an_StartDate = "StartDate",
		an_EndDate = "EndDate",
		an_Sensitive = "Sensitive",
		an_AlwaysSensitive = "AlwaysSensitive",
		an_Extractable = "Extractable",
		an_NeverExtractable = "NeverExtractable",
		an_Encrypt = "Encrypt",
		an_Decrypt = "Decrypt",
		an_Derive = "Derive",
		an_Sign = "Sign",
		an_Verify = "Verify",
		an_SignRecover = "SignRecover",
		an_VerifyRecover = "VerifyRecover",
		an_Wrap = "Wrap",
		an_Unwrap = "Unwrap";

	// @@@ HARDWIRED Based on what SecKeychain layer expects @@@
	switch (relationId)
	{
	case CSSM_DL_DB_RECORD_GENERIC:
		createAttribute(*rt, &an_PrintName, kSecLabelItemAttr, kAF_BLOB, false)
			.attributeCoder(&mDescriptionCoder);
		createAttribute(*rt, &an_Alias, kSecAlias, kAF_BLOB, false)
			.attributeCoder(&mZeroCoder);
		rt->metaRecord().attributeCoderForData(&mDataAttributeCoder);
		break;
	case CSSM_DL_DB_RECORD_X509_CERTIFICATE:
        createAttribute(*rt, &an_CertType, kSecCertTypeItemAttr,
			kAF_UINT32, true).attributeCoder(&mCertificateCoder);
        createAttribute(*rt, &an_CertEncoding, kSecCertEncodingItemAttr,
			kAF_UINT32, false).attributeCoder(&mCertEncodingBERCoder);
        createAttribute(*rt, &an_PrintName, kSecLabelItemAttr,
			kAF_BLOB, false).attributeCoder(&mCertificateCoder);
        createAttribute(*rt, &an_Alias, kSecAlias,
			kAF_BLOB, false).attributeCoder(&mCertificateCoder);
        createAttribute(*rt, &an_Subject, kSecSubjectItemAttr,
			kAF_BLOB, false).attributeCoder(&mCertificateCoder);
        createAttribute(*rt, &an_Issuer, kSecIssuerItemAttr,
			kAF_BLOB, true).attributeCoder(&mCertificateCoder);
        createAttribute(*rt, &an_SerialNumber, kSecSerialNumberItemAttr,
			kAF_BLOB, true).attributeCoder(&mCertificateCoder);
        createAttribute(*rt, &an_SubjectKeyIdentifier,
			kSecSubjectKeyIdentifierItemAttr,
			kAF_BLOB, false).attributeCoder(&mCertificateCoder);
        createAttribute(*rt, &an_PublicKeyHash, kSecPublicKeyHashItemAttr,
			kAF_BLOB, false).attributeCoder(&mCertificateCoder);
		rt->metaRecord().attributeCoderForData(&mDataAttributeCoder);
        // Initialize mPublicKeyHashCoder so it knows which attribute of a
		// certificate to use to get the public key hash of a key.
        mPublicKeyHashCoder.setCertificateMetaAttribute(&(rt->metaRecord()
			.metaAttribute(kSecPublicKeyHashItemAttr)));
		break;
	case CSSM_DL_DB_RECORD_PUBLIC_KEY:
	case CSSM_DL_DB_RECORD_PRIVATE_KEY:
	case CSSM_DL_DB_RECORD_SYMMETRIC_KEY:
		rt->metaRecord().attributeCoderForData(&mKeyDataCoder);
		createAttribute(*rt, &an_KeyClass, kSecKeyKeyClass,
			kAF_UINT32, false).attributeCoder(
				relationId == CSSM_DL_DB_RECORD_PUBLIC_KEY
				? &mPublicKeyClassCoder
				: relationId == CSSM_DL_DB_RECORD_PRIVATE_KEY
					? &mPrivateKeyClassCoder
					: &mSessionKeyClassCoder);
		createAttribute(*rt, &an_PrintName, kSecKeyPrintName,
			kAF_BLOB, false).attributeCoder(&mZeroCoder);
		createAttribute(*rt, &an_Alias, kSecKeyAlias,
			kAF_BLOB, false).attributeCoder(&mZeroCoder);
		createAttribute(*rt, &an_Permanent, kSecKeyPermanent,
			kAF_UINT32, false).attributeCoder(&mTrueCoder);
		createAttribute(*rt, &an_Private, kSecKeyPrivate,
			kAF_UINT32, false).attributeCoder(
				relationId == CSSM_DL_DB_RECORD_PUBLIC_KEY
				? &mFalseCoder : &mTrueCoder);
		createAttribute(*rt, &an_Modifiable, kSecKeyModifiable,
			kAF_UINT32, false).attributeCoder(&mFalseCoder);
		createAttribute(*rt, &an_Label, kSecKeyLabel,
			kAF_BLOB, true).attributeCoder(
				relationId == CSSM_DL_DB_RECORD_PRIVATE_KEY
				? &mPublicKeyHashCoder : NULL);
		createAttribute(*rt, &an_ApplicationTag, kSecKeyApplicationTag,
			kAF_BLOB, true).attributeCoder(&mZeroCoder);
		createAttribute(*rt, &an_KeyCreator, kSecKeyKeyCreator,
			kAF_BLOB, true).attributeCoder(&mSdCSPDLGuidCoder);
		createAttribute(*rt, &an_KeyType, kSecKeyKeyType, kAF_UINT32, true);
		createAttribute(*rt, &an_KeySizeInBits, kSecKeyKeySizeInBits,
			kAF_UINT32, true);
		createAttribute(*rt, &an_EffectiveKeySize, kSecKeyEffectiveKeySize,
			kAF_UINT32, true);
		createAttribute(*rt, &an_StartDate, kSecKeyStartDate,
			kAF_TIME_DATE, true).attributeCoder(&mZeroCoder);
		createAttribute(*rt, &an_EndDate, kSecKeyEndDate,
			kAF_TIME_DATE, true).attributeCoder(&mZeroCoder);
		createAttribute(*rt, &an_Sensitive, kSecKeySensitive,
			kAF_UINT32, false).attributeCoder(
				relationId == CSSM_DL_DB_RECORD_PUBLIC_KEY
				? &mFalseCoder : &mTrueCoder);
		createAttribute(*rt, &an_AlwaysSensitive, kSecKeyAlwaysSensitive,
			kAF_UINT32, false).attributeCoder(&mFalseCoder);
		createAttribute(*rt, &an_Extractable, kSecKeyExtractable,
			kAF_UINT32, false).attributeCoder(&mFalseCoder);
		createAttribute(*rt, &an_NeverExtractable, kSecKeyNeverExtractable,
			kAF_UINT32, false).attributeCoder(&mFalseCoder);
		createAttribute(*rt, &an_Encrypt, kSecKeyEncrypt, kAF_UINT32, false);
		createAttribute(*rt, &an_Decrypt, kSecKeyDecrypt, kAF_UINT32, false);
		createAttribute(*rt, &an_Derive, kSecKeyDerive, kAF_UINT32, false);
		createAttribute(*rt, &an_Sign, kSecKeySign, kAF_UINT32, false);
		createAttribute(*rt, &an_Verify, kSecKeyVerify, kAF_UINT32, false);
		createAttribute(*rt, &an_SignRecover, kSecKeySignRecover,
			kAF_UINT32, false);
		createAttribute(*rt, &an_VerifyRecover, kSecKeyVerifyRecover,
			kAF_UINT32, false);
		createAttribute(*rt, &an_Wrap, kSecKeyWrap, kAF_UINT32, false);
		createAttribute(*rt, &an_Unwrap, kSecKeyUnwrap, kAF_UINT32, false);
        // Initialize mPublicKeyHashCoder so it knows which attribute of a
		// public key to use to get the public key hash of a key.
        if (relationId == CSSM_DL_DB_RECORD_PUBLIC_KEY)
            mPublicKeyHashCoder.setPublicKeyMetaAttribute(&(rt->metaRecord()
				.metaAttribute(kSecKeyLabel)));
		break;
	}

	return rt;
}

// Create a new relation using metaRecord.  Does not register this in the
// CSSM_DL_DB_SCHEMA_INFO relation.  This is used for creating the schema
// relations themselves only.
Relation *Schema::createRelation(MetaRecord *metaRecord)
{
	auto_ptr<Relation> aRelation(new Relation(metaRecord));

	if (!mRelationMap.insert(RelationMap::value_type(metaRecord->relationId(),
		aRelation.get())).second)
	{
		// @@@ Should be CSSMERR_DL_DUPLICATE_RECORDTYPE.  Since that
		// doesn't exist we report that the meta-relation's unique index would
		// no longer be valid
        CssmError::throwMe(CSSMERR_DL_INVALID_UNIQUE_INDEX_DATA);
	}

	return aRelation.release();
}

// Create a new relation and register this in the CSSM_DL_DB_SCHEMA_INFO
// relation.
Relation *Schema::createRelation(const std::string &relationName,
	RelationId relationId)
{
    MetaRecord *mr = new MetaRecord(relationId);
    Relation *rt = createRelation(mr);
	registerRelation(relationName, relationId);
    return rt;
}

// Create a new attribute and register this with the schema.  Do not use this
// for creating schema relations.
MetaAttribute &Schema::createAttribute(Relation &relation,
    const std::string *name, uint32 attributeId,
	CSSM_DB_ATTRIBUTE_FORMAT attributeFormat, bool isIndex)
{
    MetaRecord &mr = relation.metaRecord();
	registerAttribute(mr.relationId(), name, attributeId, attributeFormat,
		isIndex);
    return mr.createAttribute(name, NULL, attributeId, attributeFormat);
}

// Insert a record containing a relationId and it's name into
// CSSM_DL_DB_SCHEMA_INFO relation
void Schema::registerRelation(const std::string &relationName,
	RelationId relationId)
{
    RefPointer<Record> record = new Record();
    record->attributeAtIndex(io_rid, new Attribute(relationId));
    record->attributeAtIndex(io_rn,  new Attribute(relationName));
    mInfo->insertRecord(record);
}

// Insert a record containing a relationId, attributeId and other meta
// information into the CSSM_DL_DB_SCHEMA_ATTRIBUTES relation.  In addition, if
// isIndex is true insert a record into the CSSM_DL_DB_SCHEMA_INDEXES relation. 
void Schema::registerAttribute(RelationId relationId, const std::string *name,
	uint32 attributeId, CSSM_DB_ATTRIBUTE_FORMAT attributeFormat, bool isIndex)
{
    CSSM_DB_ATTRIBUTE_NAME_FORMAT nameFormat = name
		? CSSM_DB_ATTRIBUTE_NAME_AS_STRING : CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER;

    RefPointer<Record> rc_attribute = new Record();

    rc_attribute->attributeAtIndex(as_rid, new Attribute(relationId));
    rc_attribute->attributeAtIndex(as_aid, new Attribute(attributeId));
    rc_attribute->attributeAtIndex(as_anf, new Attribute(nameFormat));
    rc_attribute->attributeAtIndex(as_an, name
		? new Attribute(*name) : new Attribute());           // AttributeName
    rc_attribute->attributeAtIndex(as_anid, new Attribute());// AttributeNameId
    rc_attribute->attributeAtIndex(as_af,  new Attribute(attributeFormat));
    mAttributes->insertRecord(rc_attribute);

    if (isIndex)
    {
        RefPointer<Record> rc_index = new Record();
        rc_index->attributeAtIndex(ix_rid,               // RelationId
			new Attribute(relationId));
        rc_index->attributeAtIndex(ix_iid,               // IndexId
			new Attribute(uint32(0)));
        rc_index->attributeAtIndex(ix_aid,               // AttributeId
			new Attribute(attributeId));
        rc_index->attributeAtIndex(ix_it,                // IndexType
			new Attribute(uint32(CSSM_DB_INDEX_UNIQUE)));
        rc_index->attributeAtIndex(ix_idl,               // IndexedDataLocation
			new Attribute(uint32(CSSM_DB_INDEX_ON_UNKNOWN)));
        mIndices->insertRecord(rc_index);
    }
}


#pragma mark ---------------- Utility methods --------------

const Relation &Schema::findRelation(RelationId inRelationId) const
{
    RelationMap::const_iterator it = mRelationMap.find(inRelationId);
    if (it == mRelationMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORDTYPE);
	return *it->second;
}

Relation &Schema::findRelation(RelationId inRelationId)
{
    RelationMap::iterator it = mRelationMap.find(inRelationId);
    if (it == mRelationMap.end())
		CssmError::throwMe(CSSMERR_DL_INVALID_RECORDTYPE);
	return *it->second;
}

MetaRecord &Schema::findMetaRecord(RelationId inRelationId)
{
	return findRelation(inRelationId).metaRecord();
}

} // end namespace Tokend

