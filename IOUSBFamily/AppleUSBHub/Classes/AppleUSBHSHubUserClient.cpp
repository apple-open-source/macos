/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright � 1998-2009 Apple Inc.  All rights reserved.
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

#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>


#include "AppleUSBHSHubUserClient.h"
#include "AppleUSBEHCI.h"
#include "USBTracepoints.h"

#define super IOUserClient
OSDefineMetaClassAndStructors(AppleUSBHSHubUserClient, IOUserClient)


enum {
    kMethodObjectThis = 0,
    kMethodObjectOwner
};

//================================================================================================
//	Method Table
//================================================================================================
//
const IOExternalMethod
AppleUSBHSHubUserClient::sMethods[kNumUSBHSHubMethods] =
{
    {	// kAppleUSBHSHubUserClientOpen
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod ) &AppleUSBHSHubUserClient::open,		// func
        kIOUCScalarIScalarO,					// flags
        1,							// # of params in
        0							// # of params out
    },
    {	// kAppleUSBHSHubUserClientClose
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod ) &AppleUSBHSHubUserClient::close,		// func
        kIOUCScalarIScalarO,					// flags
        0,							// # of params in
        0							// # of params out
    },
    {	// kAppleUSBHSHubUserClientIsEHCIRootHub
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod )&AppleUSBHSHubUserClient::IsEHCIRootHub,	// func
        kIOUCScalarIScalarO,					// flags
        0,							// # of params in
        1							// # of params out
    },
    {	// kAppleUSBHSHubUserClientEnterTestMode
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod )&AppleUSBHSHubUserClient::EnterTestMode,	// func
        kIOUCScalarIScalarO,					// flags
        0,							// # of params in
        0							// # of params out
    },
    {	// kAppleUSBHSHubUserClientLeaveTestMode
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod )&AppleUSBHSHubUserClient::LeaveTestMode,	// func
        kIOUCScalarIScalarO,					// flags
        0,							// # of params in
        0							// # of params out
    },
    {	// kAppleUSBHSHubUserClientGetNumberOfPorts
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod )&AppleUSBHSHubUserClient::GetNumberOfPorts,	// func
        kIOUCScalarIScalarO,					// flags
        0,							// # of params in
        1							// # of params out
    },
    {	// kAppleUSBHSHubUserClientPutPortIntoTestMode
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod )&AppleUSBHSHubUserClient::PutPortIntoTestMode,	// func
        kIOUCScalarIScalarO,					// flags
        2,							// # of params in
        0							// # of params out
    },
    {	// kAppleUSBHSHubUserClientGetLocationID
        (IOService*)kMethodObjectThis,							// object
        ( IOMethod )&AppleUSBHSHubUserClient::GetLocationID,	// func
        kIOUCScalarIScalarO,									// flags
        0,														// # of params in
        1														// # of params out
    },
    {	// kAppleUSBHSHubUserClientSupportsIndicators
        (IOService*)kMethodObjectThis,								// object
        ( IOMethod )&AppleUSBHSHubUserClient::SupportsIndicators,	// func
        kIOUCScalarIScalarO,										// flags
        0,															// # of params in
        1															// # of params out
    },
    {	// kAppleUSBHSHubUserClientSetIndicatorForPort
        (IOService*)kMethodObjectThis,								// object
        ( IOMethod )&AppleUSBHSHubUserClient::SetIndicatorForPort,	// func
        kIOUCScalarIScalarO,										// flags
        2,															// # of params in
        0															// # of params out
    },
    {	// kAppleUSBHSHubUserClientGetPortIndicatorControl
        (IOService*)kMethodObjectThis,								// object
        ( IOMethod )&AppleUSBHSHubUserClient::GetPortIndicatorControl,	// func
        kIOUCScalarIScalarO,										// flags
        1,															// # of params in
        1															// # of params out
    },
    {	// kAppleUSBHSHubUserClientSetIndicatorsToAutomatic
        (IOService*)kMethodObjectThis,									// object
        ( IOMethod )&AppleUSBHSHubUserClient::SetIndicatorsToAutomatic,	// func
        kIOUCScalarIScalarO,											// flags
        0,																// # of params in
        0																// # of params out
    },
    {	// kAppleUSBHSHubUserClientGetPowerSwitchingMode
        (IOService*)kMethodObjectThis,								// object
        ( IOMethod )&AppleUSBHSHubUserClient::GetPowerSwitchingMode,// func
        kIOUCScalarIScalarO,										// flags
        0,															// # of params in
        1															// # of params out
    },
    {	// kAppleUSBHSHubUserClientSetPortPower
        (IOService*)kMethodObjectThis,								// object
        ( IOMethod )&AppleUSBHSHubUserClient::SetPortPower,			// func
        kIOUCScalarIScalarO,										// flags
        2,															// # of params in
        0															// # of params out
    },
    {	// kAppleUSBHSHubUserClientGetPortPower
        (IOService*)kMethodObjectThis,								// object
        ( IOMethod )&AppleUSBHSHubUserClient::GetPortPower,			// func
        kIOUCScalarIScalarO,										// flags
        1,															// # of params in
        1															// # of params out
    }
};

