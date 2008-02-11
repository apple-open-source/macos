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
#include "IOHIDEventServiceUserClient.h"
#include "IOHIDEventServiceQueue.h"
#include "IOHIDEventData.h"
#include "IOHIDEvent.h"
//===========================================================================
// IOHIDEventServiceUserClient class

#define super IOUserClient

OSDefineMetaClassAndStructors( IOHIDEventServiceUserClient, super )

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
    }
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
   if (_client) {
        task_deallocate(_client);
        _client = 0;
    }
   
   if (_owner) {	
        _owner->close(this, _options);
        detach(_owner);
    }

    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::registerNotificationPort
//==============================================================================
IOReturn IOHIDEventServiceUserClient::registerNotificationPort(
                            mach_port_t                 port, 
                            UInt32                      type, 
                            UInt32                      refCon )
{
    _queue->setNotificationPort(port);
         
    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::clientMemoryForType
//==============================================================================
IOReturn IOHIDEventServiceUserClient::clientMemoryForType(
                            UInt32                      type,
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
typedef struct HIDCommandGateArgs {
    uint32_t                    selector; 
    IOExternalMethodArguments * arguments;
    IOExternalMethodDispatch *  dispatch;
    OSObject *                  target;
    void *                      reference;
}HIDCommandGateArgs;

IOReturn IOHIDEventServiceUserClient::externalMethod(
                            uint32_t                    selector, 
                            IOExternalMethodArguments * arguments,
                            IOExternalMethodDispatch *  dispatch, 
                            OSObject *                  target, 
                            void *                      reference)
{
    if (selector < (uint32_t) kIOHIDEventServiceUserClientNumCommands)
    {
        dispatch = (IOExternalMethodDispatch *) &sMethods[selector];
        
        if (!target)
            target = this;
    }
	
	return super::externalMethod(selector, arguments, dispatch, target, reference);
}

//==============================================================================
// IOHIDEventServiceUserClient::initWithTask
//==============================================================================
bool IOHIDEventServiceUserClient::initWithTask(task_t owningTask, void * security_id, UInt32 type)
{
    if (!super::init())
        return false;

    _client = owningTask;
    
    task_reference (_client);

    _queue = IOHIDEventServiceQueue::withCapacity(1024);
    
    if ( !_queue )
        return false;
    
    return true;
}

//==============================================================================
// IOHIDEventServiceUserClient::start
//==============================================================================
bool IOHIDEventServiceUserClient::start( IOService * provider )
{
    if ( !super::start(provider) )
        return false;
        
    _owner = OSDynamicCast(IOHIDEventService, provider);
    if ( !_owner )
        return false;
            
    return true;
}

//==============================================================================
// IOHIDEventServiceUserClient::_open
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_open(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference, 
                                IOExternalMethodArguments *     arguments)
{
    return target->open((IOOptionBits)arguments->scalarInput[0]);
}

//==============================================================================
// IOHIDEventServiceUserClient::open
//==============================================================================
IOReturn IOHIDEventServiceUserClient::open(IOOptionBits options)
{
    
    // get ready just in case events start coming our way
    _queue->setState(true);
    _options = options;
    
    if (!_owner->open(  this, 
                        options, 
                        NULL, 
                        OSMemberFunctionCast(IOHIDEventService::Action, 
                        this, &IOHIDEventServiceUserClient::eventServiceCallback)) ) {
        _queue->setState(false);
        return kIOReturnExclusiveAccess;
    }     
    
    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::_close
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_close(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference, 
                                IOExternalMethodArguments *     arguments)
{
    return target->close();
}

//==============================================================================
// IOHIDEventServiceUserClient::close
//==============================================================================
IOReturn IOHIDEventServiceUserClient::close()
{
    _queue->setState(false);
    _owner->close(this, _options);

    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::_copyEvent
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_copyEvent(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference, 
                                IOExternalMethodArguments *     arguments)
{
    IOHIDEvent *    inEvent     = NULL;
    IOHIDEvent *    outEvent    = NULL;
    IOReturn        ret         = kIOReturnError;
    IOByteCount     length      = 0;
    
    if ( arguments->structureInput && arguments->structureInputSize)
        inEvent = IOHIDEvent::withBytes(arguments->structureInput, arguments->structureInputSize);

    do { 
        outEvent = target->copyEvent(arguments->scalarInput[0], inEvent, arguments->scalarInput[1]);
        
        if ( !outEvent )
            break;
            
        length = outEvent->getLength();
        
        if ( length > arguments->structureOutputSize ) {
            ret = kIOReturnBadArgument;
            break;
        }

        outEvent->readBytes(arguments->structureOutput, length);
        arguments->structureOutputSize = length;

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
    return _owner->copyEvent(type, matching, options);
}

//==============================================================================
// IOHIDEventServiceUserClient::didTerminate
//==============================================================================
bool IOHIDEventServiceUserClient::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{
    _owner->close(this, _options);
        
    return super::didTerminate(provider, options, defer);
}

//==============================================================================
// IOHIDEventServiceUserClient::free
//==============================================================================
void IOHIDEventServiceUserClient::free()
{
    if (_queue) {
        _queue->release();
        _queue = 0;
    }

    if (_owner) {
        _owner = 0;
    }
    
    super::free();
}

//==============================================================================
// IOHIDEventServiceUserClient::setProperties
//==============================================================================
IOReturn IOHIDEventServiceUserClient::setProperties( OSObject * properties )
{
    return _owner->setProperties(properties);
}

//==============================================================================
// IOHIDEventServiceUserClient::eventServiceCallback
//==============================================================================
void IOHIDEventServiceUserClient::eventServiceCallback(
                                IOHIDEventService *             sender, 
                                void *                          context,
                                IOHIDEvent *                    event, 
                                IOOptionBits                    options)
{
    if (!_queue || !_queue->getState())
        return;
        
    //enqueue the event
    _queue->enqueueEvent(event);
}
