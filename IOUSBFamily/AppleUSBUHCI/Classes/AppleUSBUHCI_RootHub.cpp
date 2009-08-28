/*
 * Copyright © 2004-2009 Apple Inc.  All rights reserved.
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


#include <IOKit/usb/IOUSBLog.h> 
#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBHubPolicyMaker.h>

#include "AppleUSBUHCI.h"
#include "USBTracepoints.h"

/* Convert USBLog to use kprintf debugging */
#ifndef APPLEUHCIROOTHUB_USE_KPRINTF
#define APPLEUHCIROOTHUB_USE_KPRINTF 0
#endif

#if APPLEUHCIROOTHUB_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= 5) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif



#define super IOUSBControllerV3
#define self this
#define _rootHubPollingRate32			_v3ExpansionData->_rootHubPollingRate32

// ========================================================================
#pragma mark Public root hub methods
// ========================================================================


IOReturn
AppleUSBUHCI::GetRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc)
{
    IOUSBDeviceDescriptor newDesc = 	{
        sizeof(IOUSBDeviceDescriptor),		// UInt8 length;
        kUSBDeviceDesc,						// UInt8 descType;
        USB_CONSTANT16(kUSBRel10),			// UInt16 usbRel;
        kUSBHubClass,						// UInt8 class;
        kUSBHubSubClass,					// UInt8 subClass;
        0,									// UInt8 protocol;
        8,									// UInt8 maxPacketSize;
        USB_CONSTANT16(kAppleVendorID),		// UInt16 vendor:  Use the Apple Vendor ID from USB-IF
        USB_CONSTANT16(kPrdRootHubApple),	// UInt16 product:  All our root hubs are the same
        USB_CONSTANT16(0x0110),				// UInt16 devRel: Supports USB 1.1
        2,									// UInt8 manuIdx;
        1,									// UInt8 prodIdx;
        0,									// UInt8 serialIdx;
        1									// UInt8 numConf;
    };

    if (!desc)
        return(kIOReturnNoMemory);

    bcopy(&newDesc, desc, newDesc.bLength);

    return kIOReturnSuccess;
}

IOReturn
AppleUSBUHCI::GetRootHubDescriptor(IOUSBHubDescriptor *desc)
{
    IOUSBHubDescriptor	hubDesc;
	UInt32				appleCaptive = 0;
	OSNumber *			appleCaptiveProperty = NULL;
   
    if (!desc) 
        return(kIOReturnNoMemory); 
    
    hubDesc.length = sizeof(IOUSBHubDescriptor); 
    hubDesc.hubType = kUSBHubDescriptorType; 
    hubDesc.numPorts = kUHCI_NUM_PORTS;							// UHCI all have 2 ports - how convenient
	_rootHubNumPorts = kUHCI_NUM_PORTS;

    hubDesc.powerOnToGood = 50; /* 100ms */
    hubDesc.hubCurrent = 0;
    // XXX for now do not support overcurrent, although PIIX chips do support it
    hubDesc.characteristics = HostToUSBWord(kNoPowerSwitchingBit | kNoOverCurrentBit);
    
    assert(kUHCI_NUM_PORTS < 8);

	// Set removable port flags -- we might have an AAPL property that tells us which ports
	// are captive.  If we don't then we'll assume that all ports are removable
	appleCaptiveProperty = OSDynamicCast(OSNumber, _device->getProperty(kAppleInternalUSBDevice));
	if (appleCaptiveProperty)
		appleCaptive = appleCaptiveProperty->unsigned32BitValue();
	
    hubDesc.removablePortFlags[0] = (UInt8) appleCaptive;
    hubDesc.removablePortFlags[1] = 0xFF;

    // Adjust descriptor length to account for unused bytes
    // in removable flags and power control flags arrays.
    hubDesc.length -= (sizeof(hubDesc.removablePortFlags) - 1 +
                       sizeof(hubDesc.pwrCtlPortFlags) - 1);
    
    bcopy(&hubDesc, desc, hubDesc.length);
    
    return kIOReturnSuccess; 
}



IOReturn
AppleUSBUHCI::SetRootHubDescriptor(OSData *buffer)
{
    USBLog(3,"%s[%p]: unimplemented set root hub descriptor", getName(), this); 
    return(kIOReturnSuccess); 
}

