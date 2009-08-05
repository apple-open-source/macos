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
/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 */


#include <IOKit/IOLib.h>
#include <libkern/c++/OSContainers.h>

#include "IOHIDUserClient.h"
#include "IOHIDParameter.h"
#include "IOHIDPrivate.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOHIDUserClient, IOUserClient)

OSDefineMetaClassAndStructors(IOHIDParamUserClient, IOUserClient)

OSDefineMetaClassAndStructors(IOHIDStackShotUserClient, IOUserClient)

OSDefineMetaClassAndStructors(IOHIDEventSystemUserClient, IOUserClient)

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
    owner->evClose();
#ifdef DEBUG
    kprintf("%s: client token invalidated\n", getName());
#endif
    
    owner->serverConnect = 0;
    detach( owner);
    
    return( kIOReturnSuccess);
}

IOService * IOHIDUserClient::getService( void )
{
    return( owner );
}

IOReturn IOHIDUserClient::registerNotificationPort(
		mach_port_t 	port,
		UInt32		type,
		UInt32		refCon )
{
    if( type != kIOHIDEventNotification)
	return( kIOReturnUnsupported);

    owner->setEventPort(port);
    return( kIOReturnSuccess);
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

    owner->registerScreen(graphicsDevice, bounds);

    return( kIOReturnSuccess);
}

IOReturn IOHIDUserClient::clientMemoryForType( UInt32 type,
        UInt32 * flags, IOMemoryDescriptor ** memory )
{
    if( type == kIOHIDGlobalMemory) {
        *flags = 0;
        
        if (owner->globalMemory)
            owner->globalMemory->retain();
        *memory = owner->globalMemory;
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
            kIOUCStructIStructO, 0xffffffff, 0 },
/* 4 */  { NULL, (IOMethod) &IOHIDSystem::extSetMouseLocation,
            kIOUCStructIStructO, 0xffffffff, 0 },
/* 5 */  { NULL, (IOMethod) &IOHIDSystem::extGetButtonEventNum,
            kIOUCScalarIScalarO, 1, 1 },
/* 6 */  { NULL, (IOMethod) &IOHIDSystem::extSetBounds,
            kIOUCStructIStructO, sizeof( IOGBounds), 0 }
};

    if( index > (sizeof(methodTemplate) / sizeof(methodTemplate[0])))
        return( NULL );

    *targetP = owner;
    return( (IOExternalMethod *)(methodTemplate + index) );
}

IOReturn IOHIDUserClient::setProperties( OSObject * properties )
{
    OSDictionary * dict = OSDynamicCast(OSDictionary, properties);
    if (dict && dict->getObject(kIOHIDUseKeyswitchKey) && 
        ( clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator) != kIOReturnSuccess))
    {
        dict->removeObject(kIOHIDUseKeyswitchKey);
    }

    return( owner->setProperties( properties ) );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOHIDParamUserClient::start( IOService * _owner )
{
    if( !super::start( _owner ))
	return( false);

    owner = (IOHIDSystem *) _owner;

    return( true );
}

IOReturn IOHIDParamUserClient::clientClose( void )
{
    return( kIOReturnSuccess);
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
/* 3 */  { NULL, (IOMethod) &IOHIDSystem::extPostEvent,
            kIOUCStructIStructO, 0xffffffff, 0 },
/* 4 */  { NULL, (IOMethod) &IOHIDSystem::extSetMouseLocation,
            kIOUCStructIStructO, 0xffffffff, 0 },
    };

    if( (index >= 3)
     && (index < (sizeof( methodTemplate) / sizeof( methodTemplate[0])))) {
        *targetP = owner;
	return( (IOExternalMethod *) methodTemplate + index);
    } else
	return( NULL);
}

IOReturn IOHIDParamUserClient::setProperties( OSObject * properties )
{        
    OSDictionary * dict = OSDynamicCast(OSDictionary, properties);
    if (dict && dict->getObject(kIOHIDUseKeyswitchKey) && 
        ( clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator) != kIOReturnSuccess))
    {
        dict->removeObject(kIOHIDUseKeyswitchKey);
    }

    return( owner->setProperties( properties ) );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
bool IOHIDStackShotUserClient::
initWithTask(task_t owningTask, void * /* security_id */, UInt32 /* type */)
{
    if (!super::init() || 
            (IOUserClient::clientHasPrivilege(owningTask, 
                kIOClientPrivilegeAdministrator) != kIOReturnSuccess))
        return false;

    client = owningTask;
    task_reference (client);

    return true;
}

bool IOHIDStackShotUserClient::start( IOService * _owner )
{
    if( !super::start( _owner ))
	return( false);

    owner = (IOHIDSystem *) _owner;

    return( true );
}

IOReturn IOHIDStackShotUserClient::clientClose( void )
{
   if (client) {
        task_deallocate(client);
        client = 0;
    }
 
    return( kIOReturnSuccess);
}

IOService * IOHIDStackShotUserClient::getService( void )
{
    return( owner );
}


