/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// multiobserver - Observer multiplexer
//
#include "multiobserver.h"



namespace Security {
namespace Network {


//
// Manage the observer set
//
void MultiplexObserver::add(Observer &obs)
{
    mObservers.insert(&obs);
    Observer::setEvents(getEvents() | obs.getEvents());
}

void MultiplexObserver::remove(Observer &obs)
{
    mObservers.erase(&obs);
    setEvents();
}


//
// (Re)calculate the event mask
//
void MultiplexObserver::setEvents()
{
    Events eventSet = noEvents;
    for (set<Observer *>::const_iterator it = mObservers.begin(); it != mObservers.end(); it++)
        eventSet |= (*it)->getEvents();
    Observer::setEvents(eventSet);
}


//
// Fan out an observation
//
void MultiplexObserver::observe(Events events, Transfer *xfer, const void *info)
{
    for (set<Observer *>::const_iterator it = mObservers.begin(); it != mObservers.end(); it++)
        if ((*it)->wants(events))
            (*it)->observe(events, xfer, info);
}


}	// end namespace Network
}	// end namespace Security
