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

#include <TargetConditionals.h>

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

#define DEBUG_NAME "AppleUSBCDC"

#include "AppleUSBCDCCommon.h"
#include "AppleUSBCDC.h"
#include "AppleUSBCDCPrivate.h"
#include "WWANSchemaDefinitions.h"

    // Globals

#define super IOService

OSDefineMetaClassAndStructors(AppleUSBCDC, IOService);

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
//		Method:		AppleUSBCDC::probe
//
//		Inputs:		provider - my provider
//
//		Outputs:	IOService - from super::probe, score - probe score
//
//		Desc:		Modify the probe score if necessary
//
/****************************************************************************************************/

IOService *AppleUSBCDC::probe(IOService *provider, SInt32 *score)
{
	IOUSBDevice *newDevice = NULL;
	UInt8		classValue = 0;
    OSNumber	*classInfo = NULL;
	SInt32		newScore = 0;
    IOService   *res;
	
	XTRACE(this, 0, 0, "probe");
	
	OSBoolean *boolObj = OSDynamicCast(OSBoolean, provider->getProperty("kCDCDoNotMatchThisDevice"));
    if (boolObj && boolObj->isTrue())
    {
        XTRACE(this, 0, *score, "probe - provider doesn't want us to match");
        return NULL;
    }
	
		// Check the device class, we need to handle Miscellaneous or Composite class a little different
	
	classInfo = (OSNumber *)provider->getProperty("bDeviceClass");
    if (classInfo)
    {
        classValue = classInfo->unsigned32BitValue();
		if ((classValue == kUSBCompositeClass) || (classValue == kUSBMiscellaneousClass))
		{
			newDevice = OSDynamicCast(IOUSBDevice, provider);
			if(!newDevice)
			{
				XTRACE(this, 0, 0, "probe - provider invalid");
			} else {
					// Check if it has CDC interfaces
					
				if (checkDevice(newDevice))
				{
					newScore = 1;			// We need to see the device before the Composite driver does
				} else {
					XTRACE(this, 0, 0, "probe - Composite or Micsellaneous class but not CDC");
					return NULL;			// We're not interested
				}
			}
		}
	}

	*score += newScore;
	
    res = super::probe(provider, score);
	
	XTRACE(this, 0, newScore, "probe - Exit");
    
    return res;
    
}/* end probe */

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
    UInt8		configs;	// number of device configurations
	UInt8		prefConfigValue = 0;
    OSNumber	*prefConfig = NULL;
	UInt8		devValue = 0;
    OSNumber	*devInfo = NULL;

    fTerminate = false;
    fStopping = false;

    XTRACE(this, 0, 0, "start");
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
	
		// Get the device details
	
	devInfo = (OSNumber *)fpDevice->getProperty("bDeviceClass");
    if (devInfo)
    {
        devValue = devInfo->unsigned32BitValue();
		XTRACE(this, 0, devValue, "start - Device Class");
		fDevClass = devValue;
    }
	
	devInfo = (OSNumber *)fpDevice->getProperty("bDeviceSubClass");
    if (devInfo)
    {
        devValue = devInfo->unsigned32BitValue();
		XTRACE(this, 0, devValue, "start - Device Subclass");
		fDevSubClass = devValue;
    }
	
	devInfo = (OSNumber *)fpDevice->getProperty("bDeviceProtocol");
    if (devInfo)
    {
        devValue = devInfo->unsigned32BitValue();
		XTRACE(this, 0, devValue, "start - Device Protocol");
		fDevProtocol = devValue;
    }
	
	if (fDevClass == kUSBMiscellaneousClass)
	{
		XTRACE(this, 0, fDevClass, "start - IAD device");
		fIAD = true;
	}
	
		// See if we have a preferred configuration
	
	fConfig = 0;
	
	prefConfig = (OSNumber *)fpDevice->getProperty("Preferred Configuration");
    if (prefConfig)
    {
        prefConfigValue = prefConfig->unsigned32BitValue();
		XTRACE(this, 0, prefConfigValue, "start - Preferred configuration");
		fConfig = prefConfigValue;
    }	

		// get workloop
        
    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
    {
        ALERT(0, 0, "start - getWorkLoop failed");
        return false;
    }
    
    fWorkLoop->retain();
	
	fCommandGate = IOCommandGate::commandGate(this);
    if (!fCommandGate)
    {
        ALERT(0, 0, "start - getCommandGate failed");
		fWorkLoop->release();
        return false;
    }

    fWorkLoop->addEventSource( fCommandGate );

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
	
	Log(DEBUG_NAME ": Version number - %s\n", VersionNumber);
	
    return true;
    	
}/* end start */

