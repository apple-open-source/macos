/*
 * Copyright (c) 2002-2004,2011-2014 Apple Inc. All Rights Reserved.
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
#include <Security/cssmtype.h>
#include <security_keychain/Access.h>
#include <security_keychain/Keychains.h>
#include <security_keychain/KeyItem.h>
#include <security_cdsa_client/wrapkey.h>
#include <security_cdsa_client/genkey.h>
#include <security_cdsa_client/signclient.h>
#include <security_cdsa_client/cryptoclient.h>
#include <security_utilities/CSPDLTransaction.h>

#include <security_keychain/Globals.h>
#include "KCEventNotifier.h"
#include <CommonCrypto/CommonDigest.h>
#include <SecBase.h>
#include <SecBasePriv.h>
#include <CoreFoundation/CFPriv.h>

// @@@ This needs to be shared.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"
static CSSM_DB_NAME_ATTR(kInfoKeyPrintName, kSecKeyPrintName, (char*) "PrintName", 0, NULL, BLOB);
static CSSM_DB_NAME_ATTR(kInfoKeyLabel, kSecKeyLabel, (char*) "Label", 0, NULL, BLOB);
static CSSM_DB_NAME_ATTR(kInfoKeyApplicationTag, kSecKeyApplicationTag, (char*) "ApplicationTag", 0, NULL, BLOB);
#pragma clang diagnostic pop

using namespace KeychainCore;
using namespace CssmClient;

KeyItem *KeyItem::required(SecKeyRef ptr)
{
    if (KeyItem *p = optional(ptr)) {
        return p;
    } else {
        MacOSError::throwMe(errSecInvalidItemRef);
    }
}

KeyItem *KeyItem::optional(SecKeyRef ptr)
{
    if (ptr != NULL) {
        if (KeyItem *pp = dynamic_cast<KeyItem *>(fromSecKeyRef(ptr))) {
            return pp;
        } else {
            MacOSError::throwMe(errSecInvalidItemRef);
        }
    } else {
        return NULL;
    }
}

KeyItem::operator CFTypeRef() const throw()
{
    StMaybeLock<Mutex> _(this->getMutexForObject());

    if (mWeakSecKeyRef != NULL) {
        if (_CFTryRetain(mWeakSecKeyRef) == NULL) {
            // mWeakSecKeyRef is not really valid, pointing to SecKeyRef which going to die - it is somewhere between last CFRelease and entering into mutex-protected section of SecCDSAKeyDestroy.  Avoid using it, pretend that no enveloping SecKeyRef exists.  But make sure that this KeyImpl is disconnected from this about-to-die SecKeyRef, because we do not want KeyImpl connected to it to be really destroyed, it will be connected to newly created SecKeyRef (see below).
            mWeakSecKeyRef->key = NULL;
            mWeakSecKeyRef = NULL;
        } else {
            // We did not really want to retain, it was just weak->strong promotion test.
            CFRelease(mWeakSecKeyRef);
        }
    }

    if (mWeakSecKeyRef == NULL) {
        // Create enveloping ref on-demand.  Transfer reference count from SecCFObject
        // to newly created SecKeyRef wrapper.
        attachSecKeyRef();
    }
    return mWeakSecKeyRef;
}

void KeyItem::initializeWithSecKeyRef(SecKeyRef ref)
{
    isNew();
    mWeakSecKeyRef = ref;
}


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
    ItemImpl((SecItemClass) (key->keyClass() + CSSM_DL_DB_RECORD_PUBLIC_KEY), (OSType)0, (UInt32)0, (const void*)NULL),
	mKey(key),
	algid(NULL),
	mPubKeyHash(Allocator::standard())
{
	if (key->keyClass() > CSSM_KEYCLASS_SESSION_KEY)
		MacOSError::throwMe(errSecParam);
}

KeyItem::~KeyItem()
{
}

void
KeyItem::update()
{
    //Create a new CSPDLTransaction
    Db db(mKeychain->database());
    CSPDLTransaction transaction(db);

    ItemImpl::update();

    /* Update integrity on key */
    setIntegrity();

    transaction.commit();
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
	const CSSM_KEY *cssmKey = key();
	if (cssmKey && (0==(cssmKey->KeyHeader.KeyAttr & CSSM_KEYATTR_EXTRACTABLE)))
	{
		MacOSError::throwMe(errSecDataNotAvailable);
	}

	// Generate a random label to use initially
	CssmClient::CSP appleCsp(gGuidAppleCSP);
	CssmClient::Random random(appleCsp, CSSM_ALGID_APPLE_YARROW);
	uint8 labelBytes[20];
	CssmData label(labelBytes, sizeof(labelBytes));
	random.generate(label, (uint32)label.Length);

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
	random.generate(iv, (uint32)iv.length());

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

    /* Set the acl and owner on the unwrapped key. See note in ItemImpl::copyTo about removing rights. */
    access->removeAclsForRight(CSSM_ACL_AUTHORIZATION_PARTITION_ID);
    access->removeAclsForRight(CSSM_ACL_AUTHORIZATION_INTEGRITY);
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
	random.generate(label, (uint32)label.Length);

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
	random.generate(iv, (uint32)iv.length());

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

		for (UInt32 index=0; index < attrList->count; index++) {
			SecKeychainAttribute attr = attrList->attr[index];
			CssmData attrData(attr.data, attr.length);
			if (attr.tag == kSecKeyPrintName) {
				newDbAttributes.add(kInfoKeyPrintName, attrData);
			}
			if (attr.tag == kSecKeyLabel) {
				newDbAttributes.add(kInfoKeyLabel, attrData);
			}
			if (attr.tag == kSecKeyApplicationTag) {
				newDbAttributes.add(kInfoKeyApplicationTag, attrData);
			}
		}

        modifyUniqueId(keychain, ssDb, uniqueId, newDbAttributes, recordType());
	}

	/* Set the acl and owner on the unwrapped key. */
    addIntegrity(*access);
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
	MacOSError::throwMe(errSecUnimplemented);
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

