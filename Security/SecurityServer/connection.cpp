/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// connection - manage connections to clients.
//
// Note that Connection objects are single-threaded; only one request can be outstanding
// per connection. The various operational calls (e.g. generateMac) can be called by
// multiple threads, but each call will be for a different connection (the one the request
// came in on). Thus, locking happens elsewhere as needed.
//
#include "connection.h"
#include "key.h"
#include "server.h"
#include "session.h"
#include <Security/keyclient.h>
#include <Security/genkey.h>
#include <Security/wrapkey.h>
#include <Security/signclient.h>
#include <Security/macclient.h>
#include <Security/cryptoclient.h>


//
// Construct a Connection object.
//
Connection::Connection(Process &proc, Port rPort)
 : process(proc), mClientPort(rPort), state(idle), agentWait(NULL),
   aclUpdateTrigger(NULL)
{
	// bump the send-rights count on the reply port so we keep the right after replying
	mClientPort.modRefs(MACH_PORT_RIGHT_SEND, +1);
	
	secdebug("SS", "New connection %p for process %d clientport=%d",
		this, process.pid(), int(rPort));
}


//
// When a Connection's destructor executes, the connection must already have been
// terminated. All we have to do here is clean up a bit.
//
Connection::~Connection()
{
	secdebug("SS", "Connection %p destroyed", this);
	assert(!agentWait);
}


//
// Terminate a Connection normally.
// This is assumed to be properly sequenced, so no thread races are possible.
//
void Connection::terminate()
{
	// cleanly discard port rights
	assert(state == idle);
	mClientPort.modRefs(MACH_PORT_RIGHT_SEND, -1);	// discard surplus send right
	assert(mClientPort.getRefs(MACH_PORT_RIGHT_SEND) == 1);	// one left for final reply
	secdebug("SS", "Connection %p terminated", this);
}


//
// Abort a Connection.
// This may be called from thread A while thread B is working a request for the Connection,
// so we must be careful.
//
bool Connection::abort(bool keepReplyPort)
{
	StLock<Mutex> _(lock);
    if (!keepReplyPort)
        mClientPort.destroy();		// dead as a doornail already
	switch (state) {
	case idle:
		secdebug("SS", "Connection %p aborted", this);
		return true;				// just shoot me
	case busy:
		state = dying;				// shoot me soon, please
		if (agentWait)
			agentWait->cancel();
		secdebug("SS", "Connection %p abort deferred (busy)", this);
		return false;				// but not quite yet
	default:
		assert(false);				// impossible (we hope)
		return true;				// placebo
	}
}


//
// Service request framing.
// These are here so "hanging" connection service threads don't fall
// into the Big Bad Void as Connections and processes drop out from
// under them.
//
void Connection::beginWork()
{
	switch (state) {
	case idle:
		state = busy;
		process.beginConnection(*this);
		break;
	case busy:
		secdebug("SS", "Attempt to re-enter connection %p(port %d)", this, mClientPort.port());
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	//@@@ some state-error code instead?
	default:
		assert(false);
	}
}

void Connection::checkWork()
{
	StLock<Mutex> _(lock);
	switch (state) {
	case busy:
		return;
	case dying:
		agentWait = NULL;	// obviously we're not waiting on this
		throw this;
	default:
		assert(false);
	}
}

bool Connection::endWork()
{
	switch (state) {
	case busy:
		// process the n-step aclUpdateTrigger
		if (aclUpdateTrigger) {
            if (--aclUpdateTriggerCount == 0) {
                aclUpdateTrigger = NULL;
                secdebug("kcacl", "acl update trigger expires");
            } else
                secdebug("kcacl", "acl update trigger armed for %d calls",
                    aclUpdateTriggerCount);
        }
		// end involvement
		state = idle;
		process.endConnection(*this);
		return false;
	case dying:
		secdebug("SS", "Connection %p abort resuming", this);
		if (process.endConnection(*this))
			delete &process;
		return true;
	default:
		assert(false);
		return true;	// placebo
	}
}


