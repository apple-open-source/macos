/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
// TrustStore.h - Abstract interface to permanent user trust assignments
//
#include <Security/TrustItem.h>
#include <Security/Schema.h>
#include <Security/SecCFTypes.h>


namespace Security {
namespace KeychainCore {


//
// Construct a UserTrustItem from attributes and initial content
//
UserTrustItem::UserTrustItem(Certificate *cert, Policy *policy, const TrustData &trustData) :
	ItemImpl(CSSM_DL_DB_RECORD_USER_TRUST,
		reinterpret_cast<SecKeychainAttributeList *>(NULL),
		UInt32(sizeof(trustData)),
		reinterpret_cast<const void *>(&trustData)),
	mCertificate(cert), mPolicy(policy)
{
	debug("usertrust", "create %p (%p,%p) = %d", this, cert, policy, trustData.trust);
}


//
// Destroy it
//
UserTrustItem::~UserTrustItem()
{
	debug("usertrust", "destroy %p", this);
}


//
// Retrieve the trust value from a UserTrustItem
//
UserTrustItem::TrustData UserTrustItem::trust()
{
	CssmDataContainer data;
	getData(data);
	if (data.length() != sizeof(TrustData))
		MacOSError::throwMe(errSecInvalidTrustSetting);
	return *data.interpretedAs<TrustData>();
}


//
// Add item to keychain
//
PrimaryKey UserTrustItem::add(Keychain &keychain)
{
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
		debug("usertrust", "%p inserted", this);
	}
	catch (const CssmError &e)
	{
		if (e.cssmError() != CSSMERR_DL_INVALID_RECORDTYPE)
			throw;

		// Create the cert relation and try again.
		debug("usertrust", "adding schema relation for user trusts");
		db->createRelation(CSSM_DL_DB_RECORD_USER_TRUST, "CSSM_DL_DB_RECORD_USER_TRUST",
			Schema::UserTrustSchemaAttributeCount,
			Schema::UserTrustSchemaAttributeList,
			Schema::UserTrustSchemaIndexCount,
			Schema::UserTrustSchemaIndexList);

		mUniqueId = db->insert(recordType, mDbAttributes.get(), mData.get());
		debug("usertrust", "%p inserted now", this);
	}

	mPrimaryKey = keychain->makePrimaryKey(recordType, mUniqueId);
    mKeychain = keychain;

	return mPrimaryKey;
}


void UserTrustItem::populateAttributes()
{
	const CssmData &certData = mCertificate->data();
	const CssmOid &policyOid = mPolicy->oid();
	mDbAttributes->add(Schema::attributeInfo(kSecTrustCertAttr), certData);
	mDbAttributes->add(Schema::attributeInfo(kSecTrustPolicyAttr), policyOid);
}


} // end namespace KeychainCore
} // end namespace Security
