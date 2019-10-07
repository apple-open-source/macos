/*
 * Copyright (c) 2000-2009 Apple Inc. All Rights Reserved.
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
// connection - manage connections to clients.
//
// Note that Connection objects correspond to client process threads, and are
// thus inherently single-threaded. It is physically impossible for multiple
// requests to come in for the same Connection, unless the client side is
// illegally messing with the IPC protocol (for which we check below).
// It is still necessary to take the object lock for a Connection because there
// are times when we want to manipulate a busy Connection from another securityd
// thread (say, in response to a DPN).
//
#include "connection.h"
#include "key.h"
#include "server.h"
#include "session.h"
#include <security_cdsa_client/keyclient.h>
#include <security_cdsa_client/genkey.h>
#include <security_cdsa_client/wrapkey.h>
#include <security_cdsa_client/signclient.h>
#include <security_cdsa_client/macclient.h>
#include <security_cdsa_client/cryptoclient.h>


//
// Construct a Connection object.
//
Connection::Connection(Process &proc, Port rPort)
 : mClientPort(rPort), mGuestRef(kSecNoGuest), state(idle), agentWait(NULL)
{
	parent(proc);
	
	// bump the send-rights count on the reply port so we keep the right after replying
	mClientPort.modRefs(MACH_PORT_RIGHT_SEND, +1);
	
    secinfo("SecServer", "New client connection %p: %d %d", this, rPort.port(), proc.uid());
}


//
// When a Connection's destructor executes, the connection must already have been
// terminated. All we have to do here is clean up a bit.
//
Connection::~Connection() try
{
    mClientPort.deallocate();
    secinfo("SecServer", "releasing client connection %p", this);
	assert(!agentWait);
} catch (...) {
    secerror("SecServer: Error deallocating connection port");
    return;
}

//
// Set the (last known) guest handle for this connection.
//
void Connection::guestRef(SecGuestRef newGuest, SecCSFlags flags)
{
	secinfo("SecServer", "Connection %p switches to guest 0x%x", this, newGuest);
	mGuestRef = newGuest;
}

//
// Service request framing.
// These are here so "hanging" connection service threads don't fall
// into the Big Bad Void as Connections and processes drop out from
// under them.
//
void Connection::beginWork(audit_token_t &auditToken)
{
    // assume the audit token will be valid for the Connection's lifetime 
    // (but no longer)
    mAuditToken = &auditToken;
	switch (state) {
	case idle:
		state = busy;
		mOverrideReturn = CSSM_OK;	// clear override
		break;
	case busy:
		secinfo("SecServer", "Attempt to re-enter connection %p(port %d)", this, mClientPort.port());
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	//@@@ some state-error code instead?
	default:
		assert(false);
	}
}

void Connection::checkWork()
{
	StLock<Mutex> _(*this);
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

void Connection::endWork(CSSM_RETURN &rcode)
{
    mAuditToken = NULL;

	switch (state) {
	case busy:
		if (mOverrideReturn && rcode == CSSM_OK)
			rcode = mOverrideReturn;
		state = idle;
		return;
	case dying:
		secinfo("SecServer", "Connection %p abort resuming", this);
		return;
	default:
		assert(false);
		return;	// placebo
	}
}
