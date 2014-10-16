/*
 * Copyright (c) 2004-2005 Apple Computer, Inc. All Rights Reserved.
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
// localdatabase - locally implemented database using internal CSP cryptography
//
// A LocalDatabase manages keys with a locally resident AppleCSP.
// This is an abstract class useful for subclassing.
//
#ifndef _H_LOCALDATABASE
#define _H_LOCALDATABASE

#include "database.h"

class LocalKey;

class LocalDbCommon : public DbCommon {
public:
	LocalDbCommon(Session &ssn) : DbCommon(ssn) { }
	
	Mutex &uiLock()  { return mUILock; }
	
private:
	// Contract: callers shall not simultaneously hold mUILock and the 
	// DbCommon lock.  StSyncLock coordinates them to uphold the contract.  
	Mutex mUILock;				// serializes user interaction
};

//
// A Database object represents an Apple CSP/DL open database (DL/DB) object.
// It maintains its protected semantic state (including keys) and provides controlled
// access.
//
class LocalDatabase : public Database {
public:
	LocalDatabase(Process &proc);
	
public:
	//void releaseKey(Key &key);
	void queryKeySizeInBits(Key &key, CssmKeySize &result);
	
	// service calls
	void generateSignature(const Context &context, Key &key, CSSM_ALGORITHMS signOnlyAlgorithm,
		const CssmData &data, CssmData &signature);
	void verifySignature(const Context &context, Key &key, CSSM_ALGORITHMS verifyOnlyAlgorithm,
		const CssmData &data, const CssmData &signature);
	void generateMac(const Context &context, Key &key,
		const CssmData &data, CssmData &mac);
	void verifyMac(const Context &context, Key &key,
		const CssmData &data, const CssmData &mac);
	
	void encrypt(const Context &context, Key &key, const CssmData &clear, CssmData &cipher);
	void decrypt(const Context &context, Key &key, const CssmData &cipher, CssmData &clear);
	
	void generateKey(const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs, RefPointer<Key> &newKey);
	void generateKey(const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		CSSM_KEYUSE pubUsage, CSSM_KEYATTR_FLAGS pubAttrs,
		CSSM_KEYUSE privUsage, CSSM_KEYATTR_FLAGS privAttrs,
		RefPointer<Key> &publicKey, RefPointer<Key> &privateKey);
	void deriveKey(const Context &context, Key *key,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		CssmData *param, uint32 usage, uint32 attrs, RefPointer<Key> &derivedKey);

    void wrapKey(const Context &context, const AccessCredentials *cred,
		Key *wrappingKey, Key &keyToBeWrapped,
        const CssmData &descriptiveData, CssmKey &wrappedKey);
	void unwrapKey(const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		Key *wrappingKey, Key *publicKey, CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs,
		const CssmKey wrappedKey, RefPointer<Key> &unwrappedKey, CssmData &descriptiveData);
        
    void getOutputSize(const Context &context, Key &key, uint32 inputSize, bool encrypt, uint32 &result);

protected:
	virtual RefPointer<Key> makeKey(const CssmKey &newKey, uint32 moreAttributes,
		const AclEntryPrototype *owner) = 0;
};

#endif //_H_LOCALDATABASE
