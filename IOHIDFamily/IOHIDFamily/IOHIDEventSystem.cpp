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

#include "IOHIDEventSystem.h"


typedef struct _EventServiceInfo 
{
    IOHIDEventService * service;

} EventServiceInfo, * EventServiceInfoRef;

typedef struct _HIDEventArgs
{
    void *              refCon;
    AbsoluteTime        timeStamp;
    UInt32              eventCount;
    IOHIDEvent *        events;
    IOOptionBits        options;
} HIDEventArgs, * HIDEventArgsRef;


#define super IOService
OSDefineMetaClassAndStructors(IOHIDEventSystem, super)


//====================================================================================================
// IOHIDEventService::init
//====================================================================================================
bool IOHIDEventSystem::init(OSDictionary * properties)
{
    if ( super::init(properties) == false )
        return false;
        
    _eventServiceInfoArray = OSArray::withCapacity(4);
        
    return true;
    
}

//====================================================================================================
// IOHIDEventService::start
//====================================================================================================
bool IOHIDEventSystem::start(IOService * provider)
{
    if ( super::start(provider) == false )
        return false;
        
    _workLoop       = IOWorkLoop::workLoop();
    _commandGate    = IOCommandGate::commandGate(this);
    
    if ( !_workLoop || !_commandGate )
        return false;
        
    if ( _workLoop->addEventSource(_commandGate) != kIOReturnSuccess )
        return false;

    _publishNotify = addNotification( 
                        gIOPublishNotification, 
                        serviceMatching("IOHIDEventService"),
                        OSMemberFunctionCast(IOServiceNotificationHandler, this, &IOHIDEventSystem::notificationHandler),
                        this, 
                        (void *)OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventSystem::handleServicePublicationGated) );

    _terminateNotify = addNotification( 
                        gIOTerminatedNotification, 
                        serviceMatching("IOHIDEventService"),
                        OSMemberFunctionCast(IOServiceNotificationHandler, this, &IOHIDEventSystem::notificationHandler),
                        this, 
                        (void *)OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventSystem::handleServiceTerminationGated) );

    if (!_publishNotify || !_terminateNotify) 
        return false;
        
    _eventsOpen = true;
    
    registerService();
    
    return true;
}

//====================================================================================================
// IOHIDEventService::free
//====================================================================================================
void IOHIDEventSystem::free()
{
	// we are going away. stop the workloop.
	if (workLoop)
    {
        workLoop->disableAllEventSources();
    }

    if ( _publishNotify )
    {
        _publishNotify->remove();
        _publishNotify = 0;
    }
    
    if ( _terminateNotify )
    {
        _terminateNotify->remove();
        _terminateNotify = 0;
    }

    if ( _eventServiceInfoArray )
    {
        _eventServiceInfoArray->release();
        _eventServiceInfoArray = 0;
    }
    
    if ( _commandGate )
    {
        _commandGate->release();
        _commandGate = 0;
    }
    
    if ( _workLoop )
    {
        _workLoop->release();
        _workLoop = 0;
    }
    super::free();
}

//====================================================================================================
// IOHIDEventService::message
//====================================================================================================
IOReturn IOHIDEventSystem::message(UInt32 type, IOService * provider, void * argument)
{
    return super::message(type, provider, argument);
}

//====================================================================================================
// IOHIDEventService::setProperties
//====================================================================================================
IOReturn IOHIDEventSystem::setProperties( OSObject * properties )
{
    return super::setProperties(properties);
}

//====================================================================================================
// IOHIDEventService::notificationHandler
//====================================================================================================
bool IOHIDEventSystem::notificationHandler( void * refCon,  IOService * service )
{
    IOLog("IOHIDEventSystem::notificationHandler\n");

    _commandGate->runAction((IOCommandGate::Action)refCon, service);
    
    return true;
}

//====================================================================================================
// IOHIDEventService::handleServicePublicationGated
//====================================================================================================
void IOHIDEventSystem::handleServicePublicationGated(IOService * service)
{
    IOLog("IOHIDEventSystem::handleServicePublicationGated\n");

    EventServiceInfo    tempEventServiceInfo;
    OSData *            tempData;
    IOHIDEventService * eventService;
    
    if ( !(eventService = OSDynamicCast(IOHIDEventService, service)) )
        return;
    
    attach( eventService );

    tempEventServiceInfo.service = eventService;
    
    tempData = OSData::withBytes(&tempEventServiceInfo, sizeof(EventServiceInfo));
    
    if ( tempData )
    {
        _eventServiceInfoArray->setObject(tempData);
        tempData->release();
    }
    
    if ( _eventsOpen )
        registerEventSource( eventService );
        
}

