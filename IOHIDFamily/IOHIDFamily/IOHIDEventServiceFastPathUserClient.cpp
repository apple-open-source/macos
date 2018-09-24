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
#include <sys/proc.h>
#include <stdatomic.h>

#define kQueueSizeMin   0
#define kQueueSizeMax   16384

#define kIOHIDSystemUserAccessFastPathEntitlement   "com.apple.hid.system.user-access-fast-path"

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
    
    require_action(!_opened || !isInactive(), exit, result = kIOReturnOffline);
    
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
            
    if (!_buffer) {
        uint32_t queueSize = getSharedMemorySize ();
        if (queueSize > sizeof(uint32_t)) {
            _buffer = IOBufferMemoryDescriptor::withOptions (kIODirectionNone | kIOMemoryKernelUserShared, queueSize);
            if (_buffer) {
                setProperty("SharedMemorySize", queueSize, 32);
            }
        } else {
            result = kIOReturnBadArgument;
        }
    }

    if (_buffer) {
        _buffer->retain();
        result = kIOReturnSuccess;
    }
    
    *options = 0;
    *memory  = _buffer;

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
// IOHIDEventServiceFastPathUserClient::initWithTask
//==============================================================================
bool IOHIDEventServiceFastPathUserClient::initWithTask(task_t owningTask __unused, void * security_id __unused, UInt32 type __unused)
{
    OSObject *  entitlement = NULL;
    bool        result;

    result = super::init();
    require_action(result, exit, HIDLogError("failed"));

    entitlement = copyClientEntitlement(owningTask, kIOHIDSystemUserAccessFastPathEntitlement);
    if (entitlement == kOSBooleanTrue) {
        _fastPathEntitlement = true;
        OSSafeReleaseNULL(entitlement);
    }
    else {
        proc_t process = (proc_t)get_bsdtask_info(owningTask);
        char name[255];
        bzero(name, sizeof(name));
        proc_name(proc_pid(process), name, sizeof(name));
        HIDLogInfo("%s Does not have fast path entitlement", name);
    }

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
    
    _lock = IOLockAlloc();
    require (_lock, exit);

    _owner = OSDynamicCast(IOHIDEventService, provider);
    require (_owner, exit);
    
    _owner->retain();
 
    workLoop = getWorkLoop();
    require(workLoop, exit);
  
    _commandGate = IOCommandGate::commandGate(this);
    require(_commandGate, exit);
  
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

    require(workLoop->addEventSource(_commandGate) == kIOReturnSuccess, exit);

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
    IOReturn result;

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

    // Create a property dictionary if one was not passed in.
    if (property == NULL) {
        property = OSDictionary::withCapacity(1);
        require_action(property, exit, result = kIOReturnNoMemory);
    }

    result = target->open((IOOptionBits)arguments->scalarInput[0], property);

exit:
    OSSafeReleaseNULL(property);

    return result;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::open
//==============================================================================
IOReturn IOHIDEventServiceFastPathUserClient::open(IOOptionBits  options, OSDictionary * properties)
{
    IOReturn result = kIOReturnSuccess;
    bool     good;

    require_action(_owner, exit, result = kIOReturnOffline);
    require_action(!_opened, exit, result = kIOReturnExclusiveAccess);
    require_action(properties, exit, result = kIOReturnBadArgument);

    good = properties->setObject(kIOHIDFastPathHasEntitlementKey, (_fastPathEntitlement ? kOSBooleanTrue : kOSBooleanFalse));
    require_action(good, exit, result = kIOReturnNoMemory);
    
    _options = options;
    
    if (!_owner->openForClient(this, 0, properties, &_clientContext)) {
       return kIOReturnExclusiveAccess;
    }
	
    _opened = true;

exit:
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
    if (_owner && _opened) {
        IOLockLock (_lock);
        _opened = false;
        IOLockUnlock (_lock);

        _owner->closeForClient(this, _clientContext, _options);
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
    IOReturn        ret;
    
    if ( arguments->structureInput && arguments->structureInputSize) {
        
        int copySpecType = (int)arguments->scalarInput[1];
        
        if (copySpecType == kIOHIDEventServiceFastPathCopySpecSerializedType) {
            copySpec = OSUnserializeXML( (const char *) arguments->structureInput, arguments->structureInputSize);
        } else if (copySpecType == kIOHIDEventServiceFastPathCopySpecDataType) {
            copySpec = OSData::withBytesNoCopy((void*)arguments->structureInput, arguments->structureInputSize);
        }
    }
    
    ret = target->copyEvent(copySpec, (IOOptionBits)arguments->scalarInput[0]);
    
    if (copySpec) {
        copySpec->release();
    }
    
    if (ret) {
        HIDLogError("IOHIDEventServiceFastPathUserClient::_copyEvent: %x", ret);
    }
    return ret;
}

//==============================================================================
// IOHIDEventServiceFastPathUserClient::copyEvent
//==============================================================================
IOReturn  IOHIDEventServiceFastPathUserClient::copyEvent(OSObject * copySpec, IOOptionBits options)
{
    
    IOReturn        ret = kIOReturnSuccess;
    IOHIDEvent *    event = NULL;
    
    if (isInactive()) {
        return kIOReturnOffline;
    }
    
    IOLockLock(_lock);
    
    do {
        if (!_opened) {
            ret = kIOReturnNotOpen;
            break;
        }
        event = _owner->copyEventForClient(copySpec, options, _clientContext);
        if (!event) {
            ret = kIOReturnNotFound;
            break;
        }
        if (!_buffer) {
            ret = kIOReturnNoResources;
            break;
        }
        
        IOByteCount eventSize = event->getLength();
        if (eventSize > (_buffer->getCapacity() - sizeof(UInt32))) {
            ret = kIOReturnNoSpace;
            break;
        }
        *((UInt32 *)_buffer->getBytesNoCopy()) = (UInt32) eventSize;
        event->readBytes((void *)((UInt8 *)_buffer->getBytesNoCopy() + sizeof(UInt32)), eventSize);
        
    } while (false);
   
    if (event) {
        event->release();
    }
    
    IOLockUnlock(_lock);

    return ret;
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
    OSSafeReleaseNULL(_buffer);
    OSSafeReleaseNULL(_owner);
    OSSafeReleaseNULL(_commandGate);
    
    if (_lock) {
        IOLockFree(_lock);
    }

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
    if (!_opened || isInactive()) {
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
    if (!_opened || isInactive()) {
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

    result = debugDict->setObject(kIOHIDFastPathHasEntitlementKey, (_fastPathEntitlement ? kOSBooleanTrue : kOSBooleanFalse));
    require(result, exit);

    result = debugDict->serialize(serializer);
    debugDict->release();
    
exit:
    
    return result;
}

//====================================================================================================
// IOHIDEventServiceFastPathUserClient::getSharedMemorySize
//====================================================================================================

uint32_t IOHIDEventServiceFastPathUserClient::getSharedMemorySize ()
{
    uint32_t size = 0;
    OSObject * value = _owner->copyPropertyForClient(kIOHIDEventServiceQueueSize, _clientContext);
    if (value) {
        OSNumber * num = OSDynamicCast(OSNumber, value);
        if (num) {
            size = num->unsigned32BitValue();
            size = min(kQueueSizeMax, size);
        }
        OSSafeReleaseNULL(value);
    }
    return size;
}


