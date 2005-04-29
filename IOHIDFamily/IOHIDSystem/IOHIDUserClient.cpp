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


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOHIDUserClient, IOUserClient)

OSDefineMetaClassAndStructors(IOHIDParamUserClient, IOUserClient)

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
    Bounds * 		bounds;
    IOService *		provider;
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

    if( type != kIOHIDGlobalMemory)
	return( kIOReturnBadArgument);

    *flags = 0;
    
    if (owner->globalMemory)
        owner->globalMemory->retain();
    *memory = owner->globalMemory;
    return( kIOReturnSuccess);
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
            kIOUCStructIStructO, sizeof( IOGBounds), 0 },
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

