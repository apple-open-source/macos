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
// KeyItem.cpp
//
#include <Security/KeyItem.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/cssmtype.h>
#include <Security/Access.h>
#include <Security/Keychains.h>
#include <Security/KeyItem.h>
#include <Security/wrapkey.h>
#include <Security/globals.h>

// @@@ This needs to be shared.
static CSSM_DB_NAME_ATTR(kSecKeyLabel, 6, "Label", 0, NULL, BLOB);
static CSSM_DB_NAME_ATTR(kSecKeyPrintName, 1, "PrintName", 0, NULL, BLOB);

using namespace KeychainCore;

KeyItem::KeyItem(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId) :
	ItemImpl(keychain, primaryKey, uniqueId),
	mKey(NULL)
{
}

KeyItem::KeyItem(const Keychain &keychain, const PrimaryKey &primaryKey)  :
	ItemImpl(keychain, primaryKey),
	mKey(NULL)
{
}

KeyItem::KeyItem(KeyItem &keyItem) :
	ItemImpl(keyItem),
	mKey(NULL)
{
}

KeyItem::~KeyItem()
{
	if (mKey)
	{
		CssmClient::SSDbUniqueRecord uniqueId(ssDbUniqueRecord());
		uniqueId->database()->csp()->freeKey(*mKey);
		uniqueId->allocator().free(mKey);
	}
}

void
KeyItem::update()
{
	MacOSError::throwMe(unimpErr);
}

Item
KeyItem::copyTo(const Keychain &keychain)
{
	MacOSError::throwMe(unimpErr);
}

void
KeyItem::didModify()
{
}

PrimaryKey
KeyItem::add(Keychain &keychain)
{
	MacOSError::throwMe(unimpErr);
}

CssmClient::SSDbUniqueRecord
KeyItem::ssDbUniqueRecord()
{
	DbUniqueRecordImpl *impl = &*dbUniqueRecord();
	return CssmClient::SSDbUniqueRecord(safe_cast<Security::CssmClient::SSDbUniqueRecordImpl *>(impl));
}

const CssmKey &
KeyItem::cssmKey()
{
	if (!mKey)
	{
		CssmClient::SSDbUniqueRecord uniqueId(ssDbUniqueRecord());
		CssmDataContainer dataBlob(uniqueId->allocator());
		uniqueId->get(NULL, &dataBlob);
		mKey = reinterpret_cast<CssmKey *>(dataBlob.Data);
		dataBlob.Data = NULL;
		dataBlob.Length = 0;
	}

	return *mKey;
}

const AccessCredentials *
KeyItem::getCredentials(
	CSSM_ACL_AUTHORIZATION_TAG operation,
	SecCredentialType credentialType)
{
	// @@@ Fix this to actually examine the ACL for this key and consider operation and do the right thing.
	AclFactory factory;
	switch (credentialType)
	{
	case kSecCredentialTypeDefault:
		return globals().credentials();
	case kSecCredentialTypeWithUI:
		return factory.promptCred();
	case kSecCredentialTypeNoUI:
		return factory.nullCred();
	default:
		MacOSError::throwMe(paramErr);
	}
}