//
// Key creation and release
//
void Connection::releaseKey(Key::Handle key)
{
	delete &Server::key(key);
}


//
// Key inquiries
//
CSSM_KEY_SIZE Connection::queryKeySize(Key &key)
{
    CssmClient::Key theKey(Server::csp(), key);
    return theKey.sizeInBits();
}


//
// Signatures and MACs
//
void Connection::generateSignature(const Context &context, Key &key,
	CSSM_ALGORITHMS signOnlyAlgorithm, const CssmData &data, CssmData &signature)
{
	context.replace(CSSM_ATTRIBUTE_KEY, key.cssmKey());
	key.validate(CSSM_ACL_AUTHORIZATION_SIGN, context);
	CssmClient::Sign signer(Server::csp(), context.algorithm(), signOnlyAlgorithm);
	signer.override(context);
	signer.sign(data, signature);
}

void Connection::verifySignature(const Context &context, Key &key,
	CSSM_ALGORITHMS verifyOnlyAlgorithm, const CssmData &data, const CssmData &signature)
{
	context.replace(CSSM_ATTRIBUTE_KEY, key.cssmKey());
	CssmClient::Verify verifier(Server::csp(), context.algorithm(), verifyOnlyAlgorithm);
	verifier.override(context);
	verifier.verify(data, signature);
}

void Connection::generateMac(const Context &context, Key &key,
	const CssmData &data, CssmData &mac)
{
	context.replace(CSSM_ATTRIBUTE_KEY, key.cssmKey());
	key.validate(CSSM_ACL_AUTHORIZATION_MAC, context);
	CssmClient::GenerateMac signer(Server::csp(), context.algorithm());
	signer.override(context);
	signer.sign(data, mac);
}

void Connection::verifyMac(const Context &context, Key &key,
	const CssmData &data, const CssmData &mac)
{
	context.replace(CSSM_ATTRIBUTE_KEY, key.cssmKey());
	key.validate(CSSM_ACL_AUTHORIZATION_MAC, context);
	CssmClient::VerifyMac verifier(Server::csp(), context.algorithm());
	verifier.override(context);
	verifier.verify(data, mac);
}


