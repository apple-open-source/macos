/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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
// KeyItem.cpp
//
#include <security_keychain/KeyItem.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/cssmtype.h>
#include <security_keychain/Access.h>
#include <security_keychain/Keychains.h>
#include <security_keychain/KeyItem.h>
#include <security_cdsa_client/wrapkey.h>
#include <security_cdsa_client/genkey.h>
#include <security_cdsa_client/signclient.h>
#include <security_cdsa_client/cryptoclient.h>

#include <security_keychain/Globals.h>
#include "KCEventNotifier.h"

// @@@ This needs to be shared.
static CSSM_DB_NAME_ATTR(kInfoKeyPrintName, kSecKeyPrintName, (char*) "PrintName", 0, NULL, BLOB);
static CSSM_DB_NAME_ATTR(kInfoKeyLabel, kSecKeyLabel, (char*) "Label", 0, NULL, BLOB);
static CSSM_DB_NAME_ATTR(kInfoKeyApplicationTag, kSecKeyApplicationTag, (char*) "ApplicationTag", 0, NULL, BLOB);

using namespace KeychainCore;
using namespace CssmClient;

KeyItem::KeyItem(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId) :
	ItemImpl(keychain, primaryKey, uniqueId),
	mKey(),
	algid(NULL),
	mPubKeyHash(Allocator::standard())
{
}

KeyItem::KeyItem(const Keychain &keychain, const PrimaryKey &primaryKey)  :
	ItemImpl(keychain, primaryKey),
	mKey(),
	algid(NULL),
	mPubKeyHash(Allocator::standard())
{
}

KeyItem* KeyItem::make(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId)
{
	KeyItem* k = new KeyItem(keychain, primaryKey, uniqueId);
	keychain->addItem(primaryKey, k);
	return k;
}



KeyItem* KeyItem::make(const Keychain &keychain, const PrimaryKey &primaryKey)
{
	KeyItem* k = new KeyItem(keychain, primaryKey);
	keychain->addItem(primaryKey, k);
	return k;
}



KeyItem::KeyItem(KeyItem &keyItem) :
	ItemImpl(keyItem),
	mKey(),
	algid(NULL),
	mPubKeyHash(Allocator::standard())
{
	// @@@ this doesn't work for keys that are not in a keychain.
}

KeyItem::KeyItem(const CssmClient::Key &key) :
    ItemImpl(key->keyClass() + CSSM_DL_DB_RECORD_PUBLIC_KEY, (OSType)0, (UInt32)0, (const void*)NULL),
	mKey(key),
	algid(NULL),
	mPubKeyHash(Allocator::standard())
{
	if (key->keyClass() > CSSM_KEYCLASS_SESSION_KEY)
		MacOSError::throwMe(paramErr);
}

KeyItem::~KeyItem()
{
}

void
KeyItem::update()
{
	ItemImpl::update();
}

