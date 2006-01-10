/*
 *
 * @APPLE_LICENSE_HEADER_START@
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
        (IOService*)kMethodObjectThis,				// object
        ( IOMethod )&AppleUSBHSHubUserClient::GetLocationID,	// func
        kIOUCScalarIScalarO,					// flags
        0,							// # of params in
        1							// # of params out
    }
};

const IOItemCount
AppleUSBHSHubUserClient::sMethodCount = sizeof( AppleUSBHSHubUserClient::sMethods ) / sizeof( AppleUSBHSHubUserClient::sMethods[ 0 ] );

void
AppleUSBHSHubUserClient::SetExternalMethodVectors()
{
    fMethods = sMethods;
    fNumMethods = kNumUSBControllerMethods;
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
    USBLog(1, "AppleUSBHSHubUserClient::initWithTask(type %ld)", type);

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
    if (!fOwner)
        return kIOReturnNotAttached;

    if (fOwner && (fOwner->isOpen(this)))
        fOwner->close(this);

    return kIOReturnSuccess;
}



IOReturn
AppleUSBHSHubUserClient::IsEHCIRootHub(UInt32 *ret)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    *ret = fOwner->IsHSRootHub();
    return kIOReturnSuccess;
}


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
AppleUSBHSHubUserClient::GetNumberOfPorts(UInt32 *numPorts)
{
    
    if (!fOwner)
        return kIOReturnNotAttached;

    *numPorts = fOwner->_hubDescriptor.numPorts;
    USBLog(1, "%s[%p]::GetNumberOfPorts - returning %d", getName(), this, *numPorts);
    return kIOReturnSuccess;
}


IOReturn
AppleUSBHSHubUserClient::PutPortIntoTestMode(UInt32 port, UInt32 mode)
{
    USBLog(1, "+AppleUSBHSHubUserClient::PutPortIntoTestMode");
    if (!fOwner)
        return kIOReturnNotAttached;
	
    if (!fOwner->isOpen(this))
    {
	USBLog(1, "AppleUSBHSHubUserClient::PutPortIntoTestMode - fOwner (%p) is not open", fOwner);
	return kIOReturnBadArgument;
    }
	
    return fOwner->PutPortIntoTestMode(port, mode);
}


IOReturn
AppleUSBHSHubUserClient::GetLocationID(UInt32 *locID)
{
    USBLog(1, "+AppleUSBHSHubUserClient::GetLocationID");
    if (!fOwner)
        return kIOReturnNotAttached;

    *locID = fOwner->_locationID;
    return kIOReturnSuccess;
}

