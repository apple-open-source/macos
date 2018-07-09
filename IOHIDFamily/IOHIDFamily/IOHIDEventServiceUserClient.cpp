/*
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
#include <AssertMacros.h>
#include "IOHIDEventServiceUserClient.h"
#include "IOHIDEventServiceQueue.h"
#include "IOHIDEventData.h"
#include "IOHIDEvent.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDDebug.h"
#include <sys/proc.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include "IOHIDFamilyTrace.h"

#define kQueueSizeMin   0
#define kQueueSizeFake  128
#define kQueueSizeMax   16384

#define kIOHIDSystemUserAccessServiceEntitlement "com.apple.hid.system.user-access-service"

//===========================================================================
// IOHIDEventServiceUserClient class

#define super IOUserClient

OSDefineMetaClassAndStructors( IOHIDEventServiceUserClient, IOUserClient )

//==============================================================================
// IOHIDEventServiceUserClient::sMethods
//==============================================================================
const IOExternalMethodDispatch IOHIDEventServiceUserClient::sMethods[kIOHIDEventServiceUserClientNumCommands] = {
    { //    kIOHIDEventServiceUserClientOpen
	(IOExternalMethodAction) &IOHIDEventServiceUserClient::_open,
	1, 0,
    0, 0
    },
    { //    kIOHIDEventServiceUserClientClose
	(IOExternalMethodAction) &IOHIDEventServiceUserClient::_close,
	1, 0,
    0, 0
    },
    { //    kIOHIDEventServiceUserClientCopyEvent
	(IOExternalMethodAction) &IOHIDEventServiceUserClient::_copyEvent,
	2, -1,
    0, -1
    },
    { //    kIOHIDEventServiceUserClientSetElementValue
	(IOExternalMethodAction) &IOHIDEventServiceUserClient::_setElementValue,
	3, 0,
    0, 0
    },
};

enum {
    kUserClientStateOpen  = 0x1,
    kUserClientStateClose = 0x2
};

//==============================================================================
// IOHIDEventServiceUserClient::getService
//==============================================================================
IOService * IOHIDEventServiceUserClient::getService( void )
{
    return _owner;
}

//==============================================================================
// IOHIDEventServiceUserClient::clientClose
//==============================================================================
IOReturn IOHIDEventServiceUserClient::clientClose( void )
{
    terminate();
    
    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::registerNotificationPort
//==============================================================================
IOReturn IOHIDEventServiceUserClient::registerNotificationPort(
                            mach_port_t                 port, 
                            UInt32                      type __unused,
                            UInt32                      refCon __unused )
{
    if (_queue) {
        _queue->setNotificationPort(port);
    }
    return kIOReturnSuccess;
}


//==============================================================================
// IOHIDEventServiceUserClient::clientMemoryForType
//==============================================================================
IOReturn IOHIDEventServiceUserClient::clientMemoryForType(
                                                               UInt32                      type __unused,
                                                               IOOptionBits *              options,
                                                               IOMemoryDescriptor **       memory )
{
  
    IOReturn result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    
    result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceUserClient::clientMemoryForTypeGated), options, memory);
  
exit:
  
    return result;
}

//==============================================================================
// IOHIDEventServiceUserClient::clientMemoryForType
//==============================================================================
IOReturn IOHIDEventServiceUserClient::clientMemoryForTypeGated(
                            IOOptionBits *              options,
                            IOMemoryDescriptor **       memory )
{
    IOReturn ret = kIOReturnNoMemory;
            
    if ( _queue ) {
        IOMemoryDescriptor * memoryToShare = _queue->getMemoryDescriptor();
    
        // if we got some memory
        if (memoryToShare)
        {
            // Memory will be released by user client
            // when last map is destroyed.

            memoryToShare->retain();

            ret = kIOReturnSuccess;
        }
        
        // set the result
        *options = 0;
        *memory  = memoryToShare;
    }
        
    return ret;
}



//==============================================================================
// IOHIDEventServiceUserClient::externalMethod
//==============================================================================
IOReturn IOHIDEventServiceUserClient::externalMethod(
                                                       uint32_t                    selector,
                                                       IOExternalMethodArguments * arguments,
                                                       IOExternalMethodDispatch *  dispatch,
                                                       OSObject *                  target,
                                                       void *                      reference)
{
  ExternalMethodGatedArguments gatedArguments = {selector, arguments, dispatch, target, reference};
  IOReturn result;
  
  require_action(!isInactive(), exit, result=kIOReturnOffline);
  
  result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceUserClient::externalMethodGated), &gatedArguments);
  
exit:

  return result;
}

//==============================================================================
// IOHIDEventServiceUserClient::externalMethodGated
//==============================================================================
IOReturn IOHIDEventServiceUserClient::externalMethodGated(ExternalMethodGatedArguments *arguments)
{
  IOReturn result;
  
  require_action(!isInactive(), exit, result=kIOReturnOffline);
  
  require_action(arguments->selector < (uint32_t) kIOHIDEventServiceUserClientNumCommands, exit, result=kIOReturnBadArgument);
  
  arguments->dispatch = (IOExternalMethodDispatch *) &sMethods[arguments->selector];
  if (!arguments->target)
    arguments->target = this;
  
  result = super::externalMethod(arguments->selector, arguments->arguments, arguments->dispatch, arguments->target, arguments->reference);
  
exit:
  return result;
}


//==============================================================================
// IOHIDEventServiceUserClient::initWithTask
//==============================================================================
bool IOHIDEventServiceUserClient::initWithTask(task_t owningTask, void * security_id __unused, UInt32 type __unused)
{
    bool result = false;
    
    OSObject* entitlement = copyClientEntitlement(owningTask, kIOHIDSystemUserAccessServiceEntitlement);
    if (entitlement) {
        result = (entitlement == kOSBooleanTrue);
        entitlement->release();
    }
    if (!result) {
        proc_t      process;
        process = (proc_t)get_bsdtask_info(owningTask);
        char name[255];
        bzero(name, sizeof(name));
        proc_name(proc_pid(process), name, sizeof(name));
        HIDLogError("%s is not entitled", name);
        goto exit;
    }
    
    result = super::init();
    require_action(result, exit, HIDLogError("failed"));

    _owner        = NULL;
    _commandGate  = NULL;
    _state        = 0;
    _queue        = NULL;
    
exit:
    return result;
}

//==============================================================================
// IOHIDEventServiceUserClient::start
//==============================================================================
bool IOHIDEventServiceUserClient::start( IOService * provider )
{
    OSObject *    object;
    OSNumber *    num;
    uint32_t      queueSize = kQueueSizeMax;
    IOWorkLoop *  workLoop;
    boolean_t     result = false;
    OSSerializer * debugStateSerializer;
  
    require (super::start(provider), exit);
  
    _owner = OSDynamicCast(IOHIDEventService, provider);
    require (_owner, exit);

    _owner->retain();
  
    object = provider->copyProperty(kIOHIDEventServiceQueueSize);
    num = OSDynamicCast(OSNumber, object);
    if ( num ) {
        queueSize = num->unsigned32BitValue();
        queueSize = min(kQueueSizeMax, queueSize);
    }
    OSSafeReleaseNULL(object);
    
    if ( queueSize ) {
        _queue = IOHIDEventServiceQueue::withCapacity(queueSize, getRegistryEntryID());
        require(_queue, exit);
    }
  
    workLoop = getWorkLoop();
    require(workLoop, exit);
  
  
    _commandGate = IOCommandGate::commandGate(this);
    require(_commandGate, exit);
    require(workLoop->addEventSource(_commandGate) == kIOReturnSuccess, exit);
  
    debugStateSerializer = OSSerializer::forTarget(this, OSMemberFunctionCast(OSSerializerCallback, this, &IOHIDEventServiceUserClient::serializeDebugState));
    if (debugStateSerializer) {
        setProperty("DebugState", debugStateSerializer);
        debugStateSerializer->release();
    }
 
    result = true;

exit:
  
    return result;
}

void IOHIDEventServiceUserClient::stop( IOService * provider )
{
    close();
    
    IOWorkLoop * workLoop = getWorkLoop();
  
    if (workLoop && _commandGate) {
        workLoop->removeEventSource(_commandGate);
    }

    super::stop(provider);
}

//==============================================================================
// IOHIDEventServiceUserClient::_open
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_open(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference __unused,
                                IOExternalMethodArguments *     arguments)
{
    return target->open((IOOptionBits)arguments->scalarInput[0]);
}

//==============================================================================
// IOHIDEventServiceUserClient::open
//==============================================================================
IOReturn IOHIDEventServiceUserClient::open(IOOptionBits options)
{
    if (!_owner) {
        return kIOReturnOffline;
    }
    
    if (_state == kUserClientStateOpen) {
        return kIOReturnStillOpen;
    }
    
    
    _options = options;
    
    if (!_owner->open(  this,
                        options | kIOHIDOpenedByEventSystem,
                        NULL, 
                        OSMemberFunctionCast(IOHIDEventService::Action, 
                        this, &IOHIDEventServiceUserClient::eventServiceCallback)) ) {
       return kIOReturnExclusiveAccess;
    }
    
    _state = kUserClientStateOpen;
    
    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::_close
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_close(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference __unused,
                                IOExternalMethodArguments *     arguments __unused)
{
    return target->close();
}

//==============================================================================
// IOHIDEventServiceUserClient::close
//==============================================================================
IOReturn IOHIDEventServiceUserClient::close()
{
    
    
    if (_owner && _state == kUserClientStateOpen) {
        _owner->close(this, _options | kIOHIDOpenedByEventSystem);
        _state = kUserClientStateClose;
    }

    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::_copyEvent
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_copyEvent(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference __unused, 
                                IOExternalMethodArguments *     arguments)
{
    IOHIDEvent *    inEvent     = NULL;
    IOHIDEvent *    outEvent    = NULL;
    IOReturn        ret         = kIOReturnError;
    IOByteCount     length      = 0;
    
    if ( arguments->structureInput && arguments->structureInputSize)
        inEvent = IOHIDEvent::withBytes(arguments->structureInput, arguments->structureInputSize);

    do { 
        outEvent = target->copyEvent((IOHIDEventType)arguments->scalarInput[0], inEvent, (IOOptionBits)arguments->scalarInput[1]);
        
        if ( !outEvent )
            break;
            
        length = outEvent->getLength();
        
        if ( length > arguments->structureOutputSize ) {
            ret = kIOReturnBadArgument;
            break;
        }

        outEvent->readBytes(arguments->structureOutput, length);
        arguments->structureOutputSize = (uint32_t)length;

        ret = kIOReturnSuccess;
    
    } while ( 0 );

    if ( inEvent )
        inEvent->release();
    
    if ( outEvent )
        outEvent->release();
        
    return ret;
}

//==============================================================================
// IOHIDEventServiceUserClient::copyEvent
//==============================================================================
IOHIDEvent * IOHIDEventServiceUserClient::copyEvent(IOHIDEventType type, IOHIDEvent * matching, IOOptionBits options)
{
    return _owner ? _owner->copyEvent(type, matching, options) : NULL;
}

//==============================================================================
// IOHIDEventServiceUserClient::_setElementValue
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_setElementValue(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference __unused,
                                IOExternalMethodArguments *     arguments)
{

    return target->setElementValue((UInt32)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2]);
}

//==============================================================================
// IOHIDEventServiceUserClient::setElementValue
//==============================================================================
IOReturn IOHIDEventServiceUserClient::setElementValue(UInt32 usagePage, UInt32 usage, UInt32 value)
{
    if (_owner) {
        return _owner->setElementValue(usagePage, usage, value);
    }
    return kIOReturnNoDevice;
}

//==============================================================================
// IOHIDEventServiceUserClient::didTerminate
//==============================================================================
bool IOHIDEventServiceUserClient::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{

    close ();

    return super::didTerminate(provider, options, defer);
}

//==============================================================================
// IOHIDEventServiceUserClient::free
//==============================================================================
void IOHIDEventServiceUserClient::free()
{
    OSSafeReleaseNULL(_queue);
    OSSafeReleaseNULL(_owner);
    OSSafeReleaseNULL(_commandGate);
  
    super::free();
}

//==============================================================================
// IOHIDEventServiceUserClient::setProperties
//==============================================================================
IOReturn IOHIDEventServiceUserClient::setProperties( OSObject * properties )
{
    return _owner ? _owner->setProperties(properties) : kIOReturnOffline;
}


//==============================================================================
// IOHIDEventServiceUserClient::eventServiceCallback
//==============================================================================
void IOHIDEventServiceUserClient::eventServiceCallback(
                                IOHIDEventService *             sender __unused,
                                void *                          context __unused,
                                IOHIDEvent *                    event, 
                                IOOptionBits                    options __unused)
{
    if (!_queue || _state != kUserClientStateOpen) {
        return;
    }

#if 0
    if (event && (event->getLatency(kMillisecondScale) > 500)) {
        IOLog("HID dispatch 0x%llx[%d]- high latency %llums", getRegistryEntryID(), (int)event->getType(), event->getLatency(kMillisecondScale));
    }
#endif
    
    _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceUserClient::enqueueEventGated), event);
  
}

//==============================================================================
// IOHIDEventServiceUserClient::enqueueEventGated
//==============================================================================
void IOHIDEventServiceUserClient::enqueueEventGated( IOHIDEvent * event)
{
  //enqueue the event
    if (_queue) {
        ++_eventCount;
        _lastEventTime = mach_continuous_time();
        _lastEventType = event->getType();
        Boolean result = _queue->enqueueEvent(event);
        if (result == false) {
            _lastDroppedEventTime = _lastEventTime;
            ++_droppedEventCount;
            IOHID_DEBUG(kIOHIDDebugCode_HIDEventServiceEnqueueFail, event->getTimeStamp(), 0, 0, 0);
        }
    }
}


//====================================================================================================
// IOHIDEventServiceUserClient::serializeDebugState
//====================================================================================================
bool   IOHIDEventServiceUserClient::serializeDebugState(void * ref __unused, OSSerialize * serializer) {
    bool          result = false;
    uint64_t      currentTime, deltaTime;
    uint64_t      nanoTime;
    OSDictionary  *debugDict = OSDictionary::withCapacity(6);
    OSNumber      *num;
    
    require(debugDict, exit);
    
    currentTime =  mach_continuous_time();
    
    if (_queue) {
        debugDict->setObject("EventQueue", _queue);
    }

    if (_eventCount) {
        num = OSNumber::withNumber(_eventCount, 64);
        if (num) {
            debugDict->setObject("EnqueueEventCount", num);
            OSSafeReleaseNULL(num);
        }
    }
    if (_lastEventTime) {
        deltaTime = AbsoluteTime_to_scalar(&currentTime) - AbsoluteTime_to_scalar(&(_lastEventTime));
        absolutetime_to_nanoseconds(deltaTime, &nanoTime);
        num = OSNumber::withNumber(nanoTime, 64);
        if (num) {
            debugDict->setObject("LastEventTime", num);
            OSSafeReleaseNULL(num);
        }
    }
    if (_lastEventType) {
        num = OSNumber::withNumber(_lastEventType, 32);
        if (num) {
            debugDict->setObject("LastEventType", num);
            OSSafeReleaseNULL(num);
        }
    }
    if (_lastDroppedEventTime) {
        deltaTime = AbsoluteTime_to_scalar(&currentTime) - AbsoluteTime_to_scalar(&(_lastDroppedEventTime));
        absolutetime_to_nanoseconds(deltaTime, &nanoTime);
        num = OSNumber::withNumber(nanoTime, 64);
        if (num) {
            debugDict->setObject("LastDroppedEventTime", num);
            OSSafeReleaseNULL(num);
        }
    }
    if (_droppedEventCount) {
        num = OSNumber::withNumber(_droppedEventCount, 32);
        if (num) {
            debugDict->setObject("DroppedEventCount", num);
            OSSafeReleaseNULL(num);
        }
    }
    result = debugDict->serialize(serializer);
    debugDict->release();

exit:
    return result;
}
