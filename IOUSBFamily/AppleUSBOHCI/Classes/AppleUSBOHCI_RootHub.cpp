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

extern "C" {
#include <kern/clock.h>
}

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>
#include <libkern/OSByteOrder.h>

#include "AppleUSBOHCI.h"

/* Convert USBLog to use kprintf debugging */
#ifndef APPLEOHCIROOTHUB_USE_KPRINTF
	#define APPLEOHCIROOTHUB_USE_KPRINTF 0
#endif

#if APPLEOHCIROOTHUB_USE_KPRINTF
	#undef USBLog
	#undef USBError
	void kprintf(const char *format, ...)
	__attribute__((format(printf, 1, 2)));
	#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= 5) { kprintf( FORMAT "\n", ## ARGS ) ; }
	#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme

#define super IOUSBController


enum {
	kAppleVendorID		= 0x05AC,	/* Assigned by USB-if*/
	kPrdRootHubApple	= 0x8005,	/* Apple ASIC root hub*/
	kOHCIRootHubPollingInterval = 32	// Polling interval in ms
};

/*
 * Root hub methods
 */
// FIXME  Should this routine go in the device?
IOReturn AppleUSBOHCI::GetRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc)
{
    IOUSBDeviceDescriptor newDesc =
    {
        sizeof(IOUSBDeviceDescriptor),	// UInt8 length;
        kUSBDeviceDesc,			// UInt8 descType;
        USB_CONSTANT16(kUSBRel10),	// UInt16 usbRel;
        kUSBHubClass,			// UInt8 class;
        kUSBHubSubClass,		// UInt8 subClass;
        0,				// UInt8 protocol;
        8,				// UInt8 maxPacketSize;
        USB_CONSTANT16(kAppleVendorID),	// UInt16 vendor:  Use the Apple Vendor ID from USB-IF
        USB_CONSTANT16(kPrdRootHubApple),	// UInt16 product:  All our root hubs are the same
        USB_CONSTANT16(0x0190),		// UInt16 bcdDevice
        2,				// UInt8 manuIdx;
        1,				// UInt8 prodIdx;
        0,				// UInt8 serialIdx;
        1				// UInt8 numConf;
    };

    if (!desc)
        return(kIOReturnNoMemory);

    bcopy(&newDesc, desc, newDesc.bLength);

    return(kIOReturnSuccess);
}

