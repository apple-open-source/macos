/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2009 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 */


#include <IOKit/IOLib.h>
#include <IOKit/hid/IOHIDEventTypes.h>
#include <libkern/c++/OSContainers.h>
#include <sys/proc.h>
#include <AssertMacros.h>
#include <IOKit/graphics/IOGraphicsDevice.h>
#include <IOKit/hidsystem/IOHIDevice.h>
#include <IOKit/hidsystem/ev_private.h>

#include "IOHIDUserClient.h"
#include "IOHIDParameter.h"
#include "IOHIDFamilyPrivate.h"
#include "IOHIDPrivate.h"
#include "IOHIDSystem.h"
#include "IOHIDEventSystemQueue.h"
#include "IOHIDDebug.h"

#define kIOHIDSystemUserAccessServiceEntitlement "com.apple.hid.system.user-access-service"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOHIDUserClient, IOUserClient)

OSDefineMetaClassAndStructors(IOHIDParamUserClient, IOUserClient)

//OSDefineMetaClassAndStructors(IOHIDStackShotUserClient, IOUserClient)

OSDefineMetaClassAndStructorsWithInit(IOHIDEventSystemUserClient, IOUserClient, IOHIDEventSystemUserClient::initialize())

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOHIDUserClient::start( IOService * _owner )
{
    if( !super::start( _owner ))
        return( false);

    owner = (IOHIDSystem *) _owner;

    return( true );
}

IOReturn IOHIDUserClient::clientClose( void )
{
    terminate();

    return kIOReturnSuccess;
}

IOReturn IOHIDUserClient::close( void )
{
    if (owner) {
        for (unsigned int i = 0; i < MAX_SCREENS; i++) {
            int token = _screenTokens[i];
            if (token) {
                owner->unregisterScreen(token);
                _screenTokens[i] = 0;
            }
        }
        owner->evClose();
        owner = NULL;
    }
    
    return kIOReturnSuccess;
}

void IOHIDUserClient::stop( IOService * provider )
{
    close();
    
    super::stop(provider);
}

IOService * IOHIDUserClient::getService( void )
{
    return( owner );
}

IOReturn IOHIDUserClient::registerNotificationPort(
		mach_port_t 	port __unused,
		UInt32		type,
		UInt32		refCon __unused )
{
    if( type != kIOHIDEventNotification)
        return kIOReturnUnsupported;
    if (!owner)
        return kIOReturnOffline;

    //owner->setEventPort(port);
    return kIOReturnSuccess;
}

IOReturn IOHIDUserClient::connectClient( IOUserClient * client )
{
    IOGBounds *         bounds;
    IOService *         provider;
    IOGraphicsDevice *	graphicsDevice;

    provider = client->getProvider();

    // avoiding OSDynamicCast & dependency on graphics family
    if( !provider || !provider->metaCast("IOGraphicsDevice"))
    	return( kIOReturnBadArgument );

    graphicsDevice = (IOGraphicsDevice *) provider;
    graphicsDevice->getBoundingRect(&bounds);

    if (owner) {
        int token = owner->registerScreen(graphicsDevice, bounds, bounds+1);
        // HIDSystem returns a token which is just index + SCREENTOKEN
        if (token >= SCREENTOKEN) {
            _screenTokens[token - SCREENTOKEN] = token;
        }
    }
    

    return( kIOReturnSuccess);
}

IOReturn IOHIDUserClient::clientMemoryForType( UInt32 type,
        UInt32 * flags, IOMemoryDescriptor ** memory )
{
    if( type == kIOHIDGlobalMemory) {
        *flags = 0;

        if (owner && owner->globalMemory) {
            owner->globalMemory->retain();
        *memory = owner->globalMemory;
        }
        else {
            *memory = NULL;
        }
    } else {
        return kIOReturnBadArgument;
    }

    return kIOReturnSuccess;
}

