/*
 * Copyright (c) 2004-2008,2014 Apple Inc. All Rights Reserved.
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
// pcscmonitor - use PCSC to monitor smartcard reader/card state for securityd
//
#ifndef _H_PCSCMONITOR
#define _H_PCSCMONITOR

#include "server.h"
#include "tokencache.h"
#include "reader.h"
#include "token.h"
#include <security_utilities/pcsc++.h>
#include <security_utilities/coderepository.h>
#include <set>


//
// A PCSCMonitor uses PCSC to monitor the state of smartcard readers and
// tokens (cards) in the system, and dispatches messages and events to the
// various related players in securityd. There should be at most one of these
// objects active within securityd.
//
class PCSCMonitor : private Listener, private MachServer::Timer {
public:
	enum ServiceLevel {
		forcedOff,					// no service under any circumstances
		externalDaemon				// use externally launched daemon if present (do not manage pcscd)
	};

	PCSCMonitor(Server &server, const char* pathToCache, ServiceLevel level = externalDaemon);

protected:
	Server &server;
	TokenCache& tokenCache();

protected:
    // Listener
    void notifyMe(Notification *message);

	// MachServer::Timer
	void action();

public: //@@@@
	void startSoftTokens();
	void loadSoftToken(Bundle *tokendBundle);

private:
	ServiceLevel mServiceLevel;	// level of service requested/determined

	std::string mCachePath;		// path to cache directory
	TokenCache *mTokenCache;	// cache object (lazy)

	typedef map<string, RefPointer<Reader> > ReaderMap;
	typedef set<RefPointer<Reader> > ReaderSet;
	ReaderMap mReaders;		// presently known PCSC Readers (aka slots)

	class Watcher : public Thread {
	public:
		Watcher(Server &server, TokenCache &tokenCache, ReaderMap& readers);

	protected:
		void action();

	private:
		Server &mServer;
		TokenCache &mTokenCache;
		PCSC::Session mSession;		// PCSC client session
		ReaderMap& mReaders;
	};
};


#endif //_H_PCSCMONITOR
