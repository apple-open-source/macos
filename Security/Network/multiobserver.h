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
#ifndef _H_MULTIPLEXOBSERVER
#define _H_MULTIPLEXOBSERVER

#include <Security/observer.h>
#include <set>


namespace Security {
namespace Network {


//
// A MultipexObserver can be used to "fan out" events to any number of Observers.
// Note that we do not keep an ordering of Observers.
//
class MultiplexObserver : public Observer {
public:
    MultiplexObserver();
    
    void add(Observer &obs);
    void remove(Observer &obs);
    bool contains(Observer &obs);
    
    // call setEvents() if one of the member Observers changes its event set
    // Note: this disallows setEvents(Events) on purpose
    void setEvents();
    
    void add(Observer *obs)		{ assert(obs); add(*obs); }
    void remove(Observer *obs)	{ assert(obs); remove(*obs); }
    
    void observe(Events events, Transfer *xfer, const void *info);

private:
    set<Observer *> mObservers;
};


}	// end namespace Network
}	// end namespace Security


#endif /* _H_MULTIPLEXOBSERVER */
