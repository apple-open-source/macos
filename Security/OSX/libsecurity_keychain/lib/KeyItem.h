/*
 * Copyright (c) 2002-2011,2013 Apple Inc. All Rights Reserved.
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
// KeyItem.h
//
#ifndef _SECURITY_KEYITEM_H_
#define _SECURITY_KEYITEM_H_

#include <security_keychain/Item.h>
#include <Security/SecKeyPriv.h>

namespace Security
{

namespace KeychainCore
{

class KeyItem : public ItemImpl
{
	NOCOPY(KeyItem)
public:
	SECCFFUNCTIONS_BASE(KeyItem, SecKeyRef)

    // SecKeyRef is now provided by iOS implementation, so we have to hack standard accessors normally defined by
    // SECCFUNCTIONS macro to retarget SecKeyRef to foreign object instead of normal way through SecCFObject.
    static KeyItem *required(SecKeyRef ptr);
    static KeyItem *optional(SecKeyRef ptr);
    operator CFTypeRef() const throw();
    static SecCFObject *fromSecKeyRef(CFTypeRef ref);
    void attachSecKeyRef() const;
    void initializeWithSecKeyRef(SecKeyRef ref);

private:
    // This weak backpointer to owning SecKeyRef instance (which is created by iOS SecKey code).
    mutable SecKeyRef mWeakSecKeyRef;

	// db item constructor
private:
    KeyItem(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);

	// PrimaryKey item constructor
    KeyItem(const Keychain &keychain, const PrimaryKey &primaryKey);

public:
	static KeyItem* make(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);
	static KeyItem* make(const Keychain &keychain, const PrimaryKey &primaryKey);
	
	KeyItem(KeyItem &keyItem);

	KeyItem(const CssmClient::Key &key);

    virtual ~KeyItem();

	virtual void update();
	virtual Item copyTo(const Keychain &keychain, Access *newAccess = NULL);
	virtual Item importTo(const Keychain &keychain, Access *newAccess = NULL, SecKeychainAttributeList *attrList = NULL);
	virtual void didModify();

	CssmClient::SSDbUniqueRecord ssDbUniqueRecord();
	CssmClient::Key &key();
	CssmClient::CSP csp();

    // Returns the header of the unverified key (without checking integrity). This will skip ACL checks, but don't trust the data very much.
    // Can't return a reference, because maybe the unverified key will get released upon return.
    CssmKey::Header unverifiedKeyHeader();

	const CSSM_X509_ALGORITHM_IDENTIFIER& algorithmIdentifier();
	unsigned int strengthInBits(const CSSM_X509_ALGORITHM_IDENTIFIER *algid);
    CssmClient::Key publicKey();

	const AccessCredentials *getCredentials(
		CSSM_ACL_AUTHORIZATION_TAG operation,
		SecCredentialType credentialType);

	bool operator == (KeyItem &other);

	static void createPair(
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
        SecPointer<KeyItem> &outPrivateKey);

	static void importPair(
		Keychain keychain,
		const CSSM_KEY &publicCssmKey,
		const CSSM_KEY &privateCssmKey,
        SecPointer<Access> initialAccess,
        SecPointer<KeyItem> &outPublicKey, 
        SecPointer<KeyItem> &outPrivateKey);

	static SecPointer<KeyItem> generate(
		Keychain keychain,
		CSSM_ALGORITHMS algorithm,
		uint32 keySizeInBits,
		CSSM_CC_HANDLE contextHandle,
		CSSM_KEYUSE keyUsage,
		uint32 keyAttr,
		SecPointer<Access> initialAccess);

	static SecPointer<KeyItem> generateWithAttributes(
		const SecKeychainAttributeList *attrList,
		Keychain keychain,
		CSSM_ALGORITHMS algorithm,
		uint32 keySizeInBits,
		CSSM_CC_HANDLE contextHandle,
		CSSM_KEYUSE keyUsage,
		uint32 keyAttr,
		SecPointer<Access> initialAccess);

	virtual const CssmData &itemID();
	
	virtual CFHashCode hash();

    virtual void setIntegrity(bool force = false);
    virtual bool checkIntegrity();

    // Call this function to remove the integrity and partition_id ACLs from
    // this item. You're not supposed to be able to do this, so force the issue
    // by providing credentials to this keychain.
    virtual void removeIntegrity(const AccessCredentials *cred);

    static void modifyUniqueId(Keychain keychain, SSDb ssDb, DbUniqueRecord& uniqueId, DbAttributes& newDbAttributes, CSSM_DB_RECORDTYPE recordType);

protected:
	virtual PrimaryKey add(Keychain &keychain);
private:
    CssmClient::Key unverifiedKey();

	CssmClient::Key mKey;
	const CSSM_X509_ALGORITHM_IDENTIFIER *algid;
	CssmAutoData mPubKeyHash;
    CssmClient::Key mPublicKey;
};

} // end namespace KeychainCore

} // end namespace Security

struct OpaqueSecKeyRef {
    CFRuntimeBase _base;
    const SecKeyDescriptor *key_class;
    SecKeyRef cdsaKey;
    Security::KeychainCore::KeyItem *key;
    SecCredentialType credentialType;
};

#endif // !_SECURITY_KEYITEM_H_