IOReturn
AppleUSBUHCI::GetRootHubConfDescriptor(OSData *desc)
{
    IOUSBConfigurationDescriptor confDesc =
    {
        sizeof(IOUSBConfigurationDescriptor),						//UInt8 length;
        kUSBConfDesc,												//UInt8 descriptorType;
        USB_CONSTANT16(sizeof(IOUSBConfigurationDescriptor) +
                       sizeof(IOUSBInterfaceDescriptor) +
                       sizeof(IOUSBEndpointDescriptor)),			//UInt16 totalLength;
        1,															//UInt8 numInterfaces;
        1,															//UInt8 configValue;
        0,															//UInt8 configStrIndex;
        0x60,														//UInt8 attributes; self powered,
																	//      supports remote wkup
        0,															//UInt8 maxPower;
    };
    IOUSBInterfaceDescriptor intfDesc =
    {
        sizeof(IOUSBInterfaceDescriptor),	//UInt8 length;
        kUSBInterfaceDesc,					//UInt8 descriptorType;
        0,									//UInt8 interfaceNumber;
        0,									//UInt8 alternateSetting;
        1,									//UInt8 numEndpoints;
        kUSBHubClass,						//UInt8 interfaceClass;
        kUSBHubSubClass,					//UInt8 interfaceSubClass;
        0,									//UInt8 interfaceProtocol;
        0									//UInt8 interfaceStrIndex;
    };
    IOUSBEndpointDescriptor endptDesc =
    {
        sizeof(IOUSBEndpointDescriptor),	//UInt8 length;
        kUSBEndpointDesc,					//UInt8 descriptorType;
        0x81,								//UInt8  endpointAddress; In, 1
        kUSBInterrupt,						//UInt8 attributes;
        HostToUSBWord(8),					//UInt16 maxPacketSize;
        kUSBRootHubPollingRate,				//UInt8 interval;
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

IOReturn
AppleUSBUHCI::SetRootHubFeature(UInt16 wValue)
{
    switch(wValue) 
    { 
        case kUSBHubLocalPowerChangeFeature : 
            USBLog(3,"%s[%p]: unimplemented Set Power Change Feature", getName(), this); 
            break; 
 
        case kUSBHubOverCurrentChangeFeature : 
            USBLog(3,"%s[%p]: unimplemented Set Overcurrent Change Feature", getName(), this);
            /* XXX This is erroneously called by the hub device
             * setting kUSBFeatureDeviceRemoteWakeup.
             */
            break; 
 
        default: 
            USBLog(3,"%s[%p]: Unknown set hub feature (%d) in root hub", getName(), this, wValue);
            break; 
    } 
 
    /* Return success for all unimplemented features,
     * to avoid spurious error messages on the console.
     */            
    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::ClearRootHubFeature(UInt16 wValue)
{
    switch(wValue) 
    { 
        case kUSBHubLocalPowerChangeFeature : 
            USBLog(3,"%s[%p]: unimplemented Clear Power Change Feature", getName(), this); 
            // OHCIRootHubLPSChange(false);  // not implemented yet 
            break; 
 
        case kUSBHubOverCurrentChangeFeature : 
            USBLog(3,"%s[%p]: unimplemented Clear Overcurrent Change Feature", getName(), this); 
            // OHCIRootHubOCChange(false);  // not implemented yet 
            break; 
 
        default: 
            USBLog(3,"%s[%p]: Unknown hub clear (%d) in root hub", getName(), this, wValue);
            break; 
    } 
 
    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::GetRootHubStatus(IOUSBHubStatus *status)
{
    USBLog(5, "%s[%p]::GetRootHubStatus", getName(), this);
    status->statusFlags = 0;
    status->changeFlags = 0;
    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::GetRootHubPortStatus(IOUSBHubPortStatus *status, UInt16 port)
{
    UInt16 p_status;
    UInt16 r_status, r_change;
    
    USBLog(7, "AppleUSBUHCI[%p]::GetRootHubPortStatus on port %d", this, port+1);
    RHDumpPortStatus(port);
    
	// no longer do this, as the root hub can be queried even when we are suspended
	//if (_myBusState == kUSBBusStateSuspended) 
	//{
    //    return kIOReturnNotResponding;
    //}

    port--; // convert to 0-based
    if (port >= kUHCI_NUM_PORTS) 
	{
        return kIOReturnBadArgument;
    }
    p_status = ReadPortStatus(port);
	
	// check to see if suspend is on and connect is off - if so, clear the suspend
	if ((p_status & kUHCI_PORTSC_SUSPEND) && !(p_status & kUHCI_PORTSC_CCS))
	{
		USBLog(7, "AppleUSBUHCI[%p]::GetRootHubPortStatus - clearing suspend on disconnected port status[%p]", this, (void*)p_status);
		p_status &= kUHCI_PORTSC_MASK;				// make sure not to clear the change bits
		p_status &= ~kUHCI_PORTSC_SUSPEND;			// clear suspend
		WritePortStatus(port, p_status);			// this does a sync and a delay
		p_status = ReadPortStatus(port);			// reload
		USBLog(7, "AppleUSBUHCI[%p]::GetRootHubPortStatus - new port status[%p]", this, (void*)p_status);
	}
    
    /* Power is always turned on. */
    r_status = kHubPortPower;
    
    if (p_status & kUHCI_PORTSC_SUSPEND)
        r_status |= kHubPortSuspend;
    if (p_status & kUHCI_PORTSC_RESET)
        r_status |= kHubPortBeingReset;
    if (p_status & kUHCI_PORTSC_LS)
        r_status |= kHubPortLowSpeed;
    if (p_status & kUHCI_PORTSC_PED)
        r_status |= kHubPortEnabled;
    if (p_status & kUHCI_PORTSC_CCS)
        r_status |= kHubPortConnection;
    
    status->statusFlags = HostToUSBWord(r_status);
    
    /* Synthesize the change bits that are not
     * in the hardware.
     */
    r_change = r_status ^ _lastPortStatus[port];
    
    
    if (p_status & kUHCI_PORTSC_PEDC) 
	{
        r_change |= kHubPortEnabled;
    } else 
	{
        r_change &= ~kHubPortEnabled;
    }
    
    if (p_status & kUHCI_PORTSC_CSC) 
	{
        r_change |= kHubPortConnection;
    } else 
	{
        r_change &= ~kHubPortConnection;
    }
    
    /* Suspend change is only when suspend changes
        * from true to false.  It persists until
        * reset.
        */
    if ((_lastPortStatus[port] & kHubPortSuspend) && !(r_status & kHubPortSuspend)) 
	{
        USBLog(5, "AppleUSBUHCI[%p]::GetRootHubPortStatus - Turning on suspend change bit", this);
        _portSuspendChange[port] = true;
    }
    if (_portSuspendChange[port]) 
	{
        r_change |= kHubPortSuspend;
    } else {
        r_change &= ~kHubPortSuspend;
    }

    /* Synthetic reset bit. */
    if (_portWasReset[port]) 
	{
        r_change |= kHubPortBeingReset;
    }
    
    status->changeFlags = HostToUSBWord(r_change);
	if (status->changeFlags)
	{
		USBLog(5, "AppleUSBUHCI[%p]::GetRootHubPortStatus for port(%d) returned status is (%x,%x)", this, (int)port+1, r_status, r_change);
		RHDumpHubPortStatus(status);
	}
                                                                       
    _lastPortStatus[port] = r_status;
    
    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::SetRootHubPortFeature(UInt16 wValue, UInt16 port)
{
    IOReturn result = kIOReturnSuccess;
    
    USBLog(5, "%s[%p]::SetRootHubPortFeature %d %d", getName(), this, wValue, port);
    switch(wValue)
    {
        case kUSBHubPortSuspendFeature :
            result = RHSuspendPort(port, true);
            break;
            
        case kUSBHubPortResetFeature :
            result = RHResetPort(port);
            break;
            
        case kUSBHubPortEnableFeature :
            RHEnablePort(port, true);
            result = kIOReturnSuccess;
            break;
            
        case kUSBHubPortPowerFeature :
            /* Power is always on. */
            result = kIOReturnSuccess;
            break;
            
        default:
            USBLog(5, "%s[%p]: unknown feature %d", getName(), this, wValue);
            break;
    }
    
    return result;
}



IOReturn
AppleUSBUHCI::ClearRootHubPortFeature(UInt16 wValue, UInt16 port)
{
    UInt16 value;
    USBLog(5, "%s[%p]::ClearRootHubPortFeature %d %d", getName(), this, wValue, port);

    switch(wValue)
    {
        case kUSBHubPortEnableFeature :
            USBLog(5, "%s[%p]: Clear port enable", getName(), this);
            RHEnablePort(port, false);
            break;
            
        case kUSBHubPortConnectionChangeFeature :
            USBLog(5, "%s[%p]: Clear connection change", getName(), this);
            value = ReadPortStatus(port-1) & kUHCI_PORTSC_MASK;
            WritePortStatus(port-1, value | kUHCI_PORTSC_CSC);
            break;
            
        case kUSBHubPortEnableChangeFeature :
            USBLog(5, "%s[%p]: Clear port enable change", getName(), this);
            value = ReadPortStatus(port-1) & kUHCI_PORTSC_MASK;
            WritePortStatus(port-1, value | kUHCI_PORTSC_PEDC);
            break;
            
        case kUSBHubPortResetChangeFeature :
            USBLog(5, "%s[%p]: Clear port reset change", getName(), this);
            _portWasReset[port-1] = false;
            break;
            
        case kUSBHubPortSuspendFeature :
            RHSuspendPort(port, false);
            break;
            
        case kUSBHubPortSuspendChangeFeature :
            USBLog(5, "%s[%p]: Clear port suspend change", getName(), this);
            _portSuspendChange[port-1] = false;
            break;
            
#if 0
            // These will all fall through to return unsupported.
        case kUSBHubPortOverCurrentChangeFeature :
            RHResetOverCurrentChange(port);
            break;
            
        case kUSBHubPortPowerFeature :
            //status = RHPortPort(port, false);
            break;
#endif
            
        default:
            USBLog(5,"%s[%p]: clear unknown feature %d", getName(), this, wValue);
            break;
    }
    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::GetRootHubPortState(UInt8 *state, UInt16 port)
{
    USBLog(5,"%s[%p]::GetRootHubPortState", getName(), this);
    return(kIOReturnSuccess);
}



IOReturn
AppleUSBUHCI::SetHubAddress(UInt16 functionNumber)
{
    USBLog(5, "%s[%p]::SetHubAddress %d", getName(), this, functionNumber);
    // XXX Perhaps this is a good point to allocate the root hub endpoint.
    _rootFunctionNumber = functionNumber;
    return kIOReturnSuccess;
}



void
AppleUSBUHCI::UIMRootHubStatusChange(bool abort)
{
	USBLog(1, "AppleUSBUHCI[%p]::UIMRootHubStatusChange - calling obsolete method UIMRootHubStatusChange(bool)", this);
}



void
AppleUSBUHCI::UIMRootHubStatusChange(void)
{
    UInt8								bitmap, bit;
    unsigned int						i, index, move;
    IOUSBHubPortStatus					portStatus;
    
    USBLog(7, "%s[%p]::UIMRootHubStatusChange (_controllerAvailable: %d)", getName(), this, _controllerAvailable);

	if (_controllerAvailable && !_wakingFromHibernation)
	{
		// For UHCI, we first need to see if we have a pending resume
		RHCheckStatus();
		
		// Assume a byte can hold all port bits.
		assert(kUHCI_NUM_PORTS < 8);

		/*
		 * Encode the status change bitmap.  The format of the bitmap:
		 * bit0 = hub status changed
		 * bit1 = port 1 status changed
		 * bit2 = port 2 status changed
		 * ...
		 * See USB 1.0 spec section 11.8.3 for more info.
		 */

		bitmap = 0;
		bit = 0x2;
		for (i=1; i <= kUHCI_NUM_PORTS; i++) 
		{
			GetRootHubPortStatus(&portStatus, i);
			if (portStatus.changeFlags != 0) 
			{
				UInt64			elapsedTime;
				uint64_t		currentTime;
				
				USBLog(5, "AppleUSBUHCI[%p]::UIMRootHubStatusChange  Port %d hub flags:", this, i);
				RHDumpHubPortStatus(&portStatus);

				bitmap |= bit;
				
				// If this port has seen a recovery attempt (see below) already, check to see what the current time is and if it's > than 2 seconds since the port recovery, then 'forget" about it
				currentTime = mach_absolute_time();
				SUB_ABSOLUTETIME(&currentTime, &_portRecoveryTime[i-1] );
				absolutetime_to_nanoseconds(*(AbsoluteTime *)&currentTime, &elapsedTime);
				elapsedTime /= 1000000000;									// Convert to seconds from nanoseconds
				
				if ( _previousPortRecoveryAttempted[i-1] && (elapsedTime >= kUHCITimeoutForPortRecovery) )
				{
					USBLog(2, "AppleUSBUHCI[%p]::UIMRootHubStatusChange  Forgetting about our portRecovery state since the last change occurred %qd seconds ago", this, elapsedTime);
					_previousPortRecoveryAttempted[i-1] = false;
				}
				
				//  If this port has a PED (port enable change) AND the current status is PortPower and Port Connection (which indicates that a condition
				//  on the bus caused the controller to disable the port) then we need to see if we should attempt to re-enable the port w/out calling the
				//  hub driver.  We will do this ONLY if the previous root hub status change for this port did NOT attempt this recovery -- we only try once
				
				if ( !_previousPortRecoveryAttempted[i-1] )
				{
					USBLog(7, "AppleUSBUHCI[%p]::UIMRootHubStatusChange  Port %d had a change: 0x%x", this, i, portStatus.changeFlags);
					if ( !(portStatus.statusFlags & kHubPortEnabled)				// if we are not presently enabled
						&& (portStatus.changeFlags & kHubPortEnabled)				// and we were previously enabled
						&& (portStatus.statusFlags & kHubPortConnection)			// and we are presently connected
						&& !(portStatus.changeFlags & kHubPortConnection)			// and the connection has not recently changed (i.e. quick disconnect-connect)
						&& (portStatus.statusFlags & kHubPortPower) )				// and the power is on
					{
						// Indicate that we are attempting a recovery
						_previousPortRecoveryAttempted[i-1] = true;
						currentTime = mach_absolute_time();
						_portRecoveryTime[i-1] = *(AbsoluteTime*)&currentTime;
						
						USBLog(1, "AppleUSBUHCI[%p]::UIMRootHubStatusChange  Port %d attempting to enable a disabled port to work around a fickle UHCI controller", this, i);
						USBTrace( kUSBTUHCI, kTPUHCIRootHubStatusChange, (uintptr_t)this, portStatus.statusFlags, portStatus.changeFlags, i );
						
						RHEnablePort(i, true);
						
						// Clear the bitmap
						bitmap &= ~bit;
					}
				}
				else
				{
					// If this is just the notification that the port has been enabled, then don't reset our previousPortRecoveryAttempt
					if ( (portStatus.changeFlags & kHubPortEnabled) and (portStatus.statusFlags & kHubPortConnection) and (portStatus.statusFlags & kHubPortPower) and (portStatus.statusFlags & kHubPortEnabled) )
					{
						USBLog(2, "AppleUSBUHCI[%p]::UIMRootHubStatusChange  Port %d had a change but it's just the port enabled notification", this, i);
					}
					else
					{
						USBLog(2, "AppleUSBUHCI[%p]::UIMRootHubStatusChange  Port %d had a change but last time we attempted a recovery, so not attempting again", this, i);
						_previousPortRecoveryAttempted[i-1] = false;
					}
				}
			}
			
			bit <<= 1;

			// Don't clear status bits until explicitly told to.

		}
		if (bitmap)
		{
			USBLog(5, "AppleUSBUHCI[%p]::UIMRootHubStatusChange  RH status bitmap = %x",  this, bitmap);
		}
		_rootHubStatusChangedBitmap = bitmap;
	}
    
    // Bitmap is only one byte, so it doesn't need swapping.
}



IOReturn 
AppleUSBUHCI::GetRootHubStringDescriptor(UInt8	index, OSData *desc)
{
    // The following strings are in Unicode format
    //
    UInt8 productName[] = {
        0,			// Length Byte
        kUSBStringDesc,	// Descriptor type
        0x55, 0x00,     // "U"
        0x48, 0x00,     // "H"
        0x43, 0x00,     // "C"
        0x49, 0x00,     // "I"
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
        0x53, 0x00,     // "S"
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

// ========================================================================
//
// * RootHubAbortInterruptRead
// * Abort a queued up read.
//
// ========================================================================
//
IOReturn
AppleUSBUHCI::RootHubAbortInterruptRead()
{
    int										i;
	IOUSBRootHubInterruptTransaction		xaction;
	
	if ( _errataBits & kErrataUHCISupportsResumeDetectOnConnect )
	{
		USBLog(6, "AppleUSBUHCI[%p]::RootHubAbortInterruptRead,  controller supports kErrataUHCISupportsResumeDetectOnConnect", this);
		return super::RootHubAbortInterruptRead();
	}
	
	// For UHCI controllers that do not generate a resume detect interrupt when we plug something in, we need to throttle down the timer
	// So that we can save power.  We do that by cancelling the current timer, and then starting a new one with the new timeout.  When we 
	// then start re-issuing the timer when the device is active, we will need to make sure that we use the usual polling rate
	
	USBLog(6, "AppleUSBUHCI[%p]::RootHubAbortInterruptRead, throttling down the timer to %d ms", this, (uint32_t)_rootHubPollingRate32 * 30);
	
	// Stop the timer and restart it with a polling rate of 30x (960ms)
	RootHubStopTimer();
	
	// This will set _rootHubPollingRate32 to this new value
	RootHubStartTimer32(_rootHubPollingRate32 * 30);
	
	xaction = _outstandingRHTrans[0];
	if (xaction.completion.action)
	{
		// move all other transactions down the queue
		for (i = 0; i < (kIOUSBMaxRootHubTransactions-1); i++)
		{
			_outstandingRHTrans[i] = _outstandingRHTrans[i+1];
		}
		_outstandingRHTrans[kIOUSBMaxRootHubTransactions-1].completion.action = NULL;
		Complete(xaction.completion, kIOReturnAborted, xaction.bufLen);
	}
	return kIOReturnSuccess;
}


// ========================================================================
#pragma mark Internal root hub methods
// ========================================================================


AbsoluteTime
AppleUSBUHCI::RHLastPortStatusChanged()
{
    return _rhChangeTime;
}



bool
AppleUSBUHCI::RHAreAllPortsDisconnectedOrSuspended( void )
{
    int i;
    UInt16 status;
    bool result = true;
    
    for (i=0; i<kUHCI_NUM_PORTS; i++) 
	{
        status = ReadPortStatus(i);
        if ((status & kUHCI_PORTSC_CCS) && !(status & kUHCI_PORTSC_SUSPEND))
		{
            result = false;
            break;
        }
    }

	// If we have pending bulk or control transactions, then force the result to false
	if ( result )
	{
		if ( _controlBulkTransactionsOut != 0 )
		{
            USBLog(2, "AppleUSBUHCI[%p]::RHAreAllPortsDisconnectedOrSuspended  everything disconnected, but %d control/bulk transactions are pending. ", this, (uint32_t)_controlBulkTransactionsOut);
			result = false;
		}
	}
	
    USBLog(result ? 2 : 6, "AppleUSBUHCI[%p]::RHAreAllPortsDisconnectedOrSuspended returns %d", this, result);
    return result;
}



IOReturn 
AppleUSBUHCI::RHDeleteEndpoint (short endpointNumber, short direction)
{
    return RHAbortEndpoint(endpointNumber, direction);
}



IOReturn 
AppleUSBUHCI::RHAbortEndpoint (short endpointNumber, short direction)
{
    
    if (endpointNumber == 1) 
	{
        // Interrupt endpoint
        if(direction != kUSBIn)
		{
            USBLog(5, "%s[%p]::RHAbortEndpoint - Root hub wrong direction Int pipe %d", getName(), this, direction);
            return kIOReturnBadArgument;
        }

        USBLog(3, "AppleUSBUHCI[%p]::RHAbortEndpoint - Interrupt pipe -  noting status change", this);
		RootHubAbortInterruptRead();
    } else 
	{
        // Abort Control endpoint
        USBLog(3, "AppleUSBUHCI[%p]::RHAbortEndpoint - Control pipe - NOP", this);
    }
    
    return kIOReturnSuccess;
}



void
AppleUSBUHCI::RHCheckStatus()
{
    int						i;
    UInt16					status;
	
   /* Read port status registers.
    * Check for resumed ports.
    * If the status changed on either, call the
    * port status changed method.
    */
    for (i=0; i<kUHCI_NUM_PORTS; i++) 
	{
		if (!_rhPortBeingResumed[i])								// only check ports which are not being resumed
		{
			status = ReadPortStatus(i);
			if (status & kUHCI_PORTSC_RD) 
			{
				if (_myPowerState >= kUSBPowerStateLowPower)
				{
					if (_myPowerState == kUSBPowerStateLowPower)
						EnsureUsability();
					USBLog(3, "AppleUSBUHCI[%p]::RHCheckStatus - resume detected on port %d, spawning thread to resume", this, i+1);
					_rhPortBeingResumed[i] = true;
					thread_call_enter1(_rhResumePortTimerThread[i], (void*)(i+1));
				}
				else
				{
					USBLog(3, "AppleUSBUHCI[%p]::RHCheckStatus - resume detected while not below low power state, not changing bits until we are back on", this);
				}
			}
		}
    }
}



void
AppleUSBUHCI::RHEnablePort(int port, bool enable)
{
    UInt16 value;

    // USBLog(5, "%s[%p]::RHEnablePort %d %d", getName(), this, port, enable);
    port--; // convert 1-based to 0-based.
    value = ReadPortStatus(port) & kUHCI_PORTSC_MASK;
    USBLog(3, "%s[%p]::RHEnablePort port: %d enable: %d PortSC: 0x%x", getName(), this, port+1, enable, value);
	USBLog(2, "AppleUSBUHCI[%p]::RHEnablePort  (CMD:%p STS:%p INTR:%p PORTSC1:%p PORTSC2:%p FRBASEADDR:%p FRNUM:%p, SOFMOD:%p, ConfigCMD:%p)", this, (void*)ioRead16(kUHCI_CMD), (void*)ioRead16(kUHCI_STS), (void*)ioRead16(kUHCI_INTR), (void*)ioRead16(kUHCI_PORTSC1), (void*)ioRead16(kUHCI_PORTSC2), (void*)ioRead32(kUHCI_FRBASEADDR),  
		   (void*)ioRead32(kUHCI_FRNUM),  (void*)ioRead32(kUHCI_SOFMOD), (void*)_device->configRead16(kIOPCIConfigCommand));
   if (enable) 
	{
        value |= kUHCI_PORTSC_PED;
    } else 
	{
        value &= ~kUHCI_PORTSC_PED;
    }
    WritePortStatus(port, value);
}



IOReturn
AppleUSBUHCI::RHSuspendPort(int port, bool suspended)
{
    UInt16 cmd, value;
    
    USBLog(3, "AppleUSBUHCI[%p]::RHSuspendPort %d (%s) _rhPortBeingResumed[%d](%s)", this, port, suspended ? "SUSPEND" : "RESUME", (int)port-1, _rhPortBeingResumed[port-1] ? "true" : "false");
	showRegisters(7, "RHSuspendPort");
    port--; // convert 1-based to 0-based.

	if (_rhPortBeingResumed[port])
	{
		if (!suspended)
		{
			USBLog(3, "AppleUSBUHCI[%p]::RHSuspendPort - resume on port (%d) already being resumed - gracefully ignoring", this, (int)port+1);
			return kIOReturnSuccess;
		}
		USBLog(1, "AppleUSBUHCI[%p]::RHSuspendPort - trying to suspend port (%d) which is being resumed - UNEXPECTED", this, (int)port+1);
		USBTrace( kUSBTUHCI, kTPUHCIRHSuspendPort, (uintptr_t)this, (int)port+1, 0, 0);
	}
	
    cmd = ioRead16(kUHCI_CMD);
    value = ReadPortStatus(port) & kUHCI_PORTSC_MASK;
    
    if (suspended) 
	{
        value |= kUHCI_PORTSC_SUSPEND;
        value &= ~kUHCI_PORTSC_RD;
    } else 
	{
        if (cmd & kUHCI_CMD_EGSM) 
		{
            /* Can't un-suspend a port during global suspend. */
            USBError(1, "AppleUSBUHCI[%p]: attempt to resume during global suspend", this);
            return kIOReturnError;
        }
        value |= (kUHCI_PORTSC_SUSPEND | kUHCI_PORTSC_RD);
    }
    // Always enable the port also.
    value |= kUHCI_PORTSC_PED;

    USBLog(5, "AppleUSBUHCI[%p]: writing (%p) to port control", this, (void*)value);
    
    WritePortStatus(port, value);
    
    if (suspended) 
	{
        /* Suspending.
         * Sleep for 3ms to ensure nothing goes out on the bus
         * until devices are suspended.
         */
        IOSleep(3);
        
    } else 
	{
        // Resuming
		USBLog(5,"AppleUSBUHCI[%p]::RHSuspendPort - resuming port %d, calling out to timer", this, (int)port+1);
		_rhPortBeingResumed[port] = true;
		thread_call_enter1(_rhResumePortTimerThread[port], (void*)(port+1));
    }

    USBLog(5, "AppleUSBUHCI[%p]::RHSuspendPort %d (%s) calling UIMRootHubStatusChange", this, port+1, suspended ? "SUSPEND" : "RESUME");

    UIMRootHubStatusChange();        
    
    USBLog(5, "AppleUSBUHCI[%p]::RHSuspendPort %d (%s) DONE", this, port+1, suspended ? "SUSPEND" : "RESUME");

    return kIOReturnSuccess;
}



// Reset and enable the port
IOReturn
AppleUSBUHCI::RHResetPort(int port)
{
    UInt16 value;
    int i;
    
    USBLog(3, "%s[%p]::RHResetPort %d", getName(), this, port);
    port--; // convert 1-based to 0-based.

    value = ReadPortStatus(port) & kUHCI_PORTSC_MASK;
    WritePortStatus(port, value | kUHCI_PORTSC_RESET);
    
    /* Assert RESET for 50ms */
    IOSleep(50);
    
    value = ReadPortStatus(port) & kUHCI_PORTSC_MASK;
    WritePortStatus(port, value & ~kUHCI_PORTSC_RESET);
    
    IODelay(10);
    
    value = ReadPortStatus(port) & kUHCI_PORTSC_MASK;
    WritePortStatus(port, value | kUHCI_PORTSC_PED);
    
    for (i=10; i>0; i--) 
	{
        IOSleep(10);
        
        value = ReadPortStatus(port);
        
        if ((value & kUHCI_PORTSC_CCS) == 0) 
		{
            /* No device connected; don't enter reset state. */
            //USBLog(5, "%s[%p]: no device connected, not entering reset state");
            return kIOReturnNotResponding;
            break;
        }
        
        if (value & (kUHCI_PORTSC_PEDC | kUHCI_PORTSC_CSC)) 
		{
            /* Change bits detected. Clear them and continue waiting. */
            WritePortStatus(port, (value & kUHCI_PORTSC_MASK) | (kUHCI_PORTSC_PEDC | kUHCI_PORTSC_CSC));
            continue;
        }
        
        if (value & kUHCI_PORTSC_PED) 
		{
            /* Port successfully enabled. */
            break;
        }
        
    }
    
    if (i == 0) 
	{
        USBLog(5, "%s[%p]: reset port FAILED", getName(), this);
        return kIOReturnNotResponding;
    }
    
    // Remember that we were reset
    _portWasReset[port] = true;
    
    USBLog(5, "%s[%p]: reset port succeeded", getName(), this);
    return kIOReturnSuccess;
}

// Reset and enable the port
IOReturn
AppleUSBUHCI::RHHoldPortReset(int port)
{
    UInt16 value;
    int i;
    
    USBLog(1, "%s[%p]::RHHoldPortReset %d", getName(), this, port);
	USBTrace( kUSBTUHCI, kTPUHCIRHHoldPortReset, (uintptr_t)this, (int)port, 0, 0);
	
    port--; // convert 1-based to 0-based.

    value = ReadPortStatus(port) & kUHCI_PORTSC_MASK;
    WritePortStatus(port, value | kUHCI_PORTSC_RESET);
    
    return kIOReturnSuccess;
}


/* ==== debugging ==== */
void
AppleUSBUHCI::RHDumpPortStatus(int port)
{
    UInt16 value;
    char buf[64];
    static struct {
        UInt16 mask;
        const char *string;
    } strings[] = {
    {kUHCI_PORTSC_SUSPEND, "SUSPEND "},
    {kUHCI_PORTSC_RESET, "RESET "},
    {kUHCI_PORTSC_LS, "LS "},
    {kUHCI_PORTSC_RD, "RD "},
    {kUHCI_PORTSC_LINE0, "LINE0 "},
    {kUHCI_PORTSC_LINE1, "LINE1 "},
    {kUHCI_PORTSC_PEDC, "PEDC "},
    {kUHCI_PORTSC_PED, "PED "},
    {kUHCI_PORTSC_CSC, "CSC "},
    {kUHCI_PORTSC_CCS, "CCS"},
    {0,0}
    };
    int i;
    
    port--; // convert 1-based to 0-based.
    buf[0] = '\0';
    value = ReadPortStatus(port);
    for (i=0; strings[i].string != 0; i++) 
	{
        if ((value & strings[i].mask) != 0) 
		{
            strlcat(buf, strings[i].string, sizeof(buf));
        }
    }
    USBLog(7, "%s[%p]: Port %d: status %x %s", getName(), this, port+1, value, buf);
}



void
AppleUSBUHCI::RHDumpHubPortStatus(IOUSBHubPortStatus *status)
{
    UInt16 value;
    char buf[128];
    static struct {
        UInt16 mask;
        const char *string;
    } strings[] = {
    {kHubPortConnection, "kHubPortConnection "},
    {kHubPortEnabled,    "kHubPortEnabled "},
    {kHubPortSuspend,    "kHubPortSuspend "},
    {kHubPortOverCurrent,"kHubPortOverCurrent "},
    {kHubPortBeingReset, "kHubPortBeingReset "},
    {kHubPortPower,      "kHubPortPower "},
    {kHubPortLowSpeed,   "kHubPortLowSpeed "},
    {kHubPortHighSpeed,  "kHubPortHighSpeed "},
    {kHubPortTestMode,   "kHubPortTestMode "},
    {kHubPortIndicator,  "kHubPortIndicator "},
    {0,0}
    };
    int i;
    
    buf[0] = '\0';
    value = USBToHostWord(status->statusFlags);
    for (i=0; strings[i].string != 0; i++) 
	{
        if ((value & strings[i].mask) != 0) 
		{
            strlcat(buf, strings[i].string, sizeof(buf));
        }
    }
    USBLog(5, "%s[%p]: Hub port status: %s", getName(), this, buf);
    buf[0] = '\0';
    value = USBToHostWord(status->changeFlags);
    for (i=0; strings[i].string != 0; i++) 
	{
        if ((value & strings[i].mask) != 0) 
		{
            strlcat(buf, strings[i].string, sizeof(buf));
        }
    }
    USBLog(5, "%s[%p]: Hub port change: %s", getName(), this, buf);
    
}


// these are for handling a root hub resume without hanging out in the WL for 20 ms
// static
void
AppleUSBUHCI::RHResumePortTimerEntry(OSObject *target, thread_call_param_t port)
{
    AppleUSBUHCI *me = OSDynamicCast(AppleUSBUHCI, target);
	if (!me)
		return;

	me->RHResumePortTimer((uintptr_t)port);
}



void
AppleUSBUHCI::RHResumePortTimer(UInt32 port)
{
	// we are responsible for terminating the resume on a root hub port ourselves
	// and we used to do it inside of the workloop. now we do the timing part of it
	// outside of the WL
	if (!_commandGate)
		return;
	
	USBLog(5, "AppleUSBUHCI[%p]::RHResumePortTimer - timing the resume for port %d", this, (int)port);
	IOSleep(20);								// wait 20 ms for the resume to complete
	USBLog(6, "AppleUSBUHCI[%p]::RHResumePortTimer - Host controller resume about to finish - calling EnsureUsability", this);
	EnsureUsability();		

	_commandGate->runAction(RHResumePortCompletionEntry, (void*)port);
}



// static
IOReturn
AppleUSBUHCI::RHResumePortCompletionEntry(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    AppleUSBUHCI				*me = OSDynamicCast(AppleUSBUHCI, target);
    UInt32						port = (uintptr_t)param1;
	
	if (!me)
		return kIOReturnInternalError;
	
	return me->RHResumePortCompletion(port);
}



IOReturn			
AppleUSBUHCI::RHResumePortCompletion(UInt32 port)
{
	UInt16			value;
	
	USBLog(5, "AppleUSBUHCI[%p]::RHResumePortCompletion - finishing resume on port %d", this, (int)port);
	if (!_rhPortBeingResumed[port-1])
	{
		USBLog(1, "AppleUSBUHCI[%p]::RHResumePortCompletion - port %d does not appear to be resuming!", this, (int)port);
		USBTrace( kUSBTUHCI, kTPUHCIRHResumePortCompletion, (uintptr_t)this, (int)port, 0, kIOReturnInternalError );
		return kIOReturnInternalError;
	}
	
	if (!_controllerAvailable)
	{
		USBLog(5, "AppleUSBEHCI[%p]::RHResumePortCompletion - cannot finish resume on port %d because the controller is unavailable", this, (int)port);
		_rhPortBeingResumed[port-1] = false;
		return kIOReturnInternalError;
	}
	
	value = ReadPortStatus(port-1) & kUHCI_PORTSC_MASK;
	value &= ~(kUHCI_PORTSC_RD | kUHCI_PORTSC_SUSPEND);
	USBLog(5, "AppleUSBUHCI[%p]: de-asserting resume signal by writing (%p)", this, (void*)value);
	WritePortStatus(port-1, value);
	IOSync();
	IOSleep(2);																	// allow it to kick in

	_rhPortBeingResumed[port-1] = false;
	_portSuspendChange[port-1] = true;
	return kIOReturnSuccess;
}




