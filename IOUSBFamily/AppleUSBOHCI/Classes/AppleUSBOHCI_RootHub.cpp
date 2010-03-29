/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
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

#include "AppleUSBOHCI.h"

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
#define super IOUSBControllerV3
#define _rootHubTransactionWasAborted	_v3ExpansionData->_rootHubTransactionWasAborted

#ifndef APPLEOHCIROOTHUB_USE_KPRINTF
	#define APPLEOHCIROOTHUB_USE_KPRINTF 0
#endif

#if APPLEOHCIROOTHUB_USE_KPRINTF
	#undef USBLog
	#undef USBError
	void kprintf(const char *format, ...)
	__attribute__((format(printf, 1, 2)));
	#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= APPLEOHCIROOTHUB_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
	#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif


// ========================================================================
#pragma mark Public root hub methods
// ========================================================================


IOReturn AppleUSBOHCI::GetRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc)
{
    IOUSBDeviceDescriptor newDesc =
    {
        sizeof(IOUSBDeviceDescriptor),			// UInt8 length;
        kUSBDeviceDesc,							// UInt8 descType;
        HostToUSBWord(kUSBRel20),				// UInt16 usbRel;
        kUSBHubClass,							// UInt8 class;
        kUSBHubSubClass,						// UInt8 subClass;
        0,										// UInt8 protocol;
        8,										// UInt8 maxPacketSize;
        HostToUSBWord(kAppleVendorID),			// UInt16 vendor:  Use the Apple Vendor ID from USB-IF
        HostToUSBWord(kPrdRootHubApple),		// UInt16 product:  All our root hubs are the same
        HostToUSBWord(0x0200),					// UInt16 bcdDevice
        2,										// UInt8 manuIdx;
        1,										// UInt8 prodIdx;
        0,										// UInt8 serialIdx;
        1										// UInt8 numConf;
    };

    if (!desc)
        return(kIOReturnNoMemory);


    bcopy(&newDesc, desc, newDesc.bLength);

    return(kIOReturnSuccess);
}

