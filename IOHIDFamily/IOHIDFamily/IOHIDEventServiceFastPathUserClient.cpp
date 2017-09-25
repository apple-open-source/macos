/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2017 Apple Computer, Inc.  All Rights Reserved.
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
#include "IOHIDEventServiceFastPathUserClient.h"
#include "IOHIDEventServiceQueue.h"
#include "IOHIDEvent.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDDebug.h"
#include <IOKit/hidsystem/IOHIDShared.h>
#include <stdatomic.h>

#define kQueueSizeMin   0
#define kQueueSizeFake  128
#define kQueueSizeMax   16384

#define kCloseSleepDeadlineMs 5

//===========================================================================
// IOHIDEventServiceFastPathUserClient class

#define super IOUserClient

OSDefineMetaClassAndStructors( IOHIDEventServiceFastPathUserClient, IOUserClient )

//==============================================================================
// IOHIDEventServiceFastPathUserClient::sMethods
//==============================================================================
const IOExternalMethodDispatch IOHIDEventServiceFastPathUserClient::sMethods[kIOHIDEventServiceFastPathUserClientNumCommands] = {
    [kIOHIDEventServiceFastPathUserClientOpen] = { //    kIOHIDEventServiceFastPathUserClientOpen
	(IOExternalMethodAction) &IOHIDEventServiceFastPathUserClient::_open,
	1, -1,
    0,  0
    },
    [kIOHIDEventServiceFastPathUserClientClose] = { //    kIOHIDEventServiceFastPathUserClientClose
	(IOExternalMethodAction) &IOHIDEventServiceFastPathUserClient::_close,
	1, 0,
    0, 0
    },
    [kIOHIDEventServiceFastPathUserClientCopyEvent] = { //    kIOHIDEventServiceFastPathUserClientCopyEvent
	(IOExternalMethodAction) &IOHIDEventServiceFastPathUserClient::_copyEvent,
	2, -1,
    0, -1
    },
};

//==============================================================================
// IOHIDEventServiceFastPathUserClient::getService
//==============================================================================
IOService * IOHIDEventServiceFastPathUserClient::getService( void )
{
    return this;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::clientClose
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::clientClose( void )
{
    terminate();
    
    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::clientMemoryForType
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::clientMemoryForType(UInt32 type __unused, IOOptionBits * options, IOMemoryDescriptor ** memory)
{
  
    IOReturn result;
    
    require_action(!isOpened() || !isInactive(), exit, result = kIOReturnOffline);
    
    result = _commandGate->runAction(
                                     OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceFastPathUserClient::clientMemoryForTypeGated),
                                     options,
                                     memory
                                     );
  
exit:
  
    return result;
}
//==============================================================================
// IOHIDEventServiceFastPathUserClient::clientMemoryForTypeGated
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::clientMemoryForTypeGated (IOOptionBits * options, IOMemoryDescriptor ** memory)
{
    IOReturn result = kIOReturnNoMemory;
            
    if ( _queue ) {
        IOMemoryDescriptor * memoryToShare = _queue->getMemoryDescriptor();
    
        // if we got some memory
        if (memoryToShare)
        {
            // Memory will be released by user client
            // when last map is destroyed.

            memoryToShare->retain();

            result = kIOReturnSuccess;
        }
        
        // set the result
        *options = 0;
        *memory  = memoryToShare;
    }
        
    return result;
}
//==============================================================================
// IOHIDEventServiceFastPathUserClient::externalMethod
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::externalMethod(
                                                     uint32_t                    selector,
                                                     IOExternalMethodArguments * arguments,
                                                     IOExternalMethodDispatch *  dispatch,
                                                     OSObject *                  target,
                                                     void *                      reference)
{
    IOReturn result;
    ExternalMethodGatedArguments gatedArguments = {selector, arguments, dispatch, target, reference};
    
    require_action(!isInactive(), exit, result = kIOReturnOffline);
    
    if (selector != kIOHIDEventServiceFastPathUserClientCopyEvent) {
        result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceFastPathUserClient::externalMethodGated), &gatedArguments);
    } else {
        result = externalMethodGated(&gatedArguments);
    }
    
exit:
    
    return result;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::externalMethodGated
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::externalMethodGated(ExternalMethodGatedArguments *arguments)
{
    IOReturn result;
    
    require_action(!isInactive(), exit, result = kIOReturnOffline);
    
    require_action(arguments->selector < (uint32_t) kIOHIDEventServiceFastPathUserClientNumCommands, exit, result=kIOReturnBadArgument);
    
    arguments->dispatch = (IOExternalMethodDispatch *) &sMethods[arguments->selector];
    if (!arguments->target) {
        arguments->target = this;
    }
    result = super::externalMethod(arguments->selector, arguments->arguments, arguments->dispatch, arguments->target, arguments->reference);
    
exit:
    
    return result;
}

