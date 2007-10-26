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
// notifications - handling of securityd-gated notification messages
//
#include <notify.h>

#include "notifications.h"
#include "server.h"
#include "connection.h"
#include <securityd_client/ucspNotify.h>


Listener::ListenerMap Listener::listeners;
Mutex Listener::setLock(Mutex::recursive);


//
// Listener basics
//
Listener::Listener(NotificationDomain dom, NotificationMask evs, mach_port_t port)
	: domain(dom), events(evs)
{
	assert(events);		// what's the point?
	
    // register in listener set
    StLock<Mutex> _(setLock);
    listeners.insert(ListenerMap::value_type(port, this));
	
	secdebug("notify", "%p created for domain 0x%x events 0x%x port %d",
		this, dom, evs, port);
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
	RefPointer<Notification> message = new Notification(domain, event, 0, data);
	StLock<Mutex> _(setLock);
	sendNotification(message);
}

void Listener::notify(NotificationDomain domain,
	NotificationEvent event, uint32 sequence, const CssmData &data)
{
	Connection &current = Server::active().connection();
	RefPointer<Notification> message = new Notification(domain, event, sequence, data);
	if (current.inSequence(message)) {
		StLock<Mutex> _(setLock);
		sendNotification(message);
		while (RefPointer<Notification> next = current.popNotification())
			sendNotification(next);
	}
}

void Listener::sendNotification(Notification *message)
{
    for (ListenerMap::const_iterator it = listeners.begin();
            it != listeners.end(); it++) {
		Listener *listener = it->second;
		if (listener->domain == kNotificationDomainAll || (message->domain == listener->domain && listener->wants(message->event)))
			listener->notifyMe(message);
	}
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

	assert(range.first != listeners.end());
	secdebug("notify", "remove port %d", port.port());
#if !defined(NDEBUG)
    for (Iterator it = range.first; it != range.second; it++) {
		assert(it->first == port);
		secdebug("notify", "%p listener removed", it->second.get());
	}
#endif //NDEBUG
    listeners.erase(range.first, range.second);
	port.destroy();
    return true;	// got it
}


//
// Notification message objects
//
Listener::Notification::Notification(NotificationDomain inDomain,
	NotificationEvent inEvent, uint32 seq, const CssmData &inData)
	: domain(inDomain), event(inEvent), sequence(seq), data(Allocator::standard(), inData)
{
	secdebug("notify", "%p notification created domain 0x%lx event %ld seq %ld",
		this, domain, event, sequence);
}

Listener::Notification::~Notification()
{
	secdebug("notify", "%p notification done domain 0x%lx event %ld seq %ld",
		this, domain, event, sequence);
}


//
// Jitter buffering
//
bool Listener::JitterBuffer::inSequence(Notification *message)
{
	if (message->sequence == mNotifyLast + 1) {	// next in sequence
		mNotifyLast++;			// record next sequence
		return true;			// go ahead
	} else {
		secdebug("notify-jit", "%p out of sequence (last %ld got %ld); buffering",
			message, mNotifyLast, message->sequence);
		mBuffer[message->sequence] = message;	// save for later
		return false;			// hold your fire
	}
}

RefPointer<Listener::Notification> Listener::JitterBuffer::popNotification()
{
	JBuffer::iterator it = mBuffer.find(mNotifyLast + 1);	// have next message?
	if (it == mBuffer.end())
		return NULL;			// nothing here
	else {
		RefPointer<Notification> result = it->second;	// save value
		mBuffer.erase(it);		// remove from buffer
		secdebug("notify-jit", "%p retrieved from jitter buffer", result.get());
		return result;			// return it
	}
}

/*
 * Shared memory listener
 */


SharedMemoryListener::SharedMemoryListener(const char* segmentName, SegmentOffsetType segmentSize) :
	Listener (kNotificationDomainAll, kNotificationAllEvents),
	SharedMemoryServer (segmentName, segmentSize),
	mActive (false)
{
	if (segmentName == NULL)
	{
		secdebug("notify", "Attempted to start securityd with a NULL segmentName");
		exit(1);
	}
}

SharedMemoryListener::~SharedMemoryListener ()
{
}

const double kServerWait = 0.005; // time in seconds before clients will be notified that data is available

void SharedMemoryListener::notifyMe(Notification* notification)
{
	const void* data = notification->data.data();
	UInt32 length = notification->data.length();
	WriteMessage (notification->domain, notification->event, data, length);
	
	if (!mActive)
	{
		Server::active().setTimer (this, Time::Interval(kServerWait));
		mActive = true;
	}
}

void SharedMemoryListener::action ()
{
	secdebug("notify", "Posted notification to clients.");
	notify_post (mSegmentName.c_str ());
	mActive = false;
}