IOExternalMethod * IOHIDUserClient::getTargetAndMethodForIndex(
                        IOService ** targetP, UInt32 index )
{
    static const IOExternalMethod methodTemplate[] = {
/* 0 */  { NULL, (IOMethod) &IOHIDSystem::createShmem,
            kIOUCScalarIScalarO, 1, 0 },
/* 1 */  { NULL, (IOMethod) &IOHIDSystem::setEventsEnable,
            kIOUCScalarIScalarO, 1, 0 },
/* 2 */  { NULL, (IOMethod) &IOHIDSystem::setCursorEnable,
            kIOUCScalarIScalarO, 1, 0 },
/* 3 */  { NULL, (IOMethod) &IOHIDSystem::extPostEvent,
            kIOUCStructIStructO, sizeof( struct evioLLEvent) + sizeof(int), 0 },
/* 4 */  { NULL, (IOMethod) &IOHIDSystem::extSetMouseLocation,
            kIOUCStructIStructO, kIOUCVariableStructureSize, 0 },
/* 5 */  { NULL, (IOMethod) &IOHIDSystem::extGetButtonEventNum,
            kIOUCScalarIScalarO, 1, 1 },
/* 6 */  { NULL, (IOMethod) &IOHIDSystem::extSetBounds,
            kIOUCStructIStructO, sizeof( IOGBounds), 0 },
/* 7 */  { NULL, (IOMethod) &IOHIDSystem::extRegisterVirtualDisplay,
            kIOUCScalarIScalarO, 0, 1 },
/* 8 */  { NULL, (IOMethod) &IOHIDSystem::extUnregisterVirtualDisplay,
            kIOUCScalarIScalarO, 1, 0 },
/* 9 */  { NULL, (IOMethod) &IOHIDSystem::extSetVirtualDisplayBounds,
            kIOUCScalarIScalarO, 5, 0 },
/* 10 */ { NULL, (IOMethod) &IOHIDSystem::extGetUserHidActivityState,
            kIOUCScalarIScalarO, 0, 1 },
/* 11 */ { NULL, (IOMethod) &IOHIDSystem::setContinuousCursorEnable,
            kIOUCScalarIScalarO, 1, 0 },
/* 12 */ { NULL, (IOMethod) &IOHIDSystem::extSetOnScreenBounds,
            kIOUCStructIStructO, 12, 0 },
};
    
    if( index >= (sizeof(methodTemplate) / sizeof(methodTemplate[0])))
        return( NULL );

    *targetP = owner;

    return( (IOExternalMethod *)(methodTemplate + index) );
}

IOReturn IOHIDUserClient::setProperties( OSObject * properties )
{
//    OSDictionary * dict = OSDynamicCast(OSDictionary, properties);
//    if (dict && dict->getObject(kIOHIDUseKeyswitchKey) &&
//        ( clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator) != kIOReturnSuccess))
//    {
//        dict->removeObject(kIOHIDUseKeyswitchKey);
//    }

    return( owner ? owner->setProperties( properties ) : kIOReturnOffline );
}

IOReturn IOHIDUserClient::extGetUserHidActivityState(void* value,void*,void*,void*,void*,void*)
{
    IOReturn result = owner ? owner->extSetVirtualDisplayBounds(value, 0,0,0,0,0) : kIOReturnOffline;

    return result;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
bool IOHIDParamUserClient::start( IOService * _owner )
{
    if( !super::start( _owner ))
        return( false);

    owner = (IOHIDSystem *) _owner;

    return( true );
}

IOService * IOHIDParamUserClient::getService( void )
{
    return( owner );
}

IOExternalMethod * IOHIDParamUserClient::getTargetAndMethodForIndex(
                        IOService ** targetP, UInt32 index )
{
    // get the same library function to work for param & server connects
    static const IOExternalMethod methodTemplate[] = {
        /* 0 */  { NULL, NULL, kIOUCScalarIScalarO, 1, 0 },
        /* 1 */  { NULL, NULL, kIOUCScalarIScalarO, 1, 0 },
        /* 2 */  { NULL, NULL, kIOUCScalarIScalarO, 1, 0 },
        /* 3 */  { NULL, (IOMethod) &IOHIDParamUserClient::extPostEvent, kIOUCStructIStructO, sizeof( struct evioLLEvent) + sizeof(int), 0 },
        /* 4 */  { NULL, (IOMethod) &IOHIDSystem::extSetMouseLocation, kIOUCStructIStructO, 0xffffffff, 0 },
        /* 5 */  { NULL, (IOMethod) &IOHIDSystem::extGetStateForSelector, kIOUCScalarIScalarO, 1, 1 },
        /* 6 */  { NULL, (IOMethod) &IOHIDSystem::extSetStateForSelector, kIOUCScalarIScalarO, 2, 0 },
        /* 7 */  { NULL, (IOMethod) &IOHIDSystem::extRegisterVirtualDisplay, kIOUCScalarIScalarO, 0, 1 },
        /* 8 */  { NULL, (IOMethod) &IOHIDSystem::extUnregisterVirtualDisplay, kIOUCScalarIScalarO, 1, 0 },
        /* 9 */  { NULL, (IOMethod) &IOHIDSystem::extSetVirtualDisplayBounds, kIOUCScalarIScalarO, 5, 0 },
        /* 10 */ { NULL, (IOMethod) &IOHIDParamUserClient::extGetUserHidActivityState, kIOUCScalarIScalarO, 0, 1 },
        /* 11 */ { NULL, (IOMethod) &IOHIDSystem::setContinuousCursorEnable, kIOUCScalarIScalarO, 1, 0 },
    };
    IOExternalMethod *result = NULL;

    if ((index < 3) || (index >= (sizeof(methodTemplate) / sizeof(methodTemplate[0])))) {
        *targetP = NULL;
        result = NULL;
    }
    else {
        result = (IOExternalMethod *) (methodTemplate + index);
        if ((index == 10) || (index == 3)) {
            *targetP = this;
        }
        else {
            *targetP = owner;
        }
    }

    return result;
}

IOReturn IOHIDParamUserClient::clientClose(void)
{
    terminate();
    return kIOReturnSuccess;
}

IOReturn IOHIDParamUserClient::extPostEvent(void*p1,void*p2,void*,void*,void*,void*)
{
    IOReturn result = clientHasPrivilege(current_task(), kIOClientPrivilegeLocalUser);
    if ( result == kIOReturnSuccess ) {
        result = owner ? owner->extPostEvent(p1, p2, NULL, NULL, NULL, NULL) : kIOReturnOffline;
    }
    return result;
}

IOReturn IOHIDParamUserClient::setProperties( OSObject * properties )
{
    OSDictionary * dict = OSDynamicCast(OSDictionary, properties);
    if (dict && dict->getObject(kIOHIDUseKeyswitchKey) &&
        ( clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator) != kIOReturnSuccess))
    {
        dict->removeObject(kIOHIDUseKeyswitchKey);
    }

    return( owner ? owner->setProperties( properties ) : kIOReturnOffline );
}