IOReturn AppleUSBOHCI::GetRootHubDescriptor(IOUSBHubDescriptor *desc)
{
    IOUSBHubDescriptor	hubDesc;
    UInt8				pps, nps, cd, ppoc, noc;
    UInt32				descriptorA, descriptorB, data;
    UInt8 *				dstP;
    unsigned int		i, numBytes;
    UInt32				appleCaptive = 0;
	OSNumber *			appleCaptiveProperty = NULL;

    descriptorA = USBToHostLong(_pOHCIRegisters->hcRhDescriptorA);
    descriptorB = USBToHostLong(_pOHCIRegisters->hcRhDescriptorB);

    hubDesc.length = sizeof(IOUSBHubDescriptor);
    hubDesc.hubType = kUSBHubDescriptorType;
    hubDesc.numPorts = ((descriptorA & kOHCIHcRhDescriptorA_NDP) >> kOHCIHcRhDescriptorA_NDPPhase);
	_rootHubNumPorts = hubDesc.numPorts;
	
    // Characteristics
    pps  = (descriptorA & kOHCIHcRhDescriptorA_PSM) ? 1 : 0;
    nps  = (descriptorA & kOHCIHcRhDescriptorA_NPS) ? 1 : 0;
    cd   = (descriptorA & kOHCIHcRhDescriptorA_DT) ? 1 : 0;
    ppoc = (descriptorA & kOHCIHcRhDescriptorA_OCPM) ? 1 : 0;
    noc  = (descriptorA & kOHCIHcRhDescriptorA_NOCP) ? 1 : 0;
    
    hubDesc.characteristics = 0;
    hubDesc.characteristics |= (pps  ? kPerPortSwitchingBit   : 0);
    hubDesc.characteristics |= (nps  ? kNoPowerSwitchingBit   : 0);
    hubDesc.characteristics |= (cd   ? kCompoundDeviceBit     : 0);
    hubDesc.characteristics |= (ppoc ? kPerPortOverCurrentBit : 0);
    hubDesc.characteristics |= (noc  ? kNoOverCurrentBit      : 0);
    hubDesc.characteristics = HostToUSBWord(hubDesc.characteristics);
    
    hubDesc.powerOnToGood = ((descriptorA & kOHCIHcRhDescriptorA_POTPGT)
                               >> kOHCIHcRhDescriptorA_POTPGTPhase);
    // the root hub has no power requirements
    hubDesc.hubCurrent = 0;

    // Bitmap data is packed according to the number
    // of ports on the hub, and is stored little-endian.
    numBytes = (hubDesc.numPorts + 1) / 8 + 1;
    dstP = (UInt8 *)&hubDesc.removablePortFlags[0];
        
    // bitmap of removable port flags.  If we have the apple property, use it,
	// otherwise use the OHCI register
	appleCaptiveProperty = OSDynamicCast(OSNumber, _device->getProperty(kAppleInternalUSBDevice));
	if (appleCaptiveProperty)
		appleCaptive = appleCaptiveProperty->unsigned32BitValue();
	else
		appleCaptive = (UInt32) (descriptorB & kOHCIHcRhDescriptorB_DR) >> kOHCIHcRhDescriptorB_DRPhase;
	
    for (i = 0; i < numBytes; i++) 
	{
        *dstP++ = (UInt8) (appleCaptive & 0xFF);
        appleCaptive >>= 8;
    }
        
    // bitmap of power control flags
    data = (descriptorB & kOHCIHcRhDescriptorB_PPCM) >> kOHCIHcRhDescriptorB_PPCMPhase;
    for (i=0; i<numBytes; i++) 
	{
        *dstP++ = (UInt8)data;
        data >>= 8;
    }
    
    // Adjust descriptor length to account for
    // number of bytes used in removable and power control arrays.
    hubDesc.length -= ((sizeof(hubDesc.removablePortFlags) - numBytes) +
                       (sizeof(hubDesc.pwrCtlPortFlags) - numBytes));
    
    if (!desc)
        return(kIOReturnNoMemory);

    bcopy(&hubDesc, desc, hubDesc.length);

    return(kIOReturnSuccess);
}



IOReturn AppleUSBOHCI::GetRootHubConfDescriptor(OSData *desc)
{
    IOUSBConfigurationDescriptor confDesc =
    {
        sizeof(IOUSBConfigurationDescriptor),							//UInt8 length;
        kUSBConfDesc,													//UInt8 descriptorType;
        HostToUSBWord(sizeof(IOUSBConfigurationDescriptor) +
                       sizeof(IOUSBInterfaceDescriptor) +
                       sizeof(IOUSBEndpointDescriptor)),				//UInt16 totalLength;
        1,																//UInt8 numInterfaces;
        1,																//UInt8 configValue;
        0,																//UInt8 configStrIndex;
        0x60,															//UInt8 attributes; self powered,
																		//      supports remote wkup
        0,																//UInt8 maxPower;
    };
    IOUSBInterfaceDescriptor intfDesc =
    {
        sizeof(IOUSBInterfaceDescriptor),								//UInt8 length;
        kUSBInterfaceDesc,												//UInt8 descriptorType;
        0,																//UInt8 interfaceNumber;
        0,																//UInt8 alternateSetting;
        1,																//UInt8 numEndpoints;
        kUSBHubClass,													//UInt8 interfaceClass;
        kUSBHubSubClass,												//UInt8 interfaceSubClass;
        0,																//UInt8 interfaceProtocol;
        0																//UInt8 interfaceStrIndex;
    };
    IOUSBEndpointDescriptor endptDesc =
    {
        sizeof(IOUSBEndpointDescriptor),								//UInt8 length;
        kUSBEndpointDesc,												//UInt8 descriptorType;
        0x81,															//UInt8  endpointAddress; In, 1
        kUSBInterrupt,													//UInt8 attributes;
        HostToUSBWord(8),												//UInt16 maxPacketSize;
        kUSBRootHubPollingRate,											//UInt8 interval;
    };

    if (!desc)
        return(kIOReturnNoMemory);

    if (!desc->appendBytes(&confDesc,  confDesc.bLength))
        return(kIOReturnNoMemory);

    if (!desc->appendBytes(&intfDesc,  intfDesc.bLength))
        return(kIOReturnNoMemory);

    if (!desc->appendBytes(&endptDesc, endptDesc.bLength))
        return(kIOReturnNoMemory);

    return(kIOReturnSuccess);
}

