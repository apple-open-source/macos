/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

    /* AppleUSBCDC.cpp - MacOSX implementation of		*/
    /* USB Communication Device Class (CDC) Driver.		*/

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

#include "AppleUSBCDCCommon.h"
#include "AppleUSBCDC.h"
#include "AppleUSBCDCPrivate.h"

    // Globals

#if USE_ELG
    com_apple_iokit_XTrace	*gXTrace = 0;
    UInt32			gTraceID;
#endif

#define super IOService

OSDefineMetaClassAndStructors(AppleUSBCDC, IOService);

#if USE_ELG
#define DEBUG_NAME "AppleUSBCDC"

/****************************************************************************************************/
//
//		Function:	findKernelLoggerCDC
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Just like the name says
//
/****************************************************************************************************/

IOReturn findKernelLoggerCDC()
{
    OSIterator		*iterator = NULL;
    OSDictionary	*matchingDictionary = NULL;
    IOReturn		error = 0;
	
	// Get matching dictionary
	
    matchingDictionary = IOService::serviceMatching("com_apple_iokit_XTrace");
    if (!matchingDictionary)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[findKernelLoggerCDC] Couldn't create a matching dictionary.\n");
        goto exit;
    }
	
	// Get an iterator
	
    iterator = IOService::getMatchingServices(matchingDictionary);
    if (!iterator)
    {
        error = kIOReturnError;
        IOLog(DEBUG_NAME "[findKernelLoggerCDC] No XTrace logger found.\n");
        goto exit;
    }
	
	// User iterator to find each com_apple_iokit_XTrace instance. There should be only one, so we
	// won't iterate
	
    gXTrace = (com_apple_iokit_XTrace*)iterator->getNextObject();
    if (gXTrace)
    {
        IOLog(DEBUG_NAME "[findKernelLoggerCDC] Found XTrace logger at %p.\n", gXTrace);
    }
	
exit:
	
    if (error != kIOReturnSuccess)
    {
        gXTrace = NULL;
        IOLog(DEBUG_NAME "[findKernelLoggerCDC] Could not find a logger instance. Error = %X.\n", error);
    }
	
    if (matchingDictionary)
        matchingDictionary->release();
            
    if (iterator)
        iterator->release();
		
    return error;
    
}/* end findKernelLoggerCDC */
#endif

/****************************************************************************************************/
//
//		Function:	AppleUSBCDC::Asciihex_to_binary
//
//		Inputs:		c - Ascii character
//
//		Outputs:	return byte - binary byte
//
//		Desc:		Converts to hex (binary). 
//
/****************************************************************************************************/

UInt8 AppleUSBCDC::Asciihex_to_binary(char c)
{

    if ('0' <= c && c <= '9')
        return(c-'0');
                 
    if ('A' <= c && c <= 'F')
        return((c-'A')+10);
        
    if ('a' <= c && c <= 'f')
        return((c-'a')+10);
        
      // Not a hex digit, do whatever
      
    return(0);
    
}/* end Asciihex_to_binary */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//				driver for this device.
//
/****************************************************************************************************/

bool AppleUSBCDC::start(IOService *provider)
{
    UInt8	configs;	// number of device configurations

    fTerminate = false;
    fStopping = false;
	fCDCInterfaceNumber = 0xFF;
    
#if USE_ELG
    XTraceLogInfo	*logInfo;
    
    findKernelLoggerCDC();
    if (gXTrace)
    {
        gXTrace->retain();		// don't let it unload ...
        XTRACE(this, 0, 0xbeefbeef, "Hello from start");
        logInfo = gXTrace->LogGetInfo();
        IOLog("AppleUSBCDC: start - Log is at %x\n", (unsigned int)logInfo);
    } else {
        return false;
    }
#endif

    XTRACE(this, 0, provider, "start - provider.");
    if(!super::start(provider))
    {
        ALERT(0, 0, "start - super failed");
        return false;
    }

	// Get my USB device provider - the device

    fpDevice = OSDynamicCast(IOUSBDevice, provider);
    if(!fpDevice)
    {
        ALERT(0, 0, "start - provider invalid");
		return false;
    }

	// Let's see if we have any configurations to play with
		
    configs = fpDevice->GetNumConfigurations();
    if (configs < 1)
    {
        ALERT(0, 0, "start - no configurations");
        return false;
    }
	
	// Open the device and initialize it for interface matching
		
    if (!fpDevice->open(this))
    {
        ALERT(0, 0, "start - unable to open device");
        return false;
    }

    if (!initDevice(configs))
    {
        ALERT(0, 0, "start - initDevice failed");
        fpDevice->close(this);					// jrw added for 3720288
        return false;
    }
    
	retain();
	
    return true;
    	
}/* end start */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Stops the driver
//
/****************************************************************************************************/