void AppleUSBCDC::free()
{
	
	XTRACE(this, 0, 0, "free");
	
    if ( fCommandGate )
    {
        if ( fWorkLoop )
        {
            fWorkLoop->removeEventSource( fCommandGate );
        }
        
        fCommandGate->release();
        fCommandGate = NULL;
    }        
        
    if ( fWorkLoop )
    {
        fWorkLoop->release();
        fWorkLoop = NULL;
    }

    super::free();
	
}/* end free */

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
//		Method:		AppleUSBCDC::checkDevice
//
//		Inputs:		theDevice - the device to check
//
//		Outputs:	return Code - true (CDC present), false (CDC not present)
//
//		Desc:		Determines if this device has a CDC interface (called from probe only)
//
/****************************************************************************************************/

bool AppleUSBCDC::checkDevice(IOUSBDevice *theDevice)
{
    IOUSBFindInterfaceRequest	req;
	UInt8				numConfigs;						// number of device configurations
    const IOUSBConfigurationDescriptor	*cd = NULL;		// configuration descriptor
    IOUSBInterfaceDescriptor 		*intf = NULL;		// interface descriptor
    IOReturn			ior = kIOReturnSuccess;
    UInt8				cval;
	bool				cdc = false;					// We really only want these
	
    XTRACE(this, 0, 0, "checkDevice");
	
		// Let's see if we have any configurations to play with
		
    numConfigs = theDevice->GetNumConfigurations();
    if (numConfigs < 1)
    {
        XTRACE(this, 0, 0, "checkDevice - no configurations");
        return false;
    }
	
		// Check all the configs
	
    for (cval=0; cval<numConfigs; cval++)
    {
    	XTRACE(this, 0, cval, "checkDevice - Checking Configuration");
		
     	cd = theDevice->GetFullConfigurationDescriptor(cval);
     	if (!cd)
    	{
            XTRACE(this, 0, cval, "checkDevice - Error getting the full configuration descriptor");
            break;
        } else {
            intf = NULL;
            do
            {
				req.bInterfaceClass = kIOUSBFindInterfaceDontCare;
                req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
                req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
                req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
                ior = theDevice->FindNextInterfaceDescriptor(cd, intf, &req, &intf);
                if (ior == kIOReturnSuccess)
                {
                    if (intf)
                    {
                        XTRACE(this, 0, intf->bInterfaceNumber, "initDevice - Interface descriptor found");
                        
							// We want CDC
						
						if (intf->bInterfaceClass == kUSBCommunicationClass)
						{
							cdc = true;
							break;
						}
                    }
                } else {
                    XTRACE(this, ior, cval, "checkDevice - FindNextInterfaceDescriptor returned error");
                    break;
                }
            } while (intf);
            
            if (cdc)
            {
                break;
            }
        }
    }
    
    return cdc;
	
}/* end checkDevice */

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
//	const IADDescriptor			*IAD = NULL;
    IOUSBInterfaceDescriptor 		*intf = NULL;		// interface descriptor
    IOReturn				ior = kIOReturnSuccess;
    UInt8				cval;