IOReturn AppleUSBOHCI::SetRootHubDescriptor(OSData * buffer)
{
#pragma unused (buffer)
    return(kIOReturnSuccess);
}

IOReturn AppleUSBOHCI::GetRootHubStatus(IOUSBHubStatus *status)
{
    if ( _controllerAvailable )
		*(UInt32*)status = _pOHCIRegisters->hcRhStatus;
	else
		*(UInt32*)status = 0;					// if the controller is not available, just return 0. The OC bit is the only really important bit anyway

    return kIOReturnSuccess;
}



IOReturn 
AppleUSBOHCI::SetRootHubFeature(UInt16 wValue)
{
    switch(wValue)
    {
        case kUSBHubLocalPowerChangeFeature :
            USBLog(3,"AppleUSBOHCI[%p]::SetRootHubFeature - unimplemented Set Power Change Feature", this);
            // OHCIRootHubLPSChange(true);						// not implemented yet
            break;

        case kUSBHubOverCurrentChangeFeature :
            USBLog(3,"AppleUSBOHCI[%p]::SetRootHubFeature - unimplemented Set Overcurrent Change Feature", this);
            // OHCIRootHubOCChange(true);						// not implemented yet
            break;

        default:
            USBLog(3,"AppleUSBOHCI[%p]::SetRootHubFeature - Unknown feature (%d)", this, wValue);
            break;
    }

    return(kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::ClearRootHubFeature(UInt16 wValue)
{
    switch(wValue)
    {
        case kUSBHubLocalPowerChangeFeature :
            USBLog(3, "AppleUSBOHCI[%p]::ClearRootHubFeature - unimplemented Clear Power Change Feature", this);
            // OHCIRootHubLPSChange(false);  // not implemented yet
            break;

        case kUSBHubOverCurrentChangeFeature :
            USBLog(3, "AppleUSBOHCI[%p]::ClearRootHubFeature - writing to clear the OCIC bit", this);
			_pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_OCIC);			// clear the OC indicator change
			IOSync();		
            break;

        default:
            USBLog(3, "AppleUSBOHCI[%p]::ClearRootHubFeature - Unknown feature (%d)", this, wValue);
            break;
    }

    return(kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::GetRootHubPortStatus(IOUSBHubPortStatus *status, UInt16 port)
{
    if ( _myBusState == kUSBBusStateSuspended )
        return kIOReturnNotResponding;

    if (port < 1 || port > 15)
        return(kIOReturnBadArgument);
    
    *(UInt32*)status = _pOHCIRegisters->hcRhPortStatus[port-1];
    
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBOHCI::SetRootHubPortFeature(UInt16 wValue, UInt16 wIndex)
{
    // in the code below port is a port INDEX (0 based), whereas wIndex is the port NUMBER (1 based)
    //
    UInt16	port = wIndex-1;
    IOReturn	err = kIOReturnSuccess;
    
    switch(wValue)
    {
        case kUSBHubPortSuspendFeature :
            // to be consistent with OS 9, the bit we check below needs to be the port NUMBER
	    if ( (_errataBits & kErrataLucentSuspendResume) && ((_disablePortsBitmap & (1 << wIndex)) != 0))
	    {
			USBLog(3,"AppleUSBOHCI[%p]::SetRootHubPortFeature - Ignoring suspend feature", this);
			err = kIOUSBPipeStalled;
	    }
	    else
			OHCIRootHubPortSuspend(port, true);
            break;

        case kUSBHubPortResetFeature :
            OHCIRootHubResetPort(port);
            break;

        case kUSBHubPortEnableFeature :
            OHCIRootHubPortEnable(port, true);
            break;

        case kUSBHubPortPowerFeature :
            OHCIRootHubPortPower(port, true);
            OHCIRootHubPower(true);
			showRegisters(7, "SetRootHubPortFeature");
            break;

        default:
            USBLog(3,"AppleUSBOHCI[%p]::SetRootHubPortFeature - Unknown feature (%d)", this, wValue);
            break;
    }
    
    return err;
}



IOReturn 
AppleUSBOHCI::ClearRootHubPortFeature(UInt16 wValue, UInt16 wIndex)
{
    UInt16	port = wIndex-1;
	bool	renableRHSCInterrupt = false;

    switch(wValue)
    {
        case kUSBHubPortEnableFeature :
            OHCIRootHubPortEnable(port, false);
            break;

        case kUSBHubPortSuspendFeature :
            OHCIRootHubPortSuspend(port, false);
            break;

        case kUSBHubPortPowerFeature :
            OHCIRootHubPortPower(port, false);
            // Now need to check if all ports are switched off and
            // gang off if in gang mode
            break;

        // ****** Change features *******
        case kUSBHubPortConnectionChangeFeature :
            OHCIRootHubResetChangeConnection(port);
			renableRHSCInterrupt = true;
            break;

        case kUSBHubPortEnableChangeFeature :
            OHCIRootHubResetEnableChange(port);
 			renableRHSCInterrupt = true;
           break;

        case kUSBHubPortSuspendChangeFeature :
            OHCIRootHubResetSuspendChange(port);
			renableRHSCInterrupt = true;
            break;

        case kUSBHubPortOverCurrentChangeFeature :
            OHCIRootHubResetOverCurrentChange(port);
			renableRHSCInterrupt = true;
            break;

        case kUSBHubPortResetChangeFeature :
            OHCIRootHubResetResetChange(port);
 			renableRHSCInterrupt = true;
           break;

        default:
            USBLog(3,"AppleUSBOHCI[%p]::ClearRootHubPortFeature - Unknown feature (%d)", this, wValue);
            break;
    }
	
	if ( renableRHSCInterrupt && _needToReEnableRHSCInterrupt)
	{
		UInt32	interrupts;
		
		USBLog(3,"AppleUSBOHCI[%p]::ClearRootHubPortFeature - renabling RHSC interrupt due to clear feature %d", this, wValue);

		// ReEnable the interrupt
		_needToReEnableRHSCInterrupt = false;
		_pOHCIRegisters->hcInterruptEnable = HostToUSBLong (kOHCIHcInterrupt_MIE | kOHCIHcInterrupt_RHSC);
		IOSync();
	}
	
    return(kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::GetRootHubPortState(UInt8 *state, UInt16 port)
{
#pragma unused(state, port)
    USBLog(3,"AppleUSBOHCI[%p]::GetRootHubPortState - UNIMPLEMENTED", this);
    return(kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::SetHubAddress(UInt16 wValue)
{
    USBLog(3,"AppleUSBOHCI[%p]::SetHubAddress - Setting RootHub Address to %d", this, wValue);
    _rootHubFuncAddress = wValue;
    return (kIOReturnSuccess);
}

IOReturn 
AppleUSBOHCI::GetRootHubStringDescriptor(UInt8	index, OSData *desc)
{
	
    // The following strings are in Unicode format
    //
    UInt8 productName[] = {
		0,			// Length Byte
		kUSBStringDesc,	// Descriptor type
		0x4F, 0x00, // "O"
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
    if ( index > 2 )
        return kIOReturnBadArgument;
	
    // Set the length of our strings
    //
    vendorName[0] = sizeof(vendorName);
    productName[0] = sizeof(productName);
    
    if ( index == 1 )
    {
        if (!desc)
            return kIOReturnNoMemory;
		
        if (!desc->appendBytes(&productName,  productName[0]))
            return kIOReturnNoMemory;
    }
    
    if ( index == 2 )
    {
        if (!desc)
            return kIOReturnNoMemory;
		
        if (!desc->appendBytes(&vendorName,  vendorName[0]))
            return kIOReturnNoMemory;
    }
    
    return kIOReturnSuccess;
}

// obsolete method
void
AppleUSBOHCI::UIMRootHubStatusChange( bool abort )
{
	USBLog(1, "AppleUSBOHCI[%p]::UIMRootHubStatusChange - calling obsolete method UIMRootHubStatusChange(bool)", this);
}



/*
 * UIMRootHubStatusChange
 *
 * This method gets called during a 32 ms timer task to see if there is anything that needs to be reported
 * to the root hub driver.
 */
void 
AppleUSBOHCI::UIMRootHubStatusChange(void)
{
    UInt32 									tempStatus = 0xFFFF, hubStatus, portStatus, statusBit;
    UInt16 									statusChangedBitmap;							// local copy of status changed bitmap
    unsigned int      						index, port, move;
    UInt32									descriptorA;
    
    /*
     * Encode the status change bitmap.  The format of the bitmap:
     * bit0 = hub status changed
     * bit1 = port 1 status changed
     * bit2 = port 2 status changed
     * ...
     * See USB 1.0 spec section 11.8.3 for more info.
     */
	
    statusChangedBitmap = 0;
    statusBit = 1;								// really bit 0 (which is the hub bit)
	
    if ( _controllerAvailable && !_wakingFromHibernation && (GetRootHubStatus((IOUSBHubStatus *)&tempStatus) == kIOReturnSuccess) )
    {
        // GetRootHubStatus does NOT swap bits, so we need to do it here
        //
		hubStatus = USBToHostLong( tempStatus );
   		//USBLog(6, "AppleUSBOHCI[%p]::UIMRootHubStatusChange - RHStatus: %p", this, (void*) hubStatus);
		if ((hubStatus & kOHCIHcRhStatus_Change ) != 0)
        {
            statusChangedBitmap |= statusBit;
        }
		
		
        for (port = 1; port <= _rootHubNumPorts; port++)
        {
            statusBit <<= 1;    // Next bit
			
            GetRootHubPortStatus((IOUSBHubPortStatus *)&tempStatus, port);
			
            // GetRootHubPortStatus does NOT swap bits, so we need to do it here
            //
			portStatus = USBToHostLong( tempStatus);
  			//USBLog(6, "AppleUSBOHCI[%p]::UIMRootHubStatusChange - Port(%d) Status: %p", this, port, (void*) portStatus);
			if ((portStatus & kOHCIHcRhPortStatus_Change) != 0)
            {
                statusChangedBitmap |= statusBit;
            }
        }
    }
	else
	{
		USBLog(6, "AppleUSBOHCI[%p]::UIMRootHubStatusChange - didn't go into checking for bits", this);
	}
	
	if (statusChangedBitmap)
	{
		USBLog(5, "AppleUSBOHCI[%p]::UIMRootHubStatusChange - reporting status of %p", this, (void*)statusChangedBitmap);
	}
	
    _rootHubStatusChangedBitmap = statusChangedBitmap;
}

// ========================================================================
#pragma mark Internal root hub methods
// ========================================================================

void 
AppleUSBOHCI::OHCIRootHubPower(bool on)
{

    USBLog(2,"AppleUSBOHCI[%p]::OHCIRootHubPower (%s)", this, on ? "true" : "false");
    if (on)
    {
		// when writing, LPSC is the SetGlobalPower bit
		_pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_LPSC);			// turn on global power
    }
    else
    {
		// when writing, LPS is the ClearGlobalPower bit
		_pOHCIRegisters->hcRhStatus = HostToUSBLong(kOHCIHcRhStatus_LPS);			// turn off global power
    }
    IOSync();
    return;
}



void 
AppleUSBOHCI::OHCIRootHubResetChangeConnection(UInt16 port)
{
    UInt32		value = 0;
    
    value |= kOHCIHcRhPortStatus_CSC;												// clear connect status change

    _pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
    IOSync();

	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);			// this bit SHOULD now be cleared
		while ((count++ < 10) && (newValue & kOHCIHcRhPortStatus_CSC))
		{
			USBError(1, "OHCI driver: OHCIRootHubResetChangeConnection bit not sticking. Retrying.");
			_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		}
	}

    return;
}


void 
AppleUSBOHCI::OHCIRootHubResetResetChange(UInt16 port)
{
    UInt32		value = 0;
    
    value |= kOHCIHcRhPortStatus_PRSC;											// clear reset status change

    _pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
    IOSync();

	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);			// this bit SHOULD now be cleared
		while ((count++ < 10) && (newValue & kOHCIHcRhPortStatus_PRSC))
		{
			USBError(1, "OHCI driver: OHCIRootHubResetResetChange bit not sticking. Retrying.");
			_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		}
	}

    return;
}



void 
AppleUSBOHCI::OHCIRootHubResetSuspendChange(UInt16 port)
{
    UInt32		value = 0;

    value |= kOHCIHcRhPortStatus_PSSC;											// clear suspend status change

    _pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
    IOSync();

	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);			// this bit SHOULD now be cleared
		while ((count++ < 10) && (newValue & kOHCIHcRhPortStatus_PSSC))
		{
			USBError(1, "OHCI driver: OHCIRootHubResetSuspendChange bit not sticking. Retrying.");
			_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		}
	}

    return;
}



void 
AppleUSBOHCI::OHCIRootHubResetEnableChange(UInt16 port)
{
    UInt32		value = 0;

    value |= kOHCIHcRhPortStatus_PESC;								// clear enable status change

    _pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
    IOSync();

	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);			// this bit SHOULD now be cleared
		while ((count++ < 10) && (newValue & kOHCIHcRhPortStatus_PESC))
		{
			USBError(1, "OHCI driver: OHCIRootHubResetEnableChange bit not sticking. Retrying.");
			_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		}
	}

    return;
}

    