CssmKey::Header
KeyItem::unverifiedKeyHeader() {
    return unverifiedKey()->header();
}

CssmClient::Key
KeyItem::unverifiedKey()
{
	StLock<Mutex>_(mMutex);
	if (!mKey)
	{
		CssmClient::SSDbUniqueRecord uniqueId(ssDbUniqueRecord());
		CssmDataContainer dataBlob(uniqueId->allocator());
		uniqueId->get(NULL, &dataBlob);
		return CssmClient::Key(uniqueId->database()->csp(), *reinterpret_cast<CssmKey *>(dataBlob.Data));
	}

	return mKey;
}

CssmClient::Key &
KeyItem::key()
{
    StLock<Mutex>_(mMutex);
    if (!mKey)
    {
        mKey = unverifiedKey();

        try {
            if(!ItemImpl::checkIntegrity(*mKey)) {
                secnotice("integrity", "key has no integrity, denying access");
                mKey.release();
                CssmError::throwMe(errSecInvalidItemRef);
            }
        } catch(CssmError cssme) {
            mKey.release();
            secnotice("integrity", "error while checking integrity, denying access: %s", cssme.what());
            throw cssme;
        }
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
		MacOSError::throwMe(errSecParam);
	}
}

CssmClient::Key
KeyItem::publicKey() {
    return mPublicKey;
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
    SSDb ssDb(NULL);
    Access::Maker maker;
    const AccessCredentials *cred = NULL;
    CssmClient::CSP appleCsp(gGuidAppleCSP);
    CssmClient::CSP csp = appleCsp;
    ResourceControlContext rcc;
    memset(&rcc, 0, sizeof(rcc));
    CssmData label;
    uint8 labelBytes[20];

    if (keychain) {
        if (!(keychain->database()->dl()->subserviceMask() & CSSM_SERVICE_CSP))
            MacOSError::throwMe(errSecInvalidKeychain);

        SSDbImpl* impl = dynamic_cast<SSDbImpl*>(&(*keychain->database()));
        if (impl == NULL)
            CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);

        ssDb = SSDb(impl);
        csp = CssmClient::CSP(keychain->csp());

        // Generate a random label to use initially
        CssmClient::Random random(appleCsp, CSSM_ALGID_APPLE_YARROW);
        label = CssmData(labelBytes, sizeof(labelBytes));
        random.generate(label, (uint32)label.length());

        // Create a Access::Maker for the initial owner of the private key.
        // @@@ Potentially provide a credential argument which allows us to generate keys in the csp.  Currently the CSP let's anyone do this, but we might restrict this in the future, f.e. a smartcard could require out of band pin entry before a key can be generated.
        maker.initialOwner(rcc);
        // Create the cred we need to manipulate the keys until we actually set a new access control for them.
        cred = maker.cred();
    }

    CssmKey publicCssmKey, privateCssmKey;
	CSSM_CC_HANDLE ccHandle = 0;

    bool freePublicKey = false;
    bool freePrivateKey = false;
    bool deleteContext = false;
    bool permanentPubKey = false;
    bool permanentPrivKey = false;

	SecPointer<KeyItem> publicKeyItem, privateKeyItem;
	try {
		CSSM_RETURN status;
        if (contextHandle) {
            ccHandle = contextHandle;
        } else {
			status = CSSM_CSP_CreateKeyGenContext(csp->handle(), algorithm, keySizeInBits, NULL, NULL, NULL, NULL, NULL, &ccHandle);
			if (status)
				CssmError::throwMe(status);
			deleteContext = true;
		}

        if (ssDb) {
            CSSM_DL_DB_HANDLE dldbHandle = ssDb->handle();
            CSSM_DL_DB_HANDLE_PTR dldbHandlePtr = &dldbHandle;
            CSSM_CONTEXT_ATTRIBUTE contextAttributes = { CSSM_ATTRIBUTE_DL_DB_HANDLE, sizeof(dldbHandle), { (char *)dldbHandlePtr } };
            status = CSSM_UpdateContextAttributes(ccHandle, 1, &contextAttributes);
            if (status)
                CssmError::throwMe(status);
        }

		// Generate the keypair
		status = CSSM_GenerateKeyPair(ccHandle, publicKeyUsage, publicKeyAttr, &label, &publicCssmKey, privateKeyUsage, privateKeyAttr, &label, &rcc, &privateCssmKey);
		if (status)
			CssmError::throwMe(status);
        if ((publicKeyAttr & CSSM_KEYATTR_PERMANENT) != 0) {
            permanentPubKey = true;
            freePublicKey = true;
        }
        if ((privateKeyAttr & CSSM_KEYATTR_PERMANENT) != 0) {
            permanentPrivKey = true;
            freePrivateKey = true;
        }

		// Find the keys if we just generated them in the DL so we can change the label to be the hash of the public key, and
		// fix up other attributes.

		// Look up public key in the DLDB.
        CssmClient::Key publicKey;
        DbAttributes pubDbAttributes;
        DbUniqueRecord pubUniqueId;
        if (permanentPubKey) {
            SSDbCursor dbPubCursor(ssDb, 1);
            dbPubCursor->recordType(CSSM_DL_DB_RECORD_PUBLIC_KEY);
            dbPubCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, label);
            if (!dbPubCursor->nextKey(&pubDbAttributes, publicKey, pubUniqueId))
                MacOSError::throwMe(errSecItemNotFound);
        } else {
            publicKey = CssmClient::Key(appleCsp, publicCssmKey);
            outPublicKey = new KeyItem(publicKey);
            freePublicKey = false;
        }

		// Look up private key in the DLDB.
        CssmClient::Key privateKey;
        DbAttributes privDbAttributes;
        DbUniqueRecord privUniqueId;
        if (permanentPrivKey) {
            SSDbCursor dbPrivCursor(ssDb, 1);
            dbPrivCursor->recordType(CSSM_DL_DB_RECORD_PRIVATE_KEY);
            dbPrivCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, label);
            if (!dbPrivCursor->nextKey(&privDbAttributes, privateKey, privUniqueId))
                MacOSError::throwMe(errSecItemNotFound);
        } else {
            privateKey = CssmClient::Key(appleCsp, privateCssmKey);
            outPrivateKey = new KeyItem(privateKey);
            freePrivateKey = false;
        }

        if (ssDb) {
            // Convert reference public key to a raw key so we can use it in the appleCsp.
            CssmClient::WrapKey wrap(csp, CSSM_ALGID_NONE);
            wrap.cred(cred);
            CssmClient::Key rawPubKey = wrap(publicKey);

            // Calculate the hash of the public key using the appleCSP.
            CssmClient::PassThrough passThrough(appleCsp);

            /* Given a CSSM_KEY_PTR in any format, obtain the SHA-1 hash of the
             * associated key blob.
             * Key is specified in CSSM_CSP_CreatePassThroughContext.
             * Hash is allocated by the CSP, in the App's memory, and returned
             * in *outData. */
            passThrough.key(rawPubKey);
            CssmData *pubKeyHashData;
            passThrough(CSSM_APPLECSP_KEYDIGEST, (const void *)NULL, &pubKeyHashData);
            CssmAutoData pubKeyHash(passThrough.allocator());
            pubKeyHash.set(*pubKeyHashData);
            passThrough.allocator().free(pubKeyHashData);

            auto_ptr<string>privDescription;
            auto_ptr<string>pubDescription;
            try {
                privDescription.reset(new string(initialAccess->promptDescription()));
                pubDescription.reset(new string(initialAccess->promptDescription()));
            }
            catch (...) {
                /* this path taken if no promptDescription available, e.g., for complex ACLs */
                privDescription.reset(new string("Private key"));
                pubDescription.reset(new string("Public key"));
            }

            if (permanentPubKey) {
                // Set the label of the public key to the public key hash.
                // Set the PrintName of the public key to the description in the acl.
                pubDbAttributes.add(kInfoKeyLabel, pubKeyHash.get());
                pubDbAttributes.add(kInfoKeyPrintName, *pubDescription);
                modifyUniqueId(keychain, ssDb, pubUniqueId, pubDbAttributes, CSSM_DL_DB_RECORD_PUBLIC_KEY);

                // Create keychain item which will represent the public key.
                publicKeyItem = dynamic_cast<KeyItem*>(keychain->item(CSSM_DL_DB_RECORD_PUBLIC_KEY, pubUniqueId).get());
                if (!publicKeyItem) {
                    CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
                }

                if (publicKeyAttr & CSSM_KEYATTR_PUBLIC_KEY_ENCRYPT) {
                    /*
                     * Make the public key acl completely open.
                     * If the key was not encrypted, it already has a wide-open
                     * ACL (though that is a feature of securityd; it's not
                     * CDSA-specified behavior).
                     */
                    SecPointer<Access> pubKeyAccess(new Access());
                    publicKeyItem->addIntegrity(*pubKeyAccess);
                    pubKeyAccess->setAccess(*publicKey, maker);
                }
                outPublicKey = publicKeyItem;
            }

            if (permanentPrivKey) {
                // Set the label of the private key to the public key hash.
                // Set the PrintName of the private key to the description in the acl.
                privDbAttributes.add(kInfoKeyLabel, pubKeyHash.get());
                privDbAttributes.add(kInfoKeyPrintName, *privDescription);
                modifyUniqueId(keychain, ssDb, privUniqueId, privDbAttributes, CSSM_DL_DB_RECORD_PRIVATE_KEY);

                // Create keychain item which will represent the private key.
                privateKeyItem = dynamic_cast<KeyItem*>(keychain->item(CSSM_DL_DB_RECORD_PRIVATE_KEY, privUniqueId).get());
                if (!privateKeyItem) {
                    CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
                }

                // Finally fix the acl and owner of the private key to the specified access control settings.
                privateKeyItem->addIntegrity(*initialAccess);
                initialAccess->setAccess(*privateKey, maker);
                outPrivateKey = privateKeyItem;
            }
        }
        outPrivateKey->mPublicKey = publicKey;
	}
	catch (...)
	{
        // Delete the keys if something goes wrong so we don't end up with inaccessible keys in the database.
		if (freePublicKey) {
			CSSM_FreeKey(csp->handle(), cred, &publicCssmKey, permanentPubKey);
        }
        if (freePrivateKey) {
			CSSM_FreeKey(csp->handle(), cred, &privateCssmKey, permanentPrivKey);
		}

		if (deleteContext)
			CSSM_DeleteContext(ccHandle);

		throw;
	}

	if (freePublicKey) {
		CSSM_FreeKey(csp->handle(), NULL, &publicCssmKey, FALSE);
    }
    if (freePrivateKey) {
		CSSM_FreeKey(csp->handle(), NULL, &privateCssmKey, FALSE);
	}

	if (deleteContext)
		CSSM_DeleteContext(ccHandle);

	if (keychain) {
        if (permanentPubKey) {
            keychain->postEvent(kSecAddEvent, publicKeyItem);
        }
        if (permanentPrivKey) {
            keychain->postEvent(kSecAddEvent, privateKeyItem);
        }
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

	SecPointer<KeyItem> publicKeyItem, privateKeyItem;
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
        modifyUniqueId(keychain, ssDb, pubUniqueId, pubDbAttributes, CSSM_DL_DB_RECORD_PUBLIC_KEY);

		// Set the label of the private key to the public key hash.
		// Set the PrintName of the private key to the description in the acl.
		privDbAttributes.add(kInfoKeyPrintName, *privDescription);
        modifyUniqueId(keychain, ssDb, privUniqueId, privDbAttributes, CSSM_DL_DB_RECORD_PRIVATE_KEY);

        // Create keychain items which will represent the keys.
        publicKeyItem = dynamic_cast<KeyItem*>(keychain->item(CSSM_DL_DB_RECORD_PUBLIC_KEY, pubUniqueId).get());
        privateKeyItem = dynamic_cast<KeyItem*>(keychain->item(CSSM_DL_DB_RECORD_PRIVATE_KEY, privUniqueId).get());

        if (!publicKeyItem || !privateKeyItem)
        {
            CssmError::throwMe(CSSMERR_CSSM_INVALID_POINTER);
        }

        // Finally fix the acl and owner of the private key to the specified access control settings.
        privateKeyItem->addIntegrity(*initialAccess);
		initialAccess->setAccess(*privateKey, maker);

		// Make the public key acl completely open
		SecPointer<Access> pubKeyAccess(new Access());
        publicKeyItem->addIntegrity(*pubKeyAccess);
		pubKeyAccess->setAccess(*publicKey, maker);

        outPublicKey = publicKeyItem;
        outPrivateKey = privateKeyItem;
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
		KCEventNotifier::PostKeychainEvent(kSecAddEvent, keychain, Item(publicKeyItem));
		KCEventNotifier::PostKeychainEvent(kSecAddEvent, keychain, Item(privateKeyItem));
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
		random.generate(label, (uint32)label.Length);
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



	if (keychain)
	{
		memset(&rcc, 0, sizeof(rcc));
		// @@@ Potentially provide a credential argument which allows us to generate keys in the csp.
		// Currently the CSP lets anyone do this, but we might restrict this in the future, e.g. a smartcard
		// could require out-of-band pin entry before a key can be generated.
		maker.initialOwner(rcc);
		// Create the cred we need to manipulate the keys until we actually set a new access control for them.
		cred = maker.cred();
		prcc = &rcc;

        if (!initialAccess) {
            // We don't have an access, but we need to set integrity. Make an Access.
            initialAccess = new Access(string(label));
        }
    }

	CSSM_KEY cssmKey;

	CSSM_CC_HANDLE ccHandle = 0;

	SecPointer<KeyItem> keyItem;
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

				for (UInt32 index=0; index < attrList->count; index++) {
					SecKeychainAttribute attr = attrList->attr[index];
					CssmData attrData(attr.data, attr.length);
					if (attr.tag == kSecKeyPrintName) {
						newDbAttributes.add(kInfoKeyPrintName, attrData);
					}
					if (attr.tag == kSecKeyLabel) {
						newDbAttributes.add(kInfoKeyLabel, attrData);
					}
					if (attr.tag == kSecKeyApplicationTag) {
						newDbAttributes.add(kInfoKeyApplicationTag, attrData);
					}
				}

                modifyUniqueId(keychain, ssDb, uniqueId, newDbAttributes, CSSM_DL_DB_RECORD_SYMMETRIC_KEY);
            }

            // Create keychain item which will represent the key.
            keyItem = dynamic_cast<KeyItem *>(keychain->item(CSSM_DL_DB_RECORD_SYMMETRIC_KEY, uniqueId).get());

            // Finally, fix the acl and owner of the key to the specified access control settings.
            keyItem->addIntegrity(*initialAccess);
            initialAccess->setAccess(*key, maker);
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


