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
// observer - notification client for network events
//
#ifndef _H_OBSERVER
#define _H_OBSERVER

#include <Security/utilities.h>


namespace Security {
namespace Network {


class Transfer;


//
// An Observer object has a set (bitmask) of events it is interested in.
// Observers are registered with Transfer and Manager objects to take effect.
//
class Observer {
public:
    virtual ~Observer();
    
public:
    enum {
        noEvents						= 0x000000,		// mask for no events
        transferStarting				= 0x000001,		// starting transfer operation
        transferComplete				= 0x000002,		// successfully finished
        transferFailed					= 0x000004,		// failed somehow
        connectEvent					= 0x000800,		// transport level connection done or failed
        protocolSend					= 0x001000,		// low-level protocol message sent
        protocolReceive					= 0x002000,		// low-level protocol message received

        //@@@ questionable
        resourceFound					= 0x000008,		// resource found, OK to continue
        downloading						= 0x000010,		// downloading in progress
        aborting						= 0x000020,		// abort in progress
        dataAvailable					= 0x000040,		// data ready to go
        systemEvent						= 0x000080,		// ???
        percentEvent					= 0x000100,		// a >= 1% data move has occurred
        periodicEvent					= 0x000200,		// call every so often (.25 sec)
        propertyChangedEvent			= 0x000400,
        resultCodeReady					= 0x004000,		// result code has been received by HTTP
        uploading						= 0x008000,		// uploading
        
        allEvents						= 0xFFFFFFFF	// mask for all events
    };
    typedef uint32 Event, Events;
    
    void setEvents(Events mask)		{ mEventMask = mask; }
    Events getEvents() const		{ return mEventMask; }
    bool wants(Events events) const	{ return mEventMask & events; }
    
    virtual void observe(Events events, Transfer *xfer, const void *info = NULL) = 0;
    
private:
    Events mEventMask;
};


}	// end namespace Network
}	// end namespace Security


#endif _H_OBSERVER