void 
AppleUSBOHCI::OHCIRootHubResetOverCurrentChange(UInt16 port)
{
    UInt32		value = 0;

    value |= kOHCIHcRhPortStatus_OCIC;											// clear over current status change

    _pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
    IOSync();

	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);			// this bit SHOULD now be cleared
		while ((count++ < 10) && (newValue & kOHCIHcRhPortStatus_OCIC))
		{
			USBError(1, "OHCI driver: OHCIRootHubResetOverCurrentChange bit not sticking. Retrying.");
			_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		}
	}

    return;
}


// 05-11-05 JRH
// We found a bug where sometimes the OHCI controller will "miss" the reset  bit when we set it
// So now we will re-read the bit and re-set it if it does not remain set.
// Q - should we also do this for other bits in this register? A - yes, according to an NEC errata which came Jun 23 2005
void 
AppleUSBOHCI::OHCIRootHubResetPort (UInt16 port)
{
	UInt32		value = 0;
	
    value = kOHCIHcRhPortStatus_PRS;												// sets Bit 8 in port root hub register
    _pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(kOHCIHcRhPortStatus_PRS);
    IOSync();
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		count = 0;
		value = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		while ((count++ < 10) && !(value & kOHCIHcRhPortStatus_PRS))
		{
			USBError(1, "OHCI driver: SetPortReset bit not sticking. Retrying.");
			value = kOHCIHcRhPortStatus_PRS;
			_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
			IOSync();
			value = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		}
	}
    return;
}



