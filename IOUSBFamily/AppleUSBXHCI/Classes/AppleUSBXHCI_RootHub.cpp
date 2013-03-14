/*
 *  AppleUSBXHCI_RootHub.cpp
 *
 *  Copyright © 2011-2012 Apple Inc. All Rights Reserved.
 *
 */

//================================================================================================
//
//   Headers
//
//================================================================================================
//
extern "C" {
#include <kern/clock.h>
}

#include <libkern/OSByteOrder.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>

#include "AppleUSBXHCIUIM.h"
#include "AppleUSBXHCI_RootHub.h"

// This is the debounce interval
const UInt64 kOneHundredMillisecondsInNanoseconds     = (UInt64)  100 * ((UInt64) (1000 * 1000));
const UInt64 kConnectionDebounceIntervalInNanos       = kOneHundredMillisecondsInNanoseconds;

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#ifndef APPLEXHCIROOTHUB_USE_KPRINTF 
	#define APPLEXHCIROOTHUB_USE_KPRINTF 0
#endif

#if APPLEXHCIROOTHUB_USE_KPRINTF
	#undef USBLog
	#undef USBError
	void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
	#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= APPLEXHCIROOTHUB_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
	#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#if (DEBUG_REGISTER_READS == 1)
#define Read32Reg(registerPtr, ...) Read32RegWithFileInfo(registerPtr, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define Read32RegWithFileInfo(registerPtr, function, file, line, ...) (															\
	fTempReg = Read32Reg(registerPtr, ##__VA_ARGS__),																			\
	fTempReg = (fTempReg == (typeof (*(registerPtr))) -1) ?																		\
		(kprintf("AppleUSBXHCI[%p]::%s Invalid register at %s:%d %s\n", this,function,file, line,#registerPtr), -1) : fTempReg,	\
	(typeof(*(registerPtr)))fTempReg)

#define Read64Reg(registerPtr, ...) Read64RegWithFileInfo(registerPtr, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define Read64RegWithFileInfo(registerPtr, function, file, line, ...) (															\
	fTempReg = Read64Reg(registerPtr, ##__VA_ARGS__),																			\
	fTempReg = (fTempReg == (typeof (*(registerPtr))) -1) ?																		\
		(kprintf("AppleUSBXHCI[%p]::%s Invalid register at %s:%d %s\n", this,function,file, line,#registerPtr), -1) : fTempReg,	\
	(typeof(*(registerPtr)))fTempReg)
#endif

//================================================================================================
//
//   AppleUSBXHCI  Root Hub Methods
//
//================================================================================================


#pragma mark Descriptors


IOReturn 
AppleUSBXHCI::GetRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc)
{
    IOUSBDeviceDescriptor SSDesc =
	{
		sizeof(IOUSBDeviceDescriptor),			// UInt8 length;
		kUSBDeviceDesc,							// UInt8 descType;
		HostToUSBWord(kUSBRel30),				// UInt16 usbRel Supports USB 3.0;
		kUSBHubClass,							// UInt8 class;
		kUSBHubSubClass,						// UInt8 subClass;
		kHubSuperSpeedProtocol,					// UInt8 protocol;
		9,										// UInt8 maxPacketSize;
		HostToUSBWord(kAppleVendorID),			// UInt16 vendor:  Use the Apple Vendor ID from USB-IF
		HostToUSBWord(kPrdRootHubAppleSS),		// UInt16 product:  All our root hubs are the same
		HostToUSBWord(kUSBRel30),				// UInt16 devRel: 
		2,										// UInt8 manuIdx;
		1,										// UInt8 prodIdx;
		0,										// UInt8 serialIdx;
		1										// UInt8 numConf;
	};

	IOUSBDeviceDescriptor HSDesc =
	{
		sizeof(IOUSBDeviceDescriptor),			// UInt8 length;
		kUSBDeviceDesc,							// UInt8 descType;
		HostToUSBWord(kUSBRel20),				// UInt16 usbRel Supports USB 2.0;
		kUSBHubClass,							// UInt8 class;
		kUSBHubSubClass,						// UInt8 subClass;
		1,										// UInt8 protocol;
		9,										// UInt8 maxPacketSize;
		HostToUSBWord(kAppleVendorID),			// UInt16 vendor:  Use the Apple Vendor ID from USB-IF
		HostToUSBWord(kPrdRootHubAppleSS),		// UInt16 product:  All our root hubs are the same
		HostToUSBWord(kUSBRel30),				// UInt16 devRel: 
		2,										// UInt8 manuIdx;
		1,										// UInt8 prodIdx;
		0,										// UInt8 serialIdx;
		1										// UInt8 numConf;
	};
	
	if (!desc)
		return kIOReturnNoMemory;
	
	RHCommandHeaderPtr command = (RHCommandHeaderPtr)(desc);
	UInt8 speed = ((command->request & kUSBSpeed_Mask) >> kUSBSpeed_Shift);
	
	USBLog(3, "AppleUSBXHCI[%p]::GetRootHubDeviceDescriptor - speed: %d", this, speed);
	
	if( speed == kUSBDeviceSpeedHigh )
	{
		bcopy(&HSDesc, desc, HSDesc.bLength);
	}
	else
	{
		bcopy(&SSDesc, desc, SSDesc.bLength);
	}
	
	return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::GetRootHubStringDescriptor(UInt8	index, OSData *desc)
{
	USBLog(3, "AppleUSBXHCI[%p]::GetRootHubStringDescriptor - desc: %d", this, index);
	
    // The following strings are in Unicode format
    //
    UInt8 productNameSS[] = {
		0,			// Length Byte
		kUSBStringDesc,	// Descriptor type
		0x58, 0x00, // "X"
		0x48, 0x00, // "H"
		0x43, 0x00, // "C"
		0x49, 0x00, // "I"
		0x20, 0x00,	// " "
		0x52, 0x00,	// "R"
		0x6F, 0x00,	// "o"
		0x6f, 0x00,	// "o"
		0x74, 0x00,	// "t"
		0x20, 0x00,	// " "
		0x48, 0x00,	// "H"
		0x75, 0x00,	// "u"
		0x62, 0x00,	// "b"
		0x20, 0x00,	// " "
		0x53, 0x00, // "S"
		0x53, 0x00, // "S"
		0x20, 0x00,	// " "
		0x53, 0x00, // "S"
		0x69, 0x00,	// "i"
		0x6d, 0x00,	// "m"
		0x75, 0x00,	// "u"
		0x6c, 0x00,	// "l"
		0x61, 0x00,	// "a"
		0x74, 0x00,	// "t"
		0x69, 0x00,	// "i"
		0x6f, 0x00,	// "o"
		0x6e, 0x00,	// "n"
	};
	
    UInt8 productNameHS[] = {
		0,			// Length Byte
		kUSBStringDesc,	// Descriptor type
		0x58, 0x00, // "X"
		0x48, 0x00, // "H"
		0x43, 0x00, // "C"
		0x49, 0x00, // "I"
		0x20, 0x00,	// " "
		0x52, 0x00,	// "R"
		0x6F, 0x00,	// "o"
		0x6f, 0x00,	// "o"
		0x74, 0x00,	// "t"
		0x20, 0x00,	// " "
		0x48, 0x00,	// "H"
		0x75, 0x00,	// "u"
		0x62, 0x00,	// "b"
		0x20, 0x00,	// " "
		0x55, 0x00,	// "U"
		0x53, 0x00, // "S"
		0x42, 0x00,	// "B"
		0x20, 0x00,	// " "
		0x32, 0x00,	// "2"
		0x2e, 0x00,	// "."
		0x30, 0x00,	// "0"
		0x20, 0x00,	// " "
		0x53, 0x00, // "S"
		0x69, 0x00,	// "i"
		0x6d, 0x00,	// "m"
		0x75, 0x00,	// "u"
		0x6c, 0x00,	// "l"
		0x61, 0x00,	// "a"
		0x74, 0x00,	// "t"
		0x69, 0x00,	// "i"
		0x6f, 0x00,	// "o"
		0x6e, 0x00,	// "n"
	};
	
    UInt8 vendorName[] = {
		0,			// Length Byte
		kUSBStringDesc,	// Descriptor type
		0x41, 0x00,	// "A"
		0x70, 0x00,	// "p"
		0x70, 0x00,	// "p"
		0x6c, 0x00,	// "l"
		0x65, 0x00,	// "e"
		0x20, 0x00,	// " "
		0x49, 0x00,	// "I"
		0x6e, 0x00,	// "n"
		0x63, 0x00,	// "c"
		0x2e, 0x00	// "."
	};
	
    // According to our device descriptor, index 1 is product, index 2 is Manufacturer
    //
    if ( (index > 2) || (index == 0) )
        return kIOReturnBadArgument;

	if (!desc)
		return kIOReturnNoMemory;

    // Set the length of our strings
    //
    vendorName[0] = sizeof(vendorName);
    productNameSS[0] = sizeof(productNameSS);
    productNameHS[0] = sizeof(productNameHS);
    
    if ( index == 1 )
    {
		RHCommandHeaderPtr command = (RHCommandHeaderPtr)(desc->getBytesNoCopy());
		UInt8 speed = 0;
		
		if(command)
		{
			speed = ((command->request & kUSBSpeed_Mask) >> kUSBSpeed_Shift);
		}
		
		USBLog(3, "AppleUSBXHCI[%p]::GetRootHubStringDescriptor - speed: %d", this, speed);
		
		if( speed == kUSBDeviceSpeedHigh )
		{
			if (!desc->appendBytes(&productNameHS,  productNameHS[0]))
				return kIOReturnNoMemory;
		}
		else
		{
			if (!desc->appendBytes(&productNameSS,  productNameSS[0]))
				return kIOReturnNoMemory;
		}
    }
    
    if ( index == 2 )
    {
        if (!desc->appendBytes(&vendorName,  vendorName[0]))
            return kIOReturnNoMemory;
    }
    
    return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::GetRootHubConfDescriptor(OSData *desc)
{
    IOUSBConfigurationDescriptor confDesc =
	{
		sizeof(IOUSBConfigurationDescriptor),								//UInt8 length;
		kUSBConfDesc,               										//UInt8 descriptorType;
		HostToUSBWord(sizeof(IOUSBConfigurationDescriptor) +
					  sizeof(IOUSBInterfaceDescriptor) +
					  sizeof(IOUSBEndpointDescriptor) +
					  sizeof(IOUSBSuperSpeedEndpointCompanionDescriptor)),	//UInt16 totalLength;
		1,      															//UInt8 numInterfaces;
		1,      															//UInt8 configValue;
		0,      															//UInt8 configStrIndex;
		0x60,      											      			//UInt8 attributes; self powered, supports remote wakeup
		0,                          										//UInt8 maxPower;
	};
	IOUSBInterfaceDescriptor intfDesc =
	{
		sizeof(IOUSBInterfaceDescriptor),	//UInt8 length;
		kUSBInterfaceDesc,      			//UInt8 descriptorType;
		0,                      			//UInt8 interfaceNumber;
		0,                      			//UInt8 alternateSetting;
		1,                      			//UInt8 numEndpoints;
		kUSBHubClass,           			//UInt8 interfaceClass;
		kUSBHubSubClass,        			//UInt8 interfaceSubClass;
		0,									//UInt8 interfaceProtocol;
		0                       			//UInt8 interfaceStrIndex;
	};
	IOUSBEndpointDescriptor endptDesc =
	{
		sizeof(IOUSBEndpointDescriptor),	// UInt8 length;
		kUSBEndpointDesc,					// UInt8 descriptorType;
		0x81,								// UInt8  endpointAddress; In, 1
		0x10 | kUSBInterrupt,				// UInt8 attributes; (notification interrupt)
		HostToUSBWord(2),					// UInt16 maxPacketSize;
		9,									// UInt8 interval (256 microframes or 32 ms)
	};
	IOUSBSuperSpeedEndpointCompanionDescriptor compDesc =
	{
		sizeof(IOUSBSuperSpeedEndpointCompanionDescriptor),	// UInt8 length;
		kUSBSuperSpeedEndpointCompanion,					// UInt8 descriptorType
		0,													// UInt8 bMaxBurst;
		0,													// UInt8 bmAttributes
		2													// UInt16 wBytesPerInterval
	};
	
	if (!desc)
		return(kIOReturnNoMemory);
	
	if (!desc->appendBytes(&confDesc,  confDesc.bLength))
		return(kIOReturnNoMemory);
	
	if (!desc->appendBytes(&intfDesc,  intfDesc.bLength))
		return(kIOReturnNoMemory);
	
	if (!desc->appendBytes(&endptDesc, endptDesc.bLength))
		return(kIOReturnNoMemory);
	
	if (!desc->appendBytes(&compDesc, compDesc.bLength))
		return(kIOReturnNoMemory);
	
	return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::GetRootHubBOSDescriptor(OSData *desc)
{
	// 9E1EFBDF-1D1B-4610-AE91-3B66F5561DA5
#define APPLE_XHCI_UUID 0xDF, 0xFB, 0x1E, 0x9E, 0x1B, 0x1D, 0x10, 0x46, 0xAE, 0x91, 0x3B, 0x66, 0xF5, 0x56, 0x1D, 0xA

	USBLog(6, "AppleUSBXHCI[%p]::GetRootHubBOSDescriptor", this);
    IOUSBBOSDescriptor bosDesc =
	{
		sizeof(IOUSBBOSDescriptor),										//UInt8 bLength;
		kUSBBOSDescriptor,												//UInt8 bDescriptorType;
		HostToUSBWord(sizeof(IOUSBBOSDescriptor) +
					  sizeof(IOUSBDeviceCapabilityUSB2Extension) +
					  sizeof(IOUSBDeviceCapabilitySuperSpeedUSB) +
					  sizeof(IOUSBDeviceCapabilityContainerID)),		//UInt16 wTotalLength;
		3																//UInt8 bNumDeviceCaps;
	};
	IOUSBDeviceCapabilityUSB2Extension capDesc =
	{
		sizeof(IOUSBDeviceCapabilityUSB2Extension),					//UInt8 bLength;
		kUSBDeviceCapability,										//UInt8 bDescriptorType;
		kUSBDeviceCapabilityUSB20Extension,							//UInt8 bDeviceCapability;
		HostToUSBLong(1<<kUSB20ExtensionLPMSupported)				//UInt8 bmAttributes;
	};
	IOUSBDeviceCapabilitySuperSpeedUSB ssCapDesc =
	{
		sizeof(IOUSBDeviceCapabilitySuperSpeedUSB),					//UInt8 bLength;
		kUSBDeviceCapability,										//UInt8 bDescriptorType;
		kUSBDeviceCapabilitySuperSpeedUSB,							//UInt8 bDeviceCapability;
		HostToUSBLong(1<<kUSBSuperSpeedLTMCapable),					//UInt8 bmAttributes;
		HostToUSBWord(1<<kUSBSuperSpeedSupportsHS |
				      1<<kUSBSuperSpeedSupportsSS ),				//UInt16 wSpeedsSupported;
		(1<<kUSBSuperSpeedSupportsSS),								//UInt8	bFunctionalitySupport
		10,															//UInt8 bU1DevExitLat
		HostToUSBWord(100)											//UInt16 wU2DevExitLat
	};
	IOUSBDeviceCapabilityContainerID containerDesc =
	{
		sizeof(IOUSBDeviceCapabilityContainerID),					//UInt8 bLength;
		kUSBDeviceCapability,										//UInt8 bDescriptorType;
		kUSBDeviceCapabilityContainerID,							//UInt8 bDeviceCapability;
		0,															//UInt8 bReserved
		{APPLE_XHCI_UUID}														//UInt8[16] containerID
	};
	if (!desc)
		return(kIOReturnNoMemory);
	
	if (!desc->appendBytes(&bosDesc,  bosDesc.bLength))
		return(kIOReturnNoMemory);
	
	if (!desc->appendBytes(&capDesc,  capDesc.bLength))
		return(kIOReturnNoMemory);
	
	if (!desc->appendBytes(&ssCapDesc, ssCapDesc.bLength))
		return(kIOReturnNoMemory);
	
	if (!desc->appendBytes(&containerDesc, containerDesc.bLength))
		return(kIOReturnNoMemory);
	
	return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::GetRootHubDescriptor(IOUSBHubDescriptor *desc)
{
	IOUSBHubDescriptor	hubDesc;
	UInt32				HCSParams;
	UInt8				pps;
	unsigned int		i, numBytes;
	UInt8 *				dstPtr;
	UInt32				appleCaptive = 0;
	OSNumber *			appleCaptiveProperty = NULL;
	
	hubDesc.length = sizeof(IOUSBHubDescriptor);
	hubDesc.hubType = kUSBHubDescriptorType;
	
	HCSParams = Read32Reg(&_pXHCICapRegisters->HCCParams);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	hubDesc.numPorts = _v3ExpansionData->_rootHubNumPortsHS;
	
	pps = (HCSParams & kXHCIPPCBit) != 0;
	
	hubDesc.characteristics = 0;
	hubDesc.characteristics |= (pps  ? kPerPortSwitchingBit   : 0);
	
	if ( !(hubDesc.characteristics & ( kNoOverCurrentBit | kPerPortOverCurrentBit)) )
	{
		// We have Global overcurrent protection
		// _gangedOvercurrent = true;
	}
	
	// Everything else is zero
	hubDesc.characteristics = HostToUSBWord(hubDesc.characteristics);
	
	hubDesc.powerOnToGood = 50;	// It don't tell us??
	hubDesc.hubCurrent = 0;
	
	numBytes = (hubDesc.numPorts + 1) / 8 + 1;
	
	// Set removable port flags -- we might have an AAPL property that tells us which ports
	// are captive.  If we don't then we'll assume that all ports are removable
	appleCaptiveProperty = OSDynamicCast(OSNumber, _device->getProperty(kAppleInternalUSBDevice));
	if (appleCaptiveProperty)
		appleCaptive = appleCaptiveProperty->unsigned32BitValue();
	
	dstPtr = (UInt8 *)&hubDesc.removablePortFlags[0];
	for ( i = 0; i < numBytes; i++) 
	{
		*dstPtr++ = (UInt8) (appleCaptive & 0xFF);
		appleCaptive >>= 8;
	}
	
	// Set power control flags.
	// All ports set as 1.
	for (i=0; i<numBytes; i++) 
	{
		*dstPtr++ = 0xFF;
	}
	
	// Adjust descriptor length to account for
	// number of bytes used in removable and power control arrays.
	hubDesc.length -= ((sizeof(hubDesc.removablePortFlags) - numBytes) +
					   (sizeof(hubDesc.pwrCtlPortFlags) - numBytes));
	
	if (!desc)
		return kIOReturnNoMemory;
	
	bcopy(&hubDesc, desc, hubDesc.length);
	
	return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::GetRootHub3Descriptor(IOUSB3HubDescriptor *desc)
{
	IOUSB3HubDescriptor	hubDesc;
	UInt32				HCCParams;
    UInt8				pps;
    unsigned int		i, numBytes;
    UInt8 *				dstPtr;
    UInt32				appleCaptive = 0;
	OSNumber *			appleCaptiveProperty = NULL;
	
    hubDesc.length = sizeof(IOUSB3HubDescriptor);
    hubDesc.hubType = kUSB3HubDescriptorType;
	
	
	HCCParams = Read32Reg(&_pXHCICapRegisters->HCCParams);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	hubDesc.numPorts = _v3ExpansionData->_rootHubNumPortsSS;
	
	pps = (HCCParams & kXHCIPPCBit) != 0;
	
    hubDesc.characteristics = 0;
    hubDesc.characteristics |= (pps  ? kPerPortSwitchingBit   : 0);
    
	if ( !(hubDesc.characteristics & ( kNoOverCurrentBit | kPerPortOverCurrentBit)) )
	{
		// We have Global overcurrent protection
		//_gangedOvercurrent = true;
	}
	
    // Everything else is zero
    
    hubDesc.characteristics = HostToUSBWord(hubDesc.characteristics);
	
    hubDesc.powerOnToGood = 50;	// It don't tell us??
    hubDesc.hubCurrent = 0;
	hubDesc.hubHdrDecLat = 0;
	hubDesc.hubDelay = 10;			// 10 ns -- made it up
	
    numBytes = (hubDesc.numPorts + 1) / 8 + 1;
    
	// Set removable port flags -- we might have an AAPL property that tells us which ports
	// are captive.  If we don't then we'll assume that all ports are removable.  The hub driver will
	// walk the ACPI tables and set this up.
	appleCaptiveProperty = OSDynamicCast(OSNumber, _device->getProperty(kAppleInternalUSBDevice));
	if (appleCaptiveProperty)
		appleCaptive = appleCaptiveProperty->unsigned32BitValue();
	
	dstPtr = (UInt8 *)&hubDesc.removablePortFlags[0];
    for ( i = 0; i < numBytes; i++) 
	{
		*dstPtr++ = (UInt8) (appleCaptive & 0xFF);
        appleCaptive >>= 8;
	}
	
    // Set power control flags.
    // All ports set as 1.
    for (i=0; i<numBytes; i++) 
	{
        *dstPtr++ = 0xFF;
    }
	
    if (!desc)
        return kIOReturnNoMemory;
	
    bcopy(&hubDesc, desc, hubDesc.length);
	
    return kIOReturnSuccess;
}


#pragma mark Root Hub Methods


IOReturn 
AppleUSBXHCI::SetHubAddress(UInt16 wValue)
{
	UInt8 speed		= ((wValue & kUSBSpeed_Mask) >> kUSBSpeed_Shift);
	UInt16 address	= ((wValue & kUSBAddress_Mask) >> kUSBAddress_Shift);
	
    USBLog(3, "AppleUSBXHCI[%p]::SetHubAddress - set to: %d address: %d speed: %d", this, wValue, address, speed);
	
	if(speed == kUSBDeviceSpeedSuper)
	{
		_rootHubFuncAddressSS = address;
	}
	else
	{
		_rootHubFuncAddressHS = address;
	}	
	
	TestCommands();
    return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::GetRootHubStatus(IOUSBHubStatus *status)
{
    *(UInt32 *)status = 0;		// The XHCI doesn't have OC or LPS status, nop remote wakeup status
	
    return kIOReturnSuccess;
}


IOReturn
AppleUSBXHCI::SetRootHubFeature(UInt16 wValue)
{
    switch(wValue)
    {
        case kUSBHubLocalPowerChangeFeature :
			USBLog(3,"AppleUSBXHCI[%p]: unimplemented Set Power Change Feature",  this);
            // XHCIRootHubLPSChange(true);  // not implemented yet
            break;
			
        case kUSBHubOverCurrentChangeFeature :
            USBLog(3,"AppleUSBXHCI[%p]: unimplemented Set Overcurrent Change Feature",  this);
            // XHCIRootHubOCChange(true);  // not implemented yet
            break;
            
        default:
            USBLog(3,"AppleUSBXHCI[%p]: Unknown hub set (%d) in root hub",  this, wValue);
            break;
    }
	
    return kIOReturnSuccess;
}


IOReturn
AppleUSBXHCI::GetRootHubPortErrorCount(UInt16 port, UInt16 *count)
{
	UInt16		adjustedPortIndex = port;
	
	// This call is only valid for super speed ports, assume this is a super speed port,, so get the adjusted port Index
	// (the AdjustRootHubPortNumbers() call will actually modify the parameter to return an index
	AdjustRootHubPortNumbers(kUSBDeviceSpeedSuper, &adjustedPortIndex);

	*count = Read32Reg(&_pXHCIRegisters->PortReg[adjustedPortIndex].PortLI);
	USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortErrorCount - PORTLI[%d]: 0x%04x", this, (int)adjustedPortIndex, *count);
	
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
    return kIOReturnSuccess;
}


//================================================================================================
//
// UIMRootHubStatusChange
//
//  This method gets called during a 32 ms timer task to see if there is anything that needs to be 
//  reported to the root hub driver
//  
//================================================================================================
//
void 
AppleUSBXHCI::UIMRootHubStatusChange(void)
{
    UInt16										statusChangedBitmap;   /* only have 15 ports in XHCI */
    IOUSBHubPortStatus							portStatus;
    UInt32										hubStatus, statusBit, tempStatus;
    unsigned int								port;
	IOReturn									retStatus;
    
	//Printinterrupter(0, "poll");
	
	//PrintRuntimeRegs();
	
	// USBLog(3, "AppleUSBXHCI[%p]::UIMRootHubStatusChange ", this);
	/*
     * Encode the status change bitmap.  The format of the bitmap:
     * bit0 = hub status changed
     * bit1 = port 1 status changed
     * bit2 = port 2 status changed
     * ...
     * See USB 1.0 spec section 11.8.3 for more info.
     */
	
    if((Read32Reg(&_pXHCIRegisters->USBSTS)& kXHCIHSEBit) != 0)
    {
        if(!_HSEReported)
        {
            USBError(1, "AppleUSBXHCI[%p]::UIMRootHubStatusChange - HSE bit set:%x (1)", this, Read32Reg(&_pXHCIRegisters->USBSTS));
        }
        _HSEReported = true;
    }
    
    if (_lostRegisterAccess)
    {
        return;
    }
    

    statusChangedBitmap = 0;
    statusBit = 1;
	
	if (_controllerAvailable && !_wakingFromHibernation && !_lostRegisterAccess)
	{
		RHCheckForPortResumes();
		
		if (GetRootHubStatus((IOUSBHubStatus *)&tempStatus) == kIOReturnSuccess)
		{
			hubStatus = USBToHostLong( tempStatus );
			if ((hubStatus & (kHubLocalPowerStatus | kHubOverCurrentIndicator) ) != 0)
				statusChangedBitmap |= statusBit; /* Hub status change bit */
			
			USBLog(7,"AppleUSBXHCI[%p]::UIMRootHubStatusChange numPorts %d _wakingFromHibernation(%s)",  this, _rootHubNumPorts, _wakingFromHibernation ? "true" : "false");
			
			for (port = 1; port <= _rootHubNumPorts; port++)
			{
                UInt16      adjustedPort = port;
                
				statusBit <<= 1;    /* Next bit */
				
				portStatus.changeFlags = 0;
				portStatus.statusFlags = 0;
                
                // If we are debouncing the port link state and thus taking a while to process a reset, then ignore any changes
                // until we're done with that
                if ( _rhPortBeingReset[port-1] )
                {
					USBLog(5, "AppleUSBXHCI[%p]::UIMRootHubStatusChange - port (%d) - still in reset, ignoring any changes", this, (int)port);
                    continue;
                }
                
                // GetRootHubPortStatus() will adjust the port # depending on whether the port is SS or not.  The routine iterates over ALL ports (SS and HS) so we need to 
                // pre-adjust the port # before calling that method (which will adjust based on whether the port is SS or not).  The way it determines whether it's SS or not 
                // is to look at the statusFlags, so we have to set those accordingly.
                bool    portIsSuperSpeed = IsRootHubPortSuperSpeed(&adjustedPort);
                if (portIsSuperSpeed)
                {
                    portStatus.statusFlags = (kUSBDeviceSpeedSuper << kUSBSpeed_Shift) & kUSBSpeed_Mask;
                }
				
				retStatus = GetRootHubPortStatus(&portStatus, adjustedPort);
				if (retStatus != kIOReturnSuccess)
				{
					USBLog(5, "AppleUSBXHCI[%p]::UIMRootHubStatusChange - got status (%p) from GetRootHubPortStatus for port (%d) - skipping", this, (void*)retStatus, (int)port);
					continue;
				}
				portStatus.changeFlags = USBToHostWord(portStatus.changeFlags);
				portStatus.statusFlags = USBToHostWord(portStatus.statusFlags);
				
				// Intel Errata (rdar://10403564):  After a HRST, if we have a connection but no connection status change, then we need to fake it
				if ((portStatus.statusFlags & kHubPortConnection) && !(portStatus.changeFlags & kHubPortConnection) && _synthesizeCSC[port-1])
				{
					USBLog(4,"AppleUSBXHCI[%p]::UIMRootHubStatusChange port %d _synthesizeCSC[%d] is true, setting the change bit",  this, port-1, _synthesizeCSC[port-1]);
					portStatus.changeFlags |= kHubPortConnection;
				}
				
				if (portStatus.changeFlags & kHubPortSuperSpeedStateChangeMask) 
				{
					USBLog(4,"AppleUSBXHCI[%p]::UIMRootHubStatusChange port %d status(0x%04x) change(0x%04x)",  this, port, portStatus.statusFlags, portStatus.changeFlags);
					portStatus.changeFlags |= kHubPortConnection;
					
					if ( portStatus.changeFlags & kHubPortOverCurrent )
					{
						USBLog(3,"AppleUSBXHCI[%p]::UIMRootHubStatusChange port %d had an overcurrent",  this, port);
					}
                    if(!CheckNECFirmware())
                    {
                        USBLog(2, "AppleUSBXHCI[%p]::UIMRootHubStatusChange - This NEC XHCI controller needs a firmware upgrade before it will work. Suppressing status change",this);
                        // We don't work with this rev of the NEC controller.
                        Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, Read32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC));  // Clear change bits
						if (_lostRegisterAccess)
						{
							return;
						}
                    }
                    else
                    {
                        statusChangedBitmap |= statusBit; 	// Hub status change bit
                    }
				}
			}
		}
		
		if (statusChangedBitmap)
		{
			USBLog(3,"AppleUSBXHCI[%p]::UIMRootHubStatusChange got bitmap (%p)",  this, (void*)statusChangedBitmap);
		}
		_rootHubStatusChangedBitmap = statusChangedBitmap;
	}
}


#pragma mark Port Methods

int AppleUSBXHCI::FindSlotFromPort(UInt16 port)
{
    //USBLog(2,"AppleUSBXHCI[%p]::FindSlotFromPort port:%dx",  this, (int)port);
    int slot = -1;
    for(int i = 0; i<kMaxSlots; i++)
    {
        //USBLog(2,"AppleUSBXHCI[%p]::FindSlotFromPort port:%d _slots[%d].deviceContext:%p",  this, (int)port, i, _slots[i].deviceContext);
		Context * slotContext = GetSlotContext(i);
        if(slotContext != NULL)
        {
            UInt32 cPort = GetSlCtxRootHubPort(slotContext)-1;    // Ports in context numbered 1..maxports, we're passed port index, zero based.
            UInt8 cSpeed = GetSlCtxSpeed(slotContext);
            UInt32 cRouteString = GetSlCtxRouteString(slotContext);
            
            USBLog(2,"AppleUSBXHCI[%p]::FindSlotFromPort port:%d slot:%d cPort:%d, cSpeed:%d, cRoute:%x",  this, (int)port, i, (int)cPort, cSpeed, (unsigned)cRouteString);
            if(port == cPort)
            {
                if( (cSpeed == kUSBDeviceSpeedSuper) && 
                    (cRouteString == 0) )
                {
                    if(slot != -1)
                    {
                        USBLog(1,"AppleUSBXHCI[%p]::FindSlotFromPort found another device on port new:%d, old:%d",  this, i, slot);
                    }
                    else
                    {
                        // Could return and short circuit the rest of this, but want to check we're getting this right
                        slot = i;
                    }
                }
            }
        }
    }
    return(slot);
}

IOReturn 
AppleUSBXHCI::GetRootHubPortStatus(IOUSBHubPortStatus *status, UInt16 port)
{
	UInt32		portSC, PLS;
	bool		suspend = false;
	UInt16		statusFlags, changeFlags;
	int 		speed;
	int 		controllerSpeed;
	bool		superSpeedSimulation;
	bool		warmResetIssued = false;
	
	if ( _myBusState == kUSBBusStateSuspended )
        return kIOReturnNotResponding;
	
    if(_lostRegisterAccess)
    {
        return(kIOReturnNoDevice);
    }
	
	RHCommandHeaderPtr command = (RHCommandHeaderPtr)(status);
	speed = ((command->request & kUSBSpeed_Mask) >> kUSBSpeed_Shift);
	
	AdjustRootHubPortNumbers(speed, &port);
	
	superSpeedSimulation = (speed == kUSBDeviceSpeedSuper) ? true : false;
	
	USBLog(7, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d Simulation: %s ",  this, port, superSpeedSimulation ? "SuperSpeed" : "Classic Speed");
	
	//adjust port number for array use
    port--;
	if (port >= _rootHubNumPorts)
    {
        USBLog(3, "AppleUSBXHCI[%p]::GetRootHubPortStatus Too many ports specified(%d > %d)",  this, port, _rootHubNumPorts);
		return kIOReturnBadArgument;
    }
	
	// We shall return the status/change bits according to the speed of the device:  If SuperSpeed, according to USB3.0, else, according to USB 2.0.
	// This is because this root hub simulation supports both kinds of controllers SS and HS.
	//
	portSC = Read32Reg(&_pXHCIRegisters->PortReg[port].PortSC);
	if (_lostRegisterAccess)
	{
		// our controller appears to have gone away
		return	kIOReturnNoDevice;
	}
	
	// Set the Status flags
	
	// These bits in right place in register.
	statusFlags = portSC & (kXHCIPortSC_CCS | kXHCIPortSC_PED | kXHCIPortSC_OCA | kXHCIPortSC_PR);
	
	PLS = (UInt32)(portSC & kXHCIPortSC_LinkState_Mask) >> kXHCIPortSC_LinkState_Shift;
	controllerSpeed = (UInt32)(portSC & kXHCIPortSC_Speed_Mask) >> kXHCIPortSC_Speed_Shift;
	
	if (!superSpeedSimulation)
	{
		if(PLS == kXHCIPortSC_PLS_Suspend)
		{
			suspend = true;
			statusFlags |= kHubPortSuspend;
		}
		
		if(controllerSpeed == kXHCIPortSC_LS)
		{
			statusFlags |= kHubPortLowSpeed;
		}
		else if(controllerSpeed == kXHCIPortSC_HS)
		{
			statusFlags |= kHubPortHighSpeed;
		}
		else if ( controllerSpeed == kXHCIPortSC_SS)
		{
			// We cannot report a super speed device attached to a 2.0 hub
		}
		
		if(portSC & kXHCIPortSC_PP)
		{
			statusFlags |= kHubPortPower;
		}
	}
	else
	{
		if(PLS == kXHCIPortSC_PLS_Suspend)
		{
			suspend = true;
			statusFlags |= kHubPortSuspend;
		}
		
		statusFlags |= (PLS << kSSHubPortStatusLinkStateShift);
		
		if(portSC & kXHCIPortSC_PP)
		{
			statusFlags |= kSSHubPortStatusPowerMask;
		}
		
		// Set the reserved and speed bits to 0
		statusFlags &= 0x03FF;
	}
	
	if (!superSpeedSimulation)
	{
		// These flags also in the right order (Connection, Enabled, Overcurrent, Reset)
		changeFlags = (portSC & (kXHCIPortSC_CSC | kXHCIPortSC_PEC | kXHCIPortSC_OCC | kXHCIPortSC_PRC)) >> 17;
		
		// PortLink State
		changeFlags += (((portSC &  kXHCIPortSC_PLC) >> 22) << kSSHubPortChangePortLinkStateBit);
		
		// Config Error
		changeFlags |= (((portSC &  kXHCIPortSC_CEC) >> 23) << kSSHubPortChangePortConfigErrBit);
		
		if( suspend != _prevSuspend[port])
		{
			_prevSuspend[port] = suspend;
			if(!suspend)	// Only set this bit if its a resume
			{
				_suspendChangeBits[port] = true;
			}
		}
		
		if(_suspendChangeBits[port])
		{
			changeFlags |= kHubPortSuspend;
		}
	}
	else
	{
		// These flags also in the right order (Connection, OverCurrent, Reset)
		changeFlags = (portSC & (kXHCIPortSC_CSC | kXHCIPortSC_OCC | kXHCIPortSC_PRC)) >> 17;
        
		// Warm reset
		changeFlags |= (((portSC &  kXHCIPortSC_WRC) >> 19) << kSSHubPortChangeBHResetBit);
		
		// PortLink State
		changeFlags += (((portSC &  kXHCIPortSC_PLC) >> 22) << kSSHubPortChangePortLinkStateBit);
		
		// Config Error
		changeFlags |= (((portSC &  kXHCIPortSC_CEC) >> 23) << kSSHubPortChangePortConfigErrBit);
		
		if( suspend != _prevSuspend[port])
		{
			_prevSuspend[port] = suspend;
			if(!suspend)	// Only set this bit if its a resume
			{
				_suspendChangeBits[port] = true;
			}
		}
		
		if(_suspendChangeBits[port])
		{
			changeFlags |= kHubPortSuspend;
		}
	}
	
    // If we are in the process of waiting for a warm reset to be completed because
	// we are trying to recover from a blip in the link, then don't report a change 
	// but we still need to report whatever status we have
	if ( _portIsWaitingForWRC[port] )
	{
		status->changeFlags = 0;
		status->statusFlags = HostToUSBWord(statusFlags);
		status->statusFlags |= kHubPortConnection;
        
        USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d _portIsWaitingForWRC[%d]==TRUE. Not reporting a change, status: 0x%04x",  this, port+1, port, status->statusFlags);
		
		return kIOReturnSuccess;
	}
    
	// rdar://problem/12006470 The code in UIMRootHubStatusChange() for (Intel Errata rdar://10403564)
	// would ensure that we get to know that there is a status change and PortStatusChangeHandler() runs.
	// When PortStatusChangeHandler() calls the GetPortStatus() which translates to GetRootHubPortStatus()
	// in XHCI we need to synthesize the connection change bit again until XHCIRootHubClearPortConnectionChange()
	// clears the _synthesizeCSC[port] flag.
	if ((portSC & kXHCIPortSC_CCS) && !(portSC & kXHCIPortSC_CSC) && (_synthesizeCSC[port]))
	{
		USBLog(3, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d synthesizing CSC", this, port);
		changeFlags |= kHubPortConnection;
	}

   
    // Look to see if we have a connection change and a PortLinkChange but the PortLinkState is SS.Inactive, which can indicate that 
    // the device is having trouble establishing a link.  If so, then issue a warm reset and wait for the PRC/WRC to show up.  When
    // they do, look to see if there is now a good link (U0) and a connection.  If so, then assume that we have kicked the link
    // into functional and clear the status.
    if ( superSpeedSimulation && (changeFlags & kHubPortConnection) && (changeFlags & kSSHubPortChangePortLinkStateMask) && (PLS == kXHCIPortSC_PLS_Inactive))
    {
        // We should only do this workaround if we have a super speed device already enumerated.  We check that by looking to see
        // if the XHCI slot for this port is valid -- it only is when we have succcessfully enumerated a device.  If it's not, then we
        // just let the normal changes go thru
        int slotID = FindSlotFromPort(port);
        if(slotID >= 0)
        {
            UInt32  portSCAfterReset;
            UInt32  PLSAfterReset;
            
            int retries = 8;
            
            // Since we can release the gate, we need to avoid reentering this method.  We will return no status or change while this ivar is true
            _portIsWaitingForWRC[port] = true;
            
            USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d we have a CSC, PLC, and PLS == SS.Inactive.  Issue a warm reset (PortSC = 0x%08x)",  this, port+1, (uint32_t)portSC);
            
            (void) XHCIRootHubWarmResetPort(port+1);
            
			portSC = Read32Reg(&_pXHCIRegisters->PortReg[port].PortSC);
			if (_lostRegisterAccess)
			{
				_portIsWaitingForWRC[port] = false;
				return kIOReturnNoDevice;
			}
			
            while ( retries-- && !(portSC & kXHCIPortSC_WRC) )
            {
                AbsoluteTime		deadline;
                IOReturn        	err;
                IOCommandGate * 	commandGate = GetCommandGate();
                UInt32				flag = 0;
                
                if ( getWorkLoop()->inGate() )
                {
                    clock_interval_to_deadline(32, kMillisecondScale, &deadline);
                    
                    USBLog(6,"AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, waiting(%d) for WRC (and PRC), calling commandSleep with a 32 ms timeout", this, port+1, (uint32_t)retries);
                    err = commandGate->commandSleep(&flag, deadline, THREAD_ABORTSAFE);
                    USBLog(6,"AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, commandSleep returned 0x%x", this, port+1, (uint32_t)err);
                }
                else
                {
                    USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, - waiting(%d) for WRC (and PRC)", this, port+1, (uint32_t)retries);
                    IOSleep(32);
                }
				
				portSC = Read32Reg(&_pXHCIRegisters->PortReg[port].PortSC);
				if (_lostRegisterAccess)
				{
					_portIsWaitingForWRC[port] = false;
					return kIOReturnNoDevice;
				}
            }
            
            portSCAfterReset = Read32Reg(&_pXHCIRegisters->PortReg[port].PortSC);
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
            PLSAfterReset = (UInt32)(portSCAfterReset & kXHCIPortSC_LinkState_Mask) >> kXHCIPortSC_LinkState_Shift;
            USBLog(5, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, - after waiting %d ms for PRC, we have a portSC: 0x%08x, PLS = %d", this, port+1, (uint32_t)(8-retries-1)*32, (uint32_t)portSCAfterReset, (uint32_t)PLSAfterReset);
            
            // If we have a connection and the port is enabled and it's in U0, then clear the CSC,PRC, and WRC and don't report a connection
            if ( (PLSAfterReset == kXHCIPortSC_PLS_U0) && (portSCAfterReset & (kXHCIPortSC_CCS | kXHCIPortSC_PED)) /* && speed is super? */ )
            {
                USBLog(5, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, - Looks like we have a good link after the reset, so clear any changes", this, port+1);
                // Clear the connection in the change flags
                changeFlags &= ~(kHubPortConnection);
                
                XHCIRootHubClearPortConnectionChange(port+1);
            }
            
            USBLog(5, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, - Clearing the PRC/WRC", this, port+1);
            
            // Clear the CSC, PRC and the WRC
            XHCIRootHubClearWarmResetChange(port+1);
            XHCIRootHubClearResetChange(0, port+1);
            
            // Set the deviceNeedsReset for this device.
            USBLog(2, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, - setting deviceNeedsReset on slot:%d", this, port+1, slotID);
            _slots[slotID].deviceNeedsReset = true;
            
            _portIsWaitingForWRC[port] = false;
            
            // Since we already waited for a while to issue the WarmReset and get the change, make it so that we don't debounce a connection
            UInt64      currentNanoSeconds;
            uint64_t    now = mach_absolute_time();
            
            absolutetime_to_nanoseconds( *( AbsoluteTime * ) &now, &currentNanoSeconds );        
            _debounceNanoSeconds[port] = currentNanoSeconds - kConnectionDebounceIntervalInNanos;
            _portIsDebouncing[port] = true;
            warmResetIssued = true;
            if ( statusFlags & kHubPortConnection)
            {
                _debouncingADisconnect[port] = false;
            }
            else
            {
                _debouncingADisconnect[port] = true;
            }
        }
        else 
        {
            USBLog(1, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, - Rikiki workaround but no device enumerated", this, port+1);
        }
    }
    
    // Debounce a connection change for kConnectionDebounceIntervalInNanos (nominally 100 ms), only for super speed ports
	if ( superSpeedSimulation && !warmResetIssued )
	{
		// Debounce when there is a connection change and no WRC or PRC or PR  
		if ( (changeFlags & kHubPortConnection) && !(changeFlags & kSSHubPortChangeBHResetMask) && !(changeFlags & kHubPortBeingReset) && !(statusFlags & kSSHubPortStatusBeingResetMask))
		{
			uint64_t    now = mach_absolute_time();
			
			USBLog(7, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, we have a connection change",  this, port+1);
			
			if (_portIsDebouncing[port])
			{
				// We are already debouncing a connection on this port, so check to see if it has been at least 100ms since we started debouncing.  
				// If it is not just clear the connection change
				
				UInt64      currentNanoSeconds;
				
				absolutetime_to_nanoseconds( *( AbsoluteTime * ) &now, &currentNanoSeconds );        
				
				if ((currentNanoSeconds - _debounceNanoSeconds[port]) < kConnectionDebounceIntervalInNanos)
				{
					USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, we have a connection change, we're debouncing (%s),  but it's still not 100ms: now: %qd (diff: %qd ms)",  this, port+1, _debouncingADisconnect[port] ? "DISCONNECT" : "CONNECTION", currentNanoSeconds, (currentNanoSeconds-_debounceNanoSeconds[port])/1000000);
					
					// OK, it has not been 100ms since we saw a connection change, so remove it 
					changeFlags &= ~(kHubPortConnection);
				}
				else
				{
					// We have finished debouncing it.  Report the connection change if appropriate.  We will clear the debouncing flag once we get a clear port feature for the connection change
					
					// If we were debouncing a disconnection, check to make sure we don't have a connection present.
					// If we were debouncing a connection, check to make sure we still have a connection.
					
					if ( _debouncingADisconnect[port] && (statusFlags & kHubPortConnection) )
					{
						// Looks like the device disconnected and came back, so clear the connection in the flags and in the portSC
						USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, we were debouncing a connection (%qd ms ago), but we have a device present again!",  this, port+1, (currentNanoSeconds-_debounceNanoSeconds[port])/1000000);
						changeFlags &= ~(kHubPortConnection);  
						(void) XHCIRootHubClearPortConnectionChange(port+1);
					}
					else if ( !(_debouncingADisconnect[port]) && !(statusFlags & kHubPortConnection) )
					{
						// Looks like the device connected and then went away, so clear the connection in the flags and in the portSC
						USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, we were debouncing a disconnect (%qd ms ago), but we have a device present again!",  this, port+1, (currentNanoSeconds-_debounceNanoSeconds[port])/1000000);
						changeFlags &= ~(kHubPortConnection);  
						(void) XHCIRootHubClearPortConnectionChange(port+1);
					}
					else
					{
						USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d debouncing a %s, (%qd ms ago), but the CSC did not change (CSC: 0x%x), report the change",  this, port+1, _debouncingADisconnect[port] ? "DISCONNECT" : "CONNECTION", (currentNanoSeconds-_debounceNanoSeconds[port])/1000000, (statusFlags & kHubPortConnection));
					}
				}
			}
			else
			{
				// We have a connection change and we were not debouncing.  Clear the change and set our flag and time stamp
				if ( statusFlags & kHubPortConnection)
				{
					_debouncingADisconnect[port] = false;
				}
				else
				{
					_debouncingADisconnect[port] = true;
				}
				
				changeFlags &= ~(kHubPortConnection);          
				absolutetime_to_nanoseconds( *( AbsoluteTime * ) &now, &(_debounceNanoSeconds[port]) );        
				_portIsDebouncing[port] = true;
				USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d we have a connection change (%s), but we weren't debouncing, starting now: %qd",  this, port+1, _debouncingADisconnect[port] ? "DISCONNECT" : "CONNECTION", _debounceNanoSeconds[port] );
			}
		}
		else
		{
			// We have no connection change on this port.  If we were debouncing, reset our flag no mater what.
			USBLog(7, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d debouncing a %s, no connection change",  this, port+1, _debouncingADisconnect[port] ? "DISCONNECT" : "CONNECTION");
			_portIsDebouncing[port] = false;
			_debouncingADisconnect[port] = false;
		}
    }
    
    // If the port is debouncing, then fake the port indicator flag.  This will tell the hub driver that we do have something connected
    if ( _portIsDebouncing[port] )
    {
        USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d debouncing a %s, (_portIsDebouncing is true), setting kHubPortDebouncing status bit",  this, port+1, _debouncingADisconnect[port] ? "DISCONNECT" : "CONNECTION" );
        statusFlags |= kHubPortDebouncing;
    }
    
	
	// For super speed simulations, indicate that we don't want to display the overcurrent dialog by
	// setting the kHubPortDebouncing flag
	if (superSpeedSimulation && (changeFlags & kHubPortOverCurrent || statusFlags & kHubPortOverCurrent))
	{
        USBLog(6, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d Overcurrent on super speed root hub, setting kHubPortDebouncing status bit",  this, port+1 );
		statusFlags |= kHubPortDebouncing;
	}
	
	status->statusFlags = HostToUSBWord(statusFlags);
    status->changeFlags = HostToUSBWord(changeFlags);
	
#if 1
	// For debugging print out status changes
	{
		if(port < kMaxSavePortStatus)
		{
			if( (statusFlags != _saveStatus[port]) ||
			   (changeFlags != _saveChange[port])  )
			{
				USBLog(3, "AppleUSBXHCI[%p]::GetRootHubPortStatus port: %d, portSC: 0x%08x, status: 0x%04x, change: 0x%04x, portIsDebouncing: %d",  this, port+1, (uint32_t)portSC, statusFlags, changeFlags, _portIsDebouncing[port]);
				_saveStatus[port] = statusFlags;
				_saveChange[port] = changeFlags;
			}
		}
		else
		{
			USBLog(3, "AppleUSBXHCI[%p]::GetRootHubPortStatus unexpected number of ports. port: %d, portSC: 0x%08x, status: 0x%04x, change: 0x%04x",  this, port+1, (uint32_t)portSC, statusFlags, changeFlags);
		}
		
	}
#endif
	return kIOReturnSuccess;
	
}


IOReturn 
AppleUSBXHCI::SetRootHubPortFeature(UInt16 wValue, UInt16 wIndex)
{
    IOReturn	err;
    UInt16		port = wIndex & 0x00FF;
	UInt8		speed = ((wValue & kUSBSpeed_Mask) >> kUSBSpeed_Shift);
	
	wValue >>= 8;
	
	AdjustRootHubPortNumbers(speed, &port);
		
    switch(wValue)
    {
        case kUSBHubPortSuspendFeature :
            USBLog(5,"AppleUSBXHCI[%p]::SetRootHubPortFeature suspending port %d",  this, port);
            err = XHCIRootHubSuspendPort(speed, port, true);
            break;
			
        case kUSBHubPortResetFeature :
			USBLog(5,"AppleUSBXHCI[%p]::SetRootHubPortFeature reset port %d",  this, port);
			err = XHCIRootHubResetPort(speed, port);
            break;
			
        case kUSBHubPortBHPortResetFeature :
			USBLog(5,"AppleUSBXHCI[%p]::SetRootHubPortFeature warm reset port %d",  this, port);
			err = XHCIRootHubWarmResetPort(port);
            break;
			
        case kUSBHubPortLinkStateFeature :
			USBLog(5,"AppleUSBXHCI[%p]::SetRootHubPortFeature kUSBHubPortLinkStateFeature(%d) port %d",  this, wIndex>>8, port);
			err = XHCIRootHubSetLinkStatePort(wIndex>>8, port);
            break;
			
        case kUSBHubPortEnableFeature :
			USBLog(5,"AppleUSBXHCI[%p]::SetRootHubPortFeature enable port %d",  this, port);
          	err = XHCIRootHubEnablePort(port, true);
            break;
			
        case kUSBHubPortPowerFeature :
			USBLog(3, "AppleUSBXHCI[%p]::SetRootHubPortFeature - setting kUSBHubPortPowerFeature on port %d", this, port);
            (void) XHCIRootHubPowerPort(port, true);
            err = XHCIRootHubPower(true);
            break;
			
        case kUSBHubPortU1TimeoutFeature :
			USBLog(5,"AppleUSBXHCI[%p]::SetRootHubPortFeature kUSBHubPortU1TimeoutFeature(%d) port %d",  this, wIndex>>8, port);
			err = XHCIRootHubSetU1TimeoutPort(wIndex>>8, port);
            break;
			
        case kUSBHubPortU2TimeoutFeature :
			USBLog(5,"AppleUSBXHCI[%p]::SetRootHubPortFeature kUSBHubPortU2TimeoutFeature(%d) port %d",  this, wIndex>>8, port);
			err = XHCIRootHubSetU2TimeoutPort(wIndex>>8, port);
            break;
			
        case kUSBHubPortRemoteWakeMaskFeature :
			USBLog(5,"AppleUSBXHCI[%p]::SetRootHubPortFeature kUSBHubPortRemoteWakeMaskFeature(%d) port %d",  this, wIndex>>8, port);
			err = XHCIRootHubRemoteWakeupMaskPort(wIndex>>8, port);
            break;
			
        default:
            USBLog(3,"AppleUSBXHCI[%p]::SetRootHubPortFeature unknown wValue %d, wIndex %d",  this, wValue, wIndex);
            err = kIOReturnUnsupported;
            break;
    }
	
    return err;
}


IOReturn 
AppleUSBXHCI::ClearRootHubPortFeature(UInt16 wValue, UInt16 wIndex)
{
    IOReturn	err;
	UInt8		speed = ((wValue & kUSBSpeed_Mask) >> kUSBSpeed_Shift);
    UInt16		port = wIndex;
	
	wValue >>= 8;
	
	USBLog(4, "AppleUSBXHCI[%p]::ClearRootHubPortFeature - port %d, feature: %d speed: %d",  this, wIndex, wValue, speed);
	
	AdjustRootHubPortNumbers(speed, &port);
	
    switch(wValue)
    {
        case kUSBHubPortEnableFeature :
			err = XHCIRootHubEnablePort(port, false);
            break;
			
        case kUSBHubPortSuspendFeature :
            USBLog(5,"AppleUSBXHCI[%p]::ClearRootHubPortFeature resuming port %d",  this, port);
			err = XHCIRootHubSuspendPort(speed, port, false);
            break;
			
        case kUSBHubPortPowerFeature :
			err = XHCIRootHubPowerPort(port, false);
            // Now need to check if all ports are switched off and
            // gang off if in gang mode
            break;
			
			// ****** Change features *******
        case kUSBHubPortConnectionChangeFeature :
			err = XHCIRootHubClearPortConnectionChange(port);
            break;
			
        case kUSBHubPortEnableChangeFeature :
            err = XHCIRootHubClearPortEnableChange(port);
            break;
			
        case kUSBHubPortSuspendChangeFeature :
			err = XHCIRootHubClearPortSuspendChange(port);
            break;
			
        case kUSBHubPortOverCurrentChangeFeature :
			err = XHCIRootHubResetOverCurrentChange(port);
            break;
			
        case kUSBHubPortResetChangeFeature :
			err = XHCIRootHubClearResetChange(speed, port);
            break;
			
        case kUSBHubPortBHResetChangeFeature :
			err = XHCIRootHubClearWarmResetChange(port);
            break;
			
        case kUSBHubPortLinkStateChangeFeature :
			err = XHCIRootHubClearPortLinkChange(port);
            break;
			
        case kUSBHubPortConfigErrorChangeFeature :
			err = XHCIRootHubClearConfigErrorChange(port);
            break;
			
        default:
            USBLog(3,"AppleUSBXHCI[%p]::ClearRootHubPortFeature unknown wValue %d, wIndex %d",  this, wValue, wIndex);
            err = kIOReturnUnsupported;
            break;
    }
	
		USBLog(4, "AppleUSBXHCI[%p]::ClearRootHubPortFeature - port %d, feature: %d speed: %d returning %x",  this, wIndex, wValue, speed, err);

	
    return err;
}


#pragma mark Set Port Feature


IOReturn 
AppleUSBXHCI::XHCIRootHubSuspendPort(UInt8 RHSpeed, UInt16 adjustedPort, bool suspend)
{
	UInt32 		portSC;
	
    USBLog(5,"AppleUSBXHCI[%p]::XHCIRootHubSuspendPort port: %d, %s, PortSC(0x%08x)",  this, adjustedPort, suspend ? "SUSPEND" : "RESUME", Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC));
	
    USBTrace_Start( kUSBTXHCIRootHubs, kTPXHCIRootHubPortSuspend,  (uintptr_t)this, adjustedPort, suspend, 0 );
	
	if (_rhPortBeingResumed[adjustedPort-1])
	{
		if (!suspend)
		{
			USBLog(3, "AppleUSBXHCI[%p]::XHCIRootHubSuspendPort - resume on port (%d) already being resumed - gracefully ignoring", this, (int)adjustedPort);
			return kIOReturnSuccess;
		}
		USBLog(1, "AppleUSBXHCI[%p]::XHCIRootHubSuspendPort - trying to suspend port (%d) which is being resumed - UNEXPECTED", this, (int)adjustedPort);
	}
	
	if ( suspend )
	{
		UInt32	PLS = 0;
		UInt32	loop = 0;
		
		// rdar://10403553: If the target port has a link state of Recovery, setting the PLS to U3 will fail.  So, check to see if it is and if so, then wait and try 3 times.  Bail
		// if it never gets to non-recovery
		do
		{
			loop++;
			
			portSC = Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC);
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
			
			PLS = (UInt32)(portSC & kXHCIPortSC_LinkState_Mask) >> kXHCIPortSC_LinkState_Shift;
			if ((PLS == kXHCIPortSC_PLS_Recovery) && (loop < 4))
			{
				USBLog(3,"AppleUSBXHCI[%p]::XHCIRootHubSuspendPort port:%d suspend, but portSC(0x%08x) is in Recovery, waiting 50ms",  this, adjustedPort, (int)portSC);
				IOSleep(50);
			}
			else
			{
				break;
			}
		} while(true);
		
		if ( loop > 3)
		{
			USBLog(3,"AppleUSBXHCI[%p]::XHCIRootHubSuspendPort port:%d suspend, but portSC(0x%08x) still in Recovery after trying 3 times",  this, adjustedPort, (int)portSC);
			return kIOUSBDevicePortWasNotSuspended;
		}
	}
	
	portSC = GetPortSCForWriting(adjustedPort);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	if(suspend)
	{
		USBLog(3,"AppleUSBXHCI[%p]::XHCIRootHubSuspendPort port:%d suspend.",  this, adjustedPort);
		portSC |= (UInt32)(kXHCIPortSC_LWS | (kXHCIPortSC_PLS_Suspend << kXHCIPortSC_LinkState_Shift));
	}
	else
	{
        // Section 4.15.2.2 Host Initiated
        // Confirm that port is in Suspend 
        // PED = 1 && PLS = 3 - U3 State - Device Suspended
        if( RHSpeed == kUSBDeviceSpeedHigh )
        {
            // USB2 Protocol Ports
            portSC |= (UInt32)(kXHCIPortSC_LWS | (kXHCIPortSC_PLS_Resume << kXHCIPortSC_LinkState_Shift));
        }
        else if( RHSpeed == kUSBDeviceSpeedSuper )
        {
            // USB3 Protocol Ports
            portSC |= (UInt32)(kXHCIPortSC_LWS | (kXHCIPortSC_PLS_U0 << kXHCIPortSC_LinkState_Shift));
        }
        
		USBLog(3,"AppleUSBXHCI[%p]::XHCIRootHubSuspendPort port:%d resume.",  this, adjustedPort);
	}
	Write32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC, portSC);
    IOSync();
	
	IOSleep(1);											// allow it to kick in
	
    if (!suspend)
    {
		USBLog(5,"AppleUSBXHCI[%p]::XHCIRootHubSuspendPort - resuming port %d, calling out to timer", this, (int)adjustedPort);
		_rhPortBeingResumed[adjustedPort-1] = true;
		if ( _rhResumePortTimerThread[adjustedPort-1] == NULL )
		{
			USBLog(1,"AppleUSBXHCI[%p]::XHCIRootHubSuspendPort - resuming port %d, but callout thread is NULL", this, (int)adjustedPort);
		}
		else
			thread_call_enter1(_rhResumePortTimerThread[adjustedPort-1], (void*)adjustedPort);
	}
	
    portSC = Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
    USBTrace_End( kUSBTXHCIRootHubs, kTPXHCIRootHubPortSuspend,  (uintptr_t)this, adjustedPort, portSC, 0 );
	
    return kIOReturnSuccess;
	
}


IOReturn 
AppleUSBXHCI::XHCIRootHubResetPort (UInt8 rhSpeed, UInt16 port)
{
    IOReturn            kr = kIOReturnSuccess;
    IOCommandGate * 	commandGate = GetCommandGate();
    
    USBLog(6, "AppleUSBXHCI[%p]::XHCIRootHubResetPort[%d] - resetting port, RHSpeed: %d (%s)", this, port, rhSpeed, rhSpeed == kUSBDeviceSpeedHigh ? "High" : "Super");
    USBTrace_Start( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort,  (uintptr_t)this, rhSpeed, port, 0 );

    if ( _rhPortBeingReset[port-1] )
    {
    	USBLog(6, "AppleUSBXHCI[%p]::XHCIRootHubResetPort[%d] - port already being reset", this, port);
 		USBTrace(kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort, (uintptr_t)this, 7, 0, port);
		
		return kIOReturnSuccess;
    }
    
    if ( getWorkLoop()->inGate() )
    {
        // Since we are in gate, encode the rhSpeed, port, and status and call out on a separate thread.
        _rhResetParams[port-1].rhSpeed = rhSpeed;
        _rhResetParams[port-1].port = port;
        _rhResetParams[port-1].status = -1;
        
        _rhPortBeingReset[port-1] = true;
        
		retain();
		if ( thread_call_enter1(_rhResetPortThread[port-1], (void*)(&_rhResetParams[port-1]) ) == TRUE )
		{
            USBLog(6,"AppleUSBXHCI[%p]::XHCIRootHubResetPort _rhResetPortThread already queued", this);
			USBTrace(kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort, (uintptr_t)this, 8, (uintptr_t)kIOReturnNotPermitted, port);
			kr = kIOReturnNotPermitted;
		}
        else
        {
 			AbsoluteTime	deadline;
            IOReturn        err;
			
			clock_interval_to_deadline(10, kSecondScale, &deadline);
            
            USBLog(6,"AppleUSBXHCI[%p]::XHCIRootHubResetPort calling commandSleep with a 10 second timeout", this);
            err = commandGate->commandSleep(&_rhPortBeingReset[port-1], deadline, THREAD_ABORTSAFE);
            switch (err)
			{
				case THREAD_AWAKENED:
                    // Set the status to return to our caller
                    USBLog(6,"AppleUSBXHCI[%p]::XHCIRootHubResetPort commandSleep woke up normally (THREAD_AWAKENED), status = 0x%x", this, _rhResetParams[port-1].status);
                    kr = _rhResetParams[port-1].status;
					USBTrace(kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort, (uintptr_t)this, 1, (uintptr_t)kr, port);
					break;
					
				case THREAD_TIMED_OUT:
					USBLog(3,"AppleUSBXHCI[%p]::XHCIRootHubResetPort commandSleep timeout out (THREAD_TIMED_OUT)", this);
					USBTrace(kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort, (uintptr_t)this, 2, (uintptr_t)kIOReturnTimeout, port);
					kr = kIOReturnTimeout;
					break;
					
				case THREAD_INTERRUPTED:
					USBLog(3,"AppleUSBXHCI[%p]::XHCIRootHubResetPort commandSleep interrupted (THREAD_INTERRUPTED)", this);
					USBTrace(kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort, (uintptr_t)this, 3, (uintptr_t)kIOReturnNotPermitted, port);
					kr = kIOReturnNotPermitted;
					break;
					
				case THREAD_RESTART:
					USBLog(3,"AppleUSBXHCI[%p]::XHCIRootHubResetPort commandSleep restarted (THREAD_RESTART)", this);
					USBTrace(kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort, (uintptr_t)this, 4, (uintptr_t)kIOReturnNotPermitted, port);
					kr = kIOReturnNotPermitted;
					break;
					
				case kIOReturnNotPermitted:
					USBLog(3,"AppleUSBXHCI[%p]::XHCIRootHubResetPort woke up with status (kIOReturnNotPermitted) - we do not hold the WL!", this);
					USBTrace(kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort, (uintptr_t)this, 5, (uintptr_t)kIOReturnNotPermitted, port);
					kr = err;
					break;
					
				default:
					USBLog(3,"AppleUSBXHCI[%p]::XHCIRootHubResetPort woke up with unknown status 0x%x", this, (uint32_t)err);
					USBTrace(kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort, (uintptr_t)this, 6, (uintptr_t)port, err);
					kr = kIOReturnNotPermitted;
			}
        }
        
        release();
    }
    else
    {
        USBLog(6,"AppleUSBXHCI[%p]::XHCIRootHubResetPort  not in gate, calling RHResetPort directly", this);
 		USBTrace(kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort, (uintptr_t)this, 9, 0, port);
       kr = RHResetPort(rhSpeed, port);
    }
    
    USBTrace_End( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPort,  (uintptr_t)this, rhSpeed, port, kr );
    return kr;
}


IOReturn 
AppleUSBXHCI::XHCIRootHubWarmResetPort (UInt16 port)
{
	UInt32 portSC;
	
	portSC = GetPortSCForWriting(port);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	portSC |= (UInt32)kXHCIPortSC_WPR;
	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
	
    return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::XHCIRootHubSetLinkStatePort (UInt16 linkState, UInt16 port)
{
	UInt32 portSC;
	
	// If we are going to U3 (Suspend) or U0 (resume),  we should call XHCIRootHubSuspendPort directly since it deals with states.  This might need to be revisited to deal with other 
	// link states
	if (linkState == kXHCIPortSC_PLS_U0)
	{
		return XHCIRootHubSuspendPort(kUSBDeviceSpeedSuper, port, false);
	}
	if (linkState == kXHCIPortSC_PLS_Suspend)
	{
		return XHCIRootHubSuspendPort(kUSBDeviceSpeedSuper, port, true);
	}
	
	// We need to set the LWS (Link State Write Strobe bit) AND the link state bit
	portSC = GetPortSCForWriting(port);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
    USBLog(6, "AppleUSBXHCI[%p]::XHCIRootHubSetLinkStatePort - port: %d  to state: %d, portSC: 0x%08x", this, port, linkState, (uint32_t)Read32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC));
	portSC |= (UInt32)(kXHCIPortSC_LWS | (linkState << kXHCIPortSC_LinkState_Shift));
	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
	IOSync();
    
    USBLog(6, "AppleUSBXHCI[%p]::XHCIRootHubSetLinkStatePort - after, portSC: 0x%08x", this, (uint32_t)Read32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC));
    
    return kIOReturnSuccess;
	
}


IOReturn 
AppleUSBXHCI::XHCIRootHubEnablePort(UInt16 port, bool enable)
{
	UInt32 portSC;
	
    USBTrace_Start( kUSBTXHCIRootHubs, kTPXHCIRootHubPortEnable,  (uintptr_t)this, port, enable, 0 );
	
    USBLog(5,"AppleUSBXHCI[%p]::XHCIRootHubEnablePort port: %d, on: %d",  this, port, enable);
	
    if (enable)
    {
        USBLog(1,"AppleUSBXHCI[%p]::XHCIRootHubEnablePort enabling port illegal.",  this);
        return kIOReturnUnsupported;
    }
	
	portSC = GetPortSCForWriting(port);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	portSC |= (UInt32)kXHCIPortSC_PED;
	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
    
    portSC = Read32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
    USBTrace_End( kUSBTXHCIRootHubs, kTPXHCIRootHubPortEnable,  (uintptr_t)this, port, portSC, 0 );
    
	return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::XHCIRootHubPower(bool on)
{
#pragma unused (on)

    // I don't think this has a power switch
    return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::XHCIRootHubPowerPort(UInt16 port, bool on)
{
	UInt32 	portSC, newPortSC, count;
	
	USBLog(6,"AppleUSBXHCI[%p]::XHCIRootHubPowerPort %d, port: %d, portSC before: 0x%x",  this, on, port,(uint32_t) Read32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC));
	
	portSC = GetPortSCForWriting(port);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	if(on)
	{
		// The spec says that after setting Port Power, we need to make sure that it has stuck.  It also states that we need to wait 20 ms after asserting PP before attempting to change the
		// state of the port (5.4.8)
		//
		portSC |= (UInt32)kXHCIPortSC_PP;
		Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
        IOSync();
		newPortSC = Read32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC);
		if (_lostRegisterAccess)
		{
			return kIOReturnNoDevice;
		}
		
		count = 0;
		while (!(newPortSC & kXHCIPortSC_PP) && (count++ < 10))
		{
			USBLog(6,"AppleUSBXHCI[%p]::XHCIRootHubPowerPort  PortPower bit not sticking. Retrying(%d)", this, (uint32_t)count);
			Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
			IOSync();
			newPortSC = Read32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC);
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
		}

		// This should only wait if PCC is 1
		IOSleep(20);
		portSC |= (UInt32)(kXHCIPortSC_WCE | kXHCIPortSC_WDE | kXHCIPortSC_WOE);					// enable wake on connect/disconnect/overcurrent
	}
	else
	{
		portSC &= ~(UInt32)kXHCIPortSC_PP;
	}
	
	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
	IOSync();
	
	USBLog(5,"AppleUSBXHCI[%p]::XHCIRootHubPowerPort portSC after: 0x%x",  this,(uint32_t) Read32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC));
	
    return kIOReturnSuccess;
	
}


IOReturn 
AppleUSBXHCI::XHCIRootHubSetU1TimeoutPort (UInt16 timeout, UInt16 port)
{
#pragma unused (timeout, port)
    return kIOReturnUnsupported;
	
}


IOReturn 
AppleUSBXHCI::XHCIRootHubSetU2TimeoutPort (UInt16 timeout, UInt16 port)
{
#pragma unused (timeout, port)
   return kIOReturnUnsupported;
	
}


IOReturn 
AppleUSBXHCI::XHCIRootHubRemoteWakeupMaskPort (UInt16 mask, UInt16 port)
{
#pragma unused (mask, port)
    return kIOReturnUnsupported;
	
}


#pragma mark Clear Port Feature


IOReturn 
AppleUSBXHCI::XHCIRootHubClearPortConnectionChange(UInt16 port)
{
	UInt32 portSC;
	
	portSC = GetPortSCForWriting(port);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	portSC |= (UInt32)kXHCIPortSC_CSC;
	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);

	if (_synthesizeCSC[port-1])
		_synthesizeCSC[port-1] = false;
	
    // If we are clearing a connection, we should indicate that the
    // port is not being debounced anymore.
    _portIsDebouncing[port] = false;
	_debouncingADisconnect[port] = false;

	return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::XHCIRootHubClearPortEnableChange(UInt16 port)
{
	UInt32 portSC;
	
	portSC = GetPortSCForWriting(port);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	portSC |= (UInt32)kXHCIPortSC_PEC;
	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
 	IOSync();
 
	return kIOReturnSuccess;
}	


IOReturn 
AppleUSBXHCI::XHCIRootHubClearPortSuspendChange(UInt16 port)
{
	_suspendChangeBits[port-1] = false;

    return kIOReturnSuccess;
}	


IOReturn 
AppleUSBXHCI::XHCIRootHubResetOverCurrentChange(UInt16 port)
{
	UInt32 portSC;
	
	portSC = GetPortSCForWriting(port);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	portSC |= (UInt32)kXHCIPortSC_OCC;
	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
 	IOSync();

    return kIOReturnSuccess;
}


IOReturn 
AppleUSBXHCI::XHCIRootHubClearResetChange(UInt8 RHSpeed, UInt16 adjustedPort)
{
#pragma unused (RHSpeed)
		
	UInt32	portSC	= GetPortSCForWriting(adjustedPort);
    
    USBTrace_Start( kUSBTXHCIRootHubs, kTPXHCIRootHubResetResetChange,  (uintptr_t)this, adjustedPort, portSC, 0 );
    
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	portSC |= (UInt32)kXHCIPortSC_PRC;
	Write32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC, portSC);
    IOSync();
    
	USBTrace_End( kUSBTXHCIRootHubs, kTPXHCIRootHubResetResetChange,  (uintptr_t)this, adjustedPort, Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC), 0 );
    
    return kIOReturnSuccess;
}	


IOReturn
AppleUSBXHCI::XHCIRootHubClearWarmResetChange(UInt16 port)
{
	UInt32	portSC	= GetPortSCForWriting(port);
	
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}

	portSC |= (UInt32)kXHCIPortSC_WRC;
	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
    IOSync();
    
	return kIOReturnSuccess;
}


IOReturn
AppleUSBXHCI::XHCIRootHubClearPortLinkChange(UInt16 port)
{
	UInt32	portSC	= GetPortSCForWriting(port);
	
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	portSC |= (UInt32)kXHCIPortSC_PLC;
	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
    IOSync();

	return kIOReturnSuccess;
}	


IOReturn
AppleUSBXHCI::XHCIRootHubClearConfigErrorChange(UInt16 port)
{
	UInt32	portSC	= GetPortSCForWriting(port);
	
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	portSC |= (UInt32)kXHCIPortSC_CEC;
	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, portSC);
    IOSync();
    
	return kIOReturnSuccess;
}	


#pragma mark RH Reset methods

// these are for handling a root hub resume without hanging out in the WL for 20 ms
// static
void
AppleUSBXHCI::RHResetPortEntry(OSObject *target, thread_call_param_t params)
{
    AppleUSBXHCI *              me = OSDynamicCast(AppleUSBXHCI, target);
    XHCIRootHubResetParams *    rhResetParams = (XHCIRootHubResetParams *)params;
    IOCommandGate *             commandGate;
   
	if (!me)
		return;
	
    USBLog(7, "AppleUSBXHCI[%p]::RHResetPortEntry[%d] - resetting port, RHSpeed: %d (%s)", me, rhResetParams->port, rhResetParams->rhSpeed, rhResetParams->rhSpeed == kUSBDeviceSpeedHigh ? "High" : "Super");
	rhResetParams->status = me->RHResetPort(rhResetParams->rhSpeed, rhResetParams->port);
    
    USBLog(6, "AppleUSBXHCI[%p]::RHResetPortEntry[%d] - RHResetPort returned 0x%x, calling commandWakeup", me, rhResetParams->port, rhResetParams->status);

    // Indicate that we are not resetting anymore
    me->_rhPortBeingReset[rhResetParams->port-1] = false;
    
    (me->GetCommandGate())->commandWakeup(&(me->_rhPortBeingReset[rhResetParams->port-1]));
}

IOReturn
AppleUSBXHCI::RHResetPort(UInt8 RHSpeed, UInt16 adjustedPort)
{
	IOReturn	returnValue = kIOReturnSuccess;
	UInt32 		portSC;
	
    USBTrace_Start( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, adjustedPort, _errataBits, 0 );
	
    USBLog(6, "AppleUSBXHCI[%p]::RHResetPort[%d] - resetting port, RHSpeed: %d (%s)", this, adjustedPort, RHSpeed, RHSpeed == kUSBDeviceSpeedHigh ? "High" : "Super");
    
	portSC = GetPortSCForWriting(adjustedPort);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	
	portSC |= (UInt32)kXHCIPortSC_PR;
	Write32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC, portSC);
	IOSync();
	
	USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 1, adjustedPort, Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC));

	if ((_errataBits & kXHCIErrataPPTMux) != 0)
	{
		// We are running on a PPT root hub, so we need to wait for the reset to complete to see if we have
		// a high speed device and if so, switch the muxes.
		if ( ((gUSBStackDebugFlags & kUSBDisableMuxedPortsMask) >> kUSBDisableMuxedPorts) == 0 )
		{
			UInt32		connectedSpeed;
			UInt16		companionPort;
			UInt32		companionPortSC;
			UInt32 		portSCAfterReset;
            UInt32 		companionPLSAfterReset;
			UInt32		retries = 8;
			
			// Let's wait until the PortSC register indicates that we have a PRC (Port Reset Change).  The amount we wait is 8 * 32 ms.  This is
			// kind of random;  we do it in 32 ms chunks because that's the polling rate for the root hub, so it seemed like a good idea to use it.
			portSCAfterReset = Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC);
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
			
			while ( retries-- && !(portSCAfterReset & kXHCIPortSC_PRC) )
			{
				USBLog(6, "AppleUSBXHCI[%p]::RHResetPort[%d] - waiting(%d) for PRC", this, adjustedPort, (uint32_t)retries);
				USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 2, adjustedPort, retries);
				IOSleep(32);
				
				portSCAfterReset = Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC);
				if (_lostRegisterAccess)
				{
					return kIOReturnNoDevice;
				}
			}
			
            portSCAfterReset = Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC);
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
			USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 3, adjustedPort, portSCAfterReset);
           	if ( ((portSCAfterReset & kXHCIPortSC_Speed_Mask) >> kXHCIPortSC_Speed_Shift) == kXHCIPortSC_SS )
            {
                USBLog(5, "AppleUSBXHCI[%p]::RHResetPort[%d] - after waiting %d ms for PRC, we have a SuperSpeed device, so don't switch the mux (portSC: 0x%x)", this, adjustedPort, (uint32_t)(8-retries-1)*32, (uint32_t)portSCAfterReset);
				USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 4, adjustedPort, (uintptr_t)(8-retries-1)*32);
           }
            else
            {
                USBLog(5, "AppleUSBXHCI[%p]::RHResetPort[%d] - after waiting %d ms for PRC, we will now wait for another %d ms ", this, adjustedPort, (uint32_t)(7-retries)*32, (uint32_t)(500-((7-retries)*32)));
 				USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 5, adjustedPort, (uintptr_t)(500-((7-retries)*32)));
               
				// Wait for 500 ms total
                IOSleep(500-((7-retries)*32));
                
                // Now look at the companion 
                companionPort = GetCompanionRootPort(RHSpeed, adjustedPort);
                companionPortSC = Read32Reg(&_pXHCIRegisters->PortReg[companionPort-1].PortSC);
				if (_lostRegisterAccess)
				{
					return kIOReturnNoDevice;
				}
				
                portSCAfterReset = Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC);
				if (_lostRegisterAccess)
				{
					return kIOReturnNoDevice;
				}
				
                connectedSpeed	= (UInt32)((portSCAfterReset & kXHCIPortSC_Speed_Mask) >> kXHCIPortSC_Speed_Shift);
                companionPLSAfterReset = (UInt32)((companionPortSC & kXHCIPortSC_LinkState_Mask) >> kXHCIPortSC_LinkState_Shift);
                
 				USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 6, adjustedPort, companionPort);
 				USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 7, (uintptr_t)portSCAfterReset, (uintptr_t)companionPortSC);
				
                if (companionPLSAfterReset == kXHCIPortSC_PLS_ComplianceMode) 
                {
                    USBLog(2, "AppleUSBXHCI[%p]::RHResetPort[%d] - ****PORT IN COMPLIANCE MODE - CompanionPortSC: 0x%08x PLS=%x", this, adjustedPort, (int)companionPortSC, (int)companionPLSAfterReset);
					USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 8, adjustedPort, companionPLSAfterReset);
              	}
               
                USBLog(5, "AppleUSBXHCI[%p]::RHResetPort[%d] - after waiting 500 ms for PRC: speed: %d (%s), PortSCAfterReset: 0x%08x, CompanionPortSC[%d]: 0x%08x", this, adjustedPort, (uint32_t)connectedSpeed, 
                       connectedSpeed == kXHCIPortSC_SS ? "Super" : (connectedSpeed == kXHCIPortSC_HS ? "High" : (connectedSpeed == kXHCIPortSC_FS ? "Full" : (connectedSpeed == kXHCIPortSC_LS ? "Super" : "Unknown"))),
                       (uint32_t)portSCAfterReset, companionPort, (uint32_t)companionPortSC);
                
                // Determine whether we have to switch the mux
                if ((connectedSpeed == kXHCIPortSC_HS) &&           // We have a high speed device
                    (portSCAfterReset & kXHCIPortSC_CCS) &&         // We have a connection for this high speed device
                    !(companionPortSC & kXHCIPortSC_CCS) &&         // Companion port does NOT have a connection status change
                    !(companionPortSC & kXHCIPortSC_CEC) &&         // Companion port does NOT have a port config error change
                    !(companionPortSC & kXHCIPortSC_CAS) &&         // Companion port does NOT have a cold attach status
                    !(companionPortSC & kXHCIPortSC_PLC) &&         // Companion port does NOT have a Port Link Status change
                    !(companionPortSC & kXHCIPortSC_WRC) &&          // Companion port does NOT have a Warm Port Reset Change
                    (companionPLSAfterReset == kXHCIPortSC_PLS_RxDetect)   // Companion port PortLink State is Rx.Detect 
                    )
                {
                    // Looks like we have a high speed device so we need to switch the mux.  Before doing so, we'll clear the PRC and then switch the mux
                    // and wait for the CSC to come through
                    
                    portSC	= GetPortSCForWriting(adjustedPort);
					if (_lostRegisterAccess)
					{
						return kIOReturnNoDevice;
					}
					
                    portSC |= (UInt32)kXHCIPortSC_PRC;
                    Write32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC, portSC);
                    IOSync();
                    
                    USBLog(4, "AppleUSBXHCI[%p]::RHResetPort[%d] - switching mux for XHCI port: %d -> EHCI [HCSEL: %lx]", this, adjustedPort, adjustedPort, (long unsigned int)_device->configRead32(kXHCI_XUSB2PR));
					USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 9, (uintptr_t)adjustedPort, (uintptr_t)_device->configRead32(kXHCI_XUSB2PR));
                   
                    // This switches the mux from XHCI->EHCI
                    HCSelect((adjustedPort-1), kControllerEHCI);
                    
                    // Before returning, which will disable the port, wait until we see the connection status change.  The amount we wait is 8 * 32 ms.  This is
                    // kind of random;  we do it in 32 ms chunks because that's the polling rate for the root hub, so it seemed like a good idea to use it.
                    
                    retries = 8;
					portSC = Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC);
					if (_lostRegisterAccess)
					{
						return kIOReturnNoDevice;
					}
					
                    while ( retries-- && !(portSC & kXHCIPortSC_CSC) )
                    {
                        USBLog(6, "AppleUSBXHCI[%p]::RHResetPort[%d] - waiting(%d) for CSC: portSC = 0x%08x", this, adjustedPort, (uint32_t)retries, portSC);
						USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 10, adjustedPort, (uintptr_t)retries);
                       	IOSleep(32);
						
						portSC = Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC);
						if (_lostRegisterAccess)
						{
							return kIOReturnNoDevice;
						}
                    }
                    
                    USBLog(6, "AppleUSBXHCI[%p]::RHResetPort[%d] - waited %d ms for CSC: portSC = 0x%08x, CompanionPortSC[%d]: 0x%08x", this, adjustedPort, (uint32_t)(8-retries-1)*32, Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC), companionPort, Read32Reg(&_pXHCIRegisters->PortReg[companionPort-1].PortSC));
                    
                    // Clear any CSC after switching the mux
                    portSC	= GetPortSCForWriting(adjustedPort);
					if (_lostRegisterAccess)
					{
						return kIOReturnNoDevice;
					}
                    portSC |= (UInt32)kXHCIPortSC_CSC;
                    Write32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC, portSC);
                    IOSync();
                    
					USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 11, adjustedPort, (uintptr_t)(8-retries-1)*32);
                  	returnValue = kIOUSBDeviceTransferredToCompanion;
                }
                else
                {
                    USBLog(6, "AppleUSBXHCI[%p]::RHResetPort[%d] - not switching the mux because the portSC's told us not to do so", this, adjustedPort);

					USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 12, adjustedPort, _testModeEnabled);

                    // If we are not in test mode, then issue a warm reset
                    if ( !_testModeEnabled && (companionPLSAfterReset == kXHCIPortSC_PLS_ComplianceMode) ) 
                    {
                        USBLog(2, "AppleUSBXHCI[%p]::RHResetPort[%d] - CompanionPort[%d] in compliance mode issuing WarmReset", this, adjustedPort, companionPort);
						USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 13, adjustedPort, (uintptr_t)companionPLSAfterReset);
						(void) XHCIRootHubWarmResetPort(companionPort);
                        
                        // Now, wait for the WRC or the PRC for up to 256 ms
                        retries = 8;
						portSC = Read32Reg(&_pXHCIRegisters->PortReg[companionPort-1].PortSC);
						if (_lostRegisterAccess)
						{
							return kIOReturnNoDevice;
						}
						
                        while ( retries-- && !(portSC & (kXHCIPortSC_PRC | kXHCIPortSC_WRC)) )
                        {
                            USBLog(6, "AppleUSBXHCI[%p]::RHResetPort - waiting(%d) for WRC/PRC after resetting port %d", this, (uint32_t)retries, companionPort);
							USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 14, adjustedPort, (uintptr_t)retries);
                           	IOSleep(32);
							
							portSC = Read32Reg(&_pXHCIRegisters->PortReg[companionPort-1].PortSC);
							if (_lostRegisterAccess)
							{
								return kIOReturnNoDevice;
							}
                        }
                        
                        companionPortSC = Read32Reg(&_pXHCIRegisters->PortReg[companionPort-1].PortSC);
						if (_lostRegisterAccess)
						{
							return kIOReturnNoDevice;
						}
						
                        USBLog(6, "AppleUSBXHCI[%p]::RHResetPort - waited %d ms for companion port WRC/PRC: CompanionPortSC[%d]: 0x%08x", this, (uint32_t)(8-retries-1)*32, companionPort, (uint32_t)companionPortSC);
  						USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 15, adjustedPort, (uintptr_t)companionPortSC);
                      
                        // If we have a PRC or WRC clear 'em
                        if ( companionPortSC &  kXHCIPortSC_WRC)
                        {
							USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 16, adjustedPort, 0);
                           	portSC	= GetPortSCForWriting(companionPort);
							if (_lostRegisterAccess)
							{
								return kIOReturnNoDevice;
							}
							
                            portSC |= (UInt32)kXHCIPortSC_WRC;
                            Write32Reg(&_pXHCIRegisters->PortReg[companionPort-1].PortSC, portSC);
                            IOSync();
                        }
                        if ( companionPortSC &  kXHCIPortSC_PRC)
                        {
 							USBTrace( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, 17, adjustedPort, 0);
                           	portSC	= GetPortSCForWriting(companionPort);
							if (_lostRegisterAccess)
							{
								return kIOReturnNoDevice;
							}
							
                            portSC |= (UInt32)kXHCIPortSC_PRC;
                            Write32Reg(&_pXHCIRegisters->PortReg[companionPort-1].PortSC, portSC);
                            IOSync();
                        }
                    }
                    
               }
            }
		}
	}
    else
    {
        USBLog(6, "AppleUSBXHCI[%p]::RHResetPort[%d] - mux switching is disabled", this, adjustedPort);
    }
	
    USBTrace_End( kUSBTXHCIRootHubs, kTPXHCIRootHubResetPortCallout,  (uintptr_t)this, adjustedPort, Read32Reg(&_pXHCIRegisters->PortReg[adjustedPort-1].PortSC), returnValue);
	
    return returnValue;
}

