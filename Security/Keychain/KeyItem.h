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
// KeyItem.h
//
#ifndef _SECURITY_KEYITEM_H_
#define _SECURITY_KEYITEM_H_

#include <Security/Item.h>
#include <Security/SecKeyPriv.h>

namespace Security
{

namespace KeychainCore
{

class KeyItem : public ItemImpl
{
	NOCOPY(KeyItem)
public:
	SECCFFUNCTIONS(KeyItem, SecKeyRef, errSecInvalidItemRef)

	// db item contstructor
    KeyItem(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);

	// PrimaryKey item contstructor
    KeyItem(const Keychain &keychain, const PrimaryKey &primaryKey);

	KeyItem(KeyItem &keyItem);

	KeyItem(const CssmClient::Key &key);

    virtual ~KeyItem() throw();

	virtual void update();
	virtual Item copyTo(const Keychain &keychain, Access *newAccess = NULL);
	virtual void didModify();

	CssmClient::SSDbUniqueRecord ssDbUniqueRecord();
	CssmClient::Key &key();
	CssmClient::CSP csp();

	const CSSM_X509_ALGORITHM_IDENTIFIER& algorithmIdentifier();
	unsigned int strengthInBits(const CSSM_X509_ALGORITHM_IDENTIFIER *algid);

	const AccessCredentials *getCredentials(
		CSSM_ACL_AUTHORIZATION_TAG operation,
		SecCredentialType credentialType);

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

	static KeyItem *generate(
		Keychain keychain,
		CSSM_ALGORITHMS algorithm,
		uint32 keySizeInBits,
		CSSM_CC_HANDLE contextHandle,
		CSSM_KEYUSE keyUsage,
		uint32 keyAttr,
		SecPointer<Access> initialAccess);

protected:
	virtual PrimaryKey add(Keychain &keychain);
private:
	CssmClient::Key mKey;
	const CSSM_X509_ALGORITHM_IDENTIFIER *algid;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_KEYITEM_H_