void AppleUSBCDC::stop(IOService *provider)
{
    
    XTRACE(this, 0, 0, "stop");
    
    fStopping = true;
    
    if (fpDevice)
    {
        fpDevice->close(this);
        fpDevice = NULL;
    }
    
    super::stop(provider);
	
	release();
    
    return;

}/* end stop */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::getCDCDevice
//
//		Inputs:		
//
//		Outputs:	Device - the device address
//
//		Desc:		Returns the device address 
//
/****************************************************************************************************/

IOUSBDevice *AppleUSBCDC::getCDCDevice()
{

    XTRACE(this, 0, 0, "getCDCDevice");
    
    return fpDevice;

}/* end getCDCDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::initDevice
//
//		Inputs:		numConfigs - number of configurations present
//
//		Outputs:	return Code - true (CDC present), false (CDC not present)
//
//		Desc:		Determines if this is a CDC compliant device and then sets the configuration
//
/****************************************************************************************************/

bool AppleUSBCDC::initDevice(UInt8 numConfigs)
{
    IOUSBFindInterfaceRequest		req;
    const IOUSBConfigurationDescriptor	*cd = NULL;		// configuration descriptor
    IOUSBInterfaceDescriptor 		*intf = NULL;		// interface descriptor
    IOReturn				ior = kIOReturnSuccess;
    UInt8				cval;
//    UInt8				config = 0;
    bool				configOK = false;
       
    XTRACE(this, 0, numConfigs, "initDevice");
	
	fConfig = 0;
    	
        // Make sure we have a CDC interface to play with
        
    for (cval=0; cval<numConfigs; cval++)
    {
    	XTRACE(this, 0, cval, "initDevice - Checking Configuration");
		
     	cd = fpDevice->GetFullConfigurationDescriptor(cval);
     	if (!cd)
    	{
            XTRACE(this, 0, 0, "initDevice - Error getting the full configuration descriptor");
            break;
        } else {
            intf = NULL;
            do
            {
                req.bInterfaceClass = kUSBCommClass;
                req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
                req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
                req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
                ior = fpDevice->FindNextInterfaceDescriptor(cd, intf, &req, &intf);
                if (ior == kIOReturnSuccess)
                {
                    if (intf)
                    {
                        XTRACE(this, intf, 0, "initDevice - Interface descriptor found");
                        
                            // Let's make sure it's something we can really work with
                        
                        switch (intf->bInterfaceSubClass)
                        {
                            case kUSBAbstractControlModel:
                                if (intf->bInterfaceProtocol == kUSBv25)
                                {
                                    configOK = true;
									fCDCInterfaceNumber = intf->bInterfaceNumber;
                                }
                                break;
                            case kUSBEthernetControlModel:
                                configOK = true;
								fCDCInterfaceNumber = intf->bInterfaceNumber;
                                break;
                            case kUSBWirelessHandsetControlModel:
                                configOK = true;
								fCDCInterfaceNumber = intf->bInterfaceNumber;
                                break;
                            case kUSBDeviceManagementModel:
                                configOK = true;
								fCDCInterfaceNumber = intf->bInterfaceNumber;
                                break;
                            default:
                                break;
                        }
                    }
                } else {
                    XTRACE(this, ior, cval, "initDevice - FindNextInterfaceDescriptor returned error");
                    break;
                }
                if (configOK)
                {
                    break;
                }
            } while (intf);
            
            if (configOK)
            {
                break;
            }
        }
    }
    
    if (configOK)
    {
        fConfig = cd->bConfigurationValue;
        fbmAttributes = cd->bmAttributes;
                                    
        registerService();			// Better register before we kick off the interface drivers
        
        ior = fpDevice->SetConfiguration(this, fConfig);
        if (ior != kIOReturnSuccess)
        {
            XTRACE(this, 0, ior, "initDevice - SetConfiguration error");
            configOK = false;			
        }
    }
    
    return configOK;

}/* end initDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::reInitDevice
//
//		Inputs:		
//
//		Outputs:	return Code - from SetConfiguartion call
//
//		Desc:		Re-initilazes the device after a reset has been issued
//
/****************************************************************************************************/

