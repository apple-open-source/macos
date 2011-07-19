/*
 * Copyright (c) 2002-2010 Apple Inc. All Rights Reserved.
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
	SECCFFUNCTIONS(KeyItem, SecKeyRef, errSecInvalidItemRef, gTypes().KeyItem)

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

    virtual ~KeyItem() throw();

	virtual void update();
	virtual Item copyTo(const Keychain &keychain, Access *newAccess = NULL);
	virtual Item importTo(const Keychain &keychain, Access *newAccess = NULL, SecKeychainAttributeList *attrList = NULL);
	virtual void didModify();

	CssmClient::SSDbUniqueRecord ssDbUniqueRecord();
	CssmClient::Key &key();
	CssmClient::CSP csp();

	const CSSM_X509_ALGORITHM_IDENTIFIER& algorithmIdentifier();
	unsigned int strengthInBits(const CSSM_X509_ALGORITHM_IDENTIFIER *algid);

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
	
	void RawSign(SecPadding padding, CSSM_DATA dataToSign, const AccessCredentials *credentials, CSSM_DATA& signedData);
	void RawVerify(SecPadding padding, CSSM_DATA dataToVerify, const AccessCredentials *credentials, CSSM_DATA signature);
	void Encrypt(SecPadding padding, CSSM_DATA dataToEncrypt, const AccessCredentials *credentials, CSSM_DATA& encryptedData);
	void Decrypt(SecPadding padding, CSSM_DATA dataToEncrypt, const AccessCredentials *credentials, CSSM_DATA& encryptedData);

protected:
	virtual PrimaryKey add(Keychain &keychain);
private:
	CssmClient::Key mKey;
	const CSSM_X509_ALGORITHM_IDENTIFIER *algid;
	CssmAutoData mPubKeyHash;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_KEYITEM_H_