IOReturn AppleUSBOHCI::GetRootHubDescriptor(IOUSBHubDescriptor *desc)
{
    IOUSBHubDescriptor hubDesc;
    UInt8 pps, nps, cd, ppoc, noc;
    UInt32 descriptorA, descriptorB, data;
    UInt8 *dstP;
    unsigned int i, numBytes;

    descriptorA = USBToHostLong(_pOHCIRegisters->hcRhDescriptorA);
    descriptorB = USBToHostLong(_pOHCIRegisters->hcRhDescriptorB);

    hubDesc.length = sizeof(IOUSBHubDescriptor);
    hubDesc.hubType = kUSBHubDescriptorType;
    hubDesc.numPorts = ((descriptorA & kOHCIHcRhDescriptorA_NDP)
                          >> kOHCIHcRhDescriptorA_NDPPhase);
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
        
    // bitmap of removable port flags
    data = (descriptorB & kOHCIHcRhDescriptorB_DR) >> kOHCIHcRhDescriptorB_DRPhase;
    for (i=0; i<numBytes; i++) {
        *dstP++ = (UInt8)data;
        data >>= 8;
    }
        
    // bitmap of power control flags
    data = (descriptorB & kOHCIHcRhDescriptorB_PPCM) >> kOHCIHcRhDescriptorB_PPCMPhase;
    for (i=0; i<numBytes; i++) {
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
        sizeof(IOUSBConfigurationDescriptor),//UInt8 length;
        kUSBConfDesc,               //UInt8 descriptorType;
        USB_CONSTANT16(sizeof(IOUSBConfigurationDescriptor) +
                       sizeof(IOUSBInterfaceDescriptor) +
                       sizeof(IOUSBEndpointDescriptor)),   //UInt16 totalLength;
        1,                          //UInt8 numInterfaces;
        1,                          //UInt8 configValue;
        0,                          //UInt8 configStrIndex;
        0x60,                       //UInt8 attributes; self powered,
				    //      supports remote wkup
        0,                          //UInt8 maxPower;
    };
    IOUSBInterfaceDescriptor intfDesc =
    {
        sizeof(IOUSBInterfaceDescriptor),//UInt8 length;
        kUSBInterfaceDesc,      //UInt8 descriptorType;
        0,                      //UInt8 interfaceNumber;
        0,                      //UInt8 alternateSetting;
        1,                      //UInt8 numEndpoints;
        kUSBHubClass,           //UInt8 interfaceClass;
        kUSBHubSubClass,        //UInt8 interfaceSubClass;
        0,                      //UInt8 interfaceProtocol;
        0                       //UInt8 interfaceStrIndex;
    };
    IOUSBEndpointDescriptor endptDesc =
    {
        sizeof(IOUSBEndpointDescriptor),//UInt8 length;
        kUSBEndpointDesc,       //UInt8 descriptorType;
        0x81,                   //UInt8  endpointAddress; In, 1
        kUSBInterrupt,          //UInt8 attributes;
        USB_CONSTANT16(8),      	//UInt16 maxPacketSize;
        255,                    //UInt8 interval;
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

IOReturn AppleUSBOHCI::SetRootHubDescriptor(OSData * /*buffer*/)
{

    return(kIOReturnSuccess);
}

IOReturn AppleUSBOHCI::GetRootHubStatus(IOUSBHubStatus *status)
{
    if ( _ohciBusState == kOHCIBusStateSuspended )
        return kIOReturnNotResponding;

    *(UInt32*)status = _pOHCIRegisters->hcRhStatus;

    return kIOReturnSuccess;
}



IOReturn 
AppleUSBOHCI::SetRootHubFeature(UInt16 wValue)
{
    switch(wValue)
    {
        case kUSBHubLocalPowerChangeFeature :
            USBLog(3,"AppleUSBOHCI[%p]::SetRootHubFeature - unimplemented Set Power Change Feature", this);
            // OHCIRootHubLPSChange(true);  // not implemented yet
            break;

        case kUSBHubOverCurrentChangeFeature :
            USBLog(3,"AppleUSBOHCI[%p]::SetRootHubFeature - unimplemented Set Overcurrent Change Feature", this);
            // OHCIRootHubOCChange(true);  // not implemented yet
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
            USBLog(3, "AppleUSBOHCI[%p]::ClearRootHubFeature - unimplemented Clear Overcurrent Change Feature", this);
            // OHCIRootHubOCChange(false);  // not implemented yet
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
    if ( _ohciBusState == kOHCIBusStateSuspended )
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
	    if( (_errataBits & kErrataLucentSuspendResume) && ((_disablePortsBitmap & (1 << wIndex)) != 0))
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
            break;

        case kUSBHubPortEnableChangeFeature :
            OHCIRootHubResetEnableChange(port);
            break;

        case kUSBHubPortSuspendChangeFeature :
            OHCIRootHubResetSuspendChange(port);
            break;

        case kUSBHubPortOverCurrentChangeFeature :
            OHCIRootHubResetOverCurrentChange(port);
            break;

        case kUSBHubPortResetChangeFeature :
            OHCIRootHubResetResetChange(port);
            break;

        default:
            USBLog(3,"AppleUSBOHCI[%p]::ClearRootHubPortFeature - Unknown feature (%d)", this, wValue);
            break;
    }
    return(kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::GetRootHubPortState(UInt8 */*state*/, UInt16 /*port*/)
{
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



void 
AppleUSBOHCI::OHCIRootHubPower(bool on)
{

    USBLog(5,"AppleUSBOHCI[%p]::SetHubAddress - OHCIRootHubPower (%s)", this, on ? "true" : "false");
    if(on)
    {
		_pOHCIRegisters->hcRhStatus |= HostToUSBLong(kOHCIHcRhStatus_LPSC);			// turn on global power
    }
    else
    {
		_pOHCIRegisters->hcRhStatus |= HostToUSBLong(kOHCIHcRhStatus_LPS);			// turn off global power
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

    if(on)
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

    if(on)
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

    USBLog(5,"AppleUSBOHCI[%p]::OHCIRootHubPortPower for port %d", this, port);
    
    if(on)
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

/*
 * UIMRootHubStatusChange
 *
 * This method gets called when there is a change in the root hub status
 * or a change in the status of one of the ports.  It is assumed that the
 * interrupt will be cleared for us, but we don't clear the change
 * condition, we will get another interrupt.  So turn off the interrupt.
 * Entering this function means that someone turned on the RHSC interrupt
 * and that there is a client waiting in the queue.  The client expects a
 * status change bitmap.
 * To fix: currently there can only be one client in the queue.  The RHSC
 * interrupt should only be turned off if there is no one else in the queue,
 * or, all clients should be responded to with the one interrupt.
 */
void 
AppleUSBOHCI::UIMRootHubStatusChange(void)
{
    UIMRootHubStatusChange(false);
}



OSMetaClassDefineReservedUsed(IOUSBController,  10);
void 
AppleUSBOHCI::UIMRootHubStatusChange(bool abort)
{
    UInt32 		tempStatus, hubStatus, portStatus, statusBit;
    UInt16 		statusChangedBitmap;   /* only have 15 ports in OHCI */
    UInt8		numPorts = 0;
    unsigned int      	index, port, move;
    struct InterruptTransaction last;
    UInt32		descriptorA;
    
    /* turn off RHSC interrupt */
    _pOHCIRegisters->hcInterruptDisable = HostToUSBLong(kOHCIHcInterrupt_RHSC);
    IOSync();

    /*
     * Encode the status change bitmap.  The format of the bitmap:
     * bit0 = hub status changed
     * bit1 = port 1 status changed
     * bit2 = port 2 status changed
     * ...
     * See USB 1.0 spec section 11.8.3 for more info.
     */

    statusChangedBitmap = 0;
    statusBit = 1;

    if (!abort && GetRootHubStatus((IOUSBHubStatus *)&tempStatus) == kIOReturnSuccess)
    {
        // GetRootHubStatus does NOT swap bits, so we need to do it here
        //
        hubStatus = USBToHostLong( tempStatus );
        if ((hubStatus & kOHCIHcRhStatus_Change ) != 0)
        {
            statusChangedBitmap |= statusBit;
        }

        // Find the numPorts for this root hub
        //
        descriptorA = USBToHostLong(_pOHCIRegisters->hcRhDescriptorA);
        numPorts = ((descriptorA & kOHCIHcRhDescriptorA_NDP)
                    >> kOHCIHcRhDescriptorA_NDPPhase);

        for (port = 1; port <= numPorts; port++)
        {
            statusBit <<= 1;    /* Next bit */

            GetRootHubPortStatus((IOUSBHubPortStatus *)&tempStatus, port);

            // GetRootHubPortStatus does NOT swap bits, so we need to do ti here
            //
            portStatus = USBToHostLong( tempStatus);
            if ((portStatus & kOHCIHcRhPortStatus_Change) != 0)
            {
                statusChangedBitmap |= statusBit;
            }
        }
    }
    
    /*
     * If a transaction is queued, handle it
     */
    if ( (abort || (statusChangedBitmap != 0)) && (_outstandingTrans[0].completion.action != NULL) )
    {
        last = _outstandingTrans[0];
        IOTakeLock(_intLock);
        for (index = 1; index < kMaxOutstandingTrans ; index++)
        {
            _outstandingTrans[index-1] = _outstandingTrans[index];
            if (_outstandingTrans[index].completion.action == NULL)
            {
                break;
            }
        }

        // Update the time stamp for a root hub status change
        //
        clock_get_uptime( &_lastRootHubStatusChanged );

        move = last.bufLen;
        if (move > sizeof(statusChangedBitmap))
            move = sizeof(statusChangedBitmap);
        if (numPorts < 8)
            move = 1;

        statusChangedBitmap = HostToUSBWord(statusChangedBitmap);
        last.buf->writeBytes(0,&statusChangedBitmap, move);
        IOUnlock(_intLock); /* Unlock the queue */
        
        Complete(last.completion, (abort ? kIOReturnAborted : kIOReturnSuccess), last.bufLen - move);

    }

    // If we are aborting, we do not need to enable the RHSC.  If there is a transaction pending in the queue,
    // or the RHSC did not involve a status change, we need to enable the RHSC interrupt
    //
    if ( !abort && ((_outstandingTrans[0].completion.action != NULL) || (statusChangedBitmap == 0)) )
    {
        // Turn on RHSC interrupt
        //
        _pOHCIRegisters->hcInterruptEnable = HostToUSBLong(kOHCIHcInterrupt_RHSC);
        IOSync();
    }
}

/*
 * SimulateRootHubInt
 * Simulate the interrupt pipe (status change pipe) of a hub for the root
 * hub.  The basic concept is to simulate the UIMCreateInterruptTransfer
 * so we keep our own little queue -> _outstandingTrans.  We are turning
 * on the RHSC interrupt so UIMRootHubStatusChange() should handle the
 * dequeueing.
 */
void AppleUSBOHCI::SimulateRootHubInt(UInt8							endpoint,
									  IOMemoryDescriptor *			buf,
									  UInt32						bufLen,
									  IOUSBCompletion				completion)
{
    int				index;
	AbsoluteTime	lastRootHubChangeTime, currentTime;
	UInt64			elapsedTime = 0;

    if (endpoint != 1)
    {
        Complete(completion, -1, bufLen);
        return;
    }

    IOTakeLock(_intLock);
    
    for (index = 0; index < kMaxOutstandingTrans; index++)
    {
        if (_outstandingTrans[index].completion.action == NULL)
        {
            /* found free trans */
            _outstandingTrans[index].buf = buf;
            _outstandingTrans[index].bufLen = bufLen;
            _outstandingTrans[index].completion = completion;
            IOUnlock(_intLock); /* Unlock the queue */

			// Calculate how long it's been since the last status change and wait until the pollingRate to call the UIMRootHubStatusChange()
			lastRootHubChangeTime = LastRootHubPortStatusChanged( false );
			
			clock_get_uptime( &currentTime );
			SUB_ABSOLUTETIME(&currentTime, &lastRootHubChangeTime );
			absolutetime_to_nanoseconds(currentTime, &elapsedTime);
			elapsedTime /= 1000000;  // convert it to ms
			
			if ( elapsedTime < kOHCIRootHubPollingInterval )
			{
				USBLog(5, "AppleUSBOHCI[%p]::SimulateRootHubInt  Last change was %qd ms ago.  IOSleep'ing for %qd ms",  this, elapsedTime, kOHCIRootHubPollingInterval - elapsedTime );
				IOSleep( kOHCIRootHubPollingInterval - elapsedTime );
			}

			UIMRootHubStatusChange();
			
            return;
        }
    }

    IOUnlock(_intLock); /* Unlock the queue */
    Complete(completion, -1, bufLen); /* too many trans */
}

IOReturn
AppleUSBOHCI::SimulateInterruptEDCreate(UInt16 maxPacketSize, short pollingRate)
{
    return kIOReturnSuccess;
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
    int i;
    if (endpointNumber == 1)
    {
            if(direction != kUSBIn)
            {
                    USBLog(3, "AppleUSBOHCI[%p]::SimulateEDAbort - Root hub wrong direction Int pipe %d", this, direction);
                    return(-1);
            }
            USBLog(5, "AppleUSBOHCI[%p]::SimulateEDAbort Root hub aborting int transactions", this);
            for( i=0; i < kMaxOutstandingTrans; i++)
            {
                    UIMRootHubStatusChange(true);
            }
    }
    else
    {
            USBLog(5, "AppleUSBOHCI[%p]::SimulateEDAbort Root hub aborting control pipe", this);

            UIMRootHubStatusChange(false);
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
    if ( resetTime )
        clock_get_uptime(&_lastRootHubStatusChanged);
        
    return _lastRootHubStatusChanged;
}

bool	
AppleUSBOHCI::RootHubAreAllPortsDisconnected( )
{
    UInt8	numPorts = 0;
    UInt32	portStatus;
    
    OHCIGetNumberOfPorts( &numPorts );

    if ( numPorts == 0 )
        return false;

    while ( numPorts )
    {
        if ( _ohciBusState == kOHCIBusStateOff )
        {
            return false;
        }
        else
        {
            portStatus = USBToHostLong( _pOHCIRegisters->hcRhPortStatus[--numPorts] );
            if ( (portStatus & kOHCIHcRhPortStatus_CCS) != 0 )
                return false;
        }
    }

    return true;
}

void
AppleUSBOHCI::OHCIGetNumberOfPorts( UInt8 * numPorts )
{
    UInt32		descriptorA;

    if ( _ohciBusState == kOHCIBusStateOff )
    {
       *numPorts = 0;
    }
    else
    {
        descriptorA = USBToHostLong(_pOHCIRegisters->hcRhDescriptorA);
        *numPorts = ( (descriptorA & kOHCIHcRhDescriptorA_NDP) >> kOHCIHcRhDescriptorA_NDPPhase );
    }
}

OSMetaClassDefineReservedUsed(IOUSBController,  14);

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
                            0x43, 0x00,	// "C"
                            0x6f, 0x00,	// "o"
                            0x6d, 0x00,	// "m"
                            0x70, 0x00,	// "p"
                            0x75, 0x00,	// "u"
                            0x74, 0x00,	// "t"
                            0x65, 0x00,	// "e"
                            0x72, 0x00,	// "r"
                            0x2c, 0x00,	// ","
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


    