IOReturn AppleUSBCDC::reInitDevice()
{
	IOUSBDevRequest req;
    IOReturn		ior = kIOReturnSuccess;
       
    XTRACE(this, 0, 0, "reInitDevice");
	
	req.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
    req.bRequest = kUSBRqSetConfig;
    req.wValue = fConfig;
    req.wIndex = 0;
    req.wLength = 0;
    req.pData = 0;
    ior = fpDevice->DeviceRequest(&req, 5000, 0);
	if (ior != kIOReturnSuccess)
	{
		XTRACE(this, 0, ior, "reInitDevice - DeviceRequest (SetConfig) error");
	}
    
    return ior;

}/* end reInitDevice */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::checkACM
//
//		Inputs:		Comm - pointer to the interface
//				dataInterfaceNum - the interface number of the enquiring driver
//
//		Outputs:	return Code - true (correct), false (incorrect)
//
//		Desc:		Checks the interface number of Abstract Control Model interface
//
/****************************************************************************************************/

bool AppleUSBCDC::checkACM(IOUSBInterface *Comm, UInt8 dataInterfaceNum)
{
    bool				gotDescriptors = false;
    bool				configOK = true;
    UInt8				acmDataInterfaceNumber = 0xff;
    const FunctionalDescriptorHeader 	*funcDesc = NULL;
    CMFunctionalDescriptor		*CMFDesc;		// call management functional descriptor
    UnionFunctionalDescriptor		*UNNFDesc;		// union functional descriptor
	bool				descError = false;
       
    XTRACE(this, 0, 0, "checkACM");
        
    do
    {
        (IOUSBDescriptorHeader*)funcDesc = Comm->FindNextAssociatedDescriptor((void*)funcDesc, CS_INTERFACE);
        if (!funcDesc)
        {
            gotDescriptors = true;				// We're done
        } else {
            switch (funcDesc->bDescriptorSubtype)
            {
                case CM_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)CMFDesc = funcDesc;
					if (!descError)
					{
						acmDataInterfaceNumber = CMFDesc->bDataInterface;
					}
                    break;
                case Union_FunctionalDescriptor:
                    (const FunctionalDescriptorHeader*)UNNFDesc = funcDesc;
                    if (UNNFDesc->bFunctionLength > sizeof(FunctionalDescriptorHeader))
                    {
						if (fCDCInterfaceNumber == UNNFDesc->bMasterInterface)
						{
							if (acmDataInterfaceNumber == 0xff)
							{
								acmDataInterfaceNumber = UNNFDesc->bSlaveInterface[0];		// Use the first slave (only if CMF not present)
							}
						} else {
							if (fCDCInterfaceNumber == UNNFDesc->bSlaveInterface[0])
							{
								acmDataInterfaceNumber = UNNFDesc->bMasterInterface;		// Work around for Conexant problem
								descError = true;											// Set the error flag just in case the Union descriptor is before the CM descriptor
							} else {
								XTRACE(this, 0, 0, "checkACM - Functional descriptors are incorrect");
							}
						}
					}
                    break;
                default:
                    break;
            }
        }
    } while(!gotDescriptors);

    if (acmDataInterfaceNumber != dataInterfaceNum)
    {
        XTRACE(this, acmDataInterfaceNumber, dataInterfaceNum, "checkACM - No data interface found");
        configOK = false;
    }
    
    return configOK;

}/* end checkACM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::checkECM
//
//		Inputs:		Comm - pointer to the interface
//				dataInterfaceNum - the interface number of the enquiring driver
//
//		Outputs:	return Code - true (correct), false (incorrect)
//
//		Desc:		Checks the interface number of Ethernet Control Model interface
//
/****************************************************************************************************/