Item
KeyItem::copyTo(const Keychain &keychain, Access *newAccess)
{
	if (!(keychain->database()->dl()->subserviceMask() & CSSM_SERVICE_CSP))
		MacOSError::throwMe(errSecInvalidKeychain);

	/* Get the destination keychain's db. */
	SSDbImpl* dbImpl = dynamic_cast<SSDbImpl*>(&(*keychain->database()));
	if (dbImpl == NULL)
	{
		CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
	}

	SSDb ssDb(dbImpl);

	/* Make sure mKey is valid. */
	key();

	// Generate a random label to use initially
	CssmClient::CSP appleCsp(gGuidAppleCSP);
	CssmClient::Random random(appleCsp, CSSM_ALGID_APPLE_YARROW);
	uint8 labelBytes[20];
	CssmData label(labelBytes, sizeof(labelBytes));
	random.generate(label, label.Length);

	/* Set up the ACL for the new key. */
	SecPointer<Access> access;
	if (newAccess)
		access = newAccess;
	else
		access = new Access(*mKey);

	/* Generate a random 3DES wrapping Key. */
	CssmClient::GenerateKey genKey(csp(), CSSM_ALGID_3DES_3KEY, 192);
	CssmClient::Key wrappingKey(genKey(KeySpec(CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP,
		CSSM_KEYATTR_EXTRACTABLE /* | CSSM_KEYATTR_RETURN_DATA */)));

	/* make a random IV */
	uint8 ivBytes[8];
	CssmData iv(ivBytes, sizeof(ivBytes));
	random.generate(iv, iv.length());

	/* Extract the key by wrapping it with the wrapping key. */
	CssmClient::WrapKey wrap(csp(), CSSM_ALGID_3DES_3KEY_EDE);
	wrap.key(wrappingKey);
	wrap.cred(getCredentials(CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED, kSecCredentialTypeDefault));
	wrap.mode(CSSM_ALGMODE_ECBPad);
	wrap.padding(CSSM_PADDING_PKCS7);
	wrap.initVector(iv);
	CssmClient::Key wrappedKey(wrap(mKey));

	/* Unwrap the new key into the new Keychain. */
	CssmClient::UnwrapKey unwrap(keychain->csp(), CSSM_ALGID_3DES_3KEY_EDE);
	unwrap.key(wrappingKey);
	unwrap.mode(CSSM_ALGMODE_ECBPad);
	unwrap.padding(CSSM_PADDING_PKCS7);
	unwrap.initVector(iv);

	/* Setup the dldbHandle in the context. */
	unwrap.add(CSSM_ATTRIBUTE_DL_DB_HANDLE, ssDb->handle());

	/* Set up an initial aclEntry so we can change it after the unwrap. */
	Access::Maker maker(Allocator::standard(), Access::Maker::kAnyMakerType);
	ResourceControlContext rcc;
	maker.initialOwner(rcc, NULL);
	unwrap.owner(rcc.input());

	/* Unwrap the key. */
	uint32 usage = mKey->usage();
	/* Work around csp brokeness where it sets all usage bits in the Keyheader when CSSM_KEYUSE_ANY is set. */
	if (usage & CSSM_KEYUSE_ANY)
		usage = CSSM_KEYUSE_ANY;

	CssmClient::Key unwrappedKey(unwrap(wrappedKey, KeySpec(usage,
		(mKey->attributes() | CSSM_KEYATTR_PERMANENT) & ~(CSSM_KEYATTR_ALWAYS_SENSITIVE | CSSM_KEYATTR_NEVER_EXTRACTABLE),
		label)));

	/* Look up unwrapped key in the DLDB. */
	DbUniqueRecord uniqueId;
	SSDbCursor dbCursor(ssDb, 1);
	dbCursor->recordType(recordType());
	dbCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, label);
	CssmClient::Key copiedKey;
	if (!dbCursor->nextKey(NULL, copiedKey, uniqueId))
		MacOSError::throwMe(errSecItemNotFound);

	/* Copy the Label, PrintName and ApplicationTag attributes from the old key to the new one. */
	dbUniqueRecord();
	DbAttributes oldDbAttributes(mUniqueId->database(), 3);
	oldDbAttributes.add(kInfoKeyLabel);
	oldDbAttributes.add(kInfoKeyPrintName);
	oldDbAttributes.add(kInfoKeyApplicationTag);
	mUniqueId->get(&oldDbAttributes, NULL);
	try
	{
		uniqueId->modify(recordType(), &oldDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
	}
	catch (CssmError e)
	{
		// clean up after trying to insert a duplicate key
		uniqueId->deleteRecord ();
		throw;
	}

	/* Set the acl and owner on the unwrapped key. */
	access->setAccess(*unwrappedKey, maker);

	/* Return a keychain item which represents the new key.  */
	Item item(keychain->item(recordType(), uniqueId));

    KCEventNotifier::PostKeychainEvent(kSecAddEvent, keychain, item);

	return item;
}

