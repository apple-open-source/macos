/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

    /* AppleUSBCDCWCM.cpp - MacOSX implementation of			*/
    /* USB Communication Device Class (CDC) Driver, WMC Interface.	*/

#include <machine/limits.h>			/* UINT_MAX */
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>

#include <IOKit/pwr_mgt/RootDomain.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBInterface.h>

#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/IOSerialDriverSync.h>
#include <IOKit/serial/IOModemSerialStreamSync.h>
#include <IOKit/serial/IORS232SerialStreamSync.h>

#include <UserNotification/KUNCUserNotifications.h>

#define DEBUG_NAME "AppleUSBCDCWCM"

#include "AppleUSBCDCWCM.h"

    // Globals

static IOPMPowerState gOurPowerStates[kNumCDCStates] =
{
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

#define super IOService

OSDefineMetaClassAndStructors(AppleUSBCDCWCM, IOService);

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::probe
//
//		Inputs:		provider - my provider
//
//		Outputs:	IOService - from super::probe, score - probe score
//
//		Desc:		Modify the probe score if necessary (we don't  at the moment)
//
/****************************************************************************************************/

IOService* AppleUSBCDCWCM::probe( IOService *provider, SInt32 *score )
{ 
    IOService   *res;
	
		// If our IOUSBInterface has a "do not match" property, it means that we should not match and need 
		// to bail.  See rdar://3716623
    
    OSBoolean *boolObj = OSDynamicCast(OSBoolean, provider->getProperty("kDoNotClassMatchThisInterface"));
    if (boolObj && boolObj->isTrue())
    {
        XTRACE(this, 0, 0, "probe - provider doesn't want us to match");
        return NULL;
    }

    res = super::probe(provider, score);
    
    return res;
    
}/* end probe */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//				driver for this interface.
//
/****************************************************************************************************/

bool AppleUSBCDCWCM::start(IOService *provider)
{

    fTerminate = false;
    fStopping = false;
	fControlLen = 0;
	fControlMap = NULL;
    
    XTRACE(this, 0, provider, "start - provider.");
    
    if(!super::start(provider))
    {
        ALERT(0, 0, "start - super failed");
        return false;
    }

	// Get my USB provider - the interface

    fInterface = OSDynamicCast(IOUSBInterface, provider);
    if(!fInterface)
    {
        ALERT(0, 0, "start - provider invalid");
        return false;
    }
            
    if (!configureDevice())
    {
        ALERT(0, 0, "start - configureDevice failed");
        return false;
    }
    
    if (!allocateResources()) 
    {
        ALERT(0, 0, "start - allocateResources failed");
        return false;
    }
    
    if (!initForPM(provider))
    {
        ALERT(0, 0, "start - initForPM failed");
        return false;
    }
    
    fInterface->retain();
    
    registerService();
        
    XTRACE(this, 0, 0, "start - successful");
	Log(DEBUG_NAME ": Version number - %s\n", VersionNumber);
    
    return true;
    	
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Stops the driver
//
/****************************************************************************************************/

void AppleUSBCDCWCM::stop(IOService *provider)
{

    XTRACE(this, 0, 0, "stop");
    
    fStopping = true;
    
    releaseResources();
	
    PMstop();
	
	if (fControlMap)
	{
		IOFree(fControlMap, fControlLen);
		fControlMap = NULL;
		fControlLen = 0;
	}
                    
    super::stop(provider);
    
}/* end stop */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::configureWHCM
//
//		Inputs:		None
//
//		Outputs:	return Code - true (configured), false (not configured)
//
//		Desc:		Configures the Wireless handset Control Model interface
//
/****************************************************************************************************/

bool AppleUSBCDCWCM::configureWHCM()
{

    XTRACE(this, 0, 0, "configureWHCM");
    
    fInterfaceNumber = fInterface->GetInterfaceNumber();
    XTRACE(this, 0, fInterfaceNumber, "configureWHCM - Comm interface number.");
    	
    if (!getFunctionalDescriptors())
    {
        XTRACE(this, 0, 0, "configureWHCM - getFunctionalDescriptors failed");
        return false;
    }
    
    return true;

}/* end configureWHCM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::configureDevice
//
//		Inputs:		None
//
//		Outputs:	return Code - true (device configured), false (device not configured)
//
//		Desc:		Finds the appropriate interface etc.
//
/****************************************************************************************************/

bool AppleUSBCDCWCM::configureDevice()
{
    bool	configOK = false;

    XTRACE(this, 0, 0, "configureDevice");

    fInterfaceNumber = fInterface->GetInterfaceNumber();
    fSubClass = fInterface->GetInterfaceSubClass();
    XTRACE(this, fSubClass, fInterfaceNumber, "configureDevice - Subclass and interface number.");
    
    switch (fSubClass)
    {
        case kUSBWirelessHandsetControlModel:
            if (configureWHCM())
            {
                configOK = true;
            }
            break;
        default:
            XTRACE(this, 0, fSubClass, "configureDevice - Unsupported subclass");
            break;
        }

    if (!configOK)
    {
        XTRACE(this, 0, 0, "configureDevice - configuration failed");
        return false;
    }
    
    return true;
    
}/* end configureDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::getFunctionalDescriptors
//
//		Inputs:		
//
//		Outputs:	return - true (descriptors ok), false (somethings not right or not supported)	
//
//		Desc:		Finds all the functional descriptors for the specific interface
//
/****************************************************************************************************/

bool AppleUSBCDCWCM::getFunctionalDescriptors()
{
    bool				gotDescriptors = false;
    UInt16				vers;
    UInt16				*chkVers;
    const FunctionalDescriptorHeader 	*funcDesc = NULL;
    HDRFunctionalDescriptor		*HDRFDesc;		// header functional descriptor
    WHCMFunctionalDescriptor		*WCMFDesc;		// whcm functional descriptor
    UnionFunctionalDescriptor		*UNNFDesc;		// union functional descriptor
       
    XTRACE(this, 0, 0, "getFunctionalDescriptors");
    
    do
    {
        funcDesc = (const FunctionalDescriptorHeader *)fInterface->FindNextAssociatedDescriptor((void*)funcDesc, CS_INTERFACE);
        if (!funcDesc)
        {
            gotDescriptors = true;				// We're done
        } else {
            switch (funcDesc->bDescriptorSubtype)
            {
                case Header_FunctionalDescriptor:
                    HDRFDesc = (HDRFunctionalDescriptor *)funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Header Functional Descriptor");
                    chkVers = (UInt16 *)&HDRFDesc->bcdCDC1;
                    vers = USBToHostWord(*chkVers);
                    if (vers > kUSBRel11)
                    {
                        XTRACE(this, vers, kUSBRel11, "getFunctionalDescriptors - Header descriptor version number is incorrect");
                    }
                    break;
                case Union_FunctionalDescriptor:
                    UNNFDesc = (UnionFunctionalDescriptor *)funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - Union Functional Descriptor");
                    if (UNNFDesc->bFunctionLength > sizeof(FunctionalDescriptorHeader))
                    {
						if (fInterfaceNumber != UNNFDesc->bMasterInterface)
                        {
                            XTRACE(this, fInterfaceNumber, UNNFDesc->bMasterInterface, "getFunctionalDescriptors - Master interface incorrect");
                        } else {
							fControlLen = UNNFDesc->bFunctionLength - sizeof(FunctionalDescriptorHeader);
							fControlLen -= sizeof(UNNFDesc->bMasterInterface);					// Step over master as it's us and we've already checked it
							fControlMap = (UInt8 *)IOMalloc(fControlLen); 
							bcopy(&UNNFDesc->bSlaveInterface, fControlMap, fControlLen);		// Just save them for now...
							XTRACE(this, fControlMap, fControlLen, "getFunctionalDescriptors - Map and length");
						}
                    } else {
                        XTRACE(this, UNNFDesc->bFunctionLength, 0, "getFunctionalDescriptors - Union descriptor length error");
                    }
                    break;
                case WCM_FunctionalDescriptor:
                    WCMFDesc = (WHCMFunctionalDescriptor *)funcDesc;
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - WHCM Functional Descriptor");
                    chkVers = (UInt16 *)&WCMFDesc->bcdCDC1;
                    vers = USBToHostWord(*chkVers);
                    if (vers > kUSBRel10)
                    {
                        XTRACE(this, vers, kUSBRel10, "getFunctionalDescriptors - WHCM descriptor version number is incorrect");
                    }
                    break;
                default:
                    XTRACE(this, funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype, "getFunctionalDescriptors - unknown Functional Descriptor");
                    break;
            }
        }
    } while(!gotDescriptors);
    
        
                
    return true;

}/* end getFunctionalDescriptors */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::allocateResources
//
//		Inputs:		
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration
//
/****************************************************************************************************/

bool AppleUSBCDCWCM::allocateResources()
{

    XTRACE(this, 0, 0, "allocateResources.");

        // Open the interface

    if (!fInterface->open(this))
    {
        XTRACE(this, 0, 0, "allocateResources - open comm interface failed.");
        return false;
    }
    
    return true;
	
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::releaseResources
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void AppleUSBCDCWCM::releaseResources()
{
    XTRACE(this, 0, 0, "releaseResources");
	
    if (fInterface)	
    {
        fInterface->close(this);
        fInterface->release();
        fInterface = NULL;		
    }
        	
}/* end releaseResources */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::resetLogicalHandset
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Reset the logical handset after waking. 
//
/****************************************************************************************************/

void AppleUSBCDCWCM::resetLogicalHandset(void)
{

    XTRACE(this, 0, 0, "resetLogicalHandset");
	
    if ((fStopping) || (fInterface == NULL))
    {
        return;
    }
        
		
	
}/* end resetLogicalHandset */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::message
//
//		Inputs:		type - message type
//				provider - my provider
//				argument - additional parameters
//
//		Outputs:	return Code - kIOReturnSuccess
//
//		Desc:		Handles IOKit messages. 
//
/****************************************************************************************************/

IOReturn AppleUSBCDCWCM::message(UInt32 type, IOService *provider, void *argument)
{	
    
    XTRACE(this, 0, type, "message");
	
    switch (type)
    {
        case kIOMessageServiceIsTerminated:
            XTRACE(this, 0, type, "message - kIOMessageServiceIsTerminated");
            fTerminate = true;		// We're being terminated (unplugged)
            releaseResources();
            return kIOReturnSuccess;			
        case kIOMessageServiceIsSuspended: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceIsSuspended");
            break;			
        case kIOMessageServiceIsResumed: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceIsResumed");
            break;			
        case kIOMessageServiceIsRequestingClose: 
            XTRACE(this, 0, type, "message - kIOMessageServiceIsRequestingClose"); 
            break;
        case kIOMessageServiceWasClosed: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceWasClosed"); 
            break;
        case kIOMessageServiceBusyStateChange: 	
            XTRACE(this, 0, type, "message - kIOMessageServiceBusyStateChange"); 
            break;
        case kIOUSBMessagePortHasBeenResumed: 	
            XTRACE(this, 0, type, "message - kIOUSBMessagePortHasBeenResumed");
            break;
        case kIOUSBMessageHubResumePort:
            XTRACE(this, 0, type, "message - kIOUSBMessageHubResumePort");
            break;
        default:
            XTRACE(this, 0, type, "message - unknown message"); 
            break;
    }
    
    return kIOReturnUnsupported;
    
}/* end message */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::initForPM
//
//		Inputs:		provider - my provider
//
//		Outputs:	return code - true(initialized), false(failed)
//
//		Desc:		Add ourselves to the power management tree so we can do
//				the right thing on sleep/wakeup. 
//
/****************************************************************************************************/

bool AppleUSBCDCWCM::initForPM(IOService *provider)
{
    XTRACE(this, 0, 0, "initForPM");
    
    fPowerState = kCDCPowerOnState;				// init our power state to be 'on'
    PMinit();							// init power manager instance variables
    provider->joinPMtree(this);					// add us to the power management tree
    if (pm_vars != NULL)
    {
    
            // register ourselves with ourself as policy-maker
        
        registerPowerDriver(this, gOurPowerStates, kNumCDCStates);
        return true;
    } else {
        XTRACE(this, 0, 0, "initForPM - Initializing power manager failed");
    }
    
    return false;
    
}/* end initForPM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::initialPowerStateForDomainState
//
//		Inputs:		flags - 
//
//		Outputs:	return code - Current power state
//
//		Desc:		Request for our initial power state. 
//
/****************************************************************************************************/

unsigned long AppleUSBCDCWCM::initialPowerStateForDomainState(IOPMPowerFlags flags)
{

    XTRACE(this, 0, flags, "initialPowerStateForDomainState");
    
    return fPowerState;
    
}/* end initialPowerStateForDomainState */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDCWCM::setPowerState
//
//		Inputs:		powerStateOrdinal - on/off
//
//		Outputs:	return code - IOPMNoErr, IOPMAckImplied or IOPMNoSuchState
//
//		Desc:		Request to turn device on or off. 
//
/****************************************************************************************************/

IOReturn AppleUSBCDCWCM::setPowerState(unsigned long powerStateOrdinal, IOService *whatDevice)
{

    XTRACE(this, 0, powerStateOrdinal, "setPowerState");
    
    if (powerStateOrdinal == kCDCPowerOffState || powerStateOrdinal == kCDCPowerOnState)
    {
        if (powerStateOrdinal == fPowerState)
            return IOPMAckImplied;

        fPowerState = powerStateOrdinal;
        if (fPowerState == kCDCPowerOnState)
        {
            resetLogicalHandset();
        }
    
        return IOPMNoErr;
    }
    
    return IOPMNoSuchState;
    
}/* end setPowerState */