void 
KeyItem::createPair(
	Keychain keychain,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE publicKeyUsage,
	uint32 publicKeyAttr,
	CSSM_KEYUSE privateKeyUsage,
	uint32 privateKeyAttr,
	RefPointer<Access> initialAccess,
	RefPointer<KeyItem> &outPublicKey, 
	RefPointer<KeyItem> &outPrivateKey)
{
	bool freeKeys = false;
	bool deleteContext = false;

	if (!keychain->database()->dl()->subserviceMask() & CSSM_SERVICE_CSP)
		MacOSError::throwMe(errSecInvalidKeychain);

	SSDb ssDb(safe_cast<SSDbImpl *>(&(*keychain->database())));
	CssmClient::CSP csp(keychain->csp());
	CssmClient::CSP appleCsp(gGuidAppleCSP);

	// Generate a random label to use initially
	CssmClient::Random random(appleCsp, CSSM_ALGID_APPLE_YARROW);
	uint8 labelBytes[20];
	CssmData label(labelBytes, sizeof(labelBytes));
	random.generate(label, label.Length);

	// Create a Access::Maker for the initial owner of the private key.
	ResourceControlContext rcc;
	memset(&rcc, 0, sizeof(rcc));
	Access::Maker maker;
	// @@@ Potentially provide a credential argument which allows us to generate keys in the csp.  Currently the CSP let's anyone do this, but we might restrict this in the future, f.e. a smartcard could require out of band pin entry before a key can be generated.
	maker.initialOwner(rcc);
	// Create the cred we need to manipulate the keys until we actually set a new access control for them.
	const AccessCredentials *cred = maker.cred();

	CSSM_KEY publicCssmKey, privateCssmKey;
	memset(&publicCssmKey, 0, sizeof(publicCssmKey));
	memset(&privateCssmKey, 0, sizeof(privateCssmKey));

	CSSM_CC_HANDLE ccHandle = 0;

	try
	{
		CSSM_RETURN status;
		if (contextHandle)
				ccHandle = contextHandle;
		else
		{
			status = CSSM_CSP_CreateKeyGenContext(csp->handle(), algorithm, keySizeInBits, NULL, NULL, NULL, NULL, NULL, &ccHandle);
			if (status)
				CssmError::throwMe(status);
			deleteContext = true;
		}
	
		CSSM_DL_DB_HANDLE dldbHandle = ssDb->handle();
		CSSM_DL_DB_HANDLE_PTR dldbHandlePtr = &dldbHandle;
		CSSM_CONTEXT_ATTRIBUTE contextAttributes = { CSSM_ATTRIBUTE_DL_DB_HANDLE, sizeof(dldbHandle), { (char *)dldbHandlePtr } };
		status = CSSM_UpdateContextAttributes(ccHandle, 1, &contextAttributes);
		if (status)
			CssmError::throwMe(status);
	
		// Generate the keypair
		status = CSSM_GenerateKeyPair(ccHandle, publicKeyUsage, publicKeyAttr, &label, &publicCssmKey, privateKeyUsage, privateKeyAttr, &label, &rcc, &privateCssmKey);
		if (status)
			CssmError::throwMe(status);
		freeKeys = true;

		// Find the keys we just generated in the DL to get SecKeyRef's to them
		// so we can change the label to be the hash of the public key, and
		// fix up other attributes.

		// Look up public key in the DLDB.
		DbAttributes pubDbAttributes;
		DbUniqueRecord pubUniqueId;
		SSDbCursor dbPubCursor(ssDb, 1);
		dbPubCursor->recordType(CSSM_DL_DB_RECORD_PUBLIC_KEY);
		dbPubCursor->add(CSSM_DB_EQUAL, kSecKeyLabel, label);
		CssmClient::Key publicKey;
		if (!dbPubCursor->nextKey(&pubDbAttributes, publicKey, pubUniqueId))
			MacOSError::throwMe(errSecItemNotFound);

		// Look up private key in the DLDB.
		DbAttributes privDbAttributes;
		DbUniqueRecord privUniqueId;
		SSDbCursor dbPrivCursor(ssDb, 1);
		dbPrivCursor->recordType(CSSM_DL_DB_RECORD_PRIVATE_KEY);
		dbPrivCursor->add(CSSM_DB_EQUAL, kSecKeyLabel, label);
		CssmClient::Key privateKey;
		if (!dbPrivCursor->nextKey(&privDbAttributes, privateKey, privUniqueId))
			MacOSError::throwMe(errSecItemNotFound);

		// Convert reference public key to a raw key so we can use it
		// in the appleCsp.
		CssmClient::WrapKey wrap(csp, CSSM_ALGID_NONE);
		wrap.cred(cred);
		CssmClient::Key rawPubKey = wrap(publicKey);

		// Calculate the hash of the public key using the appleCSP.
		CssmClient::PassThrough passThrough(appleCsp);
		void *outData;
		CssmData *cssmData;

		/* Given a CSSM_KEY_PTR in any format, obtain the SHA-1 hash of the 
		* associated key blob. 
		* Key is specified in CSSM_CSP_CreatePassThroughContext.
		* Hash is allocated bythe CSP, in the App's memory, and returned
		* in *outData. */
		passThrough.key(rawPubKey);
		passThrough(CSSM_APPLECSP_KEYDIGEST, NULL, &outData);
		cssmData = reinterpret_cast<CssmData *>(outData);
		CssmData &pubKeyHash = *cssmData;

		std::string description(initialAccess->promptDescription());
		// Set the label of the public key to the public key hash.
		// Set the PrintName of the public key to the description in the acl.
		pubDbAttributes.add(kSecKeyLabel, pubKeyHash);
		pubDbAttributes.add(kSecKeyPrintName, description);
		pubUniqueId->modify(CSSM_DL_DB_RECORD_PUBLIC_KEY, &pubDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

		// Set the label of the private key to the public key hash.
		// Set the PrintName of the private key to the description in the acl.
		privDbAttributes.add(kSecKeyLabel, pubKeyHash);
		privDbAttributes.add(kSecKeyPrintName, description);
		privUniqueId->modify(CSSM_DL_DB_RECORD_PRIVATE_KEY, &privDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

		// @@@ Not exception safe!
		csp.allocator().free(cssmData->Data);
		csp.allocator().free(cssmData);

		// Finally fix the acl and owner of the private key to the specified access control settings.
		initialAccess->setAccess(*privateKey, maker);

		// Make the public key acl completely open
		Access pubKeyAccess;
		pubKeyAccess.setAccess(*publicKey, maker);

		// Create keychain items which will represent the keys.
		outPublicKey = safe_cast<KeyItem*>(&(*keychain->item(CSSM_DL_DB_RECORD_PUBLIC_KEY, pubUniqueId)));
		outPrivateKey = safe_cast<KeyItem*>(&(*keychain->item(CSSM_DL_DB_RECORD_PRIVATE_KEY, privUniqueId)));
	}
	catch (...)
	{
		if (freeKeys)
		{
			// Delete the keys if something goes wrong so we don't end up with inaccesable keys in the database.
			CSSM_FreeKey(csp->handle(), cred, &publicCssmKey, TRUE);
			CSSM_FreeKey(csp->handle(), cred, &privateCssmKey, TRUE);
		}
		
		if (deleteContext)
			CSSM_DeleteContext(ccHandle);

		throw;
	}

	if (freeKeys)
	{
		CSSM_FreeKey(csp->handle(), NULL, &publicCssmKey, FALSE);
		CSSM_FreeKey(csp->handle(), NULL, &privateCssmKey, FALSE);
	}

	if (deleteContext)
		CSSM_DeleteContext(ccHandle);
}

void
KeyItem::importPair(
	Keychain keychain,
	const CSSM_KEY &publicWrappedKey,
	const CSSM_KEY &privateWrappedKey,
	RefPointer<Access> initialAccess,
	RefPointer<KeyItem> &outPublicKey, 
	RefPointer<KeyItem> &outPrivateKey)
{
	bool freePublicKey = false;
	bool freePrivateKey = false;
	bool deleteContext = false;

	if (!keychain->database()->dl()->subserviceMask() & CSSM_SERVICE_CSP)
		MacOSError::throwMe(errSecInvalidKeychain);

	SSDb ssDb(safe_cast<SSDbImpl *>(&(*keychain->database())));
	CssmClient::CSP csp(keychain->csp());
	CssmClient::CSP appleCsp(gGuidAppleCSP);

	// Create a Access::Maker for the initial owner of the private key.
	ResourceControlContext rcc;
	memset(&rcc, 0, sizeof(rcc));
	Access::Maker maker;
	// @@@ Potentially provide a credential argument which allows us to unwrap keys in the csp.  Currently the CSP let's anyone do this, but we might restrict this in the future, f.e. a smartcard could require out of band pin entry before a key can be generated.
	maker.initialOwner(rcc);
	// Create the cred we need to manipulate the keys until we actually set a new access control for them.
	const AccessCredentials *cred = maker.cred();

	CSSM_KEY publicCssmKey, privateCssmKey;
	memset(&publicCssmKey, 0, sizeof(publicCssmKey));
	memset(&privateCssmKey, 0, sizeof(privateCssmKey));

	CSSM_CC_HANDLE ccHandle = 0;

	try
	{
		CSSM_RETURN status;

		// Calculate the hash of the public key using the appleCSP.
		CssmClient::PassThrough passThrough(appleCsp);
		void *outData;
		CssmData *cssmData;

		/* Given a CSSM_KEY_PTR in any format, obtain the SHA-1 hash of the 
		* associated key blob. 
		* Key is specified in CSSM_CSP_CreatePassThroughContext.
		* Hash is allocated bythe CSP, in the App's memory, and returned
		* in *outData. */
		passThrough.key(&publicWrappedKey);
		passThrough(CSSM_APPLECSP_KEYDIGEST, NULL, &outData);
		cssmData = reinterpret_cast<CssmData *>(outData);
		CssmData &pubKeyHash = *cssmData;

		status = CSSM_CSP_CreateSymmetricContext(csp->handle(), publicWrappedKey.KeyHeader.AlgorithmId, CSSM_ALGMODE_NONE, NULL, NULL, NULL, CSSM_PADDING_NONE, NULL, &ccHandle);
		if (status)
			CssmError::throwMe(status);
		deleteContext = true;
	
		CSSM_DL_DB_HANDLE dldbHandle = ssDb->handle();
		CSSM_DL_DB_HANDLE_PTR dldbHandlePtr = &dldbHandle;
		CSSM_CONTEXT_ATTRIBUTE contextAttributes = { CSSM_ATTRIBUTE_DL_DB_HANDLE, sizeof(dldbHandle), { (char *)dldbHandlePtr } };
		status = CSSM_UpdateContextAttributes(ccHandle, 1, &contextAttributes);
		if (status)
			CssmError::throwMe(status);
	
		// Unwrap the the keys
		status = CSSM_UnwrapKey(
			ccHandle,
			NULL,
			&publicWrappedKey,
			publicWrappedKey.KeyHeader.KeyUsage,
			publicWrappedKey.KeyHeader.KeyAttr,
			&pubKeyHash,
			&rcc,
			&publicCssmKey,
			NULL);
		if (status)
			CssmError::throwMe(status);
		freePublicKey = true;

		status = CSSM_UnwrapKey(
			ccHandle,
			NULL,
			&privateWrappedKey,
			privateWrappedKey.KeyHeader.KeyUsage,
			privateWrappedKey.KeyHeader.KeyAttr,
			&pubKeyHash,
			&rcc,
			&privateCssmKey,
			NULL);

		if (status)
			CssmError::throwMe(status);

		freePrivateKey = true;

		// Find the keys we just generated in the DL to get SecKeyRef's to them
		// so we can change the label to be the hash of the public key, and
		// fix up other attributes.

		// Look up public key in the DLDB.
		DbAttributes pubDbAttributes;
		DbUniqueRecord pubUniqueId;
		SSDbCursor dbPubCursor(ssDb, 1);
		dbPubCursor->recordType(CSSM_DL_DB_RECORD_PUBLIC_KEY);
		dbPubCursor->add(CSSM_DB_EQUAL, kSecKeyLabel, pubKeyHash);
		CssmClient::Key publicKey;
		if (!dbPubCursor->nextKey(&pubDbAttributes, publicKey, pubUniqueId))
			MacOSError::throwMe(errSecItemNotFound);

		// Look up private key in the DLDB.
		DbAttributes privDbAttributes;
		DbUniqueRecord privUniqueId;
		SSDbCursor dbPrivCursor(ssDb, 1);
		dbPrivCursor->recordType(CSSM_DL_DB_RECORD_PRIVATE_KEY);
		dbPrivCursor->add(CSSM_DB_EQUAL, kSecKeyLabel, pubKeyHash);
		CssmClient::Key privateKey;
		if (!dbPrivCursor->nextKey(&privDbAttributes, privateKey, privUniqueId))
			MacOSError::throwMe(errSecItemNotFound);

		// @@@ Not exception safe!
		csp.allocator().free(cssmData->Data);
		csp.allocator().free(cssmData);

		std::string description(initialAccess->promptDescription());
		// Set the label of the public key to the public key hash.
		// Set the PrintName of the public key to the description in the acl.
		pubDbAttributes.add(kSecKeyPrintName, description);
		pubUniqueId->modify(CSSM_DL_DB_RECORD_PUBLIC_KEY, &pubDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

		// Set the label of the private key to the public key hash.
		// Set the PrintName of the private key to the description in the acl.
		privDbAttributes.add(kSecKeyPrintName, description);
		privUniqueId->modify(CSSM_DL_DB_RECORD_PRIVATE_KEY, &privDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

		// Finally fix the acl and owner of the private key to the specified access control settings.
		initialAccess->setAccess(*privateKey, maker);

		// Make the public key acl completely open
		Access pubKeyAccess;
		pubKeyAccess.setAccess(*publicKey, maker);

		// Create keychain items which will represent the keys.
		outPublicKey = safe_cast<KeyItem*>(&(*keychain->item(CSSM_DL_DB_RECORD_PUBLIC_KEY, pubUniqueId)));
		outPrivateKey = safe_cast<KeyItem*>(&(*keychain->item(CSSM_DL_DB_RECORD_PRIVATE_KEY, privUniqueId)));
	}
	catch (...)
	{
		if (freePublicKey)
			CSSM_FreeKey(csp->handle(), cred, &publicCssmKey, TRUE);
		if (freePrivateKey)
			CSSM_FreeKey(csp->handle(), cred, &privateCssmKey, TRUE);
		
		if (deleteContext)
			CSSM_DeleteContext(ccHandle);

		throw;
	}

	if (freePublicKey)
		CSSM_FreeKey(csp->handle(), cred, &publicCssmKey, FALSE);
	if (freePrivateKey)
		CSSM_FreeKey(csp->handle(), cred, &privateCssmKey, FALSE);

	if (deleteContext)
		CSSM_DeleteContext(ccHandle);
}