Item
KeyItem::importTo(const Keychain &keychain, Access *newAccess, SecKeychainAttributeList *attrList)
{
	if (!(keychain->database()->dl()->subserviceMask() & CSSM_SERVICE_CSP))
		MacOSError::throwMe(errSecInvalidKeychain);

	/* Get the destination keychain's db. */
	SSDbImpl* dbImpl = dynamic_cast<SSDbImpl*>(&(*keychain->database()));
	if (dbImpl == NULL)
		CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);

	SSDb ssDb(dbImpl);

	/* Make sure mKey is valid. */
	/* We can't call key() here, since we won't have a unique record id yet */
	if (!mKey)
		CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);

	// Generate a random label to use initially
	CssmClient::CSP appleCsp(gGuidAppleCSP);
	CssmClient::Random random(appleCsp, CSSM_ALGID_APPLE_YARROW);
	uint8 labelBytes[20];
	CssmData label(labelBytes, sizeof(labelBytes));
	random.generate(label, label.Length);

	/* Set up the ACL for the new key. */
	SecPointer<Access> access;
	if (newAccess)
		access = newAccess;
	else
		access = new Access(*mKey);

	/* Generate a random 3DES wrapping Key. */
	CssmClient::GenerateKey genKey(csp(), CSSM_ALGID_3DES_3KEY, 192);
	CssmClient::Key wrappingKey(genKey(KeySpec(CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP,
		CSSM_KEYATTR_EXTRACTABLE /* | CSSM_KEYATTR_RETURN_DATA */)));

	/* make a random IV */
	uint8 ivBytes[8];
	CssmData iv(ivBytes, sizeof(ivBytes));
	random.generate(iv, iv.length());

	/* Extract the key by wrapping it with the wrapping key. */
	CssmClient::WrapKey wrap(csp(), CSSM_ALGID_3DES_3KEY_EDE);
	wrap.key(wrappingKey);
	wrap.cred(getCredentials(CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED, kSecCredentialTypeDefault));
	wrap.mode(CSSM_ALGMODE_ECBPad);
	wrap.padding(CSSM_PADDING_PKCS7);
	wrap.initVector(iv);
	CssmClient::Key wrappedKey(wrap(mKey));

	/* Unwrap the new key into the new Keychain. */
	CssmClient::UnwrapKey unwrap(keychain->csp(), CSSM_ALGID_3DES_3KEY_EDE);
	unwrap.key(wrappingKey);
	unwrap.mode(CSSM_ALGMODE_ECBPad);
	unwrap.padding(CSSM_PADDING_PKCS7);
	unwrap.initVector(iv);

	/* Setup the dldbHandle in the context. */
	unwrap.add(CSSM_ATTRIBUTE_DL_DB_HANDLE, ssDb->handle());

	/* Set up an initial aclEntry so we can change it after the unwrap. */
	Access::Maker maker(Allocator::standard(), Access::Maker::kAnyMakerType);
	ResourceControlContext rcc;
	maker.initialOwner(rcc, NULL);
	unwrap.owner(rcc.input());

	/* Unwrap the key. */
	uint32 usage = mKey->usage();
	/* Work around csp brokeness where it sets all usage bits in the Keyheader when CSSM_KEYUSE_ANY is set. */
	if (usage & CSSM_KEYUSE_ANY)
		usage = CSSM_KEYUSE_ANY;

	CssmClient::Key unwrappedKey(unwrap(wrappedKey, KeySpec(usage,
		(mKey->attributes() | CSSM_KEYATTR_PERMANENT) & ~(CSSM_KEYATTR_ALWAYS_SENSITIVE | CSSM_KEYATTR_NEVER_EXTRACTABLE),
		label)));

	/* Look up unwrapped key in the DLDB. */
	DbUniqueRecord uniqueId;
	SSDbCursor dbCursor(ssDb, 1);
	dbCursor->recordType(recordType());
	dbCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, label);
	CssmClient::Key copiedKey;
	if (!dbCursor->nextKey(NULL, copiedKey, uniqueId))
		MacOSError::throwMe(errSecItemNotFound);

	// Set the initial label, application label, and application tag (if provided)
	if (attrList) {
		DbAttributes newDbAttributes;
		SSDbCursor otherDbCursor(ssDb, 1);
		otherDbCursor->recordType(recordType());
		bool checkForDuplicates = false;

		for (UInt32 index=0; index < attrList->count; index++) {
			SecKeychainAttribute attr = attrList->attr[index];
			CssmData attrData(attr.data, attr.length);
			if (attr.tag == kSecKeyPrintName) {
				newDbAttributes.add(kInfoKeyPrintName, attrData);
			}
			if (attr.tag == kSecKeyLabel) {
				newDbAttributes.add(kInfoKeyLabel, attrData);
				otherDbCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, attrData);
				checkForDuplicates = true;
			}
			if (attr.tag == kSecKeyApplicationTag) {
				newDbAttributes.add(kInfoKeyApplicationTag, attrData);
				otherDbCursor->add(CSSM_DB_EQUAL, kInfoKeyApplicationTag, attrData);
				checkForDuplicates = true;
			}
		}

		DbAttributes otherDbAttributes;
		DbUniqueRecord otherUniqueId;
		CssmClient::Key otherKey;
		try
		{
			if (checkForDuplicates && otherDbCursor->nextKey(&otherDbAttributes, otherKey, otherUniqueId))
				MacOSError::throwMe(errSecDuplicateItem);

			uniqueId->modify(recordType(), &newDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
		}
		catch (CssmError e)
		{
			// clean up after trying to insert a duplicate key
			uniqueId->deleteRecord ();
			throw;
		}
	}

	/* Set the acl and owner on the unwrapped key. */
	access->setAccess(*unwrappedKey, maker);

	/* Return a keychain item which represents the new key.  */
	Item item(keychain->item(recordType(), uniqueId));

    KCEventNotifier::PostKeychainEvent(kSecAddEvent, keychain, item);

	return item;
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
	Security::CssmClient::SSDbUniqueRecordImpl *simpl = dynamic_cast<Security::CssmClient::SSDbUniqueRecordImpl *>(impl);
	if (simpl == NULL)
	{
		CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
	}

	return CssmClient::SSDbUniqueRecord(simpl);
}

CssmClient::Key &
KeyItem::key()
{
	if (!mKey)
	{
		CssmClient::SSDbUniqueRecord uniqueId(ssDbUniqueRecord());
		CssmDataContainer dataBlob(uniqueId->allocator());
		uniqueId->get(NULL, &dataBlob);
		mKey = CssmClient::Key(uniqueId->database()->csp(), *reinterpret_cast<CssmKey *>(dataBlob.Data));
	}

	return mKey;
}

CssmClient::CSP
KeyItem::csp()
{
	return key()->csp();
}