//
// Encryption/decryption
//
void Connection::encrypt(const Context &context, Key &key,
	const CssmData &clear, CssmData &cipher)
{
	context.replace(CSSM_ATTRIBUTE_KEY, key.cssmKey());
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

void Connection::decrypt(const Context &context, Key &key,
	const CssmData &cipher, CssmData &clear)
{
	context.replace(CSSM_ATTRIBUTE_KEY, key.cssmKey());
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
void Connection::generateKey(Database *db, const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		uint32 usage, uint32 attrs, Key * &newKey)
{
	// prepare a context
	CssmClient::GenerateKey generate(Server::csp(), context.algorithm());
	generate.override(context);
	
	// generate key
	// @@@ turn "none" return into reference if permanent (only)
	CssmKey key;
	generate(key, Key::KeySpec(usage, attrs));
		
	// register and return the generated key
    newKey = new Key(db, key, attrs & Key::managedAttributes, owner);
}

void Connection::generateKey(Database *db, const Context &context,
	const AccessCredentials *cred, const AclEntryPrototype *owner,
	uint32 pubUsage, uint32 pubAttrs, uint32 privUsage, uint32 privAttrs,
    Key * &publicKey, Key * &privateKey)
{
	// prepare a context
	CssmClient::GenerateKey generate(Server::csp(), context.algorithm());
	generate.override(context);
	
	// this may take a while; let our server object know
	Server::active().longTermActivity();
	
	// generate keys
	// @@@ turn "none" return into reference if permanent (only)
	CssmKey pubKey, privKey;
	generate(pubKey, Key::KeySpec(pubUsage, pubAttrs),
		privKey, Key::KeySpec(privUsage, privAttrs));
		
	// register and return the generated keys
	publicKey = new Key(db, pubKey, pubAttrs & Key::managedAttributes, owner);
	privateKey = new Key(db, privKey, privAttrs & Key::managedAttributes, owner);
}

Key &Connection::deriveKey(Database *db, const Context &context, Key *baseKey,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
        CssmData *param, uint32 usage, uint32 attrs)
{
	// prepare a key-derivation context
    if (baseKey) {
		baseKey->validate(CSSM_ACL_AUTHORIZATION_DERIVE, cred);
        context.replace(CSSM_ATTRIBUTE_KEY, baseKey->cssmKey());
	}
	CssmClient::DeriveKey derive(Server::csp(), context.algorithm(), CSSM_ALGID_NONE);
	derive.override(context);
	
	// derive key
	// @@@ turn "none" return into reference if permanent (only)
	CssmKey key;
	derive(param, Key::KeySpec(usage, attrs), key);
		
	// register and return the generated key
    return *new Key(db, key, attrs & Key::managedAttributes, owner);
}


//
// Key wrapping and unwrapping.
// Note that the key argument (the key in the context) is optional because of the special
// case of "cleartext" (null algorithm) wrapping for import/export.
//

void Connection::wrapKey(const Context &context, Key *key,
    Key &keyToBeWrapped, const AccessCredentials *cred,
    const CssmData &descriptiveData, CssmKey &wrappedKey)
{
    keyToBeWrapped.validate(context.algorithm() == CSSM_ALGID_NONE ?
            CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR : CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED,
        cred);
	if(!(keyToBeWrapped.attributes() & CSSM_KEYATTR_EXTRACTABLE)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
	}
    if (key)
        context.replace(CSSM_ATTRIBUTE_KEY, key->cssmKey());
    CssmClient::WrapKey wrap(Server::csp(), context.algorithm());
    wrap.override(context);
    wrap.cred(const_cast<AccessCredentials *>(cred));	//@@@ const madness - fix in client/pod
    wrap(keyToBeWrapped, wrappedKey, &descriptiveData);
}

Key &Connection::unwrapKey(Database *db, const Context &context, Key *key,
	const AccessCredentials *cred, const AclEntryPrototype *owner,
	uint32 usage, uint32 attrs, const CssmKey wrappedKey,
    Key *publicKey, CssmData *descriptiveData)
{
    if (key)
        context.replace(CSSM_ATTRIBUTE_KEY, key->cssmKey());
    CssmClient::UnwrapKey unwrap(Server::csp(), context.algorithm());
    unwrap.override(context);
    CssmKey unwrappedKey;
    unwrap.cred(const_cast<AccessCredentials *>(cred));	//@@@ const madness - fix in client/pod
    if (owner) {
        AclEntryInput ownerInput(*owner);	//@@@ const trouble - fix in client/pod
        unwrap.aclEntry(ownerInput);
    }

    // @@@ Invoking conversion operator to CssmKey & on *publicKey and take the address of the result.
    unwrap(wrappedKey, Key::KeySpec(usage, attrs), unwrappedKey,
        descriptiveData, publicKey ? &static_cast<const CssmKey &>(*publicKey) : NULL);

    return *new Key(db, unwrappedKey, attrs & Key::managedAttributes, owner);
}


//
// Miscellaneous CSSM functions
//
uint32 Connection::getOutputSize(const Context &context, Key &key, uint32 inputSize, bool encrypt)
{
    // We're fudging here somewhat, since the context can be any type.
    // ctx.override will fix the type, and no-one's the wiser.
	context.replace(CSSM_ATTRIBUTE_KEY, key.cssmKey());
    CssmClient::Digest ctx(Server::csp(), context.algorithm());
    ctx.override(context);
    return ctx.getOutputSize(inputSize, encrypt);
}