#pragma mark RH Resume methods


// these are for handling a root hub resume without hanging out in the WL for 20 ms
// static
void
AppleUSBXHCI::RHResumePortTimerEntry(OSObject *target, thread_call_param_t port)
{
    AppleUSBXHCI *me = OSDynamicCast(AppleUSBXHCI, target);
	if (!me)
		return;
	
	me->RHResumePortTimer((UInt32)(uintptr_t)port);	// GCC doesn't like each cast individually, it wants both
}


void
AppleUSBXHCI::RHResumePortTimer(UInt32 port)
{
	// we are responsible for terminating the resume on a root hub port ourselves
	// and we used to do it inside of the workloop. now we do the timing part of it
	// outside of the WL
	if (!_commandGate)
		return;
	
	USBLog(5, "AppleUSBXHCI[%p]::RHResumePortTimer - timing the resume for port %d", this, (int)port);
	IOSleep(20);								// wait 20 ms for the resume to complete
	USBLog(6, "AppleUSBXHCI[%p]::RHResumePortTimer - Host controller resume about to finish - calling EnsureUsability", this);
	EnsureUsability();		
	_commandGate->runAction(RHResumePortCompletionEntry, (void*)port);
}


IOReturn
AppleUSBXHCI::RHResumePortCompletionEntry(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
#pragma unused(param2)
#pragma unused(param3)
#pragma unused(param4)
    AppleUSBXHCI				*me = OSDynamicCast(AppleUSBXHCI, target);
    UInt32						port = (UInt32)(uintptr_t)param1;	// GCC doesn't like each cast individually, it wants both
	
	if (!me)
		return kIOReturnInternalError;
	
	return me->RHResumePortCompletion(port);
}


