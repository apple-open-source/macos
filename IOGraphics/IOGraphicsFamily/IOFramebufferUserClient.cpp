/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#define IOFRAMEBUFFER_PRIVATE
#include <IOKit/graphics/IOFramebufferShared.h>
#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <libkern/c++/OSContainers.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include <IOKit/IOPlatformExpert.h>

#include <IOKit/assert.h>

#include "IOFramebufferUserClient.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOFramebufferUserClient, IOUserClient)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOFramebufferUserClient * IOFramebufferUserClient::withTask( task_t owningTask )
{
    IOFramebufferUserClient * inst;

    inst = new IOFramebufferUserClient;

    if (inst && !inst->init())
    {
        inst->release();
        inst = 0;
    }

    return (inst);
}

bool IOFramebufferUserClient::start( IOService * _owner )
{
    if (!super::start(_owner))
        return (false);

    owner = (IOFramebuffer *) _owner;
    owner->serverConnect = this;

    return (true);
}

IOReturn IOFramebufferUserClient::registerNotificationPort(
    mach_port_t         port,
    UInt32              type,
    UInt32              refCon )
{
    return (owner->extRegisterNotificationPort(port, type, refCon));
}

IOReturn IOFramebufferUserClient::getNotificationSemaphore(
    UInt32 interruptType, semaphore_t * semaphore )
{
    return (owner->getNotificationSemaphore(interruptType, semaphore));
}

// The window server is going away.

IOReturn IOFramebufferUserClient::clientClose( void )
{
    owner->close();
    detach( owner);

    return (kIOReturnSuccess);
}

IOService * IOFramebufferUserClient::getService( void )
{
    return (owner);
}

IOReturn IOFramebufferUserClient::clientMemoryForType( UInt32 type,
        IOOptionBits * flags, IOMemoryDescriptor ** memory )
{
    static bool          havePublishedResource;
    IOMemoryDescriptor * mem;
    IOReturn             err;

    switch (type)
    {
        case kIOFBCursorMemory:

            if (!havePublishedResource)
            {
                havePublishedResource = true;
                publishResource("WindowServer");
            }

            mem = owner->sharedCursor;
            mem->retain();
            break;

        case kIOFBVRAMMemory:
            mem = owner->getVRAMRange();
            break;

        default:
            mem = (IOMemoryDescriptor *) owner->userAccessRanges->getObject( type );
            mem->retain();
            break;
    }

    *memory = mem;
    if (mem)
        err = kIOReturnSuccess;
    else
        err = kIOReturnBadArgument;

    return (err);
}

IOReturn IOFramebufferUserClient::setProperties( OSObject * properties )
{
    OSDictionary *      props;
    IOReturn            kr = kIOReturnUnsupported;

    if (!(props = OSDynamicCast(OSDictionary, properties)))
        return (kIOReturnBadArgument);

    kr = owner->extSetProperties( props );

    return (kr);
}

