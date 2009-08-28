/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright © 1998-2009 Apple Inc.  All rights reserved.
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
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>

#include <IOKit/usb/IOUSBController.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBLog.h>

#include "IOUSBControllerUserClient.h"
#include "USBTracepoints.h"

#define super IOUserClient

#ifndef kIOUserClientCrossEndianKey
#define kIOUserClientCrossEndianKey "IOUserClientCrossEndian"
#endif

#ifndef kIOUserClientCrossEndianCompatibleKey
#define kIOUserClientCrossEndianCompatibleKey "IOUserClientCrossEndianCompatible"
#endif


//================================================================================================
//	IOKit stuff and Constants
//================================================================================================
//
OSDefineMetaClassAndStructors(IOUSBControllerUserClient, IOUserClient)

enum {
    kMethodObjectThis = 0,
    kMethodObjectOwner
};

//================================================================================================
//	Method Table
//================================================================================================
//
const IOExternalMethod
IOUSBControllerUserClient::sMethods[kNumUSBControllerMethods] =
{
    {	// kUSBControllerUserClientOpen
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod ) &IOUSBControllerUserClient::open,		// func
        kIOUCScalarIScalarO,					// flags
        1,							// # of params in
        0							// # of params out
    },
    {	// kUSBControllerUserClientClose
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod ) &IOUSBControllerUserClient::close,		// func
        kIOUCScalarIScalarO,					// flags
        0,							// # of params in
        0							// # of params out
    },
    {	// kUSBControllerUserClientEnableLogger
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod ) &IOUSBControllerUserClient::EnableKernelLogger,	// func
        kIOUCScalarIScalarO,					// flags
        1,							// # of params in
        0							// # of params out
    },
    {	// kUSBControllerUserClientSetDebuggingLevel
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod ) &IOUSBControllerUserClient::SetDebuggingLevel,// func
        kIOUCScalarIScalarO,					// flags
        1,							// # of params in
        0							// # of params out
    },
    {	// kUSBControllerUserClientSetDebuggingType
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod ) &IOUSBControllerUserClient::SetDebuggingType,// func
        kIOUCScalarIScalarO,					// flags
        1,							// # of params in
        0							// # of params out
    },
    {	// kUSBControllerUserClientGetDebuggingLevel
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod ) &IOUSBControllerUserClient::GetDebuggingLevel,// func
        kIOUCScalarIScalarO,					// flags
        0,							// # of params in
        1							// # of params out
    },
    {	// kUSBControllerUserClientGetDebuggingType
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod ) &IOUSBControllerUserClient::GetDebuggingType,// func
        kIOUCScalarIScalarO,					// flags
        0,							// # of params in
        1							// # of params out
    },
    {	// kUSBControllerUserClientSetTestMode
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod )&IOUSBControllerUserClient::SetTestMode,	// func
        kIOUCScalarIScalarO,					// flags
        2,							// # of params in
        0							// # of params out
    }
    ,
    {	// kUSBControllerUserClientReadRegister
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod )&IOUSBControllerUserClient::ReadRegister,	// func
        kIOUCScalarIScalarO,					// flags
        2,							// # of params in
        1							// # of params out
    }
    ,
    {	// kUSBControllerUserClientWriteRegister
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod )&IOUSBControllerUserClient::WriteRegister,	// func
        kIOUCScalarIScalarO,					// flags
        3,							// # of params in
        0							// # of params out
    }
};

const IOItemCount
IOUSBControllerUserClient::sMethodCount = sizeof( IOUSBControllerUserClient::sMethods ) /
sizeof( IOUSBControllerUserClient::sMethods[ 0 ] );

void
IOUSBControllerUserClient::SetExternalMethodVectors()
{
    fMethods = sMethods;
    fNumMethods = kNumUSBControllerMethods;
}



IOExternalMethod *
IOUSBControllerUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if (index < (UInt32)fNumMethods)
    {
        if ((IOService*)kMethodObjectThis == fMethods[index].object)
            *target = this;
        else if ((IOService*)kMethodObjectOwner == fMethods[index].object)
            *target = fOwner;
        else
            return NULL;
        return (IOExternalMethod *) &fMethods[index];
    }
    else
        return NULL;
}



//================================================================================================

bool
IOUSBControllerUserClient::initWithTask(task_t owningTask, void *security_id, UInt32 type, OSDictionary * properties)
{
	if ( properties != NULL )
	{
		properties->setObject( kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue);
	}
		
	if (!owningTask)
        return false;

    fTask = owningTask;
    fOwner = NULL;
    fGate = NULL;
    fDead = false;

    SetExternalMethodVectors();

    return (super::initWithTask(owningTask, security_id , type, properties));
}



bool
IOUSBControllerUserClient::start( IOService * provider )
{
    fOwner = OSDynamicCast(IOUSBController, provider);

    if (!fOwner)
        return false;

    if(!super::start(provider))
        return false;

    fMemMap = fOwner->getProvider()->mapDeviceMemoryWithIndex(0);
    if (!fMemMap)
    {
        USBLog(1, "IOUSBControllerUserClient::start - unable to get a memory map");
		USBTrace( kUSBTControllerUserClient,  kTPControllerUCStart, (uintptr_t)this, (uintptr_t)fOwner, kIOReturnNoResources, 0 );
        return kIOReturnNoResources;
    }

    return true;
}