IOReturn IOHIDParamUserClient::extGetUserHidActivityState(void* value,void*,void*,void*,void*,void*)
{
    IOReturn result = owner ? owner->extGetUserHidActivityState(value, 0,0,0,0,0) : kIOReturnOffline;

    return result;
}


enum { kIOHIDEventSystemKernelQueueID = 100, kIOHIDEventSystemUserQueueID = 200 };

static OSArray * gAllUserQueues;
static IOLock  * gAllUserQueuesLock;

void
IOHIDEventSystemUserClient::initialize(void)
{
	gAllUserQueuesLock = IOLockAlloc();
	gAllUserQueues     = OSArray::withCapacity(4);
}

UInt32
IOHIDEventSystemUserClient::createIDForDataQueue(IOSharedDataQueue * eventQueue)
{
	UInt32 queueIdx;

	if (!eventQueue)
		return (0);

	IOLockLock(gAllUserQueuesLock);
	for (queueIdx = 0;
		  OSDynamicCast(IOSharedDataQueue, gAllUserQueues->getObject(queueIdx));
		  queueIdx++) {}
	gAllUserQueues->setObject(queueIdx, eventQueue);
	IOLockUnlock(gAllUserQueuesLock);

	return (queueIdx + kIOHIDEventSystemUserQueueID);
}

void
IOHIDEventSystemUserClient::removeIDForDataQueue(IOSharedDataQueue * eventQueue)
{
	UInt32     queueIdx;
	OSObject * obj;

	if (!eventQueue)
		return;

	IOLockLock(gAllUserQueuesLock);
	for (queueIdx = 0;
		  (obj = gAllUserQueues->getObject(queueIdx));
		  queueIdx++) {
		if (obj == eventQueue)
			gAllUserQueues->replaceObject(queueIdx, kOSBooleanFalse);
	}
	IOLockUnlock(gAllUserQueuesLock);
}

IOSharedDataQueue *
IOHIDEventSystemUserClient::copyDataQueueWithID(UInt32 queueID)
{
	IOSharedDataQueue * eventQueue;

	IOLockLock(gAllUserQueuesLock);
	eventQueue = OSDynamicCast(IOSharedDataQueue, gAllUserQueues->getObject(queueID - kIOHIDEventSystemUserQueueID));
	if (eventQueue)
		eventQueue->retain();
	IOLockUnlock(gAllUserQueuesLock);

	return (eventQueue);
}

bool IOHIDEventSystemUserClient::
initWithTask(task_t owningTask, void * /* security_id */, UInt32 /* type */)
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
    
exit:
    return result;
}

