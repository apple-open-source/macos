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
//
//
#ifndef _H_NOTIFICATIONS
#define _H_NOTIFICATIONS

#include <Security/mach++.h>
#include <Security/globalizer.h>
#include <map>


using namespace MachPlusPlus;


class Process;


//
// A registered receiver of notifications.
// Each one is for a particular database (or all), set of events,
// and to a particular Mach port. A process may have any number
// of listeners, each independent; so that multiple notifications can
// be sent to the same process if it registers repeatedly.
//
class Listener {
public:
    enum {
        lockedEvent                = 1,	// a keychain was locked
        unlockedEvent              = 2,	// a keychain was unlocked
        passphraseChangedEvent	   = 6,	// a keychain password was (possibly) changed
        
        allEvents = lockedEvent | unlockedEvent | passphraseChangedEvent
    };
    typedef uint32 Event, EventMask;
    
    enum {
        allNotifications			= 0, // all domains (useful for testing only)
        databaseNotifications		= 1	// something happened to a database (aka keychain)
    };
    typedef uint32 Domain;
    
public:
    Listener(Process &proc, Port receiver, Domain domain, EventMask evs = allEvents);
    virtual ~Listener();

    Process &process;
    const Domain domain;
    const EventMask events;
    
    virtual void notifyMe(Domain domain, Event event, const CssmData &data);
    static void notify(Domain domain, Event event, const CssmData &data);
    static bool remove(Port port);

protected:
    Port mNotificationPort;
    
private:
    typedef multimap<mach_port_t, Listener *> ListenerMap;
    static ListenerMap listeners;
    static Mutex setLock;
};


#endif //_H_NOTIFICATIONS
