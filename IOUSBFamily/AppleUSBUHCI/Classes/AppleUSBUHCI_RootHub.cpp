/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include "AppleUSBUHCI.h"



enum {
    kAppleVendorID      = 0x05AC,	/* Assigned by USB-if*/
    kPrdRootHubApple	= 0x8005	/* Apple ASIC root hub*/
};


// ========================================================================
#pragma mark Public root hub methods
// ========================================================================


IOReturn
AppleUSBUHCI::GetRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc)
{
    IOUSBDeviceDescriptor newDesc = 	{
        sizeof(IOUSBDeviceDescriptor),	// UInt8 length;
        kUSBDeviceDesc,			// UInt8 descType;
        USB_CONSTANT16(kUSBRel10),	// UInt16 usbRel;
        kUSBHubClass,			// UInt8 class;
        kUSBHubSubClass,		// UInt8 subClass;
        0,				// UInt8 protocol;
        8,				// UInt8 maxPacketSize;
        USB_CONSTANT16(kAppleVendorID),	// UInt16 vendor:  Use the Apple Vendor ID from USB-IF
        USB_CONSTANT16(kPrdRootHubApple),	// UInt16 product:  All our root hubs are the same
        USB_CONSTANT16(0x0110),		// UInt16 devRel: Supports USB 1.1
        0,				// UInt8 manuIdx;
        1,				// UInt8 prodIdx;
        0,				// UInt8 serialIdx;
        1				// UInt8 numConf;
    };

    if (!desc)
        return(kIOReturnNoMemory);

    bcopy(&newDesc, desc, newDesc.bLength);

    return kIOReturnSuccess;
}

IOReturn
AppleUSBUHCI::GetRootHubDescriptor(IOUSBHubDescriptor *desc)
{
    IOUSBHubDescriptor hubDesc;
    
    if (!desc) 
        return(kIOReturnNoMemory); 
    
    hubDesc.length = sizeof(IOUSBHubDescriptor); 
    hubDesc.hubType = kUSBHubDescriptorType; 
    hubDesc.numPorts = kUHCI_NUM_PORTS;
    hubDesc.powerOnToGood = 50; /* 100ms */
    hubDesc.hubCurrent = 0;
    // XXX for now do not support overcurrent, although PIIX chips do support it
    hubDesc.characteristics = HostToUSBWord(kNoPowerSwitchingBit | kNoOverCurrentBit);
    
    // All ports are removable;
    // no ports are switchable, but the USB 1.1 spec says
    // that the switchable bits should all be set to 1.
    assert(kUHCI_NUM_PORTS < 8);
    hubDesc.removablePortFlags[0] = 0;
    hubDesc.removablePortFlags[1] = 0xFF;

    // Adjust descriptor length to account for unused bytes
    // in removable flags and power control flags arrays.
    hubDesc.length -= (sizeof(hubDesc.removablePortFlags) - 1 +
                       sizeof(hubDesc.pwrCtlPortFlags) - 1);
    
    bcopy(&hubDesc, desc, hubDesc.length);
    
    return(kIOReturnSuccess); 
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
        1,                      //UInt8 interfaceProtocol;
        0                       //UInt8 interfaceStrIndex;
    };
    IOUSBEndpointDescriptor endptDesc =
    {
        sizeof(IOUSBEndpointDescriptor),//UInt8 length;
        kUSBEndpointDesc,       //UInt8 descriptorType;
        0x81,                   //UInt8  endpointAddress; In, 1
        kUSBInterrupt,          //UInt8 attributes;
        HostToUSBWord(8),      	//UInt16 maxPacketSize;
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
    IOReturn status;
    switch(wValue) 
    { 
        case kUSBHubLocalPowerChangeFeature : 
            USBLog(3,"%s[%p]: unimplemented Clear Power Change Feature", getName(), this); 
            // OHCIRootHubLPSChange(false);  // not implemented yet 
            status = kIOReturnUnsupported;
            break; 
 
        case kUSBHubOverCurrentChangeFeature : 
            USBLog(3,"%s[%p]: unimplemented Clear Overcurrent Change Feature", getName(), this); 
            // OHCIRootHubOCChange(false);  // not implemented yet 
            status = kIOReturnUnsupported;
            break; 
 
        default: 
            USBLog(3,"%s[%p]: Unknown hub clear (%d) in root hub", getName(), this, wValue);
            status = kIOReturnUnsupported;
            break; 
    } 
 
    return(status); 
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
    
    USBLog(5, "%s[%p]::GetRootHubPortStatus %d", getName(), this, port);
    RHDumpPortStatus(port);
    
    if (_powerLevel == kUHCIPowerLevelSuspend) {
        return kIOReturnNotResponding;
    }

    port--; // convert to 0-based
    if (port >= kUHCI_NUM_PORTS) {
        return kIOReturnBadArgument;
    }
    p_status = ReadPortStatus(port);
    
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
    
    
    if (p_status & kUHCI_PORTSC_PEDC) {
        r_change |= kHubPortEnabled;
    } else {
        r_change &= ~kHubPortEnabled;
    }
    
    if (p_status & kUHCI_PORTSC_CSC) {
        r_change |= kHubPortConnection;
    } else {
        r_change &= ~kHubPortConnection;
    }
    
    /* Suspend change is only when suspend changes
        * from true to false.  It persists until
        * reset.
        */
    if ((_lastPortStatus[port] & kHubPortSuspend) &&
        !(r_status & kHubPortSuspend)) {
        USBLog(5, "%s[%p]: Turning on suspend change bit", getName(), this);
        _portSuspendChange[port] = true;
    }
    if (_portSuspendChange[port]) {
        r_change |= kHubPortSuspend;
    } else {
        r_change &= ~kHubPortSuspend;
    }

    /* Synthetic reset bit. */
    if (_portWasReset[port]) {
        r_change |= kHubPortBeingReset;
    }
    
    status->changeFlags = HostToUSBWord(r_change);
    USBLog(5, "%s[%p]: returned status is (%x,%x)", getName(), this, r_status, r_change);
    RHDumpHubPortStatus(status);
                                                                       
    _lastPortStatus[port] = r_status;
    
    return kIOReturnSuccess;
}

