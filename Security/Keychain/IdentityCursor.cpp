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


/*
	File:		IdentityCursor.cpp

	Contains:	Working with IdentityCursor

	Copyright:	2002 by Apple Computer, Inc., all rights reserved.

	To Do:
*/

#include <Security/IdentityCursor.h>
#include <Security/Identity.h>
#include <Security/Item.h>
#include <Security/Certificate.h>
#include <Security/KeyItem.h>
#include <Security/Schema.h>

// From AppleCSPDL
#include <Security/KeySchema.h>

using namespace KeychainCore;

IdentityCursor::IdentityCursor(const StorageManager::KeychainList &searchList, CSSM_KEYUSE keyUsage) :
	mSearchList(searchList),
	mKeyCursor(mSearchList, CSSM_DL_DB_RECORD_PRIVATE_KEY, NULL)
{
	// If keyUsage is CSSM_KEYUSE_ANY then we need a key that can do everything
	if (keyUsage & CSSM_KEYUSE_ANY)
		keyUsage = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT
						   | CSSM_KEYUSE_DERIVE | CSSM_KEYUSE_SIGN
						   | CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_SIGN_RECOVER
						   | CSSM_KEYUSE_VERIFY_RECOVER | CSSM_KEYUSE_WRAP
						   | CSSM_KEYUSE_UNWRAP;

	if (keyUsage & CSSM_KEYUSE_ENCRYPT)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Encrypt, true);
	if (keyUsage & CSSM_KEYUSE_DECRYPT)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Decrypt, true);
	if (keyUsage & CSSM_KEYUSE_DERIVE)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Derive, true);
	if (keyUsage & CSSM_KEYUSE_SIGN)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Sign, true);
	if (keyUsage & CSSM_KEYUSE_VERIFY)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Verify, true);
	if (keyUsage & CSSM_KEYUSE_SIGN_RECOVER)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::SignRecover, true);
	if (keyUsage & CSSM_KEYUSE_VERIFY_RECOVER)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::VerifyRecover, true);
	if (keyUsage & CSSM_KEYUSE_WRAP)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Wrap, true);
	if (keyUsage & CSSM_KEYUSE_UNWRAP)
		mKeyCursor->add(CSSM_DB_EQUAL, KeySchema::Unwrap, true);
}

IdentityCursor::~IdentityCursor()
{
}

bool
IdentityCursor::next(RefPointer<Identity> &identity)
{
	for (;;)
	{
		if (!mCertificateCursor)
		{
			Item key;
			if (!mKeyCursor->next(key))
				return false;
	
			mCurrentKey = static_cast<KeyItem *>(key.get());

			CssmClient::DbUniqueRecord uniqueId = mCurrentKey->dbUniqueRecord();
			CssmClient::DbAttributes dbAttributes(uniqueId->database(), 1);
			dbAttributes.add(KeySchema::Label);
			uniqueId->get(&dbAttributes, NULL);
			const CssmData &keyHash = dbAttributes[0];

			mCertificateCursor = KCCursor(mSearchList, CSSM_DL_DB_RECORD_X509_CERTIFICATE, NULL);
			mCertificateCursor->add(CSSM_DB_EQUAL, Schema::kX509CertificatePublicKeyHash, keyHash);
		}
	
		Item cert;
		if (mCertificateCursor->next(cert))
		{
			RefPointer<Certificate> certificate(static_cast<Certificate *>(cert.get()));
			identity = new Identity(mCurrentKey, certificate);
			return true;
		}
		else
			mCertificateCursor = KCCursor();
	}
}