void 
AppleUSBOHCI::OHCIRootHubPortEnable(UInt16	port, bool	on)
{
    UInt32		value = 0;

    if (on)
        value |= kOHCIHcRhPortStatus_PES;					// enable port
    else
        value |= kOHCIHcRhPortStatus_CCS;					// disable port

    _pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
    IOSync();

	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		while ((count++ < 10) && (on ? !(newValue & kOHCIHcRhPortStatus_PES) : (newValue & kOHCIHcRhPortStatus_PES)))
		{
			USBError(1, "OHCI driver: OHCIRootHubPortEnable bit not sticking (%d). Retrying.", on);
			_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		}
	}

    return;
}



void 
AppleUSBOHCI::OHCIRootHubPortSuspend(UInt16 port, bool	on)
{
    UInt32		value = 0;

    if (on)
        value |= kOHCIHcRhPortStatus_PSS;									// suspend port
    else
        value |= kOHCIHcRhPortStatus_POCI;									// resume port

    _pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
    IOSync();

	// we can only check to see that we went into suspend state, not that we went into resume
	if ((_errataBits & kErrataNECIncompleteWrite) && on)
	{
		UInt32		newValue = 0, count = 0;
		IOSleep(1);									// wait for the write to settle
		newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		while ((count++ < 10) && !(newValue & kOHCIHcRhPortStatus_PSS))
		{
			USBError(1, "OHCI driver: OHCIRootHubPortSuspend bit not sticking (on). Retrying.");
			_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
			IOSync();
			IOSleep(count);
			newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		}
	}

    return;
}



