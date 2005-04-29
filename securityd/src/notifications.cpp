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
// EntropyManager - manage entropy on the system.
//
#include "notifications.h"
#include "server.h"
#include <securityd_client/ucspNotify.h>


Listener::ListenerMap Listener::listeners;
Mutex Listener::setLock;


Listener::Listener(Port port, NotificationDomain dom, NotificationMask evs)
	: domain(dom), events(evs), mPort(port)
{ setup(); }

Listener::Listener(NotificationDomain dom, NotificationMask evs)
	: domain(dom), events(evs)
{ setup(); }

void Listener::setup()
{
	assert(events);		// what's the point?
    
    // register in listener set
    StLock<Mutex> _(setLock);
    listeners.insert(ListenerMap::value_type(mPort, this));
}

Listener::~Listener()
{
    secdebug("notify", "%p destroyed", this);
}


//
// Send a notification to all registered listeners
//
void Listener::notify(NotificationDomain domain,
	NotificationEvent event, const CssmData &data)
{
    for (ListenerMap::const_iterator it = listeners.begin();
            it != listeners.end(); it++) {
		Listener *listener = it->second;
		if (listener->domain == domain && listener->wants(event))
			listener->notifyMe(domain, event, data);
	}
}


//
// Handle a port death or deallocation by removing all Listeners using that port.
// Returns true iff we had one.
//
bool Listener::remove(Port port)
{
	assert(port);  // Listeners with null ports are eternal
    typedef ListenerMap::iterator Iterator;
    StLock<Mutex> _(setLock);
    pair<Iterator, Iterator> range = listeners.equal_range(port);
    if (range.first == range.second)
        return false;	// not one of ours

    for (Iterator it = range.first; it != range.second; it++)
        delete it->second;
    listeners.erase(range.first, range.second);
	port.destroy();
    return true;	// got it
}


//
// Construct a new Listener and hook it up
//
ProcessListener::ProcessListener(Process &proc, Port receiver,
	NotificationDomain dom, NotificationMask evs)
    : Listener(receiver, dom, evs), process(proc)
{
    // let's get told when the receiver port dies
    Server::active().notifyIfDead(mPort);
    
    secdebug("notify", "%p created domain %ld events 0x%lx port %d",
        this, domain, events, mPort.port());
}


//
// Send a single notification for this listener
//
void ProcessListener::notifyMe(NotificationDomain domain,
	NotificationEvent event, const CssmData &data)
{
    secdebug("notify", "%p sending domain %ld event 0x%lx to port %d process %d",
        this, domain, event, mPort.port(), process.pid());
    
    // send mach message (via MIG simpleroutine)
    if (IFDEBUG(kern_return_t rc =) ucsp_notify_sender_notify(mPort,
        domain, event, data.data(), data.length(),
        0 /*@@@ placeholder for sender ID */))
        secdebug("notify", "%p send failed (error=%d)", this, rc);
}