IOReturn
AppleUSBUHCI::SetRootHubPortFeature(UInt16 wValue, UInt16 port)
{
    IOReturn result;
    
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
            result = kIOReturnUnsupported;
            break;
    }
    
    return result;
}

IOReturn
AppleUSBUHCI::ClearRootHubPortFeature(UInt16 wValue, UInt16 port)
{
    IOReturn result = kIOReturnUnsupported;
    UInt16 value;
    USBLog(5, "%s[%p]::ClearRootHubPortFeature %d %d", getName(), this, wValue, port);

    switch(wValue)
    {
        case kUSBHubPortEnableFeature :
            USBLog(5, "%s[%p]: Clear port enable", getName(), this);
            RHEnablePort(port, false);
            result = kIOReturnSuccess;
            break;
            
        case kUSBHubPortConnectionChangeFeature :
            USBLog(5, "%s[%p]: Clear connection change", getName(), this);
            value = ReadPortStatus(port-1) & kUHCI_PORTSC_MASK;
            WritePortStatus(port-1, value | kUHCI_PORTSC_CSC);
            result = kIOReturnSuccess;
            break;
            
        case kUSBHubPortEnableChangeFeature :
            USBLog(5, "%s[%p]: Clear port enable change", getName(), this);
            value = ReadPortStatus(port-1) & kUHCI_PORTSC_MASK;
            WritePortStatus(port-1, value | kUHCI_PORTSC_PEDC);
            result = kIOReturnSuccess;
            break;
            
        case kUSBHubPortResetChangeFeature :
            USBLog(5, "%s[%p]: Clear port reset change", getName(), this);
            _portWasReset[port-1] = false;
            result = kIOReturnSuccess;
            break;
            
        case kUSBHubPortSuspendFeature :
            result = RHSuspendPort(port, false);
            break;
            
        case kUSBHubPortSuspendChangeFeature :
            USBLog(5, "%s[%p]: Clear port suspend change", getName(), this);
            _portSuspendChange[port-1] = false;
            result = kIOReturnSuccess;
            break;
            
#if 0
            // These will all fall through to return unsupported.
        case kUSBHubPortOverCurrentChangeFeature :
            result = RHResetOverCurrentChange(port);
            break;
            
        case kUSBHubPortPowerFeature :
            //status = RHPortPort(port, false);
            result = kIOReturnUnsupported;
            break;
#endif
            
        default:
            USBLog(5,"%s[%p]: clear unknown feature %d", getName(), this, wValue);
            result = kIOReturnUnsupported;
            break;
    }
    return result;
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
    _rhEndpoint->functionNumber = functionNumber;
    return kIOReturnSuccess;
}