//================================================================================================
//
//   RHCompleteResumeOnAllPorts
//
//	 This is not a general purpose routine.  It is only called from waking up a controller from Doze.
//   We want to complete any pending resume on all ports at the same time, in order to streamline
//   the wakeup time.
//
//================================================================================================
//
IOReturn			
AppleUSBXHCI::RHCompleteResumeOnAllPorts()
{
	UInt32			value;
	int				i;
	UInt32			portIndex;
	IOReturn		returnValue = kIOReturnSuccess;
	UInt32			waitTime = 0;
	
	USBLog(6, "AppleUSBXHCI[%p]::RHCompleteResumeOnAllPorts", this);
	
	if (_lostRegisterAccess || !_controllerAvailable)
	{
		for (portIndex = 0; portIndex < _rootHubNumPorts; portIndex++)
		{
			USBLog(5, "AppleUSBXHCI[%p]::RHCompleteResumeOnAllPorts - cannot finish resume on port %d because the controller is unavailable", this, (int)portIndex+1);
			if ( _rhPortBeingResumed[portIndex] )
				_rhPortBeingResumed[portIndex] = false;
		}
		return kIOReturnInternalError;
	}
	
	for (portIndex = 0; portIndex < _rootHubNumPorts; portIndex++)
	{
		UInt32	port = portIndex + 1;
		
		if ( _rhPortBeingResumed[portIndex] )
		{
			USBLog(5, "AppleUSBXHCI[%p]::RHCompleteResumeOnAllPorts - resuming port %d", this, (int)port);
			
			value = GetPortSCForWriting(port);
			if (_lostRegisterAccess)
			{
				return kIOReturnNoDevice;
			}
			
			value |= (UInt32)(kXHCIPortSC_LWS | (kXHCIPortSC_PLS_U0 << kXHCIPortSC_LinkState_Shift));
			Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, value);
			IOSync();
			waitTime = 2;
		}
	}
	
	// Now allow the right to the ports to kick in
	if ( waitTime )
		IOSleep(waitTime);
	
	// Finally mark the ports as not being resumed anymore
	for (portIndex = 0; portIndex < _rootHubNumPorts; portIndex++)
	{
		if ( _rhPortBeingResumed[portIndex] )
		{
			_rhPortBeingResumed[portIndex] = false;
			_suspendChangeBits[portIndex] = true;
		}
	}
	
	return kIOReturnSuccess;
}