bool IOHIDEventSystemUserClient::start( IOService * provider )
{
    if( !super::start( provider )) {
      return( false);
    }
  
    owner = (IOHIDSystem *) provider;
    if (owner) {
        owner->retain();
    }
  
  
    IOWorkLoop * workLoop = getWorkLoop();
    if (workLoop == NULL)
    {
       return false;
    }
  
    commandGate = IOCommandGate::commandGate(this);
    if (commandGate == NULL)
    {
       return false;
    }
  
    if (workLoop->addEventSource(commandGate) != kIOReturnSuccess) {
       return false;
    }

    return( true );
}


void IOHIDEventSystemUserClient::stop( IOService * provider)
{
    IOWorkLoop * workLoop = getWorkLoop();
    if (workLoop && commandGate)
    {
        workLoop->removeEventSource(commandGate);
    }
    super::stop(provider);
}

IOReturn IOHIDEventSystemUserClient::clientClose( void )
{

//    if (owner)
//        detach(owner);
//    owner = NULL;
    terminate();
    return( kIOReturnSuccess);
}

IOService * IOHIDEventSystemUserClient::getService( void )
{
    return( owner );
}

IOReturn IOHIDEventSystemUserClient::clientMemoryForType( UInt32 type,
        UInt32 * flags, IOMemoryDescriptor ** memory )
{
    IOReturn result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    
    result = commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventSystemUserClient::clientMemoryForTypeGated), (void*)(intptr_t)type, flags, memory);
    
exit:
    
    return result;
}

IOReturn IOHIDEventSystemUserClient::clientMemoryForTypeGated( UInt32 type,
        UInt32 * flags, IOMemoryDescriptor ** memory )
{
    IOSharedDataQueue *   eventQueue = NULL;
    IOReturn        ret = kIOReturnNoMemory;
    
    if (type == kIOHIDEventSystemKernelQueueID)
        eventQueue = kernelQueue;
    else
        eventQueue  = copyDataQueueWithID(type);

    if ( eventQueue ) {
        IOMemoryDescriptor * desc = NULL;
        *flags = 0;

        desc = eventQueue->getMemoryDescriptor();

        if ( desc ) {
            desc->retain();
            ret = kIOReturnSuccess;
        }

        *memory = desc;
        if (type != kIOHIDEventSystemKernelQueueID)
            eventQueue->release();

    } else {
        ret = kIOReturnBadArgument;
    }

    return ret;
}

IOExternalMethod * IOHIDEventSystemUserClient::getTargetAndMethodForIndex(
                        IOService ** targetP, UInt32 index )
{
    static const IOExternalMethod methodTemplate[] = {
/* 0 */  { NULL, (IOMethod) &IOHIDEventSystemUserClient::createEventQueue,
            kIOUCScalarIScalarO, 2, 1 },
/* 1 */  { NULL, (IOMethod) &IOHIDEventSystemUserClient::destroyEventQueue,
            kIOUCScalarIScalarO, 2, 0 },
/* 2 */  { NULL, (IOMethod) &IOHIDEventSystemUserClient::tickle,
            kIOUCScalarIScalarO, 1, 0 }
    };

    if( index >= (sizeof(methodTemplate) / sizeof(methodTemplate[0])))
        return( NULL );

    *targetP = this;
    return( (IOExternalMethod *)(methodTemplate + index) );
}


IOReturn IOHIDEventSystemUserClient::createEventQueue(void*p1,void*p2,void*p3,void*,void*,void*) {
  IOReturn status = kIOReturnSuccess;
  if (!isInactive() && commandGate) {
    status = commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventSystemUserClient::createEventQueueGated),p1, p2, p3, NULL);
  }
  return status;
}

IOReturn IOHIDEventSystemUserClient::createEventQueueGated(void*p1,void*p2,void*p3, void*)
{
    UInt32          type        = (UInt32)(uintptr_t)p1;
    IOByteCount     size        = (uintptr_t)p2;
    UInt32 *        pToken      = (UInt32 *)p3;
    UInt32          token       = 0;
    IOSharedDataQueue *   eventQueue  = NULL;

    if( !size )
        return kIOReturnBadArgument;

    switch ( type ) {
        case kIOHIDEventQueueTypeKernel:
            if (!owner)
                return kIOReturnOffline;
            if ( !kernelQueue ) {
                kernelQueue = IOHIDEventServiceQueue::withCapacity((UInt32)size);
                if ( kernelQueue ) {
                    kernelQueue->setState(true);
                    owner->registerEventQueue(kernelQueue);
                }
            }
            eventQueue = kernelQueue;
			token = kIOHIDEventSystemKernelQueueID;
			if ( pToken ) {
				*pToken = kIOHIDEventSystemKernelQueueID;
			}
            break;
        case kIOHIDEventQueueTypeUser:
            if (!userQueues)
                userQueues = OSSet::withCapacity(4);

            eventQueue = IOHIDEventSystemQueue::withCapacity((UInt32)size);
			token = createIDForDataQueue(eventQueue);
			if (eventQueue && userQueues) {
				userQueues->setObject(eventQueue);
				eventQueue->release();
			}
            break;
    }

    if( !eventQueue )
        return kIOReturnNoMemory;

    if ( pToken ) {
		*pToken = token;
	}

    return kIOReturnSuccess;
}