CFHashCode KeyItem::hash()
{
	CFHashCode result = 0;
	const CSSM_KEY *cssmKey = key();
	if (NULL != cssmKey)
	{
		unsigned char digest[CC_SHA256_DIGEST_LENGTH];
		
		CFIndex size_of_data = sizeof(CSSM_KEYHEADER) +  cssmKey->KeyData.Length;
		
		CFMutableDataRef temp_cfdata = CFDataCreateMutable(kCFAllocatorDefault, size_of_data);
		if (NULL == temp_cfdata)
		{
			return result;
		}
		
		CFDataAppendBytes(temp_cfdata, (const UInt8 *)cssmKey, sizeof(CSSM_KEYHEADER));
		CFDataAppendBytes(temp_cfdata, cssmKey->KeyData.Data, cssmKey->KeyData.Length);

		if (size_of_data < 80)
		{
			// If it is less than 80 bytes then CFData can be used
			result = CFHash(temp_cfdata);
			CFRelease(temp_cfdata);
		}
		// CFData truncates its hash value to 80 bytes. ????
		// In order to do the 'right thing' a SHA 256 hash will be used to
		// include all of the data
		else
		{
			memset(digest, 0, CC_SHA256_DIGEST_LENGTH);

			CC_SHA256((const void *)CFDataGetBytePtr(temp_cfdata), (CC_LONG)CFDataGetLength(temp_cfdata), digest);

			CFDataRef data_to_hash = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
				(const UInt8 *)digest, CC_SHA256_DIGEST_LENGTH, kCFAllocatorNull);
			result = CFHash(data_to_hash);
			CFRelease(data_to_hash);
			CFRelease(temp_cfdata);
		}
	}
	return result;
}