IOReturn			
AppleUSBXHCI::RHResumePortCompletion(UInt32 port)
{
#if 1
	UInt32			value;
	
	USBLog(5, "AppleUSBXHCI[%p]::RHResumePortCompletion - finishing resume on port %d", this, (int)port);
	if (!_rhPortBeingResumed[port-1])
	{
		USBLog(1, "AppleUSBXHCI[%p]::RHResumePortCompletion - port %d does not appear to be resuming!", this, (int)port);
		return kIOReturnInternalError;
	}
	
	if (_lostRegisterAccess || !_controllerAvailable)
	{
		USBLog(5, "AppleUSBXHCI[%p]::RHResumePortCompletion - cannot finish resume on port %d because the controller is unavailable", this, (int)port);
		_rhPortBeingResumed[port-1] = false;
		return kIOReturnNoDevice;
	}
	
//	PrintRuntimeRegs();
	
	value = GetPortSCForWriting(port);
	if (_lostRegisterAccess)
	{
		return kIOReturnNoDevice;
	}
	value |= (UInt32)(kXHCIPortSC_LWS | (kXHCIPortSC_PLS_U0 << kXHCIPortSC_LinkState_Shift));

//	USBLog(5, "AppleUSBXHCI[%p]::RHResumePortCompletion - writing %x to portreg", this, (int)value);

	Write32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC, value);
	IOSync();
	IOSleep(2);																	// allow it to kick in
	PrintRuntimeRegs();
	
	_rhPortBeingResumed[port-1] = false;
	_suspendChangeBits[port-1] = true;
	
	// Look to see if we have any pending changes while we still hold the gate:
	CheckForRootHubChanges();