//==============================================================================
// IOHIDEventServiceUserClient::initWithTask
//==============================================================================
bool IOHIDEventServiceFastPathUserClient::initWithTask(task_t owningTask __unused, void * security_id __unused, UInt32 type __unused)
{
    bool result;
    
    result = super::init();
    
    require_action(result, exit, HIDLogError("IOHIDEventServiceFastPathUserClient failed to start"));
    
    _owner        = NULL;
    _commandGate  = NULL;
    _queue        = NULL;
    _state        = 0;
    
exit:
    return result;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::start
//==============================================================================
bool IOHIDEventServiceFastPathUserClient::start( IOService * provider )
{
    IOWorkLoop *    workLoop;
    boolean_t       result = false;
    OSSerializer *  debugStateSerializer;
    
    require (super::start(provider), exit);
    
    _owner = OSDynamicCast(IOHIDEventService, provider);
    require (_owner, exit);
    
    _owner->retain();
 
    workLoop = getWorkLoop();
    require(workLoop, exit);
  
    _commandGate = IOCommandGate::commandGate(this);
    require(_commandGate, exit);
    require(workLoop->addEventSource(_commandGate) == kIOReturnSuccess, exit);

    debugStateSerializer = OSSerializer::forTarget(
                              this,
                              OSMemberFunctionCast(OSSerializerCallback,
                              this,
                              &IOHIDEventServiceFastPathUserClient::serializeDebugState)
                              );
    if (debugStateSerializer) {
        setProperty("DebugState", debugStateSerializer);
        debugStateSerializer->release();
    }
    result = true;
exit:
    
    return result;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::stop
//==============================================================================
void IOHIDEventServiceFastPathUserClient::stop( IOService * provider )
{
    close();
    
    IOWorkLoop * workLoop = getWorkLoop();
  
    if (workLoop && _commandGate) {
        workLoop->removeEventSource(_commandGate);
    }

    super::stop(provider);
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::_open
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::_open(
                                IOHIDEventServiceFastPathUserClient *   target,
                                void *                                  reference __unused,
                                IOExternalMethodArguments *             arguments)
{
    OSObject * object = NULL;
    OSDictionary * property = NULL;
    if (arguments->structureInput && arguments->structureInputSize) {
        object = OSUnserializeXML( (const char *) arguments->structureInput, arguments->structureInputSize);
        if (object) {
            property = OSDynamicCast (OSDictionary, object);
            if (!property) {
                object->release();
            }
        }
    }
    
    IOReturn result = target->open((IOOptionBits)arguments->scalarInput[0], property);
  
    OSSafeReleaseNULL(property);
    
    return result;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::open
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::open(IOOptionBits  options, OSDictionary * properties)
{
    IOReturn result = kIOReturnSuccess;
    
    if (!_owner) {
        return kIOReturnOffline;
    }
    
    if (isOpened()) {
        return kIOReturnExclusiveAccess;
    }
    
    _options = options;
    
    if (!_owner->openForClient(this, 0, properties, &_clientContext)) {
       return kIOReturnExclusiveAccess;
    }
	
    atomic_fetch_or ((_Atomic uint32_t *)&_state, kStateOpen);
    
    uint32_t queueSize = 0;
    OSObject * value = _owner->copyPropertyForClient(kIOHIDEventServiceQueueSize, _clientContext);
    OSNumber * num = OSDynamicCast(OSNumber, value);
    if (num) {
        queueSize = num->unsigned32BitValue();
        queueSize = min(kQueueSizeMax, queueSize);
    }
    OSSafeReleaseNULL(value);
    
    if (queueSize) {
        _queue = IOHIDEventServiceQueue::withCapacity(queueSize, getRegistryEntryID());
        if (!_queue) {
            result = kIOReturnNoMemory;
        }
    }

    return result;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::_close
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::_close (
                                IOHIDEventServiceFastPathUserClient *   target,
                                void *                                  reference __unused,
                                IOExternalMethodArguments *             arguments __unused)
{
    return target->close();
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::close
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::close ()
{
    if (_owner && isOpened ()) {
        uint32_t state = atomic_fetch_or((_Atomic uint32_t *)&_state, kStateClosing);
      
        //If state == kStateOpen we can close immediately
        if (state != kStateOpen) {
            do {
                AbsoluteTime deadline = 0;
                nanoseconds_to_absolutetime(kCloseSleepDeadlineMs, &deadline);
                deadline += mach_absolute_time();
                _commandGate->commandSleep((void*)&_state, deadline, THREAD_ABORTSAFE);
            } while (_state != (kStateOpen | kStateClosing));
        }
        
        _owner->closeForClient(this, _clientContext, _options);
        atomic_store ((_Atomic uint32_t *)&_state, 0);
	}
    
    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServicefastPathUserClient::_copyEvent
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::_copyEvent (
                                                 IOHIDEventServiceFastPathUserClient *  target,
                                                 void *                                 reference __unused,
                                                 IOExternalMethodArguments *            arguments)
{
    OSObject *      copySpec = NULL;
    IOHIDEvent *    outEvent = NULL;
    IOReturn        ret      = kIOReturnError;
    
    if ( arguments->structureInput && arguments->structureInputSize) {
        
        int copySpecType = (int)arguments->scalarInput[1];
        
        if (copySpecType == kIOHIDEventServiceFastPathCopySpecSerializedType) {
            copySpec = OSUnserializeXML( (const char *) arguments->structureInput, arguments->structureInputSize);
        } else if (copySpecType == kIOHIDEventServiceFastPathCopySpecDataType) {
            copySpec = OSData::withBytesNoCopy((void*)arguments->structureInput, arguments->structureInputSize);
        }
    }
    
    do {
        outEvent = target->copyEvent(copySpec, (IOOptionBits)arguments->scalarInput[0]);
        
        if (!outEvent) {
            break;
        }
        if (target->_queue) {
            target->_queue->enqueueEvent(outEvent);
        }
        
        ret = kIOReturnSuccess;
        
    } while ( 0 );
    
    if (copySpec) {
        copySpec->release();
    }
    
    if (outEvent) {
        outEvent->release();
    }
    
    if (ret) {
        HIDLogError("IOHIDEventServiceFastPathUserClient::_copyEvent: %x", ret);
    }
    return ret;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::copyEvent
//==============================================================================
IOHIDEvent *  IOHIDEventServiceFastPathUserClient::copyEvent(OSObject * copySpec, IOOptionBits options)
{
    IOHIDEvent * event = NULL;
    
    if (!isOpened() || isInactive()) {
        return event;
    }
    atomic_fetch_add((_Atomic uint32_t *)&_state, 1);
    
    event = _owner->copyEventForClient(copySpec, options, _clientContext);

    atomic_fetch_sub((_Atomic uint32_t *)&_state, 1);
    
    if (_state & kStateClosing) {
        _commandGate->commandWakeup((void *)&_state);
    }
    return event;
}


//==============================================================================
// IOHIDEventServiceFastPathUserClient::didTerminate
//==============================================================================
bool IOHIDEventServiceFastPathUserClient::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{
    
    close ();
    
    return super::didTerminate(provider, options, defer);
}

//==============================================================================
// IOHIDEventServicefastPathUserClient::free
//==============================================================================
void IOHIDEventServiceFastPathUserClient::free()
{
    OSSafeReleaseNULL(_queue);
    OSSafeReleaseNULL(_owner);
    OSSafeReleaseNULL(_commandGate);
  
    super::free();
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::setProperties
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::setProperties( OSObject * properties)
{
    if (isInactive()) {
        return kIOReturnOffline;
    }
    
    IOReturn result = _commandGate->runAction(
                                        OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceFastPathUserClient::setPropertiesGated),
                                        properties
                                        );
 
    return result;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::setPropertiesGated
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::setPropertiesGated( OSObject * properties)
{
    if (!isOpened() || isInactive()) {
        return kIOReturnExclusiveAccess;
    }
    
    IOReturn result = _owner->setPropertiesForClient(properties, _clientContext);
    
    return result;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::copyProperty
//==============================================================================
OSObject * IOHIDEventServiceFastPathUserClient::copyProperty( const char * aKey) const
{
    if (isInactive()) {
        return NULL;
    }
    OSObject * result = NULL;
    _commandGate->runAction(
                        OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceFastPathUserClient::copyPropertyGated),
                        (void*)aKey,
                        &result
                        );
	return result;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::copyPropertyGated
//==============================================================================
void IOHIDEventServiceFastPathUserClient::copyPropertyGated (const char * aKey, OSObject **result) const
{
    if (!isOpened() || isInactive()) {
        return;
    }
    *result = _owner->copyPropertyForClient(aKey, _clientContext);
}


//====================================================================================================
// IOHIDEventServiceFastPathUserClient::serializeDebugState
//====================================================================================================
bool   IOHIDEventServiceFastPathUserClient::serializeDebugState(void * ref __unused, OSSerialize * serializer)
{
    bool          result = false;
    OSDictionary  *debugDict = OSDictionary::withCapacity(6);
    
    require(debugDict, exit);
    
    if (_queue) {
        debugDict->setObject("EventQueue", _queue);
    }
    
    result = debugDict->serialize(serializer);
    debugDict->release();
    
exit:
    
    return result;
}

//====================================================================================================
// IOHIDEventServiceFastPathUserClient::isOpened
//====================================================================================================
bool IOHIDEventServiceFastPathUserClient::isOpened () const
{
    return ((_state & (kStateOpen | kStateClosing)) == kStateOpen);
}
