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
// pcscmonitor - use PCSC to monitor smartcard reader/card state for securityd
//
#ifndef _H_PCSCMONITOR
#define _H_PCSCMONITOR

#include "server.h"
#include "tokencache.h"
#include "reader.h"
#include "token.h"
#include "notifications.h"
#include <security_utilities/unixchild.h>
#include <security_utilities/powerwatch.h>
#include <security_utilities/pcsc++.h>
#include <security_utilities/iodevices.h>
#include <set>


//
// A PCSCMonitor uses PCSC to monitor the state of smartcard readers and
// tokens (cards) in the system, and dispatches messages and events to the
// various related players in securityd. There should be at most one of these
// objects active within securityd.
//
class PCSCMonitor : private Listener,
	private MachServer::Timer,
	private IOKit::NotificationPort::Receiver,
	private MachPlusPlus::PowerWatcher,
	private UnixPlusPlus::Child,
	private Mutex {
public:
	enum ServiceLevel {
		forcedOff,					// no service under any circumstances
		conservative,				// launch pcscd for certain smartcard devices
		aggressive,					// launch pcscd for possible (and certain) smartcard devices
		forcedOn,					// keep pcscd running at all times
		externalDaemon				// use externally launched daemon if present (do not manage pcscd)
	};

	PCSCMonitor(Server &server, const char* pathToCache, ServiceLevel level = conservative);

protected:
	void pollReaders();
	void clearReaders();

	Server &server;
	TokenCache *cache;
	std::string cachePath;
	TokenCache& getTokenCache ();

protected:
	// Listener
	void notifyMe(Notification *message);
	
	// MachServer::Timer
	void action();
	
	// NotificationPort::Receiver
	void ioChange(IOKit::DeviceIterator &iterator);
	
	// PowerWatcher
	void systemWillSleep();
	void systemIsWaking();
	
	// Unix++/Child
	void childAction();
	void dying();
	
protected:
	void launchPcscd();
	void scheduleTimer(bool enable);
	void initialSetup();
	void noDeviceTimeout();

	enum DeviceSupport {
		impossible,				// certain this is not a smartcard
		definite,				// definitely a smartcard device
		possible				// perhaps... we're not sure
	};
	DeviceSupport deviceSupport(const IOKit::Device &dev);
	bool isExcludedDevice(const IOKit::Device &dev);

private:
	ServiceLevel mServiceLevel;	// level of service requested/determined
	void (PCSCMonitor::*mTimerAction)(); // what to do when our timer fires	
	bool mGoingToSleep;			// between sleep and wakeup; special timer handling

	PCSC::Session mSession;		// PCSC client session
	IOKit::MachPortNotificationPort mIOKitNotifier; // IOKit connection
		
	typedef map<string, RefPointer<Reader> > ReaderMap;
	typedef set<RefPointer<Reader> > ReaderSet;
	ReaderMap mReaders;			// presently known PCSC Readers (aka slots)
};


#endif //_H_PCSCMONITOR