#endif	
	
	return kIOReturnSuccess;
}


//================================================================================================
//
//   RHCheckForPortResumes
//
//   Check to see if any of the Ports have a resume pending and if so, spawn a thread to time the 
//   required 20ms.  That way we don't block the workloop while waiting.
//
//================================================================================================
//
void
AppleUSBXHCI::RHCheckForPortResumes()
{
    int				i;
	UInt32			port, PLS;
	
    if(_lostRegisterAccess)
    {
        return;
    }
	
	for (port = 0; port < _rootHubNumPorts; port++)
	{
		if (!_rhPortBeingResumed[port])
		{
			UInt32		portSC = Read32Reg(&_pXHCIRegisters->PortReg[port].PortSC);
			if (_lostRegisterAccess)
			{
				// our controller appears to have gone away
				return;
			}
			
			PLS = (UInt32)(portSC & kXHCIPortSC_LinkState_Mask) >> kXHCIPortSC_LinkState_Shift;
	
			if (PLS == kXHCIPortSC_PLS_Resume)
			{
				USBLog(5, "AppleUSBXHCI[%p]::RHCheckForPortResumes - port %d appears to be resuming from a remote wakeup - spawning thread to resume", this, (int)port+1);
				_rhPortBeingResumed[port] = true;
				if ( _rhResumePortTimerThread[port] == NULL )
				{
					USBLog(1,"AppleUSBXHCI[%p]::RHCheckForPortResumes  port %d appears to be resuming from a remote wakeup, but the thread callout is NULL!", this, (uint32_t)port+1);
				}
				else
					thread_call_enter1(_rhResumePortTimerThread[port], (void*)(port+1));
			}
		}
	}
	
}


