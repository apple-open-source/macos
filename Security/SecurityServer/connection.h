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
// connection - manage connections to clients
//
#ifndef _H_CONNECTION
#define _H_CONNECTION

#include "securityserver.h"
#include "SecurityAgentClient.h"
#include <Security/osxsigning.h>
#include "process.h"
#include "key.h"
#include <string>

using MachPlusPlus::Port;
using MachPlusPlus::TaskPort;

class Session;


//
// A Connection object represents an established connection between a client
// and the SecurityServer. Note that in principle, a client process can have
// multiple Connections (each represented by an IPC channel), though there will
// usually be only one.
//
class Connection {
	typedef Key::Handle KeyHandle;
public:
	Connection(Process &proc, Port rPort);
	virtual ~Connection();
	void terminate();		// normal termination
	bool abort(bool keepReplyPort = false); // abnormal termination
	
    Port clientPort() const	{ return mClientPort; }

	// work framing - called as work threads pick up connection work
	void beginWork();		// I've got it
	void checkWork();		// everything still okay?
	bool endWork();			// Done with this
	
	// notify that a SecurityAgent call may hang the active worker thread for a while
	void useAgent(SecurityAgent::Client *client)
	{ StLock<Mutex> _(lock); agentWait = client; }
	
	// special UI convenience - set a don't-ask-again trigger for Keychain-style ACLs
	void setAclUpdateTrigger(const SecurityServerAcl &object)
	{ aclUpdateTrigger = &object; aclUpdateTriggerCount = aclUpdateTriggerLimit + 1; }
	bool aclWasSetForUpdateTrigger(const SecurityServerAcl &object) const
	{ return aclUpdateTriggerCount > 0 && aclUpdateTrigger == &object; }

	Process &process;
	
public:
	void releaseKey(KeyHandle key);
	
	// service calls
	void generateSignature(const Context &context, Key &key,
		const CssmData &data, CssmData &signature);
	void verifySignature(const Context &context, Key &key,
		const CssmData &data, const CssmData &signature);
	void generateMac(const Context &context, Key &key,
		const CssmData &data, CssmData &mac);
	void verifyMac(const Context &context, Key &key,
		const CssmData &data, const CssmData &mac);
	
	void encrypt(const Context &context, Key &key, const CssmData &clear, CssmData &cipher);
	void decrypt(const Context &context, Key &key, const CssmData &cipher, CssmData &clear);
	
	void generateKey(Database *db, const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		uint32 usage, uint32 attrs, Key * &newKey);
	void generateKey(Database *db, const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		uint32 pubUsage, uint32 pubAttrs, uint32 privUsage, uint32 privAttrs,
		Key * &publicKey, Key * &privateKey);

    void wrapKey(const Context &context, Key *key,
        Key &keyToBeWrapped, const AccessCredentials *cred,
        const CssmData &descriptiveData, CssmKey &wrappedKey);
	Key &unwrapKey(Database *db, const Context &context, Key *key,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		uint32 usage, uint32 attrs, const CssmKey wrappedKey,
        Key *publicKey, CssmData *descriptiveData);

private:
	// peer state: established during connection startup; fixed thereafter
	Port mClientPort;
	
	// transient state (altered as we go)
	Mutex lock;
	enum State {
		idle,					// no thread services us
		busy,					// a thread is busy servicing us
		dying					// busy and scheduled to die as soon as possible
	} state;
	SecurityAgent::Client *agentWait;	// SA client session we may be waiting on
	
	// see KeychainPromptAclSubject in acl_keychain.cpp for more information on this
	const SecurityServerAcl *aclUpdateTrigger; // update trigger set for this (NULL if none)
    uint8 aclUpdateTriggerCount; // number of back-to-back requests honored
    static const uint8 aclUpdateTriggerLimit = 2;	// two subsequent calls (getAcl + changeAcl)
};


#endif //_H_CONNECTION