IOReturn
IOUSBControllerUserClient::open(bool seize)
{
    IOOptionBits	options = seize ? (IOOptionBits) kIOServiceSeize : 0;

    USBLog(1, "+IOUSBControllerUserClient::open");
	USBTrace( kUSBTControllerUserClient,  kTPControllerUCOpen, (uintptr_t)this, (uintptr_t)fOwner, options, seize );
	
    if (!fOwner)
        return kIOReturnNotAttached;

    if (!fOwner->open(this, options))
        return kIOReturnExclusiveAccess;

    return kIOReturnSuccess;
}



IOReturn
IOUSBControllerUserClient::close()
{
    if (!fOwner)
        return kIOReturnNotAttached;

    if (fOwner && (fOwner->isOpen(this)))
        fOwner->close(this);

    return kIOReturnSuccess;
}

IOReturn
IOUSBControllerUserClient::EnableKernelLogger(bool enable)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    KernelDebugEnable(enable);

    return kIOReturnSuccess;
}

IOReturn
IOUSBControllerUserClient::SetDebuggingLevel(KernelDebugLevel inLevel)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    KernelDebugSetLevel(inLevel);

    return kIOReturnSuccess;
}

IOReturn
IOUSBControllerUserClient::SetDebuggingType(KernelDebuggingOutputType inType)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    KernelDebugSetOutputType(inType);

    return kIOReturnSuccess;
}

IOReturn
IOUSBControllerUserClient::GetDebuggingLevel(KernelDebugLevel * inLevel)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    *inLevel = KernelDebugGetLevel();

    return kIOReturnSuccess;
}

IOReturn
IOUSBControllerUserClient::GetDebuggingType(KernelDebuggingOutputType * inType)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    *inType = KernelDebugGetOutputType();

    return kIOReturnSuccess;
}


IOReturn
IOUSBControllerUserClient::SetTestMode(UInt32 mode, UInt32 port)
{
    // this method only available for v2 controllers
    IOUSBControllerV2	*v2 = OSDynamicCast(IOUSBControllerV2, fOwner);

    if (!v2)
        return kIOReturnNotAttached;

    return v2->SetTestMode(mode, port);
}


IOReturn
IOUSBControllerUserClient::ReadRegister(UInt32 offset, UInt32 size, void *value)
{
    UInt8	bVal;
    UInt16	wVal;
    UInt32	lVal;

    USBLog(1, "+IOUSBControllerUserClient::ReadRegister Offset(0x%x), Size (%d)", (int)offset, (int)size);
	USBTrace( kUSBTControllerUserClient,  kTPControllerUCReadRegister, (uintptr_t)this, (uintptr_t)fOwner, (int)offset, (int)size );
	
    if (!fOwner)
        return kIOReturnNotAttached;

    if (!fMemMap)
        return kIOReturnNoResources;

    switch (size)
    {
        case 8:
            bVal = *((UInt8 *)fMemMap->getVirtualAddress() + offset);
            *(UInt8*)value = bVal;
            USBLog(4, "IOUSBControllerUserClient::ReadRegister - got byte value 0x%x", bVal);
            break;

        case 16:
            wVal = OSReadLittleInt16((void*)(fMemMap->getVirtualAddress()), offset);
            *(UInt16*)value = wVal;
            USBLog(4, "IOUSBControllerUserClient::ReadRegister - got word value 0x%x", wVal);
            break;

        case 32:
            lVal = OSReadLittleInt32((void*)(fMemMap->getVirtualAddress()), offset);
            *(UInt32*)value = lVal;
            USBLog(4, "IOUSBControllerUserClient::ReadRegister - got (uint32_t) value 0x%x", (uint32_t)lVal);
            break;

        default:
            USBLog(1, "IOUSBControllerUserClient::ReadRegister - invalid size");
			USBTrace( kUSBTControllerUserClient,  kTPControllerUCReadRegister, (uintptr_t)this, (uintptr_t)fOwner, kIOReturnBadArgument, 0 );
            return kIOReturnBadArgument;
    }
    return kIOReturnSuccess;
}


IOReturn
IOUSBControllerUserClient::WriteRegister(UInt32 offset, UInt32 size, UInt32 value)
{
    USBLog(1, "+IOUSBControllerUserClient::WriteRegister Offset(0x%x), Size (%d) Value (0x%x)", (int)offset, (int)size, (int)value);
	USBTrace( kUSBTControllerUserClient,  kTPControllerUCWriteRegister, (uintptr_t)this, (uintptr_t)fOwner, (int)offset, (int)size );
	USBTrace( kUSBTControllerUserClient,  kTPControllerUCWriteRegister, 1, (int)value, 0, 0 );

    if (!fOwner)
        return kIOReturnNotAttached;

    return kIOReturnSuccess;
}

void
IOUSBControllerUserClient::stop( IOService * provider )
{
	super::stop( provider );

    if (fMemMap)
    {
        fMemMap->release();
        fMemMap = NULL;
    }
}

IOReturn
IOUSBControllerUserClient::clientClose( void )
{
    /*
     * Kill ourselves off if the client closes or the client dies.
     */
    if( !isInactive())
        terminate();

    return( kIOReturnSuccess );
}