IOReturn IOHIDStackShotUserClient::registerNotificationPort(
		mach_port_t 	port,
		UInt32		type,
		UInt32		refCon )
{
    if( type != kIOHIDStackShotNotification)
	return( kIOReturnUnsupported);

    owner->setStackShotPort(port);
    return( kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
bool IOHIDEventSystemUserClient::
initWithTask(task_t owningTask, void * /* security_id */, UInt32 /* type */)
{
    if ( !super::init() ) {
        return false;
    }
        
    if ( IOUserClient::clientHasPrivilege(owningTask, 
                kIOClientPrivilegeAdministrator) != kIOReturnSuccess ) {
        IOLog("%s: Client task not privileged to open IOHIDSystem for mapping memory\n", __PRETTY_FUNCTION__);
        
        return false;
    }


    client = owningTask;
    task_reference (client);

    return true;
}

bool IOHIDEventSystemUserClient::start( IOService * _owner )
{
    if( !super::start( _owner ))
	return( false);

    owner = (IOHIDSystem *) _owner;

    return( true );
}

IOReturn IOHIDEventSystemUserClient::clientClose( void )
{
   if (client) {
        task_deallocate(client);
        client = 0;
    }
 
    return( kIOReturnSuccess);
}

IOService * IOHIDEventSystemUserClient::getService( void )
{
    return( owner );
}

IOReturn IOHIDEventSystemUserClient::clientMemoryForType( UInt32 type,
        UInt32 * flags, IOMemoryDescriptor ** memory )
{
    IODataQueue *   eventQueue  = OSDynamicCast(IODataQueue, (OSObject*)type);
    IOReturn        ret         = kIOReturnNoMemory;
    
    if ( eventQueue ) {
        IOMemoryDescriptor * desc = NULL;
        *flags = 0;
        
        desc = eventQueue->getMemoryDescriptor();
        
        if ( desc ) { 
            desc->retain();
            ret = kIOReturnSuccess;
        }
        
        *memory = desc;
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
            kIOUCScalarIScalarO, 0, 0 }
    };

    if( index > (sizeof(methodTemplate) / sizeof(methodTemplate[0])))
        return( NULL );

    *targetP = this;
    return( (IOExternalMethod *)(methodTemplate + index) );
}

IOReturn IOHIDEventSystemUserClient::createEventQueue(void*p1,void*p2,void*p3,void*,void*,void*)
{
    UInt32          type        = (UInt32)p1;
    IOByteCount     size        = (IOByteCount)p2;
    UInt32 *        pToken      = (UInt32*)p3;
    IODataQueue *   eventQueue  = NULL;
    
    if( !size )
        return kIOReturnBadArgument;

    switch ( type ) {
        case kIOHIDEventQueueTypeKernel:
            if ( !kernelQueue ) {
                kernelQueue = IOHIDEventServiceQueue::withCapacity(size);
                if ( kernelQueue ) {
                    kernelQueue->setState(true);
                    owner->registerEventQueue(kernelQueue);
                }
            }
            eventQueue = kernelQueue;
            break;
        case kIOHIDEventQueueTypeUser:
            if ( !userQueues )
                userQueues = OSSet::withCapacity(4);
                
            eventQueue = IOSharedDataQueue::withCapacity(size);
            
            userQueues->setObject(eventQueue);
            
            eventQueue->release();

            break;    
    }

    if( !eventQueue )
        return kIOReturnNoMemory;
        
    if ( pToken )
        *pToken = (UInt32)eventQueue;

    return kIOReturnSuccess;
}

IOReturn IOHIDEventSystemUserClient::destroyEventQueue(void*p1,void*p2,void*p3,void*,void*,void*)
{
    UInt32          type        = (UInt32)p1;
    IODataQueue *   eventQueue  = OSDynamicCast(IODataQueue, (OSObject*)p2);

    if ( !eventQueue )
        return kIOReturnBadArgument;

    switch ( type ) {
        case kIOHIDEventQueueTypeKernel:
            if ( kernelQueue == eventQueue ) {
                kernelQueue->setState(false);
                owner->unregisterEventQueue(kernelQueue);
                kernelQueue->release();
                kernelQueue = NULL;
                break;
            }
        case kIOHIDEventQueueTypeUser:
            if ( userQueues )
                userQueues->removeObject(eventQueue);
            break;
    }

    return kIOReturnSuccess;
}

IOReturn IOHIDEventSystemUserClient::tickle(void*,void*,void*,void*,void*,void*)
{
    owner->displayManager->activityTickle(0,0);;

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
        userQueues->release();
    }
    
    super::free();
}

IOReturn IOHIDEventSystemUserClient::registerNotificationPort(
		mach_port_t 	port,
		UInt32		type,
		UInt32		refCon )
{
    IODataQueue *   eventQueue  = OSDynamicCast(IODataQueue, (OSObject*)type);

    if ( !eventQueue )
        return kIOReturnBadArgument;
        
    eventQueue->setNotificationPort(port);
    
    return( kIOReturnSuccess);
}