IOReturn IOHIDEventSystemUserClient::destroyEventQueue(void*p1,void*p2,void*,void*,void*,void*) {
  IOReturn status = kIOReturnSuccess;
  if (!isInactive() && commandGate) {
    status = commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventSystemUserClient::destroyEventQueueGated),p1, p2);
  }
  return status;
}

IOReturn IOHIDEventSystemUserClient::destroyEventQueueGated(void*p1,void*p2,void*,void*)
{
    UInt32          type       = (UInt32)(uintptr_t) p1;
    UInt32          queueID    = (UInt32)(uintptr_t) p2;
    IOSharedDataQueue *   eventQueue = NULL;

	if (queueID == kIOHIDEventSystemKernelQueueID) {
		eventQueue = kernelQueue;
		type = kIOHIDEventQueueTypeKernel;
	} else {
		eventQueue = copyDataQueueWithID(queueID);
		type = kIOHIDEventQueueTypeUser;
	}

    if ( !eventQueue )
        return kIOReturnBadArgument;

    switch ( type ) {
        case kIOHIDEventQueueTypeKernel:
			kernelQueue->setState(false);
			if (owner) owner->unregisterEventQueue(kernelQueue);
			kernelQueue->release();
			kernelQueue = NULL;
			break;
        case kIOHIDEventQueueTypeUser:
            if (userQueues)
                userQueues->removeObject(eventQueue);
			removeIDForDataQueue(eventQueue);
			eventQueue->release();
            break;
    }

    return kIOReturnSuccess;
}

IOReturn IOHIDEventSystemUserClient::tickle(void*p1,void*,void*,void*,void*,void*)
{
    IOHIDEventType eventType = (UInt32)(uintptr_t) p1;

    /* Tickles coming from userspace must follow the same policy as IOHIDSystem.cpp:
     *   If the display is on, send tickles as usual
     *   If the display is off, only tickle on key presses and button clicks.
     */
    intptr_t otherType = NX_NULLEVENT;
    if (eventType == kIOHIDEventTypeButton) {
        otherType = NX_LMOUSEDOWN;
    } else if (eventType == kIOHIDEventTypeKeyboard) {
        otherType = NX_KEYDOWN;
    } else if (eventType == kIOHIDEventTypePointer) {
        otherType = NX_MOUSEMOVED;
    } else if (eventType == kIOHIDEventTypeScroll) {
        otherType = NX_SCROLLWHEELMOVED;
    }
    if (otherType)
    {
        IOHIDSystemActivityTickle((SInt32)otherType, this);
    }

    return kIOReturnSuccess;
}

void IOHIDEventSystemUserClient::free()
{
    if ( kernelQueue ) {
        kernelQueue->setState(false);
        if ( owner )
            owner->unregisterEventQueue(kernelQueue);

        kernelQueue->release();
    }

    if ( userQueues ) {
        OSObject * obj;
        while ((obj = userQueues->getAnyObject()))
        {
            removeIDForDataQueue(OSDynamicCast(IOSharedDataQueue, obj));
            userQueues->removeObject(obj);
        }
        userQueues->release();
    }
  
    OSSafeReleaseNULL(commandGate);
    OSSafeReleaseNULL(owner);

    super::free();
}

IOReturn IOHIDEventSystemUserClient::registerNotificationPort(
		mach_port_t 	port,
		UInt32		type,
		UInt32		refCon __unused )
{
    IOSharedDataQueue * eventQueue = NULL;

	if (type == kIOHIDEventSystemKernelQueueID)
		eventQueue = kernelQueue;
	else
		eventQueue = copyDataQueueWithID(type);

    if ( !eventQueue )
        return kIOReturnBadArgument;

    eventQueue->setNotificationPort(port);

	if (type != kIOHIDEventSystemKernelQueueID)
		eventQueue->release();

    return (kIOReturnSuccess);
}

