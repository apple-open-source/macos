/*
 * Copyright (c) 2000-2004,2006-2008 Apple Inc. All Rights Reserved.
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
// notifications - handling of securityd-gated notification messages
//
#ifndef _H_NOTIFICATIONS
#define _H_NOTIFICATIONS

#include <security_utilities/mach++.h>
#include <security_utilities/machserver.h>
#include <security_utilities/globalizer.h>
#include <securityd_client/ssclient.h>
#include <map>
#include <queue>

#include "SharedMemoryServer.h"

using MachPlusPlus::Port;
using MachPlusPlus::MachServer;
using SecurityServer::NotificationDomain;
using SecurityServer::NotificationEvent;
using SecurityServer::NotificationMask;

class SharedMemoryListener;

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
class Listener: public RefCount {
public:
	Listener(NotificationDomain domain, NotificationMask events,
		mach_port_t port = MACH_PORT_NULL);	
	virtual ~Listener();

	// inject an event into the notification system
    static void notify(NotificationDomain domain,
		NotificationEvent event, const CssmData &data);
    static void notify(NotificationDomain domain,
		NotificationEvent event, uint32 sequence, const CssmData &data);
    static bool remove(Port port);

    const NotificationDomain domain;
    const NotificationMask events;
	
	bool wants(NotificationEvent event)
	{ return (1 << event) & events; }

protected:
	class Notification : public RefCount {
	public:
		Notification(NotificationDomain domain, NotificationEvent event,
			uint32 seq, const CssmData &data);
		virtual ~Notification();
		
		const NotificationDomain domain;
		const NotificationEvent event;
		const uint32 sequence;
		const CssmAutoData data;
		
		size_t size() const
		{ return data.length(); }	//@@@ add "slop" here for heuristic?
	};
	
	virtual void notifyMe(Notification *message) = 0;
	
public:
	class JitterBuffer {
	public:
		JitterBuffer() : mNotifyLast(0) { }

		bool inSequence(Notification *message);
		RefPointer<Notification> popNotification();
		
	private:
		uint32 mNotifyLast;		// last notification seq processed
		typedef std::map<uint32, RefPointer<Notification> > JBuffer;
		JBuffer mBuffer;		// early messages buffer
	};
	
private:
	static void sendNotification(Notification *message);
    
private:
    typedef multimap<mach_port_t, RefPointer<Listener> > ListenerMap;
    static ListenerMap& listeners;
    static Mutex setLock;
};



class SharedMemoryListener : public Listener, public SharedMemoryServer, public Security::MachPlusPlus::MachServer::Timer
{
protected:
	virtual void action ();
	virtual void notifyMe(Notification *message);

	bool mActive;

public:
	SharedMemoryListener (const char* serverName, u_int32_t serverSize);
	virtual ~SharedMemoryListener ();
};

#endif