#pragma mark Utility Functions


//================================================================================================
//
//	GetCompanionRootPort
//
// 	Let's say there are 8 RH ports - 1 2 3 4 5 6 7 8
//          SS Simulation - 5 6 7 8 
//			USB2.0 Simulation - 1 2 3 4 
//
//	_rootHubPortsSSStartRange is 5  
// 	_rootHubPortsHSStartRange is 1
// 
// 	ex:
// 	Port is already adjusted before this function is called.
// 	So port can be only between 1 - 4
// 
// 	If adjustedPort = 1 and RH speed is USB2.0, then to find out 
// 	companion port we just use _rootHubPortsSSStartRange-1 + port
// 	which is 5
//
//================================================================================================
//
UInt16 
AppleUSBXHCI::GetCompanionRootPort(UInt8 speed, UInt16 adjustedPort)
{
    UInt16 companionRootPort = 0;
    
	if( speed == kUSBDeviceSpeedHigh )
	{
		companionRootPort = (_v3ExpansionData->_rootHubPortsSSStartRange-1) + adjustedPort;
	}
	else if( speed == kUSBDeviceSpeedSuper )
	{
		companionRootPort = (_v3ExpansionData->_rootHubPortsHSStartRange-1) + adjustedPort;
	}
    
    return companionRootPort;
}