IOReturn IOFramebufferUserClient::externalMethod( uint32_t selector, IOExternalMethodArguments * args,
                                        IOExternalMethodDispatch * dispatch, OSObject * target, void * reference )
{
    IOReturn ret;

    static const IOExternalMethodDispatch methodTemplate[20] =
    {
        /*[0]*/  { (IOExternalMethodAction) &IOFramebuffer::extCreateSharedCursor,
                    3, 0, 0, 0 },
        /*[1]*/  { (IOExternalMethodAction) &IOFramebuffer::extGetPixelInformation,
                    3, 0, 0, sizeof(IOPixelInformation) },
        /*[2]*/  { (IOExternalMethodAction) &IOFramebuffer::extGetCurrentDisplayMode,
                    0, 0, 2, 0 },
        /*[3]*/  { (IOExternalMethodAction) &IOFramebuffer::extSetStartupDisplayMode,
                    2, 0, 0, 0 },
        /*[4]*/  { (IOExternalMethodAction) &IOFramebuffer::extSetDisplayMode,
                    2, 0, 0, 0 },
        /*[5]*/  { (IOExternalMethodAction) &IOFramebuffer::extGetInformationForDisplayMode,
                    1, 0, 0, kIOUCVariableStructureSize },
        /*[6]*/  { (IOExternalMethodAction) &IOFramebuffer::extGetDisplayModeCount,
                    0, 0, 1, 0 },
        /*[7]*/  { (IOExternalMethodAction) &IOFramebuffer::extGetDisplayModes,
                    0, 0, 0, kIOUCVariableStructureSize },
        /*[8]*/  { (IOExternalMethodAction) &IOFramebuffer::extGetVRAMMapOffset,
                    1, 0, 1, 0 },
        /*[9]*/  { (IOExternalMethodAction) &IOFramebuffer::extSetBounds,
                    0, kIOUCVariableStructureSize, 0, 0 },
        /*[10]*/ { (IOExternalMethodAction) &IOFramebuffer::extSetNewCursor,
                    3, 0, 0, 0 },
        /*[11]*/ { (IOExternalMethodAction) &IOFramebuffer::extSetGammaTable,
                    3, kIOUCVariableStructureSize, 0, 0 },
        /*[12]*/ { (IOExternalMethodAction) &IOFramebuffer::extSetCursorVisible,
                    1, 0, 0, 0 },
        /*[13]*/ { (IOExternalMethodAction) &IOFramebuffer::extSetCursorPosition,
                    2, 0, 0, 0 },
        /*[14]*/ { (IOExternalMethodAction) &IOFramebuffer::extAcknowledgeNotification,
                    kIOUCVariableStructureSize, 0, 0, 0 },
        /*[15]*/ { (IOExternalMethodAction) &IOFramebuffer::extSetColorConvertTable,
                    1, kIOUCVariableStructureSize, 0, 0 },
        /*[16]*/ { (IOExternalMethodAction) &IOFramebuffer::extSetCLUTWithEntries,
                    2, kIOUCVariableStructureSize, 0, 0 },
        /*[17]*/ { (IOExternalMethodAction) &IOFramebuffer::extValidateDetailedTiming,
                    0, kIOUCVariableStructureSize, 0, kIOUCVariableStructureSize },
        /*[18]*/ { (IOExternalMethodAction) &IOFramebuffer::extGetAttribute,
                    1, 0, 1, 0 },
        /*[19]*/ { (IOExternalMethodAction) &IOFramebuffer::extSetAttribute,
                    2, 0, 0, 0 },
    };

    if (selector > (sizeof(methodTemplate) / sizeof(methodTemplate[0])))
        return (kIOReturnBadArgument);

    ret = super::externalMethod(selector, args, 
                    const_cast<IOExternalMethodDispatch *>(&methodTemplate[selector]), 
                    owner, other);

    return (ret);
}

IOReturn IOFramebufferUserClient::connectClient( IOUserClient * _other )
{
    other = OSDynamicCast(IOFramebuffer, _other->getService());

    if (_other && !other)
        return (kIOReturnBadArgument);
    else
        return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOFramebufferSharedUserClient, IOUserClient)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOFramebufferSharedUserClient * IOFramebufferSharedUserClient::withTask(
    task_t owningTask )
{
    IOFramebufferSharedUserClient * inst;

    inst = new IOFramebufferSharedUserClient;

    if (inst && !inst->init())
    {
        inst->release();
        inst = 0;
    }

    return (inst);
}

bool IOFramebufferSharedUserClient::start( IOService * _owner )
{
    if (!super::start(_owner))
        return (false);

    owner = (IOFramebuffer *) _owner;
    owner->sharedConnect = this;
    setProperty(kIOUserClientSharedInstanceKey, kOSBooleanTrue);

    return (true);
}

void IOFramebufferSharedUserClient::free( void )
{
    owner->sharedConnect = 0;
    super::free();
}

void IOFramebufferSharedUserClient::release() const
{
    super::release();
}

IOReturn IOFramebufferSharedUserClient::clientClose( void )
{
    return (kIOReturnSuccess);
}

IOService * IOFramebufferSharedUserClient::getService( void )
{
    return (owner);
}

IOReturn IOFramebufferSharedUserClient::clientMemoryForType( UInt32 type,
        IOOptionBits * options, IOMemoryDescriptor ** memory )
{
    IOMemoryDescriptor *        mem = 0;
    IOReturn                    err;

    switch (type)
    {
        case kIOFBCursorMemory:
            mem = owner->sharedCursor;
            mem->retain();
            *options = kIOMapReadOnly;
            break;

        case kIOFBVRAMMemory:
            if (kIOReturnSuccess == clientHasPrivilege(current_task(), kIOClientPrivilegeLocalUser))
                mem = owner->getVRAMRange();
            break;
    }

    *memory = mem;
    if (mem)
        err = kIOReturnSuccess;
    else
        err = kIOReturnBadArgument;

    return (err);
}

IOReturn IOFramebufferSharedUserClient::getNotificationSemaphore(
    UInt32 interruptType, semaphore_t * semaphore )
{
    return (owner->getNotificationSemaphore(interruptType, semaphore));
}

