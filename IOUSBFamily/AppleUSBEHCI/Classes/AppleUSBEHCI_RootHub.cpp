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


#include <libkern/OSByteOrder.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>

#include "AppleUSBEHCI.h"

#ifndef APPLEEHCIROOTHUB_USE_KPRINTF 
	#define APPLEEHCIROOTHUB_USE_KPRINTF 0
#endif

#if APPLEEHCIROOTHUB_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= APPLEEHCIROOTHUB_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

enum {
	kAppleVendorID				= 0x05AC,	// Assigned by USB-if
	kPrdRootHubAppleE			= 0x8006,	// Apple ASIC root hub
	kEHCIRootHubPollingInterval = 32		// Polling interval for Root Hub
};


#define super IOUSBControllerV3
#define self this

/*
 * Root hub methods
 */
// FIXME  Should this routine go in the device?
IOReturn 
AppleUSBEHCI::GetRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc)
{
    IOUSBDeviceDescriptor newDesc =
{
	sizeof(IOUSBDeviceDescriptor),			// UInt8 length;
	kUSBDeviceDesc,							// UInt8 descType;
	HostToUSBWord(kUSBRel20),				// UInt16 usbRel;
	kUSBHubClass,							// UInt8 class;
	kUSBHubSubClass,						// UInt8 subClass;
	1,										// UInt8 protocol;
	64,										// UInt8 maxPacketSize;
	HostToUSBWord(kAppleVendorID),			// UInt16 vendor:  Use the Apple Vendor ID from USB-IF
	HostToUSBWord(kPrdRootHubAppleE),		// UInt16 product:  All our root hubs are the same
	HostToUSBWord(0x0200),					// UInt16 devRel: Supports USB 2.0
	2,										// UInt8 manuIdx;
	1,										// UInt8 prodIdx;
	0,										// UInt8 serialIdx;
	1										// UInt8 numConf;
};

if (!desc)
return kIOReturnNoMemory;

bcopy(&newDesc, desc, newDesc.bLength);

return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::GetRootHubDescriptor(IOUSBHubDescriptor *desc)
{
    IOUSBHubDescriptor hubDesc;
	UInt32						HCSParams;
    UInt8 pps;
    int i, numBytes;
    UInt8 *dstPtr;
    
	
    hubDesc.length = sizeof(IOUSBHubDescriptor);
    hubDesc.hubType = kUSBHubDescriptorType;
	
	
	HCSParams = USBToHostLong(_pEHCICapRegisters->HCSParams);
	
	hubDesc.numPorts = HCSParams & kEHCINumPortsMask;
	_rootHubNumPorts = hubDesc.numPorts;
	
	pps = (HCSParams & kEHCIPPCMask) != 0;
	
    hubDesc.characteristics = 0;
    hubDesc.characteristics |= (pps  ? kPerPortSwitchingBit   : 0);
    
	if ( !(hubDesc.characteristics & ( kNoOverCurrentBit | kPerPortOverCurrentBit)) )
	{
		// We have Global overcurrent protection
		_gangedOvercurrent = true;
	}
	
    // Everything else is zero
    
    hubDesc.characteristics = HostToUSBWord(hubDesc.characteristics);
	
    hubDesc.powerOnToGood = 50;	// It don't tell us??
    hubDesc.hubCurrent = 0;
	
    numBytes = (hubDesc.numPorts + 1) / 8 + 1;
    
    dstPtr = (UInt8 *)&hubDesc.removablePortFlags[0];
    // Set removable port flags.
    // All ports are removable.
    for (i=0; i<numBytes; i++) {
        *dstPtr++ = 0;
    }
    // Set power control flags.
    // All ports set as 1.
    for (i=0; i<numBytes; i++) {
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
AppleUSBEHCI::GetRootHubConfDescriptor(OSData *desc)
{
    IOUSBConfigurationDescriptor confDesc =
	{
		sizeof(IOUSBConfigurationDescriptor),//UInt8 length;
		kUSBConfDesc,               //UInt8 descriptorType;
		HostToUSBWord(sizeof(IOUSBConfigurationDescriptor) +
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
		1,                      //UInt8 interfaceProtocol;
		0                       //UInt8 interfaceStrIndex;
	};
	IOUSBEndpointDescriptor endptDesc =
	{
		sizeof(IOUSBEndpointDescriptor),	// UInt8 length;
		kUSBEndpointDesc,					// UInt8 descriptorType;
		0x81,								// UInt8  endpointAddress; In, 1
		kUSBInterrupt,						// UInt8 attributes;
		HostToUSBWord(8),					// UInt16 maxPacketSize;
		9,									// UInt8 interval (256 microframes or 32 ms)
	};

	if (!desc)
		return(kIOReturnNoMemory);

	if (!desc->appendBytes(&confDesc,  confDesc.bLength))
		return(kIOReturnNoMemory);

	if (!desc->appendBytes(&intfDesc,  intfDesc.bLength))
		return(kIOReturnNoMemory);

	if (!desc->appendBytes(&endptDesc, endptDesc.bLength))
		return(kIOReturnNoMemory);

	return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::SetRootHubDescriptor(OSData * /*buffer*/)
{
    USBLog(3,"AppleUSBEHCI[%p]::SetRootHubDescriptor unimplemented",  this);
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::GetRootHubStatus(IOUSBHubStatus *status)
{
    *(UInt32 *)status = 0;		// The EHCI doesn't have OC or LPS status, nop remote wakeup status
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::SetRootHubFeature(UInt16 wValue)
{
    switch(wValue)
    {
        case kUSBHubLocalPowerChangeFeature :
			USBLog(3,"AppleUSBEHCI[%p]: unimplemented Set Power Change Feature",  this);
            // EHCIRootHubLPSChange(true);  // not implemented yet
            break;
			
        case kUSBHubOverCurrentChangeFeature :
            USBLog(3,"AppleUSBEHCI[%p]: unimplemented Set Overcurrent Change Feature",  this);
            // EHCIRootHubOCChange(true);  // not implemented yet
            break;
            
        default:
            USBLog(3,"AppleUSBEHCI[%p]: Unknown hub set (%d) in root hub",  this, wValue);
            break;
    }
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::ClearRootHubFeature(UInt16 wValue)
{
    switch(wValue)
    {
        case kUSBHubLocalPowerChangeFeature :
            USBLog(3,"AppleUSBEHCI[%p]: unimplemented Clear Power Change Feature",  this);
            // EHCIRootHubLPSChange(false);  // not implemented yet
            break;
			
        case kUSBHubOverCurrentChangeFeature :
            USBLog(3,"AppleUSBEHCI[%p]: unimplemented Clear Overcurrent Change Feature",  this);
            // EHCIRootHubOCChange(false);  // not implemented yet
            break;
			
        default:
            USBLog(3,"AppleUSBEHCI[%p]: Unknown hub clear (%d) in root hub",  this, wValue);
            break;
    }
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::GetRootHubPortStatus(IOUSBHubPortStatus *status, UInt16 port)
{
    UInt16		statusFlags, changeFlags;
    UInt32		portSC, portSCChange;
	
	
    if ( _myBusState == kUSBBusStateSuspended )
        return kIOReturnNotResponding;
	
    if (port < 1 || port > 15)
        return kIOReturnBadArgument; 
	
    //adjust port number for array use
    port--;
	
    portSC = USBToHostLong (_pEHCIRegisters->PortSC[port]);
    USBLog(7,"AppleUSBEHCI[%p]::GetRootHubPortStatus for port %d, current: 0x%x, previous: 0x%x",  this, port+1, (unsigned int)portSC, (unsigned int)_rhPrevStatus[port] );
	
    
    // translate EHCI's random flag order into same order as hub report (and OHCI reg)
    statusFlags = 0;
    if( (portSC & kEHCIPortSC_Connect) != 0)
    {
	    statusFlags |= kHubPortConnection;
    }
    if( (portSC & kEHCIPortSC_Enabled) != 0)
    {
	    statusFlags |= kHubPortEnabled;
    }
    if(((portSC & kEHCIPortSC_Suspend) != 0) && !_rhPortBeingResumed[port])
    {
		// only show the suspend status if we are not in the process of resuming. In that case, we will
		// have the status going clear during that 20ms time period, and the change will be set when it is done.
	    statusFlags |= kHubPortSuspend;
    }
    if( (portSC & kEHCIPortSC_OverCurrent) != 0)
    {
	    statusFlags |= kHubPortOverCurrent;
    }
    if( (portSC & kEHCIPortSC_Reset) != 0)
    {
	    statusFlags |= kHubPortBeingReset;
    }
    if( (portSC & kEHCIPortSC_Power) != 0)
    {
	    statusFlags |= kHubPortPower;
    }
    
    // Note: Speed not reported, this is USB 2
    // Not quite, we have to say its a high speed device
    
    statusFlags |= kHubPortHighSpeed;
    
    status->statusFlags = HostToUSBWord(statusFlags);
    
    // Now do the same for change flags, we need to work some of this out ourselves.
    
    portSCChange = (_rhPrevStatus[port] ^ portSC);	// That's Xor, bitwise
    changeFlags = 0;
	
    if ( (portSC & kEHCIPortSC_ConnectChange) != 0)
    {
	    changeFlags |= kHubPortConnection;
	    if ((statusFlags & kHubPortConnection) == 0)
		{
			// if we were connected and now we aren't
			// and our suspend status has changed..
			if ((portSCChange & kEHCIPortSC_Suspend) != 0)
				_rhChangeBits[port] |= kHubPortSuspend;
		}
    }
	
    if ( (portSC & kEHCIPortSC_EnableChange) != 0)
    {
	    changeFlags |= kHubPortEnabled;
    }
	// don't set the suspend change bit until we are done with the resume sequence
	// we also set this bit above if we were suspended and are now disconnected
    if (_rhChangeBits[port] & kHubPortSuspend)
    {
	    changeFlags |= kHubPortSuspend;
    }
    if ( (portSC & kEHCIPortSC_OCChange) != 0)
    {
	    changeFlags |= kHubPortOverCurrent;
    }
    
    if ( (portSCChange & kEHCIPortSC_Reset) != 0)
    {
	    _rhChangeBits[port] |= kHubPortBeingReset;
    }
    if (_rhChangeBits[port] & kHubPortBeingReset)
    {
	    changeFlags |= kHubPortBeingReset;
    }
	
    // This bit should not be set for the change bits. Its makes no sense, its confusing the change logic.
	//    portFlags |= kHubPortHighSpeed;		// Anything connected to EHCI is always high speed
	// If its not high speed, its on the companion
	
	// If we need to debounce the overcurrent bit, do it here before we change the status flags
	if ( (changeFlags & kHubPortOverCurrent) && (_errataBits & kErrataNeedsOvercurrentDebounce) )
	{
		UInt32		portSC2;
		
		USBLog(3,"AppleUSBEHCI[%p]::GetRootHubPortStatus Have an overcurrent on port (%d), but need to debounce it",  this, port + 1);
		// Wait for 10ms and check the stuff again
		IOSleep(10);
		
		// Now, check the Overcurrent ACTIVE bit, not the changed bit.
		
		portSC2 = USBToHostLong (_pEHCIRegisters->PortSC[port]);
		
		if( (portSC2 & kEHCIPortSC_OverCurrent) == 0)
		{
			USBLog(3,"AppleUSBEHCI[%p]::GetRootHubPortStatus Overcurrent on port (%d) has gone away, clearing the change",  this, port + 1);
			// This indicates that the overcurrent is not there anymore, so clear the bit from the change flags
			changeFlags &= ~kHubPortOverCurrent;
			
			// Now, clear the Over-current Change bit
			EHCIRootHubResetOverCurrentChange(port+1);
		}
	}
    status->changeFlags = HostToUSBWord(changeFlags);
	
    _rhPrevStatus[port] = portSC;
	
    USBLog(7,"AppleUSBEHCI[%p]::GetRootHubPortStatus for port %d, status: 0x%x, change: 0x%x, ganged: %d",  this, port+1, status->statusFlags, status->changeFlags, _gangedOvercurrent );
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::SetRootHubPortFeature(UInt16 wValue, UInt16 wIndex)
{
    IOReturn	err;
    UInt16	port = wIndex;
    
    switch(wValue)
    {
        case kUSBHubPortSuspendFeature :
            USBLog(5,"AppleUSBEHCI[%p]::SetRootHubPortFeature suspending port %d",  this, port);
            err = EHCIRootHubPortSuspend(port, true);
            break;
			
        case kUSBHubPortResetFeature :
            err = EHCIRootHubResetPort(port);
            break;
			
        case kUSBHubPortEnableFeature :
            err = EHCIRootHubPortEnable(port, true);
            break;
			
        case kUSBHubPortPowerFeature :
            err = EHCIRootHubPortPower(port, true);
            err = EHCIRootHubPower(true);
            break;
			
        default:
            USBLog(3,"AppleUSBEHCI[%p]::SetRootHubPortFeature unknown wValue %d, wIndex %d",  this, wValue, wIndex);
            err = kIOReturnUnsupported;
            break;
    }
	
    return err;
}



IOReturn 
AppleUSBEHCI::ClearRootHubPortFeature(UInt16 wValue, UInt16 wIndex)
{
    IOReturn	err;
    UInt16	port = wIndex;
	
	USBLog(4, "AppleUSBEHCI[%p]::ClearRootHubPortFeature - port %d, feature: %d",  this, wIndex, wValue);

	
    switch(wValue)
    {
        case kUSBHubPortEnableFeature :
			err = EHCIRootHubPortEnable(port, false);
            break;
			
        case kUSBHubPortSuspendFeature :
            USBLog(5,"AppleUSBEHCI[%p]::ClearRootHubPortFeature resuming port %d",  this, port);
			err = EHCIRootHubPortSuspend(port, false);
            break;
			
        case kUSBHubPortPowerFeature :
			err = EHCIRootHubPortPower(port, false);
            // Now need to check if all ports are switched off and
            // gang off if in gang mode
            break;
			
			// ****** Change features *******
        case kUSBHubPortConnectionChangeFeature :
			err = EHCIRootHubResetChangeConnection(port);
            break;
			
        case kUSBHubPortEnableChangeFeature :
            err = EHCIRootHubResetEnableChange(port);
            break;
			
        case kUSBHubPortSuspendChangeFeature :
			err = EHCIRootHubResetSuspendChange(port);
            break;
			
        case kUSBHubPortOverCurrentChangeFeature :
			err = EHCIRootHubResetOverCurrentChange(port);
            break;
			
        case kUSBHubPortResetChangeFeature :
			err = EHCIRootHubResetResetChange(port);
            break;
			
        default:
            USBLog(3,"AppleUSBEHCI[%p]::ClearRootHubPortFeature unknown wValue %d, wIndex %d",  this, wValue, wIndex);
            err = kIOReturnUnsupported;
            break;
    }
    return err;
}



IOReturn 
AppleUSBEHCI::GetRootHubPortState(UInt8 *state, UInt16 port)
{
    USBLog(5,"AppleUSBEHCI[%p]::GetRootHubPortState for port %d",  this, port);
    return kIOReturnSuccess;
}


IOReturn 
AppleUSBEHCI::SetHubAddress(UInt16 wValue)
{
    _rootHubFuncAddress = wValue;
	
    return kIOReturnSuccess;
}



UInt32 
getPortSCForWriting(EHCIRegistersPtr _pEHCIRegisters, short port)
{
    return USBToHostLong(_pEHCIRegisters->PortSC[port-1]) & 
	~(kEHCIPortSC_ConnectChange|kEHCIPortSC_EnableChange|kEHCIPortSC_OCChange);	// Mask out these bits, if set, they clear the condition
	
}



IOReturn 
AppleUSBEHCI::EHCIRootHubPower(bool on)
{
    on = 0;
	
    // I don't think this has a power switch
    
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::EHCIRootHubResetChangeConnection(UInt16 port)
{
    UInt32 value, newValue, count;
	
    value = getPortSCForWriting(_pEHCIRegisters,port);
	
    value |= kEHCIPortSC_ConnectChange;					// clear status change
	
    _pEHCIRegisters->PortSC[port-1] = HostToUSBLong (value);
    IOSync();
	
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		count = 0;
		while ((newValue & kEHCIPortSC_ConnectChange) && (count++ < 10))
		{
			USBError(1, "EHCI driver - Connect Change bit not clearing. Retrying");
			_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		}
	}
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::EHCIRootHubResetResetChange(UInt16 port)
{
    port--;
    if(port >= kMaxPorts)
    {
        USBLog(3, "AppleUSBEHCI[%p]::EHCIRootHubResetResetChange Too many ports specified(%d > %d)",  this, port, kMaxPorts);
		return kIOReturnBadArgument;
    }
    
    _rhChangeBits[port] &= ~kHubPortBeingReset;
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::EHCIRootHubResetSuspendChange(UInt16 port)
{
    port--;
    if(port >= kMaxPorts)
    {
        USBLog(3, "AppleUSBEHCI[%p]::EHCIRootHubResetSuspendChange Too many ports specified(%d > %d)",  this, port, kMaxPorts);
        return kIOReturnBadArgument;
    }
    
    _rhChangeBits[port] &= ~kHubPortSuspend;
    IOSync();
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::EHCIRootHubResetEnableChange(UInt16 port)
{
    UInt32 value, newValue, count;
	
    value = getPortSCForWriting(_pEHCIRegisters,port);
	
    value |= kEHCIPortSC_EnableChange;			// clear status change
	
    _pEHCIRegisters->PortSC[port-1] = HostToUSBLong (value);
    IOSync();
	
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		count = 0;
		while ((newValue & kEHCIPortSC_EnableChange) && (count++ < 10))
		{
			USBError(1, "EHCI driver - Enable Change bit not clearing. Retrying");
			_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		}
	}
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::EHCIRootHubResetOverCurrentChange(UInt16 port)
{
    UInt32 value, newValue, count;
	
    value = getPortSCForWriting(_pEHCIRegisters,port);
	
    value |= kEHCIPortSC_OCChange;					// clear status change
	
	_pEHCIRegisters->PortSC[port-1] = HostToUSBLong (value);
	IOSync();
	
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		count = 0;
		while ((newValue & kEHCIPortSC_OCChange) && (count++ < 10))
		{
			USBError(1, "EHCI driver - OC Change bit not clearing. Retrying");
			_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		}
	}
	
	return kIOReturnSuccess;
}



void 
AppleUSBEHCI::waitForSOF(EHCIRegistersPtr pEHCIRegisters)
{
    IOSleep(1);
}



IOReturn 
AppleUSBEHCI::EHCIRootHubResetPort (UInt16 port)
{
    UInt32 		value, newValue;
    UInt32		count, resetCount, portSC = 0;
	
	
    USBLog(5, "AppleUSBEHCI[%p]::EHCIRootHubResetPort begin port %d",  this, (int)port);
	
    value = getPortSCForWriting(_pEHCIRegisters, port);
    
    if( (value & (kEHCIPortSC_Connect | kEHCIPortSC_Power)) != (kEHCIPortSC_Connect | kEHCIPortSC_Power) )
    {
		// Can only enter the resetting state if the device is connected and the power is on.
		// Having probelms with reset happening to powered off device, reset change is on 
		// when next device is connected. That consfuses hub logic which skips the reset
		// state. So no device is enumerated if that happens.
		// This is all because EHCI is so miserly, they can't have a proper reset state machine like OHCI.
		USBLog(1, "AppleUSBEHCI[%p]::EHCIRootHubResetPort - Not resetting port, beacuse device is unplugged of powered off (%x).",  this, (unsigned int)value);
		return kIOReturnNotResponding;	
    }
	
    if( ((value & kEHCIPortSC_LineSt) >> kEHCIPortSC_LineStPhase) == kEHCILine_Low)
    {
		value |= kEHCIPortSC_Owner;
		USBLog(5, "AppleUSBEHCI[%p]::EHCIRootHubResetPort: LS device detected (portSC = %p) - writing value (%p) to release the device",  this, (void*)portSC, (void*)value);
		_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
		IOSync();
		IOSleep(1);
		if (_errataBits & kErrataNECIncompleteWrite)
		{
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
			count = 0;
			while (!(newValue & kEHCIPortSC_Owner) && (count++ < 10))
			{
				USBError(1, "EHCI driver - PortSC_Owner (LS device) bit not sticking. Retrying");
				_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
				IOSync();
				IOSleep(1);
				newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
			}
		}
        // Pretend the port doesn't exist.
		return kIOUSBDeviceNotHighSpeed;
    }
	
    value |= kEHCIPortSC_Reset;
    value &= ~kEHCIPortSC_Enabled;
    
    // Set the reset on
    _pEHCIRegisters->PortSC[port-1] = HostToUSBLong (value);
    IOSync();
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		count = 0;
		while ((count++ < 10) && (!(newValue & kEHCIPortSC_Reset) || (newValue & kEHCIPortSC_Enabled)))
		{
			USBError(1, "EHCI driver - Reset and enabled bits not sticking. Retrying");
			_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		}
	}
    // Note:  Section 7.1.7.5 of the USB Spec requires a 50ms reset for RootHub ports, not 10, as we had done prior to Feb 2004.  However, in
	// testing the 10.3.6 update, (SUPanNavy), the following 2 bugs were traced to this pulse being 50ms:
	// <rdar://problem/3829981> 7R13: Suntac Slipper X does not work correctly on Navy
    // <rdar://problem/3841991> SUPanNavy7R20 with USBCDC3.0.1GMc2: iSync hangs while trying to connect to V710.
	//
	// Because we have no record of any devices failing when we had the 10ms reset pulse, we are changing it back to 10ms, even tho' it violates
	// the spec.  We certainly hope not to have to revisit this, but we might.  
	//
    IOSleep(10);
    
    value = getPortSCForWriting(_pEHCIRegisters, port);
    value &= ~kEHCIPortSC_Reset;
	count = 0;
	do {
		_pEHCIRegisters->PortSC[port-1] = HostToUSBLong (value);
		IOSync();
		
		// sleep for 3ms instead of 2ms, just to give the chip a little more margin (2 here, 1 in the do loop)
		IOSleep(2);
		
		resetCount = 0;
		do {
			IOSleep(1);
			portSC = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		} while( (portSC & kEHCIPortSC_Reset) && (resetCount++ < 2001) );
		
		if (!(portSC & kEHCIPortSC_Reset))
		{
			USBLog(5, "AppleUSBEHCI[%p]::EHCIRootHubResetPort reset took extra %d",  this, (int)count);
		}
		else
			USBError(1, "EHCIRootHubResetPort - clear reset didn't take - retrying");
	} while ((portSC & kEHCIPortSC_Reset) && (count++ < 10));
	
    IOSleep(1);
    
    if ( portSC != USBToHostLong(_pEHCIRegisters->PortSC[port-1]) )
    {
        USBLog(1, "AppleUSBEHCI[%p]::EHCIRootHubResetPort-  portSC is not equal to value of register! (%p)(%p)",  this, (void*)portSC, (void*)USBToHostLong(_pEHCIRegisters->PortSC[port-1]));
		// use the updated value instead of the cached one
		portSC = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
    }
	
    if( ((portSC & kEHCIPortSC_Reset) != 0) || (count >= 2000) )
    {
		USBLog(4, "AppleUSBEHCI[%p]::EHCIRootHubResetPort-  port slow to come out of reset %d",  this, (int)count);
    }
	
    if( (portSC & (kEHCIPortSC_Connect | kEHCIPortSC_Power)) != (kEHCIPortSC_Connect | kEHCIPortSC_Power) )
    {
		// Have been disconnected or powered off, pretend reset never happened.
		
		USBLog(1, "AppleUSBEHCI[%p]::EHCIRootHubResetPort - Not resetting port 2, beacuse device is unplugged of powered off (%p).",  this, (void*)portSC);
		return kIOReturnNotResponding;	
    }
	
    USBLog(5, "AppleUSBEHCI[%p]::EHCIRootHubResetPort - Setting port (%d) reset change bit to 0x%lx.",  this, (int)port, value);
    _rhChangeBits[port-1] |= kHubPortBeingReset;
	
    if( (portSC & kEHCIPortSC_Enabled) == 0)
    {
		// USBLog(2, "AppleUSBEHCI[%p]::EHCIRootHubResetPort-  full speed device (no enable) releasing device %x",  this, portSC);
		value = getPortSCForWriting(_pEHCIRegisters, port);
		value |= kEHCIPortSC_Owner;
		USBLog(5, "AppleUSBEHCI[%p]::EHCIRootHubResetPort (port %d): FS device detected (portSC = 0x%lx) - writing value (0x%lx) to release the device",  this, (int)port, portSC, value);
		_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
		IOSync();
		IOSleep(1);
		if (_errataBits & kErrataNECIncompleteWrite)
		{
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
			count = 0;
			while (!(newValue & kEHCIPortSC_Owner) && (count++ < 10))
			{
				USBError(1, "EHCI driver - PortSC_Owner (FS device) bit not sticking. Retrying");
				_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
				IOSync();
				IOSleep(1);
				newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
			}
		}
        // Pretend the port doesn't exist.
		return kIOUSBDeviceNotHighSpeed;
    }
    else
    {
		waitForSOF(_pEHCIRegisters);
		IOSleep(1);
		portSC = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		if( (portSC & kEHCIPortSC_Enabled) == 0)
		{
			USBLog(3, "AppleUSBEHCI[%p]::EHCIRootHubResetPort *********** Port disabled after 1 frame******* 0x%lx",  this, portSC);
		}
    }
	
    USBLog(5, "AppleUSBEHCI[%p]::EHCIRootHubResetPort done",  this);

    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::EHCIRootHubPortEnable(UInt16 port, bool enable)
{
    UInt32 		value;
	
    USBLog(5,"AppleUSBEHCI[%p]::EHCIRootHubPortEnable port: %d, on: %d",  this, port, enable);
	
    if (enable)
    {
        USBLog(1,"AppleUSBEHCI[%p]::EHCIRootHubPortEnable enabling port illegal.",  this);
        return kIOReturnUnsupported;
    }
	
    value = getPortSCForWriting(_pEHCIRegisters, port);
    value &= ~kEHCIPortSC_Enabled;													// disable port
    _pEHCIRegisters->PortSC[port-1] = HostToUSBLong (value);
    IOSync();
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32	newValue, count;
		IOSleep(3);
		newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		count = 0;
		while ((newValue & kEHCIPortSC_Enabled) && (count++ < 10))
		{
			USBError(1, "EHCI driver - PortEnable did not disable. Retrying");
			_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		}
	}
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::EHCIRootHubPortSuspend(UInt16 port, bool suspend)
{
    UInt32 		value, newValue, count;
    
    USBLog(5,"AppleUSBEHCI[%p]::EHCIRootHubPortSuspend port: %d, %s",  this, port, suspend ? "SUSPEND" : "RESUME");
	
    value = getPortSCForWriting(_pEHCIRegisters, port);
    
    if (suspend)
	{
        value |= kEHCIPortSC_Suspend;					// suspend port
		
		// Update our previous statuas so next time we don't see a suspend change (which would get reported as a resume)
		_rhPrevStatus[port-1] = value;
	}
    else
	{
        value |= kEHCIPortSC_Resume;					// resume port
	}
	
    _pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
    IOSync();
	
	IOSleep(1);											// allow it to kick in
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		count = 0;
		while (!(newValue & (suspend ? kEHCIPortSC_Suspend : kEHCIPortSC_Resume)) && (count++ < 10))
		{
			USBError(1, "EHCI driver - PortSuspend did not stick. Retrying");
			_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
			IOSleep(1);											// allow it to kick in
		}
	}
    
    if (!suspend)
    {
		USBLog(5,"AppleUSBEHCI[%p]::EHCIRootHubPortSuspend - resuming port %d, calling out to timer", this, (int)port);
		_rhPortBeingResumed[port-1] = true;
		thread_call_enter1(_rhResumePortTimerThread[port-1], (void*)port);
	}
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::EHCIRootHubPortPower(UInt16 port, bool on)
{
    UInt32 	value, newValue, count;
	
    value = getPortSCForWriting(_pEHCIRegisters, port);
    
    if(on)
    {
		// we need to power on EHCI in two steps. The connect/disconnect bits latch to 0 if Port Power is off
		// and they don't seem to be able to be set simultaneously. So first set the PP, then connect and disconnect
		value |= kEHCIPortSC_Power;								// enable port power
		_pEHCIRegisters->PortSC[port-1] = HostToUSBLong (value);
        IOSync();
		if (_errataBits & kErrataNECIncompleteWrite)
		{
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
			count = 0;
			while (!(newValue & kEHCIPortSC_Power) && (count++ < 10))
			{
				USBError(1, "EHCI driver - PortPower bit not sticking. Retrying");
				_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
				IOSync();
				newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
			}
		}
		
		value |= kEHCIPortSC_WKCNNT_E | kEHCIPortSC_WKDSCNNT_E;					// enable connect/disconnect
    }
    else
    {
		value &= ~kEHCIPortSC_Power;											// disable port power
    }
	
    _pEHCIRegisters->PortSC[port-1] = HostToUSBLong (value);
    IOSync();
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		count = 0;
		while ((on ? (!(newValue & kEHCIPortSC_WKCNNT_E) || !(newValue & kEHCIPortSC_WKDSCNNT_E)) : (newValue & kEHCIPortSC_Power)) && (count++ < 10))
		{
			USBError(1, "EHCI driver - PortPower end bits not sticking. Retrying");
			_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		}
	}
	
    return kIOReturnSuccess;
}



// obsolete method
void
AppleUSBEHCI::UIMRootHubStatusChange( bool abort )
{
	USBLog(1, "AppleUSBEHCI[%p]::UIMRootHubStatusChange - calling obsolete method UIMRootHubStatusChange(bool)", this);
}



/*
 * UIMRootHubStatusChange
 *
 * This method gets called during a 32 ms timer task to see if there is anything that needs to be reported
 * to the root hub driver.
 */
void 
AppleUSBEHCI::UIMRootHubStatusChange(void)
{
    UInt8										numPorts = 0;
    UInt32										HCSParams;
    UInt16										statusChangedBitmap;   /* only have 15 ports in EHCI */
    IOUSBHubPortStatus							portStatus;
    UInt32										hubStatus, statusBit, tempStatus;
    unsigned int								index, port, move;
	bool										overCurrentReported = false;
	IOReturn									retStatus;
    
	USBLog(7, "AppleUSBEHCI[%p]::UIMRootHubStatusChange - USBIntr[%p] _myPowerState[%d]", this, (void*)USBToHostLong(_pEHCIRegisters->USBIntr), (int)_myPowerState);
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
	
    if (_controllerAvailable && !_wakingFromHibernation && (GetRootHubStatus((IOUSBHubStatus *)&tempStatus) == kIOReturnSuccess))
    {
        hubStatus = USBToHostLong( tempStatus );
        if ((hubStatus & (kHubLocalPowerStatus | kHubOverCurrentIndicator) ) != 0)
            statusChangedBitmap |= statusBit; /* Hub status change bit */
		
        HCSParams = USBToHostLong(_pEHCICapRegisters->HCSParams);
        numPorts = HCSParams & kEHCINumPortsMask;
		
        USBLog(7,"AppleUSBEHCI[%p]::UIMRootHubStatusChange numPorts %d _wakingFromHibernation(%s)",  this, numPorts, _wakingFromHibernation ? "true" : "false");
        
        for (port = 1; port <= numPorts; port++)
        {
            statusBit <<= 1;    /* Next bit */
			
            retStatus = GetRootHubPortStatus(&portStatus, port);
			if (retStatus != kIOReturnSuccess)
			{
				USBLog(5, "AppleUSBEHCI[%p]::UIMRootHubStatusChange - got status (%p) from GetRootHubPortStatus for port (%d) - skipping", this, (void*)retStatus, (int)port);
				continue;
			}
            portStatus.changeFlags = USBToHostWord(portStatus.changeFlags);
            portStatus.statusFlags = USBToHostWord(portStatus.statusFlags);
			
			// If we have ganged overcurrent AND we already have a port that is reporting overcurrent, then don't report it on this
			// port.  Just clear it and go to the next port.
			if ( _gangedOvercurrent &&  overCurrentReported && ( portStatus.changeFlags & kHubPortOverCurrent) )
			{
				USBLog(3,"AppleUSBEHCI[%p]::UIMRootHubStatusChange port %d had an overcurrent, but another port already reported it",  this, port);
				
				// Clear the change in the RH register
				portStatus.changeFlags &= ~kHubPortOverCurrent;
				
				EHCIRootHubResetOverCurrentChange(port);
			}
			
			// if ( (portStatus.changeFlags & kHubPortConnection) || (portStatus.changeFlags & kHubPortEnabled))
			// This should use a symbolic constant, but there doesn't seem to be one handy.
			// 1f is all the possible change bits. It needs to be masked because get status
			// was accidentally returning bad change bits at one time. (high speed change!!)
			// The first fix (above) stopped all but connect and enabled status changes getting through
			if (portStatus.changeFlags & kHubPortStateChangeMask) 
            {
                USBLog(4,"AppleUSBEHCI[%p]::UIMRootHubStatusChange port %d status(%p) change(%p)",  this, port, (void*)portStatus.statusFlags, (void*)portStatus.changeFlags);
				
				if ( portStatus.changeFlags & kHubPortOverCurrent )
				{
					USBLog(3,"AppleUSBEHCI[%p]::UIMRootHubStatusChange port %d had an overcurrent",  this, port);
					overCurrentReported = true;
				}
				
                statusChangedBitmap |= statusBit; 	// Hub status change bit
            }
        }
    }
	if (statusChangedBitmap)
	{
		USBLog(3,"AppleUSBEHCI[%p]::UIMRootHubStatusChange got bitmap (%p)",  this, (void*)statusChangedBitmap);
	}
	_rootHubStatusChangedBitmap = statusChangedBitmap;
}



IOReturn 
AppleUSBEHCI::SimulateEDDelete (short endpointNumber, short direction)
{
    return SimulateEDAbort(endpointNumber, direction);
}



IOReturn 
AppleUSBEHCI::SimulateEDAbort (short endpointNumber, short direction)
{
    int i;
    if (endpointNumber == 1)
    {
		if(direction != kUSBIn)
		{
			USBLog(3, "AppleUSBEHCI[%p]::SimulateEDAbort - Root hub wrong direction Int pipe %d",  this, direction);
			return kIOReturnInternalError;
		}
		USBLog(5, "AppleUSBEHCI[%p]::SimulateEDAbort Root hub aborting int transactions",  this);
		RootHubAbortInterruptRead();
    }
    else
    {
		USBLog(5, "AppleUSBEHCI[%p]::SimulateEDAbort Root hub aborting control pipe (NOP)",  this);
	}
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::GetRootHubStringDescriptor(UInt8	index, OSData *desc)
{
	
    // The following strings are in Unicode format
    //
    UInt8 productName[] = {
		0,			// Length Byte
		kUSBStringDesc,	// Descriptor type
		0x45, 0x00, // "E"
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



AbsoluteTime	
AppleUSBEHCI::LastRootHubPortStatusChanged( bool resetTime )
{
    if ( resetTime )
        clock_get_uptime(&_lastRootHubStatusChanged);
	
    return _lastRootHubStatusChanged;
}



void
AppleUSBEHCI::GetNumberOfPorts( UInt8 * numPorts )
{
    UInt32		descriptorA;
	
    if ( _myBusState == kUSBBusStateReset )
    {
		*numPorts = 0;
    }
    else
    {
		*numPorts = USBToHostLong(_pEHCICapRegisters->HCSParams) & kEHCINumPortsMask;
    }
}



// these are for handling a root hub resume without hanging out in the WL for 20 ms
// static
void
AppleUSBEHCI::RHResumePortTimerEntry(OSObject *target, thread_call_param_t port)
{
    AppleUSBEHCI *me = OSDynamicCast(AppleUSBEHCI, target);
	if (!me)
		return;

	me->RHResumePortTimer((UInt32)port);
}



void
AppleUSBEHCI::RHResumePortTimer(UInt32 port)
{
	// we are responsible for terminating the resume on a root hub port ourselves
	// and we used to do it inside of the workloop. now we do the timing part of it
	// outside of the WL
	if (!_commandGate)
		return;

	USBLog(5, "AppleUSBEHCI[%p]::RHResumePortTimer - timing the resume for port %d", this, (int)port);
	IOSleep(20);								// wait 20 ms for the resume to complete
	_commandGate->runAction(RHResumePortCompletionEntry, (void*)port);
}



// static
IOReturn
AppleUSBEHCI::RHResumePortCompletionEntry(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    AppleUSBEHCI				*me = OSDynamicCast(AppleUSBEHCI, target);
    UInt32						port = (UInt32)param1;
	
	if (!me)
		return kIOReturnInternalError;
	
	return me->RHResumePortCompletion(port);
}



IOReturn			
AppleUSBEHCI::RHResumePortCompletion(UInt32 port)
{
	UInt32			value;
	
	USBLog(5, "AppleUSBEHCI[%p]::RHResumePortCompletion - finishing resume on port %d", this, (int)port);
	if (!_rhPortBeingResumed[port-1])
	{
		USBLog(1, "AppleUSBEHCI[%p]::RHResumePortCompletion - port %d does not appear to be resuming!", this, (int)port);
		return kIOReturnInternalError;
	}
	
	if (!_controllerAvailable)
	{
		USBLog(5, "AppleUSBEHCI[%p]::RHResumePortCompletion - cannot finish resume on port %d because the controller is unavailable", this, (int)port);
		_rhPortBeingResumed[port-1] = false;
		return kIOReturnInternalError;
	}
	
	value = getPortSCForWriting(_pEHCIRegisters, port);
	value &= ~kEHCIPortSC_Resume;												// resume port
	_pEHCIRegisters->PortSC[port-1] = HostToUSBLong (value);
	IOSync();
	IOSleep(2);																	// allow it to kick in
	if (_errataBits & kErrataNECIncompleteWrite)
	{
		UInt32	newValue, count;
		
		newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
		count = 0;
		while ((newValue & kEHCIPortSC_Resume) && (count++ < 10))
		{
			USBError(1, "EHCI driver - PortSuspend reesume did not clear. Retrying");
			_pEHCIRegisters->PortSC[port-1] = HostToUSBLong(value);
			IOSync();
			newValue = USBToHostLong(_pEHCIRegisters->PortSC[port-1]);
			IOSleep(3);															// allow it to kick in
		}
	}

	_rhPortBeingResumed[port-1] = false;
	_rhChangeBits[port-1] |= kHubPortSuspend;									// mark the suspend bit as having changed
	return kIOReturnSuccess;
}
