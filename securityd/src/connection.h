/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// connection - manage connections to clients
//
#ifndef _H_CONNECTION
#define _H_CONNECTION

#include <security_agent_client/agentclient.h>
#include <security_utilities/osxcode.h>
#include "process.h"
#include "session.h"
#include "key.h"
#include <string>

using MachPlusPlus::Port;
using MachPlusPlus::TaskPort;

class Session;


//
// A Connection object represents an established connection between a client
// and securityd. Note that in principle, a client process can have
// multiple Connections (each represented by an IPC channel), though there will
// usually be only one.
//
class Connection : public PerConnection {
	typedef Key::Handle KeyHandle;
public:
	Connection(Process &proc, Port rPort);
	virtual ~Connection();
	void terminate();		// normal termination
	void abort(bool keepReplyPort = false); // abnormal termination
	
    Port clientPort() const	{ return mClientPort; }

	// work framing - called as work threads pick up connection work
	void beginWork();		// I've got it
	void checkWork();		// everything still okay?
	void endWork();			// Done with this
	
	// notify that a SecurityAgent call may hang the active worker thread for a while
	void useAgent(SecurityAgent::Client *client)
	{ StLock<Mutex> _(*this); agentWait = client; }
	
	// special UI convenience - set a don't-ask-again trigger for Keychain-style ACLs
	void setAclUpdateTrigger(const SecurityServerAcl &object)
	{ aclUpdateTrigger = &object; aclUpdateTriggerCount = aclUpdateTriggerLimit + 1; }
	bool aclWasSetForUpdateTrigger(const SecurityServerAcl &object) const
	{ return aclUpdateTriggerCount > 0 && aclUpdateTrigger == &object; }
	
	Process &process() const { return parent<Process>(); }
	Session &session() const { return process().session(); }
	
private:
	// peer state: established during connection startup; fixed thereafter
	Port mClientPort;
	
	// transient state (altered as we go)
	enum State {
		idle,					// no thread services us
		busy,					// a thread is busy servicing us
		dying					// busy and scheduled to die as soon as possible
	} state;
	SecurityAgent::Client *agentWait;	// SA client session we may be waiting on
	
	// see KeychainPromptAclSubject in acl_keychain.cpp for more information on this
	const SecurityServerAcl *aclUpdateTrigger; // update trigger set for this (NULL if none)
    uint8 aclUpdateTriggerCount; // number of back-to-back requests honored
    static const uint8 aclUpdateTriggerLimit = 3;	// 3 calls (getAcl+getOwner+changeAcl)
};


#endif //_H_CONNECTION
