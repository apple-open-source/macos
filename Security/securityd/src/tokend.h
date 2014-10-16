/*
 * Copyright (c) 2004-2006 Apple Computer, Inc. All Rights Reserved.
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
#ifndef _H_TOKEND
#define _H_TOKEND

#include "structure.h"
#include "child.h"
#include "tokencache.h"
#include <security_utilities/pcsc++.h>
#include <security_utilities/osxcode.h>
#include <security_tokend_client/tdclient.h>


//
// A Mix-in for classes that can receive (progated) fault nofications
//
class FaultRelay {
public:
	virtual ~FaultRelay();
	virtual void relayFault(bool async) = 0;
};


//
// A TokenDaemon object is the ServerChild object representing the real
// tokend process driving a token. It provides the only (official) communications
// and control point between securityd and that tokend.
//
// TokenDaemon is sufficiently aware to track changes in its tokend, particularly
// any sudden, violent, agonizing death it may have suffered.
// If TokenDaemon communications with its tokend break down for any rason, it declares
// a FAULT condition and cuts off any further attempts at communication. There is no way
// to recover from a FAULT condition. (You can create a new TokenDaemon and try again,
// of course.) Fault is propagated to the owner object through a simple callback scheme.
//
// If TokenDaemon is destroyed while its process is still alive, it will (try to) kill
// it right there and then. That's good enough for hard error recovery, though you may
// try to let it down easier to allow it to save its caches and wind down. Caller's choice.
//
// NB: If you ever want to make TokenDaemon BE a Bundle, you must switch NodeCore
// AND OSXCode to virtually derive RefCount.
//
class TokenDaemon : public PerGlobal, public ServerChild, public Tokend::ClientSession {
public:
	TokenDaemon(RefPointer<Bundle> code,
		const std::string &reader, const PCSC::ReaderState &state, TokenCache &cache);
	virtual ~TokenDaemon();
	
	bool faulted() const { return mFaulted; }
	void fault(bool async, const char *reason);
	
	void faultRelay(FaultRelay *rcv)		{ mFaultRelay = rcv; }
	
	string bundlePath() const { return mMe->canonicalPath(); }
	string bundleIdentifier() const { return mMe->identifier(); }
	uint32 maxScore() const;

	Score score() const			{ return mScore; }
	bool hasTokenUid() const	{ return !mTokenUid.empty(); }
	std::string tokenUid() const;
	
	uid_t uid() const			{ return mUid; }
	gid_t gid() const			{ return mGid; }

	// startup phase calls
	using ClientSession::probe;
	bool probe();

	IFDUMP(void dumpNode());

protected:
	void childAction();
	void dying();
	
	void fault();				// relay from Tokend::ClientSession

private:
	RefPointer<Bundle> mMe; // code object for the tokend (it's an Application)
	std::string mReaderName;	// PCSC name of reader we're working with
	PCSC::ReaderState mState;	// card state at time of creation (not updated after that)

	// fault processing
	FaultRelay *mFaultRelay;	// forward initial fault declarations to this object
	bool mFaulted;				// fault condition

	// returned by tokend scoring system
	bool mProbed;				// probe() has succeeded; mScore/mTokenUid valid
	Score mScore;				// token support score as returned by probe()
	std::string mTokenUid;		// tokenUid as returned by probe(), may be empty

	// credentials of underlying process
	uid_t mUid;					// uid of tokend process
	gid_t mGid;					// gid of tokend process
};


#endif //_H_TOKEND