//====================================================================================================
// IOHIDEventService::handleServiceTerminationGated
//====================================================================================================
void IOHIDEventSystem::handleServiceTerminationGated(IOService * service)
{
    EventServiceInfoRef tempEventServiceInfoRef;
    OSData *            tempData;
    UInt32              index;

    IOLog("IOHIDEventSystem::handleServiceTerminationGated\n");
    
    if ( _eventsOpen )
        service->close(this);

    for ( index = 0; index<_eventServiceInfoArray->getCount(); index++ )
    {
        if ( (tempData = OSDynamicCast(OSData, _eventServiceInfoArray->getObject(index)))
            && (tempEventServiceInfoRef = (EventServiceInfoRef)tempData->getBytesNoCopy())
            && (tempEventServiceInfoRef->service == service) )
        {
            _eventServiceInfoArray->removeObject(index);
            break;
        }
    }
        
    detach(service);
}

//====================================================================================================
// IOHIDEventService::registerEventSource
//====================================================================================================
void IOHIDEventSystem::registerEventSource(IOHIDEventService * service)
{
    EventServiceInfoRef tempEventServiceInfoRef;
    OSData *            tempData = NULL;
    UInt32              index;

    IOLog("IOHIDEventSystem::registerEventSource\n");

    for ( index = 0; index<_eventServiceInfoArray->getCount(); index++ )
    {
        if ( (tempData = OSDynamicCast(OSData, _eventServiceInfoArray->getObject(index)))
            && (tempEventServiceInfoRef = (EventServiceInfoRef)tempData->getBytesNoCopy())
            && (tempEventServiceInfoRef->service == service) )
            break;
        
        tempData = NULL;
    }

    service->open(this, 0, tempData, 
                OSMemberFunctionCast(IOHIDEventService::HIDEventCallback, this, &IOHIDEventSystem::handleHIDEvent));
}


//====================================================================================================
// IOHIDEventService::handleHIDEvent
//====================================================================================================
void IOHIDEventSystem::handleHIDEvent(
                            void *                          refCon,
                            AbsoluteTime                    timeStamp,
                            UInt32                          eventCount,
                            IOHIDEvent *                    events,
                            IOOptionBits                    options)
{
    IOLog("IOHIDEventSystem::handleHIDEvent\n");
    
    HIDEventArgs args;
    
    args.refCon     = refCon;
    args.timeStamp  = timeStamp;
    args.eventCount = eventCount;
    args.events     = events;
    args.options    = options;
    
    _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventSystem::handleHIDEventGated), (void *)&args);
}

//====================================================================================================
// IOHIDEventService::handleHIDEventGated
//====================================================================================================
void IOHIDEventSystem::handleHIDEventGated(void * args)
{    
    HIDEventArgsRef eventArgsRef = (HIDEventArgsRef)args;
    
    if ( !eventArgsRef->events ) 
    {
        IOLog("IOHIDEventSystem::handleHIDEventGated: type=%d timestamp=%lld\n", 0, *((UInt64 *)&(eventArgsRef->timeStamp)));
        return;
    }
    
    IOLog("IOHIDEventSystem::handleHIDEventGated: eventCount=%d timestamp=%lld\n", eventArgsRef->eventCount, *((UInt64 *)&(eventArgsRef->timeStamp)));
    for ( UInt32 i=0; i<eventArgsRef->eventCount; i++)
    {
        IOHIDEvent * event = &(eventArgsRef->events[i]);

        IOLog("IOHIDEventSystem::handleHIDEventGated: type=%d", event->type);
        switch ( event->type )
        {
            case kIOHIDKeyboardEvent:
                IOLog(" usagePage=%x usage=%x value=%d repeat=%d", event->data.keyboard.usagePage, event->data.keyboard.usage, event->data.keyboard.value, event->data.keyboard.repeat);
                break;
                
            case kIOHIDMouseEvent:
                IOLog(" buttons=%x dx=%d dy=%d", event->data.mouse.buttons, event->data.mouse.dx, event->data.mouse.dy);
                break;
                
            case kIOHIDScrollEvent:
                IOLog(" deltaAxis1=%d deltaAxis2=%d deltaAxis3=%d", event->data.scroll.lines.deltaAxis1, event->data.scroll.lines.deltaAxis2, event->data.scroll.lines.deltaAxis3);
                break;
                
            default:
                break;
        }
        IOLog("\n");
    }
    
}