void 
AppleUSBOHCI::OHCIRootHubPortPower(UInt16	port, bool	on)
{
    UInt32		value = 0;

    USBLog(5,"AppleUSBOHCI[%p]::OHCIRootHubPortPower(%s) for port %d", this, on ? "on" : "off", port);
    
    if (on)
        value |= kOHCIHcRhPortStatus_PPS;							// enable port power 
    else
        value |= kOHCIHcRhPortStatus_LSDA;							// disable port power

    _pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
    IOSync();

	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32		newValue = 0, count = 0;
		newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		while ((count++ < 10) && (on ? !(newValue & kOHCIHcRhPortStatus_PPS) : (newValue & kOHCIHcRhPortStatus_PPS)))
		{
			USBError(1, "OHCI driver: OHCIRootHubPortPower bit not sticking (%d). Retrying.", on);
			_pOHCIRegisters->hcRhPortStatus[port] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pOHCIRegisters->hcRhPortStatus[port]);
		}
	}

    return;
}


IOReturn 
AppleUSBOHCI::SimulateControlEDCreate (UInt16 maxPacketSize)
{
    return kIOReturnSuccess;
}

IOReturn 
AppleUSBOHCI::SimulateEDDelete (short endpointNumber, short direction)
{
    return SimulateEDAbort(endpointNumber, direction);
}



