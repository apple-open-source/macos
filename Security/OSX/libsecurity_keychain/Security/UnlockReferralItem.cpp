/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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

//
// UnlockReferralItem - Abstract interface to permanent user trust assignments
//
#include <security_keychain/UnlockReferralItem.h>
#include <security_cdsa_utilities/Schema.h>
#include <security_keychain/SecCFTypes.h>


namespace Security {
namespace KeychainCore {


//
// Construct a UnlockReferralItem from attributes and initial content
//
UnlockReferralItem::UnlockReferralItem() :
	ItemImpl((SecItemClass) CSSM_DL_DB_RECORD_UNLOCK_REFERRAL,
		reinterpret_cast<SecKeychainAttributeList *>(NULL),
		UInt32(0/*size*/),
		NULL/*data*/)
{
	secinfo("referral", "create %p", this);
}


//
// Destroy it
//
UnlockReferralItem::~UnlockReferralItem() 
{
	secinfo("referral", "destroy %p", this);
}


//
// Add item to keychain
//
PrimaryKey UnlockReferralItem::add(Keychain &keychain)
{
	StLock<Mutex>_(mMutex);
	// If we already have a Keychain we can't be added.
	if (mKeychain)
		MacOSError::throwMe(errSecDuplicateItem);

	populateAttributes();

	CSSM_DB_RECORDTYPE recordType = mDbAttributes->recordType();

	Db db(keychain->database());
	// add the item to the (regular) db
	try
	{
		mUniqueId = db->insert(recordType, mDbAttributes.get(), mData.get());
		secinfo("usertrust", "%p inserted", this);
	}
	catch (const CssmError &e)
	{
		if (e.osStatus() != CSSMERR_DL_INVALID_RECORDTYPE)
			throw;

		// Create the referral relation and try again.
		secinfo("usertrust", "adding schema relation for user trusts");
#if 0
		db->createRelation(CSSM_DL_DB_RECORD_UNLOCK_REFERRAL,
			"CSSM_DL_DB_RECORD_UNLOCK_REFERRAL",
			Schema::UnlockReferralSchemaAttributeCount,
			Schema::UnlockReferralSchemaAttributeList,
			Schema::UnlockReferralSchemaIndexCount,
			Schema::UnlockReferralSchemaIndexList);
		keychain->keychainSchema()->didCreateRelation(
			CSSM_DL_DB_RECORD_UNLOCK_REFERRAL,
			"CSSM_DL_DB_RECORD_UNLOCK_REFERRAL",
			Schema::UnlockReferralSchemaAttributeCount,
			Schema::UnlockReferralSchemaAttributeList,
			Schema::UnlockReferralSchemaIndexCount,
			Schema::UnlockReferralSchemaIndexList);
#endif
		//keychain->resetSchema();

		mUniqueId = db->insert(recordType, mDbAttributes.get(), mData.get());
		secinfo("usertrust", "%p inserted now", this);
	}

	mPrimaryKey = keychain->makePrimaryKey(recordType, mUniqueId);
    mKeychain = keychain;
	return mPrimaryKey;
}


void UnlockReferralItem::populateAttributes()
{
#if 0
	CssmAutoData encodedIndex(Allocator::standard());
	makeCertIndex(mCertificate, encodedIndex);
	const CssmOid &policyOid = mPolicy->oid();

	mDbAttributes->add(Schema::attributeInfo(kSecTrustCertAttr), encodedIndex.get());
	mDbAttributes->add(Schema::attributeInfo(kSecTrustPolicyAttr), policyOid);
#endif
}


} // end namespace KeychainCore
} // end namespace Security
