/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#include "AppleUSBOpticalMouse.h"

#define super IOUSBHIDDriver

#define kDefaultFixedResolution (400 << 16)

OSDefineMetaClassAndStructors(AppleUSBOpticalMouse, super)

static	bool switchTo800dpi = true;

IOReturn
AppleUSBOpticalMouse::StartFinalProcessing()
{
    OSNumber 		*curResPtr, *resPrefPtr;
    UInt32 		curResInt, resPrefInt;
    IOFixed		curRes, resPref;
    IOReturn		err = kIOReturnSuccess;
    
    USBLog(3, "%s[%p]::StartFinalProcessing", getName(), this);

    curResPtr = (OSNumber*)getProperty(kIOHIDPointerResolutionKey);
    if (curResPtr)
    {
        curResInt = curResPtr->unsigned32BitValue();
        USBLog(3, "%s[%p]::StartFinalProcessing - found current resolution property - value %x", getName(), this, curResInt);
    }
    else
    {
        curResInt = kDefaultFixedResolution;
        USBLog(3, "%s[%p]::StartFinalProcessing - no current property found - using default %x", getName(), this, curResInt);
    }
    
    resPrefPtr = (OSNumber *)getProperty("xResolutionPref");
    if (resPrefPtr)
        resPrefInt = resPrefPtr->unsigned32BitValue();
    else
    {
        resPrefInt = kDefaultFixedResolution * 2;
        USBLog(3, "%s[%p]::StartFinalProcessing - no preference property found - using default %x", getName(), this, resPrefInt);
    }

    resPref = (IOFixed) resPrefInt;
    curRes = (IOFixed) curResInt;

    if (resPref != curRes)
    {
        if (switchTo800dpi)
        {
            IOUSBDevRequest		devReq;

            devReq.bmRequestType = 0x40;
            devReq.bRequest = 0x01;
            devReq.wValue = 0x05AC;
            devReq.wIndex = 0x0452;
            devReq.wLength = 0x0000;
            devReq.pData = NULL;

            err = _device->DeviceRequest(&devReq, 5000, 0);

            if (err)
                USBLog(3, "%s[%p]::StartFinalProcessing - error (%x) setting resolution", getName(), this, err);
            else
            // with this mouse, we do NOT want to start reading on the interrupt pipe, nor do
            // we want to call super::start. We just want to wait for the device to get terminated
                USBLog(3, "%s[%p]::StartFinalProcessing - waiting for click mouse termination", getName(), this);
        }
    }
    else
    {
        // If we are already at the correct resolution for OSX, OK. But what if we are going
        // back to OS 9? On restart, switch back to boot setup. Power Manager will tell us
        // when we are going to restart.
        //
        USBLog(3, "%s[%p]::StartFinalProcessing - registering PowerDownHandler", getName(), this);
        _notifier = registerPrioritySleepWakeInterest(PowerDownHandler, this, 0);
        err = super::StartFinalProcessing();
        if (err)
        {
            if (_notifier)
                _notifier->remove();
            _notifier = NULL;
        }
    }
    return err;
}




//=============================================================================================
//
//  PowerDownHandler
//	When OSX starts up the Click mouse, it switches into 800 dpi, 16 bit report mode for better
//	response. When we restart back into OS 9 AND the Click mouse is plugged into the root hub,
//	it will not be powered down and will remain in the 800 dpi mode. This makes the cursor move
//	too rapidly in 9. The fix is to switch it back to 400 dpi and 8 bit report mode. We only have
//	to do this when we are going to restart since that is the only time we can get to OS 9 without
//	powering down. Since we only registerPrioritySleepWakeInterest() when the Click mouse is being
//	switched to 800 dpi mode, anytime we get here, we know we may need that switchback command.
//
//=============================================================================================
//
IOReturn 
AppleUSBOpticalMouse::PowerDownHandler(void *target, void *refCon, UInt32 messageType, IOService *service,
                                void *messageArgument, vm_size_t argSize )
{
    IOUSBDevRequest 	devReq;
    IOReturn		err = kIOReturnUnsupported;
    AppleUSBOpticalMouse *	me = OSDynamicCast(AppleUSBOpticalMouse, (OSObject *)target);

    if (!me)
        return err;

    switch (messageType)
    {
        case kIOMessageSystemWillRestart:
            // Tell the driver (using a static variable that will survive across termination)
            // that we don't want to switch to 800 dpi on the next driver start
            //
            switchTo800dpi = false;

            // Send switch back command.
            devReq.bmRequestType = 0x40;
            devReq.bRequest = 0x01;
            devReq.wValue = 0x05AC;
            devReq.wIndex = 0x0052;		// switch = 0452; switchback = 0052
            devReq.wLength = 0x0000;
            devReq.pData = NULL;
        
            err = (me)->_device->DeviceRequest(&devReq, 5000, 0);

            break;
            
        default:
            // We don't care about any other message that comes in here.
            break;
    }
    
    // Allow shutdown to go on no matter what Click mouse is doing.
    return err;
}


bool
AppleUSBOpticalMouse::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that 
    // we have begun getting our callbacks in order. by the time we get here, the 
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    USBLog(3, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());

    // Clean up our notifier.  That will release it
    //
    if ( _notifier )
        _notifier->remove();

    _notifier = NULL;
    
    return super::willTerminate(provider, options);
}