void
AppleUSBUHCI::UIMRootHubStatusChange(void)
{
    UIMRootHubStatusChange(false);
}

/* The root hub status changed, so we must complete queued interrupt transactions
 * on the root hub.
 * If abort is true, then dequeue the transactions.
 * XXX Right now one status change will complete all transactions;
 * is that correct?
 */
void
AppleUSBUHCI::UIMRootHubStatusChange(bool abort)
{
    UHCITransaction *tp;
    UInt8 bitmap, bit;
    int i;
    IOUSBHubPortStatus portStatus;
    
    USBLog(5, "%s[%p]::UIMRootHubStatusChange %d", getName(), this, abort);
    
    /* Update timestamp. */
    clock_get_uptime(&_rhChangeTime);
    
    if (_powerLevel == kUHCIPowerLevelIdleSuspend) {
        setPowerState(kUHCIPowerLevelRunning, this);
    }
    
    /* Assume a byte can hold all port bits. */
    assert(kUHCI_NUM_PORTS < 8);

    if (queue_empty(&_rhIntrTransactions)) {
        /* No one is listening; nothing to do here. */
        return;
    }
    
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
    for (i=1; i <= kUHCI_NUM_PORTS; i++) {
        GetRootHubPortStatus(&portStatus, i);
        USBLog(5, "%s[%p]: Port %d hub flags:", getName(), this, i);
        RHDumpHubPortStatus(&portStatus);
        if (portStatus.changeFlags != 0) {
            bitmap |= bit;
        }
        bit <<= 1;

        // Don't clear status bits until explicitly told to.
    }
    USBLog(5, "%s[%p]: RH status bitmap = %x", getName(), this, bitmap);
    
    // Bitmap is only one byte, so it doesn't need swapping.
    
    while (!queue_empty(&_rhIntrTransactions)) {
        
        queue_remove_first(&_rhIntrTransactions, tp, UHCITransaction *, active_chain);
        
        USBLog(5, "%s[%p]: completing RH interrupt transaction %p", getName(), this, tp);

        tp->buf->writeBytes(0, &bitmap, sizeof(bitmap));
        
        queue_remove(&tp->endpoint->activeTransactions, tp, UHCITransaction *, endpoint_chain);
    
        Complete(tp->completion, (abort ? kIOReturnAborted : kIOReturnSuccess), tp->bufLen - sizeof(bitmap));
        
        FreeTransaction(tp);
    }
}

IOReturn AppleUSBUHCI::GetRootHubStringDescriptor(UInt8	index, OSData *desc)
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


// ========================================================================
#pragma mark Internal root hub methods
// ========================================================================


AbsoluteTime
AppleUSBUHCI::RHLastPortStatusChanged()
{
    return _rhChangeTime;
}

bool
AppleUSBUHCI::RHAreAllPortsDisconnected( void )
{
    int i;
    UInt16 status;
    bool result = true;
    
    for (i=0; i<kUHCI_NUM_PORTS; i++) {
        status = ReadPortStatus(i);
        if (status & kUHCI_PORTSC_CCS) {
            result = false;
            break;
        }
    }
    USBLog(5, "%s[%p]::RootHubAreAllPortsDisconnected returns %d", getName(), this, result);
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
    //int i;
    
    if (endpointNumber == 1) {
        /* Interrupt endpoint. */
        if(direction != kUSBIn) {
            USBLog(5, "%s[%p]::RHAbortEndpoint - Root hub wrong direction Int pipe %d",
                   getName(), this, direction);
            return kIOReturnBadArgument;
        }
        USBLog(5, "%s[%p]::RHAbortEndpoint aborting interrupt transactions",
               getName(), this);
        
        /* Turn off periodic events. */
        _rhEndpoint->pollingRate = 0;
        _rhTimer->cancelTimeout();
        USBLog(5, "%s[%p]: turning off periodic events", getName(), this);
        
        UIMRootHubStatusChange(true);
    } else {
        /* Control endpoint. */
        USBLog(5, "%s[%p]::SimulateEndpointAbort Root hub aborting control pipe",
               getName(), this);
        
        UIMRootHubStatusChange(false);
    }
    
    return kIOReturnSuccess;
}

