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
#include <IOKit/usb/IOUSBControllerV3.h>
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

	IOReturn ret = clientHasPrivilege(security_id, kIOClientPrivilegeAdministrator);
	USBLog(6,"IOUSBControllerUserClient[%p]::initWithTask  clientHasPrivilege returned 0x%x (%s)", this, ret, USBStringFromReturn(ret));
	if ( ret == kIOReturnSuccess )
	{
		fIsTaskPrileged = true;
	}
	else 
	{
		fIsTaskPrileged = false;
	}

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
    IOWorkLoop	*		workLoop = NULL;
    IOCommandGate *		commandGate = NULL;

    fOwner = OSDynamicCast(IOUSBController, provider);

    if (!fOwner)
        return false;

    if (!super::start(provider))
        return false;

    fMemMap = fOwner->getProvider()->mapDeviceMemoryWithIndex(0);
    if (!fMemMap)
    {
        USBLog(1, "IOUSBControllerUserClient::start - unable to get a memory map");
		USBTrace( kUSBTControllerUserClient,  kTPControllerUCStart, (uintptr_t)this, (uintptr_t)fOwner, kIOReturnNoResources, 0 );
        return kIOReturnNoResources;
    }

    commandGate = IOCommandGate::commandGate(this);
	
    if (!commandGate)
    {
		USBError(1, "IOUSBControllerUserClient[%p]::start - unable to create command gate",  this);
		goto ErrorExit;
    }
	
	// Get the USB controller workloop
    workLoop = fOwner->getWorkLoop();
    if (!workLoop)
    {
		USBError(1, "IOUSBControllerUserClient[%p]::start - unable to find my workloop",  this);
		goto ErrorExit;
    }
    workLoop->retain();
	
    if (workLoop->addEventSource(commandGate) != kIOReturnSuccess)
    {
		USBError(1, "IOUSBControllerUserClient[%p]::start - unable to add gate to work loop",  this);
		goto ErrorExit;
    }
	
	// Now that we have succesfully added our gate to the workloop, set our member variables
    //
    fGate = commandGate;
    fWorkLoop = workLoop;

    return true;

ErrorExit:
	
	if ( commandGate != NULL )
	{
		commandGate->release();
		commandGate = NULL;
	}
	
    if ( workLoop != NULL )
    {
        workLoop->release();
        workLoop = NULL;
    }
	
    return false;
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




void
IOUSBControllerUserClient::stop( IOService * provider )
{
    USBLog(6, "IOUSBControllerUserClient[%p]::stop(%p)",  this, provider);
	
    if (fMemMap)
    {
        fMemMap->release();
        fMemMap = NULL;
    }

	if (fWorkLoop && fGate)
	{
		fWorkLoop->removeEventSource(fGate);
		
		if (fGate)
		{
			fGate->release();
			fGate = NULL;
		}
		
		if (fWorkLoop)
		{
			fWorkLoop->release();
			fWorkLoop = NULL;
		}
	}
	
	super::stop(provider);
	
    USBLog(7, "-IOUSBControllerUserClient[%p]::stop(%p)",  this, provider);
	super::stop( provider );

}

IOReturn
IOUSBControllerUserClient::clientClose( void )
{
    /*
     * Kill ourselves off if the client closes or the client dies.
     */
    if ( !isInactive())
        terminate();

    return( kIOReturnSuccess );
}



