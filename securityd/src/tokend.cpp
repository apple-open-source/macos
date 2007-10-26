/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
// tokend - internal tracker for a tokend smartcard driver process
//
#include "tokend.h"
#include <security_utilities/logging.h>


//
// Construct a TokenDaemon.
// This will (try to) execute the actual tokend at 'path'; it will not communicate
// with it beyond the standard securityd checkin mechanism.
// The constructor will return if the tokend is either checked in and ready, or
// it has died (or been unable to start at all). It's then our owner's responsibility
// to manage us from there, including deleting us eventually.
//
TokenDaemon::TokenDaemon(RefPointer<Bundle> code,
		const string &reader, const PCSC::ReaderState &readerState, TokenCache &cache)
	: Tokend::ClientSession(Allocator::standard(), Allocator::standard()),
	  mMe(code), mReaderName(reader), mState(readerState),
	  mFaultRelay(NULL), mFaulted(false), mProbed(false),
	  mUid(cache.tokendUid()), mGid(cache.tokendGid())
{
	this->fork();
	switch (ServerChild::state()) {
	case alive:
		Tokend::ClientSession::servicePort(ServerChild::servicePort());
		secdebug("tokend", "%p (pid %d) %s has launched", this, pid(), bundlePath().c_str());
		break;
	case dead:
		// tokend died or quit before becoming ready
		secdebug("tokend", "%p (pid %d) %s failed on startup", this, pid(), bundlePath().c_str());
		break;
	default:
		assert(false);
	}
}


//
// The destructor for TokenDaemon *may* be called with tokend still alive.
// We rely on ServerChild's destructor to kill it for us.
// If we wanted to do something especally nice just for tokend (such as sending
// a "go die" message), we'd do it here.
//
TokenDaemon::~TokenDaemon()
{
	secdebug("tokend", "%p (pid %d) %s is being destroyed", this, pid(), bundlePath().c_str());
}


//
// Calculate a tokenUid as a concatenation of tokend identifier and uid
//
std::string TokenDaemon::tokenUid() const
{
	assert(hasTokenUid());
	return mTokenUid;
}


//
// Access to custom Info.plist fields
//
uint32 TokenDaemon::maxScore() const
{
	return cfNumber(CFNumberRef(mMe->infoPlistItem("TokendBestScore")), INT_MAX);
}


//
// Our childAction is to launch tokend after preparing its environment
//
void TokenDaemon::childAction()
{
	
	// permanently relinquish high privilege
#if defined(NDEBUG)
	UnixError::check(::setgid(mGid));
	UnixError::check(::setuid(mUid));
#else //NDEBUG
	// best effort, okay if not
	::setgid(mGid);
	::setuid(mUid);
#endif //NDEBUG
	secdebug("tokend", "uid=%d gid=%d", getuid(), getgid());

	// go run the tokend
	char protocol[20]; snprintf(protocol, sizeof(protocol), "%d", TDPROTOVERSION);
	secdebug("tokend", "executing %s(\"%s\",%s)",
		mMe->executablePath().c_str(), mReaderName.c_str(), protocol);
	execl(mMe->executablePath().c_str(),
		mMe->executablePath().c_str(),
		protocol,									// #1: protocol version
		mReaderName.c_str(),						// #2: reader name
		CssmData::wrap(mState).toHex().c_str(),		// #3: PCSC reader state (hex)
		NULL);
}


//
// This will be called (by the UnixChild layer) when UNIX tells us that our tokend
// has died. That means it's quite dead (a Zombie) already.
//
void TokenDaemon::dying()
{
	ServerChild::dying();					// honor prior engagement
	fault(true, "token daemon has died");	// flag asynchronous fault
}


//
// Declare a fault.
//@@@ Semantics TBD.
//
void TokenDaemon::fault(bool async, const char *reason)
{
	if (!mFaulted) {
		secdebug("tokend", "%p declaring %s FAULT condition: %s",
			this, async ? "ASYNCHRONOUS" : "SYNCHRONOUS", reason);
		Syslog::notice("card in reader %s has faulted (%s)",
			mReaderName.c_str(), reason);
		mFaulted = true;
		if (mFaultRelay)
			mFaultRelay->relayFault(async);
	}
	if (!async)
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_FAILED);
}


//
// A fault signalled from the ClientSession layer is just a (synchronous) fault
// of TokenDaemon itself.
//
void TokenDaemon::fault()
{
	this->fault(false, "tokend service failed");
}


//
// Overridden Tokend::ClientSession methods (to siphon off some return data).
// Note that this does NOT include the Access magic; you still have to use
// TokenDaemon::Access to mediate the call.
//
bool TokenDaemon::probe()
{
	secdebug("tokend", "%p probing", this);
	ClientSession::probe(mScore, mTokenUid);
	secdebug("tokend", "%p probed score=%d tokenUid=\"%s\"", this, mScore, mTokenUid.c_str());
	mProbed = true;
	return mScore > 0;
}


//
// FaultRelay
//
FaultRelay::~FaultRelay()
{ /* virtual */ }


//
// Debug dump support
//
#if defined(DEBUGDUMP)

void TokenDaemon::dumpNode()
{
	PerGlobal::dumpNode();
	if (mFaulted)
		Debug::dump(" FAULT");
	Debug::dump(" service=%d/%d",
		ClientSession::servicePort().port(), ServerChild::servicePort().port());
}

#endif //DEBUGDUMP