IOReturn
AppleUSBUHCI::RHCreateInterruptEndpoint(
                                        short				endpointNumber,
                                        UInt8				direction,
                                        short				speed,
                                        UInt16				maxPacketSize,
                                        short				pollingRate)
{
    USBLog(3, "%s[%p]::RHCreateInterruptEndpoint rate %d", getName(), this, pollingRate);
    
    _rhEndpoint->pollingRate = pollingRate;
    return kIOReturnSuccess;
}

IOReturn
AppleUSBUHCI::RHCreateInterruptTransfer(IOUSBCommand* command)
{
    UHCITransaction *tp;
    
    USBLog(5, "%s[%p]::RHCreateInterruptTransfer", getName(), this);
    
    if (_rhEndpoint->pollingRate == 0) {
        return kIOReturnBadArgument;
    }
    
    tp = AllocTransaction(_rhEndpoint);
    if (tp == NULL) {
        return kIOReturnNoMemory;
    }
    
    tp->buf = command->GetBuffer();
    tp->bufLen = command->GetReqCount();
    tp->completion = command->GetUSLCompletion();
    tp->nCompletions = 1;
    queue_enter(&_rhIntrTransactions, tp, UHCITransaction *, active_chain);
    
    queue_enter(&_rhEndpoint->activeTransactions, tp, UHCITransaction *, endpoint_chain);
    
    _rhTimer->setTimeoutMS(_rhEndpoint->pollingRate);
        
    return kIOReturnSuccess;
}

void
AppleUSBUHCI::RHCheckStatus()
{
    int i;
    UInt16 status;
    bool change = false;
        
    
    /* Read port status registers.
    * Check for resumed ports.
    * If the status changed on either, call the
    * port status changed method.
    */
    for (i=0; i<kUHCI_NUM_PORTS; i++) {
        status = ReadPortStatus(i);
        if (status & kUHCI_PORTSC_RD) {
            USBLog(3, "%s[%p]: resume detected on port %d, resuming", getName(), this, i);
            RHSuspendPort(i+1, false);
            change = true;
        } else if (status & (kUHCI_PORTSC_PEDC|kUHCI_PORTSC_CSC|kUHCI_PORTSC_RD)) {
            USBLog(5,"%s[%p]: Status changed on port %d", getName(), this, i);
            change = true;
        } else if (_portWasReset[i]) {
            change = true;
        }
    }
    
    if (change) {
        USBLog(5, "%s[%p]: Status change for root hub", getName(), this);
        UIMRootHubStatusChange(false);
    }    
}


void 
AppleUSBUHCI::RHTimerFired(OSObject *owner, IOTimerEventSource *sender)
{
    AppleUSBUHCI *myself;
    UInt32 period;
    
    myself = OSDynamicCast(AppleUSBUHCI, owner);
    if (myself == NULL) {
        return;
    }
    
    myself->RHCheckStatus();

    period = myself->_rhEndpoint->pollingRate;
    if (period)
        myself->_rhTimer->setTimeoutMS(period);
}

void
AppleUSBUHCI::RHEnablePort(int port, bool enable)
{
    UInt16 value;

    USBLog(5, "%s[%p]::RHEnablePort %d %d", getName(), this, port, enable);
    port--; // convert 1-based to 0-based.
    value = ReadPortStatus(port) & kUHCI_PORTSC_MASK;
    if (enable) {
        value |= kUHCI_PORTSC_PED;
    } else {
        value &= ~kUHCI_PORTSC_PED;
    }
    WritePortStatus(port, value);
}