bool AppleUSBCDC::checkECM(IOUSBInterface *Comm, UInt8 dataInterfaceNum)
{
    bool				gotDescriptors = false;
    bool				configOK = true;
    UInt8				ecmDataInterfaceNumber = 0xff;
    const FunctionalDescriptorHeader 	*funcDesc = NULL;
    UnionFunctionalDescriptor		*UNNFDesc;		// union functional descriptor
    ECMFunctionalDescriptor		*ENETFDesc;		// ethernet functional descriptor
    IOReturn				ior;
    UInt8				addrString;
    char 				ascii_mac[14];
    int 				i;
       
    XTRACE(this, 0, 0, "checkECM");
        
    do
    {
        (IOUSBDescriptorHeader*)funcDesc = Comm->FindNextAssociatedDescriptor((void*)funcDesc, CS_INTERFACE);
        if (!funcDesc)
        {
            gotDescriptors = true;				// We're done
        } else {
            if (funcDesc->bDescriptorSubtype == Union_FunctionalDescriptor)
            {
                (const FunctionalDescriptorHeader*)UNNFDesc = funcDesc;
                if (UNNFDesc->bFunctionLength > sizeof(FunctionalDescriptorHeader))
                {
                    ecmDataInterfaceNumber = UNNFDesc->bSlaveInterface[0];	// Use the first slave
                }
            } else {
                if (funcDesc->bDescriptorSubtype == ECM_Functional_Descriptor)
                {
                    (const FunctionalDescriptorHeader*)ENETFDesc = funcDesc;
                
                        // Cache the ethernet address in case it's needed early
                
                    if (ENETFDesc->iMACAddress != 0)
                    {
                        addrString = ENETFDesc->iMACAddress;
                    } else {
                        addrString = fpDevice->GetSerialNumberStringIndex();	// Default if none defined in the ECM functional descriptor
                    }
                    ior = fpDevice->GetStringDescriptor(addrString, (char *)&ascii_mac, 13);
                    if (ior == kIOReturnSuccess)
                    {
                        for (i = 0; i < 6; i++)
                        {
                            fCacheEaddr[i] = (Asciihex_to_binary(ascii_mac[i*2]) << 4) | Asciihex_to_binary(ascii_mac[i*2+1]);
//                            IOLog("AppleUSBCDC: checkECM - Ethernet address[%d] = %8x\n",(unsigned int)(i),(unsigned int)(fCacheEaddr[i]));
                        }
                        XTRACE(this, 0, addrString, "checkECM - Ethernet address (cached)");
                    } else {
                        XTRACE(this, ior, addrString, "checkECM - Error retrieving Ethernet address");
                    }
                }
            }
        }
    } while(!gotDescriptors);

    if (!ecmDataInterfaceNumber == dataInterfaceNum)
    {
        XTRACE(this, ecmDataInterfaceNumber, dataInterfaceNum, "checkECM - No data interface found");
        configOK = false;
    }
    
    return configOK;

}/* end checkECM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::checkWMC
//
//		Inputs:		Comm - pointer to the interface
//				dataInterfaceNum - the interface number of the enquiring driver
//
//		Outputs:	return Code - true (correct), false (incorrect)
//
//		Desc:		Checks the interface number of Abstract Control Model interface
//
/****************************************************************************************************/

