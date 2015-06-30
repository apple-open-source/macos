/*
 * Copyright (c) 2004-2008,2011,2014 Apple Inc. All Rights Reserved.
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
// PCSCMonitor is the "glue" between PCSC and the securityd objects representing
// smartcard-related things. Its job is to manage the daemon and translate real-world
// events (such as card and device insertions) into the securityd object web.
//
#include "pcscmonitor.h"
#include <security_utilities/logging.h>

//
// Construct a PCSCMonitor.
// We strongly assume there's only one of us around here.
//
// Note that this constructor may well run before the server loop has started.
// Don't call anything here that requires an active server loop (like Server::active()).
// In fact, you should push all the hard work into a timer, so as not to hold up the
// general startup process.
//
PCSCMonitor::PCSCMonitor(Server &server, const char* pathToCache, ServiceLevel level)
	: Listener(kNotificationDomainPCSC, SecurityServer::kNotificationAllEvents),
      server(server),
	  mServiceLevel(level),
      MachServer::Timer(true),
	  mCachePath(pathToCache),
	  mTokenCache(NULL)
{
	// do all the smartcard-related work once the event loop has started
	server.setTimer(this, Time::now());		// ASAP
}

PCSCMonitor::Watcher::Watcher(Server &server, TokenCache &tokenCache, ReaderMap& readers)
  : mServer(server), mTokenCache(tokenCache), mReaders(readers)
{}

//
// Poll PCSC for smartcard status.
// We are enumerating all readers on each call.
//
void PCSCMonitor::Watcher::action()
{
    // Associate this watching thread with the server, so that it is possible to call
    // Server::active() from inside code called from this thread.
    mServer.associateThread();

    try {
        // open PCSC session
        mSession.open();

        // Array of states, userData() points to associated Reader instance,
        // name points to string held by Reader::name attribute.
        vector<PCSC::ReaderState> states;

        for (;;) {
            // enumerate all current readers.
            vector<string> names;
            mSession.listReaders(names);
            secdebug("pcsc", "%ld reader(s) in system", names.size());

            // Update PCSC states array with new/removed readers.
            for (vector<PCSC::ReaderState>::iterator stateIt = states.begin(); stateIt != states.end(); ) {
                Reader *reader = stateIt->userData<Reader>();
                vector<string>::iterator nameIt = find(names.begin(), names.end(), reader->name());
                if (nameIt == names.end()) {
                    // Reader was removed from the system.
                    if (Reader *reader = stateIt->userData<Reader>()) {
                        secdebug("pcsc", "removing reader %s", stateIt->name());
                        Syslog::notice("Token reader %s removed from system", stateIt->name());
                        reader->kill();						// prepare to die
                        mReaders.erase(reader->name());		// remove from reader map
                        stateIt = states.erase(stateIt);
                    }
                } else {
                    // This reader is already tracked, copy its signalled state into the last known state.
                    stateIt->lastKnown(stateIt->state());
                    names.erase(nameIt);
                    stateIt++;
                }
            }

            // Add states for remaining (newly appeared) reader names.
            for (vector<string>::iterator it = names.begin(); it != names.end(); ++it) {
                PCSC::ReaderState state;
                state.clearPod();
                state.set(it->c_str());
                states.push_back(state);
            }

            // Now ask PCSC for status changes, and wait for them.
            mSession.statusChange(states, INFINITE);
            
            // Go through the states and notify changed readers.
            for (vector<PCSC::ReaderState>::iterator stateIt = states.begin(); stateIt != states.end(); stateIt++) {
                Reader *reader = stateIt->userData<Reader>();
                if (!reader) {
                    reader = new Reader(mTokenCache, *stateIt);
                    stateIt->userData<Reader>() = reader;
                    stateIt->name(reader->name().c_str());
                    mReaders.insert(make_pair(reader->name(), reader));
                    Syslog::notice("Token reader %s inserted into system", stateIt->name());
                }

                // if PCSC flags a change, notify the Reader
                if (stateIt->changed()) {
                    Syslog::notice("reader %s: state changed %lu -> %lu", stateIt->name(), stateIt->lastKnown(), stateIt->state());
                    try {
                        reader->update(*stateIt);
                    } catch (const exception &e) {
                        Syslog::notice("Token in reader %s: %s", stateIt->name(), e.what());
                    }
                }
            }

            //wakeup mach server to process notifications
            ClientSession session(Allocator::standard(), Allocator::standard());
            session.postNotification(kNotificationDomainPCSC, kNotificationPCSCStateChange, CssmData());
        }
    } catch (const exception &e) {
        Syslog::error("An error '%s' occured while tracking token readers", e.what());
    }
}

TokenCache& PCSCMonitor::tokenCache()
{
	if (mTokenCache == NULL)
		mTokenCache = new TokenCache(mCachePath.c_str());
	return *mTokenCache;
}

//
// Event notifier.
// These events are sent by pcscd for our (sole) benefit.
//
void PCSCMonitor::notifyMe(Notification *message)
{
}

//
// Timer action. Perform the initial PCSC subsystem initialization.
// This runs (shortly) after securityd is fully functional and the
// server loop has started.
//
void PCSCMonitor::action()
{
    switch (mServiceLevel) {
        case forcedOff:
            secdebug("pcsc", "smartcard operation is FORCED OFF");
            break;

        case externalDaemon:
            secdebug("pcsc", "using PCSC");
            startSoftTokens();

            // Start PCSC reader watching thread.
            (new Watcher(server, tokenCache(), mReaders))->run();
            break;
    }
}


//
// Software token support
//
void PCSCMonitor::startSoftTokens()
{
	// scan for new ones
	CodeRepository<Bundle> candidates("Security/tokend", ".tokend", "TOKENDAEMONPATH", false);
	candidates.update();
	for (CodeRepository<Bundle>::iterator it = candidates.begin(); it != candidates.end(); ++it) {
		if (CFTypeRef type = (*it)->infoPlistItem("TokendType"))
			if (CFEqual(type, CFSTR("software")))
				loadSoftToken(*it);
	}
}

void PCSCMonitor::loadSoftToken(Bundle *tokendBundle)
{
	try {
		string bundleName = tokendBundle->identifier();
		
		// prepare a virtual reader, removing any existing one (this would kill a previous tokend)
		assert(mReaders.find(bundleName) == mReaders.end());	// not already present
		RefPointer<Reader> reader = new Reader(tokenCache(), bundleName);

		// now launch the tokend
		RefPointer<TokenDaemon> tokend = new TokenDaemon(tokendBundle,
			reader->name(), reader->pcscState(), reader->cache);
		
		if (tokend->state() == ServerChild::dead) {	// ah well, this one's no good
			secdebug("pcsc", "softtoken %s tokend launch failed", bundleName.c_str());
			Syslog::notice("Software token %s failed to run", tokendBundle->canonicalPath().c_str());
			return;
		}
		
		// probe the (single) tokend
		if (!tokend->probe()) {		// non comprende...
			secdebug("pcsc", "softtoken %s probe failed", bundleName.c_str());
			Syslog::notice("Software token %s refused operation", tokendBundle->canonicalPath().c_str());
			return;
		}
		
		// okay, this seems to work. Set it up
		mReaders.insert(make_pair(reader->name(), reader));
		reader->insertToken(tokend);
		Syslog::notice("Software token %s activated", bundleName.c_str());
	} catch (...) {
		secdebug("pcsc", "exception loading softtoken %s - continuing", tokendBundle->identifier().c_str());
	}
}
