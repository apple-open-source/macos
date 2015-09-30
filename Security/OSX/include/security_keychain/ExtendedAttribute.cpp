/*
 * Copyright (c) 2006,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * ExtendedAttribute.cpp - Extended Keychain Item Attribute class.
 *
 */

#include "ExtendedAttribute.h"
#include "SecKeychainItemExtendedAttributes.h"
#include "SecKeychainItemPriv.h"
#include "cssmdatetime.h"
#include <security_cdsa_utilities/Schema.h>

using namespace KeychainCore;

/* 
 * Construct new ExtendedAttr from API.
 */
ExtendedAttribute::ExtendedAttribute(
	CSSM_DB_RECORDTYPE recordType, 
	const CssmData &itemID, 
	const CssmData attrName,
	const CssmData attrValue) :
		ItemImpl(CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE, 
			reinterpret_cast<SecKeychainAttributeList *>(NULL), 
			0, NULL),
		mRecordType(recordType),
		mItemID(Allocator::standard(), itemID.Data, itemID.Length),
		mAttrName(Allocator::standard(), attrName.Data, attrName.Length),
		mAttrValue(Allocator::standard(), attrValue.Data, attrValue.Length)
{
	setupAttrs();
}

// db item contstructor
ExtendedAttribute::ExtendedAttribute(
	const Keychain &keychain, 
	const PrimaryKey &primaryKey, 
	const CssmClient::DbUniqueRecord &uniqueId) :
		ItemImpl(keychain, primaryKey, uniqueId),
		mRecordType(0),
		mItemID(Allocator::standard()),
		mAttrName(Allocator::standard()),
		mAttrValue(Allocator::standard())
{

}

// PrimaryKey item contstructor
ExtendedAttribute::ExtendedAttribute(
	const Keychain &keychain, 
	const PrimaryKey &primaryKey) :
		ItemImpl(keychain, primaryKey),
		mRecordType(0),
		mItemID(Allocator::standard()),
		mAttrName(Allocator::standard()),
		mAttrValue(Allocator::standard())
{

}

ExtendedAttribute* ExtendedAttribute::make(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId)
{
	ExtendedAttribute* ea = new ExtendedAttribute(keychain, primaryKey, uniqueId);
	keychain->addItem(primaryKey, ea);
	return ea;
}



ExtendedAttribute* ExtendedAttribute::make(const Keychain &keychain, const PrimaryKey &primaryKey)
{
	ExtendedAttribute* ea = new ExtendedAttribute(keychain, primaryKey);
	keychain->addItem(primaryKey, ea);
	return ea;
}



// copy - required due to Item's weird constructor/vendor
ExtendedAttribute::ExtendedAttribute(
	ExtendedAttribute &extendedAttr) :
		ItemImpl(extendedAttr),
		mRecordType(extendedAttr.mRecordType),
		mItemID(Allocator::standard()),
		mAttrName(Allocator::standard()),
		mAttrValue(Allocator::standard())
{
	// CssmData cd = extendedAttr.mItemID;
	mItemID.copy(extendedAttr.mItemID);
	// cd = extendedAttr.mAttrName;
	mAttrName.copy(extendedAttr.mAttrName);
	// cd = extendedAttr.mAttrValue;
	mAttrValue.copy(extendedAttr.mAttrValue);
	setupAttrs();
}

ExtendedAttribute::~ExtendedAttribute() throw()
{

}

PrimaryKey
ExtendedAttribute::add(Keychain &keychain)
{
	StLock<Mutex>_(mMutex);
	// If we already have a Keychain we can't be added.
	if (mKeychain)
		MacOSError::throwMe(errSecDuplicateItem);

	SInt64 date;
	CSSMDateTimeUtils::GetCurrentMacLongDateTime(date);
	CssmDbAttributeInfo attrInfo(kSecModDateItemAttr, CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE);
	setAttribute(attrInfo, date);

	Db db(keychain->database());
	// add the item to the (regular) db
	try
	{
		mUniqueId = db->insert(CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE, mDbAttributes.get(), mData.get());
	}
	catch (const CssmError &e)
	{
		if (e.osStatus() != CSSMERR_DL_INVALID_RECORDTYPE)
			throw;

		/* 
		 * First exposure of this keychain to the extended attribute record type.
		 * Create the relation and try again.
		 */
		db->createRelation(CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE,
			"CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE",
			Schema::ExtendedAttributeSchemaAttributeCount,
			Schema::ExtendedAttributeSchemaAttributeList,
			Schema::ExtendedAttributeSchemaIndexCount,
			Schema::ExtendedAttributeSchemaIndexList);
		keychain->keychainSchema()->didCreateRelation(
			CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE,
			"CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE",
			Schema::ExtendedAttributeSchemaAttributeCount,
			Schema::ExtendedAttributeSchemaAttributeList,
			Schema::ExtendedAttributeSchemaIndexCount,
			Schema::ExtendedAttributeSchemaIndexList);

		mUniqueId = db->insert(CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE, mDbAttributes.get(), mData.get());
	}

	mPrimaryKey = keychain->makePrimaryKey(CSSM_DL_DB_RECORD_EXTENDED_ATTRIBUTE, mUniqueId);
    mKeychain = keychain;

	return mPrimaryKey;
}

/* set up DB attrs based on member vars */
void ExtendedAttribute::setupAttrs()
{
	StLock<Mutex>_(mMutex);
	CssmDbAttributeInfo attrInfo1(kExtendedAttrRecordTypeAttr, CSSM_DB_ATTRIBUTE_FORMAT_UINT32);
	setAttribute(attrInfo1, (uint32)mRecordType);
	CssmData cd = mItemID;
	CssmDbAttributeInfo attrInfo2(kExtendedAttrItemIDAttr, CSSM_DB_ATTRIBUTE_FORMAT_BLOB);
	setAttribute(attrInfo2, cd);
	cd = mAttrName;
	CssmDbAttributeInfo attrInfo3(kExtendedAttrAttributeNameAttr, CSSM_DB_ATTRIBUTE_FORMAT_BLOB);
	setAttribute(attrInfo3, cd);
	cd = mAttrValue;
	CssmDbAttributeInfo attrInfo4(kExtendedAttrAttributeValueAttr, CSSM_DB_ATTRIBUTE_FORMAT_BLOB);
	setAttribute(attrInfo4, cd);
}