#pragma mark IOKit
const IOItemCount
AppleUSBHSHubUserClient::sMethodCount = sizeof( AppleUSBHSHubUserClient::sMethods ) / sizeof( AppleUSBHSHubUserClient::sMethods[ 0 ] );

void
AppleUSBHSHubUserClient::SetExternalMethodVectors()
{
    fMethods = sMethods;
    fNumMethods = kNumUSBHSHubMethods;
}



IOExternalMethod *
AppleUSBHSHubUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
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
AppleUSBHSHubUserClient::initWithTask(task_t owningTask, void *security_id, UInt32 type, OSDictionary * properties)
{
    USBLog(1, "AppleUSBHSHubUserClient::initWithTask(type %d)", (uint32_t)type);
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCInitWithTask, (uintptr_t)this, (uintptr_t)owningTask, (uintptr_t)security_id, type );

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
AppleUSBHSHubUserClient::start( IOService * provider )
{    
    fOwner = OSDynamicCast(AppleUSBHub, provider);
    USBLog(1, "+AppleUSBHSHubUserClient::start (%p)", fOwner);
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCStart, (uintptr_t)this, (uintptr_t)fOwner, 0, 0 );
	
    if (!fOwner)
        return false;
	
    if(!super::start(provider))
        return false;

    return true;
}


IOReturn
AppleUSBHSHubUserClient::open(bool seize)
{
    IOOptionBits	options = seize ? (IOOptionBits) kIOServiceSeize : 0;

    USBLog(5, "+AppleUSBHSHubUserClient::open (fOwner = %p)", fOwner);
    if (!fOwner)
        return kIOReturnNotAttached;

    if (!fOwner->open(this, options))
        return kIOReturnExclusiveAccess;

    USBLog(5, "AppleUSBHSHubUserClient::open - open returned no error");

    return kIOReturnSuccess;
}



IOReturn
AppleUSBHSHubUserClient::close()
{
    USBLog(1, "+AppleUSBHSHubUserClient::close");
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCClose, (uintptr_t)this, (uintptr_t)fOwner, 0, 0 );
	
    if (!fOwner)
        return kIOReturnNotAttached;

    if (fOwner && (fOwner->isOpen(this)))
        fOwner->close(this);

    return kIOReturnSuccess;
}


#pragma mark Utilities

IOReturn
AppleUSBHSHubUserClient::IsEHCIRootHub(UInt32 *ret)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    *ret = fOwner->IsHSRootHub();
    return kIOReturnSuccess;
}

IOReturn
AppleUSBHSHubUserClient::GetNumberOfPorts(UInt32 *numPorts)
{
    
    if (!fOwner)
        return kIOReturnNotAttached;
	
    *numPorts = fOwner->_hubDescriptor.numPorts;
    USBLog(1, "%s[%p]::GetNumberOfPorts - returning %d", getName(), this, (uint32_t)*numPorts);
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCGetNumberOfPorts, (uintptr_t)this, (uintptr_t)fOwner, *numPorts, 0 );
	
    return kIOReturnSuccess;
}