IOReturn 
AppleUSBOHCI::SimulateEDAbort (short endpointNumber, short direction)
{
    int			i;
    if (endpointNumber == 1)
    {
		if (direction != kUSBIn)
		{
			USBLog(3, "AppleUSBOHCI[%p]::SimulateEDAbort - Root hub wrong direction Int pipe %d", this, direction);
			return kIOReturnInternalError;
		}
		USBLog(5, "AppleUSBOHCI[%p]::SimulateEDAbort Root hub aborting int transactions", this);
		RootHubAbortInterruptRead();
    }
    else
    {
		USBLog(5, "AppleUSBOHCI[%p]::SimulateEDAbort Root hub aborting control pipe (NOP)", this);
    }

    return kIOReturnSuccess;
}



IOReturn 
AppleUSBOHCI::SimulateEDClearStall (short endpointNumber, short direction)
{
    return kIOReturnSuccess;
}


AbsoluteTime	
AppleUSBOHCI::LastRootHubPortStatusChanged( bool resetTime )
{
	uint64_t		currentTime;
	
    if ( resetTime )
	{
		currentTime = mach_absolute_time();
		_lastRootHubStatusChanged = *(AbsoluteTime*) &currentTime;
	}
	
    return _lastRootHubStatusChanged;
}

// ========================================================================
#pragma mark Used binary compatibilty slots
// ========================================================================


OSMetaClassDefineReservedUsed(IOUSBController,  14);


