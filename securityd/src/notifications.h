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
//
//
#ifndef _H_NOTIFICATIONS
#define _H_NOTIFICATIONS

#include <security_utilities/mach++.h>
#include <security_utilities/globalizer.h>
#include <securityd_client/ssclient.h>
#include <map>

using MachPlusPlus::Port;
using SecurityServer::NotificationDomain;
using SecurityServer::NotificationEvent;
using SecurityServer::NotificationMask;


//
// A registered receiver of notifications.
// This is an abstract class; you must subclass to define notifyMe().
//
// All Listeners in existence are collected in an internal map of ports to
// Listener*s, which makes them eligible to have events delivered to them via
// their notifyMe() method. There are (only) two viable lifetime management
// strategies for your Listener subclass:
// (1) Eternal: don't ever destroy your Listener. All is well. By convention,
// such Listeners use the null port.
// (2) Port-based: To get rid of your Listeners, call Listener::remove(port),
// which will delete(!) all Listeners constructed with that port.
// Except for the remove() functionality, Listener does not interpret the port.
//
// If you need another Listener lifetime management strategy, you will probably
// have to change things around here.
//
class Listener {
public:
	Listener(Port port, NotificationDomain domain, NotificationMask events);
	Listener(NotificationDomain domain, NotificationMask events);	
	virtual ~Listener();

	// inject an event into the notification system
    static void notify(NotificationDomain domain,
		NotificationEvent event, const CssmData &data);
    static bool remove(Port port);
	
	// consume an event for this Listener
	virtual void notifyMe(NotificationDomain domain,
		NotificationEvent event, const CssmData &data) = 0;

    const NotificationDomain domain;
    const NotificationMask events;
	
	bool wants(NotificationEvent event)
	{ return (1 << event) & events; }

protected:
    Port mPort;

private:
	void setup();
    
private:
    typedef multimap<mach_port_t, Listener *> ListenerMap;
    static ListenerMap listeners;
    static Mutex setLock;
};


//
// A registered receiver of notifications.
// Each one is for a particular database (or all), set of events,
// and to a particular Mach port. A process may have any number
// of listeners, each independent; so that multiple notifications can
// be sent to the same process if it registers repeatedly.
//
class Process;

class ProcessListener : public Listener {
public:
    ProcessListener(Process &proc, Port receiver, NotificationDomain domain,
		NotificationMask evs = SecurityServer::kNotificationAllEvents);

    Process &process;
    
	void notifyMe(NotificationDomain domain,
		NotificationEvent event, const CssmData &data);
};


#endif //_H_NOTIFICATIONS