IOReturn
AppleUSBHSHubUserClient::GetLocationID(UInt32 *locID)
{
    USBLog(1, "+AppleUSBHSHubUserClient::GetLocationID");
	
    if (!fOwner)
        return kIOReturnNotAttached;

	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCGetLocationID, (uintptr_t)this, (uintptr_t)fOwner, fOwner->_locationID, 0 );
	
    *locID = fOwner->_locationID;
    return kIOReturnSuccess;
}

#pragma mark Test Mode Support
IOReturn
AppleUSBHSHubUserClient::EnterTestMode(void)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    if (!fOwner->isOpen(this))
	return kIOReturnBadArgument;
	
    return fOwner->EnterTestMode();
}


IOReturn
AppleUSBHSHubUserClient::LeaveTestMode(void)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    return fOwner->LeaveTestMode();
}



IOReturn
AppleUSBHSHubUserClient::PutPortIntoTestMode(UInt32 port, UInt32 mode)
{
    USBLog(1, "+AppleUSBHSHubUserClient::PutPortIntoTestMode");

	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCPutPortIntoTestMode, (uintptr_t)this, (uintptr_t)fOwner, port, mode);
	
    if (!fOwner)
        return kIOReturnNotAttached;
	
    if (!fOwner->isOpen(this))
    {
		USBLog(1, "AppleUSBHSHubUserClient::PutPortIntoTestMode - fOwner (%p) is not open", fOwner);
		USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCPutPortIntoTestMode, (uintptr_t)this, (uintptr_t)fOwner, kIOReturnBadArgument, 0);
		return kIOReturnBadArgument;
    }
	
    return fOwner->PutPortIntoTestMode(port, mode);
}



#pragma mark Hub Indicator Support
//================================================================================================
//   SupportsIndicators
//================================================================================================
IOReturn
AppleUSBHSHubUserClient::SupportsIndicators(UInt32 *indicatorSupport)
{
    if (!fOwner)
        return kIOReturnNotAttached;
	
	if ( (USBToHostWord(fOwner->_hubDescriptor.characteristics) & kHubPortIndicatorMask) == 0)
	{
		*indicatorSupport = 0;
	}
	else
	{
		*indicatorSupport = 1;
	}
	
    USBLog(1, "+AppleUSBHSHubUserClient::AppleUSB returning %d", (uint32_t)*indicatorSupport);
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCSupportsIndicators, (uintptr_t)this, (uintptr_t)fOwner, *indicatorSupport, 0);
	
	return kIOReturnSuccess;
}

//================================================================================================
//   SetIndicatorForPort
//================================================================================================
IOReturn
AppleUSBHSHubUserClient::SetIndicatorForPort(UInt32 portNumber, UInt32 selector)
{
    USBLog(1, "+AppleUSBHSHubUserClient::SetIndicatorForPort (port %d, %d)", (uint32_t)portNumber, (uint32_t)selector);
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCSetIndicatorForPort, (uintptr_t)this, (uintptr_t)fOwner, (uint32_t)portNumber, (uint32_t)selector);
	
    if (!fOwner)
        return kIOReturnNotAttached;
	
    if (!fOwner->isOpen(this))
    {
		USBLog(1, "AppleUSBHSHubUserClient::SetIndicatorForPort - fOwner (%p) is not open", fOwner);
		USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCSetIndicatorForPort, (uintptr_t)this, (uintptr_t)fOwner, kIOReturnBadArgument, 0);
		return kIOReturnBadArgument;
    }
	
    return fOwner->SetIndicatorForPort(portNumber, selector);
}

//================================================================================================
//   GetIndicatorForPort
//================================================================================================
IOReturn
AppleUSBHSHubUserClient::GetPortIndicatorControl(UInt32 portNumber, UInt32 *defaultColors)
{
	IOReturn	kr;
	
    if (!fOwner)
        return kIOReturnNotAttached;
	
    if (!fOwner->isOpen(this))
    {
		USBLog(1, "AppleUSBHSHubUserClient::GetIndicatorForPort - fOwner (%p) is not open", fOwner);
		USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCGetPortIndicatorControl, (uintptr_t)this, (uintptr_t)fOwner, kIOReturnBadArgument, 0);
		return kIOReturnBadArgument;
    }
    
	kr = fOwner->GetPortIndicatorControl(portNumber, defaultColors);
	
    USBLog(1, "+AppleUSBHSHubUserClient::GetIndicatorForPort (port %d, %d), 0x%x", (uint32_t)portNumber, (uint32_t)*defaultColors, kr);
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCGetPortIndicatorControl, (uintptr_t)fOwner, (uint32_t)portNumber, (uint32_t)*defaultColors, kr);
	
    return kr;
}

