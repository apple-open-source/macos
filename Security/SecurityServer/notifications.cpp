/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// EntropyManager - manage entropy on the system.
//
#include "notifications.h"
#include "server.h"
#include "ucspNotify.h"


Listener::ListenerMap Listener::listeners;
Mutex Listener::setLock;


//
// Construct a new Listener and hook it up
//
Listener::Listener(Process &proc, Port receiver, Domain dom, EventMask evs)
    : process(proc), domain(dom), events(evs), mNotificationPort(receiver)
{
    assert(events);		// what's the point?
    
    // register in listener set
    StLock<Mutex> _(setLock);
    listeners.insert(ListenerMap::value_type(receiver, this));
    
    // let's get told when the receiver port dies
    Server::active().notifyIfDead(receiver);
    
    secdebug("notify", "%p created domain %ld events 0x%lx port %d",
        this, domain, events, mNotificationPort.port());
}


//
// Destroy a listener. Cleans up.
//
Listener::~Listener()
{
    secdebug("notify", "%p destroyed", this);
}


//
// Send a single notification for this listener
//
void Listener::notifyMe(Domain domain, Event event, const CssmData &data)
{
    if (domain != this->domain || !(event & events))
        return;		// not interested
    
    secdebug("notify", "%p sending domain %ld event 0x%lx to port %d process %d",
        this, domain, event, mNotificationPort.port(), process.pid());
    
    // send mach message (via MIG simpleroutine)
    if (IFDEBUG(kern_return_t rc =) ucsp_notify_sender_notify(mNotificationPort,
        MACH_SEND_TIMEOUT, 0,
        domain, event, data.data(), data.length(),
        0 /*@@@ placeholder for sender ID */))
        secdebug("notify", "%p send failed (error=%d)", this, rc);
}


//
// Send a notification to all registered listeners
//
void Listener::notify(Domain domain, Event event, const CssmData &data)
{
    for (ListenerMap::const_iterator it = listeners.begin();
            it != listeners.end(); it++)
        it->second->notifyMe(domain, event, data);
}


//
// Handle a port death or deallocation by removing all Listeners using that port.
// Returns true iff we had one.
//
bool Listener::remove(Port port)
{
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
