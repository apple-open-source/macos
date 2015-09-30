/*
 * Copyright (c) 2004,2006,2008 Apple Inc. All Rights Reserved.
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
#include "localdatabase.h"
#include "agentquery.h"
#include "localkey.h"
#include "server.h"
#include "session.h"
#include <security_cdsa_utilities/acl_any.h>	// for default owner ACLs
#include <security_cdsa_client/wrapkey.h>
#include <security_cdsa_client/genkey.h>
#include <security_cdsa_client/signclient.h>
#include <security_cdsa_client/cryptoclient.h>
#include <security_cdsa_client/macclient.h>
#include <security_utilities/endian.h>


//
// Create a Database object from initial parameters (create operation)
//
LocalDatabase::LocalDatabase(Process &proc)
	: Database(proc)
{
}


static inline LocalKey &myKey(Key &key)
{
	return safer_cast<LocalKey &>(key);
}


//
// Key inquiries
//
void LocalDatabase::queryKeySizeInBits(Key &key, CssmKeySize &result)
{
    CssmClient::Key theKey(Server::csp(), myKey(key));
    result = theKey.sizeInBits();
}


//
// Signatures and MACs
//
void LocalDatabase::generateSignature(const Context &context, Key &key,
	CSSM_ALGORITHMS signOnlyAlgorithm, const CssmData &data, CssmData &signature)
{
	context.replace(CSSM_ATTRIBUTE_KEY, myKey(key).cssmKey());
	key.validate(CSSM_ACL_AUTHORIZATION_SIGN, context);
	CssmClient::Sign signer(Server::csp(), context.algorithm(), signOnlyAlgorithm);
	signer.override(context);
	signer.sign(data, signature);
}

void LocalDatabase::verifySignature(const Context &context, Key &key,
	CSSM_ALGORITHMS verifyOnlyAlgorithm, const CssmData &data, const CssmData &signature)
{
	context.replace(CSSM_ATTRIBUTE_KEY, myKey(key).cssmKey());
	CssmClient::Verify verifier(Server::csp(), context.algorithm(), verifyOnlyAlgorithm);
	verifier.override(context);
	verifier.verify(data, signature);
}

void LocalDatabase::generateMac(const Context &context, Key &key,
	const CssmData &data, CssmData &mac)
{
	context.replace(CSSM_ATTRIBUTE_KEY, myKey(key).cssmKey());
	key.validate(CSSM_ACL_AUTHORIZATION_MAC, context);
	CssmClient::GenerateMac signer(Server::csp(), context.algorithm());
	signer.override(context);
	signer.sign(data, mac);
}

void LocalDatabase::verifyMac(const Context &context, Key &key,
	const CssmData &data, const CssmData &mac)
{
	context.replace(CSSM_ATTRIBUTE_KEY, myKey(key).cssmKey());
	key.validate(CSSM_ACL_AUTHORIZATION_MAC, context);
	CssmClient::VerifyMac verifier(Server::csp(), context.algorithm());
	verifier.override(context);
	verifier.verify(data, mac);
}


//
// Encryption/decryption
//
void LocalDatabase::encrypt(const Context &context, Key &key,
	const CssmData &clear, CssmData &cipher)
{
	context.replace(CSSM_ATTRIBUTE_KEY, myKey(key).cssmKey());
	key.validate(CSSM_ACL_AUTHORIZATION_ENCRYPT, context);
	CssmClient::Encrypt cryptor(Server::csp(), context.algorithm());
	cryptor.override(context);
	CssmData remData;
	size_t totalLength = cryptor.encrypt(clear, cipher, remData);
	// shouldn't need remData - if an algorithm REQUIRES this, we'd have to ship it
	if (remData)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	cipher.length(totalLength);
}

void LocalDatabase::decrypt(const Context &context, Key &key,
	const CssmData &cipher, CssmData &clear)
{
	context.replace(CSSM_ATTRIBUTE_KEY, myKey(key).cssmKey());
	key.validate(CSSM_ACL_AUTHORIZATION_DECRYPT, context);
	CssmClient::Decrypt cryptor(Server::csp(), context.algorithm());
	cryptor.override(context);
	CssmData remData;
	size_t totalLength = cryptor.decrypt(cipher, clear, remData);
	// shouldn't need remData - if an algorithm REQUIRES this, we'd have to ship it
	if (remData)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
	clear.length(totalLength);
}


//
// Key generation and derivation.
// Currently, we consider symmetric key generation to be fast, but
// asymmetric key generation to be (potentially) slow.
//
void LocalDatabase::generateKey(const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		uint32 usage, uint32 attrs, RefPointer<Key> &newKey)
{
	// prepare a context
	CssmClient::GenerateKey generate(Server::csp(), context.algorithm());
	generate.override(context);
	
	// generate key
	// @@@ turn "none" return into reference if permanent (only)
	CssmKey key;
	generate(key, LocalKey::KeySpec(usage, attrs));
		
	// register and return the generated key
    newKey = makeKey(key, attrs & LocalKey::managedAttributes, owner);
}

void LocalDatabase::generateKey(const Context &context,
	const AccessCredentials *cred, const AclEntryPrototype *owner,
	uint32 pubUsage, uint32 pubAttrs, uint32 privUsage, uint32 privAttrs,
    RefPointer<Key> &publicKey, RefPointer<Key> &privateKey)
{
	// prepare a context
	CssmClient::GenerateKey generate(Server::csp(), context.algorithm());
	generate.override(context);
	
	// this may take a while; let our server object know
	Server::active().longTermActivity();
	
	// generate keys
	// @@@ turn "none" return into reference if permanent (only)
	CssmKey pubKey, privKey;
	generate(pubKey, LocalKey::KeySpec(pubUsage, pubAttrs),
		privKey, LocalKey::KeySpec(privUsage, privAttrs));
		
	// register and return the generated keys
	publicKey = makeKey(pubKey, pubAttrs & LocalKey::managedAttributes, 
		(pubAttrs & CSSM_KEYATTR_PUBLIC_KEY_ENCRYPT) ? owner : NULL);
	privateKey = makeKey(privKey, privAttrs & LocalKey::managedAttributes, owner);
}


//
// Key wrapping and unwrapping.
// Note that the key argument (the key in the context) is optional because of the special
// case of "cleartext" (null algorithm) wrapping for import/export.
//

void LocalDatabase::wrapKey(const Context &context, const AccessCredentials *cred,
	Key *wrappingKey, Key &keyToBeWrapped,
	const CssmData &descriptiveData, CssmKey &wrappedKey)
{
    keyToBeWrapped.validate(context.algorithm() == CSSM_ALGID_NONE ?
            CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR : CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED,
        cred);
    if (wrappingKey) {
        context.replace(CSSM_ATTRIBUTE_KEY, myKey(*wrappingKey).cssmKey());
		wrappingKey->validate(CSSM_ACL_AUTHORIZATION_ENCRYPT, context);
	}
    CssmClient::WrapKey wrap(Server::csp(), context.algorithm());
    wrap.override(context);
    wrap.cred(cred);
    wrap(myKey(keyToBeWrapped), wrappedKey, &descriptiveData);
}

void LocalDatabase::unwrapKey(const Context &context,
	const AccessCredentials *cred, const AclEntryPrototype *owner,
	Key *wrappingKey, Key *publicKey, CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs,
	const CssmKey wrappedKey, RefPointer<Key> &unwrappedKey, CssmData &descriptiveData)
{
    if (wrappingKey) {
        context.replace(CSSM_ATTRIBUTE_KEY, myKey(*wrappingKey).cssmKey());
		wrappingKey->validate(CSSM_ACL_AUTHORIZATION_DECRYPT, context);
	}
	// we are not checking access on the public key, if any
	
    CssmClient::UnwrapKey unwrap(Server::csp(), context.algorithm());
    unwrap.override(context);
    unwrap.cred(cred);
	
	// the AclEntryInput will have to live until unwrap is done
	AclEntryInput ownerInput;
    if (owner) {
		ownerInput.proto() = *owner;
        unwrap.owner(ownerInput);
	}
	
    CssmKey result;
	unwrap(wrappedKey, LocalKey::KeySpec(usage, attrs), result, &descriptiveData,
		publicKey ? &myKey(*publicKey).cssmKey() : NULL);
    unwrappedKey = makeKey(result, attrs & LocalKey::managedAttributes, owner);
}


//
// Key derivation
//
void LocalDatabase::deriveKey(const Context &context, Key *key,
	const AccessCredentials *cred, const AclEntryPrototype *owner,
	CssmData *param, uint32 usage, uint32 attrs, RefPointer<Key> &derivedKey)
{
    if (key) {
		key->validate(CSSM_ACL_AUTHORIZATION_DERIVE, context);
        context.replace(CSSM_ATTRIBUTE_KEY, myKey(*key).cssmKey());
	}
	CssmClient::DeriveKey derive(Server::csp(), context.algorithm(), CSSM_ALGID_NONE);
	derive.override(context);
	
	// derive key
	// @@@ turn "none" return into reference if permanent (only)
	CssmKey dKey;
	derive(param, LocalKey::KeySpec(usage, attrs), dKey);

	// register and return the generated key
    derivedKey = makeKey(dKey, attrs & LocalKey::managedAttributes, owner);
}


//
// Miscellaneous CSSM functions
//
void LocalDatabase::getOutputSize(const Context &context, Key &key, uint32 inputSize,
	bool encrypt, uint32 &result)
{
    // We're fudging here somewhat, since the context can be any type.
    // ctx.override will fix the type, and no-one's the wiser.
	context.replace(CSSM_ATTRIBUTE_KEY, myKey(key).cssmKey());
    CssmClient::Digest ctx(Server::csp(), context.algorithm());
    ctx.override(context);
    result = ctx.getOutputSize(inputSize, encrypt);
}