IOReturn
AppleUSBUHCI::RHSuspendPort(int port, bool suspended)
{
    UInt16 cmd, value;
    
    USBLog(5, "%s[%p]::RHSuspendPort %d (%s)", getName(), this, port, suspended ? "SUSPEND" : "RESUME");
    port--; // convert 1-based to 0-based.

    cmd = ioRead16(kUHCI_CMD);
    value = ReadPortStatus(port) & kUHCI_PORTSC_MASK;
    
    if (suspended) {
        value |= kUHCI_PORTSC_SUSPEND;
        value &= ~kUHCI_PORTSC_RD;
    } else {
        if (cmd & kUHCI_CMD_EGSM) {
            /* Can't un-suspend a port during global suspend. */
            USBLog(1, "%s[%p]: attempt to resume during global suspend", getName(), this);
            return kIOReturnError;
        }
        value |= (kUHCI_PORTSC_SUSPEND | kUHCI_PORTSC_RD);
    }
    /* Always enable the port also. */
    value |= kUHCI_PORTSC_PED;

    USBLog(7, "%s[%p]: writing 0x%x to port control", getName(), this, value);
    
    WritePortStatus(port, value);
    
    if (suspended) {
        /* Suspending.
         * Sleep for 10ms to ensure nothing goes out on the bus
         * until devices are suspended.
         */
        IOSleep(10);
        
    } else {
        /* Resuming */
        int i;
        
        /* Wait at least the required 20ms, then de-assert the resume signal. */
        IOSleep(30);
        
        value = ReadPortStatus(port) & kUHCI_PORTSC_MASK;
        value &= ~(kUHCI_PORTSC_RD | kUHCI_PORTSC_SUSPEND);
        USBLog(7, "%s[%p]: de-asserting resume signal 0x%x", getName(), this, value);
        WritePortStatus(port, value);

        /* Wait for EOP to finish. */
        for (i=0; i<10; i++) {
            IOSleep(1);
            if ((ReadPortStatus(port) & kUHCI_PORTSC_RD) == 0) {
                break;
            }
        }
        USBLog(7, "%s[%p]: EOP finished", getName(), this);
        
        /* Wait another 10ms for devices to recover. */
        IOSleep(10);
    }

    USBLog(5, "%s[%p]::RHSuspendPort %d (%s) calling UIMRootHubStatusChange", getName(), this, port, suspended ? "SUSPEND" : "RESUME");

    UIMRootHubStatusChange();        
    
    USBLog(5, "%s[%p]::RHSuspendPort %d (%s) DONE", getName(), this, port, suspended ? "SUSPEND" : "RESUME");

    return kIOReturnSuccess;
}

/* Reset and enable the port. */

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
    
    for (i=10; i>0; i--) {
        IOSleep(10);
        
        value = ReadPortStatus(port);
        
        if ((value & kUHCI_PORTSC_CCS) == 0) {
            /* No device connected; don't enter reset state. */
            //USBLog(5, "%s[%p]: no device connected, not entering reset state");
            return kIOReturnNotResponding;
            break;
        }
        
        if (value & (kUHCI_PORTSC_PEDC | kUHCI_PORTSC_CSC)) {
            /* Change bits detected. Clear them and continue waiting. */
            WritePortStatus(port, (value & kUHCI_PORTSC_MASK) |
                            (kUHCI_PORTSC_PEDC | kUHCI_PORTSC_CSC));
            continue;
        }
        
        if (value & kUHCI_PORTSC_PED) {
            /* Port successfully enabled. */
            break;
        }
        
    }
    
    if (i == 0) {
        USBLog(5, "%s[%p]: reset port FAILED", getName(), this);
        return kIOReturnNotResponding;
    }
    
    /* Remember that we were reset. */
    _portWasReset[port] = true;
    
    USBLog(5, "%s[%p]: reset port succeeded", getName(), this);
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
        char *string;
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
    for (i=0; strings[i].string != 0; i++) {
        if ((value & strings[i].mask) != 0) {
            strcat(buf, strings[i].string);
        }
    }
    USBLog(5, "%s[%p]: Port %d: status %x %s", getName(), this, port, value, buf);
}

void
AppleUSBUHCI::RHDumpHubPortStatus(IOUSBHubPortStatus *status)
{
    UInt16 value;
    char buf[128];
    static struct {
        UInt16 mask;
        char *string;
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
    for (i=0; strings[i].string != 0; i++) {
        if ((value & strings[i].mask) != 0) {
            strcat(buf, strings[i].string);
        }
    }
    USBLog(5, "%s[%p]: Hub port status: %s", getName(), this, buf);
    buf[0] = '\0';
    value = USBToHostWord(status->changeFlags);
    for (i=0; strings[i].string != 0; i++) {
        if ((value & strings[i].mask) != 0) {
            strcat(buf, strings[i].string);
        }
    }
    USBLog(5, "%s[%p]: Hub port change: %s", getName(), this, buf);
    
}

