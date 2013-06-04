/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _IOKIT_HID_IOHIDEVENTSYSTEM_H
#define _IOKIT_HID_IOHIDEVENTSYSTEM_H

#include <IOKit/IOMessage.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>
#include "IOHIDEventService.h"
#include "IOHIDEventTypes.h"
#include "IOHIDEvent.h"

class IOHIDEventSystem: public IOService
{
    OSDeclareAbstractStructors( IOHIDEventSystem )

    IOWorkLoop *		_workLoop;
    IOCommandGate *     _commandGate;
    
    IONotifier *		_publishNotify;
    IONotifier *		_terminateNotify;
    OSArray *           _eventServiceInfoArray;
    
    bool                _eventsOpen;
    
    struct ExpansionData { 
    };
    /*! @var reserved
        Reserved for future use.  (Internal use only)  */
    ExpansionData *         _reserved;

    bool notificationHandler(
                                void *                          refCon, 
                                IOService *                     service );
                                
    void handleHIDEvent(
                                void *                          refCon,
                                AbsoluteTime                    timeStamp,
                                UInt32                          eventCount,
                                IOHIDEvent *                    events,
                                IOOptionBits                    options);    
    

    // Gated Methods
    void handleServicePublicationGated(IOService * service);

    void handleServiceTerminationGated(IOService * service);

    void handleHIDEventGated(void * args);
    
    void registerEventSource(IOHIDEventService * service);

public:
    virtual bool      init(OSDictionary * properties = 0);
    virtual bool      start(IOService * provider);
    virtual void      free();
    virtual IOReturn  message(UInt32 type, IOService * provider, void * argument);
    virtual IOReturn  setProperties( OSObject * properties );

};

#endif /* _IOKIT_HID_IOHIDEVENTSYSTEM_H */