void 
AppleUSBXHCI::AdjustRootHubPortNumbers(UInt8 speed, UInt16 *port)
{
	if( speed == kUSBDeviceSpeedHigh )
	{
		*port += _v3ExpansionData->_rootHubPortsHSStartRange-1;
	}
	else if( speed == kUSBDeviceSpeedSuper )
	{
		*port += _v3ExpansionData->_rootHubPortsSSStartRange-1;
	}
}


bool 
AppleUSBXHCI::IsRootHubPortSuperSpeed(UInt16 *port)
{
    bool    isSuperSpeed = false;
    
    if ( _v3ExpansionData->_rootHubPortsSSStartRange > _v3ExpansionData->_rootHubPortsHSStartRange )
    {
        // SS Range starts AFTER HS Range
        if ( *port >= _v3ExpansionData->_rootHubPortsSSStartRange)
        {
            // Indicate that the port is SS and adjust the port #
            isSuperSpeed = true;
            *port = *port - _v3ExpansionData->_rootHubPortsSSStartRange + 1;
        }
    }
    else
    {
        // SS Range is BEFORE HS Range
        if ( *port < _v3ExpansionData->_rootHubPortsHSStartRange)
        {
            isSuperSpeed = true;
        }
    }

    return isSuperSpeed;
}

UInt32 
AppleUSBXHCI::GetPortSCForWriting(short port)
{
	UInt32 portSC;
	
	portSC = Read32Reg(&_pXHCIRegisters->PortReg[port-1].PortSC);
	if (_lostRegisterAccess)
	{
		return portSC;
	}
	
	portSC &= ~(UInt32)kXHCIPortSC_Write_Zeros;
	
	return(portSC);
}