//    UInt8				config = 0;
	UInt16				dataClass;
	bool				configOK = true;				// Assume it's good
	bool				cdc = false;					// We really only want these
	
    XTRACE(this, 0, numConfigs, "initDevice");
	
		// Make sure we have a CDC interface to play with
	
    for (cval=0; cval<numConfigs; cval++)
    {
    	XTRACE(this, 0, cval, "initDevice - Checking Configuration");
		
		dataClass = 0;
		fDataInterfaceNumber = 0xFF;
		
     	cd = fpDevice->GetFullConfigurationDescriptor(cval);
     	if (!cd)
    	{
            XTRACE(this, 0, 0, "initDevice - Error getting the full configuration descriptor");
			configOK = false;
            break;
        } else {
			if (fIAD)	// This doesn't work here, we need the real interface not the descriptor
			{
//				IAD = (const IADDescriptor *)cd->FindNextAssociatedDescriptor((void *)IAD, kUSBInterfaceAssociationDesc);
			}
            intf = NULL;
            do
            {
//                req.bInterfaceClass = kUSBCommunicationClass;
				req.bInterfaceClass = kIOUSBFindInterfaceDontCare;
                req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
                req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
                req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
                ior = fpDevice->FindNextInterfaceDescriptor(cd, intf, &req, &intf);
                if (ior == kIOReturnSuccess)
                {
                    if (intf)
                    {
                        XTRACE(this, 0, intf->bInterfaceNumber, "initDevice - Interface descriptor found");
						
							// Check if we have a preferred config
						
						if (fConfig != 0)
						{
							if (fConfig != cd->bConfigurationValue)
							{
								XTRACE(this, fConfig, cd->bConfigurationValue, "initDevice - Not preferred configuration");
								continue;				// We have a preferred configuration and this isn't it
							}
						}
						
						configOK = true;			// No errors is all this really means
                        
							// Let's make sure it's something we can really work with (Data or Comm)
						
						if (intf->bInterfaceClass == kUSBDataClass)
						{
							dataClass++;
							fDataInterfaceNumber = intf->bInterfaceNumber;
						} else {
							if (intf->bInterfaceClass == kUSBCommunicationClass)
							{
								cdc = true;
								if (intf->bInterfaceSubClass == kUSBAbstractControlModel)
								{
										// Check for vendor specific protocol and ignore the interface
									
									if (intf->bInterfaceProtocol == 0xFF)
									{
										XTRACE(this, 0, 0, "initDevice - ACM interface has vendor specific protocol...");
										cdc = false;				// We'll allow this here and let the interface driver refuse it
									}
								}
							} else {
								XTRACE(this, intf->bInterfaceClass, intf->bInterfaceNumber, "initDevice - Ignoring interface...");
							}
						}
                    }
                } else {
                    XTRACE(this, ior, cval, "initDevice - FindNextInterfaceDescriptor returned error");
                    break;
                }
            } while (intf);
            
            if ((configOK) && (cdc))
            {
                break;
            }
        }
    }
    
    if ((configOK) && (cdc))		// Need to make sure it's CDC now we also match on Miscellaneous devices
    {
		XTRACE(this, 0, cd->bConfigurationValue, "initDevice - Configuration is valid");
		if (cdc)
		{
			if (dataClass > 1)				// Can only be one in order to save the number
			{
				fDataInterfaceNumber = 0xFF;
			}
		}
		fConfig = cd->bConfigurationValue;
		fbmAttributes = cd->bmAttributes;
		
		registerService();			// Better register before we kick off the interface drivers
//		IOSleep(500);				// Let it happen...
		
		if (fpDevice)
		{
			ior = fpDevice->SetConfiguration(this, fConfig);
			if (ior != kIOReturnSuccess)
			{
				XTRACE(this, fConfig, ior, "initDevice - SetConfiguration error");
			}
		} else {
			XTRACE(this, 0, 0, "initDevice - The device has gone");
			configOK = false;
		}
	} else {
		configOK = false;
		XTRACE(this, fConfig, configOK, "initDevice - No valid configuration or preferred configuration error");
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
//				cInterfaceNum - the interface number of the current Comm. interface
//				dataInterfaceNum - the interface number of the enquiring driver
//
//		Outputs:	return Code - true (correct), false (incorrect)
//
//		Desc:		Checks the interface number of Abstract Control Model interface
//
/****************************************************************************************************/

bool AppleUSBCDC::checkACM(IOUSBInterface *Comm, UInt8 cInterfaceNumber, UInt8 dataInterfaceNum)
{
    bool				gotDescriptors = false;
    bool				configOK = true;
    UInt8				acmDataInterfaceNumber = 0xFF;
    const FunctionalDescriptorHeader 	*funcDesc = NULL;
    CMFunctionalDescriptor		*CMFDesc;		// call management functional descriptor
    UnionFunctionalDescriptor		*UNNFDesc;		// union functional descriptor
	bool				descError = false;
       
    XTRACE(this, 0, 0, "checkACM");
        
    do
    {
        funcDesc = (const FunctionalDescriptorHeader *)Comm->FindNextAssociatedDescriptor((void*)funcDesc, CS_INTERFACE);
        if (!funcDesc)
        {
            gotDescriptors = true;				// We're done
        } else {
            switch (funcDesc->bDescriptorSubtype)
            {
                case CM_FunctionalDescriptor:
                    CMFDesc = (CMFunctionalDescriptor *)funcDesc;
					if (!descError)
					{
						acmDataInterfaceNumber = CMFDesc->bDataInterface;
					}
                    break;
                case Union_FunctionalDescriptor:
                    UNNFDesc = (UnionFunctionalDescriptor *)funcDesc;
                    if (UNNFDesc->bFunctionLength > sizeof(FunctionalDescriptorHeader))
                    {
						XTRACE(this, cInterfaceNumber, UNNFDesc->bMasterInterface, "checkACM - Interfaces(Control, Master)");
						if (cInterfaceNumber == UNNFDesc->bMasterInterface)
						{
							if (acmDataInterfaceNumber == 0xFF)
							{
								acmDataInterfaceNumber = UNNFDesc->bSlaveInterface[0];		// Use the first slave (only if CMF not present)
							}
						} else {
							if (cInterfaceNumber == UNNFDesc->bSlaveInterface[0])
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
	
		//
		// This'll need explaining. It's for devices that have no functional descriptors or they are in the wrong place (I.E. usually after the Data interface)
		//
		// If the acmDataInterfaceNumber is stil 0xFF then there's every reason to believe there's no functional descriptors present (or they're incorrect)
		// If the fDataInterfaceNumber is not 0xFF (see initDevice) then there's only one data interface present so that's probably the one we want, correct?
		//
	
	if ((acmDataInterfaceNumber == 0xFF) && (fDataInterfaceNumber != 0xFF))
	{
		acmDataInterfaceNumber = fDataInterfaceNumber;
	}

    if (acmDataInterfaceNumber != dataInterfaceNum)
    {
        XTRACE(this, acmDataInterfaceNumber, dataInterfaceNum, "checkACM - No data interface found from functional descriptors");
        configOK = false;
    }
    
    return configOK;

}/* end checkACM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::checkECM
//
//		Inputs:		Comm - pointer to the interface
//				cInterfaceNum - the interface number of the current Comm. interface
//				dataInterfaceNum - the interface number of the enquiring driver
//
//		Outputs:	return Code - true (correct), false (incorrect)
//
//		Desc:		Checks the interface number of Ethernet Control Model interface
//
/****************************************************************************************************/

bool AppleUSBCDC::checkECM(IOUSBInterface *Comm, UInt8 cInterfaceNumber, UInt8 dataInterfaceNum)
{
    bool				gotDescriptors = false;
    bool				configOK = true;
    UInt8				ecmDataInterfaceNumber = 0xFF;
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
        funcDesc = (const FunctionalDescriptorHeader *)Comm->FindNextAssociatedDescriptor((void*)funcDesc, CS_INTERFACE);
        if (!funcDesc)
        {
            gotDescriptors = true;				// We're done
        } else {
            if (funcDesc->bDescriptorSubtype == Union_FunctionalDescriptor)
            {
                UNNFDesc = (UnionFunctionalDescriptor *)funcDesc;
                if (UNNFDesc->bFunctionLength > sizeof(FunctionalDescriptorHeader))
                {
					if (cInterfaceNumber == UNNFDesc->bMasterInterface)
					{
						ecmDataInterfaceNumber = UNNFDesc->bSlaveInterface[0];	// Use the first slave
					}
                }
            } else {
                if (funcDesc->bDescriptorSubtype == ECM_Functional_Descriptor)
                {
                    ENETFDesc = (ECMFunctionalDescriptor *)funcDesc;
                
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
//                            Log("AppleUSBCDC: checkECM - Ethernet address[%d] = %8x\n",(unsigned int)(i),(unsigned int)(fCacheEaddr[i]));
                        }
                        XTRACE(this, 0, addrString, "checkECM - Ethernet address (cached)");
                    } else {
                        XTRACE(this, ior, addrString, "checkECM - Error retrieving Ethernet address");
                    }
                }
            }
        }
    } while(!gotDescriptors);

    if (ecmDataInterfaceNumber != dataInterfaceNum)
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
//				cInterfaceNum - the interface number of the current Comm. interface
//				dataInterfaceNum - the interface number of the enquiring driver
//
//		Outputs:	return Code - true (correct), false (incorrect)
//
//		Desc:		Checks the interface number of Abstract Control Model interface
//
/****************************************************************************************************/

bool AppleUSBCDC::checkWMC(IOUSBInterface *Comm, UInt8 cInterfaceNumber, UInt8 dataInterfaceNum)
{
       
    XTRACE(this, 0, 0, "checkWMC");
        
    return false;

}/* end checkWMC */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::checkDMM
//
//		Inputs:		Comm - pointer to the interface
//				cInterfaceNum - the interface number of the current Comm. interface
//				dataInterfaceNum - the interface number of the enquiring driver
//
//		Outputs:	return Code - true (correct), false (incorrect)
//
//		Desc:		Checks the interface number of Abstract Control Model interface
//
/****************************************************************************************************/

bool AppleUSBCDC::checkDMM(IOUSBInterface *Comm, UInt8 cInterfaceNumber, UInt8 dataInterfaceNum)
{
       
    XTRACE(this, 0, 0, "checkDMM");
        
    return false;

}/* end checkDMM */

/****************************************************************************************************/
//
//		Method:		AppleUSBCDC::confirmDriver
//
//		Inputs:		subClass - the subclass needed by the inquiring data driver
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
	UInt8			controlInterfaceNumber;
    bool			driverOK = false;

    XTRACE(this, subClass, dataInterface, "confirmDriver");
    
        // We need to look for CDC interfaces of the specified subclass
    
    req.bInterfaceClass	= kUSBCommunicationClass;
//    req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
	req.bInterfaceSubClass = subClass;
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
		controlInterfaceNumber = Comm->GetInterfaceNumber();
        intSubClass = Comm->GetInterfaceSubClass();
        if (intSubClass == subClass)					// Just to make sure...
        {
            switch (intSubClass)
            {
                case kUSBAbstractControlModel:
                    driverOK = checkACM(Comm, controlInterfaceNumber, dataInterface);
                    break;
                case kUSBEthernetControlModel:
                    driverOK = checkECM(Comm, controlInterfaceNumber, dataInterface);
                    break;
                case kUSBWirelessHandsetControlModel:
                    driverOK = checkWMC(Comm, controlInterfaceNumber, dataInterface);
                    break;
                case kUSBDeviceManagementModel:
                    driverOK = checkDMM(Comm, controlInterfaceNumber, dataInterface);
                    break;
                case kUSBEthernetEmulationModel:
                        // There's only one interface for EEM so the data and the control interface are the same
                        // May need to revisit this
                    
                    if (controlInterfaceNumber == dataInterface)
                    {
                        driverOK = true; 
                    }
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

            // see if there's another CDC interface
            
        req.bInterfaceClass = kUSBCommunicationClass;
//	req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
	req.bInterfaceSubClass = subClass;
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
//		Method:		AppleUSBCDC::confirmControl
//
//		Inputs:		subClass - the subclass of the inquiring control driver
//					CInterface - the control interface
//
//		Outputs:	
//
//		Desc:		Called by the control driver to confirm if this is the correct interface
//
/****************************************************************************************************/

bool AppleUSBCDC::confirmControl(UInt8 subClass, IOUSBInterface *CInterface)
{
    IOUSBFindInterfaceRequest	req;
    IOUSBInterface		*Comm;
    UInt8			intSubClass;
    bool			driverOK = false;

    XTRACE(this, subClass, 0, "confirmControl");
    
        // We need to look for CDC interfaces of the specified subclass
    
    req.bInterfaceClass	= kUSBCommunicationClass;
//    req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
	req.bInterfaceSubClass = subClass;
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
        if (intSubClass == subClass)					// Just to make sure...
        {
			XTRACE(this, Comm->GetInterfaceNumber(), CInterface->GetInterfaceNumber(), "confirmControl - Checking interfaces");
            if (Comm == CInterface)
			{
				driverOK = true;
				break;
			}
        }

            // see if there's another CDC interface
            
		req.bInterfaceClass = kUSBCommunicationClass;
//		req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
		req.bInterfaceSubClass = subClass;
		req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
		req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
            
        Comm = fpDevice->FindNextInterface(Comm, &req);
        if (!Comm)
        {
            XTRACE(this, 0, 0, "confirmControl - No more CDC interfaces");
        }
    }
    
    return driverOK;

}/* end confirmControl */

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


IOCommandGate *AppleUSBCDC::getCommandGate() const
{
    return fCommandGate;
}


IOReturn AppleUSBCDC::setProperties( OSObject * properties )
{
	IOReturn result = kIOReturnError;
	IOCommandGate *cg;
	
	XTRACE(this, 0, 0, "setProperties");
	
	cg = getCommandGate();
	
	if ( cg != NULL )
	{
        //<radar://problem/7488030> retain the CommandGate in case device gets pulled when we are setting the properties..
        cg->retain();
		result = cg->runAction( setPropertiesAction, (void *)properties );
        cg->release();
	}
	
	XTRACE(this, 0, 0, "setProperties - Exit");
	
	return result;
}

//===========================================================================================================================
//	setPropertiesAction
//===========================================================================================================================

IOReturn AppleUSBCDC::setPropertiesAction(	OSObject	*owner, 
														void		*arg1, 
														void		*arg2, 
														void		*arg3, 
														void		*arg4 )
{
	IOReturn result = kIOReturnBadArgument;
	
	if ( owner != NULL )
	{
		AppleUSBCDC *me = OSDynamicCast( AppleUSBCDC, owner );
		
		if ( me != NULL )
		{
			result = me->setPropertiesWL( (OSObject *)arg1 );
		}
	}
	
	return result;
}

//===========================================================================================================================
//	setPropertiesWL
//===========================================================================================================================

IOReturn AppleUSBCDC::setPropertiesWL( OSObject * properties )
{
	IOReturn result = kIOReturnBadArgument;
	OSDictionary *propertyDict;
	
	WWAN_DICTIONARY whichDictionary = WWAN_DICTIONARY_UNKNOWN;
	
	OSObject *	dynamicKey = NULL;
	bool		rc = false;
	
	propertyDict = OSDynamicCast( OSDictionary, properties );

	if ( propertyDict != NULL )
	{
		OSCollectionIterator *propertyIterator;
		
#if TARGET_OS_EMBEDDED
		
		// Suspend support
		OSBoolean *suspendDevice = OSDynamicCast(OSBoolean, propertyDict->getObject("SuspendDevice"));
		
		if ( suspendDevice == kOSBooleanTrue ) 
		{
			Log("AppleUSBCDC::setProperties - SuspendDevice: true\n");
			
			if ( fpDevice->isOpen(this) )
			{
				if ( ( result = fpDevice->SuspendDevice(true) ) != kIOReturnSuccess )
				{
					Log("AppleUSBCDC::setProperties - failed to suspend the device, error: %08x \n", result);
				}
			}
			else
			{
				Log("AppleUSBCDC::setProperties - device was not open \n");
				return kIOReturnError;
			}
			
			return result;			
		}
		else if ( suspendDevice == kOSBooleanFalse )
		{
			Log("AppleUSBCDC::setProperties - SuspendDevice: false\n");
			
			if ( fpDevice->isOpen(this) )
			{
				if ( ( result = fpDevice->SuspendDevice(false) ) != kIOReturnSuccess )
				{
					Log("AppleUSBCDC::setProperties - failed to !suspend the device, error: %08x \n", result);
				}
			}
			else
			{
				IOLog("AppleUSBCDC::setProperties - device was not open \n");
				result = kIOReturnError;
			}
			
			return result;			
		}
		
		
		// Remote Wake-up support
		OSBoolean *remoteWakeup = OSDynamicCast(OSBoolean, propertyDict->getObject("RemoteWakeUp"));
		IOUSBDevRequest	devreq;
		
		if ( remoteWakeup )
		{
			devreq.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
			if ( remoteWakeup == kOSBooleanTrue )
			{
				devreq.bRequest = kUSBRqSetFeature;
			} else {
				devreq.bRequest = kUSBRqClearFeature;
			}
			devreq.wValue = kUSBFeatureDeviceRemoteWakeup;
			devreq.wIndex = 0;
			devreq.wLength = 0;
			devreq.pData = 0;
			
			result = fpDevice->DeviceRequest(&devreq);
			if ( result == kIOReturnSuccess )
			{
				IOLog("AppleUSBCDC::setProperties - Set/Clear remote wake up feature successful\n");
			} else {
				IOLog("AppleUSBCDC::setProperties - Set/Clear remote wake up feature failed, %08x\n", result);
		}

			return result;			
		}
		
#endif // TARGET_OS_EMBEDDED
		
		if (dynamicKey = propertyDict->getObject(kWWAN_TYPE))
			whichDictionary	= WWAN_SET_DYNAMIC_DICTIONARY;
		else
			if (dynamicKey = propertyDict->getObject(kWWAN_HW_VERSION))					
				whichDictionary	= WWAN_SET_HARDWARE_DICTIONARY;
			else
				if (dynamicKey = propertyDict->getObject("AccessPointName"))
					whichDictionary	= WWAN_SET_MODEM_DICTIONARY;
				else
					if (dynamicKey = propertyDict->getObject("LCPMTU"))
					whichDictionary	= WWAN_SET_PPP_DICTIONARY;
					else
						if (dynamicKey = propertyDict->getObject(kWWAN_UNIQUIFIER))
						whichDictionary	= WWAN_SET_MODEM_DICTIONARY;
					
			// if we still can't determine which dictionary it is
			// Iterate to see if it is a property we know about..
		
		if (whichDictionary == WWAN_DICTIONARY_UNKNOWN) 
		{
		propertyIterator = OSCollectionIterator::withCollection( propertyDict );
		
		if ( propertyIterator != NULL )
		{
			OSSymbol *key;
			
			while( ( key = (OSSymbol *)propertyIterator->getNextObject() ) )
			{
//				Log("[setPropertiesWL] key: %s \n", key->getCStringNoCopy());
//				if (dynamicKey)
//					setProperty(key->getCStringNoCopy(),key);					
//					setProperty(key->getCStringNoCopy(),propertyDict->getObject(key));					
					if (key->isEqualTo(kWWAN_SC_SETUP))
					{
						rc = fpDevice->setProperty(kWWAN_SC_SETUP,propertyDict->getObject(key));
						goto exit;
					}
					/*
					if (key->isEqualTo(kWWAN_UNIQUIFIER))
					{
						rc = fpDevice->setProperty(kWWAN_UNIQUIFIER,propertyDict->getObject(key));
						goto exit;
					}
					*/
					
				}
			propertyIterator->release();
		}
		else
		{
//			Log("[setPropertiesWL] could not obtain an OSCollectionIterator... \n");
			ALERT(0, 0, "setPropertiesWL - Could not obtain an OSCollectionIterator...");
			result = kIOReturnError;
		}
		}
		else
		{		
			switch (whichDictionary)
			{
				case WWAN_SET_DYNAMIC_DICTIONARY:
					rc = fpDevice->setProperty(kWWAN_DynamicDictonary,propertyDict);
//					Log("[setPropertiesWL] setting kWWAN_DynamicDictonary\n");

					break;
				
				case WWAN_SET_HARDWARE_DICTIONARY: 	
				rc = fpDevice->setProperty(kWWAN_HardwareDictionary,propertyDict);
//					Log("[setPropertiesWL] setting kWWAN_HardwareDictionary\n");
					break;
							
				case WWAN_SET_MODEM_DICTIONARY: 	
					rc = fpDevice->setProperty("DeviceModemOverrides",propertyDict);
//					Log("[setPropertiesWL] setting DeviceModemOverrides\n");
					break;
					
				case WWAN_SET_PPP_DICTIONARY: 
					rc = fpDevice->setProperty("DevicePPPOverrides",propertyDict);
//					Log("[setPropertiesWL] setting DevicePPPOverrides\n");
					break;

				case WWAN_DICTIONARY_UNKNOWN: 	
//					Log("AppleWWANSUpport::setPropertiesWL - Unknown Dictionary");
					
					break;

				default:
//					Log("AppleWWANSUpport::setPropertiesWL - default Unknown Dictionary");
					break;
			}
		}

				fpDevice->messageClients ( kIOMessageServicePropertyChange );
//				Log("[setPropertiesWL] set kWWAN_HardwareDictionary [%x] pNub mesaging Clients with  kIOMessageServicePropertyChange \n",rc);
	
	}

exit:
	
	this->messageClients ( kIOMessageServicePropertyChange );
	fpDevice->messageClients ( kIOMessageServicePropertyChange );

	return kIOReturnSuccess;
}