//================================================================================================
//   SetIndicatorsToAutomatic
//================================================================================================
IOReturn
AppleUSBHSHubUserClient::SetIndicatorsToAutomatic()
{
    USBLog(1, "+AppleUSBHSHubUserClient::SetIndicatorsToAutomatic ");
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCSetIndicatorsToAutomatic, (uintptr_t)this, (uintptr_t)fOwner, 0, 0);
	
    if (!fOwner)
        return kIOReturnNotAttached;
	
    if (!fOwner->isOpen(this))
    {
		USBLog(1, "AppleUSBHSHubUserClient::SetIndicatorsToAutomatic - fOwner (%p) is not open", fOwner);
		USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCSetIndicatorsToAutomatic, (uintptr_t)this, (uintptr_t)fOwner, kIOReturnBadArgument, 0);
		return kIOReturnBadArgument;
    }
    
	return fOwner->SetIndicatorsToAutomatic();
}

#pragma mark Port Power Support

//================================================================================================
//   GetPowerSwitchingMode
//================================================================================================
IOReturn
AppleUSBHSHubUserClient::GetPowerSwitchingMode(UInt32 *mode)
{
    if (!fOwner)
        return kIOReturnNotAttached;
	
	if ( !USBToHostWord(fOwner->_hubDescriptor.characteristics) & kPerPortSwitchingBit )
	{
		*mode = kHubSupportsGangPower;
	}
	else
	{
		*mode = kHubSupportsIndividualPortPower;
	}
	
    USBLog(1, "+AppleUSBHSHubUserClient::GetPowerSwitchingMode returning %d", (uint32_t)*mode);
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCGetPowerSwitchingMode, (uintptr_t)this, (uintptr_t)fOwner, *mode, 0 );
	
	return kIOReturnSuccess;
}

//================================================================================================
//   GetPortPower
//================================================================================================
IOReturn
AppleUSBHSHubUserClient::GetPortPower(UInt32 portNumber, UInt32 *on)
{
	IOReturn	kr;
	
    if (!fOwner)
        return kIOReturnNotAttached;
	
    if (!fOwner->isOpen(this))
    {
		USBLog(1, "AppleUSBHSHubUserClient::GetPortPower - fOwner (%p) is not open", fOwner);
		USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCGetPortPower, (uintptr_t)this, (uintptr_t)fOwner, kIOReturnBadArgument, 0 );
		return kIOReturnBadArgument;
    }
    
	kr = fOwner->GetPortPower(portNumber, on);
	
    USBLog(1, "+AppleUSBHSHubUserClient::GetPortPower (port %d, to %s), 0x%x", (uint32_t)portNumber, *on ? "ON" : "OFF", kr);
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCGetPortPower, (uintptr_t)fOwner, portNumber, *on, kr );
	
    return kr;
}

//================================================================================================
//   SetPortPower
//================================================================================================
IOReturn
AppleUSBHSHubUserClient::SetPortPower(UInt32 portNumber, UInt32 on)
{
    USBLog(1, "+AppleUSBHSHubUserClient::SetPortPower (port %d to %s)", (uint32_t)portNumber, on ? "ON" : "OFF");
	USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCSetPortPower, (uintptr_t)fOwner, portNumber, on, 0);
	
    if (!fOwner)
        return kIOReturnNotAttached;
	
    if (!fOwner->isOpen(this))
    {
		USBLog(1, "AppleUSBHSHubUserClient::SetPortPower - fOwner (%p) is not open", fOwner);
		USBTrace( kUSBTHSHubUserClient,  kTPHSHubUCSetPortPower, (uintptr_t)this, (uintptr_t)fOwner, kIOReturnBadArgument, 0 );
		return kIOReturnBadArgument;
    }
    
	return fOwner->SetPortPower(portNumber, on);
}