bool AppleUSBCDC::checkWMC(IOUSBInterface *Comm, UInt8 dataInterfaceNum)
{
       
    XTRACE(this, 0, 0, "checkWMC");
        
    return false;

}/* end checkWMC */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::checkDMM
//
//		Inputs:		Comm - pointer to the interface
//				dataInterfaceNum - the interface number of the enquiring driver
//
//		Outputs:	return Code - true (correct), false (incorrect)
//
//		Desc:		Checks the interface number of Abstract Control Model interface
//
/****************************************************************************************************/

bool AppleUSBCDC::checkDMM(IOUSBInterface *Comm, UInt8 dataInterfaceNum)
{
       
    XTRACE(this, 0, 0, "checkDMM");
        
    return false;

}/* end checkDMM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::confirmDriver
//
//		Inputs:		subClass - the subclass of the inquiring data driver
//				dataInterface - the data interface of the data driver
//
//		Outputs:	
//
//		Desc:		Called by the data driver to confirm if this is the correct
//				configuration for the data interface driver. 
//
/****************************************************************************************************/

bool AppleUSBCDC::confirmDriver(UInt8 subClass, UInt8 dataInterface)
{
    IOUSBFindInterfaceRequest	req;
    IOUSBInterface		*Comm;
    UInt8			intSubClass;
    bool			driverOK = false;

    XTRACE(this, subClass, dataInterface, "confirmDriver");
    
        // We need to look at the interfaces
    
    req.bInterfaceClass	= kUSBCommClass;
    req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
    
    Comm = fpDevice->FindNextInterface(NULL, &req);
    if (!Comm)
    {
        XTRACE(this, 0, 0, "confirmDriver - Finding the first CDC interface failed");
        return false;
    }

    while (Comm)
    {
        intSubClass = Comm->GetInterfaceSubClass();
        if (intSubClass == subClass)
        {
            switch (intSubClass)
            {
                case kUSBAbstractControlModel:
                    driverOK = checkACM(Comm, dataInterface);
                    break;
                case kUSBEthernetControlModel:
                    driverOK = checkECM(Comm, dataInterface);
                    break;
                case kUSBWirelessHandsetControlModel:
                    driverOK = checkWMC(Comm, dataInterface);
                    break;
                case kUSBDeviceManagementModel:
                    driverOK = checkDMM(Comm, dataInterface);
                    break;
                default:
                    break;
            }
        }
        
        if (driverOK)
        {
            XTRACE(this, 0, 0, "confirmDriver - Interface confirmed");
            break;
        }

            // see if there's another interface
            
        req.bInterfaceClass = kUSBCommClass;
	req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
	req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
	req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
            
        Comm = fpDevice->FindNextInterface(Comm, &req);
        if (!Comm)
        {
            XTRACE(this, 0, 0, "confirmDriver - No more CDC interfaces");
        }
    }
    
    return driverOK;

}/* end confirmDriver */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::message
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

IOReturn AppleUSBCDC::message(UInt32 type, IOService *provider, void *argument)
{	
    
    XTRACE(this, 0, type, "message");
	
    switch (type)
    {
        case kIOMessageServiceIsTerminated:
            XTRACE(this, 0, type, "message - kIOMessageServiceIsTerminated");
            fTerminate = true;		// We're being terminated (unplugged)
            
                // Close the device - hopefully everyone's cleaned up
            
            if (fpDevice)
            {
                fpDevice->close(this);
                fpDevice = NULL;
            }
            
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
        case kIOUSBMessagePortHasBeenReset:
            XTRACE(this, 0, type, "message - kIOUSBMessagePortHasBeenReset");
			reInitDevice();					// What should we do if there's an error?
            break;
        default:
            XTRACE(this, 0, type, "message - unknown message"); 
            break;
    }
    
    return kIOReturnUnsupported;
    
}/* end message */