void KeyItem::setIntegrity(bool force) {
    ItemImpl::setIntegrity(*unverifiedKey(), force);
}

bool KeyItem::checkIntegrity() {
    if(!isPersistent()) {
        return true;
    }

    try {
        // key() checks integrity of itself, and throws if there's a problem.
        key();
        return true;
    } catch (CssmError cssme) {
        return false;
    }
}

 void KeyItem::removeIntegrity(const AccessCredentials *cred) {
    ItemImpl::removeIntegrity(*key(), cred);
 }

// KeyItems are a little bit special: the only modifications you can do to them
// is to change their Print Name, Label, or Application Tag.
//
// When we do this modification, we need to look ahead to see if there's an item
// that's already there. If there are, we're going to throw a errSecDuplicateItem.
//
// Unless that item doesn't pass the integrity check, in which case we delete it
// and continue with the add.
void KeyItem::modifyUniqueId(Keychain keychain, SSDb ssDb, DbUniqueRecord& uniqueId, DbAttributes& newDbAttributes, CSSM_DB_RECORDTYPE recordType) {
    SSDbCursor otherDbCursor(ssDb, 1);
    otherDbCursor->recordType(recordType);

    bool checkForDuplicates = false;
    // Set up the ssdb cursor
    CssmDbAttributeData* label = newDbAttributes.findAttribute(kInfoKeyLabel);
    if(label) {
        otherDbCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, label->at(0));
        checkForDuplicates = true;
    }
    CssmDbAttributeData* apptag = newDbAttributes.findAttribute(kInfoKeyApplicationTag);
    if(apptag) {
        otherDbCursor->add(CSSM_DB_EQUAL, kInfoKeyApplicationTag, apptag->at(0));
        checkForDuplicates = true;
    }

    // KeyItems only have integrity if the keychain supports it; otherwise,
    // don't pre-check for duplicates
    if((!keychain) || !keychain->hasIntegrityProtection()) {
        secnotice("integrity", "key skipping duplicate integrity check due to keychain version");
        checkForDuplicates = false;
    }

    if (checkForDuplicates) {
        secnotice("integrity", "looking for duplicates");
        // If there are duplicates that are invalid, delete it and
        // continue. Otherwise, if there are duplicates, throw errSecDuplicateItem.
        DbAttributes otherDbAttributes;
        DbUniqueRecord otherUniqueId;
        CssmClient::Key otherKey;

        while(otherDbCursor->nextKey(&otherDbAttributes, otherKey, otherUniqueId)) {
            secnotice("integrity", "found a duplicate, checking integrity");

            PrimaryKey pk = keychain->makePrimaryKey(recordType, otherUniqueId);

            ItemImpl* maybeItem = keychain->_lookupItem(pk);
            if(maybeItem) {
                if(maybeItem->checkIntegrity()) {
                    secnotice("integrity", "duplicate is real, throwing error");
                    MacOSError::throwMe(errSecDuplicateItem);
                } else {
                    secnotice("integrity", "existing duplicate item is invalid, removing...");
                    Item item(maybeItem);
                    keychain->deleteItem(item);
                }
            } else {
                KeyItem temp(keychain, pk, otherUniqueId);

                if(temp.checkIntegrity()) {
                    secnotice("integrity", "duplicate is real, throwing error");
                    MacOSError::throwMe(errSecDuplicateItem);
                } else {
                    secnotice("integrity", "duplicate is invalid, removing");
                    // Keychain's idea of deleting items involves notifications and callbacks. We don't want that,
                    // (since this isn't a real item and it should go away quietly), so use this roundabout method.
                    otherUniqueId->deleteRecord();
                    keychain->removeItem(temp.primaryKey(), &temp);
                }
            }
        }
    }

    try {
        secnotice("integrity", "modifying unique id");
        uniqueId->modify(recordType, &newDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
        secnotice("integrity", "done modifying unique id");
    } catch(CssmError e) {
        // Just in case something went wrong, clean up after this add
        uniqueId->deleteRecord();
        throw;
    }
}