const CSSM_X509_ALGORITHM_IDENTIFIER&
KeyItem::algorithmIdentifier()
{
#if 0
	CssmKey *mKey;
	CSSM_KEY_TYPE algorithm
		CSSM_KEY_PTR cssmKey =  (CSSM_KEY_PTR)thisData->Data;
cssmKey->KeyHeader
	static void printKeyHeader(
	const CSSM_KEYHEADER &hdr)
{
	printf("   Algorithm       : ");
	switch(hdr.AlgorithmId) {
CSSM_X509_ALGORITHM_IDENTIFIER algID;

CSSM_OID *CL_algToOid(
	CSSM_ALGORITHMS algId)
typedef struct cssm_x509_algorithm_identifier {
    CSSM_OID algorithm;
    CSSM_DATA parameters;
} CSSM_X509_ALGORITHM_IDENTIFIER, *CSSM_X509_ALGORITHM_IDENTIFIER_PTR;
#endif

	abort();
}

/*
 * itemID, used to locate Extended Attributes, is the public key hash for keys.
 */
const CssmData &KeyItem::itemID()
{
	if(mPubKeyHash.length() == 0) {
		/*
		 * Fetch the attribute from disk.
		 */
		UInt32 tag = kSecKeyLabel;
		UInt32 format = 0;
		SecKeychainAttributeInfo attrInfo = {1, &tag, &format};
		SecKeychainAttributeList *attrList = NULL;
		getAttributesAndData(&attrInfo, NULL, &attrList, NULL, NULL);
		if((attrList == NULL) || (attrList->count != 1)) {
			MacOSError::throwMe(errSecNoSuchAttr);
		}
		mPubKeyHash.copy(attrList->attr->data, attrList->attr->length);
		freeAttributesAndData(attrList, NULL);
	}
	return mPubKeyHash;
}


unsigned int
KeyItem::strengthInBits(const CSSM_X509_ALGORITHM_IDENTIFIER *algid)
{
	// @@@ Make a context with key based on algid and use that to get the effective keysize and not just the logical one.
	CSSM_KEY_SIZE keySize = {};
	CSSM_RETURN rv = CSSM_QueryKeySizeInBits (csp()->handle(),
                         CSSM_INVALID_HANDLE,
                         key(),
                         &keySize);
	if (rv)
		return 0;

	return keySize.LogicalKeySizeInBits;
}

const AccessCredentials *
KeyItem::getCredentials(
	CSSM_ACL_AUTHORIZATION_TAG operation,
	SecCredentialType credentialType)
{
	// @@@ Fix this to actually examine the ACL for this key and consider operation and do the right thing.
	//AutoAclEntryInfoList aclInfos;
	//key()->getAcl(aclInfos);

	bool smartcard = keychain() != NULL ? (keychain()->database()->dl()->guid() == gGuidAppleSdCSPDL) : false;

	AclFactory factory;
	switch (credentialType)
	{
	case kSecCredentialTypeDefault:
		return smartcard?globals().smartcardItemCredentials():globals().itemCredentials();
	case kSecCredentialTypeWithUI:
		return smartcard?globals().smartcardItemCredentials():factory.promptCred();
	case kSecCredentialTypeNoUI:
		return factory.nullCred();
	default:
		MacOSError::throwMe(paramErr);
	}
}

bool
KeyItem::operator == (KeyItem &other)
{
	if (mKey && *mKey)
	{
		// Pointer compare
		return this == &other;
	}

	// If keychains are different, then keys are different
	Keychain otherKeychain = other.keychain();
	return (mKeychain && otherKeychain && (*mKeychain == *otherKeychain));
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
	SecPointer<Access> initialAccess,
	SecPointer<KeyItem> &outPublicKey,
	SecPointer<KeyItem> &outPrivateKey)
{
	bool freeKeys = false;
	bool deleteContext = false;

	if (!(keychain->database()->dl()->subserviceMask() & CSSM_SERVICE_CSP))
		MacOSError::throwMe(errSecInvalidKeychain);

	SSDbImpl* impl = dynamic_cast<SSDbImpl*>(&(*keychain->database()));
	if (impl == NULL)
	{
		CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
	}

	SSDb ssDb(impl);
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

	Item publicKeyItem, privateKeyItem;
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
		dbPubCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, label);
		CssmClient::Key publicKey;
		if (!dbPubCursor->nextKey(&pubDbAttributes, publicKey, pubUniqueId))
			MacOSError::throwMe(errSecItemNotFound);

		// Look up private key in the DLDB.
		DbAttributes privDbAttributes;
		DbUniqueRecord privUniqueId;
		SSDbCursor dbPrivCursor(ssDb, 1);
		dbPrivCursor->recordType(CSSM_DL_DB_RECORD_PRIVATE_KEY);
		dbPrivCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, label);
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

		auto_ptr<string>privDescription;
		auto_ptr<string>pubDescription;
		try {
			privDescription.reset(new string(initialAccess->promptDescription()));
			pubDescription.reset(new string(initialAccess->promptDescription()));
		}
		catch(...) {
			/* this path taken if no promptDescription available, e.g., for complex ACLs */
			privDescription.reset(new string("Private key"));
			pubDescription.reset(new string("Public key"));
		}

		// Set the label of the public key to the public key hash.
		// Set the PrintName of the public key to the description in the acl.
		pubDbAttributes.add(kInfoKeyLabel, pubKeyHash);
		pubDbAttributes.add(kInfoKeyPrintName, *pubDescription);
		pubUniqueId->modify(CSSM_DL_DB_RECORD_PUBLIC_KEY, &pubDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

		// Set the label of the private key to the public key hash.
		// Set the PrintName of the private key to the description in the acl.
		privDbAttributes.add(kInfoKeyLabel, pubKeyHash);
		privDbAttributes.add(kInfoKeyPrintName, *privDescription);
		privUniqueId->modify(CSSM_DL_DB_RECORD_PRIVATE_KEY, &privDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

		// @@@ Not exception safe!
		csp.allocator().free(cssmData->Data);
		csp.allocator().free(cssmData);

		// Finally fix the acl and owner of the private key to the specified access control settings.
		initialAccess->setAccess(*privateKey, maker);

		if(publicKeyAttr & CSSM_KEYATTR_PUBLIC_KEY_ENCRYPT) {
			/*
			 * Make the public key acl completely open.
			 * If the key was not encrypted, it already has a wide-open
			 * ACL (though that is a feature of securityd; it's not
			 * CDSA-specified behavior).
			 */
			SecPointer<Access> pubKeyAccess(new Access());
			pubKeyAccess->setAccess(*publicKey, maker);
		}

		// Create keychain items which will represent the keys.
		publicKeyItem = keychain->item(CSSM_DL_DB_RECORD_PUBLIC_KEY, pubUniqueId);
		privateKeyItem = keychain->item(CSSM_DL_DB_RECORD_PRIVATE_KEY, privUniqueId);

		KeyItem* impl = dynamic_cast<KeyItem*>(&(*publicKeyItem));
		if (impl == NULL)
		{
			CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
		}

		outPublicKey = impl;

		impl = dynamic_cast<KeyItem*>(&(*privateKeyItem));
		if (impl == NULL)
		{
			CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
		}

		outPrivateKey = impl;
	}
	catch (...)
	{
		if (freeKeys)
		{
			// Delete the keys if something goes wrong so we don't end up with inaccessible keys in the database.
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

	if (keychain && publicKeyItem && privateKeyItem)
	{
		keychain->postEvent(kSecAddEvent, publicKeyItem);
		keychain->postEvent(kSecAddEvent, privateKeyItem);
	}
}

void
KeyItem::importPair(
	Keychain keychain,
	const CSSM_KEY &publicWrappedKey,
	const CSSM_KEY &privateWrappedKey,
	SecPointer<Access> initialAccess,
	SecPointer<KeyItem> &outPublicKey,
	SecPointer<KeyItem> &outPrivateKey)
{
	bool freePublicKey = false;
	bool freePrivateKey = false;
	bool deleteContext = false;

	if (!(keychain->database()->dl()->subserviceMask() & CSSM_SERVICE_CSP))
		MacOSError::throwMe(errSecInvalidKeychain);

	SSDbImpl* impl = dynamic_cast<SSDbImpl *>(&(*keychain->database()));
	if (impl == NULL)
	{
		CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
	}

	SSDb ssDb(impl);
	CssmClient::CSP csp(keychain->csp());
	CssmClient::CSP appleCsp(gGuidAppleCSP);

	// Create a Access::Maker for the initial owner of the private key.
	ResourceControlContext rcc;
	memset(&rcc, 0, sizeof(rcc));
	Access::Maker maker(Allocator::standard(), Access::Maker::kAnyMakerType);
	// @@@ Potentially provide a credential argument which allows us to unwrap keys in the csp.
	// Currently the CSP lets anyone do this, but we might restrict this in the future, e.g.
	// a smartcard could require out of band pin entry before a key can be generated.
	maker.initialOwner(rcc);
	// Create the cred we need to manipulate the keys until we actually set a new access control for them.
	const AccessCredentials *cred = maker.cred();

	CSSM_KEY publicCssmKey, privateCssmKey;
	memset(&publicCssmKey, 0, sizeof(publicCssmKey));
	memset(&privateCssmKey, 0, sizeof(privateCssmKey));

	CSSM_CC_HANDLE ccHandle = 0;

	Item publicKeyItem, privateKeyItem;
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

		status = CSSM_CSP_CreateSymmetricContext(csp->handle(), publicWrappedKey.KeyHeader.WrapAlgorithmId, CSSM_ALGMODE_NONE, NULL, NULL, NULL, CSSM_PADDING_NONE, NULL, &ccHandle);
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
		CSSM_DATA descriptiveData = {0, NULL};

		status = CSSM_UnwrapKey(
			ccHandle,
			NULL,
			&publicWrappedKey,
			publicWrappedKey.KeyHeader.KeyUsage,
			publicWrappedKey.KeyHeader.KeyAttr | CSSM_KEYATTR_PERMANENT,
			&pubKeyHash,
			&rcc,
			&publicCssmKey,
			&descriptiveData);

		if (status)
			CssmError::throwMe(status);
		freePublicKey = true;

		if (descriptiveData.Data != NULL)
			free (descriptiveData.Data);

		status = CSSM_UnwrapKey(
			ccHandle,
			NULL,
			&privateWrappedKey,
			privateWrappedKey.KeyHeader.KeyUsage,
			privateWrappedKey.KeyHeader.KeyAttr | CSSM_KEYATTR_PERMANENT,
			&pubKeyHash,
			&rcc,
			&privateCssmKey,
			&descriptiveData);

		if (status)
			CssmError::throwMe(status);

		if (descriptiveData.Data != NULL)
			free (descriptiveData.Data);

		freePrivateKey = true;

		// Find the keys we just generated in the DL to get SecKeyRefs to them
		// so we can change the label to be the hash of the public key, and
		// fix up other attributes.

		// Look up public key in the DLDB.
		DbAttributes pubDbAttributes;
		DbUniqueRecord pubUniqueId;
		SSDbCursor dbPubCursor(ssDb, 1);
		dbPubCursor->recordType(CSSM_DL_DB_RECORD_PUBLIC_KEY);
		dbPubCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, pubKeyHash);
		CssmClient::Key publicKey;
		if (!dbPubCursor->nextKey(&pubDbAttributes, publicKey, pubUniqueId))
			MacOSError::throwMe(errSecItemNotFound);

		// Look up private key in the DLDB.
		DbAttributes privDbAttributes;
		DbUniqueRecord privUniqueId;
		SSDbCursor dbPrivCursor(ssDb, 1);
		dbPrivCursor->recordType(CSSM_DL_DB_RECORD_PRIVATE_KEY);
		dbPrivCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, pubKeyHash);
		CssmClient::Key privateKey;
		if (!dbPrivCursor->nextKey(&privDbAttributes, privateKey, privUniqueId))
			MacOSError::throwMe(errSecItemNotFound);

		// @@@ Not exception safe!
		csp.allocator().free(cssmData->Data);
		csp.allocator().free(cssmData);

		auto_ptr<string>privDescription;
		auto_ptr<string>pubDescription;
		try {
			privDescription.reset(new string(initialAccess->promptDescription()));
			pubDescription.reset(new string(initialAccess->promptDescription()));
		}
		catch(...) {
			/* this path taken if no promptDescription available, e.g., for complex ACLs */
			privDescription.reset(new string("Private key"));
			pubDescription.reset(new string("Public key"));
		}

		// Set the label of the public key to the public key hash.
		// Set the PrintName of the public key to the description in the acl.
		pubDbAttributes.add(kInfoKeyPrintName, *pubDescription);
		pubUniqueId->modify(CSSM_DL_DB_RECORD_PUBLIC_KEY, &pubDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

		// Set the label of the private key to the public key hash.
		// Set the PrintName of the private key to the description in the acl.
		privDbAttributes.add(kInfoKeyPrintName, *privDescription);
		privUniqueId->modify(CSSM_DL_DB_RECORD_PRIVATE_KEY, &privDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

		// Finally fix the acl and owner of the private key to the specified access control settings.
		initialAccess->setAccess(*privateKey, maker);

		// Make the public key acl completely open
		SecPointer<Access> pubKeyAccess(new Access());
		pubKeyAccess->setAccess(*publicKey, maker);

		// Create keychain items which will represent the keys.
		publicKeyItem = keychain->item(CSSM_DL_DB_RECORD_PUBLIC_KEY, pubUniqueId);
		privateKeyItem = keychain->item(CSSM_DL_DB_RECORD_PRIVATE_KEY, privUniqueId);

		KeyItem* impl = dynamic_cast<KeyItem*>(&(*publicKeyItem));
		if (impl == NULL)
		{
			CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
		}

		outPublicKey = impl;

		impl = dynamic_cast<KeyItem*>(&(*privateKeyItem));
		if (impl == NULL)
		{
			CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
		}
		outPrivateKey = impl;
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

	if (keychain && publicKeyItem && privateKeyItem)
	{
		KCEventNotifier::PostKeychainEvent(kSecAddEvent, keychain, publicKeyItem);
		KCEventNotifier::PostKeychainEvent(kSecAddEvent, keychain, privateKeyItem);
	}
}

SecPointer<KeyItem>
KeyItem::generateWithAttributes(const SecKeychainAttributeList *attrList,
	Keychain keychain,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE keyUsage,
	uint32 keyAttr,
	SecPointer<Access> initialAccess)
{
	CssmClient::CSP appleCsp(gGuidAppleCSP);
	CssmClient::CSP csp(NULL);
	SSDb ssDb(NULL);
	uint8 labelBytes[20];
	CssmData label(labelBytes, sizeof(labelBytes));
	bool freeKey = false;
	bool deleteContext = false;
	const CSSM_DATA *plabel = NULL;

	if (keychain)
	{
		if (!(keychain->database()->dl()->subserviceMask() & CSSM_SERVICE_CSP))
			MacOSError::throwMe(errSecInvalidKeychain);

		SSDbImpl* impl = dynamic_cast<SSDbImpl *>(&(*keychain->database()));
		if (impl == NULL)
		{
			CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
		}

		ssDb = SSDb(impl);
		csp = keychain->csp();

		// Generate a random label to use initially
		CssmClient::Random random(appleCsp, CSSM_ALGID_APPLE_YARROW);
		random.generate(label, label.Length);
		plabel = &label;
	}
	else
	{
		// Not a persistent key so create it in the regular csp
		csp = appleCsp;
	}

	// Create a Access::Maker for the initial owner of the private key.
	ResourceControlContext *prcc = NULL, rcc;
	const AccessCredentials *cred = NULL;
	Access::Maker maker;
	if (keychain && initialAccess)
	{
		memset(&rcc, 0, sizeof(rcc));
		// @@@ Potentially provide a credential argument which allows us to generate keys in the csp.
		// Currently the CSP lets anyone do this, but we might restrict this in the future, e.g. a smartcard
		// could require out-of-band pin entry before a key can be generated.
		maker.initialOwner(rcc);
		// Create the cred we need to manipulate the keys until we actually set a new access control for them.
		cred = maker.cred();
		prcc = &rcc;
	}

	CSSM_KEY cssmKey;

	CSSM_CC_HANDLE ccHandle = 0;

	Item keyItem;
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

		if (ssDb)
		{
			CSSM_DL_DB_HANDLE dldbHandle = ssDb->handle();
			CSSM_DL_DB_HANDLE_PTR dldbHandlePtr = &dldbHandle;
			CSSM_CONTEXT_ATTRIBUTE contextAttributes = { CSSM_ATTRIBUTE_DL_DB_HANDLE, sizeof(dldbHandle), { (char *)dldbHandlePtr } };
			status = CSSM_UpdateContextAttributes(ccHandle, 1, &contextAttributes);
			if (status)
				CssmError::throwMe(status);

			keyAttr |= CSSM_KEYATTR_PERMANENT;
		}

		// Generate the key
		status = CSSM_GenerateKey(ccHandle, keyUsage, keyAttr, plabel, prcc, &cssmKey);
		if (status)
			CssmError::throwMe(status);

		if (ssDb)
		{
			freeKey = true;
			// Find the key we just generated in the DL and get a SecKeyRef
			// so we can specify the label attribute(s) and initial ACL.

			// Look up key in the DLDB.
			DbAttributes dbAttributes;
			DbUniqueRecord uniqueId;
			SSDbCursor dbCursor(ssDb, 1);
			dbCursor->recordType(CSSM_DL_DB_RECORD_SYMMETRIC_KEY);
			dbCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, label);
			CssmClient::Key key;
			if (!dbCursor->nextKey(&dbAttributes, key, uniqueId))
				MacOSError::throwMe(errSecItemNotFound);

			// Set the initial label, application label, and application tag (if provided)
			if (attrList) {
				DbAttributes newDbAttributes;
				SSDbCursor otherDbCursor(ssDb, 1);
				otherDbCursor->recordType(CSSM_DL_DB_RECORD_SYMMETRIC_KEY);
				bool checkForDuplicates = false;

				for (UInt32 index=0; index < attrList->count; index++) {
					SecKeychainAttribute attr = attrList->attr[index];
					CssmData attrData(attr.data, attr.length);
					if (attr.tag == kSecKeyPrintName) {
						newDbAttributes.add(kInfoKeyPrintName, attrData);
					}
					if (attr.tag == kSecKeyLabel) {
						newDbAttributes.add(kInfoKeyLabel, attrData);
						otherDbCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, attrData);
						checkForDuplicates = true;
					}
					if (attr.tag == kSecKeyApplicationTag) {
						newDbAttributes.add(kInfoKeyApplicationTag, attrData);
						otherDbCursor->add(CSSM_DB_EQUAL, kInfoKeyApplicationTag, attrData);
						checkForDuplicates = true;
					}
				}

				DbAttributes otherDbAttributes;
				DbUniqueRecord otherUniqueId;
				CssmClient::Key otherKey;
				if (checkForDuplicates && otherDbCursor->nextKey(&otherDbAttributes, otherKey, otherUniqueId))
					MacOSError::throwMe(errSecDuplicateItem);

				uniqueId->modify(CSSM_DL_DB_RECORD_SYMMETRIC_KEY, &newDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
			}

			// Finally, fix the acl and owner of the key to the specified access control settings.
			if (initialAccess)
				initialAccess->setAccess(*key, maker);

			// Create keychain item which will represent the key.
			keyItem = keychain->item(CSSM_DL_DB_RECORD_SYMMETRIC_KEY, uniqueId);
		}
		else
		{
			CssmClient::Key tempKey(csp, cssmKey);
			keyItem = new KeyItem(tempKey);
		}
	}
	catch (...)
	{
		if (freeKey)
		{
			// Delete the key if something goes wrong so we don't end up with inaccessible keys in the database.
			CSSM_FreeKey(csp->handle(), cred, &cssmKey, TRUE);
		}

		if (deleteContext)
			CSSM_DeleteContext(ccHandle);

		throw;
	}

	if (freeKey)
	{
		CSSM_FreeKey(csp->handle(), NULL, &cssmKey, FALSE);
	}

	if (deleteContext)
		CSSM_DeleteContext(ccHandle);

	if (keychain && keyItem)
		keychain->postEvent(kSecAddEvent, keyItem);

	KeyItem* item = dynamic_cast<KeyItem*>(&*keyItem);
	if (item == NULL)
	{
		CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
	}

	return item;
}

SecPointer<KeyItem>
KeyItem::generate(Keychain keychain,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE keyUsage,
	uint32 keyAttr,
	SecPointer<Access> initialAccess)
{
	return KeyItem::generateWithAttributes(NULL, keychain,
		algorithm, keySizeInBits, contextHandle,
		keyUsage, keyAttr, initialAccess);
}


void KeyItem::RawSign(SecPadding padding, CSSM_DATA dataToSign, const AccessCredentials *credentials, CSSM_DATA& signature)
{
	CSSM_ALGORITHMS baseAlg = key()->header().algorithm();
	if ((baseAlg != CSSM_ALGID_RSA) && (baseAlg != CSSM_ALGID_ECDSA))
	{
		MacOSError::throwMe(paramErr);
	}

	CSSM_ALGORITHMS paddingAlg = CSSM_PADDING_PKCS1;

	switch (padding)
	{
		case kSecPaddingPKCS1:
		{
			paddingAlg = CSSM_PADDING_PKCS1;
			break;
		}

		case kSecPaddingPKCS1MD2:
		{
			baseAlg = CSSM_ALGID_MD2WithRSA;
			break;
		}

		case kSecPaddingPKCS1MD5:
		{
			baseAlg = CSSM_ALGID_MD5WithRSA;
			break;
		}

		case kSecPaddingPKCS1SHA1:
		{
			baseAlg = CSSM_ALGID_SHA1WithRSA;
			break;
		}

		default:
		{
			paddingAlg = CSSM_PADDING_NONE;
			break;
		}
	}

	Sign signContext(csp(), baseAlg);
	signContext.key(key());
	signContext.set(CSSM_ATTRIBUTE_PADDING, paddingAlg);
	signContext.cred(credentials);

	CssmData data(dataToSign.Data, dataToSign.Length);
	signContext.sign(data);

    CssmData sig(signature.Data, signature.Length);
	signContext(sig); // yes, this is an accessor.  Believe it, or not.
    signature.Length = sig.length();
}



void KeyItem::RawVerify(SecPadding padding, CSSM_DATA dataToVerify, const AccessCredentials *credentials, CSSM_DATA sig)
{
	CSSM_ALGORITHMS baseAlg = key()->header().algorithm();
	if ((baseAlg != CSSM_ALGID_RSA) && (baseAlg != CSSM_ALGID_ECDSA))
	{
		MacOSError::throwMe(paramErr);
	}

	CSSM_ALGORITHMS paddingAlg = CSSM_PADDING_PKCS1;

	switch (padding)
	{
		case kSecPaddingPKCS1:
		{
			paddingAlg = CSSM_PADDING_PKCS1;
			break;
		}

		case kSecPaddingPKCS1MD2:
		{
			baseAlg = CSSM_ALGID_MD2WithRSA;
			break;
		}

		case kSecPaddingPKCS1MD5:
		{
			baseAlg = CSSM_ALGID_MD5WithRSA;
			break;
		}

		case kSecPaddingPKCS1SHA1:
		{
			baseAlg = CSSM_ALGID_SHA1WithRSA;
			break;
		}

		default:
		{
			paddingAlg = CSSM_PADDING_NONE;
			break;
		}
	}

	Verify verifyContext(csp(), baseAlg);
	verifyContext.key(key());
	verifyContext.set(CSSM_ATTRIBUTE_PADDING, paddingAlg);
	verifyContext.cred(credentials);

	CssmData data(dataToVerify.Data, dataToVerify.Length);
	CssmData signature(sig.Data, sig.Length);
	verifyContext.verify(data, signature);
}



void KeyItem::Encrypt(SecPadding padding, CSSM_DATA dataToEncrypt, const AccessCredentials *credentials, CSSM_DATA& encryptedData)
{
	CSSM_ALGORITHMS baseAlg = key()->header().algorithm();
	if (baseAlg != CSSM_ALGID_RSA)
	{
		MacOSError::throwMe(paramErr);
	}

	CSSM_ALGORITHMS paddingAlg = CSSM_PADDING_PKCS1;

	switch (padding)
	{
		case kSecPaddingPKCS1:
		{
			paddingAlg = CSSM_PADDING_PKCS1;
			break;
		}

		default:
		{
			paddingAlg = CSSM_PADDING_NONE;
			break;
		}
	}

	CssmClient::Encrypt encryptContext(csp(), baseAlg);
	encryptContext.key(key());
	encryptContext.padding(paddingAlg);
	encryptContext.cred(credentials);

	CssmData inData(dataToEncrypt.Data, dataToEncrypt.Length);
	CssmData outData(encryptedData.Data, encryptedData.Length);
	CssmData remData((void*) NULL, 0);

	encryptedData.Length = encryptContext.encrypt(inData, outData, remData);
}



void KeyItem::Decrypt(SecPadding padding, CSSM_DATA dataToDecrypt, const AccessCredentials *credentials, CSSM_DATA& decryptedData)
{
	CSSM_ALGORITHMS baseAlg = key()->header().algorithm();
	if (baseAlg != CSSM_ALGID_RSA)
	{
		MacOSError::throwMe(paramErr);
	}

	CSSM_ALGORITHMS paddingAlg = CSSM_PADDING_PKCS1;

	switch (padding)
	{
		case kSecPaddingPKCS1:
		{
			paddingAlg = CSSM_PADDING_PKCS1;
			break;
		}


		default:
		{
			paddingAlg = CSSM_PADDING_NONE;
			break;
		}
	}

	CssmClient::Decrypt decryptContext(csp(), baseAlg);
	decryptContext.key(key());
	decryptContext.padding(paddingAlg);
	decryptContext.cred(credentials);

	CssmData inData(dataToDecrypt.Data, dataToDecrypt.Length);
	CssmData outData(decryptedData.Data, decryptedData.Length);
	CssmData remData((void*) NULL, 0);
	decryptedData.Length = decryptContext.decrypt(inData, outData, remData);
    if (remData.Data != NULL)
    {
        free(remData.Data);
    }
}

