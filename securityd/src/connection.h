/*
 * Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
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
#include "process.h"
#include "session.h"
#include "notifications.h"
#include <string>

using MachPlusPlus::Port;
using MachPlusPlus::TaskPort;

class Session;


//
// A Connection object represents an established connection between a client
// and securityd. There is a separate Connection object for each Mach reply port
// that was (ever) used to talk to securityd. In practice, this maps to one reply
// port (and thus one Connection) for each client thread that (ever) talks to securityd.
//
// If a client tricked us into using multiple reply ports from one thread, we'd treat
// them as distinct client threads (which really doesn't much matter to us). The standard
// client library (libsecurityd) won't let you do that.
//
class Connection : public PerConnection, public Listener::JitterBuffer {
public:
	Connection(Process &proc, Port rPort);
	virtual ~Connection();
	void terminate();		// normal termination
	void abort(bool keepReplyPort = false); // abnormal termination
	
    Port clientPort() const	{ return mClientPort; }
	
	// Code Signing guest management - tracks current guest id in client
	SecGuestRef guestRef() const { return mGuestRef; }
	void guestRef(SecGuestRef newGuest, SecCSFlags flags = 0);

	// work framing - called as work threads pick up connection work
	void beginWork();		// I've got it
	void checkWork();		// everything still okay?
	void endWork(CSSM_RETURN &rcode); // Done with this
	
	// notify that a SecurityAgent call may hang the active worker thread for a while
	void useAgent(SecurityAgent::Client *client)
	{ StLock<Mutex> _(*this); agentWait = client; }
	
	// set an overriding CSSM_RETURN to return instead of success
	void overrideReturn(CSSM_RETURN rc) { mOverrideReturn = rc; }
	
	Process &process() const { return parent<Process>(); }
	Session &session() const { return process().session(); }
	
private:
	// peer state: established during connection startup; fixed thereafter
	Port mClientPort;			// client's Mach reply port
	SecGuestRef mGuestRef;		// last known Code Signing guest reference for this client thread
	CSSM_RETURN mOverrideReturn; // override successful return code (only)
	
	// transient state (altered as we go)
	enum State {
		idle,					// no thread services us
		busy,					// a thread is busy servicing us
		dying					// busy and scheduled to die as soon as possible
	} state;
	SecurityAgent::Client *agentWait;	// SA client session we may be waiting on
};


#endif //_H_CONNECTION
